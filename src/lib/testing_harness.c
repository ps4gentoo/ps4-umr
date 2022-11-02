/*
 * Copyright 2022 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors: Tom St Denis <tom.stdenis@amd.com>
 *
 */
#include "umr.h"

#include <ctype.h>
#include <stdbool.h>

// chomp out rest of line
static void chomp(const char **ptr)
{
	// skip until we hit a newline
	while (**ptr && **ptr != '\n')
		++(*ptr);

	// skip newline
	if (**ptr)
		++(*ptr);
}

static void consume_whitespace(const char **ptr)
{
	int n;

	// eat white space and/or consume comments
	do {
		n = 0;
		while (**ptr && (**ptr == ' ' || **ptr == '\t' || **ptr == '\n')) {
			n = 1;
			++(*ptr);
		}
		if (**ptr == ';') {
			n = 1;
			chomp(ptr);
		}
	} while (n);
}

static void consume_hexdigits(const char **ptr)
{
	while (**ptr && (isxdigit(**ptr) || **ptr == 'x' || **ptr == 'X'))
		++(*ptr);
}

static uint64_t consume_xint64(const char **ptr, int *res)
{
	uint64_t v;
	consume_whitespace(ptr);
	if (sscanf(*ptr, "%"SCNx64, &v) == 1) {
		*res = 1;
		consume_hexdigits(ptr);
		return v;
	} else {
		*res = 0;
		return 0;
	}
}

static uint32_t consume_xint32(const char **ptr, int *res)
{
	uint32_t v;
	consume_whitespace(ptr);
	if (sscanf(*ptr, "%"SCNx32, &v) == 1) {
		*res = 1;
		consume_hexdigits(ptr);
		return v;
	} else {
		*res = 0;
		return 0;
	}
}

static uint8_t consume_xint8(const char **ptr, int *res)
{
	uint8_t v;
	const char* nextdigit;
	char t[3];
	t[2] = 0;

	consume_whitespace(ptr);
	if (**ptr == '}') {
		*res = 0;
		return 0;
	}

	nextdigit = *ptr;
	while (*nextdigit && !isxdigit(*nextdigit)) ++nextdigit;
	t[0] = *nextdigit++;
	while (*nextdigit && !isxdigit(*nextdigit)) ++nextdigit;
	t[1] = *nextdigit++;

	if (sscanf(t, "%"SCNx8, &v) == 1) {
		*res = 1;
		(*ptr) = nextdigit;
		return v;
	} else {
		*res = 0;
		return 0;
	}
}

// expect '{...}'
static uint8_t *consume_bytes(const char **ptr, uint32_t *size)
{
	int s = 32, x, r;
	uint8_t *p, *po;

	consume_whitespace(ptr);
	if (**ptr == '{') {
		++(*ptr);
	} else {
		fprintf(stderr, "[ERROR]: Expecting '{' for array of bytes\n");
		*size = 0;
		return NULL;
	}

	p = calloc(1, s);
	if (!p) {
		fprintf(stderr, "[ERROR]: Out of memory\n");
		*size = 0;
		return NULL;
	}

	x = 0;
	for (;;) {
		p[x] = consume_xint8(ptr, &r);
		if (!r)
			break;
		if (++x == s) {
			po = realloc(p, s + 32);
			if (po) {
				p = po;
				s += 32;
			} else {
				free(p);
				fprintf(stderr, "[ERROR]: Out of memory\n");
				*size = 0;
				return NULL;
			}
		}
	}

	consume_whitespace(ptr);
	if (**ptr == '}') {
		++(*ptr);
	} else {
		fprintf(stderr, "[ERROR]: Expecting '}' for array of bytes\n");
		free(p);
		*size = 0;
		return NULL;
	}
	*size = x;
	return p;
}

// format: '{...[,...]}'
static uint32_t *consume_words(const char **ptr, uint32_t *size)
{
	int s = 8, x, r;
	uint32_t *p, *po;

	consume_whitespace(ptr);
	if (**ptr == '{') {
		++(*ptr);
	} else {
		fprintf(stderr, "[ERROR]: Expecting '{' for array of words\n");
		*size = 0;
		return NULL;
	}

	p = calloc(sizeof(p[0]), s);
	if (!p) {
		fprintf(stderr, "[ERROR]: Out of memory\n");
		*size = 0;
		return NULL;
	}

	x = 0;
	for (;;) {
		p[x] = consume_xint32(ptr, &r);
		if (!r)
			break;
		if (++x == s) {
			po = realloc(p, (s + 8) * sizeof(p[0]));
			if (po) {
				p = po;
				s += 8;
			} else {
				free(p);
				fprintf(stderr, "[ERROR]: Out of words\n");
				*size = 0;
				return NULL;
			}
		}

		// continue if there is a ','
		consume_whitespace(ptr);
		if (**ptr != ',')
			break;
		++(*ptr);
	}

	consume_whitespace(ptr);
	if (**ptr == '}') {
		++(*ptr);
	} else {
		fprintf(stderr, "[ERROR]: Expecting '}' for array of words\n");
		free(p);
		*size = 0;
		return NULL;
	}
	*size = x;
	return p;
}

static int consume_word(const char **ptr, char *token)
{
	consume_whitespace(ptr);
	if (!memcmp(*ptr, token, strlen(token))) {
		*ptr += strlen(token);
		return 1;
	}
	return 0;
}

static int expect_word(const char **ptr, char *token)
{
	int r;
	r = consume_word(ptr, token);
	if (!r)
		fprintf(stderr, "[ERROR]: Expecting token '%s'\n", token);
	return r;
}

void umr_free_test_harness(struct umr_test_harness *th)
{
	struct umr_ram_blocks *sram, *vram, *config, *discovery;
	struct umr_mmio_blocks *mmio, *vgpr, *sgpr, *wave, *ring;
	struct umr_sq_blocks *sq;
	void *t;

	if (!th)
		return;

	discovery = th->discovery.next;
	config = th->config.next;
	sram = th->sysram.next;
	vram = th->vram.next;
	mmio = th->mmio.next;
	sq   = th->sq.next;
	vgpr = th->vgpr.next;
	sgpr = th->sgpr.next;
	wave = th->wave.next;
	ring = th->ring.next;

	free(th->discovery.contents);
	free(th->config.contents);
	free(th->sysram.contents);
	free(th->vram.contents);
	free(th->mmio.values);
	free(th->vgpr.values);
	free(th->sgpr.values);
	free(th->wave.values);
	free(th->ring.values);
	free(th->sq.values);

	while (discovery) {
		t = discovery->next;
		free(discovery->contents);
		free(discovery);
		discovery = t;
	}

	while (config) {
		t = config->next;
		free(config->contents);
		free(config);
		config = t;
	}

	while (sram) {
		t = sram->next;
		free(sram->contents);
		free(sram);
		sram = t;
	}

	while (vram) {
		t = vram->next;
		free(vram->contents);
		free(vram);
		vram = t;
	}

	while (mmio) {
		t = mmio->next;
		free(mmio->values);
		free(mmio);
		mmio = t;
	}

	while (vgpr) {
		t = vgpr->next;
		free(vgpr->values);
		free(vgpr);
		vgpr = t;
	}

	while (sgpr) {
		t = sgpr->next;
		free(sgpr->values);
		free(sgpr);
		sgpr = t;
	}

	while (wave) {
		t = wave->next;
		free(wave->values);
		free(wave);
		wave = t;
	}

	while (ring) {
		t = ring->next;
		free(ring->values);
		free(ring);
		ring = t;
	}

	while (sq) {
		t = sq->next;
		free(sq->values);
		free(sq);
		sq = t;
	}
	free(th);
}

struct umr_test_harness *umr_create_test_harness(const char *script)
{
	struct umr_test_harness *th;
	struct umr_ram_blocks *sram, *vram, *config, *discovery;
	struct umr_mmio_blocks *mmio, *vgpr, *sgpr, *wave, *ring;
	struct umr_sq_blocks *sq;
	int r;

	th = calloc(1, sizeof *th);

	discovery = &th->discovery;
	config = &th->config;
	sram = &th->sysram;
	vram = &th->vram;
	mmio = &th->mmio;
	sq   = &th->sq;
	vgpr = &th->vgpr;
	sgpr = &th->sgpr;
	wave = &th->wave;
	ring = &th->ring;

	while (*script) {
		consume_whitespace(&script);
		if (consume_word(&script, "GCACONFIG")) {
			if (!expect_word(&script, "="))
				goto error;
			config->contents = consume_bytes(&script, &config->size);
			if (!config->size)
				goto error;
			config->next = calloc(1, sizeof *config);
			config = config->next;
		}
		if (consume_word(&script, "DISCOVERY")) {
			if (!expect_word(&script, "="))
				goto error;
			discovery->contents = consume_bytes(&script, &discovery->size);
			if (!discovery->size)
				goto error;
			discovery->next = calloc(1, sizeof *discovery);
			discovery = discovery->next;
		}
		if (consume_word(&script, "SYSRAM@")) {
			sram->base_address = consume_xint64(&script, &r);
			if (!r)
				goto error;
			if (!expect_word(&script, "="))
				goto error;
			sram->contents = consume_bytes(&script, &sram->size);
			if (!sram->size)
				goto error;
			sram->next = calloc(1, sizeof *sram);
			sram = sram->next;
		}
		if (consume_word(&script, "VRAM@")) {
			vram->base_address = consume_xint64(&script, &r);
			if (!r)
				goto error;
			if (!expect_word(&script, "="))
				goto error;
			vram->contents = consume_bytes(&script, &vram->size);
			if (!vram->size)
				goto error;
			vram->next = calloc(1, sizeof *vram);
			vram = vram->next;
		}
		if (consume_word(&script, "MMIO@")) {
			mmio->mmio_address = consume_xint32(&script, &r);
			if (!r)
				goto error;
			if (!expect_word(&script, "="))
				goto error;
			mmio->values = consume_words(&script, &mmio->no_values);
			if (!mmio->no_values)
				goto error;
			mmio->next = calloc(1, sizeof *mmio);
			mmio = mmio->next;
		}
		if (consume_word(&script, "VGPR@")) {
			vgpr->mmio_address = consume_xint64(&script, &r);
			if (!r)
				goto error;
			if (!expect_word(&script, "="))
				goto error;
			vgpr->values = consume_words(&script, &vgpr->no_values);
			if (!vgpr->no_values)
				goto error;
			vgpr->next = calloc(1, sizeof *vgpr);
			vgpr = vgpr->next;
		}
		if (consume_word(&script, "SGPR@")) {
			sgpr->mmio_address = consume_xint64(&script, &r);
			if (!r)
				goto error;
			if (!expect_word(&script, "="))
				goto error;
			sgpr->values = consume_words(&script, &sgpr->no_values);
			if (!sgpr->no_values)
				goto error;
			sgpr->next = calloc(1, sizeof *sgpr);
			sgpr = sgpr->next;
		}
		if (consume_word(&script, "WAVESTATUS@")) {
			wave->mmio_address = consume_xint64(&script, &r);
			if (!r)
				goto error;
			if (!expect_word(&script, "="))
				goto error;
			wave->values = consume_words(&script, &wave->no_values);
			if (!wave->no_values)
				goto error;
			wave->next = calloc(1, sizeof *wave);
			wave = wave->next;
		}
		if (consume_word(&script, "RINGDATA")) {
			if (!expect_word(&script, "="))
				goto error;
			ring->values = consume_words(&script, &ring->no_values);
			if (!ring->no_values)
				goto error;
			ring->next = calloc(1, sizeof *ring);
			ring = ring->next;
		}
		if (consume_word(&script, "SQ@")) {
			sq->sq_address = consume_xint32(&script, &r);
			if (!r)
				goto error;
			if (!expect_word(&script, "="))
				goto error;
			sq->values = consume_words(&script, &sq->no_values);
			if (!sq->no_values)
				goto error;
			sq->next = calloc(1, sizeof *sq);
			sq = sq->next;
		}
	}
	return th;
error:
	umr_free_test_harness(th);
	return NULL;
}

struct umr_test_harness *umr_create_test_harness_file(const char *fname)
{
	const char *script;
	int fd;
	off_t size;
	struct umr_test_harness *th;

	fd = open(fname, O_RDONLY);
	size = lseek(fd, 0, SEEK_END);
	lseek(fd, 0, SEEK_SET);
	script = calloc(1, size + 1);
	read(fd, (char*)script, size);
	close(fd);

	th = umr_create_test_harness(script);
	free((void*)script);
	return th;
}

static int access_sram(struct umr_asic *asic, uint64_t address, uint32_t size, void *dst, int write_en)
{
	struct umr_test_harness *th = asic->mem_funcs.data;
	struct umr_ram_blocks *rb = &th->sysram;

	// try to find first block that covers the range
	while (rb) {
		if (rb->base_address <= address &&
			((rb->base_address + rb->size) >= (address + size))) {
				if (!write_en)
					memcpy(dst, &rb->contents[address - rb->base_address], size);
				else
					memcpy(&rb->contents[address - rb->base_address], dst, size);
				return 0;
			}
		rb = rb->next;
	}
	fprintf(stderr, "[ERROR]: System address 0x%"PRIx64 " not found in test harness\n", address);
	return -1;
}

static int access_linear_vram(struct umr_asic *asic, uint64_t address, uint32_t size, void *data, int write_en)
{
	struct umr_test_harness *th = asic->mem_funcs.data;
	struct umr_ram_blocks *rb = &th->vram;

	// try to find first block that covers the range
	while (rb) {
		if (rb->base_address <= address &&
			((rb->base_address + rb->size) >= (address + size))) {
				if (!write_en)
					memcpy(data, &rb->contents[address - rb->base_address], size);
				else
					memcpy(&rb->contents[address - rb->base_address], data, size);
				return 0;
			}
		rb = rb->next;
	}
	fprintf(stderr, "[ERROR]: VRAM address 0x%"PRIx64 " not found in test harness\n", address);
	return -1;
}

static uint64_t gpu_bus_to_cpu_address(struct umr_asic *asic, uint64_t dma_addr)
{
	(void)asic;
	return dma_addr;
}

static uint32_t read_reg(struct umr_asic *asic, uint64_t addr, enum regclass type)
{
	struct umr_test_harness *th = asic->reg_funcs.data;
	struct umr_sq_blocks *sq;
	struct umr_mmio_blocks *mm;
	uint32_t v;
	uint64_t qaddr;

	// 'addr' is a byte address of the register but the database
	// is stored in DWORD addresses
	qaddr = addr >> 2;

	if (type != REG_MMIO)
		return 0xDEADBEEF;

//	printf("Reading: %lx\n", (unsigned long)addr);

	// are we reading from SQ_IND_DATA or MM_DATA?
	if (qaddr == umr_find_reg(asic, "mmSQ_IND_DATA")) {
		// find in SQ list
		sq = &th->sq;
		while (sq) {
			if (sq->sq_address == th->sq_ind_index && sq->cur_slot < sq->no_values) {
				v = sq->values[sq->cur_slot];
				++(sq->cur_slot);
				return v;
			}
			sq = sq->next;
		}
		return 0xDEADBEEF;
	} else if (qaddr == umr_find_reg(asic, "mmSQ_IND_INDEX")) {
		return th->sq_ind_index;
	} else if (qaddr == umr_find_reg(asic, "@mmMM_DATA") ||
			   qaddr == umr_find_reg(asic, "@mmBIF_BX_PF_MM_DATA")) {
		fprintf(stderr, "MM_DATA!\n");
		// read from VRAM
		if (th->vram_mm_index & (1ULL << 31)) {
			uint64_t addr;
			addr = (th->vram_mm_index & 0x7FFFFFFFULL) | ((th->vram_mm_index & 0xFFFFFFFF00000000ULL) >> 1);
			if (!access_linear_vram(asic, addr, 4, &v, 0)) {
				return v;
			} else {
				fprintf(stderr, "[ERROR]: Access to missing VRAM block via MMIO method\n");
				return 0;
			}
		} else {
			fprintf(stderr, "[ERROR]: MM_INDEX must have 32nd bit set\n");
			return 0;
		}
	} else if (qaddr == umr_find_reg(asic, "@mmMM_INDEX") ||
			   qaddr == umr_find_reg(asic, "@mmBIF_BX_PF_MM_INDEX")) {
		return th->vram_mm_index & 0xFFFFFFFFULL;
	} else if (qaddr == umr_find_reg(asic, "@mmMM_INDEX_HI") ||
			   qaddr == umr_find_reg(asic, "@mmBIF_BX_PF_MM_INDEX_HI")) {
		return th->vram_mm_index >> 32;
	} else {
		// read from MMIO list
		mm = &th->mmio;
		while (mm) {
			if (mm->mmio_address == addr && mm->cur_slot != mm->no_values) {
				v = mm->values[mm->cur_slot];
				++(mm->cur_slot);
				return v;
			}
			mm = mm->next;
		}
		return 0xDEADBEEF;
	}
}

static int write_reg(struct umr_asic *asic, uint64_t addr, uint32_t value, enum regclass type)
{
	struct umr_test_harness *th = asic->reg_funcs.data;
	struct umr_mmio_blocks *mm;
	uint64_t qaddr;

	// 'addr' is a byte address of the register but the database
	// is stored in DWORD addresses
	qaddr = addr >> 2;

	if (type != REG_MMIO)
		return -1;

	// are we reading from SQ_IND_DATA or MM_DATA?
	if (qaddr == umr_find_reg(asic, "mmSQ_IND_DATA")) {
		// don't allow writing to SQ_IND_DATA
		return -1;
	} else if (qaddr == umr_find_reg(asic, "mmSQ_IND_INDEX")) {
		th->sq_ind_index = value;
		return 0;
	} else if (qaddr == umr_find_reg(asic, "@mmMM_DATA") ||
			   qaddr == umr_find_reg(asic, "@mmBIF_BX_PF_MM_DATA")) {
		// write to VRAM
		if (th->vram_mm_index & (1ULL << 31)) {
			uint64_t addr;
			addr = (th->vram_mm_index & 0x7FFFFFFFULL) | ((th->vram_mm_index & 0xFFFFFFFF00000000ULL) >> 1);
			if (!access_linear_vram(asic, addr, 4, &value, 1)) {
				return 0;
			} else {
				fprintf(stderr, "[ERROR]: Access to missing VRAM block via MMIO method\n");
				return -1;
			}
		} else {
			fprintf(stderr, "[ERROR]: MM_INDEX must have 32nd bit set\n");
			return -1;
		}
	} else if (qaddr == umr_find_reg(asic, "@mmMM_INDEX") ||
			   qaddr == umr_find_reg(asic, "@mmBIF_BX_PF_MM_INDEX")) {
		th->vram_mm_index = (th->vram_mm_index & 0xFFFFFFFF00000000ULL) | value;
		return 0;
	} else if (qaddr == umr_find_reg(asic, "@mmMM_INDEX_HI") ||
			   qaddr == umr_find_reg(asic, "@mmBIF_BX_PF_MM_INDEX_HI")) {
		th->vram_mm_index = (th->vram_mm_index & 0xFFFFFFFFULL) | ((uint64_t)value << 32);
		return 0;
	} else {
		// write to MMIO
		mm = &th->mmio;
		while (mm) {
			if (mm->mmio_address == addr) {
				mm->values[mm->cur_slot] = value;
				return 0;
			}
			mm = mm->next;
		}
		return -1;
	}
}

static int read_sgprs(struct umr_asic *asic, struct umr_wave_status *ws, uint32_t *dst)
{
	uint64_t addr, shift, nr, x;
	struct umr_test_harness *th = asic->reg_funcs.data;
	struct umr_mmio_blocks *mm;

	if (asic->family >= FAMILY_NV) {
		addr =
			(1ULL << 60)                             | // reading SGPRs
			((uint64_t)0)                            | // starting address to read from
			((uint64_t)ws->hw_id1.se_id << 12)       |
			((uint64_t)ws->hw_id1.sa_id << 20)       |
			((uint64_t)((ws->hw_id1.wgp_id << 2) | ws->hw_id1.simd_id) << 28)  |
			((uint64_t)ws->hw_id1.wave_id << 36)     |
			(0ULL << 52); // thread_id

		nr = 112;
	} else {
		if (asic->family <= FAMILY_CIK)
			shift = 3;  // on SI..CIK allocations were done in 8-dword blocks
		else
			shift = 4;  // on VI allocations are in 16-dword blocks

		addr =
			(1ULL << 60)                             | // reading SGPRs
			((uint64_t)0)                            | // starting address to read from
			((uint64_t)ws->hw_id.se_id << 12)        |
			((uint64_t)ws->hw_id.sh_id << 20)        |
			((uint64_t)ws->hw_id.cu_id << 28)        |
			((uint64_t)ws->hw_id.wave_id << 36)      |
			((uint64_t)ws->hw_id.simd_id << 44)      |
			(0ULL << 52); // thread_id

		nr = (ws->gpr_alloc.sgpr_size + 1) << shift;
	}

	// grab upto 'nr' words into dst[0..nr-1]
	for (x = 0; x < nr; x++) {
		// read from SGPR list
		mm = &th->sgpr;
		while (mm) {
			/* because of how reading GPRs is done there could be more than
			 * one vector entry for this given GPR address so we stop reading
			 * from a given link when it's been exhausted */
			if (mm->mmio_address == addr && mm->cur_slot < mm->no_values) {
				dst[x] = mm->values[mm->cur_slot];
				++(mm->cur_slot);
			}
			mm = mm->next;
		}
	}

	// read trap if any
	if (ws->wave_status.trap_en || ws->wave_status.priv) {
		addr += 4 * 0x6C;  // byte offset, kernel adds 0x200 to address
		for (x = 0; x < nr; x++) {
			// read from VGPR list
			mm = &th->sgpr;
			while (mm) {
				if (mm->mmio_address == addr && mm->cur_slot < mm->no_values) {
					dst[0x6C + x] = mm->values[mm->cur_slot];
					++(mm->cur_slot);
				}
				mm = mm->next;
			}
		}
	}
	return 0;
}

static int read_vgprs(struct umr_asic *asic, struct umr_wave_status *ws, uint32_t thread, uint32_t *dst)
{
	struct umr_test_harness *th = asic->reg_funcs.data;
	struct umr_mmio_blocks *mm;
	uint64_t addr, nr, x;
	unsigned granularity = asic->parameters.vgpr_granularity; // default is blocks of 4 registers

	// reading VGPR is not supported on pre GFX9 devices
	if (asic->family < FAMILY_AI)
		return -1;

	if (asic->family >= FAMILY_NV) {
		addr =
			(0ULL << 60)                             | // reading VGPRs
			((uint64_t)0)                            | // starting address to read from
			((uint64_t)ws->hw_id1.se_id << 12)        |
			((uint64_t)ws->hw_id1.sa_id << 20)        |
			((uint64_t)((ws->hw_id1.wgp_id << 2) | ws->hw_id1.simd_id) << 28)  |
			((uint64_t)ws->hw_id1.wave_id << 36)      |
			((uint64_t)thread << 52);

		nr = (ws->gpr_alloc.vgpr_size + 1) << granularity;
	} else {
		addr =
			(0ULL << 60)                             | // reading VGPRs
			((uint64_t)0)                            | // starting address to read from
			((uint64_t)ws->hw_id.se_id << 12)        |
			((uint64_t)ws->hw_id.sh_id << 20)        |
			((uint64_t)ws->hw_id.cu_id << 28)        |
			((uint64_t)ws->hw_id.wave_id << 36)      |
			((uint64_t)ws->hw_id.simd_id << 44)      |
			((uint64_t)thread << 52);

		nr = (ws->gpr_alloc.vgpr_size + 1) << granularity;
	}

	// grab upto 'nr' words into dst[0..nr-1]
	for (x = 0; x < nr; x++) {
		// read from VGPR list
		mm = &th->vgpr;
		while (mm) {
			/* because of how reading GPRs is done there could be more than
			 * one vector entry for this given GPR address so we stop reading
			 * from a given link when it's been exhausted */
			if (mm->mmio_address == addr && mm->cur_slot < mm->no_values) {
				dst[x] = mm->values[mm->cur_slot];
				++(mm->cur_slot);
			}
			mm = mm->next;
		}
	}
	return 0;
}

static int wave_status(struct umr_asic *asic, unsigned se, unsigned sh, unsigned cu, unsigned simd, unsigned wave, struct umr_wave_status *ws)
{
	struct umr_test_harness *th = asic->reg_funcs.data;
	struct umr_mmio_blocks *mm;
	uint64_t addr, x;
	uint32_t buf[32];

	memset(buf, 0, sizeof buf);

	addr = 0 |
		((uint64_t)se << 7) |
		((uint64_t)sh << 15) |
		((uint64_t)cu << 23) |
		((uint64_t)wave << 31) |
		((uint64_t)simd << 37);

	// find the first unused slot and then read all of the contents
	mm = &th->vgpr;
	x = 0;
	while (mm) {
		if (mm->mmio_address == addr && mm->cur_slot == 0) {
			for (x = 0; x < mm->no_values; x++) {
				buf[x] = mm->values[mm->cur_slot];
				++(mm->cur_slot);
			}
			break;
		}
		mm = mm->next;
	}

	if (x)
		return umr_parse_wave_data_gfx(asic, ws, buf);
	else
		return -1;
}

int umr_test_harness_get_config_data(struct umr_asic *asic, uint8_t *dst)
{
	int x;
	struct umr_test_harness *th = asic->options.th;

	for (x = 0; x < (int)th->config.size; x++) {
		dst[x] = th->config.contents[x];
	}
	return x;
}


void *umr_test_harness_get_ring_data(struct umr_asic *asic, uint32_t *ringsize)
{
	uint32_t x, *rd;
	struct umr_test_harness *th = asic->reg_funcs.data;

	rd = calloc(th->ring.no_values, sizeof *rd);
	for (x = 0; x < th->ring.no_values; x++) {
		rd[x] = th->ring.values[x];
	}
	*ringsize = (x * 4) - 12; // return size in bytes minus the rptr/wptr/dev_wptr
	return rd;
}

void umr_attach_test_harness(struct umr_test_harness *th, struct umr_asic *asic)
{
	asic->mem_funcs.access_linear_vram = access_linear_vram;
	asic->mem_funcs.access_sram = access_sram;
	asic->mem_funcs.gpu_bus_to_cpu_address = gpu_bus_to_cpu_address;
	asic->mem_funcs.vm_message = &printf;
	asic->mem_funcs.data = th;

	asic->reg_funcs.read_reg = read_reg;
	asic->reg_funcs.write_reg = write_reg;
	asic->reg_funcs.data = th;

	asic->gpr_read_funcs.read_sgprs = read_sgprs;
	asic->gpr_read_funcs.read_vgprs = read_vgprs;

	asic->wave_funcs.get_wave_status = wave_status;

	th->asic = asic;
}

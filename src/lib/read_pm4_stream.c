/*
 * Copyright (c) 2025 Advanced Micro Devices, Inc.
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


static uint32_t fetch_word(struct umr_asic *asic, struct umr_pm4_stream *stream, uint32_t off)
{
	if (off >= stream->n_words) {
		if (!(stream->invalid))
			asic->err_msg("[ERROR]: PM4 decoding of opcode (%"PRIx32") went out of bounds.\n", stream->opcode);
		stream->invalid = 1;
		return 0;
	} else {
		return stream->words[off];
	}
}

/**
 * parse_pm4 - Parse a PM4 packet looking for pointers to shaders or IBs
 *
 * @vm_partition: What VM partition does it come from (-1 is default)
 * @vmid:  The known VMID this packet belongs to (or 0 if from a ring)
 * @ps: The PM4 packet to parse
 *
 * This function looks for shaders that are indicated by a single
 * SET_SH_REG packet or further IBs indicated by INDIRECT_BUFFER
 * packets.
 */
static void parse_pm4(struct umr_asic *asic, int vm_partition, uint32_t vmid, struct umr_pm4_stream *ps)
{
	uint64_t addr;
	uint32_t size, tvmid, rsrc1, rsrc2;
	void *buf;

	switch (ps->opcode) {
		case 0x76: // SET_SH_REG (looking for writes to shader registers);
		{
			unsigned n, na;
			uint32_t reg_addr = fetch_word(asic, ps, 0) + 0x2C00;
			uint64_t shader_addr = 0;
			int type = 0;
			char *tmp;

			rsrc1 = rsrc2 = 0;

			for (na = 0, n = 1; n < ps->n_words; n++) {
				tmp = umr_reg_name(asic, reg_addr + n - 1);
				if (strstr(tmp, "SPI_SHADER_PGM_LO_") || strstr(tmp, "COMPUTE_PGM_LO")) {
					// grab shader type (pixel, vertex, compute)
					if (strstr(tmp, "LO_PS")) {
						type = asic->options.shader_enable.enable_ps_shader ? UMR_SHADER_PIXEL : UMR_SHADER_OPAQUE;
					} else if (strstr(tmp, "LO_VS")) {
						type = asic->options.shader_enable.enable_vs_shader ? UMR_SHADER_VERTEX : UMR_SHADER_OPAQUE;
					} else if (strstr(tmp, "LO_HS")) {
						type = (asic->family <= FAMILY_VI || asic->options.shader_enable.enable_hs_shader) ? UMR_SHADER_HS : UMR_SHADER_OPAQUE;
					} else if (strstr(tmp, "LO_GS")) {
						type = (asic->family <= FAMILY_VI || asic->options.shader_enable.enable_gs_shader) ? UMR_SHADER_GS : UMR_SHADER_OPAQUE;
					} else if (strstr(tmp, "LO_LS")) {
						if (asic->options.shader_enable.enable_ls_shader)
							type = (asic->family > FAMILY_VI && asic->options.shader_enable.enable_es_ls_swap) ? UMR_SHADER_HS : UMR_SHADER_LS;
						else
							type = UMR_SHADER_OPAQUE;
					} else if (strstr(tmp, "LO_ES")) {
						if (asic->options.shader_enable.enable_es_shader)
							type = (asic->family > FAMILY_VI && asic->options.shader_enable.enable_es_ls_swap) ? UMR_SHADER_GS : UMR_SHADER_ES;
						else
							type = UMR_SHADER_OPAQUE;
					} else {
						type = asic->options.shader_enable.enable_comp_shader ? UMR_SHADER_COMPUTE : UMR_SHADER_OPAQUE;
					}

					shader_addr = (shader_addr & ~0xFFFFFFFFFFULL) | ((uint64_t)fetch_word(asic, ps, n) << 8);
					if (type != UMR_SHADER_OPAQUE)
						na |= 1;
				} else if (strstr(tmp, "SPI_SHADER_PGM_HI_") || strstr(tmp, "COMPUTE_PGM_HI")) {
					shader_addr = (shader_addr & 0xFFFFFFFFFFULL) | ((uint64_t)fetch_word(asic, ps, n) << 40);
					na |= 2;
				} else if (strstr(tmp, "SPI_SHADER_PGM_RSRC1") || strstr(tmp, "COMPUTE_PGM_RSRC1")) {
					rsrc1 = fetch_word(asic, ps, n);
				} else if (strstr(tmp, "SPI_SHADER_PGM_RSRC2") || strstr(tmp, "COMPUTE_PGM_RSRC2")) {
					rsrc2 = fetch_word(asic, ps, n);
				}
			}

			if (na == 3) {
				// we have a shader address
				ps->shader = calloc(1, sizeof(ps->shader[0]));
				ps->shader->vmid = vmid;
				ps->shader->addr = shader_addr;
				if (!asic->options.no_follow_shader)
					ps->shader->size = umr_compute_shader_size(asic, vm_partition, ps->shader);
				else
					ps->shader->size = 1;
				ps->shader->type = type;
				ps->shader->rsrc1 = rsrc1;
				ps->shader->rsrc2 = rsrc2;
			}
			break;
		}
		case 0x3f: // INDIRECT_BUFFER_CIK
		case 0x33: // INDIRECT_BUFFER_CONST
			if (!asic->options.no_follow_ib) {
				addr = (fetch_word(asic, ps, 0) & ~3ULL) | ((uint64_t)(fetch_word(asic, ps, 1) & 0xFFFF) << 32);

				// abort if the IB is >8 MB in size which is very likely just garbage data
				size = (fetch_word(asic, ps, 2) & ((1UL << 20) - 1)) * 4;
				if (size > (1024UL * 1024UL * 8UL))
					break;

				tvmid = (fetch_word(asic, ps, 2) >> 24) & 0xF;
				if (!tvmid)
					tvmid = vmid;
				buf = calloc(1, size);
				if (umr_read_vram(asic, vm_partition, tvmid, addr, size, buf) < 0) {
					asic->err_msg("[ERROR]: Could not read IB at 0x%"PRIx32":0x%" PRIx64 "\n", tvmid, addr);
				} else {
					ps->ib = umr_pm4_decode_stream(asic, vm_partition, tvmid, buf, size / 4);
					ps->ib_source.addr = addr;
					ps->ib_source.vmid = tvmid;
				}
				free(buf);
			}
			break;
	}
}


/**
 * umr_find_shader_in_stream - Find a shader in a PM4 stream
 *
 * @stream: A previously captured PM4 stream from a ring
 * @vmid:  The VMID of the shader to look for
 * @addr: An address inside the shader to match
 *
 * Returns a pointer to a copy of a shader object if found or
 * NULL if not.
 */
struct umr_shaders_pgm *umr_find_shader_in_stream(
	struct umr_pm4_stream *stream, unsigned vmid, uint64_t addr)
{
	struct umr_shaders_pgm *p, *pp;

	p = NULL;
	while (stream) {
		// compare shader if any
		if (stream->shader)
			if (stream->shader->vmid == vmid &&
				(addr >= stream->shader->addr) &&
				(addr < (stream->shader->addr + stream->shader->size))) {
					p = stream->shader;
					break;
				}

		// recurse into IBs if any
		if (stream->ib) {
			p = umr_find_shader_in_stream(stream->ib, vmid, addr);
			if (p)
				return p;
		}
		stream = stream->next;
	}

	if (p) {
		pp = calloc(1, sizeof(struct umr_shaders_pgm));
		*pp = *p;
		return pp;
	}

	return NULL;
}

/**
 * umr_free_pm4_stream - Free a PM4 stream object
 */
void umr_free_pm4_stream(struct umr_pm4_stream *stream)
{
	while (stream) {
		struct umr_pm4_stream *n;
		n = stream->next;
		if (stream->ib)
			umr_free_pm4_stream(stream->ib);
		free(stream->shader);
		free(stream->words);
		free(stream);
		stream = n;
	}
}

/**
 * umr_pm4_decode_stream - Decode an array of PM4 packets into a PM4 stream
 *
 * @vm_partition: What VM partition does it come from (-1 is default)
 * @vmid:  The VMID (or zero) that this array comes from (if say an IB)
 * @stream: An array of DWORDS which contain the PM4 packets
 * @nwords:  The number of words in the stream
 *
 * Returns a PM4 stream if successfully decoded.
 */
struct umr_pm4_stream *umr_pm4_decode_stream(struct umr_asic *asic, int vm_partition, uint32_t vmid, uint32_t *stream, uint32_t nwords)
{
	struct umr_pm4_stream *ops, *ps, *prev_ps = NULL;
	struct {
		int n;
		uint32_t
			size,
			vmid;
		uint64_t
			addr;
	} uvd_ib;

	ps = ops = calloc(1, sizeof *ops);
	if (!ps) {
		asic->err_msg("[ERROR]: Out of memory\n");
		return NULL;
	}

	memset(&uvd_ib, 0, sizeof uvd_ib);

	while (nwords) {
		// fetch basics out of header
		ps->header = *stream;
		ps->pkttype = *stream >> 30;
		ps->n_words = ((*stream >> 16) + 1) & 0x3FFF;

		// grab type specific header data
		if (ps->pkttype == 0)
			ps->pkt0off = *stream & 0xFFFF;
		else if (ps->pkttype == 3)
			ps->opcode = (*stream >> 8) & 0xFF;

		if (nwords < 1 + ps->n_words) {
			// if not enough words to fill packet, stop and set current packet to null
			free(ps);
			if (prev_ps) {
				prev_ps->next = NULL;
			} else {
				ops = NULL;
			}
			return ops;
		} 

		// grab rest of words
		if (ps->n_words) {
			ps->words = calloc(ps->n_words, sizeof(ps->words[0]));
			memcpy(ps->words, &stream[1], ps->n_words * sizeof(stream[0]));
		}

		// decode specific packets
		if (ps->pkttype == 3) {
			parse_pm4(asic, vm_partition, vmid, ps);
		} else if (ps->pkttype == 0) {
			char *name;
			name = umr_reg_name(asic, ps->pkt0off);

			// look for UVD IBs which are marked by 3-4 distinct
			// register writes.  They can occur in any order
			// except for the SIZE so we use a bitfield to keep
			// track of them
			if (strstr(name, "mmUVD_LMI_RBC_IB_VMID")) {
				uvd_ib.vmid = fetch_word(asic, ps, 0) | ((asic->family <= FAMILY_VI) ? 0 : UMR_MM_HUB);
				uvd_ib.n |= 1;
			} else if (strstr(name, "mmUVD_LMI_RBC_IB_64BIT_BAR_LOW")) {
				uvd_ib.addr |= fetch_word(asic, ps, 0);
				uvd_ib.n |= 2;
			} else if (strstr(name, "mmUVD_LMI_RBC_IB_64BIT_BAR_HIGH")) {
				uvd_ib.addr |= (uint64_t)fetch_word(asic, ps, 0) << 32;
				uvd_ib.n |= 4;
			} else if (strstr(name, "mmUVD_RBC_IB_SIZE")) {
				uvd_ib.size = fetch_word(asic, ps, 0) * 4;
				uvd_ib.n |= 8;
			}

			// if we have everything but the VMID assume vmid 0
			if (uvd_ib.n == (2|4|8)) {
				uvd_ib.vmid = 0;
				uvd_ib.n = 15;
			}

			// we have everything we need to point to an IB
			if (!asic->options.no_follow_ib && uvd_ib.n == 15) {
				void *buf;
				buf = calloc(1, uvd_ib.size);
				if (umr_read_vram(asic, vm_partition, uvd_ib.vmid, uvd_ib.addr, uvd_ib.size, buf) < 0) {
					asic->err_msg("[ERROR]: Could not read IB at 0x%"PRIx32":0x%" PRIx64 "\n", uvd_ib.vmid, uvd_ib.addr);
				} else {
					ps->ib = umr_pm4_decode_stream(asic, vm_partition, uvd_ib.vmid, buf, uvd_ib.size / 4);
					ps->ib_source.addr = uvd_ib.addr;
					ps->ib_source.vmid = uvd_ib.vmid;
				}
				free(buf);
				memset(&uvd_ib, 0, sizeof uvd_ib);
			}
		}

		// advance stream
		nwords -= 1 + ps->n_words;
		stream += 1 + ps->n_words;
		if (nwords) {
			ps->next = calloc(1, sizeof(*ps));
			prev_ps = ps;
			ps = ps->next;
		}
	}

	return ops;
}


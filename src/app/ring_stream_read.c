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
#include "umrapp.h"
#include <inttypes.h>

/* NOTE: This stream based presentation will eventually replace the existing
 * umr_read_ring() --ring, -R code since having duplicate opcode decoding is
 * tedious and a waste of time.
 */

struct ui_data {
	struct {
		uint64_t ib_addr, f_addr, b_addr;
		const uint32_t *rawdata;
		uint32_t off;
		FILE *f;
	} stack[32];
	int sp, no;
	struct umr_asic *asic;
};

static void next_level(struct umr_stream_decode_ui *ui)
{
	struct ui_data *data = ui->data;
	char tmpname[64];
	sprintf(tmpname, "/tmp/umr_ring_out.%d", (data->no)++);
	++(data->sp);
	data->stack[data->sp].f = fopen(tmpname, "w");
}

static void start_ib(struct umr_stream_decode_ui *ui, uint64_t ib_addr, uint32_t ib_vmid, uint64_t from_addr, uint32_t from_vmid, uint32_t size, int type)
{
	struct ui_data *data = ui->data;
	struct umr_asic *asic = data->asic;

	next_level(ui);
	data->stack[data->sp].ib_addr = ib_addr;
	fprintf(data->stack[data->sp].f, "Decoding IB at %s0x%"PRIx32"%s@%s0x%"PRIx64"%s from %s0x%"PRIx32"%s@%s0x%"PRIx64"%s of %s%lu%s words (type %s%d%s)",
	BLUE, ib_vmid, RST,
	YELLOW, ib_addr, RST,
	BLUE, from_vmid, RST,
	YELLOW, from_addr, RST,
	BLUE, (unsigned long)size, RST,
	BLUE, type, RST);
}

static void start_opcode(struct umr_stream_decode_ui *ui, uint64_t ib_addr, uint32_t ib_vmid, int pkttype, uint32_t opcode, uint32_t subop, uint32_t nwords, const char *opcode_name, uint32_t header, const uint32_t* raw_data)
{
	struct ui_data *data = ui->data;
	struct umr_asic *asic = data->asic;
	(void)raw_data;
	data->stack[data->sp].b_addr = ib_addr + 4;
	data->stack[data->sp].f_addr = ib_addr - 4;
	data->stack[data->sp].rawdata = raw_data;
	if (ui->rt == UMR_RING_SDMA) {
		fprintf(data->stack[data->sp].f, "\n[%s0x%"PRIx32"%s@%s0x%08"PRIx64"%s + %s0x%04"PRIx64"%s]\t[%s%8s0x%08"PRIx32"%s]\t%sOpcode%s %s0x%"PRIx32"%s/%s0x%"PRIx32"%s [%s%s%s] (%s%"PRIu32"%s words, type: %s%d%s, hdr: %s0x%"PRIx32"%s)",
			BLUE, ib_vmid, RST,
			YELLOW, data->stack[data->sp].ib_addr, RST,
			YELLOW, ib_addr - data->stack[data->sp].ib_addr, RST,
			BMAGENTA, "", header, RST,
			BWHITE, RST, GREEN, opcode, RST, GREEN, subop, RST, GREEN, opcode_name, RST,
			BLUE, nwords, RST,
			BLUE, pkttype, RST,
			BLUE, header, RST);
	} else {
		fprintf(data->stack[data->sp].f, "\n[%s0x%"PRIx32"%s@%s0x%08"PRIx64"%s + %s0x%04"PRIx64"%s]\t[%s%8s0x%08"PRIx32"%s]\t%sOpcode%s %s0x%"PRIx32"%s [%s%s%s] (%s%"PRIu32"%s words, type: %s%d%s, hdr: %s0x%"PRIx32"%s)",
			BLUE, ib_vmid, RST,
			YELLOW, data->stack[data->sp].ib_addr, RST,
			YELLOW, ib_addr - data->stack[data->sp].ib_addr, RST,
			BMAGENTA, "", header, RST,
			BWHITE, RST, GREEN, opcode, RST, GREEN, opcode_name, RST,
			BLUE, nwords, RST,
			BLUE, pkttype, RST,
			BLUE, header, RST);
	}
}

static void add_field(struct umr_stream_decode_ui *ui, uint64_t ib_addr, uint32_t ib_vmid, const char *field_name, uint64_t value, char *str, int ideal_radix, int field_size)
{
	struct ui_data *data = ui->data;
	struct umr_asic *asic = data->asic;
	int i64 = abs(field_size) > 32;
	int use16 = (field_size < 0);
	field_size = abs(field_size);
	if (data->stack[data->sp].f_addr != ib_addr) {
		data->stack[data->sp].f_addr = ib_addr;
		if (!use16) {
			if (!i64) {
				fprintf(data->stack[data->sp].f, "\n[%s0x%"PRIx32"%s@%s0x%08"PRIx64"%s + %s0x%04"PRIx64"%s]\t[%s%8s0x%08"PRIx32"%s]\t|---> ",
					BLUE, ib_vmid, RST,
					YELLOW, data->stack[data->sp].ib_addr, RST,
					YELLOW, ib_addr - data->stack[data->sp].ib_addr, RST,
					BMAGENTA, "", data->stack[data->sp].rawdata[(ib_addr - data->stack[data->sp].b_addr)/4], RST);
			} else {
				fprintf(data->stack[data->sp].f, "\n[%s0x%"PRIx32"%s@%s0x%08"PRIx64"%s + %s0x%04"PRIx64"%s]\t[%s0x%08"PRIx32"%08"PRIx32"%s]\t|---> ",
					BLUE, ib_vmid, RST,
					YELLOW, data->stack[data->sp].ib_addr, RST,
					YELLOW, ib_addr - data->stack[data->sp].ib_addr, RST,
					BMAGENTA,
						data->stack[data->sp].rawdata[1 + (ib_addr - data->stack[data->sp].b_addr)/4],
						data->stack[data->sp].rawdata[(ib_addr - data->stack[data->sp].b_addr)/4],
					RST);
			}
		} else {
			// each rawdata[] only carries 16-bits of data
			if (field_size <= 16) {
				fprintf(data->stack[data->sp].f, "\n[%s0x%"PRIx32"%s@%s0x%08"PRIx64"%s + %s0x%04"PRIx64"%s]\t[%s%12s0x%04"PRIx32"%s]\t|---> ",
					BLUE, ib_vmid, RST,
					YELLOW, data->stack[data->sp].ib_addr, RST,
					YELLOW, ib_addr - data->stack[data->sp].ib_addr, RST,
					BMAGENTA, "", data->stack[data->sp].rawdata[1 + (ib_addr - data->stack[data->sp].b_addr)/2], RST);
			} else if (field_size <= 32) {
				fprintf(data->stack[data->sp].f, "\n[%s0x%"PRIx32"%s@%s0x%08"PRIx64"%s + %s0x%04"PRIx64"%s]\t[%s%8s0x%04"PRIx32"%04"PRIx32"%s]\t|---> ",
					BLUE, ib_vmid, RST,
					YELLOW, data->stack[data->sp].ib_addr, RST,
					YELLOW, ib_addr - data->stack[data->sp].ib_addr, RST,
					BMAGENTA, "",
						data->stack[data->sp].rawdata[2 + (ib_addr - data->stack[data->sp].b_addr)/2],
						data->stack[data->sp].rawdata[1 + (ib_addr - data->stack[data->sp].b_addr)/2],
					RST);
			} else {
				fprintf(data->stack[data->sp].f, "\n[%s0x%"PRIx32"%s@%s0x%08"PRIx64"%s + %s0x%04"PRIx64"%s]\t[%s0x%04"PRIx32"%04"PRIx32"%04"PRIx32"%04"PRIx32"%s]\t|---> ",
					BLUE, ib_vmid, RST,
					YELLOW, data->stack[data->sp].ib_addr, RST,
					YELLOW, ib_addr - data->stack[data->sp].ib_addr, RST,
					BMAGENTA,
						data->stack[data->sp].rawdata[4 + (ib_addr - data->stack[data->sp].b_addr)/2],
						data->stack[data->sp].rawdata[3 + (ib_addr - data->stack[data->sp].b_addr)/2],
						data->stack[data->sp].rawdata[2 + (ib_addr - data->stack[data->sp].b_addr)/2],
						data->stack[data->sp].rawdata[1 + (ib_addr - data->stack[data->sp].b_addr)/2],
					RST);
			}
		}
	} else {
		fprintf(data->stack[data->sp].f, ", ");
	}

	if (field_name && !strcmp(field_name, "REG") && ideal_radix == 16) {
		// register name/value pairs
		fprintf(data->stack[data->sp].f, "%s%s%s=%s0x%"PRIx64"%s",
			RED, str, RST,
			YELLOW, value, RST);
	} else {
		// default
		fprintf(data->stack[data->sp].f, "%s%s%s=", CYAN, field_name, RST);
		if (str)
			fprintf(data->stack[data->sp].f, "[%s%s%s]", RED, str, RST);

		if (str && (ideal_radix == 10 || ideal_radix == 16))
			fprintf(data->stack[data->sp].f, "/");

		switch (ideal_radix) {
			case 10: fprintf(data->stack[data->sp].f, "%s%"PRIu64"%s", BBLUE, value, RST); break;
			case 16: fprintf(data->stack[data->sp].f, "%s0x%"PRIx64"%s", YELLOW, value, RST); break;
		}
	}
}

static void add_shader(struct umr_stream_decode_ui *ui, struct umr_asic *asic, uint64_t ib_addr, uint32_t ib_vmid, struct umr_shaders_pgm *shader)
{
	struct ui_data *data = ui->data;
	char **str;
	int x;

	next_level(ui);
	fprintf(data->stack[data->sp].f, "Shader from 0x%"PRIx32"@[0x%"PRIx64" + 0x%"PRIx64"] at 0x%"PRIx32"@0x%"PRIx64", type %d, size %lu\n", ib_vmid, data->stack[data->sp-1].ib_addr, ib_addr - data->stack[data->sp-1].ib_addr, shader->vmid, shader->addr, shader->type, (unsigned long)shader->size);
	umr_vm_disasm_to_str(asic, asic->options.vm_partition, shader->vmid, shader->addr, 0, shader->size, 0, &str);
	x = 0;
	while (str[x]) {
		fprintf(data->stack[data->sp].f, "%s\n", str[x]);
		free(str[x++]);
	}
	free(str);
	fprintf(data->stack[data->sp].f, "Done disassembly of shader\n\n");
	fclose(data->stack[data->sp].f);
	--(data->sp);
}

static void add_vcn(struct umr_stream_decode_ui *ui, struct umr_asic *asic, struct umr_vcn_cmd_message *vcn)
{
	struct ui_data *data = ui->data;
	FILE * pOut;
	struct umr_vcn_cmd_message *p;

	next_level(ui);
	pOut = data->stack[data->sp].f;
	while (vcn) {
		p = vcn;
		if (vcn->type == 0)
			umr_parse_vcn_dec(asic, vcn, pOut);
		else
			umr_parse_vcn_enc(asic, vcn, pOut);
		if (vcn->buf)
			free(vcn->buf);
		vcn = vcn->next;
		free(p);
	}
	fclose(data->stack[data->sp].f);
	--(data->sp);
}

static void add_data(struct umr_stream_decode_ui *ui, struct umr_asic *asic, uint64_t ib_addr, uint32_t ib_vmid, uint64_t buf_addr, uint32_t buf_vmid, enum UMR_DATABLOCK_ENUM type, uint64_t etype)
{
	struct ui_data *data = ui->data;
	char **txt;

	// don't fetch data blocks if no_follow is enabled
	if (asic->options.no_follow_ib)
		return;

	next_level(ui);
	fprintf(data->stack[data->sp].f, "Data block from 0x%"PRIx32"@[0x%"PRIx64" + 0x%"PRIx64"] at 0x%"PRIx32"@0x%"PRIx64", type %d, ", ib_vmid, data->stack[data->sp-1].ib_addr, ib_addr - data->stack[data->sp-1].ib_addr, buf_vmid, buf_addr, type);

	if (type == UMR_DATABLOCK_MQD_VI || type == UMR_DATABLOCK_MQD_NV) {
		static const char *selnames[] = { "compute", "reserved", "sdma0", "sdma1", "gfx", "mes" };
		enum umr_mqd_engine_sel eng;
		uint32_t mqd[512], x;

		fprintf(data->stack[data->sp].f, "sub-type %"PRIu64"[%s]\n", etype, selnames[etype]);
		switch (etype) { // ENGINE_SEL from MAP_QUEUES packet
			case 0: eng = UMR_MQD_ENGINE_COMPUTE; break;
			case 2:
			case 3: eng = UMR_MQD_ENGINE_SDMA0; break;
			case 4: eng = UMR_MQD_ENGINE_GFX; break;
			case 5: eng = UMR_MQD_ENGINE_MES; break;
			default:
				eng = UMR_MQD_ENGINE_INVALID; break;
		}
		if (!umr_read_vram(asic, asic->options.vm_partition, buf_vmid, buf_addr, 512 * 4, &mqd[0])) {
			txt = umr_mqd_decode_data(eng, asic->family, mqd, "*");
			if (txt) {
				for (x = 0; txt[x]; x++) {
					fprintf(data->stack[data->sp].f, "\t%s\n", txt[x]);
					free(txt[x]);
				}
				free(txt);
			}
		}
	}
	fprintf(data->stack[data->sp].f, "Done output of block\n\n");
	fclose(data->stack[data->sp].f);
	--(data->sp);
}


static void unhandled(struct umr_stream_decode_ui *ui, struct umr_asic *asic, uint64_t ib_addr, uint32_t ib_vmid, void *stream, enum umr_ring_type stream_type)
{
	(void)ui;
	(void)asic;
	(void)ib_addr;
	(void)ib_vmid;
	(void)stream;
	(void)stream_type;
}

static int unhandled_size(struct umr_stream_decode_ui *ui, struct umr_asic *asic, void *stream, enum umr_ring_type stream_type)
{
	(void)ui;
	(void)asic;
	(void)stream;
	(void)stream_type;
	return 1;
}

static void unhandled_subop(struct umr_stream_decode_ui *ui, struct umr_asic *asic, uint64_t ib_addr, uint32_t ib_vmid, void *stream, enum umr_ring_type stream_type)
{
	(void)ui;
	(void)asic;
	(void)ib_addr;
	(void)ib_vmid;
	(void)stream;
	(void)stream_type;
}

static void done(struct umr_stream_decode_ui *ui)
{
	struct ui_data *data = ui->data;
	fprintf(data->stack[data->sp].f, "\nDone decoding IB\n\n");
	fclose(data->stack[data->sp].f);
	--(data->sp);
}

static struct umr_stream_decode_ui umr_ui = { UMR_RING_UNK, start_ib, NULL, start_opcode, add_field, add_shader, add_vcn, add_data, unhandled, unhandled_size, unhandled_subop, done, NULL };

static uint32_t *read_ib_file(struct umr_asic *asic, char *filename, uint32_t *nwords)
{
	FILE *infile;
	char buf[128];
	uint32_t  *data, x;

	infile = fopen(filename, "rb");
	if (!infile) {
		asic->err_msg("Cannot open IB file");
		return NULL;
	}

	if (strstr(filename, ".ring")) {
		uint32_t size;
		fseek(infile, 0, SEEK_END);
		size = ftell(infile) - 12;
		fseek(infile, 12, SEEK_SET);
		data = calloc(1, size);
		fread(data, 1, size, infile);
		*nwords = size / 4;
		fclose(infile);
		return data;
	}

	if (strstr(filename, ".bin")) {
		uint32_t size;
		fseek(infile, 0, SEEK_END);
		size = ftell(infile) ;
		fseek(infile, 0, SEEK_SET);
		data = calloc(1, size);
		fread(data, 1, size, infile);
		*nwords = size / 4;
		fclose(infile);
		return data;
	}

	data = calloc(1024, sizeof(*data));
	if (!data) {
		fclose(infile);
		asic->err_msg("[ERROR]: Out of memory\n");
		return NULL;
	}


	x = 0;
	while (fgets(buf, sizeof(buf)-1, infile) != NULL) {
		// skip any line that isn't just a hex value
		if (sscanf(buf, "%"SCNx32, &data[x]) == 1) {
			++x;
			if (!(x & 1023)) {
				void *tmp = realloc(data, sizeof(*data) * (x + 1024));
				if (tmp) {
					data = tmp;
				} else {
					asic->err_msg("[ERROR]: Out of memory\n");
					free(data);
					fclose(infile);
					return NULL;
				}
			}
		}
	}
	fclose(infile);
	*nwords = x;
	return data;
}

void umr_ring_stream_present(struct umr_asic *asic, char *ringname, int start, int end, uint32_t vmid, uint64_t addr, uint32_t *words, uint32_t nwords, enum umr_ring_type rt)
{
	struct umr_packet_stream *str = NULL;
	struct umr_stream_decode_ui ui;
	int x;
	char tmpname[64], buf[256];
	FILE *f;
	struct ui_data *data;

	if (rt == UMR_RING_UNK)
		return;

	// print decode str
	ui = umr_ui;
	ui.rt = rt;
	data = ui.data = calloc(1, sizeof(struct ui_data));
	data->sp = -1;
	data->asic = asic;

	switch (rt) {
		case UMR_RING_PM4:
		case UMR_RING_PM4_LITE:
		case UMR_RING_SDMA:
		case UMR_RING_MES:
		case UMR_RING_VPE:
		case UMR_RING_UMSCH:
		case UMR_RING_GUESS:
		case UMR_RING_HSA:
		case UMR_RING_VCN_ENC:
		case UMR_RING_VCN_DEC:
			if (ringname)
				str = umr_packet_decode_ring(asic, &ui, ringname, asic->options.halt_waves, &start, &end, rt);
			else if (words)
				str = umr_packet_decode_buffer(asic, &ui, vmid, addr, words, nwords, rt);
			else
				str = umr_packet_decode_vm_buffer(asic, &ui, vmid, addr, nwords, rt);
			break;
		case UMR_RING_UNK:
			asic->err_msg("[BUG]: Unknown ring type passed to ring stream present()\n");
			break;
	}

	if (str) {
		switch (str->type) {
			case UMR_RING_PM4:
			case UMR_RING_PM4_LITE:
			case UMR_RING_SDMA:
			case UMR_RING_MES:
			case UMR_RING_VPE:
			case UMR_RING_UMSCH:
			case UMR_RING_HSA:
			case UMR_RING_VCN_ENC:
			case UMR_RING_VCN_DEC:
				umr_packet_disassemble_stream(str, ringname ? (uint64_t)(start * 4) : addr, vmid, 0, 0, ~0UL, 1, 0);
				break;
			case UMR_RING_GUESS:
			case UMR_RING_UNK:
				asic->err_msg("[BUG]: Unknown ring type passed to ring stream present()\n");
				break;
		}

		for (x = 0; x < data->no; x++) {
			sprintf(tmpname, "/tmp/umr_ring_out.%d", x);
			f = fopen(tmpname, "r");
			while (fgets(buf, sizeof buf, f)) {
				printf("%s", buf);
			}
			fclose(f);
			remove(tmpname);
		}

		switch (str->type) {
			case UMR_RING_PM4:
			case UMR_RING_PM4_LITE:
			case UMR_RING_SDMA:
			case UMR_RING_MES:
			case UMR_RING_VPE:
			case UMR_RING_UMSCH:
			case UMR_RING_HSA:
			case UMR_RING_VCN_ENC:
			case UMR_RING_VCN_DEC:
				umr_packet_free(str);
				break;
			case UMR_RING_GUESS:
			case UMR_RING_UNK:
				asic->err_msg("[BUG]: Unknown ring type passed to ring stream present()\n");
				break;
		}

	}
	free(ui.data);
}

void umr_read_ring_stream(struct umr_asic *asic, char *ringpath)
{
	char ringname[32], from[32], to[32], fname[128];
	int  enable_decoder, start, end, ring_or_file = 0;
	uint32_t vmid = 0, nwords, *words = NULL;
	uint64_t addr = 0;
	int rts[] = {
		UMR_RING_GUESS, UMR_RING_VPE, UMR_RING_MES, UMR_RING_SDMA,
		UMR_RING_PM4, UMR_RING_UMSCH, UMR_RING_HSA, UMR_RING_VCN_DEC,
		UMR_RING_VCN_ENC };

	start = end = 0;
	nwords = 0;
	fname[0] = 0;
	if (sscanf(ringpath, "%d/%"SCNx32"@0x%"SCNx64".%"SCNx32, &enable_decoder, &vmid, &addr, &nwords) == 4) {
		ring_or_file = 1; // ring
	} else if (sscanf(ringpath, "%d/%s", &enable_decoder, fname) == 2) {
		ring_or_file = 0; // file
	} else {
		memset(ringname, 0, sizeof ringname);
		memset(from, 0, sizeof from);
		memset(to, 0, sizeof to);
		if (sscanf(ringpath, "%[a-z0-9._][%[.0-9]:%[.0-9]]", ringname, from, to) < 1) {
			printf("Invalid ringpath\n");
			return;
		}

		enable_decoder = 0;

		/* default to reading entire ring */
		if (!from[0]) {
			start = 0;
			end   = 2000000;
		} else {
			if (from[0] == '.') {
				if (to[0] == 0 || to[0] == '.') {
					/* Notation: [.] or [.:.], meaning
					 * [rptr, wptr].
					 */
					start = -1;
					end = -1;
				} else {
					/* Notation: [.:k], k >=0, meaning
					 * [rptr, rtpr+k] double-words.
					 */
					start = -1;
					sscanf(to, "%d", &end);
				}
			} else {
				sscanf(from, "%d", &start);

				if (to[0] != 0 && to[0] != '.') {
					/* [k:r] ==> absolute [k, r].
					 */
					sscanf(to, "%d", &end);
				} else {
					/* to[0] is 0 or '.',
					 * [k] or [k:.] ==> [wptr - k, wptr]
					 */
					end = -1;
				}
			}
		}
	}

	if (!ring_or_file && fname[0]) {
		vmid = addr = 0;
		words = read_ib_file(asic, fname, &nwords);
	}

	if (enable_decoder < 0 || enable_decoder >= (int)(sizeof(rts)/sizeof(rts[0]))) {
		fprintf(stderr, "[BUG]: Unknown ring type for [%s]\n", ringname);
	} else {
		umr_ring_stream_present(asic, nwords ? NULL : ringname, start, end, vmid, addr, words, nwords, rts[enable_decoder]);
	}
	free(words);
}

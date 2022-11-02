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
#include "umrapp.h"
#include <inttypes.h>

//#define PM4_STREAM

#ifdef PM4_STREAM
//#include <umr.h>

// example opaque data to keep track of offsets
struct demo_ui_data {
	uint64_t off[16]; // address of start of IB so we can compute offsets when printing opcodes/fields
	int i;
	struct umr_asic *asic;
};

static void start_ib(struct umr_pm4_stream_decode_ui *ui, uint64_t ib_addr, uint32_t ib_vmid, uint64_t from_addr, uint32_t from_vmid, uint32_t size, int type)
{
	struct demo_ui_data *data = ui->data;
	data->off[data->i++] = ib_addr;
	printf("Decoding IB at %lu@0x%llx from %lu@0x%llx of %lu words (type %d)\n", (unsigned long)ib_vmid, (unsigned long long)ib_addr, (unsigned long)from_vmid, (unsigned long long)from_addr, (unsigned long)size, type);
}
static void start_opcode(struct umr_pm4_stream_decode_ui *ui, uint64_t ib_addr, uint32_t ib_vmid, int pkttype, uint32_t opcode, uint32_t nwords, char *opcode_name, uint32_t header, const uint32_t* raw_data)
{
	struct demo_ui_data *data = ui->data;
	printf("Opcode 0x%lx [%s] at %lu@[0x%llx + 0x%llx] (%lu words, type: %d)\n", (unsigned long)opcode, opcode_name, (unsigned long)ib_vmid, (unsigned long long)data->off[data->i - 1], (unsigned long long)ib_addr - data->off[data->i - 1], (unsigned long)nwords, pkttype);
}
static void add_field(struct umr_pm4_stream_decode_ui *ui, uint64_t ib_addr, uint32_t ib_vmid, const char *field_name, uint32_t value, char *str, int ideal_radix)
{
	struct demo_ui_data *data = ui->data;
	printf("\t[%lu@0x%llx + 0x%llx] -- %s == ", (unsigned long)ib_vmid, (unsigned long long)data->off[data->i - 1], (unsigned long long)ib_addr - data->off[data->i - 1], field_name);

	if (str) {
		printf("[%s]", str);
	} else {
		switch (ideal_radix) {
			case 10: printf("%llu", (unsigned long long)value); break;
			case 16: printf("0x%llx", (unsigned long long)value); break;
		}
	}

	// if there is radix and str chances are it's a register
	if (ideal_radix && str) {
		struct umr_reg *reg;
		int k;
		char *p = strstr(str, ".");
		if (p)
			reg = umr_find_reg_data(data->asic, p + 1);
		else
			reg = umr_find_reg_data(data->asic, str);
		if (reg) {
			printf("\n");
			for (k = 0; k < reg->no_bits; k++) {
				printf("\t\t0x%08lx == %s.%s\n", (unsigned long)umr_bitslice_reg(data->asic, reg, reg->bits[k].regname, value), str, reg->bits[k].regname);
			}
		}
	}

	printf("\n");
}

static	void add_shader(struct umr_pm4_stream_decode_ui *ui, struct umr_asic *asic, uint64_t ib_addr, uint32_t ib_vmid, struct umr_shaders_pgm *shader)
{
	struct demo_ui_data *data = ui->data;
	(void)asic;
	printf("Shader from %lu@[0x%llx + 0x%llx] at %lu@0x%llx, type %d, size %lu\n", (unsigned long)ib_vmid, (unsigned long long)data->off[data->i - 1], (unsigned long long)ib_addr - data->off[data->i - 1], (unsigned long)shader->vmid, (unsigned long long)shader->addr, shader->type, (unsigned long)shader->size);
}

static void unhandled(struct umr_pm4_stream_decode_ui *ui, struct umr_asic *asic, uint64_t ib_addr, uint32_t ib_vmid, struct umr_pm4_stream *stream)
{
	(void)ui;
	(void)asic;
	(void)ib_addr;
	(void)ib_vmid;
	(void)stream;
}

static void done(struct umr_pm4_stream_decode_ui *ui)
{
	struct demo_ui_data *data = ui->data;
	data->i--;

	printf("Done decoding IB\n");
}

static struct  umr_pm4_stream_decode_ui demo_ui = { start_ib, start_opcode, add_field, add_shader, unhandled, done, NULL };

static void foo(struct umr_asic *asic, uint32_t *data, uint32_t x)
{
	struct umr_pm4_stream *stream, *sstream;
	struct umr_pm4_stream_decode_ui myui;
	myui = demo_ui;

	// assign our opaque structure
	myui.data = calloc(1, sizeof(struct demo_ui_data));
	((struct demo_ui_data*)(myui.data))->asic = asic;

	stream = umr_pm4_decode_stream(asic, UMR_PROCESS_HUB, data, x);
	sstream = umr_pm4_decode_stream_opcodes(asic, &myui, stream, 0, 0, 0, 0, 3, 1); // ~0UL);
	printf("\nand now the rest...\n");
	umr_pm4_decode_stream_opcodes(asic, &myui, sstream, 0, 0, 0, 0, ~0UL, 1);

	free(myui.data);
}

#endif

void umr_ib_read_file(struct umr_asic *asic, char *filename, int pm)
{
	struct umr_ring_decoder decoder;
	int follow_ib;
	FILE *infile;
	char buf[128];
	uint32_t  *data, x;

	memset(&decoder, 0, sizeof decoder);

	data = calloc(sizeof(*data), 1024);
	if (!data) {
		fprintf(stderr, "[ERROR]: Out of memory\n");
		return;
	}

	infile = fopen(filename, "r");
	if (!infile) {
		free(data);
		perror("Cannot open IB file");
		return;
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
					fprintf(stderr, "[ERROR]: Out of memory\n");
					free(data);
					fclose(infile);
					return;
				}
			}
		}
	}
	fclose(infile);

	follow_ib = asic->options.no_follow_ib;
	asic->options.no_follow_ib = 1;
#ifdef PM4_STREAM
	foo(asic, data, x);
#else
	decoder.next_ib_info.vmid = UMR_PROCESS_HUB;
	decoder.next_ib_info.ib_addr = (uintptr_t)data;
	decoder.next_ib_info.size = x * sizeof(*data);
	decoder.pm = pm;
	umr_dump_ib(asic, &decoder);
#endif
	asic->options.no_follow_ib = follow_ib;
	free(data);
}

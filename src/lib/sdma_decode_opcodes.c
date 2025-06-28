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
#include <inttypes.h>

#define BITS(x, a, b) (unsigned long)((x >> (a)) & ((1ULL << ((b)-(a)))-1))

#define VER_VI 3
#define VER_AI 4
#define VER_NV 5

struct sdma_config {
	int ver_maj;
	int ver_min;
	int has_cpv_flag;
	int has_cp_fields;
	uint32_t z_mask;
	uint32_t pitch_mask;
	uint32_t pitch_shift;
};

static char *poll_regmem_funcs[] = { "always", "<", "<=", "==", "!=", ">=", ">", "N/A" };

static uint32_t fetch_word(struct umr_asic *asic, struct umr_sdma_stream *stream, uint32_t off)
{
	if (off >= stream->nwords) {
		if (!(stream->invalid))
			asic->err_msg("[ERROR]: SDMA decoding of opcode (%"PRIx32":%"PRIx32") went out of bounds.\n", stream->opcode, stream->sub_opcode);
		stream->invalid = 1;
		return 0;
	} else {
		return stream->words[off];
	}
}


static void decode_upto_vi(struct umr_asic *asic, struct umr_stream_decode_ui *ui, struct umr_sdma_stream *stream,
			   uint64_t ib_addr, uint32_t ib_vmid, uint64_t from_addr, uint64_t from_vmid, int follow, struct sdma_config *sc)
{
	uint32_t n;
	char str_buf[128];
	(void)from_addr;
	(void)from_vmid;
	switch (stream->opcode) {
		case 0:
			ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "NOP", stream->header_dw, stream->words);
			break;
		case 1: // COPY
			switch (stream->sub_opcode) {
				case 0: // LINEAR
					switch (stream->header_dw & (1UL << 27)) {
						case 0: // not broadcast
							ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "COPY (LINEAR)", stream->header_dw, stream->words);
							ui->add_field(ui, ib_addr + 0, ib_vmid, "BACKWARDS", (stream->header_dw >> 25) & 0x1, NULL, 10, 32);
							ui->add_field(ui, ib_addr + 0, ib_vmid, "BROADCAST", (stream->header_dw >> 27) & 0x1, NULL, 10, 32);
							ui->add_field(ui, ib_addr + 4, ib_vmid, "COPY_COUNT", fetch_word(asic, stream, 0), NULL, 10, 32);
							ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_SW", (fetch_word(asic, stream, 1) >> 16) & 3, NULL, 10, 32);
							ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_HA", (fetch_word(asic, stream, 1) >> 22) & 0x1, NULL, 10, 32);
							ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_SW", (fetch_word(asic, stream, 1) >> 24) & 3, NULL, 10, 32);
							ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_HA", (fetch_word(asic, stream, 1) >> 30) & 0x1, NULL, 10, 32);
							ui->add_field(ui, ib_addr + 12, ib_vmid, "SRC_ADDR_LO", fetch_word(asic, stream, 2), NULL, 16, 32);
							ui->add_field(ui, ib_addr + 16, ib_vmid, "SRC_ADDR_HI", fetch_word(asic, stream, 3), NULL, 16, 32);
							ui->add_field(ui, ib_addr + 20, ib_vmid, "DST_ADDR_LO", fetch_word(asic, stream, 4), NULL, 16, 32);
							ui->add_field(ui, ib_addr + 24, ib_vmid, "DST_ADDR_HI", fetch_word(asic, stream, 5), NULL, 16, 32);
							return;
						case 1: // broadcast
							ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "COPY (LINEAR BROADCAST)", stream->header_dw, stream->words);
							ui->add_field(ui, ib_addr + 0, ib_vmid, "ENCRYPT", (stream->header_dw >> 16) & 0x1, NULL, 10, 32);
							ui->add_field(ui, ib_addr + 0, ib_vmid, "TMZ", (stream->header_dw >> 18) & 0x1, NULL, 10, 32);
							ui->add_field(ui, ib_addr + 0, ib_vmid, "BROADCAST", (stream->header_dw >> 27) & 0x1, NULL, 10, 32);
							ui->add_field(ui, ib_addr + 4, ib_vmid, "COPY_COUNT", fetch_word(asic, stream, 0), NULL, 10, 32);
							ui->add_field(ui, ib_addr + 8, ib_vmid, "DST2_SW", (fetch_word(asic, stream, 1) >> 8) & 3, NULL, 10, 32);
							ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_SW", (fetch_word(asic, stream, 1) >> 16) & 3, NULL, 10, 32);
							ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_SW", (fetch_word(asic, stream, 1) >> 24) & 3, NULL, 10, 32);
							ui->add_field(ui, ib_addr + 12, ib_vmid, "SRC_ADDR_LO", fetch_word(asic, stream, 2), NULL, 16, 32);
							ui->add_field(ui, ib_addr + 16, ib_vmid, "SRC_ADDR_HI", fetch_word(asic, stream, 3), NULL, 16, 32);
							ui->add_field(ui, ib_addr + 20, ib_vmid, "DST_ADDR_LO", fetch_word(asic, stream, 4), NULL, 16, 32);
							ui->add_field(ui, ib_addr + 24, ib_vmid, "DST_ADDR_HI", fetch_word(asic, stream, 5), NULL, 16, 32);
							ui->add_field(ui, ib_addr + 28, ib_vmid, "DST2_ADDR_LO", fetch_word(asic, stream, 6), NULL, 16, 32);
							ui->add_field(ui, ib_addr + 32, ib_vmid, "DST2_ADDR_HI", fetch_word(asic, stream, 7), NULL, 16, 32);
							return;
						default:
							break;
					}
					break;
				case 1: // TILED
					if ((stream->header_dw >> 26) & 0x3) { // L2T BROADCAST or L2T FRAME TO FIELD
						if ((stream->header_dw >> 26) & 0x1) {
							ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "COPY (L2T_FRAME_TO_FIELD)", stream->header_dw, stream->words);
						} else {
							ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "COPY (L2T_BROADCAST)", stream->header_dw, stream->words);
						}
						ui->add_field(ui, ib_addr + 0, ib_vmid, "ENCRYPT", (stream->header_dw >> 16) & 0x1, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 0, ib_vmid, "TMZ", (stream->header_dw >> 18) & 0x1, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 0, ib_vmid, "VIDEOCOPY", (stream->header_dw >> 26) & 0x1, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 0, ib_vmid, "BROADCAST", (stream->header_dw >> 27) & 0x1, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 4, ib_vmid, "TILED_ADDR0_LO", fetch_word(asic, stream, 0), NULL, 16, 32);
						ui->add_field(ui, ib_addr + 8, ib_vmid, "TILED_ADDR0_HI", fetch_word(asic, stream, 1), NULL, 16, 32);
						ui->add_field(ui, ib_addr + 12, ib_vmid, "TILED_ADDR1_LO", fetch_word(asic, stream, 2), NULL, 16, 32);
						ui->add_field(ui, ib_addr + 16, ib_vmid, "TILED_ADDR1_HI", fetch_word(asic, stream, 3), NULL, 16, 32);
						ui->add_field(ui, ib_addr + 20, ib_vmid, "PITCH_IN_TILE", fetch_word(asic, stream, 4) & 0x7FF, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 20, ib_vmid, "HEIGHT", (fetch_word(asic, stream, 4) >> 16) & 0x3FFF, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 24, ib_vmid, "SLICE_PITCH", fetch_word(asic, stream, 5) & 0x3FFFFF, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 28, ib_vmid, "ELEMENT_SIZE", fetch_word(asic, stream, 6) & 0x7, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 28, ib_vmid, "ARRAY_MODE", (fetch_word(asic, stream, 6) >> 3) & 0xF, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 28, ib_vmid, "MIT_MODE", (fetch_word(asic, stream, 6) >> 8) & 0x7, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 28, ib_vmid, "TILESPLIT_SIZE", (fetch_word(asic, stream, 6) >> 11) & 0x7, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 28, ib_vmid, "BANK_W", (fetch_word(asic, stream, 6) >> 15) & 0x3, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 28, ib_vmid, "BANK_H", (fetch_word(asic, stream, 6) >> 18) & 0x3, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 28, ib_vmid, "NUM_BANK", (fetch_word(asic, stream, 6) >> 21) & 0x3, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 28, ib_vmid, "MAT_ASPT", (fetch_word(asic, stream, 6) >> 24) & 0x3, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 28, ib_vmid, "PIPE_CONFIG", (fetch_word(asic, stream, 6) >> 26) & 0x1F, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 32, ib_vmid, "X", fetch_word(asic, stream, 7) & 0x3FFF, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 32, ib_vmid, "Y", (fetch_word(asic, stream, 7) >> 16) & 0x3FFF, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 36, ib_vmid, "Z", fetch_word(asic, stream, 8) & 0x7FF, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 40, ib_vmid, "DST2_SW", (fetch_word(asic, stream, 9) >> 8) & 0x3, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 40, ib_vmid, "LINEAR_SW", (fetch_word(asic, stream, 9) >> 16) & 0x3, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 40, ib_vmid, "TILE_SW", (fetch_word(asic, stream, 9) >> 24) & 0x3, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 44, ib_vmid, "LINEAR_ADDR_LO", fetch_word(asic, stream, 10), NULL, 16, 32);
						ui->add_field(ui, ib_addr + 48, ib_vmid, "LINEAR_ADDR_HI", fetch_word(asic, stream, 11), NULL, 16, 32);
						ui->add_field(ui, ib_addr + 52, ib_vmid, "LINEAR_PITCH", fetch_word(asic, stream, 12) & 0x7FFFF, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 56, ib_vmid, "COUNT", fetch_word(asic, stream, 13) & 0xFFFFF, NULL, 10, 32);
						return;
					} else {
						ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "COPY (TILED)", stream->header_dw, stream->words);
						ui->add_field(ui, ib_addr + 0, ib_vmid, "ENCRYPT", (stream->header_dw >> 16) & 0x1, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 0, ib_vmid, "TMZ", (stream->header_dw >> 18) & 0x1, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 0, ib_vmid, "VIDEOCOPY", (stream->header_dw >> 26) & 0x1, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 0, ib_vmid, "BROADCAST", (stream->header_dw >> 27) & 0x1, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 0, ib_vmid, "DETILE", (stream->header_dw >> 31) & 0x1, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 4, ib_vmid, "TILED_ADDR_LO", fetch_word(asic, stream, 0), NULL, 16, 32);
						ui->add_field(ui, ib_addr + 8, ib_vmid, "TILED_ADDR_HI", fetch_word(asic, stream, 1), NULL, 16, 32);
						ui->add_field(ui, ib_addr + 12, ib_vmid, "PITCH_IN_TILE", fetch_word(asic, stream, 2) & 0x7FF, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 12, ib_vmid, "HEIGHT", (fetch_word(asic, stream, 2) >> 16) & 0x3FFF, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 16, ib_vmid, "SLICE_PITCH", fetch_word(asic, stream, 3) & 0x3FFFFF, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 20, ib_vmid, "ELEMENT_SIZE", fetch_word(asic, stream, 4) & 0x7, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 20, ib_vmid, "ARRAY_MODE", (fetch_word(asic, stream, 4) >> 3) & 0xF, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 20, ib_vmid, "MIT_MODE", (fetch_word(asic, stream, 4) >> 8) & 0x7, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 20, ib_vmid, "TILESPLIT_SIZE", (fetch_word(asic, stream, 4) >> 11) & 0x7, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 20, ib_vmid, "BANK_W", (fetch_word(asic, stream, 4) >> 15) & 0x3, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 20, ib_vmid, "BANK_H", (fetch_word(asic, stream, 4) >> 18) & 0x3, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 20, ib_vmid, "NUM_BANK", (fetch_word(asic, stream, 4) >> 21) & 0x3, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 20, ib_vmid, "MAT_ASPT", (fetch_word(asic, stream, 4) >> 24) & 0x3, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 20, ib_vmid, "PIPE_CONFIG", (fetch_word(asic, stream, 4) >> 26) & 0x1F, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 24, ib_vmid, "X", fetch_word(asic, stream, 5) & 0x3FFF, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 24, ib_vmid, "Y", (fetch_word(asic, stream, 5) >> 16) & 0x3FFF, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 28, ib_vmid, "Z", fetch_word(asic, stream, 6) & 0x1FFF, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 28, ib_vmid, "LINEAR_SW", (fetch_word(asic, stream, 6) >> 16) & 0x3, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 28, ib_vmid, "TILE_SW", (fetch_word(asic, stream, 6) >> 24) & 0x3, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 32, ib_vmid, "LINEAR_ADDR_LO", fetch_word(asic, stream, 7), NULL, 16, 32);
						ui->add_field(ui, ib_addr + 36, ib_vmid, "LINEAR_ADDR_HI", fetch_word(asic, stream, 8), NULL, 16, 32);
						ui->add_field(ui, ib_addr + 40, ib_vmid, "LINEAR_PITCH", fetch_word(asic, stream, 9) & 0x7FFFF, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 44, ib_vmid, "COUNT", fetch_word(asic, stream, 10) & 0xFFFFF, NULL, 10, 32);
						return;
					}
				case 3: // SOA
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "COPY (STRUCT)", stream->header_dw, stream->words);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "TMZ", (stream->header_dw >> 18) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "DETILE", (stream->header_dw >> 31) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "SB_ADDR_LO", fetch_word(asic, stream, 0), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "SB_ADDR_HI", fetch_word(asic, stream, 1), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "START_INDEX", fetch_word(asic, stream, 2), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 16, ib_vmid, "COUNT", fetch_word(asic, stream, 3), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "STRIDE", fetch_word(asic, stream, 4) & 0x7FF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "LINEAR_SW", (fetch_word(asic, stream, 4) >> 16) & 0x3, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "STRUCT_SW", (fetch_word(asic, stream, 4) >> 24) & 0x3, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "LINEAR_ADDR_LO", fetch_word(asic, stream, 5), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 28, ib_vmid, "LINEAR_ADDR_HI", fetch_word(asic, stream, 6), NULL, 16, 32);
					return;
				case 4: // LINEAR_SUB_WINDOW
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "COPY (LINEAR_SUB_WINDOW)", stream->header_dw, stream->words);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "TMZ", (stream->header_dw >> 18) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "ELEMENTSIZE", (stream->header_dw >> 29) & 0x7, NULL, 10, 32);

					ui->add_field(ui, ib_addr + 4, ib_vmid, "SRC_ADDR_LO", fetch_word(asic, stream, 0), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_ADDR_HI", fetch_word(asic, stream, 1), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "SRC_X", fetch_word(asic, stream, 2) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "SRC_Y", (fetch_word(asic, stream, 2) >> 16) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 16, ib_vmid, "SRC_Z", fetch_word(asic, stream, 3) & sc->z_mask, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 16, ib_vmid, "SRC_PITCH", (fetch_word(asic, stream, 3) >> sc->pitch_shift) & sc->pitch_mask, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "SRC_SLICE_PITCH", fetch_word(asic, stream, 4) & 0xFFFFFFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "DST_ADDR_LO", fetch_word(asic, stream, 5), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 28, ib_vmid, "DST_ADDR_HI", fetch_word(asic, stream, 6), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 32, ib_vmid, "DST_X", fetch_word(asic, stream, 7) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 32, ib_vmid, "DST_Y", (fetch_word(asic, stream, 7) >> 16) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 36, ib_vmid, "DST_Z", fetch_word(asic, stream, 8) & sc->z_mask, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 36, ib_vmid, "DST_PITCH", (fetch_word(asic, stream, 8) >> sc->pitch_shift) & sc->pitch_mask, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 40, ib_vmid, "DST_SLICE_PITCH", fetch_word(asic, stream, 9) & 0xFFFFFFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 44, ib_vmid, "RECT_X", fetch_word(asic, stream, 10) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 44, ib_vmid, "RECT_Y", (fetch_word(asic, stream, 10) >> 16) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 48, ib_vmid, "RECT_Z", fetch_word(asic, stream, 11) & 0x1FFF, NULL, 10, 32);

					ui->add_field(ui, ib_addr + 48, ib_vmid, "DST_SW", (fetch_word(asic, stream, 11) >> 16) & 0x3, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 48, ib_vmid, "DST_HA", (fetch_word(asic, stream, 11) >> 22) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 48, ib_vmid, "SRC_SW", (fetch_word(asic, stream, 11) >> 24) & 0x3, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 48, ib_vmid, "SRC_HA", (fetch_word(asic, stream, 11) >> 30) & 0x1, NULL, 10, 32);
					return;
				case 5: // TILED_SUB_WINDOW
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "COPY (TILED_SUB_WINDOW)", stream->header_dw, stream->words);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "TMZ", (stream->header_dw >> 18) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "DETILE", stream->header_dw >> 31, NULL, 16, 32);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "TILED_ADDR_LO", fetch_word(asic, stream, 0), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "TILED_ADDR_HI", fetch_word(asic, stream, 1), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "TILED_X", (fetch_word(asic, stream, 2) >> 0) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "TILED_Y", (fetch_word(asic, stream, 2) >> 16) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 16, ib_vmid, "TILED_Z", (fetch_word(asic, stream, 3) >> 0) & sc->z_mask, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 16, ib_vmid, "TILED_PITCH", (fetch_word(asic, stream, 3) >> 16) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "PITCH_IN_TILE", fetch_word(asic, stream, 4) & 0xFFFFFFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "ELEMENT_SIZE", (fetch_word(asic, stream, 5) >> 0) & 0x7, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "ARRAY_MODE", (fetch_word(asic, stream, 5) >> 3) & 0xF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "MIT_MODE", (fetch_word(asic, stream, 5) >> 8) & 0x7, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "TILESPLIT_SIZE", (fetch_word(asic, stream, 5) >> 11) & 0x7, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "BANK_W", (fetch_word(asic, stream, 5) >> 15) & 0x3, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "BANK_H", (fetch_word(asic, stream, 5) >> 18) & 0x3, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "NUM_BANK", (fetch_word(asic, stream, 5) >> 21) & 0x3, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "MAT_ASPT", (fetch_word(asic, stream, 5) >> 24) & 0x3, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "PIPE_CONFIG", (fetch_word(asic, stream, 5) >> 26) & 0x1F, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 28, ib_vmid, "LINEAR_ADDR_LO", fetch_word(asic, stream, 6), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 32, ib_vmid, "LINEAR_ADDR_HI", fetch_word(asic, stream, 7), NULL, 16, 32);

					ui->add_field(ui, ib_addr + 36, ib_vmid, "LINEAR_X", (fetch_word(asic, stream, 8) >> 0) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 36, ib_vmid, "LINEAR_Y", (fetch_word(asic, stream, 8) >> 0) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 40, ib_vmid, "LINEAR_Z", (fetch_word(asic, stream, 9) >> 0) & sc->z_mask, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 40, ib_vmid, "LINEAR_PITCH", (fetch_word(asic, stream, 9) >> 16) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 44, ib_vmid, "LINEAR_SLICE_PITCH", fetch_word(asic, stream, 10) & 0xFFFFFFF, NULL, 10, 32);

					ui->add_field(ui, ib_addr + 48, ib_vmid, "RECT_X", (fetch_word(asic, stream, 11) >> 0) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 48, ib_vmid, "RECT_Y", (fetch_word(asic, stream, 11) >> 16) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 52, ib_vmid, "RECT_Z", (fetch_word(asic, stream, 12) >> 0) & sc->z_mask, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 52, ib_vmid, "LINEAR_SW", (fetch_word(asic, stream, 12) >> 16) & 0x3, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 52, ib_vmid, "TILE_SW", (fetch_word(asic, stream, 12) >> 22) & 0x3, NULL, 10, 32);
					return;
				case 6: // T2T_SUB_WINDOW
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "COPY (T2T_SUB_WINDOW)", stream->header_dw, stream->words);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "TMZ", (stream->header_dw >> 18) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "SRC_ADDR_LO", fetch_word(asic, stream, 0), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_ADDR_HI", fetch_word(asic, stream, 1), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "SRC_X", (fetch_word(asic, stream, 2) >> 0) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "SRC_Y", (fetch_word(asic, stream, 2) >> 16) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 16, ib_vmid, "SRC_Z", (fetch_word(asic, stream, 3) >> 0) & sc->z_mask, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 16, ib_vmid, "SRC_PITCH", (fetch_word(asic, stream, 3) >> 16) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "SRC_SLICE_PITCH", fetch_word(asic, stream, 4) & 0xFFFFFFF, NULL, 10, 32);

					ui->add_field(ui, ib_addr + 24, ib_vmid, "SRC_ELEMENT_SIZE", (fetch_word(asic, stream, 5) >> 0) & 0x7, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "SRC_ARRAY_MODE", (fetch_word(asic, stream, 5) >> 3) & 0xF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "SRC_MIT_MODE", (fetch_word(asic, stream, 5) >> 8) & 0x7, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "SRC_TILESPLIT_SIZE", (fetch_word(asic, stream, 5) >> 11) & 0x7, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "SRC_BANK_W", (fetch_word(asic, stream, 5) >> 15) & 0x3, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "SRC_BANK_H", (fetch_word(asic, stream, 5) >> 18) & 0x3, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "SRC_NUM_BANKS", (fetch_word(asic, stream, 5) >> 21) & 0x3, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "SRC_MAT_ASPT", (fetch_word(asic, stream, 5) >> 24) & 0x3, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "SRC_PIPE_CONFIG", (fetch_word(asic, stream, 5) >> 26) & 0x1F, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 28, ib_vmid, "DST_ADDR_LO", fetch_word(asic, stream, 6), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 32, ib_vmid, "DST_ADDR_HI", fetch_word(asic, stream, 7), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 36, ib_vmid, "DST_X", (fetch_word(asic, stream, 8) >> 0) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 36, ib_vmid, "DST_Y", (fetch_word(asic, stream, 8) >> 16) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 40, ib_vmid, "DST_Z", (fetch_word(asic, stream, 9) >> 0) & sc->z_mask, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 40, ib_vmid, "DST_PITCH", (fetch_word(asic, stream, 9) >> 16) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 44, ib_vmid, "DST_SLICE_PITCH", fetch_word(asic, stream, 10) & 0xFFFFFFF, NULL, 10, 32);

					ui->add_field(ui, ib_addr + 48, ib_vmid, "DST_ARRAY_MODE", (fetch_word(asic, stream, 11) >> 0) & 0x7, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 48, ib_vmid, "DST_ARRAY_MODE", (fetch_word(asic, stream, 11) >> 3) & 0xF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 48, ib_vmid, "DST_MIT_MODE", (fetch_word(asic, stream, 11) >> 8) & 0x7, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 48, ib_vmid, "DST_TILESPLIT_SIZE", (fetch_word(asic, stream, 11) >> 11) & 0x7, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 48, ib_vmid, "DST_BANK_W", (fetch_word(asic, stream, 11) >> 15) & 0x3, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 48, ib_vmid, "DST_BANK_H", (fetch_word(asic, stream, 11) >> 18) & 0x3, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 48, ib_vmid, "DST_NUM_BANKS", (fetch_word(asic, stream, 11) >> 21) & 0x3, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 48, ib_vmid, "DST_MAT_ASPT", (fetch_word(asic, stream, 11) >> 24) & 0x3, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 48, ib_vmid, "DST_PIPE_CONFIG", (fetch_word(asic, stream, 11) >> 26) & 0x1F, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 52, ib_vmid, "RECT_X", (fetch_word(asic, stream, 12) >> 0) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 52, ib_vmid, "RECT_Y", (fetch_word(asic, stream, 12) >> 16) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 56, ib_vmid, "RECT_Z", (fetch_word(asic, stream, 13) >> 0) & sc->z_mask, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 56, ib_vmid, "DST_SW", (fetch_word(asic, stream, 13) >> 16) & 0x3, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 56, ib_vmid, "SRC_SW", (fetch_word(asic, stream, 13) >> 22) & 0x3, NULL, 10, 32);
					return;
				case 7: // DIRTY_PAGE
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "COPY (DIRTY_PAGE)", stream->header_dw, stream->words);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "TMZ", (stream->header_dw >> 18) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "ALL", (stream->header_dw >> 31) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "COUNT", (fetch_word(asic, stream, 0) >> 0) & 0x3FFFFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_SW", (fetch_word(asic, stream, 1) >> 16) & 0x3, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_GCC", (fetch_word(asic, stream, 1) >> 19) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_SYS", (fetch_word(asic, stream, 1) >> 20) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_SNOOP", (fetch_word(asic, stream, 1) >> 22) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_GPA", (fetch_word(asic, stream, 1) >> 23) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_SW", (fetch_word(asic, stream, 1) >> 24) & 0x3, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_SYS", (fetch_word(asic, stream, 1) >> 28) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_SNOOP", (fetch_word(asic, stream, 1) >> 30) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_GPA", (fetch_word(asic, stream, 1) >> 31) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "SRC_ADDR_LO", fetch_word(asic, stream, 2), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 16, ib_vmid, "SRC_ADDR_HI", fetch_word(asic, stream, 3), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "DST_ADDR_LO", fetch_word(asic, stream, 4), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "DST_ADDR_HI", fetch_word(asic, stream, 5), NULL, 16, 32);
					return;
				case 8: // LINEAR_PHY
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "COPY (LINEAR_PHY)", stream->header_dw, stream->words);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "TMZ", (stream->header_dw >> 18) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "COUNT", (fetch_word(asic, stream, 0) >> 0) & 0x3FFFFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_GCC", (fetch_word(asic, stream, 1) >> 19) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_SYS", (fetch_word(asic, stream, 1) >> 20) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_LOG", (fetch_word(asic, stream, 1) >> 21) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_SNOOP", (fetch_word(asic, stream, 1) >> 22) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_GPA", (fetch_word(asic, stream, 1) >> 23) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_GCC", (fetch_word(asic, stream, 1) >> 27) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_SYS", (fetch_word(asic, stream, 1) >> 28) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_SNOOP", (fetch_word(asic, stream, 1) >> 30) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_GPA", (fetch_word(asic, stream, 1) >> 31) & 0x1, NULL, 10, 32);
					for (n = 2; n + 3 < stream->nwords; n += 4) {
						uint32_t addr_idx = (n - 2) / 4;
						sprintf(str_buf, "SRC_ADDR%"PRIu32"_LO", addr_idx);
						ui->add_field(ui, ib_addr + 12 + 4 * (n - 2), ib_vmid, str_buf, fetch_word(asic, stream, n), NULL, 16, 32);
						sprintf(str_buf, "SRC_ADDR%"PRIu32"_HI", addr_idx);
						ui->add_field(ui, ib_addr + 16 + 4 * (n - 2), ib_vmid, str_buf, fetch_word(asic, stream, n + 1), NULL, 16, 32);
						sprintf(str_buf, "DST_ADDR%"PRIu32"_LO", addr_idx);
						ui->add_field(ui, ib_addr + 20 + 4 * (n - 2), ib_vmid, str_buf, fetch_word(asic, stream, n + 2), NULL, 16, 32);
						sprintf(str_buf, "DST_ADDR%"PRIu32"_HI", addr_idx);
						ui->add_field(ui, ib_addr + 24 + 4 * (n - 2), ib_vmid, str_buf, fetch_word(asic, stream, n + 3), NULL, 16, 32);
					}
					return;
				case 16: // LINEAR_BC
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "COPY (LINEAR_BC)", stream->header_dw, stream->words);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "COUNT", (fetch_word(asic, stream, 0) >> 0) & 0x3FFFFF, NULL, 10, 32);

					ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_SW", (fetch_word(asic, stream, 1) >> 16) & 0x3, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_HA", (fetch_word(asic, stream, 1) >> 19) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_SW", (fetch_word(asic, stream, 1) >> 24) & 0x3, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_HA", (fetch_word(asic, stream, 1) >> 27) & 0x1, NULL, 10, 32);

					ui->add_field(ui, ib_addr + 12, ib_vmid, "SRC_ADDR_LO", fetch_word(asic, stream, 2), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 16, ib_vmid, "SRC_ADDR_HI", fetch_word(asic, stream, 3), NULL, 16, 32);

					ui->add_field(ui, ib_addr + 20, ib_vmid, "DST_ADDR_LO", fetch_word(asic, stream, 4), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "DST_ADDR_HI", fetch_word(asic, stream, 5), NULL, 16, 32);
					return;
				case 17: // TILED_BC
					if ((stream->header_dw >> 26) & 0x3) { // L2T BROADCAST or L2T FRAME TO FIELD
						if ((stream->header_dw >> 26) & 0x1) {
							ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "COPY (L2T_FRAME_TO_FIELD_BC)", stream->header_dw, stream->words);
						} else {
							ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "COPY (L2T_BROADCAST_BC)", stream->header_dw, stream->words);
						}
						ui->add_field(ui, ib_addr + 0, ib_vmid, "VIDEOCOPY", (stream->header_dw >> 26) & 0x1, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 0, ib_vmid, "BROADCAST", (stream->header_dw >> 27) & 0x1, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 0, ib_vmid, "DETILE", stream->header_dw >> 31, NULL, 16, 32);
						ui->add_field(ui, ib_addr + 4, ib_vmid, "TILED_ADDR0_LO", fetch_word(asic, stream, 0), NULL, 16, 32);
						ui->add_field(ui, ib_addr + 8, ib_vmid, "TILED_ADDR0_HI", fetch_word(asic, stream, 1), NULL, 16, 32);
						ui->add_field(ui, ib_addr + 12, ib_vmid, "TILED_ADDR1_LO", fetch_word(asic, stream, 2), NULL, 16, 32);
						ui->add_field(ui, ib_addr + 16, ib_vmid, "TILED_ADDR1_HI", fetch_word(asic, stream, 3), NULL, 16, 32);
						ui->add_field(ui, ib_addr + 20, ib_vmid, "WIDTH", fetch_word(asic, stream, 4) & 0x3FFF, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 24, ib_vmid, "HEIGHT", fetch_word(asic, stream, 5) & 0x3FFF, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 24, ib_vmid, "DEPTH", (fetch_word(asic, stream, 5) >> 16) & 0x1FFF, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 28, ib_vmid, "ELEMENT_SIZE", fetch_word(asic, stream, 6) & 0x7, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 28, ib_vmid, "ARRAY_MODE", (fetch_word(asic, stream, 6) >> 3) & 0xF, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 28, ib_vmid, "MIT_MODE", (fetch_word(asic, stream, 6) >> 8) & 0x7, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 28, ib_vmid, "TILESPLIT_SIZE", (fetch_word(asic, stream, 6) >> 11) & 0x7, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 28, ib_vmid, "BANK_W", (fetch_word(asic, stream, 6) >> 15) & 0x3, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 28, ib_vmid, "BANK_H", (fetch_word(asic, stream, 6) >> 18) & 0x3, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 28, ib_vmid, "NUM_BANK", (fetch_word(asic, stream, 6) >> 21) & 0x3, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 28, ib_vmid, "MAT_ASPT", (fetch_word(asic, stream, 6) >> 24) & 0x3, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 28, ib_vmid, "PIPE_CONFIG", (fetch_word(asic, stream, 6) >> 26) & 0x1F, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 32, ib_vmid, "X", fetch_word(asic, stream, 7) & 0x3FFF, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 32, ib_vmid, "Y", (fetch_word(asic, stream, 7) >> 16) & 0x3FFF, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 36, ib_vmid, "Z", fetch_word(asic, stream, 8) & 0x7FF, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 40, ib_vmid, "DST2_SW", (fetch_word(asic, stream, 9) >> 8) & 0x3, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 40, ib_vmid, "LINEAR_SW", (fetch_word(asic, stream, 9) >> 16) & 0x3, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 40, ib_vmid, "TILE_SW", (fetch_word(asic, stream, 9) >> 24) & 0x3, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 44, ib_vmid, "LINEAR_ADDR_LO", fetch_word(asic, stream, 10), NULL, 16, 32);
						ui->add_field(ui, ib_addr + 48, ib_vmid, "LINEAR_ADDR_HI", fetch_word(asic, stream, 11), NULL, 16, 32);
						ui->add_field(ui, ib_addr + 52, ib_vmid, "LINEAR_PITCH", fetch_word(asic, stream, 12) & 0x7FFFF, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 56, ib_vmid, "LINEAR_SLICE_PITCH", fetch_word(asic, stream, 13), NULL, 10, 32);
						ui->add_field(ui, ib_addr + 60, ib_vmid, "COUNT", fetch_word(asic, stream, 14) & 0x3FFFFF, NULL, 10, 32);
					} else {
						ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "COPY (TILED_BC)", stream->header_dw, stream->words);
						ui->add_field(ui, ib_addr + 0, ib_vmid, "VIDEOCOPY", (stream->header_dw >> 26) & 0x1, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 0, ib_vmid, "BROADCAST", (stream->header_dw >> 27) & 0x1, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 0, ib_vmid, "DETILE", stream->header_dw >> 31, NULL, 16, 32);
						ui->add_field(ui, ib_addr + 4, ib_vmid, "TILED_ADDR_LO", fetch_word(asic, stream, 0), NULL, 16, 32);
						ui->add_field(ui, ib_addr + 8, ib_vmid, "TILED_ADDR_HI", fetch_word(asic, stream, 1), NULL, 16, 32);
						ui->add_field(ui, ib_addr + 12, ib_vmid, "WIDTH", (fetch_word(asic, stream, 2) >> 0) & 0x3FFF, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 16, ib_vmid, "HEIGHT", (fetch_word(asic, stream, 3) >> 0) & 0x3FFF, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 16, ib_vmid, "DEPTH", (fetch_word(asic, stream, 3) >> 16) & 0x7FF, NULL, 10, 32);

						ui->add_field(ui, ib_addr + 20, ib_vmid, "ELEMENT_SIZE", (fetch_word(asic, stream, 4) >> 0) & 0x7, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 20, ib_vmid, "ARRAY_MODE", (fetch_word(asic, stream, 4) >> 3) & 0xF, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 20, ib_vmid, "MIT_MODE", (fetch_word(asic, stream, 4) >> 8) & 0x7, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 20, ib_vmid, "TIMESPLIT_SIZE", (fetch_word(asic, stream, 4) >> 11) & 0x7, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 20, ib_vmid, "BANK_W", (fetch_word(asic, stream, 4) >> 15) & 0x3, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 20, ib_vmid, "BANK_H", (fetch_word(asic, stream, 4) >> 18) & 0x3, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 20, ib_vmid, "NUM_BANK", (fetch_word(asic, stream, 4) >> 21) & 0x3, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 20, ib_vmid, "MAT_ASPT", (fetch_word(asic, stream, 4) >> 24) & 0x3, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 20, ib_vmid, "PIPE_CONFIG", (fetch_word(asic, stream, 4) >> 26) & 0x1F, NULL, 10, 32);

						ui->add_field(ui, ib_addr + 24, ib_vmid, "X", (fetch_word(asic, stream, 5) >> 0) & 0x3FFF, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 24, ib_vmid, "Y", (fetch_word(asic, stream, 5) >> 16) & 0x3FFF, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 28, ib_vmid, "Z", (fetch_word(asic, stream, 6) >> 0) & 0x7FF, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 28, ib_vmid, "LINEAR_SW", (fetch_word(asic, stream, 6) >> 16) & 0x3, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 28, ib_vmid, "TILE_SW", (fetch_word(asic, stream, 6) >> 24) & 0x3, NULL, 10, 32);

						ui->add_field(ui, ib_addr + 32, ib_vmid, "LINEAR_ADDR_LO", fetch_word(asic, stream, 7), NULL, 16, 32);
						ui->add_field(ui, ib_addr + 36, ib_vmid, "LINEAR_ADDR_HI", fetch_word(asic, stream, 8), NULL, 16, 32);

						ui->add_field(ui, ib_addr + 40, ib_vmid, "LINEAR_PITCH", (fetch_word(asic, stream, 9) >> 0) & 0x7FFFF, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 44, ib_vmid, "COUNT", (fetch_word(asic, stream, 10) >> 2) & 0xFFFFF, NULL, 10, 32);
					}
					return;
				case 20: // LINEAR_SUB_WINDOW_BC
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "COPY (LINEAR_SUB_WINDOW_BC)", stream->header_dw, stream->words);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "ELEMENTSIZE", (stream->header_dw >> 29) & 0x7, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "SRC_ADDR_LO", fetch_word(asic, stream, 0), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_ADDR_HI", fetch_word(asic, stream, 1), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "SRC_X", (fetch_word(asic, stream, 2) >> 0) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "SRC_Y", (fetch_word(asic, stream, 2) >> 16) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 16, ib_vmid, "SRC_Z", (fetch_word(asic, stream, 3) >> 0) & 0x7FF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 16, ib_vmid, "SRC_PITCH", (fetch_word(asic, stream, 3) >> 13) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "SRC_SLICE_PITCH", (fetch_word(asic, stream, 4) >> 0) & 0xFFFFFFF, NULL, 10, 32);

					ui->add_field(ui, ib_addr + 24, ib_vmid, "DST_ADDR_LO", fetch_word(asic, stream, 5), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 28, ib_vmid, "DST_ADDR_HI", fetch_word(asic, stream, 6), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 32, ib_vmid, "DST_X", (fetch_word(asic, stream, 7) >> 0) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 32, ib_vmid, "DST_Y", (fetch_word(asic, stream, 7) >> 16) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 36, ib_vmid, "DST_Z", (fetch_word(asic, stream, 8) >> 0) & 0x7FF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 36, ib_vmid, "DST_PITCH", (fetch_word(asic, stream, 8) >> 13) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 40, ib_vmid, "DST_SLICE_PITCH", (fetch_word(asic, stream, 9) >> 0) & 0xFFFFFFF, NULL, 10, 32);

					ui->add_field(ui, ib_addr + 44, ib_vmid, "RECT_X", (fetch_word(asic, stream, 10) >> 0) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 44, ib_vmid, "RECT_Y", (fetch_word(asic, stream, 10) >> 16) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 48, ib_vmid, "RECT_Z", (fetch_word(asic, stream, 11) >> 0) & 0x7FF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 48, ib_vmid, "DST_SW", (fetch_word(asic, stream, 11) >> 16) & 0x3, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 48, ib_vmid, "DST_HA", (fetch_word(asic, stream, 11) >> 19) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 48, ib_vmid, "SRC_SW", (fetch_word(asic, stream, 11) >> 24) & 0x3, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 48, ib_vmid, "SRC_HA", (fetch_word(asic, stream, 11) >> 27) & 0x1, NULL, 10, 32);
					return;
				case 21: // TILED_SUB_WINDOW_BC
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "COPY (TILED_SUB_WINDOW_BC)", stream->header_dw, stream->words);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "DETILE", stream->header_dw >> 31, NULL, 16, 32);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "TILED_ADDR_LO", fetch_word(asic, stream, 0), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "TILED_ADDR_HI", fetch_word(asic, stream, 1), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "TILED_X", (fetch_word(asic, stream, 2) >> 0) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "TILED_Y", (fetch_word(asic, stream, 2) >> 16) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 16, ib_vmid, "TILED_Z", (fetch_word(asic, stream, 3) >> 0) & 0x7FF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 16, ib_vmid, "WIDTH", (fetch_word(asic, stream, 3) >> 16) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "HEIGHT", (fetch_word(asic, stream, 3) >> 0) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "DEPTH", (fetch_word(asic, stream, 3) >> 16) & 0x7FF, NULL, 10, 32);

					ui->add_field(ui, ib_addr + 24, ib_vmid, "ELEMENT_SIZE", (fetch_word(asic, stream, 5) >> 0) & 0x7, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "ARRAY_MODE", (fetch_word(asic, stream, 5) >> 3) & 0xF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "MIT_MODE", (fetch_word(asic, stream, 5) >> 8) & 0x7, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "TILESPLIT_SIZE", (fetch_word(asic, stream, 5) >> 11) & 0x7, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "BANK_W", (fetch_word(asic, stream, 5) >> 15) & 0x3, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "BANK_H", (fetch_word(asic, stream, 5) >> 18) & 0x3, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "NUM_BANK", (fetch_word(asic, stream, 5) >> 21) & 0x3, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "MAT_ASPT", (fetch_word(asic, stream, 5) >> 24) & 0x3, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "PIPE_CONFIG", (fetch_word(asic, stream, 5) >> 26) & 0x1F, NULL, 10, 32);

					ui->add_field(ui, ib_addr + 28, ib_vmid, "LINEAR_ADDR_LO", fetch_word(asic, stream, 6), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 32, ib_vmid, "LINEAR_ADDR_HI", fetch_word(asic, stream, 7), NULL, 16, 32);

					ui->add_field(ui, ib_addr + 36, ib_vmid, "LINEAR_X", (fetch_word(asic, stream, 8) >> 0) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 36, ib_vmid, "LINEAR_Y", (fetch_word(asic, stream, 8) >> 0) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 40, ib_vmid, "LINEAR_Z", (fetch_word(asic, stream, 9) >> 0) & 0x7FF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 40, ib_vmid, "LINEAR_PITCH", (fetch_word(asic, stream, 9) >> 16) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 44, ib_vmid, "LINEAR_SLICE_PITCH", fetch_word(asic, stream, 10) & 0xFFFFFFF, NULL, 10, 32);

					ui->add_field(ui, ib_addr + 48, ib_vmid, "RECT_X", (fetch_word(asic, stream, 11) >> 0) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 48, ib_vmid, "RECT_Y", (fetch_word(asic, stream, 11) >> 16) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 52, ib_vmid, "RECT_Z", (fetch_word(asic, stream, 12) >> 0) & 0x7FF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 52, ib_vmid, "LINEAR_SW", (fetch_word(asic, stream, 12) >> 16) & 0x3, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 52, ib_vmid, "TILE_SW", (fetch_word(asic, stream, 12) >> 24) & 0x3, NULL, 10, 32);
					return;
				case 22: // T2T_SUB_WINDOW_BC
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "COPY (T2T_SUB_WINDOW_BC)", stream->header_dw, stream->words);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "SRC_ADDR_LO", fetch_word(asic, stream, 0), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_ADDR_HI", fetch_word(asic, stream, 1), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "SRC_X", (fetch_word(asic, stream, 2) >> 0) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "SRC_Y", (fetch_word(asic, stream, 2) >> 16) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 16, ib_vmid, "SRC_Z", (fetch_word(asic, stream, 3) >> 0) & 0x7FF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 16, ib_vmid, "SRC_WIDTH", (fetch_word(asic, stream, 3) >> 16) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "SRC_HEIGHT", (fetch_word(asic, stream, 4) >> 0) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "SRC_DEPTH", (fetch_word(asic, stream, 4) >> 16) & 0x7FF, NULL, 10, 32);

					ui->add_field(ui, ib_addr + 24, ib_vmid, "SRC_ELEMENT_SIZE", (fetch_word(asic, stream, 5) >> 0) & 0x7, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "SRC_ARRAY_MODE", (fetch_word(asic, stream, 5) >> 3) & 0xF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "SRC_MIT_MODE", (fetch_word(asic, stream, 5) >> 8) & 0x7, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "SRC_TILESPLIT_SIZE", (fetch_word(asic, stream, 5) >> 11) & 0x7, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "SRC_BANK_W", (fetch_word(asic, stream, 5) >> 15) & 0x3, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "SRC_BANK_H", (fetch_word(asic, stream, 5) >> 18) & 0x3, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "SRC_NUM_BANKS", (fetch_word(asic, stream, 5) >> 21) & 0x3, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "SRC_MAT_ASPT", (fetch_word(asic, stream, 5) >> 24) & 0x3, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "SRC_PIPE_CONFIG", (fetch_word(asic, stream, 5) >> 26) & 0x1F, NULL, 10, 32);

					ui->add_field(ui, ib_addr + 28, ib_vmid, "DST_ADDR_LO", fetch_word(asic, stream, 6), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 32, ib_vmid, "DST_ADDR_HI", fetch_word(asic, stream, 7), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 36, ib_vmid, "DST_X", (fetch_word(asic, stream, 8) >> 0) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 36, ib_vmid, "DST_Y", (fetch_word(asic, stream, 8) >> 16) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 40, ib_vmid, "DST_Z", (fetch_word(asic, stream, 9) >> 0) & 0x7FF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 40, ib_vmid, "SRC_WIDTH", (fetch_word(asic, stream, 9) >> 16) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 44, ib_vmid, "SRC_HEIGHT", (fetch_word(asic, stream, 10) >> 0) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 44, ib_vmid, "SRC_DEPTH", (fetch_word(asic, stream, 10) >> 16) & 0xFFF, NULL, 10, 32);

					ui->add_field(ui, ib_addr + 48, ib_vmid, "DST_ELEMENT_SIZE", (fetch_word(asic, stream, 11) >> 0) & 0x7, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 48, ib_vmid, "DST_ARRAY_MODE", (fetch_word(asic, stream, 11) >> 3) & 0xF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 48, ib_vmid, "DST_MIT_MODE", (fetch_word(asic, stream, 11) >> 8) & 0x7, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 48, ib_vmid, "DST_TILESPLIT_SIZE", (fetch_word(asic, stream, 11) >> 11) & 0x7, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 48, ib_vmid, "DST_BANK_W", (fetch_word(asic, stream, 11) >> 15) & 0x3, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 48, ib_vmid, "DST_BANK_H", (fetch_word(asic, stream, 11) >> 18) & 0x3, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 48, ib_vmid, "DST_NUM_BANK", (fetch_word(asic, stream, 11) >> 21) & 0x3, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 48, ib_vmid, "DST_MAT_ASPT", (fetch_word(asic, stream, 11) >> 24) & 0x3, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 48, ib_vmid, "DST_PIPE_CONFIG", (fetch_word(asic, stream, 11) >> 26) & 0x1F, NULL, 10, 32);

					ui->add_field(ui, ib_addr + 52, ib_vmid, "RECT_X", (fetch_word(asic, stream, 12) >> 0) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 52, ib_vmid, "RECT_Y", (fetch_word(asic, stream, 12) >> 16) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 56, ib_vmid, "RECT_Z", (fetch_word(asic, stream, 13) >> 0) & 0x7FF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 56, ib_vmid, "DST_SW", (fetch_word(asic, stream, 13) >> 16) & 0x3, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 56, ib_vmid, "SRC_SW", (fetch_word(asic, stream, 13) >> 22) & 0x3, NULL, 10, 32);
					return;
				case 36: // SUBWIN_LARGE
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "COPY (SUBWIN_LARGE)", stream->header_dw, stream->words);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "TMZ", (stream->header_dw >> 18) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "CPV", (stream->header_dw >> 19) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "SRC_ADDR_LO", fetch_word(asic, stream, 0), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_ADDR_HI", fetch_word(asic, stream, 1), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "SRC_X", fetch_word(asic, stream, 2), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 16, ib_vmid, "SRC_Y", fetch_word(asic, stream, 3), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "SRC_Z", fetch_word(asic, stream, 4), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "SRC_PITCH", fetch_word(asic, stream, 5), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 28, ib_vmid, "SRC_SLICE_PITCH_LO", fetch_word(asic, stream, 6), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 32, ib_vmid, "SRC_SLICE_PITCH_47_32", fetch_word(asic, stream, 7) & 0xFFFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 36, ib_vmid, "DST_ADDR_LO", fetch_word(asic, stream, 8), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 40, ib_vmid, "DST_ADDR_HI", fetch_word(asic, stream, 9), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 44, ib_vmid, "DST_X", fetch_word(asic, stream, 10), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 48, ib_vmid, "DST_Y", fetch_word(asic, stream, 11), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 52, ib_vmid, "DST_Z", fetch_word(asic, stream, 12), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 56, ib_vmid, "DST_PITCH", fetch_word(asic, stream, 13), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 60, ib_vmid, "DST_SLICE_PITCH_LO", fetch_word(asic, stream, 14), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 64, ib_vmid, "DST_SLICE_PITCH_47_32", fetch_word(asic, stream, 15) & 0xFFFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 64, ib_vmid, "DST_SW", (fetch_word(asic, stream, 15) >> 16) & 0x3, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 64, ib_vmid, "DST_POLICY", (fetch_word(asic, stream, 15) >> 18) & 0x7, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 64, ib_vmid, "SRC_SW", (fetch_word(asic, stream, 15) >> 24) & 0x3, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 64, ib_vmid, "SRC_POLICY", (fetch_word(asic, stream, 15) >> 26) & 0x7, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 68, ib_vmid, "RECT_X", fetch_word(asic, stream, 16), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 72, ib_vmid, "RECT_Y", fetch_word(asic, stream, 17), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 76, ib_vmid, "RECT_Z", fetch_word(asic, stream, 18), NULL, 10, 32);
					return;
				default:
					if (ui->unhandled_subop)
						ui->unhandled_subop(ui, asic, ib_addr, ib_vmid, stream, UMR_RING_SDMA);
					break;
			}
			break;
		case 2: // WRITE
			switch (stream->sub_opcode) {
				case 0: // LINEAR
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "WRITE (LINEAR)", stream->header_dw, stream->words);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "ENCRYPT", (stream->header_dw >> 16) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "TMZ", (stream->header_dw >> 18) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "DST_ADDR_LO", fetch_word(asic, stream, 0), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_ADDR_HI", fetch_word(asic, stream, 1), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "COUNT", fetch_word(asic, stream, 2), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "SWAP", (fetch_word(asic, stream, 2) >> 24) & 0x3, NULL, 10, 32);
					for (n = 3; n < stream->nwords; n++) {
						uint32_t data_idx = n - 3;
						sprintf(str_buf, "DATA_%"PRIu32, data_idx);
						ui->add_field(ui, ib_addr + 16 + 4 * (n - 3), ib_vmid, str_buf, fetch_word(asic, stream, n), NULL, 16, 32);
					}
					return;
				case 1: // TILED
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "WRITE (TILED)", stream->header_dw, stream->words);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "DST_ADDR_LO", fetch_word(asic, stream, 0), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_ADDR_HI", fetch_word(asic, stream, 1), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "PITCH_IN_TILE", (fetch_word(asic, stream, 2) >> 0) & 0x7FF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "HEIGHT", (fetch_word(asic, stream, 2) >> 16) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 16, ib_vmid, "SLICE_PITCH", (fetch_word(asic, stream, 3) >> 0) & 0x3FFFFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "ELEMENT_SIZE", (fetch_word(asic, stream, 4) >> 0) & 0x7, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "ARRAY_MODE", (fetch_word(asic, stream, 4) >> 3) & 0xF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "MIT_MODE", (fetch_word(asic, stream, 4) >> 8) & 0x7, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "TILESPLIT_SIZE", (fetch_word(asic, stream, 4) >> 11) & 0x7, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "BANK_W", (fetch_word(asic, stream, 4) >> 15) & 0x3, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "BANK_H", (fetch_word(asic, stream, 4) >> 18) & 0x3, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "NUM_BANK", (fetch_word(asic, stream, 4) >> 21) & 0x3, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "MAT_ASPT", (fetch_word(asic, stream, 4) >> 24) & 0x3, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "PIPE_CONFIG", (fetch_word(asic, stream, 4) >> 26) & 0x1F, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "X", (fetch_word(asic, stream, 5) >> 0) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "Y", (fetch_word(asic, stream, 5) >> 16) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 28, ib_vmid, "Z", (fetch_word(asic, stream, 6) >> 0) & sc->z_mask, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 28, ib_vmid, "SW", (fetch_word(asic, stream, 6) >> 24) & 0x3, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 32, ib_vmid, "COUNT", (fetch_word(asic, stream, 7) >> 0) & 0xFFFFF, NULL, 10, 32);
					for (n = 8; n < stream->nwords; n++) {
						uint32_t data_idx = n - 8;
						sprintf(str_buf, "DATA_%"PRIu32, data_idx);
						ui->add_field(ui, ib_addr + 36 + 4 * (n - 8), ib_vmid, str_buf, fetch_word(asic, stream, n), NULL, 16, 32);
					}
					return;
				case 17: // TILED_BC
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "WRITE (TILED_BC)", stream->header_dw, stream->words);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "DST_ADDR_LO", fetch_word(asic, stream, 0), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_ADDR_HI", fetch_word(asic, stream, 1), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "WIDTH", (fetch_word(asic, stream, 2) >> 0) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 16, ib_vmid, "HEIGHT", (fetch_word(asic, stream, 3) >> 0) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 16, ib_vmid, "DEPTH", (fetch_word(asic, stream, 3) >> 16) & 0x7FF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "ELEMENT_SIZE", (fetch_word(asic, stream, 4) >> 0) & 0x7, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "ARRAY_MODE", (fetch_word(asic, stream, 4) >> 3) & 0xF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "MIT_MODE", (fetch_word(asic, stream, 4) >> 8) & 0x7, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "TILESPLIT_SIZE", (fetch_word(asic, stream, 4) >> 11) & 0x7, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "BANK_W", (fetch_word(asic, stream, 4) >> 15) & 0x3, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "BANK_H", (fetch_word(asic, stream, 4) >> 18) & 0x3, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "NUM_BANK", (fetch_word(asic, stream, 4) >> 21) & 0x3, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "MAT_ASPT", (fetch_word(asic, stream, 4) >> 24) & 0x3, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "PIPE_CONFIG", (fetch_word(asic, stream, 4) >> 26) & 0x1F, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "X", (fetch_word(asic, stream, 5) >> 0) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "Y", (fetch_word(asic, stream, 5) >> 16) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 28, ib_vmid, "Z", (fetch_word(asic, stream, 6) >> 0) & sc->z_mask, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 28, ib_vmid, "SW", (fetch_word(asic, stream, 6) >> 24) & 0x3, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 32, ib_vmid, "COUNT", (fetch_word(asic, stream, 7) >> 0) & 0xFFFFF, NULL, 10, 32);
					for (n = 8; n < stream->nwords; n++) {
						uint32_t data_idx = n - 8;
						sprintf(str_buf, "DATA_%"PRIu32, data_idx);
						ui->add_field(ui, ib_addr + 36 + 4 * (n - 8), ib_vmid, str_buf, fetch_word(asic, stream, n), NULL, 16, 32);
					}
					return;
				default:
					if (ui->unhandled_subop)
						ui->unhandled_subop(ui, asic, ib_addr, ib_vmid, stream, UMR_RING_SDMA);
					return;
			}
			break;
		case 4: // INDIRECT
			ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "INDIRECT_BUFFER", stream->header_dw, stream->words);
			ui->add_field(ui, ib_addr + 0, ib_vmid, "VMID", (stream->header_dw >> 16) & 0xF, NULL, 16, 32);
			ui->add_field(ui, ib_addr + 0, ib_vmid, "PRIV", (stream->header_dw >> 31) & 0x1, NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "IB_BASE_LO", fetch_word(asic, stream, 0), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "IB_BASE_HI", fetch_word(asic, stream, 1), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "IB_BASE_SIZE", fetch_word(asic, stream, 2), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 16, ib_vmid, "IB_CSA_ADDR_LO", fetch_word(asic, stream, 3), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 20, ib_vmid, "IB_CSA_ADDR_HI", fetch_word(asic, stream, 4), NULL, 16, 32);
			if (follow && stream->next_ib)
				umr_sdma_decode_stream_opcodes(asic, ui, stream->next_ib, stream->ib.addr, stream->ib.vmid, stream->next_ib->from.addr, stream->next_ib->from.vmid, ~0UL, 1);
			return;
		case 5: // FENCE
			ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "FENCE", stream->header_dw, stream->words);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "FENCE_ADDR_LO", fetch_word(asic, stream, 0), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "FENCE_ADDR_HI", fetch_word(asic, stream, 1), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "FENCE_DATA", fetch_word(asic, stream, 2), NULL, 16, 32);
			return;
		case 6: // TRAP
			ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "TRAP", stream->header_dw, stream->words);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "TRAP_INT_CONTEXT", fetch_word(asic, stream, 0) & 0xFFFFFF, NULL, 16, 32);
			break;
		case 7: // SEM
			switch (stream->sub_opcode) {
				case 0: // SEM
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "SEM", stream->header_dw, stream->words);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "WRITE_ONE", (stream->header_dw >> 29) & 1, NULL, 16, 32);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "SIGNAL", (stream->header_dw >> 30) & 1, NULL, 16, 32);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "MAILBOX", (stream->header_dw >> 31) & 1, NULL, 16, 32);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "SEMAPHORE_ADDR_LO", fetch_word(asic, stream, 0), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "SEMAPHORE_ADDR_HI", fetch_word(asic, stream, 1), NULL, 16, 32);
					return;
				case 1: // MEM_INCR
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "SEM (MEM_INCR)", stream->header_dw, stream->words);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "ADDR_LO", fetch_word(asic, stream, 0), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "ADDR_HI", fetch_word(asic, stream, 1), NULL, 16, 32);
					return;
				default:
					if (ui->unhandled_subop)
						ui->unhandled_subop(ui, asic, ib_addr, ib_vmid, stream, UMR_RING_SDMA);
					return;
			}

			break;
		case 8: // POLL_REGMEM
			switch (stream->sub_opcode) {
				case 0: // POLL_REGMEM
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "POLL_REGMEM", stream->header_dw, stream->words);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "HDP_FLUSH", (stream->header_dw >> 26) & 1, NULL, 16, 32);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "FUNCTION", 0, poll_regmem_funcs[(stream->header_dw >> 28) & 7], 0, 32);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "MEM_POLL", (stream->header_dw >> 31) & 1, NULL, 16, 32);
					if (!(stream->header_dw & (1UL << 31))) {
						ui->add_field(ui, ib_addr + 4, ib_vmid, "REGISTER", BITS(fetch_word(asic, stream, 0), 2, 20), umr_reg_name(asic, BITS(fetch_word(asic, stream, 0), 2, 20)), 16, 32);
						if (((stream->header_dw >> 26) & 3) == 1) { // if HDP_FLUSH, the write register is provided
							ui->add_field(ui, ib_addr + 8, ib_vmid, "REGISTER", BITS(fetch_word(asic, stream, 1), 2, 18), umr_reg_name(asic, BITS(fetch_word(asic, stream, 1), 2, 18)), 16, 32);
						} else {
							ui->add_field(ui, ib_addr + 8, ib_vmid, NULL, fetch_word(asic, stream, 1), NULL, 16, 32);
						}
					} else {
						ui->add_field(ui, ib_addr + 4, ib_vmid, "POLL_REGMEM_ADDR_LO", fetch_word(asic, stream, 0), NULL, 16, 32);
						ui->add_field(ui, ib_addr + 8, ib_vmid, "POLL_REGMEM_ADDR_HI", fetch_word(asic, stream, 1), NULL, 16, 32);
					}
					ui->add_field(ui, ib_addr + 12, ib_vmid, "VALUE", fetch_word(asic, stream, 2), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 16, ib_vmid, "MASK", fetch_word(asic, stream, 3), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "INTERVAL", fetch_word(asic, stream, 4) & 0xFFFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "RETRY_COUNT", (fetch_word(asic, stream, 4) >> 16) & 0xFFF, NULL, 10, 32);
					return;
				case 1: // POLL_REG_WRITE_MEM
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "POLL_REG_WRITE_MEM", stream->header_dw, stream->words);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "SRC_ADDR", fetch_word(asic, stream, 0), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_ADDR_LO", fetch_word(asic, stream, 1), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "DST_ADDR_HI", fetch_word(asic, stream, 2), NULL, 16, 32);
					return;
				case 2: // POLL_DBIT_WRITE_MEM
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "POLL_DBIT_WRITE_MEM", stream->header_dw, stream->words);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "EA", (stream->header_dw >> 16) & 0x3, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "DST_ADDR_LO", fetch_word(asic, stream, 0), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_ADDR_HI", fetch_word(asic, stream, 1), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "START_PAGE", (fetch_word(asic, stream, 2) >> 4) & 0xFFFFFFF, NULL, 16, 32);
					ui->add_field(ui, ib_addr + 16, ib_vmid, "PAGE_NUM", fetch_word(asic, stream, 3), NULL, 16, 32);
					break;
				case 3: // MEM_VERIFY
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "MEM_VERIFY", stream->header_dw, stream->words);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "MODE", (stream->header_dw >> 31) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "PATTERN", fetch_word(asic, stream, 0), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "CMP0_ADDR_START_LO", fetch_word(asic, stream, 1), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "CMP0_ADDR_START_HI", fetch_word(asic, stream, 2), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 16, ib_vmid, "CMP0_ADDR_END_LO", fetch_word(asic, stream, 3), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "CMP0_ADDR_END_HI", fetch_word(asic, stream, 4), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "CMP1_ADDR_START_LO", fetch_word(asic, stream, 5), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 28, ib_vmid, "CMP1_ADDR_START_HI", fetch_word(asic, stream, 6), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 32, ib_vmid, "CMP1_ADDR_END_LO", fetch_word(asic, stream, 7), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 36, ib_vmid, "CMP1_ADDR_END_HI", fetch_word(asic, stream, 8), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 40, ib_vmid, "REC_ADDR_LO", fetch_word(asic, stream, 9), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 44, ib_vmid, "REC_ADDR_HI", fetch_word(asic, stream, 10), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 48, ib_vmid, "RESERVED", fetch_word(asic, stream, 11), NULL, 10, 32);
					return;
				default:
					if (ui->unhandled_subop)
						ui->unhandled_subop(ui, asic, ib_addr, ib_vmid, stream, UMR_RING_SDMA);
					return;
			}
			break;
		case 9:  // COND_EXE
			ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "COND_EXE", stream->header_dw, stream->words);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "ADDR_LO", fetch_word(asic, stream, 0), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "ADDR_HI", fetch_word(asic, stream, 1), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "REFERENCE", fetch_word(asic, stream, 2), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 16, ib_vmid, "EXEC_COUNT", fetch_word(asic, stream, 3), NULL, 10, 32);
			return;
		case 10:  // ATOMIC
			ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "ATOMIC", stream->header_dw, stream->words);
			ui->add_field(ui, ib_addr + 0, ib_vmid, "LOOP", (stream->header_dw >> 16) & 1, NULL, 16, 32);
			ui->add_field(ui, ib_addr + 0, ib_vmid, "OP", (stream->header_dw >> 25) & 0x7F, NULL, 16, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "ADDR_LO", fetch_word(asic, stream, 0), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "ADDR_HI", fetch_word(asic, stream, 1), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "SRC_DATA_LO", fetch_word(asic, stream, 2), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 16, ib_vmid, "SRC_DATA_HI", fetch_word(asic, stream, 3), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 20, ib_vmid, "CMP_DATA_LO", fetch_word(asic, stream, 4), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 24, ib_vmid, "CMP_DATA_HI", fetch_word(asic, stream, 5), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 28, ib_vmid, "LOOP_INTERVAL", fetch_word(asic, stream, 6) & 0x1FFF, NULL, 16, 32);
			return;
		case 11: // CONST_FILL
			switch (stream->sub_opcode) {
				case 0: // CONST_FILL
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "CONST_FILL", stream->header_dw, stream->words);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "SWAP", (stream->header_dw >> 16) & 0x3, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "FILL_SIZE", (stream->header_dw >> 30) & 0x3, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "CONST_FILL_DST_LO", fetch_word(asic, stream, 0), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "CONST_FILL_DST_HI", fetch_word(asic, stream, 1), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "CONST_FILL_DATA", fetch_word(asic, stream, 2), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 16, ib_vmid, "CONST_FILL_BYTE_COUNT", fetch_word(asic, stream, 3), NULL, 10, 32);
					return;
				case 1: // CONST_FILL (DATA_FILL_MULTI)
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "CONST_FILL (DATA_FILL_MULTI)", stream->header_dw, stream->words);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "MEMLOG_CLR", (stream->header_dw >> 31) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "BYTE_STRIDE", fetch_word(asic, stream, 0), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "DMA_COUNT", fetch_word(asic, stream, 1), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "DST_ADDR_LO", fetch_word(asic, stream, 2), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 16, ib_vmid, "DST_ADDR_HI", fetch_word(asic, stream, 3), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "COUNT", (fetch_word(asic, stream, 4) >> 0) & 0x3FFFFFF, NULL, 10, 32);
					return;
				default:
					if (ui->unhandled_subop)
						ui->unhandled_subop(ui, asic, ib_addr, ib_vmid, stream, UMR_RING_SDMA);
					return;
			}
			break;
		case 12: // GEN_PTEPDE
			switch (stream->sub_opcode) {
				case 0: // GEN_PTEPDE
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "GEN_PTEPDE", stream->header_dw, stream->words);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "DST_ADDR_LO", fetch_word(asic, stream, 0), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_ADDR_HI", fetch_word(asic, stream, 1), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "MASK_LO", fetch_word(asic, stream, 2), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 16, ib_vmid, "MASK_HI", fetch_word(asic, stream, 3), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "INIT_LO", fetch_word(asic, stream, 4), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "INIT_HI", fetch_word(asic, stream, 5), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 28, ib_vmid, "INCR_LO", fetch_word(asic, stream, 6), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 32, ib_vmid, "INCR_HI", fetch_word(asic, stream, 7), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 36, ib_vmid, "COUNT", fetch_word(asic, stream, 8) & 0x7FFFF, NULL, 10, 32);
					return;
				case 1: // COPY
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "GEN_PTEPDE (COPY)", stream->header_dw, stream->words);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "PTEPDE_OP", (stream->header_dw >> 31) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "SRC_ADDR_LO", fetch_word(asic, stream, 0), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_ADDR_HI", fetch_word(asic, stream, 1), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "DST_ADDR_LO", fetch_word(asic, stream, 2), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 16, ib_vmid, "DST_ADDR_HI", fetch_word(asic, stream, 3), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "MASK_LO", fetch_word(asic, stream, 4), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "MASK_HI", fetch_word(asic, stream, 5), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 28, ib_vmid, "COUNT", fetch_word(asic, stream, 6) & 0x7FFFF, NULL, 10, 32);
					return;
				case 2: // RMW
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "GEN_PTEPDE (RMW)", stream->header_dw, stream->words);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "MTYPE", (stream->header_dw >> 16) & 0x7, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "GCC", (stream->header_dw >> 19) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "SYS", (stream->header_dw >> 20) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "SNP", (stream->header_dw >> 22) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "GPA", (stream->header_dw >> 23) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "ADDR_LO", fetch_word(asic, stream, 0), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "ADDR_HI", fetch_word(asic, stream, 1), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "MASK_LO", fetch_word(asic, stream, 2), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 16, ib_vmid, "MASK_HI", fetch_word(asic, stream, 3), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "VALUE_LO", fetch_word(asic, stream, 4), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "VALUE_HI", fetch_word(asic, stream, 5), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 28, ib_vmid, "NUM_OF_PTE", fetch_word(asic, stream, 6), NULL, 10, 32);
					return;
				default:
					if (ui->unhandled_subop)
						ui->unhandled_subop(ui, asic, ib_addr, ib_vmid, stream, UMR_RING_SDMA);
					return;
			}
			break;
		case 13: // TIMESTAMP
			switch (stream->sub_opcode) {
				case 0: // TIMESTAMP_SET
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "TIMESTAMP (SET)", stream->header_dw, stream->words);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "INIT_DATA_LO", fetch_word(asic, stream, 0), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "INIT_DATA_HI", fetch_word(asic, stream, 1), NULL, 16, 32);
					return;
				case 1: // TIMESTAMP_GET
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "TIMESTAMP (GET)", stream->header_dw, stream->words);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "WRITE_ADDR_LO", fetch_word(asic, stream, 0), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "WRITE_ADDR_HI", fetch_word(asic, stream, 1), NULL, 16, 32);
					return;
				case 2: // TIMESTAMP_GET_GLOBAL
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "TIMESTAMP (GET_GLOBAL)", stream->header_dw, stream->words);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "WRITE_ADDR_LO", fetch_word(asic, stream, 0), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "WRITE_ADDR_HI", fetch_word(asic, stream, 1), NULL, 16, 32);
					return;
				default:
					if (ui->unhandled_subop)
						ui->unhandled_subop(ui, asic, ib_addr, ib_vmid, stream, UMR_RING_SDMA);
					return;
			}
			break;
		case 14: // WRITE REG
			switch (stream->sub_opcode) {
				case 0: // SRBM_WRITE
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "SRBM_WRITE", stream->header_dw, stream->words);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "BYTE_ENABLE", (stream->header_dw >> 28), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "SRBM_WRITE_ADDR", fetch_word(asic, stream, 0) & 0xFFFF, umr_reg_name(asic, fetch_word(asic, stream, 0) & 0xFFFF), 16, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "SRBM_WRITE_DATA", fetch_word(asic, stream, 1), NULL, 16, 32);
					return;
				default:
					if (ui->unhandled_subop)
						ui->unhandled_subop(ui, asic, ib_addr, ib_vmid, stream, UMR_RING_SDMA);
					return;
			}
			break;
		case 15: // PRE_EXE
			ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "PRE_EXE", stream->header_dw, stream->words);
			ui->add_field(ui, ib_addr + 0, ib_vmid, "DEV_SEL", (stream->header_dw >> 16) & 0xFF, NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "EXEC_COUNT", fetch_word(asic, stream, 0) & 0x3FFF, NULL, 10, 32);
			return;
		case 16: // GPUVM_INV (NV and beyond)
			ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "GPUVM_INV", stream->header_dw, stream->words);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "PER_VMID_INV_REQ", fetch_word(asic, stream, 0), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "FLUSH_TYPE", BITS(fetch_word(asic, stream, 0), 16, 19), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "INV_L2_PTES", BITS(fetch_word(asic, stream, 0), 19, 20), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "INV_L2_PDE0", BITS(fetch_word(asic, stream, 0), 20, 21), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "INV_L2_PDE1", BITS(fetch_word(asic, stream, 0), 21, 22), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "INV_L2_PDE2", BITS(fetch_word(asic, stream, 0), 22, 23), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "INV_L1_PTES", BITS(fetch_word(asic, stream, 0), 23, 24), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "CLEAR_PROTECTION_FAULT_STATUS_ADDR", BITS(fetch_word(asic, stream, 0), 24, 25), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "LOG_REQUEST", BITS(fetch_word(asic, stream, 0), 25, 26), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "4KB", BITS(fetch_word(asic, stream, 0), 26, 27), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "S_BIT", BITS(fetch_word(asic, stream, 1), 0, 1), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "PAGE_VM_ADDR_LO", BITS(fetch_word(asic, stream, 1), 1, 32), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "PAGE_VM_ADDR_HI", BITS(fetch_word(asic, stream, 2), 0, 6), NULL, 16, 32);
			return;
		case 17: // GCR
			ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "GCR", stream->header_dw, stream->words);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "BASE_VA_LO", BITS(fetch_word(asic, stream, 0), 7, 32) << 7, NULL, 16, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "BASE_VA_HI", BITS(fetch_word(asic, stream, 1), 0, 16), NULL, 16, 32);
			n = fetch_word(asic, stream, 1) >> 16;
			ui->add_field(ui, ib_addr + 8, ib_vmid, "GL2_WB", BITS(n, 15, 16), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "GL2_INV", BITS(n, 14, 15), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "GL2_DISCARD", BITS(n, 13, 14), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "GL2_RANGE", BITS(n, 11, 13), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "GL2_US", BITS(n, 10, 11), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "GL1_INV", BITS(n, 9, 10), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "GLV_INV", BITS(n, 8, 9), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "GLK_INV", BITS(n, 7, 8), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "GLK_WB", BITS(n, 6, 7), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "GLM_INV", BITS(n, 5, 6), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "GLM_WB", BITS(n, 4, 5), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "GL1_RANGE", BITS(n, 2, 4), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "GLI_INV", BITS(n, 0, 2), NULL, 10, 32);

			ui->add_field(ui, ib_addr + 12, ib_vmid, "RANGE_IS_PA", BITS(fetch_word(asic, stream, 2), 2, 3), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "SEQ", BITS(fetch_word(asic, stream, 2), 0, 2), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "LIMIT_VA_LO", BITS(fetch_word(asic, stream, 2), 7, 32) << 7, NULL, 16, 32);

			ui->add_field(ui, ib_addr + 16, ib_vmid, "LIMIT_VA_HI", BITS(fetch_word(asic, stream, 3), 0, 16), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 16, ib_vmid, "VMID", BITS(fetch_word(asic, stream, 3), 24, 28), NULL, 10, 32);
			return;
		case 32: // DUMMY_TRAP (NV and beyond)
			ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "DUMMY_TRAP", stream->header_dw, stream->words);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "INT_CONTEXT", (fetch_word(asic, stream, 0) >> 0) & 0xFFFFFFF, NULL, 10, 32);
			return;
		default:
			if (ui->unhandled)
				ui->unhandled(ui, asic, ib_addr, ib_vmid, stream, UMR_RING_SDMA);
			return;
		}
}

static void decode_upto_ai(struct umr_asic *asic, struct umr_stream_decode_ui *ui, struct umr_sdma_stream *stream,
			   uint64_t ib_addr, uint32_t ib_vmid, uint64_t from_addr, uint64_t from_vmid, int follow, struct sdma_config *sc)
{
	uint32_t n;
	char str_buf[128];
	(void)from_addr;
	(void)from_vmid;
	switch (stream->opcode) {
		case 1: // COPY
			switch (stream->sub_opcode) {
				case 0: // LINEAR
					switch (stream->header_dw & (1UL << 27)) {
						case 0: // not broadcast
							ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "COPY (LINEAR)", stream->header_dw, stream->words);
							ui->add_field(ui, ib_addr + 0, ib_vmid, "ENCRYPT", (stream->header_dw >> 16) & 0x1, NULL, 10, 32);
							ui->add_field(ui, ib_addr + 0, ib_vmid, "TMZ", (stream->header_dw >> 18) & 0x1, NULL, 10, 32);
							if (sc->has_cpv_flag) {
								ui->add_field(ui, ib_addr + 0, ib_vmid, "CPV", (stream->header_dw >> 19) & 0x1, NULL, 10, 32);
							}
							ui->add_field(ui, ib_addr + 0, ib_vmid, "BACKWARDS", (stream->header_dw >> 25) & 0x1, NULL, 10, 32);
							ui->add_field(ui, ib_addr + 0, ib_vmid, "BROADCAST", (stream->header_dw >> 27) & 0x1, NULL, 10, 32);
							ui->add_field(ui, ib_addr + 4, ib_vmid, "COPY_COUNT", fetch_word(asic, stream, 0), NULL, 10, 32);
							ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_SW", (fetch_word(asic, stream, 1) >> 16) & 3, NULL, 10, 32);
							if (sc->has_cp_fields) {
								ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_CACHE_POLICY", (fetch_word(asic, stream, 1) >> 18) & 0x7, NULL, 10, 32);
							}
							ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_SW", (fetch_word(asic, stream, 1) >> 24) & 3, NULL, 10, 32);
							if (sc->has_cp_fields) {
								ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_CACHE_POLICY", (fetch_word(asic, stream, 1) >> 26) & 0x7, NULL, 10, 32);
							}
							ui->add_field(ui, ib_addr + 12, ib_vmid, "SRC_ADDR_LO", fetch_word(asic, stream, 2), NULL, 16, 32);
							ui->add_field(ui, ib_addr + 16, ib_vmid, "SRC_ADDR_HI", fetch_word(asic, stream, 3), NULL, 16, 32);
							ui->add_field(ui, ib_addr + 20, ib_vmid, "DST_ADDR_LO", fetch_word(asic, stream, 4), NULL, 16, 32);
							ui->add_field(ui, ib_addr + 24, ib_vmid, "DST_ADDR_HI", fetch_word(asic, stream, 5), NULL, 16, 32);
							return;
						case 1: // broadcast
							ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "COPY (LINEAR BROADCAST)", stream->header_dw, stream->words);
							ui->add_field(ui, ib_addr + 0, ib_vmid, "ENCRYPT", (stream->header_dw >> 16) & 0x1, NULL, 10, 32);
							ui->add_field(ui, ib_addr + 0, ib_vmid, "TMZ", (stream->header_dw >> 18) & 0x1, NULL, 10, 32);
							if (sc->has_cpv_flag) {
								ui->add_field(ui, ib_addr + 0, ib_vmid, "CPV", (stream->header_dw >> 19) & 0x1, NULL, 10, 32);
							}
							ui->add_field(ui, ib_addr + 0, ib_vmid, "BROADCAST", (stream->header_dw >> 27) & 0x1, NULL, 10, 32);
							ui->add_field(ui, ib_addr + 4, ib_vmid, "COPY_COUNT", fetch_word(asic, stream, 0), NULL, 10, 32);
							ui->add_field(ui, ib_addr + 8, ib_vmid, "DST2_SW", (fetch_word(asic, stream, 1) >> 8) & 3, NULL, 10, 32);
							if (sc->has_cp_fields) {
								ui->add_field(ui, ib_addr + 8, ib_vmid, "DST2_CACHE_POLICY", (fetch_word(asic, stream, 1) >> 10) & 0x7, NULL, 10, 32);
							}
							ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_SW", (fetch_word(asic, stream, 1) >> 16) & 3, NULL, 10, 32);
							if (sc->has_cp_fields) {
								ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_CACHE_POLICY", (fetch_word(asic, stream, 1) >> 18) & 0x7, NULL, 10, 32);
							}
							ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_SW", (fetch_word(asic, stream, 1) >> 24) & 3, NULL, 10, 32);
							if (sc->has_cp_fields) {
								ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_CACHE_POLICY", (fetch_word(asic, stream, 1) >> 26) & 0x7, NULL, 10, 32);
							}
							ui->add_field(ui, ib_addr + 12, ib_vmid, "SRC_ADDR_LO", fetch_word(asic, stream, 2), NULL, 16, 32);
							ui->add_field(ui, ib_addr + 16, ib_vmid, "SRC_ADDR_HI", fetch_word(asic, stream, 3), NULL, 16, 32);
							ui->add_field(ui, ib_addr + 20, ib_vmid, "DST_ADDR_LO", fetch_word(asic, stream, 4), NULL, 16, 32);
							ui->add_field(ui, ib_addr + 24, ib_vmid, "DST_ADDR_HI", fetch_word(asic, stream, 5), NULL, 16, 32);
							ui->add_field(ui, ib_addr + 28, ib_vmid, "DST2_ADDR_LO", fetch_word(asic, stream, 6), NULL, 16, 32);
							ui->add_field(ui, ib_addr + 32, ib_vmid, "DST2_ADDR_HI", fetch_word(asic, stream, 7), NULL, 16, 32);
							return;
						default:
							break;
					}
					break;
				case 1: // TILED
					if ((stream->header_dw >> 26) & 0x3) { // L2T BROADCAST or L2T FRAME TO FIELD
						if ((stream->header_dw >> 26) & 0x1) {
							ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "COPY (L2T_FRAME_TO_FIELD)", stream->header_dw, stream->words);
						} else {
							ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "COPY (L2T_BROADCAST)", stream->header_dw, stream->words);
						}
						ui->add_field(ui, ib_addr + 0, ib_vmid, "ENCRYPT", (stream->header_dw >> 16) & 0x1, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 0, ib_vmid, "TMZ", (stream->header_dw >> 18) & 0x1, NULL, 10, 32);
						if (sc->has_cpv_flag) {
							ui->add_field(ui, ib_addr + 0, ib_vmid, "CPV", (stream->header_dw >> 19) & 0x1, NULL, 10, 32);
						}
						ui->add_field(ui, ib_addr + 0, ib_vmid, "MIP_MAX", (stream->header_dw >> 20) & 0xF, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 0, ib_vmid, "VIDEOCOPY", (stream->header_dw >> 26) & 0x1, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 0, ib_vmid, "BROADCAST", (stream->header_dw >> 27) & 0x1, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 4, ib_vmid, "TILED_ADDR0_LO", fetch_word(asic, stream, 0), NULL, 16, 32);
						ui->add_field(ui, ib_addr + 8, ib_vmid, "TILED_ADDR0_HI", fetch_word(asic, stream, 1), NULL, 16, 32);
						ui->add_field(ui, ib_addr + 12, ib_vmid, "TILED_ADDR1_LO", fetch_word(asic, stream, 2), NULL, 16, 32);
						ui->add_field(ui, ib_addr + 16, ib_vmid, "TILED_ADDR1_HI", fetch_word(asic, stream, 3), NULL, 16, 32);
						ui->add_field(ui, ib_addr + 20, ib_vmid, "WIDTH", fetch_word(asic, stream, 4) & 0x3FFF, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 24, ib_vmid, "HEIGHT", fetch_word(asic, stream, 5) & 0x3FFF, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 24, ib_vmid, "DEPTH", (fetch_word(asic, stream, 5) >> 16) & 0x1FFF, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 28, ib_vmid, "ELEMENT_SIZE", fetch_word(asic, stream, 6) & 0x7, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 28, ib_vmid, "SWIZZLE_MODE", (fetch_word(asic, stream, 6) >> 3) & 0x1F, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 28, ib_vmid, "DIMENSION", (fetch_word(asic, stream, 6) >> 9) & 0x3, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 28, ib_vmid, "EPITCH", (fetch_word(asic, stream, 6) >> 16) & 0xFFFF, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 32, ib_vmid, "X", fetch_word(asic, stream, 7) & 0x3FFF, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 32, ib_vmid, "Y", (fetch_word(asic, stream, 7) >> 16) & 0x3FFF, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 36, ib_vmid, "Z", fetch_word(asic, stream, 8) & 0x7FF, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 40, ib_vmid, "DST2_SW", (fetch_word(asic, stream, 9) >> 8) & 0x3, NULL, 10, 32);
						if (sc->has_cp_fields) {
							ui->add_field(ui, ib_addr + 40, ib_vmid, "DST2_CACHE_POLICY", (fetch_word(asic, stream, 9) >> 10) & 0x7, NULL, 10, 32);
						}
						ui->add_field(ui, ib_addr + 40, ib_vmid, "LINEAR_SW", (fetch_word(asic, stream, 9) >> 16) & 0x3, NULL, 10, 32);
						if (sc->has_cp_fields) {
							ui->add_field(ui, ib_addr + 40, ib_vmid, "LINEAR_CACHE_POLICY", (fetch_word(asic, stream, 9) >> 18) & 0x7, NULL, 10, 32);
						}
						ui->add_field(ui, ib_addr + 40, ib_vmid, "TILE_SW", (fetch_word(asic, stream, 9) >> 24) & 0x3, NULL, 10, 32);
						if (sc->has_cp_fields) {
							ui->add_field(ui, ib_addr + 40, ib_vmid, "TILE_CACHE_POLICY", (fetch_word(asic, stream, 9) >> 26) & 0x7, NULL, 10, 32);
						}

						ui->add_field(ui, ib_addr + 44, ib_vmid, "LINEAR_ADDR_LO", fetch_word(asic, stream, 10), NULL, 16, 32);
						ui->add_field(ui, ib_addr + 48, ib_vmid, "LINEAR_ADDR_HI", fetch_word(asic, stream, 11), NULL, 16, 32);
						ui->add_field(ui, ib_addr + 52, ib_vmid, "LINEAR_PITCH", fetch_word(asic, stream, 12) & 0x7FFFF, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 56, ib_vmid, "LINEAR_SLICE_PITCH", fetch_word(asic, stream, 13), NULL, 10, 32);
						ui->add_field(ui, ib_addr + 60, ib_vmid, "COUNT", fetch_word(asic, stream, 14) & 0x3FFFFF, NULL, 10, 32);
						return;
					} else {
						ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "COPY (TILED)", stream->header_dw, stream->words);
						ui->add_field(ui, ib_addr + 0, ib_vmid, "ENCRYPT", (stream->header_dw >> 16) & 0x1, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 0, ib_vmid, "TMZ", (stream->header_dw >> 18) & 0x1, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 0, ib_vmid, "MIP_MAX", (stream->header_dw >> 20) & 0xF, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 0, ib_vmid, "VIDEOCOPY", (stream->header_dw >> 26) & 0x1, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 0, ib_vmid, "BROADCAST", (stream->header_dw >> 27) & 0x1, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 0, ib_vmid, "DETILE", (stream->header_dw >> 31) & 0x1, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 4, ib_vmid, "TILED_ADDR_LO", fetch_word(asic, stream, 0), NULL, 16, 32);
						ui->add_field(ui, ib_addr + 8, ib_vmid, "TILED_ADDR_HI", fetch_word(asic, stream, 1), NULL, 16, 32);
						ui->add_field(ui, ib_addr + 12, ib_vmid, "WIDTH", fetch_word(asic, stream, 2) & 0x3FFF, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 16, ib_vmid, "HEIGHT", fetch_word(asic, stream, 3) & 0x3FFF, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 16, ib_vmid, "DEPTH", (fetch_word(asic, stream, 3) >> 16) & 0x1FFF, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 20, ib_vmid, "ELEMENT_SIZE", fetch_word(asic, stream, 4) & 0x7, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 20, ib_vmid, "SWIZZLE_MODE", (fetch_word(asic, stream, 4) >> 3) & 0x1F, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 20, ib_vmid, "DIMENSION", (fetch_word(asic, stream, 4) >> 9) & 0x3, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 20, ib_vmid, "EPITCH", (fetch_word(asic, stream, 4) >> 16) & 0xFFFF, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 24, ib_vmid, "X", fetch_word(asic, stream, 5) & 0x3FFF, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 24, ib_vmid, "Y", (fetch_word(asic, stream, 5) >> 16) & 0x3FFF, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 28, ib_vmid, "Z", fetch_word(asic, stream, 6) & 0x1FFF, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 28, ib_vmid, "LINEAR_SW", (fetch_word(asic, stream, 6) >> 16) & 0x3, NULL, 10, 32);
						if (sc->has_cp_fields) {
							ui->add_field(ui, ib_addr + 28, ib_vmid, "LINEAR_CACHE_POLICY", (fetch_word(asic, stream, 6) >> 18) & 0x7, NULL, 10, 32);
						}
						ui->add_field(ui, ib_addr + 28, ib_vmid, "TILE_SW", (fetch_word(asic, stream, 6) >> 24) & 0x3, NULL, 10, 32);
						if (sc->has_cp_fields) {
							ui->add_field(ui, ib_addr + 28, ib_vmid, "TILE_CACHE_POLICY", (fetch_word(asic, stream, 6) >> 26) & 0x7, NULL, 10, 32);
						}
						ui->add_field(ui, ib_addr + 32, ib_vmid, "LINEAR_ADDR_LO", fetch_word(asic, stream, 7), NULL, 16, 32);
						ui->add_field(ui, ib_addr + 36, ib_vmid, "LINEAR_ADDR_HI", fetch_word(asic, stream, 8), NULL, 16, 32);
						ui->add_field(ui, ib_addr + 40, ib_vmid, "LINEAR_PITCH", fetch_word(asic, stream, 9) & 0x7FFFF, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 44, ib_vmid, "LINEAR_SLICE_PITCH", fetch_word(asic, stream, 10), NULL, 10, 32);
						ui->add_field(ui, ib_addr + 48, ib_vmid, "COUNT", fetch_word(asic, stream, 11) & 0x3FFFFFFF, NULL, 10, 32);
						return;
					}
				case 3: // SOA
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "COPY (STRUCT)", stream->header_dw, stream->words);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "TMZ", (stream->header_dw >> 18) & 0x1, NULL, 10, 32);
					if (sc->has_cpv_flag) {
						ui->add_field(ui, ib_addr + 0, ib_vmid, "CPV", (stream->header_dw >> 28) & 0x1, NULL, 10, 32);
					}
					ui->add_field(ui, ib_addr + 0, ib_vmid, "DETILE", (stream->header_dw >> 31) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "SB_ADDR_LO", fetch_word(asic, stream, 0), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "SB_ADDR_HI", fetch_word(asic, stream, 1), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "START_INDEX", fetch_word(asic, stream, 2), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 16, ib_vmid, "COUNT", fetch_word(asic, stream, 3), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "STRIDE", fetch_word(asic, stream, 4) & 0x7FF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "LINEAR_SW", (fetch_word(asic, stream, 4) >> 16) & 0x3, NULL, 10, 32);
					if (sc->has_cp_fields) {
						ui->add_field(ui, ib_addr + 20, ib_vmid, "LINEAR_CACHE_POLICY", (fetch_word(asic, stream, 4) >> 18) & 0x7, NULL, 10, 32);
					}
					ui->add_field(ui, ib_addr + 20, ib_vmid, "STRUCT_SW", (fetch_word(asic, stream, 4) >> 24) & 0x3, NULL, 10, 32);
					if (sc->has_cp_fields) {
						ui->add_field(ui, ib_addr + 20, ib_vmid, "STRUCT_CACHE_POLICY", (fetch_word(asic, stream, 4) >> 26) & 0x7, NULL, 10, 32);
					}
					ui->add_field(ui, ib_addr + 24, ib_vmid, "LINEAR_ADDR_LO", fetch_word(asic, stream, 5), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 28, ib_vmid, "LINEAR_ADDR_HI", fetch_word(asic, stream, 6), NULL, 16, 32);
					return;
				case 4: // LINEAR_SUB_WINDOW
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "COPY (LINEAR_SUB_WINDOW)", stream->header_dw, stream->words);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "TMZ", (stream->header_dw >> 18) & 0x1, NULL, 10, 32);
					if (sc->has_cpv_flag) {
						ui->add_field(ui, ib_addr + 0, ib_vmid, "CPV", (stream->header_dw >> 19) & 0x1, NULL, 10, 32);
					}
					ui->add_field(ui, ib_addr + 0, ib_vmid, "ELEMENTSIZE", (stream->header_dw >> 29) & 0x7, NULL, 10, 32);

					ui->add_field(ui, ib_addr + 4, ib_vmid, "SRC_ADDR_LO", fetch_word(asic, stream, 0), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_ADDR_HI", fetch_word(asic, stream, 1), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "SRC_X", fetch_word(asic, stream, 2) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "SRC_Y", (fetch_word(asic, stream, 2) >> 16) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 16, ib_vmid, "SRC_Z", fetch_word(asic, stream, 3) & sc->z_mask, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 16, ib_vmid, "SRC_PITCH", (fetch_word(asic, stream, 3) >> sc->pitch_shift) & sc->pitch_mask, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "SRC_SLICE_PITCH", fetch_word(asic, stream, 4) & 0xFFFFFFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "DST_ADDR_LO", fetch_word(asic, stream, 5), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 28, ib_vmid, "DST_ADDR_HI", fetch_word(asic, stream, 6), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 32, ib_vmid, "DST_X", fetch_word(asic, stream, 7) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 32, ib_vmid, "DST_Y", (fetch_word(asic, stream, 7) >> 16) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 36, ib_vmid, "DST_Z", fetch_word(asic, stream, 8) & sc->z_mask, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 36, ib_vmid, "DST_PITCH", (fetch_word(asic, stream, 8) >> sc->pitch_shift) & sc->pitch_mask, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 40, ib_vmid, "DST_SLICE_PITCH", fetch_word(asic, stream, 9) & 0xFFFFFFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 44, ib_vmid, "RECT_X", fetch_word(asic, stream, 10) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 44, ib_vmid, "RECT_Y", (fetch_word(asic, stream, 10) >> 16) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 48, ib_vmid, "RECT_Z", fetch_word(asic, stream, 11) & 0x1FFF, NULL, 10, 32);

					ui->add_field(ui, ib_addr + 48, ib_vmid, "DST_SW", (fetch_word(asic, stream, 11) >> 16) & 0x3, NULL, 10, 32);
					if (sc->has_cp_fields) {
						ui->add_field(ui, ib_addr + 48, ib_vmid, "DST_CACHE_POLICY", (fetch_word(asic, stream, 11) >> 18) & 0x7, NULL, 10, 32);
					} else {
						ui->add_field(ui, ib_addr + 48, ib_vmid, "DST_HA", (fetch_word(asic, stream, 11) >> 22) & 0x1, NULL, 10, 32);
					}

					ui->add_field(ui, ib_addr + 48, ib_vmid, "SRC_SW", (fetch_word(asic, stream, 11) >> 24) & 0x3, NULL, 10, 32);
					if (sc->has_cp_fields) {
						ui->add_field(ui, ib_addr + 48, ib_vmid, "SRC_CACHE_POLICY", (fetch_word(asic, stream, 11) >> 26) & 0x7, NULL, 10, 32);
					} else {
						ui->add_field(ui, ib_addr + 48, ib_vmid, "SRC_HA", (fetch_word(asic, stream, 11) >> 30) & 0x1, NULL, 10, 32);
					}
					return;
				case 5: // TILED_SUB_WINDOW
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "COPY (TILED_SUB_WINDOW)", stream->header_dw, stream->words);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "TMZ", (stream->header_dw >> 18) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "MIP_MAX", (fetch_word(asic, stream, 0) >> 20) & 0xF, NULL, 16, 32);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "MIP_ID", (fetch_word(asic, stream, 0) >> 24) & 0xF, NULL, 16, 32);
					if (sc->has_cpv_flag) {
						ui->add_field(ui, ib_addr + 0, ib_vmid, "CPV", (stream->header_dw >> 28) & 0x1, NULL, 10, 32);
					}
					ui->add_field(ui, ib_addr + 0, ib_vmid, "DETILE", stream->header_dw >> 31, NULL, 16, 32);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "TILED_ADDR_LO", fetch_word(asic, stream, 0), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "TILED_ADDR_HI", fetch_word(asic, stream, 1), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "TILED_X", (fetch_word(asic, stream, 2) >> 0) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "TILED_Y", (fetch_word(asic, stream, 2) >> 16) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 16, ib_vmid, "TILED_Z", (fetch_word(asic, stream, 3) >> 0) & sc->z_mask, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 16, ib_vmid, "WIDTH", (fetch_word(asic, stream, 3) >> 16) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "HEIGHT", (fetch_word(asic, stream, 4) >> 0) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "DEPTH", (fetch_word(asic, stream, 4) >> 16) & sc->z_mask, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "ELEMENT_SIZE", (fetch_word(asic, stream, 5) >> 0) & 0x7, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "SWIZZLE_MODE", (fetch_word(asic, stream, 5) >> 3) & 0x1F, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "DIMENSION", (fetch_word(asic, stream, 5) >> 9) & 0x3, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "EPITCH", (fetch_word(asic, stream, 5) >> 16) & 0xFFFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 28, ib_vmid, "LINEAR_ADDR_LO", fetch_word(asic, stream, 6), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 32, ib_vmid, "LINEAR_ADDR_HI", fetch_word(asic, stream, 7), NULL, 16, 32);

					ui->add_field(ui, ib_addr + 36, ib_vmid, "LINEAR_X", (fetch_word(asic, stream, 8) >> 0) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 36, ib_vmid, "LINEAR_Y", (fetch_word(asic, stream, 8) >> 0) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 40, ib_vmid, "LINEAR_Z", (fetch_word(asic, stream, 9) >> 0) & sc->z_mask, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 40, ib_vmid, "LINEAR_PITCH", (fetch_word(asic, stream, 9) >> 16) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 44, ib_vmid, "LINEAR_SLICE_PITCH", fetch_word(asic, stream, 10) & 0xFFFFFFF, NULL, 10, 32);

					ui->add_field(ui, ib_addr + 48, ib_vmid, "RECT_X", (fetch_word(asic, stream, 11) >> 0) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 48, ib_vmid, "RECT_Y", (fetch_word(asic, stream, 11) >> 16) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 52, ib_vmid, "RECT_Z", (fetch_word(asic, stream, 12) >> 0) & sc->z_mask, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 52, ib_vmid, "LINEAR_SW", (fetch_word(asic, stream, 12) >> 16) & 0x3, NULL, 10, 32);
					if (sc->has_cp_fields) {
						ui->add_field(ui, ib_addr + 52, ib_vmid, "LINEAR_CACHE_POLICY", (fetch_word(asic, stream, 12) >> 18) & 0x7, NULL, 10, 32);
					}
					ui->add_field(ui, ib_addr + 52, ib_vmid, "TILE_SW", (fetch_word(asic, stream, 12) >> 22) & 0x3, NULL, 10, 32);
					if (sc->has_cp_fields) {
						ui->add_field(ui, ib_addr + 52, ib_vmid, "TILE_CACHE_POLICY", (fetch_word(asic, stream, 12) >> 26) & 0x7, NULL, 10, 32);
					}
					return;
				case 6: // T2T_SUB_WINDOW
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "COPY (T2T_SUB_WINDOW)", stream->header_dw, stream->words);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "TMZ", (stream->header_dw >> 18) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "MIP_MAX", (stream->header_dw >> 20) & 0xF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "SRC_ADDR_LO", fetch_word(asic, stream, 0), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_ADDR_HI", fetch_word(asic, stream, 1), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "SRC_X", (fetch_word(asic, stream, 2) >> 0) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "SRC_Y", (fetch_word(asic, stream, 2) >> 16) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 16, ib_vmid, "SRC_Z", (fetch_word(asic, stream, 3) >> 0) & sc->z_mask, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 16, ib_vmid, "SRC_WIDTH", (fetch_word(asic, stream, 3) >> 16) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "SRC_HEIGHT", (fetch_word(asic, stream, 4) >> 0) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "SRC_DEPTH", (fetch_word(asic, stream, 4) >> 16) & sc->z_mask, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "SRC_ELEMENT_SIZE", (fetch_word(asic, stream, 5) >> 0) & 0x7, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "SRC_SWIZZLE_MODE", (fetch_word(asic, stream, 5) >> 3) & 0x1F, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "SRC_DIMENSION", (fetch_word(asic, stream, 5) >> 9) & 0x3, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "SRC_EPITCH", (fetch_word(asic, stream, 5) >> 16) & 0xFFFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 28, ib_vmid, "DST_ADDR_LO", fetch_word(asic, stream, 6), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 32, ib_vmid, "DST_ADDR_HI", fetch_word(asic, stream, 7), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 36, ib_vmid, "DST_X", (fetch_word(asic, stream, 8) >> 0) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 36, ib_vmid, "DST_Y", (fetch_word(asic, stream, 8) >> 16) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 40, ib_vmid, "DST_Z", (fetch_word(asic, stream, 9) >> 0) & sc->z_mask, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 40, ib_vmid, "DST_WIDTH", (fetch_word(asic, stream, 9) >> 16) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 44, ib_vmid, "DST_HEIGHT", (fetch_word(asic, stream, 10) >> 0) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 44, ib_vmid, "DST_DEPTH", (fetch_word(asic, stream, 10) >> 16) & sc->z_mask, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 48, ib_vmid, "DST_ELEMENT_SIZE", (fetch_word(asic, stream, 11) >> 0) & 0x7, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 48, ib_vmid, "DST_SWIZZLE_MODE", (fetch_word(asic, stream, 11) >> 3) & 0x1F, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 48, ib_vmid, "DST_DIMENSION", (fetch_word(asic, stream, 11) >> 9) & 0x3, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 48, ib_vmid, "DST_EPITCH", (fetch_word(asic, stream, 11) >> 16) & 0xFFFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 52, ib_vmid, "RECT_X", (fetch_word(asic, stream, 12) >> 0) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 52, ib_vmid, "RECT_Y", (fetch_word(asic, stream, 12) >> 16) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 56, ib_vmid, "RECT_Z", (fetch_word(asic, stream, 13) >> 0) & sc->z_mask, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 56, ib_vmid, "DST_SW", (fetch_word(asic, stream, 13) >> 16) & 0x3, NULL, 10, 32);
					if (sc->has_cp_fields) {
						ui->add_field(ui, ib_addr + 56, ib_vmid, "DST_CACHE_POLICY", (fetch_word(asic, stream, 13) >> 18) & 0x7, NULL, 10, 32);
					}
					ui->add_field(ui, ib_addr + 56, ib_vmid, "SRC_SW", (fetch_word(asic, stream, 13) >> 22) & 0x3, NULL, 10, 32);
					if (sc->has_cp_fields) {
						ui->add_field(ui, ib_addr + 56, ib_vmid, "SRC_CACHE_POLICY", (fetch_word(asic, stream, 13) >> 26) & 0x7, NULL, 10, 32);
					}
					return;
				case 7: // DIRTY_PAGE
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "COPY (DIRTY_PAGE)", stream->header_dw, stream->words);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "TMZ", (stream->header_dw >> 18) & 0x1, NULL, 10, 32);
					if (sc->has_cpv_flag) {
						ui->add_field(ui, ib_addr + 0, ib_vmid, "CPV", (stream->header_dw >> 19) & 0x1, NULL, 10, 32);
					}
					ui->add_field(ui, ib_addr + 0, ib_vmid, "ALL", (stream->header_dw >> 31) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "COUNT", (fetch_word(asic, stream, 0) >> 0) & 0x3FFFFF, NULL, 10, 32);
					if (sc->has_cpv_flag) {
						ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_L2_POLICY", (fetch_word(asic, stream, 1) >> 6) & 0x3, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_LLC", (fetch_word(asic, stream, 1) >> 8) & 0x1, NULL, 10, 32);
					} else if (sc->has_cp_fields) {
						ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_CACHE_POLICY", (fetch_word(asic, stream, 1) >> 5) & 0x3, NULL, 10, 32);
					}
					if (sc->has_cpv_flag) {
						ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_L2_POLICY", (fetch_word(asic, stream, 1) >> 14) & 0x3, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_LLC", (fetch_word(asic, stream, 1) >> 16) & 0x1, NULL, 10, 32);
					} else if (sc->has_cp_fields) {
						ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_CACHE_POLICY", (fetch_word(asic, stream, 1) >> 13) & 0x3, NULL, 10, 32);
					}
					ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_SW", (fetch_word(asic, stream, 1) >> 16) & 0x3, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_GCC", (fetch_word(asic, stream, 1) >> 19) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_SYS", (fetch_word(asic, stream, 1) >> 20) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_SNOOP", (fetch_word(asic, stream, 1) >> 22) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_GPA", (fetch_word(asic, stream, 1) >> 23) & 0x1, NULL, 10, 32);
					if (sc->has_cpv_flag) {
						ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_SW", (fetch_word(asic, stream, 1) >> 25) & 0x3, NULL, 10, 32);
					} else {
						ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_SW", (fetch_word(asic, stream, 1) >> 24) & 0x3, NULL, 10, 32);
					}
					ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_SYS", (fetch_word(asic, stream, 1) >> 28) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_SNOOP", (fetch_word(asic, stream, 1) >> 30) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_GPA", (fetch_word(asic, stream, 1) >> 31) & 0x1, NULL, 10, 32);

					ui->add_field(ui, ib_addr + 12, ib_vmid, "SRC_ADDR_LO", fetch_word(asic, stream, 2), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 16, ib_vmid, "SRC_ADDR_HI", fetch_word(asic, stream, 3), NULL, 16, 32);

					ui->add_field(ui, ib_addr + 20, ib_vmid, "DST_ADDR_LO", fetch_word(asic, stream, 4), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "DST_ADDR_HI", fetch_word(asic, stream, 5), NULL, 16, 32);
					return;
				case 8: // LINEAR_PHY
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "COPY (LINEAR_PHY)", stream->header_dw, stream->words);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "TMZ", (stream->header_dw >> 18) & 0x1, NULL, 10, 32);
					if (sc->has_cpv_flag) {
						ui->add_field(ui, ib_addr + 0, ib_vmid, "CPV", (stream->header_dw >> 19) & 0x1, NULL, 10, 32);
					}
					ui->add_field(ui, ib_addr + 4, ib_vmid, "COUNT", (fetch_word(asic, stream, 0) >> 0) & 0x3FFFFF, NULL, 10, 32);
					if (sc->has_cpv_flag) {
						ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_L2_POLICY", (fetch_word(asic, stream, 1) >> 6) & 0x3, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_LLC", (fetch_word(asic, stream, 1) >> 8) & 0x1, NULL, 10, 32);
					} else if (sc->has_cp_fields) {
						ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_CACHE_POLICY", (fetch_word(asic, stream, 1) >> 5) & 0x3, NULL, 10, 32);
					}
					if (sc->has_cpv_flag) {
						ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_L2_POLICY", (fetch_word(asic, stream, 1) >> 14) & 0x3, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_LLC", (fetch_word(asic, stream, 1) >> 16) & 0x1, NULL, 10, 32);
					} else if (sc->has_cp_fields) {
						ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_CACHE_POLICY", (fetch_word(asic, stream, 1) >> 13) & 0x3, NULL, 10, 32);
					}
					ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_SW", (fetch_word(asic, stream, 1) >> 16) & 0x3, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_GCC", (fetch_word(asic, stream, 1) >> 19) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_SYS", (fetch_word(asic, stream, 1) >> 20) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_LOG", (fetch_word(asic, stream, 1) >> 21) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_SNOOP", (fetch_word(asic, stream, 1) >> 22) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_GPA", (fetch_word(asic, stream, 1) >> 23) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_SW", (fetch_word(asic, stream, 1) >> 24) & 0x3, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_GCC", (fetch_word(asic, stream, 1) >> 27) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_SYS", (fetch_word(asic, stream, 1) >> 28) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_SNOOP", (fetch_word(asic, stream, 1) >> 30) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_GPA", (fetch_word(asic, stream, 1) >> 31) & 0x1, NULL, 10, 32);
					for (n = 2; n + 3 < stream->nwords; n += 4) {
						uint32_t addr_idx = (n - 2) / 4;
						sprintf(str_buf, "SRC_ADDR%"PRIu32"_LO", addr_idx);
						ui->add_field(ui, ib_addr + 12 + 4 * (n - 2), ib_vmid, str_buf, fetch_word(asic, stream, n), NULL, 16, 32);
						sprintf(str_buf, "SRC_ADDR%"PRIu32"_HI", addr_idx);
						ui->add_field(ui, ib_addr + 16 + 4 * (n - 2), ib_vmid, str_buf, fetch_word(asic, stream, n + 1), NULL, 16, 32);
						sprintf(str_buf, "DST_ADDR%"PRIu32"_LO", addr_idx);
						ui->add_field(ui, ib_addr + 20 + 4 * (n - 2), ib_vmid, str_buf, fetch_word(asic, stream, n + 2), NULL, 16, 32);
						sprintf(str_buf, "DST_ADDR%"PRIu32"_HI", addr_idx);
						ui->add_field(ui, ib_addr + 24 + 4 * (n - 2), ib_vmid, str_buf, fetch_word(asic, stream, n + 3), NULL, 16, 32);
					}
					return;
				case 16: // LINEAR_BC
					break; // fall through
				case 17: // TILED_BC
					break; // fall through
				case 20: // LINEAR_SUB_WINDOW_BC
					break; // fall through
				case 21: // TILED_SUB_WINDOW_BC
					break; // fall through
				case 22: // T2T_SUB_WINDOW_BC
					break; // fall through
				case 36: // SUBWIN_LARGE
					break; // fall through
				default:
					if (ui->unhandled_subop)
						ui->unhandled_subop(ui, asic, ib_addr, ib_vmid, stream, UMR_RING_SDMA);
					break;
			}
			break;
		case 2: // WRITE
			switch (stream->sub_opcode) {
				case 0: // LINEAR
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "WRITE (LINEAR)", stream->header_dw, stream->words);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "ENCRYPT", (stream->header_dw >> 16) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "TMZ", (stream->header_dw >> 18) & 0x1, NULL, 10, 32);
					if (sc->has_cpv_flag) {
						ui->add_field(ui, ib_addr + 0, ib_vmid, "CPV", (stream->header_dw >> 28) & 0x1, NULL, 10, 32);
					}
					ui->add_field(ui, ib_addr + 4, ib_vmid, "DST_ADDR_LO", fetch_word(asic, stream, 0), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_ADDR_HI", fetch_word(asic, stream, 1), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "COUNT", fetch_word(asic, stream, 2), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "SWAP", (fetch_word(asic, stream, 2) >> 24) & 0x3, NULL, 10, 32);
					if (sc->has_cp_fields) {
						ui->add_field(ui, ib_addr + 12, ib_vmid, "CACHE_POLICY", (fetch_word(asic, stream, 2) >> 26) & 0x7, NULL, 10, 32);
					}
					for (n = 3; n < stream->nwords; n++) {
						uint32_t data_idx = n - 3;
						sprintf(str_buf, "DATA_%"PRIu32, data_idx);
						ui->add_field(ui, ib_addr + 16 + 4 * (n - 3), ib_vmid, str_buf, fetch_word(asic, stream, n), NULL, 16, 32);
					}
					return;
				case 1: // TILED
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "WRITE (TILED)", stream->header_dw, stream->words);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "ENCRYPT", (stream->header_dw >> 16) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "TMZ", (stream->header_dw >> 18) & 0x1, NULL, 10, 32);
					if (sc->has_cpv_flag) {
						ui->add_field(ui, ib_addr + 0, ib_vmid, "CPV", (stream->header_dw >> 28) & 0x1, NULL, 10, 32);
					}
					ui->add_field(ui, ib_addr + 4, ib_vmid, "DST_ADDR_LO", fetch_word(asic, stream, 0), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_ADDR_HI", fetch_word(asic, stream, 1), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "WIDTH", (fetch_word(asic, stream, 2) >> 16) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 16, ib_vmid, "HEIGHT", (fetch_word(asic, stream, 3) >> 0) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 16, ib_vmid, "DEPTH", (fetch_word(asic, stream, 3) >> 16) & sc->z_mask, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "ELEMENT_SIZE", (fetch_word(asic, stream, 4) >> 0) & 0x7, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "SWIZZLE_MODE", (fetch_word(asic, stream, 4) >> 3) & 0x1F, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "DIMENSION", (fetch_word(asic, stream, 4) >> 9) & 0x3, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "EPITCH", (fetch_word(asic, stream, 4) >> 16) & 0xFFFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "X", (fetch_word(asic, stream, 5) >> 0) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "Y", (fetch_word(asic, stream, 5) >> 16) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 28, ib_vmid, "Z", (fetch_word(asic, stream, 6) >> 0) & sc->z_mask, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 28, ib_vmid, "SW", (fetch_word(asic, stream, 6) >> 24) & 0x3, NULL, 10, 32);
					if (sc->has_cp_fields) {
						ui->add_field(ui, ib_addr + 28, ib_vmid, "CACHE_POLICY", (fetch_word(asic, stream, 6) >> 26) & 0x7, NULL, 10, 32);
					}
					ui->add_field(ui, ib_addr + 32, ib_vmid, "COUNT", (fetch_word(asic, stream, 7) >> 0) & 0xFFFFF, NULL, 10, 32);
					for (n = 8; n < stream->nwords; n++) {
						uint32_t data_idx = n - 8;
						sprintf(str_buf, "DATA_%"PRIu32, data_idx);
						ui->add_field(ui, ib_addr + 36 + 4 * (n - 8), ib_vmid, str_buf, fetch_word(asic, stream, n), NULL, 16, 32);
					}
					return;
				case 17: // TILED_BC
					break; // fall through
				default:
					if (ui->unhandled_subop)
						ui->unhandled_subop(ui, asic, ib_addr, ib_vmid, stream, UMR_RING_SDMA);
					break;
			}
			break;
		case 4: // INDIRECT
			break; // fall through
		case 5: // FENCE
			ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "FENCE", stream->header_dw, stream->words);
			if (sc->has_cp_fields) {
				ui->add_field(ui, ib_addr + 0, ib_vmid, "L2_POLICY", (stream->header_dw >> 24) & 0x3, NULL, 10, 32);
				ui->add_field(ui, ib_addr + 0, ib_vmid, "LLC_POLICY", (stream->header_dw >> 26) & 0x1, NULL, 10, 32);
			}
			if (sc->has_cpv_flag) {
				ui->add_field(ui, ib_addr + 0, ib_vmid, "CPV", (stream->header_dw >> 28) & 0x1, NULL, 10, 32);
			}
			ui->add_field(ui, ib_addr + 4, ib_vmid, "FENCE_ADDR_LO", fetch_word(asic, stream, 0), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "FENCE_ADDR_HI", fetch_word(asic, stream, 1), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "FENCE_DATA", fetch_word(asic, stream, 2), NULL, 16, 32);
			return;
		case 6: // TRAP
			break; // fall through
		case 7: // SEM
			switch (stream->sub_opcode) {
				case 0: // SEM
					break; // fall through
				case 1: // MEM_INCR
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "SEM (MEM_INCR)", stream->header_dw, stream->words);
					if (sc->has_cp_fields) {
						ui->add_field(ui, ib_addr + 0, ib_vmid, "L2_POLICY", (stream->header_dw >> 24) & 0x3, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 0, ib_vmid, "LLC_POLICY", (stream->header_dw >> 26) & 0x1, NULL, 10, 32);
					}
					if (sc->has_cpv_flag) {
						ui->add_field(ui, ib_addr + 0, ib_vmid, "CPV", (stream->header_dw >> 28) & 0x1, NULL, 10, 32);
					}
					ui->add_field(ui, ib_addr + 4, ib_vmid, "ADDR_LO", fetch_word(asic, stream, 0), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "ADDR_HI", fetch_word(asic, stream, 1), NULL, 16, 32);
					return;
				default:
					if (ui->unhandled_subop)
						ui->unhandled_subop(ui, asic, ib_addr, ib_vmid, stream, UMR_RING_SDMA);
					return;
			}

			break;
		case 8: // POLL_REGMEM
			switch (stream->sub_opcode) {
				case 0: // POLL_REGMEM
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "POLL_REGMEM", stream->header_dw, stream->words);
					if (sc->has_cp_fields) ui->add_field(ui, ib_addr + 0, ib_vmid, "CACHE_POLICY", (stream->header_dw >> 20) & 0x7, NULL, 16, 32);
					if (sc->has_cpv_flag) ui->add_field(ui, ib_addr + 0, ib_vmid, "CPV", (stream->header_dw >> 24) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "HDP_FLUSH", (stream->header_dw >> 26) & 1, NULL, 16, 32);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "FUNCTION", 0, poll_regmem_funcs[(stream->header_dw >> 28) & 7], 0, 32);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "MEM_POLL", (stream->header_dw >> 31) & 1, NULL, 16, 32);
					if (!(stream->header_dw & (1UL << 31))) {
						ui->add_field(ui, ib_addr + 4, ib_vmid, "REGISTER", BITS(fetch_word(asic, stream, 0), 2, 20), umr_reg_name(asic, BITS(fetch_word(asic, stream, 0), 2, 20)), 16, 32);
						if (((stream->header_dw >> 26) & 3) == 1) { // if HDP_FLUSH, the write register is provided
							ui->add_field(ui, ib_addr + 8, ib_vmid, "REGISTER", BITS(fetch_word(asic, stream, 1), 2, 18), umr_reg_name(asic, BITS(fetch_word(asic, stream, 1), 2, 18)), 16, 32);
						} else {
							ui->add_field(ui, ib_addr + 8, ib_vmid, "RESERVED", fetch_word(asic, stream, 1), NULL, 16, 32);
						}
					} else {
						ui->add_field(ui, ib_addr + 4, ib_vmid, "POLL_REGMEM_ADDR_LO", fetch_word(asic, stream, 0), NULL, 16, 32);
						ui->add_field(ui, ib_addr + 8, ib_vmid, "POLL_REGMEM_ADDR_HI", fetch_word(asic, stream, 1), NULL, 16, 32);
					}
					ui->add_field(ui, ib_addr + 12, ib_vmid, "VALUE", fetch_word(asic, stream, 2), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 16, ib_vmid, "MASK", fetch_word(asic, stream, 3), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "INTERVAL", fetch_word(asic, stream, 4) & 0xFFFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "RETRY_COUNT", (fetch_word(asic, stream, 4) >> 16) & 0xFFF, NULL, 10, 32);
					return;
				case 1: // POLL_REG_WRITE_MEM
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "POLL_REG_WRITE_MEM", stream->header_dw, stream->words);
					if (sc->has_cp_fields) ui->add_field(ui, ib_addr + 0, ib_vmid, "CACHE_POLICY", (stream->header_dw >> 24) & 0x7, NULL, 16, 32);
					if (sc->has_cpv_flag) ui->add_field(ui, ib_addr + 0, ib_vmid, "CPV", (stream->header_dw >> 28) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "SRC_ADDR", fetch_word(asic, stream, 0), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_ADDR_LO", fetch_word(asic, stream, 1), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "DST_ADDR_HI", fetch_word(asic, stream, 2), NULL, 16, 32);
					return;
				case 2: // POLL_DBIT_WRITE_MEM
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "POLL_DBIT_WRITE_MEM", stream->header_dw, stream->words);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "EA", (stream->header_dw >> 16) & 0x3, NULL, 10, 32);
					if (sc->has_cp_fields) ui->add_field(ui, ib_addr + 0, ib_vmid, "CACHE_POLICY", (stream->header_dw >> 24) & 0x7, NULL, 16, 32);
					if (sc->has_cpv_flag) ui->add_field(ui, ib_addr + 0, ib_vmid, "CPV", (stream->header_dw >> 28) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "DST_ADDR_LO", fetch_word(asic, stream, 0), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_ADDR_HI", fetch_word(asic, stream, 1), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "START_PAGE", (fetch_word(asic, stream, 2) >> 4) & 0xFFFFFFF, NULL, 16, 32);
					ui->add_field(ui, ib_addr + 16, ib_vmid, "PAGE_NUM", fetch_word(asic, stream, 3), NULL, 16, 32);
					return;
				case 3: // MEM_VERIFY
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "MEM_VERIFY", stream->header_dw, stream->words);
					if (sc->has_cp_fields) ui->add_field(ui, ib_addr + 0, ib_vmid, "CACHE_POLICY", (stream->header_dw >> 24) & 0x7, NULL, 16, 32);
					if (sc->has_cpv_flag) ui->add_field(ui, ib_addr + 0, ib_vmid, "CPV", (stream->header_dw >> 28) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "MODE", (stream->header_dw >> 31) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "PATTERN", fetch_word(asic, stream, 0), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "CMP0_ADDR_START_LO", fetch_word(asic, stream, 1), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "CMP0_ADDR_START_HI", fetch_word(asic, stream, 2), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 16, ib_vmid, "CMP0_ADDR_END_LO", fetch_word(asic, stream, 3), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "CMP0_ADDR_END_HI", fetch_word(asic, stream, 4), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "CMP1_ADDR_START_LO", fetch_word(asic, stream, 5), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 28, ib_vmid, "CMP1_ADDR_START_HI", fetch_word(asic, stream, 6), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 32, ib_vmid, "CMP1_ADDR_END_LO", fetch_word(asic, stream, 7), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 36, ib_vmid, "CMP1_ADDR_END_HI", fetch_word(asic, stream, 8), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 40, ib_vmid, "REC_ADDR_LO", fetch_word(asic, stream, 9), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 44, ib_vmid, "REC_ADDR_HI", fetch_word(asic, stream, 10), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 48, ib_vmid, "RESERVED", fetch_word(asic, stream, 11), NULL, 10, 32);
					return;
				case 4: // INVALIDATION
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "INVALIDATION", stream->header_dw, stream->words);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "INVALIDATEREQ", fetch_word(asic, stream, 0), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "ADDRESSRANGE_LO", fetch_word(asic, stream, 1), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "INVALIDATEACK", (fetch_word(asic, stream, 2) >> 0) & 0xFFFF, NULL, 16, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "ADDRESSRANGE_HI", (fetch_word(asic, stream, 2) >> 16) & 0x1F, NULL, 16, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "INVALIDATEGFXHUB", (fetch_word(asic, stream, 2) >> 21) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "INVALIDATEMMHUB", (fetch_word(asic, stream, 2) >> 22) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "RESERVED", (fetch_word(asic, stream, 2) >> 23) & 0x1FF, NULL, 16, 32);
					return;
				default:
					if (ui->unhandled_subop)
						ui->unhandled_subop(ui, asic, ib_addr, ib_vmid, stream, UMR_RING_SDMA);
					return;
			}
			break;
		case 9:  // COND_EXE
			ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "COND_EXE", stream->header_dw, stream->words);
			if (sc->has_cp_fields) ui->add_field(ui, ib_addr + 0, ib_vmid, "CACHE_POLICY", (stream->header_dw >> 24) & 0x7, NULL, 16, 32);
			if (sc->has_cpv_flag) ui->add_field(ui, ib_addr + 0, ib_vmid, "CPV", (stream->header_dw >> 28) & 0x1, NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "ADDR_LO", fetch_word(asic, stream, 0), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "ADDR_HI", fetch_word(asic, stream, 1), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "REFERENCE", fetch_word(asic, stream, 2), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 16, ib_vmid, "EXEC_COUNT", fetch_word(asic, stream, 3), NULL, 10, 32);
			return;
		case 10:  // ATOMIC
			ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "ATOMIC", stream->header_dw, stream->words);
			ui->add_field(ui, ib_addr + 0, ib_vmid, "LOOP", (stream->header_dw >> 16) & 1, NULL, 16, 32);
			ui->add_field(ui, ib_addr + 0, ib_vmid, "TMZ", (stream->header_dw >> 18) & 0x1, NULL, 10, 32);
			if (sc->has_cp_fields) ui->add_field(ui, ib_addr + 0, ib_vmid, "CACHE_POLICY", (stream->header_dw >> 20) & 0x7, NULL, 16, 32);
			if (sc->has_cpv_flag) ui->add_field(ui, ib_addr + 0, ib_vmid, "CPV", (stream->header_dw >> 24) & 0x1, NULL, 10, 32);
			ui->add_field(ui, ib_addr + 0, ib_vmid, "OP", (stream->header_dw >> 25) & 0x7F, NULL, 16, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "ADDR_LO", fetch_word(asic, stream, 0), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "ADDR_HI", fetch_word(asic, stream, 1), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "SRC_DATA_LO", fetch_word(asic, stream, 2), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 16, ib_vmid, "SRC_DATA_HI", fetch_word(asic, stream, 3), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 20, ib_vmid, "CMP_DATA_LO", fetch_word(asic, stream, 4), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 24, ib_vmid, "CMP_DATA_HI", fetch_word(asic, stream, 5), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 28, ib_vmid, "LOOP_INTERVAL", fetch_word(asic, stream, 6) & 0x1FFF, NULL, 16, 32);
			return;
		case 11: // CONST_FILL
			switch (stream->sub_opcode) {
				case 0: // CONST_FILL
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "CONST_FILL", stream->header_dw, stream->words);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "SWAP", (stream->header_dw >> 16) & 0x3, NULL, 10, 32);
					if (sc->has_cp_fields) ui->add_field(ui, ib_addr + 0, ib_vmid, "CACHE_POLICY", (stream->header_dw >> 24) & 0x7, NULL, 16, 32);
					if (sc->has_cpv_flag) ui->add_field(ui, ib_addr + 0, ib_vmid, "CPV", (stream->header_dw >> 28) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "FILL_SIZE", (stream->header_dw >> 30) & 0x3, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "CONST_FILL_DST_LO", fetch_word(asic, stream, 0), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "CONST_FILL_DST_HI", fetch_word(asic, stream, 1), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "CONST_FILL_DATA", fetch_word(asic, stream, 2), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 16, ib_vmid, "CONST_FILL_BYTE_COUNT", fetch_word(asic, stream, 3), NULL, 10, 32);
					return;
				case 1: // CONST_FILL (DATA_FILL_MULTI)
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "CONST_FILL (DATA_FILL_MULTI)", stream->header_dw, stream->words);
					if (sc->has_cp_fields) ui->add_field(ui, ib_addr + 0, ib_vmid, "CACHE_POLICY", (stream->header_dw >> 24) & 0x7, NULL, 16, 32);
					if (sc->has_cpv_flag) ui->add_field(ui, ib_addr + 0, ib_vmid, "CPV", (stream->header_dw >> 28) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "MEMLOG_CLR", (stream->header_dw >> 31) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "BYTE_STRIDE", fetch_word(asic, stream, 0), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "DMA_COUNT", fetch_word(asic, stream, 1), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "DST_ADDR_LO", fetch_word(asic, stream, 2), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 16, ib_vmid, "DST_ADDR_HI", fetch_word(asic, stream, 3), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "COUNT", (fetch_word(asic, stream, 4) >> 0) & 0x3FFFFFF, NULL, 10, 32);
					return;
				default:
					if (ui->unhandled_subop)
						ui->unhandled_subop(ui, asic, ib_addr, ib_vmid, stream, UMR_RING_SDMA);
					return;
			}
			break;
		case 12: // GEN_PTEPDE
			switch (stream->sub_opcode) {
				case 0: // GEN_PTEPDE
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "GEN_PTEPDE", stream->header_dw, stream->words);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "DST_ADDR_LO", fetch_word(asic, stream, 0), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_ADDR_HI", fetch_word(asic, stream, 1), NULL, 16, 32);
					if (sc->has_cp_fields) ui->add_field(ui, ib_addr + 0, ib_vmid, "CACHE_POLICY", (stream->header_dw >> 24) & 0x7, NULL, 10, 32);
					if (sc->has_cpv_flag) ui->add_field(ui, ib_addr + 0, ib_vmid, "CPV", (stream->header_dw >> 28) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "MASK_LO", fetch_word(asic, stream, 2), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 16, ib_vmid, "MASK_HI", fetch_word(asic, stream, 3), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "INIT_LO", fetch_word(asic, stream, 4), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "INIT_HI", fetch_word(asic, stream, 5), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 28, ib_vmid, "INCR_LO", fetch_word(asic, stream, 6), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 32, ib_vmid, "INCR_HI", fetch_word(asic, stream, 7), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 36, ib_vmid, "COUNT", fetch_word(asic, stream, 8) & 0x7FFFF, NULL, 10, 32);
					return;
				case 1: // COPY
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "GEN_PTEPDE (COPY)", stream->header_dw, stream->words);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "TMZ", (stream->header_dw >> 18) & 0x1, NULL, 10, 32);
					if (sc->has_cpv_flag) {
						ui->add_field(ui, ib_addr + 0, ib_vmid, "CPV", (stream->header_dw >> 28) & 0x1, NULL, 10, 32);
					}
					ui->add_field(ui, ib_addr + 0, ib_vmid, "PTEPDE_OP", (stream->header_dw >> 31) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "SRC_ADDR_LO", fetch_word(asic, stream, 0), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_ADDR_HI", fetch_word(asic, stream, 1), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "DST_ADDR_LO", fetch_word(asic, stream, 2), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 16, ib_vmid, "DST_ADDR_HI", fetch_word(asic, stream, 3), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "MASK_LO", fetch_word(asic, stream, 4), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "MASK_HI", fetch_word(asic, stream, 5), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 28, ib_vmid, "COUNT", fetch_word(asic, stream, 6) & 0x7FFFF, NULL, 10, 32);
					if (sc->has_cp_fields) {
						ui->add_field(ui, ib_addr + 28, ib_vmid, "DST_CACHE_POLICY", (fetch_word(asic, stream, 6) >> 22) & 0x7, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 28, ib_vmid, "SRC_CACHE_POLICY", (fetch_word(asic, stream, 6) >> 29) & 0x7, NULL, 10, 32);
					}
					return;
				case 2: // RMW
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "GEN_PTEPDE (RMW)", stream->header_dw, stream->words);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "MTYPE", (stream->header_dw >> 16) & 0x7, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "GCC", (stream->header_dw >> 19) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "SYS", (stream->header_dw >> 20) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "SNP", (stream->header_dw >> 22) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "GPA", (stream->header_dw >> 23) & 0x1, NULL, 10, 32);
					if (sc->has_cp_fields) {
						ui->add_field(ui, ib_addr + 0, ib_vmid, "L2_POLICY", (stream->header_dw >> 24) & 0x3, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 0, ib_vmid, "LLC_POLICY", (stream->header_dw >> 26) & 0x1, NULL, 10, 32);
					}
					if (sc->has_cpv_flag) {
						ui->add_field(ui, ib_addr + 0, ib_vmid, "CPV", (stream->header_dw >> 28) & 0x1, NULL, 10, 32);
					}
					ui->add_field(ui, ib_addr + 4, ib_vmid, "ADDR_LO", fetch_word(asic, stream, 0), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "ADDR_HI", fetch_word(asic, stream, 1), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "MASK_LO", fetch_word(asic, stream, 2), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 16, ib_vmid, "MASK_HI", fetch_word(asic, stream, 3), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "VALUE_LO", fetch_word(asic, stream, 4), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "VALUE_HI", fetch_word(asic, stream, 5), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 28, ib_vmid, "NUM_OF_PTE", fetch_word(asic, stream, 6), NULL, 10, 32);
					return;
				default:
					if (ui->unhandled_subop)
						ui->unhandled_subop(ui, asic, ib_addr, ib_vmid, stream, UMR_RING_SDMA);
					return;
			}
			break;
		case 13: // TIMESTAMP
			switch (stream->sub_opcode) {
				case 0: // TIMESTAMP_SET
					break; // fall through
				case 1: // TIMESTAMP_GET
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "TIMESTAMP (GET)", stream->header_dw, stream->words);
					if (sc->has_cp_fields) {
						ui->add_field(ui, ib_addr + 0, ib_vmid, "L2_POLICY", (stream->header_dw >> 24) & 0x3, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 0, ib_vmid, "LLC_POLICY", (stream->header_dw >> 26) & 0x1, NULL, 10, 32);
					}
					if (sc->has_cpv_flag) {
						ui->add_field(ui, ib_addr + 0, ib_vmid, "CPV", (stream->header_dw >> 28) & 0x1, NULL, 10, 32);
					}
					ui->add_field(ui, ib_addr + 4, ib_vmid, "WRITE_ADDR_LO", fetch_word(asic, stream, 0), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "WRITE_ADDR_HI", fetch_word(asic, stream, 1), NULL, 16, 32);
					return;
				case 2: // TIMESTAMP_GET_GLOBAL
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "TIMESTAMP (GET_GLOBAL)", stream->header_dw, stream->words);
					if (sc->has_cp_fields) {
						ui->add_field(ui, ib_addr + 0, ib_vmid, "L2_POLICY", (stream->header_dw >> 24) & 0x3, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 0, ib_vmid, "LLC_POLICY", (stream->header_dw >> 26) & 0x1, NULL, 10, 32);
					}
					if (sc->has_cpv_flag) {
						ui->add_field(ui, ib_addr + 0, ib_vmid, "CPV", (stream->header_dw >> 28) & 0x1, NULL, 10, 32);
					}
					ui->add_field(ui, ib_addr + 4, ib_vmid, "WRITE_ADDR_LO", fetch_word(asic, stream, 0), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "WRITE_ADDR_HI", fetch_word(asic, stream, 1), NULL, 16, 32);
					return;
				default:
					if (ui->unhandled_subop)
						ui->unhandled_subop(ui, asic, ib_addr, ib_vmid, stream, UMR_RING_SDMA);
					return;
			}
			break;
		case 14: // WRITE REG
			switch (stream->sub_opcode) {
				case 0: // SRBM_WRITE
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "SRBM_WRITE", stream->header_dw, stream->words);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "BYTE_ENABLE", (stream->header_dw >> 28), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "SRBM_WRITE_ADDR", fetch_word(asic, stream, 0) & 0x3FFFF, umr_reg_name(asic, fetch_word(asic, stream, 0) & 0x3FFFF), 16, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "SRBM_WRITE_DATA", fetch_word(asic, stream, 1), NULL, 16, 32);
					return;
				default:
					if (ui->unhandled_subop)
						ui->unhandled_subop(ui, asic, ib_addr, ib_vmid, stream, UMR_RING_SDMA);
					return;
			}
			break;
	}

	// fall through to VI decoder
	decode_upto_vi(asic, ui, stream, ib_addr, ib_vmid, from_addr, from_vmid, follow, sc);
}

static void decode_upto_nv(struct umr_asic *asic, struct umr_stream_decode_ui *ui, struct umr_sdma_stream *stream,
			   uint64_t ib_addr, uint32_t ib_vmid, uint64_t from_addr, uint64_t from_vmid, int follow, struct sdma_config *sc)
{
	uint32_t n;
	char str_buf[128];
	(void)from_addr;
	(void)from_vmid;
	switch (stream->opcode) {
		case 1: // COPY
			switch (stream->sub_opcode) {
				case 0: // LINEAR
					switch (stream->header_dw & (1UL << 27)) {
						case 0: // not broadcast
							break; // fall through
						case 1: // broadcast
							break; // fall through
						default:
							break;
					}
					break;
				case 1: // TILED
					if ((stream->header_dw >> 26) & 0x3) { // L2T BROADCAST or L2T FRAME TO FIELD
						if ((stream->header_dw >> 26) & 0x1) {
							ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "COPY (L2T_FRAME_TO_FIELD)", stream->header_dw, stream->words);
						} else {
							ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "COPY (L2T_BROADCAST)", stream->header_dw, stream->words);
						}
						ui->add_field(ui, ib_addr + 0, ib_vmid, "ENCRYPT", (stream->header_dw >> 16) & 0x1, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 0, ib_vmid, "TMZ", (stream->header_dw >> 18) & 0x1, NULL, 10, 32);
						if (sc->has_cpv_flag) {
							ui->add_field(ui, ib_addr + 0, ib_vmid, "CPV", (stream->header_dw >> 19) & 0x1, NULL, 10, 32);
						}
						ui->add_field(ui, ib_addr + 0, ib_vmid, "VIDEOCOPY", (stream->header_dw >> 26) & 0x1, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 0, ib_vmid, "BROADCAST", (stream->header_dw >> 27) & 0x1, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 4, ib_vmid, "TILED_ADDR0_LO", fetch_word(asic, stream, 0), NULL, 16, 32);
						ui->add_field(ui, ib_addr + 8, ib_vmid, "TILED_ADDR0_HI", fetch_word(asic, stream, 1), NULL, 16, 32);
						ui->add_field(ui, ib_addr + 12, ib_vmid, "TILED_ADDR1_LO", fetch_word(asic, stream, 2), NULL, 16, 32);
						ui->add_field(ui, ib_addr + 16, ib_vmid, "TILED_ADDR1_HI", fetch_word(asic, stream, 3), NULL, 16, 32);
						ui->add_field(ui, ib_addr + 20, ib_vmid, "WIDTH", fetch_word(asic, stream, 4) & 0x3FFF, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 24, ib_vmid, "HEIGHT", fetch_word(asic, stream, 5) & 0x3FFF, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 24, ib_vmid, "DEPTH", (fetch_word(asic, stream, 5) >> 16) & 0x1FFF, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 28, ib_vmid, "ELEMENT_SIZE", fetch_word(asic, stream, 6) & 0x7, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 28, ib_vmid, "SWIZZLE_MODE", (fetch_word(asic, stream, 6) >> 3) & 0x1F, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 28, ib_vmid, "DIMENSION", (fetch_word(asic, stream, 6) >> 9) & 0x3, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 28, ib_vmid, "MIP_MAX", (fetch_word(asic, stream, 6) >> 16) & 0xF, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 32, ib_vmid, "X", fetch_word(asic, stream, 7) & 0x3FFF, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 32, ib_vmid, "Y", (fetch_word(asic, stream, 7) >> 16) & 0x3FFF, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 36, ib_vmid, "Z", fetch_word(asic, stream, 8) & 0x7FF, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 40, ib_vmid, "DST2_SW", (fetch_word(asic, stream, 9) >> 8) & 0x3, NULL, 10, 32);
						if (sc->has_cp_fields) {
							ui->add_field(ui, ib_addr + 40, ib_vmid, "DST2_CACHE_POLICY", (fetch_word(asic, stream, 9) >> 10) & 0x7, NULL, 10, 32);
						}
						ui->add_field(ui, ib_addr + 40, ib_vmid, "LINEAR_SW", (fetch_word(asic, stream, 9) >> 16) & 0x3, NULL, 10, 32);
						if (sc->has_cp_fields) {
							ui->add_field(ui, ib_addr + 40, ib_vmid, "LINEAR_CACHE_POLICY", (fetch_word(asic, stream, 9) >> 18) & 0x7, NULL, 10, 32);
						}
						ui->add_field(ui, ib_addr + 40, ib_vmid, "TILE_SW", (fetch_word(asic, stream, 9) >> 24) & 0x3, NULL, 10, 32);
						if (sc->has_cp_fields) {
							ui->add_field(ui, ib_addr + 40, ib_vmid, "TILE_CACHE_POLICY", (fetch_word(asic, stream, 9) >> 26) & 0x7, NULL, 10, 32);
						}

						ui->add_field(ui, ib_addr + 44, ib_vmid, "LINEAR_ADDR_LO", fetch_word(asic, stream, 10), NULL, 16, 32);
						ui->add_field(ui, ib_addr + 48, ib_vmid, "LINEAR_ADDR_HI", fetch_word(asic, stream, 11), NULL, 16, 32);
						ui->add_field(ui, ib_addr + 52, ib_vmid, "LINEAR_PITCH", fetch_word(asic, stream, 12) & 0x7FFFF, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 56, ib_vmid, "LINEAR_SLICE_PITCH", fetch_word(asic, stream, 13), NULL, 10, 32);
						ui->add_field(ui, ib_addr + 60, ib_vmid, "COUNT", fetch_word(asic, stream, 14) & 0x3FFFFF, NULL, 10, 32);
						return;
					} else {
						ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "COPY (TILED)", stream->header_dw, stream->words);
						ui->add_field(ui, ib_addr + 0, ib_vmid, "ENCRYPT", (stream->header_dw >> 16) & 0x1, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 0, ib_vmid, "TMZ", (stream->header_dw >> 18) & 0x1, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 0, ib_vmid, "VIDEOCOPY", (stream->header_dw >> 26) & 0x1, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 0, ib_vmid, "BROADCAST", (stream->header_dw >> 27) & 0x1, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 0, ib_vmid, "DETILE", (stream->header_dw >> 31) & 0x1, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 4, ib_vmid, "TILED_ADDR_LO", fetch_word(asic, stream, 0), NULL, 16, 32);
						ui->add_field(ui, ib_addr + 8, ib_vmid, "TILED_ADDR_HI", fetch_word(asic, stream, 1), NULL, 16, 32);
						ui->add_field(ui, ib_addr + 12, ib_vmid, "WIDTH", fetch_word(asic, stream, 2) & 0x3FFF, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 16, ib_vmid, "HEIGHT", fetch_word(asic, stream, 3) & 0x3FFF, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 16, ib_vmid, "DEPTH", (fetch_word(asic, stream, 3) >> 16) & 0x1FFF, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 20, ib_vmid, "ELEMENT_SIZE", fetch_word(asic, stream, 4) & 0x7, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 20, ib_vmid, "SWIZZLE_MODE", (fetch_word(asic, stream, 4) >> 3) & 0x1F, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 20, ib_vmid, "DIMENSION", (fetch_word(asic, stream, 4) >> 9) & 0x3, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 20, ib_vmid, "MIP_MAX", (fetch_word(asic, stream, 4) >> 16) & 0xF, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 24, ib_vmid, "X", fetch_word(asic, stream, 5) & 0x3FFF, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 24, ib_vmid, "Y", (fetch_word(asic, stream, 5) >> 16) & 0x3FFF, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 28, ib_vmid, "Z", fetch_word(asic, stream, 6) & 0x1FFF, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 28, ib_vmid, "LINEAR_SW", (fetch_word(asic, stream, 6) >> 16) & 0x3, NULL, 10, 32);
						if (sc->has_cp_fields) {
							ui->add_field(ui, ib_addr + 28, ib_vmid, "LINEAR_CACHE_POLICY", (fetch_word(asic, stream, 6) >> 18) & 0x7, NULL, 10, 32);
						}
						ui->add_field(ui, ib_addr + 28, ib_vmid, "TILE_SW", (fetch_word(asic, stream, 6) >> 24) & 0x3, NULL, 10, 32);
						if (sc->has_cp_fields) {
							ui->add_field(ui, ib_addr + 28, ib_vmid, "TILE_CACHE_POLICY", (fetch_word(asic, stream, 6) >> 26) & 0x7, NULL, 10, 32);
						}
						ui->add_field(ui, ib_addr + 32, ib_vmid, "LINEAR_ADDR_LO", fetch_word(asic, stream, 7), NULL, 16, 32);
						ui->add_field(ui, ib_addr + 36, ib_vmid, "LINEAR_ADDR_HI", fetch_word(asic, stream, 8), NULL, 16, 32);
						ui->add_field(ui, ib_addr + 40, ib_vmid, "LINEAR_PITCH", fetch_word(asic, stream, 9) & 0x7FFFF, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 44, ib_vmid, "LINEAR_SLICE_PITCH", fetch_word(asic, stream, 10), NULL, 10, 32);
						ui->add_field(ui, ib_addr + 48, ib_vmid, "COUNT", fetch_word(asic, stream, 11) & 0x3FFFFFFF, NULL, 10, 32);
						return;
					}
				case 3: // SOA
					break; // fall through
				case 4: // LINEAR_SUB_WINDOW
					break; // fall through
				case 5: // TILED_SUB_WINDOW
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "COPY (TILED_SUB_WINDOW)", stream->header_dw, stream->words);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "TMZ", (stream->header_dw >> 18) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "DCC", (stream->header_dw >> 19) & 0x1, NULL, 10, 32);
					if (sc->has_cpv_flag) {
						ui->add_field(ui, ib_addr + 0, ib_vmid, "CPV", (stream->header_dw >> 28) & 0x1, NULL, 10, 32);
					}
					ui->add_field(ui, ib_addr + 0, ib_vmid, "DETILE", stream->header_dw >> 31, NULL, 16, 32);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "TILED_ADDR_LO", fetch_word(asic, stream, 0), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "TILED_ADDR_HI", fetch_word(asic, stream, 1), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "TILED_X", (fetch_word(asic, stream, 2) >> 0) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "TILED_Y", (fetch_word(asic, stream, 2) >> 16) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 16, ib_vmid, "TILED_Z", (fetch_word(asic, stream, 3) >> 0) & sc->z_mask, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 16, ib_vmid, "WIDTH", (fetch_word(asic, stream, 3) >> 16) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "HEIGHT", (fetch_word(asic, stream, 4) >> 0) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "DEPTH", (fetch_word(asic, stream, 4) >> 16) & sc->z_mask, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "ELEMENT_SIZE", (fetch_word(asic, stream, 5) >> 0) & 0x7, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "SWIZZLE_MODE", (fetch_word(asic, stream, 5) >> 3) & 0x1F, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "DIMENSION", (fetch_word(asic, stream, 5) >> 9) & 0x3, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "MIP_MAX", (fetch_word(asic, stream, 5) >> 16) & 0xF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "MIP_ID", (fetch_word(asic, stream, 5) >> 20) & 0xF, NULL, 10, 32);

					ui->add_field(ui, ib_addr + 28, ib_vmid, "LINEAR_ADDR_LO", fetch_word(asic, stream, 6), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 32, ib_vmid, "LINEAR_ADDR_HI", fetch_word(asic, stream, 7), NULL, 16, 32);

					ui->add_field(ui, ib_addr + 36, ib_vmid, "LINEAR_X", (fetch_word(asic, stream, 8) >> 0) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 36, ib_vmid, "LINEAR_Y", (fetch_word(asic, stream, 8) >> 0) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 40, ib_vmid, "LINEAR_Z", (fetch_word(asic, stream, 9) >> 0) & sc->z_mask, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 40, ib_vmid, "LINEAR_PITCH", (fetch_word(asic, stream, 9) >> 16) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 44, ib_vmid, "LINEAR_SLICE_PITCH", fetch_word(asic, stream, 10) & 0xFFFFFFF, NULL, 10, 32);

					ui->add_field(ui, ib_addr + 48, ib_vmid, "RECT_X", (fetch_word(asic, stream, 11) >> 0) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 48, ib_vmid, "RECT_Y", (fetch_word(asic, stream, 11) >> 16) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 52, ib_vmid, "RECT_Z", (fetch_word(asic, stream, 12) >> 0) & sc->z_mask, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 52, ib_vmid, "LINEAR_SW", (fetch_word(asic, stream, 12) >> 16) & 0x3, NULL, 10, 32);
					if (sc->has_cp_fields) {
						ui->add_field(ui, ib_addr + 52, ib_vmid, "LINEAR_CACHE_POLICY", (fetch_word(asic, stream, 12) >> 18) & 0x7, NULL, 10, 32);
					}
					ui->add_field(ui, ib_addr + 52, ib_vmid, "TILE_SW", (fetch_word(asic, stream, 12) >> 22) & 0x3, NULL, 10, 32);
					if (sc->has_cp_fields) {
						ui->add_field(ui, ib_addr + 52, ib_vmid, "TILE_CACHE_POLICY", (fetch_word(asic, stream, 12) >> 26) & 0x7, NULL, 10, 32);
					}
					ui->add_field(ui, ib_addr + 56, ib_vmid, "META_ADDR_LO", fetch_word(asic, stream, 13), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 60, ib_vmid, "META_ADDR_HI", fetch_word(asic, stream, 14), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 64, ib_vmid, "DATA_FORMAT", fetch_word(asic, stream, 15) & 0x7F, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 64, ib_vmid, "COLOR_TRANSFORM_DISABLE", (fetch_word(asic, stream, 15) >> 7) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 64, ib_vmid, "ALPHA_IS_ON_MSB", (fetch_word(asic, stream, 15) >> 8) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 64, ib_vmid, "NUMBER_TYPE", (fetch_word(asic, stream, 15) >> 9) & 0x7, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 64, ib_vmid, "SURFACE_TYPE", (fetch_word(asic, stream, 15) >> 12) & 0x3, NULL, 10, 32);
					if (sc->has_cpv_flag) ui->add_field(ui, ib_addr + 64, ib_vmid, "META_LLC", (fetch_word(asic, stream, 15) >> 14) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 64, ib_vmid, "MAX_COMP_BLOCK_SIZE", (fetch_word(asic, stream, 15) >> 24) & 0x3, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 64, ib_vmid, "MAX_UNCOMP_BLOCK_SIZE", (fetch_word(asic, stream, 15) >> 26) & 0x3, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 64, ib_vmid, "WRITE_COMPRESS_ENABLE", (fetch_word(asic, stream, 15) >> 28) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 64, ib_vmid, "META_TMZ", (fetch_word(asic, stream, 15) >> 29) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 64, ib_vmid, "PIPE_ALIGNED", (fetch_word(asic, stream, 15) >> 31) & 0x1, NULL, 10, 32);
					return;
				case 6: // T2T_SUB_WINDOW
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "COPY (T2T_SUB_WINDOW)", stream->header_dw, stream->words);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "TMZ", (stream->header_dw >> 18) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "DCC", (stream->header_dw >> 19) & 0x1, NULL, 10, 32);
					if (sc->has_cpv_flag) ui->add_field(ui, ib_addr + 0, ib_vmid, "CPV", (stream->header_dw >> 28) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "DCC_DIR", (stream->header_dw >> 31) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "SRC_ADDR_LO", fetch_word(asic, stream, 0), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_ADDR_HI", fetch_word(asic, stream, 1), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "SRC_X", (fetch_word(asic, stream, 2) >> 0) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "SRC_Y", (fetch_word(asic, stream, 2) >> 16) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 16, ib_vmid, "SRC_Z", (fetch_word(asic, stream, 3) >> 0) & sc->z_mask, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 16, ib_vmid, "SRC_WIDTH", (fetch_word(asic, stream, 3) >> 16) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "SRC_HEIGHT", (fetch_word(asic, stream, 4) >> 0) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "SRC_DEPTH", (fetch_word(asic, stream, 4) >> 16) & sc->z_mask, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "SRC_ELEMENT_SIZE", (fetch_word(asic, stream, 5) >> 0) & 0x7, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "SRC_SWIZZLE_MODE", (fetch_word(asic, stream, 5) >> 3) & 0x1F, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "SRC_DIMENSION", (fetch_word(asic, stream, 5) >> 9) & 0x3, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "SRC_MIP_MAX", (fetch_word(asic, stream, 5) >> 16) & 0xF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "SRC_MIP_ID", (fetch_word(asic, stream, 5) >> 20) & 0xF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 28, ib_vmid, "DST_ADDR_LO", fetch_word(asic, stream, 6), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 32, ib_vmid, "DST_ADDR_HI", fetch_word(asic, stream, 7), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 36, ib_vmid, "DST_X", (fetch_word(asic, stream, 8) >> 0) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 36, ib_vmid, "DST_Y", (fetch_word(asic, stream, 8) >> 16) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 40, ib_vmid, "DST_Z", (fetch_word(asic, stream, 9) >> 0) & sc->z_mask, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 40, ib_vmid, "DST_WIDTH", (fetch_word(asic, stream, 9) >> 16) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 44, ib_vmid, "DST_HEIGHT", (fetch_word(asic, stream, 10) >> 0) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 44, ib_vmid, "DST_DEPTH", (fetch_word(asic, stream, 10) >> 16) & sc->z_mask, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 48, ib_vmid, "DST_ELEMENT_SIZE", (fetch_word(asic, stream, 11) >> 0) & 0x7, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 48, ib_vmid, "DST_SWIZZLE_MODE", (fetch_word(asic, stream, 11) >> 3) & 0x1F, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 48, ib_vmid, "DST_DIMENSION", (fetch_word(asic, stream, 11) >> 9) & 0x3, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 48, ib_vmid, "DST_MIP_MAX", (fetch_word(asic, stream, 11) >> 16) & 0xF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 48, ib_vmid, "DST_MIP_ID", (fetch_word(asic, stream, 11) >> 20) & 0xF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 52, ib_vmid, "RECT_X", (fetch_word(asic, stream, 12) >> 0) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 52, ib_vmid, "RECT_Y", (fetch_word(asic, stream, 12) >> 16) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 56, ib_vmid, "RECT_Z", (fetch_word(asic, stream, 13) >> 0) & sc->z_mask, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 56, ib_vmid, "DST_SW", (fetch_word(asic, stream, 13) >> 16) & 0x3, NULL, 10, 32);
					if (sc->has_cp_fields) {
						ui->add_field(ui, ib_addr + 56, ib_vmid, "DST_CACHE_POLICY", (fetch_word(asic, stream, 13) >> 18) & 0x7, NULL, 10, 32);
					}
					ui->add_field(ui, ib_addr + 56, ib_vmid, "SRC_SW", (fetch_word(asic, stream, 13) >> 22) & 0x3, NULL, 10, 32);
					if (sc->has_cp_fields) {
						ui->add_field(ui, ib_addr + 56, ib_vmid, "SRC_CACHE_POLICY", (fetch_word(asic, stream, 13) >> 26) & 0x7, NULL, 10, 32);
					}
					ui->add_field(ui, ib_addr + 60, ib_vmid, "META_ADDR_LO", fetch_word(asic, stream, 14), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 64, ib_vmid, "META_ADDR_HI", fetch_word(asic, stream, 15), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 68, ib_vmid, "DATA_FORMAT", fetch_word(asic, stream, 16) & 0x7F, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 68, ib_vmid, "COLOR_TRANSFORM_DISABLE", (fetch_word(asic, stream, 16) >> 7) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 68, ib_vmid, "ALPHA_IS_ON_MSB", (fetch_word(asic, stream, 16) >> 8) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 68, ib_vmid, "NUMBER_TYPE", (fetch_word(asic, stream, 16) >> 9) & 0x7, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 68, ib_vmid, "SURFACE_TYPE", (fetch_word(asic, stream, 16) >> 12) & 0x3, NULL, 10, 32);
					if (sc->has_cpv_flag) ui->add_field(ui, ib_addr + 68, ib_vmid, "META_LLC", (fetch_word(asic, stream, 16) >> 14) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 68, ib_vmid, "MAX_COMP_BLOCK_SIZE", (fetch_word(asic, stream, 16) >> 24) & 0x3, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 68, ib_vmid, "MAX_UNCOMP_BLOCK_SIZE", (fetch_word(asic, stream, 16) >> 26) & 0x3, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 68, ib_vmid, "WRITE_COMPRESS_ENABLE", (fetch_word(asic, stream, 16) >> 28) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 68, ib_vmid, "META_TMZ", (fetch_word(asic, stream, 16) >> 29) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 68, ib_vmid, "PIPE_ALIGNED", (fetch_word(asic, stream, 16) >> 31) & 0x1, NULL, 10, 32);
					return;
				case 7: // DIRTY_PAGE
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "COPY (DIRTY_PAGE)", stream->header_dw, stream->words);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "TMZ", (stream->header_dw >> 18) & 0x1, NULL, 10, 32);
					if (sc->has_cpv_flag) {
						ui->add_field(ui, ib_addr + 0, ib_vmid, "CPV", (stream->header_dw >> 19) & 0x1, NULL, 10, 32);
					}
					ui->add_field(ui, ib_addr + 0, ib_vmid, "ALL", (stream->header_dw >> 31) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "COUNT", (fetch_word(asic, stream, 0) >> 0) & 0x3FFFFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_MTYPE", (fetch_word(asic, stream, 1) >> 3) & 0x7, NULL, 10, 32);
					if (sc->has_cpv_flag) {
						ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_L2_POLICY", (fetch_word(asic, stream, 1) >> 6) & 0x3, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_LLC", (fetch_word(asic, stream, 1) >> 8) & 0x1, NULL, 10, 32);
					} else if (sc->has_cp_fields) {
						ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_CACHE_POLICY", (fetch_word(asic, stream, 1) >> 5) & 0x3, NULL, 10, 32);
					}
					ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_MTYPE", (fetch_word(asic, stream, 1) >> 11) & 0x7, NULL, 10, 32);
					if (sc->has_cpv_flag) {
						ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_L2_POLICY", (fetch_word(asic, stream, 1) >> 14) & 0x3, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_LLC", (fetch_word(asic, stream, 1) >> 16) & 0x1, NULL, 10, 32);
					} else if (sc->has_cp_fields) {
						ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_CACHE_POLICY", (fetch_word(asic, stream, 1) >> 13) & 0x3, NULL, 10, 32);
					}
					ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_SW", (fetch_word(asic, stream, 1) >> 17) & 0x3, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_GCC", (fetch_word(asic, stream, 1) >> 19) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_SYS", (fetch_word(asic, stream, 1) >> 20) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_SNOOP", (fetch_word(asic, stream, 1) >> 22) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_GPA", (fetch_word(asic, stream, 1) >> 23) & 0x1, NULL, 10, 32);
					if (sc->has_cpv_flag) {
						ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_SW", (fetch_word(asic, stream, 1) >> 25) & 0x3, NULL, 10, 32);
					} else {
						ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_SW", (fetch_word(asic, stream, 1) >> 24) & 0x3, NULL, 10, 32);
					}
					ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_SYS", (fetch_word(asic, stream, 1) >> 28) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_SNOOP", (fetch_word(asic, stream, 1) >> 30) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_GPA", (fetch_word(asic, stream, 1) >> 31) & 0x1, NULL, 10, 32);

					ui->add_field(ui, ib_addr + 12, ib_vmid, "SRC_ADDR_LO", fetch_word(asic, stream, 2), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 16, ib_vmid, "SRC_ADDR_HI", fetch_word(asic, stream, 3), NULL, 16, 32);

					ui->add_field(ui, ib_addr + 20, ib_vmid, "DST_ADDR_LO", fetch_word(asic, stream, 4), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "DST_ADDR_HI", fetch_word(asic, stream, 5), NULL, 16, 32);
					return;
				case 8: // LINEAR_PHY
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "COPY (LINEAR_PHY)", stream->header_dw, stream->words);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "TMZ", (stream->header_dw >> 18) & 0x1, NULL, 10, 32);
					if (sc->has_cpv_flag) {
						ui->add_field(ui, ib_addr + 0, ib_vmid, "CPV", (stream->header_dw >> 19) & 0x1, NULL, 10, 32);
					}
					ui->add_field(ui, ib_addr + 4, ib_vmid, "COUNT", (fetch_word(asic, stream, 0) >> 0) & 0x3FFFFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_MTYPE", (fetch_word(asic, stream, 1) >> 3) & 0x7, NULL, 10, 32);
					if (sc->has_cpv_flag) {
						ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_L2_POLICY", (fetch_word(asic, stream, 1) >> 6) & 0x3, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_LLC", (fetch_word(asic, stream, 1) >> 8) & 0x1, NULL, 10, 32);
					} else if (sc->has_cp_fields) {
						ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_CACHE_POLICY", (fetch_word(asic, stream, 1) >> 5) & 0x3, NULL, 10, 32);
					}
					ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_MTYPE", (fetch_word(asic, stream, 1) >> 11) & 0x7, NULL, 10, 32);
					if (sc->has_cpv_flag) {
						ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_L2_POLICY", (fetch_word(asic, stream, 1) >> 14) & 0x3, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_LLC", (fetch_word(asic, stream, 1) >> 16) & 0x1, NULL, 10, 32);
					} else if (sc->has_cp_fields) {
						ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_CACHE_POLICY", (fetch_word(asic, stream, 1) >> 13) & 0x3, NULL, 10, 32);
					}
					ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_GCC", (fetch_word(asic, stream, 1) >> 19) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_SYS", (fetch_word(asic, stream, 1) >> 20) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_LOG", (fetch_word(asic, stream, 1) >> 21) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_SNOOP", (fetch_word(asic, stream, 1) >> 22) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_GPA", (fetch_word(asic, stream, 1) >> 23) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_GCC", (fetch_word(asic, stream, 1) >> 27) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_SYS", (fetch_word(asic, stream, 1) >> 28) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_SNOOP", (fetch_word(asic, stream, 1) >> 30) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_GPA", (fetch_word(asic, stream, 1) >> 31) & 0x1, NULL, 10, 32);
					for (n = 2; n + 3 < stream->nwords; n += 4) {
						uint32_t addr_idx = (n - 2) / 4;
						sprintf(str_buf, "SRC_ADDR%"PRIu32"_LO", addr_idx);
						ui->add_field(ui, ib_addr + 12 + 4 * (n - 2), ib_vmid, str_buf, fetch_word(asic, stream, n), NULL, 16, 32);
						sprintf(str_buf, "SRC_ADDR%"PRIu32"_HI", addr_idx);
						ui->add_field(ui, ib_addr + 16 + 4 * (n - 2), ib_vmid, str_buf, fetch_word(asic, stream, n + 1), NULL, 16, 32);
						sprintf(str_buf, "DST_ADDR%"PRIu32"_LO", addr_idx);
						ui->add_field(ui, ib_addr + 20 + 4 * (n - 2), ib_vmid, str_buf, fetch_word(asic, stream, n + 2), NULL, 16, 32);
						sprintf(str_buf, "DST_ADDR%"PRIu32"_HI", addr_idx);
						ui->add_field(ui, ib_addr + 24 + 4 * (n - 2), ib_vmid, str_buf, fetch_word(asic, stream, n + 3), NULL, 16, 32);
					}
					return;

				case 16: // LINEAR_BC
					break; // fall through
				case 17: // TILED_BC
					break; // fall through
				case 20: // LINEAR_SUB_WINDOW_BC
					break; // fall through
				case 21: // TILED_SUB_WINDOW_BC
					break; // fall through
				case 22: // T2T_SUB_WINDOW_BC
					break; // fall through
				case 36: // SUBWIN_LARGE
					break; // fall through
				default:
					if (ui->unhandled_subop)
						ui->unhandled_subop(ui, asic, ib_addr, ib_vmid, stream, UMR_RING_SDMA);
					return;
			}
			break;
		case 2: // WRITE
			switch (stream->sub_opcode) {
				case 0: // LINEAR
					break; // fall through
				case 1: // TILED
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "WRITE (TILED)", stream->header_dw, stream->words);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "ENCRYPT", (stream->header_dw >> 16) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "TMZ", (stream->header_dw >> 18) & 0x1, NULL, 10, 32);
					if (sc->has_cpv_flag) {
						ui->add_field(ui, ib_addr + 0, ib_vmid, "CPV", (stream->header_dw >> 28) & 0x1, NULL, 10, 32);
					}
					ui->add_field(ui, ib_addr + 4, ib_vmid, "DST_ADDR_LO", fetch_word(asic, stream, 0), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_ADDR_HI", fetch_word(asic, stream, 1), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "WIDTH", (fetch_word(asic, stream, 2) >> 16) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 16, ib_vmid, "HEIGHT", (fetch_word(asic, stream, 3) >> 0) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 16, ib_vmid, "DEPTH", (fetch_word(asic, stream, 3) >> 16) & sc->z_mask, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "ELEMENT_SIZE", (fetch_word(asic, stream, 4) >> 0) & 0x7, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "SWIZZLE_MODE", (fetch_word(asic, stream, 4) >> 3) & 0x1F, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "DIMENSION", (fetch_word(asic, stream, 4) >> 9) & 0x3, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "MIP_MAX", (fetch_word(asic, stream, 4) >> 16) & 0xF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "X", (fetch_word(asic, stream, 5) >> 0) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "Y", (fetch_word(asic, stream, 5) >> 16) & 0x3FFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 28, ib_vmid, "Z", (fetch_word(asic, stream, 6) >> 0) & sc->z_mask, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 28, ib_vmid, "SW", (fetch_word(asic, stream, 6) >> 24) & 0x3, NULL, 10, 32);
					if (sc->has_cp_fields) {
						ui->add_field(ui, ib_addr + 28, ib_vmid, "CACHE_POLICY", (fetch_word(asic, stream, 6) >> 26) & 0x7, NULL, 10, 32);
					}
					ui->add_field(ui, ib_addr + 32, ib_vmid, "COUNT", (fetch_word(asic, stream, 7) >> 0) & 0xFFFFF, NULL, 10, 32);
					for (n = 8; n < stream->nwords; n++) {
						uint32_t data_idx = n - 8;
						sprintf(str_buf, "DATA_%"PRIu32, data_idx);
						ui->add_field(ui, ib_addr + 36 + 4 * (n - 8), ib_vmid, str_buf, fetch_word(asic, stream, n), NULL, 16, 32);
					}
					return;
				case 17: // TILED_BC
					break; // fall through
				default:
					if (ui->unhandled_subop)
						ui->unhandled_subop(ui, asic, ib_addr, ib_vmid, stream, UMR_RING_SDMA);
					return;
			}
			break;
		case 4: // INDIRECT
			break; // fall through
		case 5: // FENCE
			ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "FENCE", stream->header_dw, stream->words);
			ui->add_field(ui, ib_addr + 0, ib_vmid, "MTYPE", (stream->header_dw >> 16) & 0x7, NULL, 10, 32);
			ui->add_field(ui, ib_addr + 0, ib_vmid, "GCC", (stream->header_dw >> 19) & 0x1, NULL, 10, 32);
			ui->add_field(ui, ib_addr + 0, ib_vmid, "SYS", (stream->header_dw >> 20) & 0x1, NULL, 10, 32);
			ui->add_field(ui, ib_addr + 0, ib_vmid, "SNP", (stream->header_dw >> 22) & 0x1, NULL, 10, 32);
			ui->add_field(ui, ib_addr + 0, ib_vmid, "GPA", (stream->header_dw >> 23) & 0x1, NULL, 10, 32);
			if (sc->has_cp_fields) {
				ui->add_field(ui, ib_addr + 0, ib_vmid, "L2_POLICY", (stream->header_dw >> 24) & 0x3, NULL, 10, 32);
				ui->add_field(ui, ib_addr + 0, ib_vmid, "LLC_POLICY", (stream->header_dw >> 26) & 0x1, NULL, 10, 32);
			}
			if (sc->has_cpv_flag) {
				ui->add_field(ui, ib_addr + 0, ib_vmid, "CPV", (stream->header_dw >> 28) & 0x1, NULL, 10, 32);
			}
			ui->add_field(ui, ib_addr + 4, ib_vmid, "FENCE_ADDR_LO", fetch_word(asic, stream, 0), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "FENCE_ADDR_HI", fetch_word(asic, stream, 1), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "FENCE_DATA", fetch_word(asic, stream, 2), NULL, 16, 32);
			return;
		case 6: // TRAP
			break; // fall through
		case 7: // SEM
			switch (stream->sub_opcode) {
				case 0: // SEM
					break; // fall through
				case 1: // MEM_INCR
					break; // fall through
				default:
					if (ui->unhandled_subop)
						ui->unhandled_subop(ui, asic, ib_addr, ib_vmid, stream, UMR_RING_SDMA);
					return;
			}

			break;
		case 8: // POLL_REGMEM
			switch (stream->sub_opcode) {
				case 0: // POLL_REGMEM
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "POLL_REGMEM", stream->header_dw, stream->words);
					if (sc->has_cp_fields) ui->add_field(ui, ib_addr + 0, ib_vmid, "CACHE_POLICY", BITS(stream->header_dw, 20, 23), NULL, 16, 32);
					if (sc->has_cpv_flag) ui->add_field(ui, ib_addr + 0, ib_vmid, "CPV", BITS(stream->header_dw, 24, 25), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "HDP_FLUSH", BITS(stream->header_dw, 26, 27), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "FUNCTION", 0, poll_regmem_funcs[BITS(stream->header_dw, 28, 31)], 0, 32);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "MEM_POLL", BITS(stream->header_dw, 31, 32), NULL, 16, 32);
					if (!(stream->header_dw & (1UL << 31))) {
						ui->add_field(ui, ib_addr + 4, ib_vmid, "REGISTER", BITS(fetch_word(asic, stream, 0), 2, 32) << 2, umr_reg_name(asic, BITS(fetch_word(asic, stream, 0), 2, 32)), 16, 32);
						if (((stream->header_dw >> 26) & 3) == 1) { // if HDP_FLUSH, the write register is provided
							ui->add_field(ui, ib_addr + 8, ib_vmid, "REGISTER", BITS(fetch_word(asic, stream, 1), 2, 32) << 2, umr_reg_name(asic, BITS(fetch_word(asic, stream, 1), 2, 32)), 16, 32);
						} else {
							ui->add_field(ui, ib_addr + 8, ib_vmid, NULL, fetch_word(asic, stream, 1), NULL, 16, 32);
						}
					} else {
						ui->add_field(ui, ib_addr + 4, ib_vmid, "POLL_REGMEM_ADDR_LO", fetch_word(asic, stream, 0), NULL, 16, 32);
						ui->add_field(ui, ib_addr + 8, ib_vmid, "POLL_REGMEM_ADDR_HI", fetch_word(asic, stream, 1), NULL, 16, 32);
					}
					ui->add_field(ui, ib_addr + 12, ib_vmid, "VALUE", fetch_word(asic, stream, 2), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 16, ib_vmid, "MASK", fetch_word(asic, stream, 3), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "INTERVAL", BITS(fetch_word(asic, stream, 4), 0, 16), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "RETRY_COUNT", BITS(fetch_word(asic, stream, 4), 16, 28), NULL, 10, 32);
					return;
				case 1: // POLL_REG_WRITE_MEM
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "POLL_REG_WRITE_MEM", stream->header_dw, stream->words);
					if (sc->has_cp_fields) ui->add_field(ui, ib_addr + 0, ib_vmid, "CACHE_POLICY", BITS(stream->header_dw, 24, 27), NULL, 16, 32);
					if (sc->has_cpv_flag) ui->add_field(ui, ib_addr + 0, ib_vmid, "CPV", BITS(stream->header_dw, 28, 29), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "SRC_ADDR", BITS(fetch_word(asic, stream, 0), 2, 32) << 2, umr_reg_name(asic, BITS(fetch_word(asic, stream, 0), 2, 32)), 16, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_ADDR_LO", fetch_word(asic, stream, 1), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "DST_ADDR_HI", fetch_word(asic, stream, 2), NULL, 16, 32);
					return;
				case 2: // POLL_DBIT_WRITE_MEM
					break; // fall through
				case 3: // MEM_VERIFY
					break; // fall through
				case 4: // INVALIDATION
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "INVALIDATION", stream->header_dw, stream->words);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "GFX_ENG_ID", (stream->header_dw >> 16) & 0x1F, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "MM_ENG_ID", (stream->header_dw >> 24) & 0x1F, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "INVALIDATEREQ", fetch_word(asic, stream, 0), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "ADDRESSRANGE_LO", fetch_word(asic, stream, 1), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "INVALIDATEACK", fetch_word(asic, stream, 2) & 0xFFFF, NULL, 16, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "ADDRESSRANGE_HI", (fetch_word(asic, stream, 2) >> 16) & 0x1F, NULL, 16, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "RESERVED", (fetch_word(asic, stream, 2) >> 23) & 0x1FF, NULL, 10, 32);
					return;
				default:
					if (ui->unhandled_subop)
						ui->unhandled_subop(ui, asic, ib_addr, ib_vmid, stream, UMR_RING_SDMA);
					return;
			}
			break;
		case 9:  // COND_EXE
			break; // fall through
		case 10:  // ATOMIC
			break; // fall through
		case 11: // CONST_FILL
			switch (stream->sub_opcode) {
				case 0: // CONST_FILL
			break; // fall through
				case 1: // CONST_FILL (DATA_FILL_MULTI)
			break; // fall through
				default:
					if (ui->unhandled_subop)
						ui->unhandled_subop(ui, asic, ib_addr, ib_vmid, stream, UMR_RING_SDMA);
					return;
			}
			break;
		case 12: // GEN_PTEPDE
			switch (stream->sub_opcode) {
				case 0: // GEN_PTEPDE
					break; // fall through
				case 1: // COPY
					break; // fall through
				case 2: // RMW
					break; // fall through
				default:
					if (ui->unhandled_subop)
						ui->unhandled_subop(ui, asic, ib_addr, ib_vmid, stream, UMR_RING_SDMA);
					return;
			}
			break;
		case 13: // TIMESTAMP
			switch (stream->sub_opcode) {
				case 0: // TIMESTAMP_SET
					break; // fall through
				case 1: // TIMESTAMP_GET
					break; // fall through
				case 2: // TIMESTAMP_GET_GLOBAL
					break; // fall through
				default:
					if (ui->unhandled_subop)
						ui->unhandled_subop(ui, asic, ib_addr, ib_vmid, stream, UMR_RING_SDMA);
					return;
			}
			break;
		case 14: // WRITE REG
			switch (stream->sub_opcode) {
				case 0: // SRBM_WRITE
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "SRBM_WRITE", stream->header_dw, stream->words);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "BYTE_ENABLE", (stream->header_dw >> 28), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "SRBM_WRITE_ADDR", BITS(fetch_word(asic, stream, 0), 0, 18),
						umr_reg_name(asic, (BITS(fetch_word(asic, stream, 0), 20, 32) << 18) | BITS(fetch_word(asic, stream, 0), 0, 18)), 16, 32);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "APERTURE_ID", BITS(fetch_word(asic, stream, 0), 20, 32), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "SRBM_WRITE_DATA", fetch_word(asic, stream, 1), NULL, 16, 32);
					return;
				case 1: // RMW_REGISTER
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "RMW_REGISTER", stream->header_dw, stream->words);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "ADDR", BITS(fetch_word(asic, stream, 0), 0, 18),
						umr_reg_name(asic, (BITS(fetch_word(asic, stream, 0), 20, 32) << 18) | BITS(fetch_word(asic, stream, 0), 0, 18)), 16, 32);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "APERTURE_ID", BITS(fetch_word(asic, stream, 0), 20, 32), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "MASK", fetch_word(asic, stream, 1), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "DATA", fetch_word(asic, stream, 2), NULL, 10, 32);
					return;
				default:
					if (ui->unhandled_subop)
						ui->unhandled_subop(ui, asic, ib_addr, ib_vmid, stream, UMR_RING_SDMA);
					return;
			}
			break;
	}

	// fall through to AI decoder
	decode_upto_ai(asic, ui, stream, ib_addr, ib_vmid, from_addr, from_vmid, follow, sc);
}

static void decode_upto_oss7(struct umr_asic *asic, struct umr_stream_decode_ui *ui, struct umr_sdma_stream *stream,
			   uint64_t ib_addr, uint32_t ib_vmid, uint64_t from_addr, uint64_t from_vmid, int follow, struct sdma_config *sc)
{
	uint32_t n, x;
	char str_buf[128];
	(void)from_addr;
	(void)from_vmid;
	switch (stream->opcode) {
		case 1: // COPY
			switch (stream->sub_opcode) {
				case 0: // LINEAR
					switch (stream->header_dw & (1UL << 27)) {
						case 0: // not broadcast
							ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "COPY (LINEAR)", stream->header_dw, stream->words);
							ui->add_field(ui, ib_addr + 0, ib_vmid, "TMZ", (stream->header_dw >> 18) & 0x1, NULL, 10, 32);
							ui->add_field(ui, ib_addr + 0, ib_vmid, "DCC", (stream->header_dw >> 19) & 0x1, NULL, 10, 32);
							ui->add_field(ui, ib_addr + 0, ib_vmid, "BROADCAST", (stream->header_dw >> 27) & 0x1, NULL, 10, 32);
							ui->add_field(ui, ib_addr + 4, ib_vmid, "COPY_COUNT", stream->words[0], NULL, 10, 32);
							ui->add_field(ui, ib_addr + 8, ib_vmid, "DST2_CACHE_POLICY", (stream->words[1] >> 12) & 0x7, NULL, 16, 32);
							ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_CACHE_POLICY", (stream->words[1] >> 20) & 0x7, NULL, 16, 32);
							ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_CACHE_POLICY", (stream->words[1] >> 28) & 0x7, NULL, 16, 32);
							ui->add_field(ui, ib_addr + 12, ib_vmid, "SRC_ADDR_LO", stream->words[2], NULL, 16, 32);
							ui->add_field(ui, ib_addr + 16, ib_vmid, "SRC_ADDR_HI", stream->words[3], NULL, 16, 32);
							ui->add_field(ui, ib_addr + 20, ib_vmid, "DST_ADDR_LO", stream->words[4], NULL, 16, 32);
							ui->add_field(ui, ib_addr + 24, ib_vmid, "DST_ADDR_HI", stream->words[5], NULL, 16, 32);
							if ((stream->header_dw >> 19) & 0x1) { // DCC = 1
								ui->add_field(ui, ib_addr + 28, ib_vmid, "DATA_FORMAT", BITS(stream->words[6], 0, 6), NULL, 10, 32);
								ui->add_field(ui, ib_addr + 28, ib_vmid, "NUM_TYPE", BITS(stream->words[6], 9, 12), NULL, 10, 32);
								ui->add_field(ui, ib_addr + 28, ib_vmid, "RD_CM", BITS(stream->words[6], 16, 18), NULL, 10, 32);
								ui->add_field(ui, ib_addr + 28, ib_vmid, "WR_CM", BITS(stream->words[6], 18, 20), NULL, 10, 32);
								ui->add_field(ui, ib_addr + 28, ib_vmid, "MAX_COM", BITS(stream->words[6], 24, 26), NULL, 10, 32);
								ui->add_field(ui, ib_addr + 28, ib_vmid, "MAX_UC", BITS(stream->words[6], 26, 27), NULL, 10, 32);
							}
							return;
						case 1: // broadcast
							ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "COPY (LINEAR BROADCAST)", stream->header_dw, stream->words);
							ui->add_field(ui, ib_addr + 0, ib_vmid, "TMZ", (stream->header_dw >> 18) & 0x1, NULL, 10, 32);
							ui->add_field(ui, ib_addr + 0, ib_vmid, "BROADCAST", (stream->header_dw >> 27) & 0x1, NULL, 10, 32);
							ui->add_field(ui, ib_addr + 4, ib_vmid, "COPY_COUNT", stream->words[0], NULL, 10, 32);
							ui->add_field(ui, ib_addr + 8, ib_vmid, "DST2_SW", (stream->words[1] >> 8) & 3, NULL, 10, 32);
							ui->add_field(ui, ib_addr + 8, ib_vmid, "DST2_CACHE_POLICY", (stream->words[1] >> 12) & 0x7, NULL, 16, 32);
							ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_CACHE_POLICY", (stream->words[1] >> 20) & 0x7, NULL, 16, 32);
							ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_CACHE_POLICY", (stream->words[1] >> 28) & 0x7, NULL, 16, 32);
							ui->add_field(ui, ib_addr + 12, ib_vmid, "SRC_ADDR_LO", stream->words[2], NULL, 16, 32);
							ui->add_field(ui, ib_addr + 16, ib_vmid, "SRC_ADDR_HI", stream->words[3], NULL, 16, 32);
							ui->add_field(ui, ib_addr + 20, ib_vmid, "DST_ADDR_LO", stream->words[4], NULL, 16, 32);
							ui->add_field(ui, ib_addr + 24, ib_vmid, "DST_ADDR_HI", stream->words[5], NULL, 16, 32);
							ui->add_field(ui, ib_addr + 28, ib_vmid, "DST2_ADDR_LO", stream->words[6], NULL, 16, 32);
							ui->add_field(ui, ib_addr + 32, ib_vmid, "DST2_ADDR_HI", stream->words[7], NULL, 16, 32);
							return;
						default:
							break;
					}
					break;
				case 1: // TILED
					if ((stream->header_dw >> 26) & 0x3) { // L2T BROADCAST or L2T FRAME TO FIELD
						if ((stream->header_dw >> 26) & 0x1) {
							ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "COPY (L2T_FRAME_TO_FIELD)", stream->header_dw, stream->words);
						} else {
							ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "COPY (L2T_BROADCAST)", stream->header_dw, stream->words);
						}
						ui->add_field(ui, ib_addr + 0, ib_vmid, "TMZ", (stream->header_dw >> 18) & 0x1, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 0, ib_vmid, "MIP MAX", BITS(stream->header_dw, 20, 25), NULL, 10, 32);
						ui->add_field(ui, ib_addr + 0, ib_vmid, "VIDEOCOPY", (stream->header_dw >> 26) & 0x1, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 0, ib_vmid, "BROADCAST", (stream->header_dw >> 27) & 0x1, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 4, ib_vmid, "TILED_ADDR0_LO", stream->words[0], NULL, 16, 32);
						ui->add_field(ui, ib_addr + 8, ib_vmid, "TILED_ADDR0_HI", stream->words[1], NULL, 16, 32);
						ui->add_field(ui, ib_addr + 12, ib_vmid, "TILED_ADDR1_LO", stream->words[2], NULL, 16, 32);
						ui->add_field(ui, ib_addr + 16, ib_vmid, "TILED_ADDR1_HI", stream->words[3], NULL, 16, 32);
						ui->add_field(ui, ib_addr + 20, ib_vmid, "WIDTH", BITS(stream->words[4], 0, 16), NULL, 10, 32);
						ui->add_field(ui, ib_addr + 24, ib_vmid, "HEIGHT", BITS(stream->words[5], 0, 16), NULL, 10, 32);
						ui->add_field(ui, ib_addr + 24, ib_vmid, "DEPTH", BITS(stream->words[5], 16, 30), NULL, 10, 32);
						ui->add_field(ui, ib_addr + 28, ib_vmid, "ELEMENT_SIZE", stream->words[6] & 0x7, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 28, ib_vmid, "SWIZZLE_MODE", BITS(stream->words[6], 3, 8), NULL, 10, 32);
						ui->add_field(ui, ib_addr + 28, ib_vmid, "DIMENSION", BITS(stream->words[6], 9, 11), NULL, 10, 32);
						ui->add_field(ui, ib_addr + 32, ib_vmid, "X", BITS(stream->words[7], 0, 16), NULL, 10, 32);
						ui->add_field(ui, ib_addr + 32, ib_vmid, "Y", BITS(stream->words[7], 16, 32), NULL, 10, 32);
						ui->add_field(ui, ib_addr + 36, ib_vmid, "Z", BITS(stream->words[8], 0, 14), NULL, 10, 32);
						ui->add_field(ui, ib_addr + 40, ib_vmid, "T1 CACHE POLICY", (stream->words[9] >> 12) & 0x7, NULL, 16, 32);
						ui->add_field(ui, ib_addr + 40, ib_vmid, "LINEAR_CACHE_POLICY", (stream->words[9] >> 20) & 0x7, NULL, 16, 32);
						ui->add_field(ui, ib_addr + 40, ib_vmid, "T0 CACHE POLICY", (stream->words[9] >> 28) & 0x7, NULL, 16, 32);

						ui->add_field(ui, ib_addr + 44, ib_vmid, "LINEAR_ADDR_LO", stream->words[10], NULL, 16, 32);
						ui->add_field(ui, ib_addr + 48, ib_vmid, "LINEAR_ADDR_HI", stream->words[11], NULL, 16, 32);
						ui->add_field(ui, ib_addr + 52, ib_vmid, "LINEAR_PITCH", BITS(stream->words[12], 0, 16), NULL, 10, 32);
						ui->add_field(ui, ib_addr + 56, ib_vmid, "LINEAR_SLICE_PITCH", stream->words[13], NULL, 10, 32);
						ui->add_field(ui, ib_addr + 60, ib_vmid, "COUNT", BITS(stream->words[14], 0, 30), NULL, 10, 32);
						return;
					} else {
						ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "COPY (TILED)", stream->header_dw, stream->words);
						ui->add_field(ui, ib_addr + 0, ib_vmid, "TMZ", (stream->header_dw >> 18) & 0x1, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 0, ib_vmid, "MIP MAX", BITS(stream->header_dw, 20, 25), NULL, 10, 32);
						ui->add_field(ui, ib_addr + 0, ib_vmid, "DETILE", (stream->header_dw >> 31) & 0x1, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 4, ib_vmid, "TILED_ADDR_LO", stream->words[0], NULL, 16, 32);
						ui->add_field(ui, ib_addr + 8, ib_vmid, "TILED_ADDR_HI", stream->words[1], NULL, 16, 32);
						ui->add_field(ui, ib_addr + 12, ib_vmid, "WIDTH", BITS(stream->words[2], 0, 16), NULL, 10, 32);
						ui->add_field(ui, ib_addr + 16, ib_vmid, "HEIGHT", BITS(stream->words[3], 0, 16), NULL, 10, 32);
						ui->add_field(ui, ib_addr + 16, ib_vmid, "DEPTH", BITS(stream->words[3], 16, 30), NULL, 10, 32);
						ui->add_field(ui, ib_addr + 20, ib_vmid, "ELEMENT_SIZE", BITS(stream->words[4], 0, 3), NULL, 10, 32);
						ui->add_field(ui, ib_addr + 20, ib_vmid, "SWIZZLE_MODE", BITS(stream->words[4], 3, 8), NULL, 10, 32);
						ui->add_field(ui, ib_addr + 20, ib_vmid, "DIMENSION", BITS(stream->words[4], 9, 11), NULL, 10, 32);
						ui->add_field(ui, ib_addr + 24, ib_vmid, "X", BITS(stream->words[5], 0, 16), NULL, 10, 32);
						ui->add_field(ui, ib_addr + 24, ib_vmid, "Y", BITS(stream->words[5], 16, 32), NULL, 10, 32);
						ui->add_field(ui, ib_addr + 28, ib_vmid, "Z", BITS(stream->words[6], 0, 14), NULL, 10, 32);
						ui->add_field(ui, ib_addr + 28, ib_vmid, "LINEAR CACHE POLICY", BITS(stream->words[6], 20, 23), NULL, 10, 32);
						ui->add_field(ui, ib_addr + 28, ib_vmid, "TILED CACHE POLICY", BITS(stream->words[6], 28, 31), NULL, 10, 32);
						ui->add_field(ui, ib_addr + 32, ib_vmid, "LINEAR_ADDR_LO", stream->words[7], NULL, 16, 32);
						ui->add_field(ui, ib_addr + 36, ib_vmid, "LINEAR_ADDR_HI", stream->words[8], NULL, 16, 32);
						ui->add_field(ui, ib_addr + 40, ib_vmid, "LINEAR_PITCH", BITS(stream->words[9], 0, 16), NULL, 10, 32);
						ui->add_field(ui, ib_addr + 44, ib_vmid, "LINEAR_SLICE_PITCH", stream->words[10], NULL, 10, 32);
						ui->add_field(ui, ib_addr + 48, ib_vmid, "COUNT", BITS(stream->words[11], 0, 30), NULL, 10, 32);
						return;
					}
				case 3: // SOA
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "COPY (STRUCT)", stream->header_dw, stream->words);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "TMZ", (stream->header_dw >> 18) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "DETILE", (stream->header_dw >> 31) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "SB_ADDR_LO", stream->words[0], NULL, 16, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "SB_ADDR_HI", stream->words[1], NULL, 16, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "START_INDEX", stream->words[2], NULL, 10, 32);
					ui->add_field(ui, ib_addr + 16, ib_vmid, "COUNT", stream->words[3], NULL, 10, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "STRIDE", BITS(stream->words[4], 0, 11), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "STRUCT CACHE POLICY", BITS(stream->words[4], 28, 31), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "LINEAR CACHE POLICY", BITS(stream->words[4], 20, 23), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "LINEAR_ADDR_LO", stream->words[5], NULL, 16, 32);
					ui->add_field(ui, ib_addr + 28, ib_vmid, "LINEAR_ADDR_HI", stream->words[6], NULL, 16, 32);
					return;
				case 4: // LINEAR_SUB_WINDOW
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "COPY (LINEAR_SUB_WINDOW)", stream->header_dw, stream->words);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "TMZ", (stream->header_dw >> 18) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "ELEMENTSIZE", BITS(stream->header_dw, 29, 32), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "SRC_ADDR_LO", stream->words[0], NULL, 16, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_ADDR_HI", stream->words[1], NULL, 16, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "SRC_X", BITS(stream->words[2], 0, 16), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "SRC_Y", BITS(stream->words[2], 16, 32), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 16, ib_vmid, "SRC_Z", BITS(stream->words[3], 0, 14), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 16, ib_vmid, "SRC_PITCH", BITS(stream->words[3], 16, 32), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "SRC_SLICE_PITCH", stream->words[4], NULL, 10, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "DST_ADDR_LO", stream->words[5], NULL, 16, 32);
					ui->add_field(ui, ib_addr + 28, ib_vmid, "DST_ADDR_HI", stream->words[6], NULL, 16, 32);
					ui->add_field(ui, ib_addr + 32, ib_vmid, "DST_X", BITS(stream->words[7], 0, 16), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 32, ib_vmid, "DST_Y", BITS(stream->words[7], 16, 32), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 36, ib_vmid, "DST_Z", BITS(stream->words[8], 0, 14), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 36, ib_vmid, "DST_PITCH", BITS(stream->words[8], 16, 32), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 40, ib_vmid, "DST_SLICE_PITCH", stream->words[9], NULL, 10, 32);
					ui->add_field(ui, ib_addr + 44, ib_vmid, "RECT_X", BITS(stream->words[10], 0, 16), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 44, ib_vmid, "RECT_Y", BITS(stream->words[10], 16, 32), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 48, ib_vmid, "RECT_Z", BITS(stream->words[11], 0, 14), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 48, ib_vmid, "DST_CACHE_POLICY", (stream->words[11] >> 20) & 0x7, NULL, 16, 32);
					ui->add_field(ui, ib_addr + 48, ib_vmid, "SRC_CACHE_POLICY", (stream->words[11] >> 28) & 0x7, NULL, 16, 32);
					return;
				case 5: // TILED_SUB_WINDOW
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "COPY (TILED_SUB_WINDOW)", stream->header_dw, stream->words);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "TMZ", (stream->header_dw >> 18) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "DCC", (stream->header_dw >> 19) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "DETILE", stream->header_dw >> 31, NULL, 16, 32);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "TILED_ADDR_LO", stream->words[0], NULL, 16, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "TILED_ADDR_HI", stream->words[1], NULL, 16, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "TILED_X", BITS(stream->words[2], 0, 16), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "TILED_Y", BITS(stream->words[2], 16, 32), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 16, ib_vmid, "TILED_Z", BITS(stream->words[3], 0, 14), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 16, ib_vmid, "WIDTH", BITS(stream->words[3], 16, 32), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "HEIGHT", BITS(stream->words[4], 0, 16), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "DEPTH", BITS(stream->words[4], 16, 30), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "ELEMENT_SIZE", BITS(stream->words[5], 0, 3), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "SWIZZLE_MODE", BITS(stream->words[5], 3, 8), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "DIMENSION", BITS(stream->words[5], 9, 11), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "MIP_MAX", BITS(stream->words[5], 16, 21), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "MIP_ID", BITS(stream->words[5], 24, 29), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 28, ib_vmid, "LINEAR_ADDR_LO", stream->words[6], NULL, 16, 32);
					ui->add_field(ui, ib_addr + 32, ib_vmid, "LINEAR_ADDR_HI", stream->words[7], NULL, 16, 32);
					ui->add_field(ui, ib_addr + 36, ib_vmid, "LINEAR_X", BITS(stream->words[8], 0, 16), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 36, ib_vmid, "LINEAR_Y", BITS(stream->words[8], 16, 32), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 40, ib_vmid, "LINEAR_Z", BITS(stream->words[9], 0, 14), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 40, ib_vmid, "LINEAR_PITCH", BITS(stream->words[9], 16, 32), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 44, ib_vmid, "LINEAR_SLICE_PITCH", stream->words[10], NULL, 10, 32);

					ui->add_field(ui, ib_addr + 48, ib_vmid, "RECT_X", BITS(stream->words[11], 0, 16), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 48, ib_vmid, "RECT_Y", BITS(stream->words[11], 16, 32), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 52, ib_vmid, "RECT_Z", BITS(stream->words[12], 0, 14), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 52, ib_vmid, "LINEAR_CACHE_POLICY", (stream->words[12] >> 20) & 0x7, NULL, 16, 32);
					ui->add_field(ui, ib_addr + 52, ib_vmid, "TILE_CACHE_POLICY", (stream->words[12] >> 28) & 0x7, NULL, 16, 32);

					if ((stream->header_dw >> 19) & 0x1) { // DCC = 1
						ui->add_field(ui, ib_addr + 56, ib_vmid, "DATA FORMAT", BITS(stream->words[13], 0, 6), NULL, 10, 32);
						ui->add_field(ui, ib_addr + 56, ib_vmid, "NUM TYPE", BITS(stream->words[13], 9, 12), NULL, 10, 32);
						ui->add_field(ui, ib_addr + 56, ib_vmid, "READ CM", BITS(stream->words[13], 16, 18), NULL, 10, 32);
						ui->add_field(ui, ib_addr + 56, ib_vmid, "WRITE CM", BITS(stream->words[13], 18, 20), NULL, 10, 32);
						ui->add_field(ui, ib_addr + 56, ib_vmid, "MAX COM", BITS(stream->words[13], 24, 26), NULL, 10, 32);
						ui->add_field(ui, ib_addr + 56, ib_vmid, "MAX UCOM", BITS(stream->words[13], 26, 27), NULL, 10, 32);
					}
					return;
				case 6: // T2T_SUB_WINDOW
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "COPY (T2T_SUB_WINDOW)", stream->header_dw, stream->words);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "TMZ", (stream->header_dw >> 18) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "DCC", (stream->header_dw >> 19) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "SRC_ADDR_LO", stream->words[0], NULL, 16, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_ADDR_HI", stream->words[1], NULL, 16, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "SRC_X", BITS(stream->words[2], 0, 16), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "SRC_Y", BITS(stream->words[2], 16, 32), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 16, ib_vmid, "SRC_Z", BITS(stream->words[3], 0, 14), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 16, ib_vmid, "SRC_WIDTH", BITS(stream->words[3], 16, 32), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "SRC_HEIGHT", BITS(stream->words[4], 0, 32), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "SRC_DEPTH", BITS(stream->words[4], 16, 30), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "SRC_ELEMENT_SIZE", BITS(stream->words[5], 0, 3), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "SRC_SWIZZLE_MODE", BITS(stream->words[5], 3, 8), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "SRC_DIMENSION", BITS(stream->words[5], 9, 11), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "SRC_MIP_MAX", BITS(stream->words[5], 16, 21), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "SRC_MIP_ID", BITS(stream->words[5], 24, 29), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 28, ib_vmid, "DST_ADDR_LO", stream->words[6], NULL, 16, 32);
					ui->add_field(ui, ib_addr + 32, ib_vmid, "DST_ADDR_HI", stream->words[7], NULL, 16, 32);
					ui->add_field(ui, ib_addr + 36, ib_vmid, "DST_X", BITS(stream->words[8], 0, 16), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 36, ib_vmid, "DST_Y", BITS(stream->words[8], 16, 32), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 40, ib_vmid, "DST_Z", BITS(stream->words[9], 0, 14), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 40, ib_vmid, "DST_WIDTH", BITS(stream->words[9], 16, 32), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 44, ib_vmid, "DST_HEIGHT", BITS(stream->words[10], 0, 16), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 44, ib_vmid, "DST_DEPTH", BITS(stream->words[10], 16, 30), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 48, ib_vmid, "DST_ELEMENT_SIZE", BITS(stream->words[11], 0, 3), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 48, ib_vmid, "DST_SWIZZLE_MODE", BITS(stream->words[11], 3, 8), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 48, ib_vmid, "DST_DIMENSION", BITS(stream->words[11], 9, 11), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 48, ib_vmid, "DST_MIP_MAX", BITS(stream->words[11], 16, 21), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 48, ib_vmid, "DST_MIP_ID", BITS(stream->words[11], 24, 29), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 52, ib_vmid, "RECT_X", BITS(stream->words[12], 0, 16), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 52, ib_vmid, "RECT_Y", BITS(stream->words[12], 16, 32), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 56, ib_vmid, "RECT_Z", BITS(stream->words[13], 0, 14), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 56, ib_vmid, "DST_CACHE_POLICY", (stream->words[13] >> 20) & 0x7, NULL, 16, 32);
					ui->add_field(ui, ib_addr + 56, ib_vmid, "SRC_CACHE_POLICY", (stream->words[13] >> 28) & 0x7, NULL, 16, 32);

					if ((stream->header_dw >> 19) & 0x1) { // DCC = 1
						ui->add_field(ui, ib_addr + 60, ib_vmid, "DATA FORMAT", BITS(stream->words[14], 0, 6), NULL, 10, 32);
						ui->add_field(ui, ib_addr + 60, ib_vmid, "NUM TYPE", BITS(stream->words[14], 9, 12), NULL, 10, 32);
						ui->add_field(ui, ib_addr + 60, ib_vmid, "READ CM", BITS(stream->words[14], 16, 18), NULL, 10, 32);
						ui->add_field(ui, ib_addr + 60, ib_vmid, "WRITE CM", BITS(stream->words[14], 18, 20), NULL, 10, 32);
						ui->add_field(ui, ib_addr + 60, ib_vmid, "MAX COM", BITS(stream->words[14], 24, 26), NULL, 10, 32);
						ui->add_field(ui, ib_addr + 60, ib_vmid, "MAX UCOM", BITS(stream->words[14], 26, 27), NULL, 10, 32);
					}
					return;
				case 8: // LINEAR_PHY
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "COPY (LINEAR_PHY)", stream->header_dw, stream->words);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "TMZ", (stream->header_dw >> 18) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "COUNT", BITS(stream->words[0], 0, 22), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "ADDR_PAIR", BITS(stream->words[0], 24, 32), NULL, 10, 32);

					ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_CACHE_POLICY", BITS(stream->words[1], 8, 11), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_CACHE_POLICY", BITS(stream->words[1], 16, 19), NULL, 16, 32);

					ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_GCC", (stream->words[1] >> 19) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_SYS", (stream->words[1] >> 20) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_LOG", (stream->words[1] >> 21) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_SNOOP", (stream->words[1] >> 22) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_GPA", (stream->words[1] >> 23) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_GCC", (stream->words[1] >> 27) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_SYS", (stream->words[1] >> 28) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_SNOOP", (stream->words[1] >> 30) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_GPA", (stream->words[1] >> 31) & 0x1, NULL, 10, 32);

					ui->add_field(ui, ib_addr + 12, ib_vmid, "DATA_FORMAT", BITS(stream->words[2], 0, 6), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "NUM_TYPE", BITS(stream->words[2], 9, 12), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "RD_CM", BITS(stream->words[2], 16, 18), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "WR_CM", BITS(stream->words[2], 18, 20), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "MAX_COM", BITS(stream->words[2], 24, 26), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "MAX_UC", BITS(stream->words[2], 26, 27), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "DCC", BITS(stream->words[2], 31, 32), NULL, 10, 32);

					for (n = 3; n + 3 < stream->nwords; n += 4) {
						uint32_t addr_idx = (n - 3) / 4;
						sprintf(str_buf, "SRC_ADDR%"PRIu32"_LO", addr_idx);
						ui->add_field(ui, ib_addr + 16 + 4 * (n - 3), ib_vmid, str_buf, stream->words[n], NULL, 16, 32);
						sprintf(str_buf, "SRC_ADDR%"PRIu32"_HI", addr_idx);
						ui->add_field(ui, ib_addr + 20 + 4 * (n - 3), ib_vmid, str_buf, stream->words[n + 1], NULL, 16, 32);
						sprintf(str_buf, "DST_ADDR%"PRIu32"_LO", addr_idx);
						ui->add_field(ui, ib_addr + 24 + 4 * (n - 3), ib_vmid, str_buf, stream->words[n + 2], NULL, 16, 32);
						sprintf(str_buf, "DST_ADDR%"PRIu32"_HI", addr_idx);
						ui->add_field(ui, ib_addr + 28 + 4 * (n - 3), ib_vmid, str_buf, stream->words[n + 3], NULL, 16, 32);
					}
					return;
				case 12: // PAGE TRANSFER COPY
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "PAGE TRANSFER COPY", stream->header_dw, stream->words);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "TMZ", (stream->header_dw >> 18) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "D", (stream->header_dw >> 31) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "PAGE_SIZE", BITS(stream->header_dw, 24, 28), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "PTE CACHE POLICY", BITS(stream->words[0], 0, 3), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "SYS", BITS(stream->words[0], 4, 5), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "PTE_SNP", BITS(stream->words[0], 6, 7), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "LOCAL CACHE POLICY", BITS(stream->words[0], 8, 11), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "LOCAL_SNP", BITS(stream->words[0], 14, 15), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "SYSMEM CACHE POLICY", BITS(stream->words[0], 16, 19), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "SYSMEM_SNP", BITS(stream->words[0], 22, 23), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "SYSMEM_ADDR_ARRAY_NUM", n = BITS(stream->words[0], 24, 32), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "DATA_FORMAT", BITS(stream->words[1], 0, 6), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "NUM_TYPE", BITS(stream->words[1], 9, 12), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "RD_CM", BITS(stream->words[1], 16, 18), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "WR_CM", BITS(stream->words[1], 18, 20), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "MAX_COM", BITS(stream->words[1], 24, 26), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "MAX_UC", BITS(stream->words[1], 26, 27), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "DCC", BITS(stream->words[1], 31, 32), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "PTE_MASK_LOW", BITS(stream->words[2], 0, 32), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 16, ib_vmid, "PTE_MASK_HI", BITS(stream->words[3], 0, 32), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "PTE_ADDR_LOW", BITS(stream->words[4], 3, 32) << 3, NULL, 16, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "PTE_ADDR_HI", BITS(stream->words[5], 0, 32), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 28, ib_vmid, "LOCALMEM_ADDR_LOW", BITS(stream->words[6], 0, 32), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 32, ib_vmid, "LOCALMEM_ADDR_HI", BITS(stream->words[7], 0, 32), NULL, 16, 32);
					++n;
					for (x = 0; x < n && (8 + x + x) < stream->nwords; x++) {
						ui->add_field(ui, ib_addr + 36 + (x + x + 0) * 4, ib_vmid, "SYSMEM_ADDR_LOW", stream->words[8 + x + x + 0], NULL, 16, 32);
						ui->add_field(ui, ib_addr + 36 + (x + x + 1) * 4, ib_vmid, "SYSMEM_ADDR_HIGH", stream->words[8 + x  + x + 1], NULL, 16, 32);
					}
					return;
				case 36: // SUBWIN_LARGE
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "COPY (SUBWIN_LARGE)", stream->header_dw, stream->words);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "TMZ", (stream->header_dw >> 18) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "SRC_ADDR_LO", stream->words[0], NULL, 16, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_ADDR_HI", stream->words[1], NULL, 16, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "SRC_X", stream->words[2], NULL, 10, 32);
					ui->add_field(ui, ib_addr + 16, ib_vmid, "SRC_Y", stream->words[3], NULL, 10, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "SRC_Z", stream->words[4], NULL, 10, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "SRC_PITCH", stream->words[5], NULL, 10, 32);
					ui->add_field(ui, ib_addr + 28, ib_vmid, "SRC_SLICE_PITCH_LO", stream->words[6], NULL, 10, 32);
					ui->add_field(ui, ib_addr + 32, ib_vmid, "SRC_SLICE_PITCH_47_32", stream->words[7] & 0xFFFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 36, ib_vmid, "DST_ADDR_LO", stream->words[8], NULL, 16, 32);
					ui->add_field(ui, ib_addr + 40, ib_vmid, "DST_ADDR_HI", stream->words[9], NULL, 16, 32);
					ui->add_field(ui, ib_addr + 44, ib_vmid, "DST_X", stream->words[10], NULL, 10, 32);
					ui->add_field(ui, ib_addr + 48, ib_vmid, "DST_Y", stream->words[11], NULL, 10, 32);
					ui->add_field(ui, ib_addr + 52, ib_vmid, "DST_Z", stream->words[12], NULL, 10, 32);
					ui->add_field(ui, ib_addr + 56, ib_vmid, "DST_PITCH", stream->words[13], NULL, 10, 32);
					ui->add_field(ui, ib_addr + 60, ib_vmid, "DST_SLICE_PITCH_LO", stream->words[14], NULL, 10, 32);
					ui->add_field(ui, ib_addr + 64, ib_vmid, "DST_SLICE_PITCH_47_32", stream->words[15] & 0xFFFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 64, ib_vmid, "DST_POLICY", (stream->words[15] >> 20) & 0x7, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 64, ib_vmid, "SRC_POLICY", (stream->words[15] >> 28) & 0x7, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 68, ib_vmid, "RECT_X", stream->words[16], NULL, 10, 32);
					ui->add_field(ui, ib_addr + 72, ib_vmid, "RECT_Y", stream->words[17], NULL, 10, 32);
					ui->add_field(ui, ib_addr + 76, ib_vmid, "RECT_Z", stream->words[18], NULL, 10, 32);
					return;
				default:
					if (ui->unhandled_subop)
						ui->unhandled_subop(ui, asic, ib_addr, ib_vmid, stream, UMR_RING_SDMA);
					return;
			}
			break;
		case 2: // WRITE
			switch (stream->sub_opcode) {
				case 0: // LINEAR
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "WRITE (LINEAR)", stream->header_dw, stream->words);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "TMZ", BITS(stream->header_dw, 18, 19), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "NOPTE_COMP", BITS(stream->header_dw, 16, 17), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "DST_ADDR_LO", stream->words[0], NULL, 16, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_ADDR_HI", stream->words[1], NULL, 16, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "COUNT", BITS(stream->words[2], 0, 20), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "CACHE_POLICY", BITS(stream->words[2], 28, 31), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "SYS", BITS(stream->words[2], 20, 21), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "SNP", BITS(stream->words[2], 22, 23), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "GPA", BITS(stream->words[2], 23, 24), NULL, 10, 32);
					for (n = 3; n < stream->nwords; n++) {
						uint32_t data_idx = n - 3;
						sprintf(str_buf, "DATA_%"PRIu32, data_idx);
						ui->add_field(ui, ib_addr + 16 + 4 * (n - 3), ib_vmid, str_buf, stream->words[n], NULL, 16, 32);
					}
					return;
				case 1: // TILED
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "WRITE (TILED)", stream->header_dw, stream->words);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "TMZ", (stream->header_dw >> 18) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "MIP MAX", BITS(stream->header_dw, 20, 25), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "DST_ADDR_LO", stream->words[0], NULL, 16, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_ADDR_HI", stream->words[1], NULL, 16, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "WIDTH", BITS(stream->words[2], 0, 16), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 16, ib_vmid, "HEIGHT", BITS(stream->words[3], 0, 16), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 16, ib_vmid, "DEPTH", BITS(stream->words[3], 16, 30), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "ELEMENT_SIZE", BITS(stream->words[4], 0, 3), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "SWIZZLE_MODE", BITS(stream->words[4], 3, 8), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "DIMENSION", BITS(stream->words[4], 9, 11), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "X", BITS(stream->words[5], 0, 16), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "Y", BITS(stream->words[5], 16, 32), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 28, ib_vmid, "Z", BITS(stream->words[6], 0, 14), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 28, ib_vmid, "CACHE_POLICY", BITS(stream->words[6], 28, 31), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 32, ib_vmid, "COUNT", BITS(stream->words[7], 0, 20), NULL, 10, 32);
					for (n = 8; n < stream->nwords; n++) {
						uint32_t data_idx = n - 8;
						sprintf(str_buf, "DATA_%"PRIu32, data_idx);
						ui->add_field(ui, ib_addr + 36 + 4 * (n - 8), ib_vmid, str_buf, stream->words[n], NULL, 16, 32);
					}
					return;
				default:
					if (ui->unhandled_subop)
						ui->unhandled_subop(ui, asic, ib_addr, ib_vmid, stream, UMR_RING_SDMA);
					return;
			}
			break;
		case 4: // INDIRECT
			break; // fall through
		case 5: // FENCE
			switch (stream->sub_opcode) {
				case 0: // FENCE
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "FENCE", stream->header_dw, stream->words);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "SYS", (stream->header_dw >> 20) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "SNP", (stream->header_dw >> 22) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "GPA", (stream->header_dw >> 23) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "CACHE POLICY", (stream->header_dw >> 26) & 0x7, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "FENCE_ADDR_LO", BITS(stream->words[0], 2, 32) << 2, NULL, 16, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "FENCE_ADDR_HI", stream->words[1], NULL, 16, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "FENCE_DATA", stream->words[2], NULL, 16, 32);
					return;
				case 1: // FENCE CONDITIONAL INTERRUPT
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "FENCE", stream->header_dw, stream->words);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "SYS", (stream->header_dw >> 20) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "SNP", (stream->header_dw >> 22) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "GPA", (stream->header_dw >> 23) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "CACHE POLICY", (stream->header_dw >> 26) & 0x7, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "QW", (stream->header_dw >> 31) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "FENCE_ADDR_LO", stream->words[0], NULL, 16, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "FENCE_ADDR_HI", stream->words[1], NULL, 16, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "FENCE_DATA_LO", stream->words[2], NULL, 16, 32);
					ui->add_field(ui, ib_addr + 16, ib_vmid, "FENCE_DATA_HI", stream->words[3], NULL, 16, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "FENCE_REF_ADDR_LO", stream->words[4], NULL, 16, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "FENCE_REF_ADDR_HI", stream->words[5], NULL, 16, 32);
					ui->add_field(ui, ib_addr + 28, ib_vmid, "INTERRUPT_CONTEXT", stream->words[6], NULL, 16, 32);
					return;
				default:
					if (ui->unhandled_subop)
						ui->unhandled_subop(ui, asic, ib_addr, ib_vmid, stream, UMR_RING_SDMA);
					return;
			}
			break;
		case 6: // TRAP
			ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "TRAP", stream->header_dw, stream->words);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "TRAP_INT_CONTEXT", stream->words[0], NULL, 16, 32);
			return;
		case 7: // SEM
			switch (stream->sub_opcode) {
				case 1: // MEM_INCR
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "SEM (MEM_INCR)", stream->header_dw, stream->words);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "CACHE POLICY", (stream->header_dw >> 26) & 0x7, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "ADDR_LO", BITS(stream->words[0], 3, 32), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "ADDR_HI", stream->words[1], NULL, 16, 32);
					return;
				default:
					if (ui->unhandled_subop)
						ui->unhandled_subop(ui, asic, ib_addr, ib_vmid, stream, UMR_RING_SDMA);
					return;
			}

			break;
		case 8: // POLL_REGMEM
			switch (stream->sub_opcode) {
				case 0: // POLL_REGMEM (DONE)
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "POLL_REGMEM", stream->header_dw, stream->words);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "CACHE_POLICY", BITS(stream->header_dw, 22, 25), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "MODE", BITS(stream->header_dw, 26, 28), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "FUNC", BITS(stream->header_dw, 28, 31), poll_regmem_funcs[BITS(stream->header_dw, 28, 31)], 10, 32);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "M", BITS(stream->header_dw, 31, 32), NULL, 16, 32);
					if (!(stream->header_dw & (1UL << 31))) {
						ui->add_field(ui, ib_addr + 4, ib_vmid, "REGISTER", BITS(stream->words[0], 2, 32) << 2, umr_reg_name(asic, BITS(stream->words[0], 2, 32)), 16, 32);
						if (((stream->header_dw >> 26) & 3) == 1) { // if HDP_FLUSH, the write register is provided
							ui->add_field(ui, ib_addr + 8, ib_vmid, "REGISTER", BITS(stream->words[1], 2, 32) << 2, umr_reg_name(asic, BITS(stream->words[1], 2, 32)), 16, 32);
						} else {
							ui->add_field(ui, ib_addr + 8, ib_vmid, NULL, stream->words[1], NULL, 16, 32);
						}
					} else {
						ui->add_field(ui, ib_addr + 4, ib_vmid, "POLL_REGMEM_ADDR_LO", stream->words[0], NULL, 16, 32);
						ui->add_field(ui, ib_addr + 8, ib_vmid, "POLL_REGMEM_ADDR_HI", stream->words[1], NULL, 16, 32);
					}
					ui->add_field(ui, ib_addr + 12, ib_vmid, "VALUE", stream->words[2], NULL, 16, 32);
					ui->add_field(ui, ib_addr + 16, ib_vmid, "MASK", stream->words[3], NULL, 16, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "INTERVAL", stream->words[4] & 0xFFFF, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "RETRY_COUNT", (stream->words[4] >> 16) & 0xFFF, NULL, 10, 32);
					return;
				case 1: // POLL_REG_WRITE_MEM (DONE)
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "POLL_REG_WRITE_MEM", stream->header_dw, stream->words);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "CACHE_POLICY", (stream->header_dw >> 22) & 0x7, NULL, 16, 32);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "SRC_REG_ADDR", BITS(stream->words[0], 2, 20) << 2, umr_reg_name(asic, BITS(stream->words[0], 2, 20)), 16, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_ADDR_LO", BITS(stream->words[1], 2, 32) << 2, NULL, 16, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "DST_ADDR_HI", stream->words[2], NULL, 16, 32);
					return;
				case 2: // POLL_DBIT_WRITE_MEM (DONE)
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "POLL_DBIT_WRITE_MEM", stream->header_dw, stream->words);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "CACHE_POLICY", (stream->header_dw >> 16) & 0x7, NULL, 16, 32);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "DP", (stream->header_dw >> 23) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "VFID", BITS(stream->header_dw, 24, 29), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "DST_ADDR_LO", BITS(stream->words[0], 5, 32) << 5, NULL, 16, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_ADDR_HI", stream->words[1], NULL, 16, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "START_PAGE", BITS(stream->words[2], 0, 31), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 16, ib_vmid, "PAGE_NUM", stream->words[3], NULL, 16, 32);
					return;
				case 4: // INVALIDATION
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "INVALIDATION", stream->header_dw, stream->words);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "GFX_ENG_ID", BITS(stream->header_dw, 16, 21), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "MM_ENG_ID", BITS(stream->header_dw, 24, 29), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "INVALIDATE_REQ", stream->words[0], NULL, 16, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "ADDRESSRANGE_LO", stream->words[1], NULL, 16, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "INVALIDATE_ACK", BITS(stream->words[2], 0, 16), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "ADDRESSRANGE_HI", BITS(stream->words[2], 16, 30), NULL, 16, 32);
					return;
				default:
					if (ui->unhandled_subop)
						ui->unhandled_subop(ui, asic, ib_addr, ib_vmid, stream, UMR_RING_SDMA);
					return;
			}
			break;
		case 9:  // COND_EXE
			ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "COND_EXE", stream->header_dw, stream->words);
			ui->add_field(ui, ib_addr + 0, ib_vmid, "CACHE_POLICY", (stream->header_dw >> 26) & 0x7, NULL, 16, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "ADDR_LO", BITS(stream->words[0], 2, 32) << 2, NULL, 16, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "ADDR_HI", stream->words[1], NULL, 16, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "REFERENCE", stream->words[2], NULL, 16, 32);
			ui->add_field(ui, ib_addr + 16, ib_vmid, "EXEC_COUNT", BITS(stream->words[3], 0, 14), NULL, 10, 32);
			return;
		case 10:  // ATOMIC
			ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "ATOMIC", stream->header_dw, stream->words);
			ui->add_field(ui, ib_addr + 0, ib_vmid, "LOOP", (stream->header_dw >> 16) & 1, NULL, 16, 32);
			ui->add_field(ui, ib_addr + 0, ib_vmid, "TMZ", (stream->header_dw >> 18) & 0x1, NULL, 10, 32);
			ui->add_field(ui, ib_addr + 0, ib_vmid, "CACHE_POLICY", (stream->header_dw >> 22) & 0x7, NULL, 16, 32);
			ui->add_field(ui, ib_addr + 0, ib_vmid, "ATOMIC_OP", (stream->header_dw >> 25) & 0x7F, NULL, 16, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "ADDR_LO", BITS(stream->words[0], 2, 32) << 2, NULL, 16, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "ADDR_HI", stream->words[1], NULL, 16, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "SRC_DATA_LO", stream->words[2], NULL, 16, 32);
			ui->add_field(ui, ib_addr + 16, ib_vmid, "SRC_DATA_HI", stream->words[3], NULL, 16, 32);
			ui->add_field(ui, ib_addr + 20, ib_vmid, "CMP_DATA_LO", stream->words[4], NULL, 16, 32);
			ui->add_field(ui, ib_addr + 24, ib_vmid, "CMP_DATA_HI", stream->words[5], NULL, 16, 32);
			ui->add_field(ui, ib_addr + 28, ib_vmid, "LOOP_INTERVAL", BITS(stream->words[6], 0, 13), NULL, 16, 32);
			return;
		case 11: // CONST_FILL
			switch (stream->sub_opcode) {
				case 0: // CONST_FILL
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "CONST_FILL", stream->header_dw, stream->words);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "NOPTE_COMP", BITS(stream->header_dw, 16, 17), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "SYS", BITS(stream->header_dw, 20, 21), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "SNP", BITS(stream->header_dw, 22, 23), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "GPA", BITS(stream->header_dw, 23, 24), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "CACHE_POLICY", BITS(stream->header_dw, 26, 29), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "FILL_SIZE", BITS(stream->header_dw, 30, 32), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "CONST_FILL_DST_LO", stream->words[0], NULL, 16, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "CONST_FILL_DST_HI", stream->words[1], NULL, 16, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "CONST_FILL_DATA", stream->words[2], NULL, 16, 32);
					ui->add_field(ui, ib_addr + 16, ib_vmid, "CONST_FILL_BYTE_COUNT", BITS(stream->words[3], 0, 30), NULL, 10, 32);
					return;
				case 1: // CONST_FILL (DATA_FILL_MULTI)
					break; // fall through 
				default:
					if (ui->unhandled_subop)
						ui->unhandled_subop(ui, asic, ib_addr, ib_vmid, stream, UMR_RING_SDMA);
					return;
			}
			break;
		case 12: // GEN_PTEPDE
			switch (stream->sub_opcode) {
				case 0: // GEN_PTEPDE
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "GEN_PTEPDE", stream->header_dw, stream->words);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "SYS", (stream->header_dw >> 20) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "SNP", (stream->header_dw >> 22) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "GPA", (stream->header_dw >> 23) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "CACHE_POLICY", (stream->header_dw >> 26) & 0x7, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "DST_ADDR_LO", BITS(stream->words[0], 3, 32) << 3, NULL, 16, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_ADDR_HI", stream->words[1], NULL, 16, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "MASK_LO", stream->words[2], NULL, 16, 32);
					ui->add_field(ui, ib_addr + 16, ib_vmid, "MASK_HI", stream->words[3], NULL, 16, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "INIT_LO", stream->words[4], NULL, 16, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "INIT_HI", stream->words[5], NULL, 16, 32);
					ui->add_field(ui, ib_addr + 28, ib_vmid, "INCR_LO", stream->words[6], NULL, 16, 32);
					ui->add_field(ui, ib_addr + 32, ib_vmid, "INCR_HI", stream->words[7], NULL, 16, 32);
					ui->add_field(ui, ib_addr + 36, ib_vmid, "numPTE", BITS(stream->words[8], 0, 19), NULL, 10, 32);
					return;
				case 1: // COPY
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "GEN_PTEPDE (COPY)", stream->header_dw, stream->words);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "PTEPDE_OP", (stream->header_dw >> 31) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "SRC_ADDR_LO", BITS(stream->words[0], 3, 32) << 3, NULL, 16, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_ADDR_HI", stream->words[1], NULL, 16, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "DST_ADDR_LO", BITS(stream->words[2], 3, 32) << 3, NULL, 16, 32);
					ui->add_field(ui, ib_addr + 16, ib_vmid, "DST_ADDR_HI", stream->words[3], NULL, 16, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "MASK_LO", stream->words[4], NULL, 16, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "MASK_HI", stream->words[5], NULL, 16, 32);
					ui->add_field(ui, ib_addr + 28, ib_vmid, "numPTE", BITS(stream->words[6], 0, 19), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 28, ib_vmid, "DST CACHE POLICY", BITS(stream->words[6], 24, 27), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 28, ib_vmid, "SRC CACHE POLICY", BITS(stream->words[6], 28, 31), NULL, 10, 32);
					return;
				case 2: // RMW
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "GEN_PTEPDE (RMW)", stream->header_dw, stream->words);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "SYS", (stream->header_dw >> 20) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "SNP", (stream->header_dw >> 22) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "GPA", (stream->header_dw >> 23) & 0x1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "CACHE_POLICY", (stream->header_dw >> 26) & 0x7, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "ADDR_LO", BITS(stream->words[0], 3, 32) << 3, NULL, 16, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "ADDR_HI", stream->words[1], NULL, 16, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "MASK_LO", stream->words[2], NULL, 16, 32);
					ui->add_field(ui, ib_addr + 16, ib_vmid, "MASK_HI", stream->words[3], NULL, 16, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "VALUE_LO", stream->words[4], NULL, 16, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "VALUE_HI", stream->words[5], NULL, 16, 32);
					ui->add_field(ui, ib_addr + 28, ib_vmid, "numPTE", BITS(stream->words[6], 0, 19), NULL, 10, 32);
					return;
				default:
					if (ui->unhandled_subop)
						ui->unhandled_subop(ui, asic, ib_addr, ib_vmid, stream, UMR_RING_SDMA);
					return;
			}
			break;
		case 13: // TIMESTAMP
			switch (stream->sub_opcode) {
				case 0: // TIMESTAMP_SET
					break; // fall through
				case 1: // TIMESTAMP_GET
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "TIMESTAMP (GET)", stream->header_dw, stream->words);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "CACHE POLICY", (stream->header_dw >> 26) & 0x7, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "WRITE_ADDR_LO", BITS(stream->words[0], 3, 32) << 3, NULL, 16, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "WRITE_ADDR_HI", stream->words[1], NULL, 16, 32);
					return;
				case 2: // TIMESTAMP_GET_GLOBAL
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "TIMESTAMP (GET_GLOBAL)", stream->header_dw, stream->words);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "CACHE POLICY", (stream->header_dw >> 26) & 0x7, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "WRITE_ADDR_LO", BITS(stream->words[0], 3, 32) << 3, NULL, 16, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "WRITE_ADDR_HI", stream->words[1], NULL, 16, 32);
					return;
				default:
					if (ui->unhandled_subop)
						ui->unhandled_subop(ui, asic, ib_addr, ib_vmid, stream, UMR_RING_SDMA);
					return;
			}
			break;
		case 14: // WRITE REG
			switch (stream->sub_opcode) {
				case 0: // REGISTER WRITE
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "SRBM_WRITE", stream->header_dw, stream->words);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "REG_WRITE_ADDR", BITS(stream->words[0], 2, 32) << 2, umr_reg_name(asic, BITS(stream->words[0], 2, 32)), 16, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "REG_WRITE_DATA", stream->words[1], NULL, 16, 32);
					if (BITS(stream->header_dw, 19, 20))
						ui->add_field(ui, ib_addr + 12, ib_vmid, "GRBM_GFX_INDEX_DATA", stream->words[2], NULL, 16, 32);
					return;
				case 1: // RMW_REGISTER
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "RMW_REGISTER", stream->header_dw, stream->words);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "ADDR", BITS(stream->words[0], 2, 32) << 2, umr_reg_name(asic, BITS(stream->words[0], 2, 32)), 16, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "MASK", stream->words[1], NULL, 10, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "DATA", stream->words[2], NULL, 10, 32);
					return;
				default:
					if (ui->unhandled_subop)
						ui->unhandled_subop(ui, asic, ib_addr, ib_vmid, stream, UMR_RING_SDMA);
					return;
			}
			break;
		case 15: // Pred Exe (same as VI)
			break; // fallthrough
		case 16: // GPUVM_INV (NV and beyond)
			ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "GPUVM_INV", stream->header_dw, stream->words);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "PER_VMID_INV_REQ", BITS(stream->words[0], 0, 16), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "FLUSH_TYPE", BITS(stream->words[0], 16, 19), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "INV_L2_PTES", BITS(stream->words[0], 19, 20), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "INV_L2_PDE0", BITS(stream->words[0], 20, 21), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "INV_L2_PDE1", BITS(stream->words[0], 21, 22), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "INV_L2_PDE2", BITS(stream->words[0], 22, 23), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "INV_L1_PTES", BITS(stream->words[0], 23, 24), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "CLEAR_PROTECTION_FAULT_STATUS_ADDR", BITS(stream->words[0], 24, 25), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "LOG_REQUEST", BITS(stream->words[0], 25, 26), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "4KB", BITS(stream->words[0], 26, 27), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "INV_L2_PDE3", BITS(stream->words[0], 27, 28), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "S_BIT", BITS(stream->words[1], 0, 1), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "PAGE_VM_ADDR_LO", BITS(stream->words[1], 1, 32) << 1, NULL, 16, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "PAGE_VM_ADDR_HI", BITS(stream->words[2], 0, 14), NULL, 16, 32);
			return;
		case 17: // GCR
			switch (stream->sub_opcode) {
				case 0: // GCR
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "GCR", stream->header_dw, stream->words);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "BASE_VA_LO", BITS(stream->words[0], 7, 32) << 7, NULL, 16, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "BASE_VA_HI", BITS(stream->words[1], 0, 16), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "GCR_CONTROL_LO", BITS(stream->words[1], 16, 32), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "GCR_CONTROL_HI", BITS(stream->words[2], 0, 3), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "LIMIT_VA_LO", BITS(stream->words[2], 7, 32) << 7, NULL, 16, 32);
					ui->add_field(ui, ib_addr + 16, ib_vmid, "LIMIT_VA_HI", BITS(stream->words[3], 0, 16), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 16, ib_vmid, "VMID", BITS(stream->words[3], 24, 28), NULL, 16, 32);
					return;
				case 1: // GCR user
					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "GCR USER", stream->header_dw, stream->words);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "BASE_VA_LO", BITS(stream->words[0], 7, 32) << 7, NULL, 16, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "BASE_VA_HI", BITS(stream->words[1], 0, 16), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "GCR_CONTROL_LO", BITS(stream->words[1], 16, 32), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "GCR_CONTROL_HI", BITS(stream->words[2], 0, 3), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "LIMIT_VA_LO", BITS(stream->words[2], 7, 32) << 7, NULL, 16, 32);
					ui->add_field(ui, ib_addr + 16, ib_vmid, "LIMIT_VA_HI", BITS(stream->words[3], 0, 16), NULL, 16, 32);
					return;
					return;
				default:
					if (ui->unhandled_subop)
						ui->unhandled_subop(ui, asic, ib_addr, ib_vmid, stream, UMR_RING_SDMA);
					return;
			}
			break;
	}

	// fall through to NV decoder
	decode_upto_nv(asic, ui, stream, ib_addr, ib_vmid, from_addr, from_vmid, follow, sc);
}

/**
 * umr_sdma_get_ip_ver - Get the version of the SDMA IP block
 *
 * @asic: The ASIC to query
 * @maj: Where to store the major version
 * @min: Where to store the minor version
 *
 * Returns -1 if no SDMA (or OSS) blocks are found.
 */
int umr_sdma_get_ip_ver(struct umr_asic *asic, int *maj, int *min)
{
	struct umr_ip_block *ip;

	// Grab OSS IP version from asic
	ip = umr_find_ip_block(asic, "oss", 0); // for multi instance
	if (!ip)
		ip = umr_find_ip_block(asic, "oss", -1); // for single instance
	if (!ip) {
		// try SDMA blocks
		ip = umr_find_ip_block(asic, "sdma", 0); // for multi instance
		if (!ip)
			ip = umr_find_ip_block(asic, "sdma", -1); // for single instance
	}

	if (!ip) {
		// try by GC version
		ip = umr_find_ip_block(asic, "gfx", 0); // for multi instance
		if (!ip)
			ip = umr_find_ip_block(asic, "gfx", -1); // for single instance


		if (ip) {
			switch (ip->discoverable.maj) {
				case 6:
					*maj = 1;
					break;
				case 7:
					*maj = 2;
					break;
				case 8:
					*maj = 3;
					break;
				case 9:
					*maj = 4;
					break;
				case 10:
					*maj = 5;
					break;
				case 11:
					*maj = 6;
					break;
				case 12:
					*maj = 7;
					break;
			}
			*min = 0;
			return 0;
		} else {
			return -1;
		}

	} else {
		*maj = ip->discoverable.maj;
		*min = ip->discoverable.min;
		return 0;
	}
}

/**
 * umr_sdma_decode_stream_opcodes - decode a stream of SDMA packets
 *
 * @asic: The ASIC the SDMA packets are bound for
 * @ui: The user interface callback that will present the decoded packets to the user
 * @stream: The pre-processed stream of SDMA packets
 * @ib_addr: The base VM address where the packets came from
 * @ib_vmid: The VMID the IB is mapped into
 * @from_addr: The address of the ring/IB that pointed to this SDMA IB
 * @from_vmid: The VMID of the ring/IB that pointed to this SDMA IB
 * @opcodes: The number of opcodes to decode
 * @follow: Follow any chained IBs
 *
 * Returns the address of the first packet that hasn't been decoded.
 */
struct umr_sdma_stream *umr_sdma_decode_stream_opcodes(struct umr_asic *asic, struct umr_stream_decode_ui *ui, struct umr_sdma_stream *stream,
						       uint64_t ib_addr, uint32_t ib_vmid, uint64_t from_addr, uint64_t from_vmid, unsigned long opcodes, int follow)
{
	uint32_t n;
	struct umr_sdma_stream *os = stream;
	struct sdma_config sc = { 0 };

	if (umr_sdma_get_ip_ver(asic, &sc.ver_maj, &sc.ver_min)) {
		asic->err_msg("[BUG] Cannot determine version of OSS block for this ASIC.\n");
		return NULL;
	}

	sc.z_mask = sc.ver_maj >= VER_NV ? 0x1FFF : 0x7FF;
	sc.pitch_mask = sc.ver_maj >= VER_AI ? 0x7FFFF : 0x3FFF;
	sc.pitch_shift = sc.ver_maj >= VER_AI ? 13 : 16;

	sc.has_cpv_flag = (sc.ver_maj > VER_NV) || (sc.ver_maj == VER_NV && sc.ver_min >= 2);
	sc.has_cp_fields = sc.has_cpv_flag || (sc.ver_maj == VER_AI && sc.ver_min >= 4);

	n = 0;
	while (os) {
		n += os->nwords;
		os = os->next;
	}

	ui->start_ib(ui, ib_addr, ib_vmid, from_addr, from_vmid, n, 0);
	while (stream && opcodes--) {
		switch (sc.ver_maj) {
			case 1:
			case 2:
			case 3:
				decode_upto_vi(asic, ui, stream, ib_addr, ib_vmid, from_addr, from_vmid, follow, &sc);
				break;
			case 4:
				decode_upto_ai(asic, ui, stream, ib_addr, ib_vmid, from_addr, from_vmid, follow, &sc);
				break;
			case 5:
			case 6:
				decode_upto_nv(asic, ui, stream, ib_addr, ib_vmid, from_addr, from_vmid, follow, &sc);
				break;
			case 7:
				decode_upto_oss7(asic, ui, stream, ib_addr, ib_vmid, from_addr, from_vmid, follow, &sc);
				break;
			default:
				asic->err_msg("[BUG]: Invalid OSS major version (%d) in SDMA decode\n", sc.ver_maj);
				return NULL;
		}

		if (stream->invalid)
			break;

		ib_addr += (1 + stream->nwords) * 4;
		stream = stream->next;
	}
	ui->done(ui);
	return stream;
}

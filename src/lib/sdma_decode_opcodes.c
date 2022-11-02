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
#include <inttypes.h>

#define BITS(x, a, b) (unsigned long)((x >> (a)) & ((1ULL << ((b)-(a)))-1))

struct umr_sdma_stream *umr_sdma_decode_stream_opcodes(struct umr_asic *asic, struct umr_sdma_stream_decode_ui *ui, struct umr_sdma_stream *stream,
						       uint64_t ib_addr, uint32_t ib_vmid, uint64_t from_addr, uint64_t from_vmid, unsigned long opcodes, int follow)
{
	uint32_t n;
	int i;
	char str_buf[64];
	struct umr_sdma_stream *os = stream;
	static char *poll_regmem_funcs[] = { "always", "<", "<=", "==", "!=", ">=", ">", "N/A" };
	const int ver_vi = 3;
	const int ver_ai = 4;
	const int ver_nv = 5;
	int ver_maj = 0;
	int ver_min = 0;
	int has_cpv_flag = 0;
	int has_cp_fields = 0;
	uint32_t z_mask;
	uint32_t pitch_mask;
	uint32_t pitch_shift;

	// Grab OSS IP version from asic
	for (i = 0; i < asic->no_blocks; i++) {
		if (strncmp(asic->blocks[i]->ipname, "oss", 3) == 0) {
			ver_maj = asic->blocks[i]->discoverable.maj;
			ver_min = asic->blocks[i]->discoverable.min;
		}
	}

	// If version not found for OSS block, fallback to setting ip version based on asic family
	if (!ver_maj) {
		if (asic->family == FAMILY_SI) {
			ver_maj = 1;
		} else if (asic->family == FAMILY_CIK) {
			ver_maj = 2;
		} else if (asic->family == FAMILY_VI) {
			ver_maj = 3;
		} else if (asic->family == FAMILY_AI) {
			ver_maj = 4;
		} else if (asic->family >= FAMILY_NV) {
			ver_maj = 5;
		}
	}

	z_mask = ver_maj >= ver_nv ? 0x1FFF : 0x7FF;
	pitch_mask = ver_maj >= ver_ai ? 0x7FFFF : 0x3FFF;
	pitch_shift = ver_maj >= ver_ai ? 13 : 16;

	has_cpv_flag = (ver_maj > ver_nv) || (ver_maj == ver_nv && ver_min >= 2);
	has_cp_fields = has_cpv_flag || (ver_maj == ver_ai && ver_min >= 4);

	n = 0;
	while (os) {
		n += os->nwords;
		os = os->next;
	}

	ui->start_ib(ui, ib_addr, ib_vmid, from_addr, from_vmid, n);
	while (stream && opcodes--) {
		switch (stream->opcode) {
			case 0:
				ui->start_opcode(ui, ib_addr, ib_vmid, stream->opcode, stream->sub_opcode, stream->nwords + 1, "NOP", stream->header_dw, stream->words);
				break;
			case 1: // COPY
				switch (stream->sub_opcode) {
					case 0: // LINEAR
						switch (stream->header_dw & (1UL << 27)) {
							case 0: // not broadcast
								ui->start_opcode(ui, ib_addr, ib_vmid, stream->opcode, stream->sub_opcode, stream->nwords + 1, "COPY (LINEAR)", stream->header_dw, stream->words);
								if (ver_maj >= ver_ai) {
									ui->add_field(ui, ib_addr + 0, ib_vmid, "ENCRYPT", (stream->header_dw >> 16) & 0x1, NULL, 10);
									ui->add_field(ui, ib_addr + 0, ib_vmid, "TMZ", (stream->header_dw >> 18) & 0x1, NULL, 10);
								}
								if (has_cpv_flag) {
									ui->add_field(ui, ib_addr + 0, ib_vmid, "CPV", (stream->header_dw >> 19) & 0x1, NULL, 10);
								}
								ui->add_field(ui, ib_addr + 0, ib_vmid, "BACKWARDS", (stream->header_dw >> 25) & 0x1, NULL, 10);
								ui->add_field(ui, ib_addr + 0, ib_vmid, "BROADCAST", (stream->header_dw >> 27) & 0x1, NULL, 10);
								ui->add_field(ui, ib_addr + 4, ib_vmid, "COPY_COUNT", stream->words[0], NULL, 10);
								ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_SW", (stream->words[1] >> 16) & 3, NULL, 10);
								if (has_cp_fields) {
									ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_CACHE_POLICY", (stream->words[1] >> 18) & 0x7, NULL, 10);
								} else if (ver_maj <= ver_vi) {
									ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_HA", (stream->words[1] >> 22) & 0x1, NULL, 10);
								}
								ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_SW", (stream->words[1] >> 24) & 3, NULL, 10);
								if (has_cp_fields) {
									ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_CACHE_POLICY", (stream->words[1] >> 26) & 0x7, NULL, 10);
								} else if (ver_maj <= ver_vi) {
									ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_HA", (stream->words[1] >> 30) & 0x1, NULL, 10);
								}
								ui->add_field(ui, ib_addr + 12, ib_vmid, "SRC_ADDR_LO", stream->words[2], NULL, 16);
								ui->add_field(ui, ib_addr + 16, ib_vmid, "SRC_ADDR_HI", stream->words[3], NULL, 16);
								ui->add_field(ui, ib_addr + 20, ib_vmid, "DST_ADDR_LO", stream->words[4], NULL, 16);
								ui->add_field(ui, ib_addr + 24, ib_vmid, "DST_ADDR_HI", stream->words[5], NULL, 16);
								break;
							case 1: // broadcast
								ui->start_opcode(ui, ib_addr, ib_vmid, stream->opcode, stream->sub_opcode, stream->nwords + 1, "COPY (LINEAR BROADCAST)", stream->header_dw, stream->words);
								ui->add_field(ui, ib_addr + 0, ib_vmid, "ENCRYPT", (stream->header_dw >> 16) & 0x1, NULL, 10);
								ui->add_field(ui, ib_addr + 0, ib_vmid, "TMZ", (stream->header_dw >> 18) & 0x1, NULL, 10);
								if (has_cpv_flag) {
									ui->add_field(ui, ib_addr + 0, ib_vmid, "CPV", (stream->header_dw >> 19) & 0x1, NULL, 10);
								}
								ui->add_field(ui, ib_addr + 0, ib_vmid, "BROADCAST", (stream->header_dw >> 27) & 0x1, NULL, 10);
								ui->add_field(ui, ib_addr + 4, ib_vmid, "COPY_COUNT", stream->words[0], NULL, 10);
								ui->add_field(ui, ib_addr + 8, ib_vmid, "DST2_SW", (stream->words[1] >> 8) & 3, NULL, 10);
								if (has_cp_fields) {
									ui->add_field(ui, ib_addr + 8, ib_vmid, "DST2_CACHE_POLICY", (stream->words[1] >> 10) & 0x7, NULL, 10);
								}
								ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_SW", (stream->words[1] >> 16) & 3, NULL, 10);
								if (has_cp_fields) {
									ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_CACHE_POLICY", (stream->words[1] >> 18) & 0x7, NULL, 10);
								}
								ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_SW", (stream->words[1] >> 24) & 3, NULL, 10);
								if (has_cp_fields) {
									ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_CACHE_POLICY", (stream->words[1] >> 26) & 0x7, NULL, 10);
								}
								ui->add_field(ui, ib_addr + 12, ib_vmid, "SRC_ADDR_LO", stream->words[2], NULL, 16);
								ui->add_field(ui, ib_addr + 16, ib_vmid, "SRC_ADDR_HI", stream->words[3], NULL, 16);
								ui->add_field(ui, ib_addr + 20, ib_vmid, "DST_ADDR_LO", stream->words[4], NULL, 16);
								ui->add_field(ui, ib_addr + 24, ib_vmid, "DST_ADDR_HI", stream->words[5], NULL, 16);
								ui->add_field(ui, ib_addr + 28, ib_vmid, "DST2_ADDR_LO", stream->words[6], NULL, 16);
								ui->add_field(ui, ib_addr + 32, ib_vmid, "DST2_ADDR_HI", stream->words[7], NULL, 16);
								break;
							default:
								break;
						}
						break;
					case 1: // TILED
						if ((stream->header_dw >> 26) & 0x3) { // L2T BROADCAST or L2T FRAME TO FIELD
							if ((stream->header_dw >> 26) & 0x1) {
								ui->start_opcode(ui, ib_addr, ib_vmid, stream->opcode, stream->sub_opcode, stream->nwords + 1, "COPY (L2T_FRAME_TO_FIELD)", stream->header_dw, stream->words);
							} else {
								ui->start_opcode(ui, ib_addr, ib_vmid, stream->opcode, stream->sub_opcode, stream->nwords + 1, "COPY (L2T_BROADCAST)", stream->header_dw, stream->words);
							}
							ui->add_field(ui, ib_addr + 0, ib_vmid, "ENCRYPT", (stream->header_dw >> 16) & 0x1, NULL, 10);
							ui->add_field(ui, ib_addr + 0, ib_vmid, "TMZ", (stream->header_dw >> 18) & 0x1, NULL, 10);
							if (has_cpv_flag) {
								ui->add_field(ui, ib_addr + 0, ib_vmid, "CPV", (stream->header_dw >> 19) & 0x1, NULL, 10);
							}
							if (ver_maj == ver_ai) {
								ui->add_field(ui, ib_addr + 0, ib_vmid, "MIP_MAX", (stream->header_dw >> 20) & 0xF, NULL, 10);
							}
							ui->add_field(ui, ib_addr + 0, ib_vmid, "VIDEOCOPY", (stream->header_dw >> 26) & 0x1, NULL, 10);
							ui->add_field(ui, ib_addr + 0, ib_vmid, "BROADCAST", (stream->header_dw >> 27) & 0x1, NULL, 10);
							ui->add_field(ui, ib_addr + 4, ib_vmid, "TILED_ADDR0_LO", stream->words[0], NULL, 16);
							ui->add_field(ui, ib_addr + 8, ib_vmid, "TILED_ADDR0_HI", stream->words[1], NULL, 16);
							ui->add_field(ui, ib_addr + 12, ib_vmid, "TILED_ADDR1_LO", stream->words[2], NULL, 16);
							ui->add_field(ui, ib_addr + 16, ib_vmid, "TILED_ADDR1_HI", stream->words[3], NULL, 16);
							if (ver_maj <= ver_vi) {
								ui->add_field(ui, ib_addr + 20, ib_vmid, "PITCH_IN_TILE", stream->words[4] & 0x7FF, NULL, 10);
								ui->add_field(ui, ib_addr + 20, ib_vmid, "HEIGHT", (stream->words[4] >> 16) & 0x3FFF, NULL, 10);
								ui->add_field(ui, ib_addr + 24, ib_vmid, "SLICE_PITCH", stream->words[5] & 0x3FFFFF, NULL, 10);
								ui->add_field(ui, ib_addr + 28, ib_vmid, "ELEMENT_SIZE", stream->words[6] & 0x7, NULL, 10);
								ui->add_field(ui, ib_addr + 28, ib_vmid, "ARRAY_MODE", (stream->words[6] >> 3) & 0xF, NULL, 10);
								ui->add_field(ui, ib_addr + 28, ib_vmid, "MIT_MODE", (stream->words[6] >> 8) & 0x7, NULL, 10);
								ui->add_field(ui, ib_addr + 28, ib_vmid, "TILESPLIT_SIZE", (stream->words[6] >> 11) & 0x7, NULL, 10);
								ui->add_field(ui, ib_addr + 28, ib_vmid, "BANK_W", (stream->words[6] >> 15) & 0x3, NULL, 10);
								ui->add_field(ui, ib_addr + 28, ib_vmid, "BANK_H", (stream->words[6] >> 18) & 0x3, NULL, 10);
								ui->add_field(ui, ib_addr + 28, ib_vmid, "NUM_BANK", (stream->words[6] >> 21) & 0x3, NULL, 10);
								ui->add_field(ui, ib_addr + 28, ib_vmid, "MAT_ASPT", (stream->words[6] >> 24) & 0x3, NULL, 10);
								ui->add_field(ui, ib_addr + 28, ib_vmid, "PIPE_CONFIG", (stream->words[6] >> 26) & 0x1F, NULL, 10);
							} else {
								ui->add_field(ui, ib_addr + 20, ib_vmid, "WIDTH", stream->words[4] & 0x3FFF, NULL, 10);
								ui->add_field(ui, ib_addr + 24, ib_vmid, "HEIGHT", stream->words[5] & 0x3FFF, NULL, 10);
								ui->add_field(ui, ib_addr + 24, ib_vmid, "DEPTH", (stream->words[5] >> 16) & 0x1FFF, NULL, 10);
								ui->add_field(ui, ib_addr + 28, ib_vmid, "ELEMENT_SIZE", stream->words[6] & 0x7, NULL, 10);
								ui->add_field(ui, ib_addr + 28, ib_vmid, "SWIZZLE_MODE", (stream->words[6] >> 3) & 0x1F, NULL, 10);
								ui->add_field(ui, ib_addr + 28, ib_vmid, "DIMENSION", (stream->words[6] >> 9) & 0x3, NULL, 10);
								if (ver_maj == ver_ai) {
									ui->add_field(ui, ib_addr + 28, ib_vmid, "EPITCH", (stream->words[6] >> 16) & 0xFFFF, NULL, 10);
								} else {
									ui->add_field(ui, ib_addr + 28, ib_vmid, "MIP_MAX", (stream->words[6] >> 16) & 0xF, NULL, 10);
								}
							}
							ui->add_field(ui, ib_addr + 32, ib_vmid, "X", stream->words[7] & 0x3FFF, NULL, 10);
							ui->add_field(ui, ib_addr + 32, ib_vmid, "Y", (stream->words[7] >> 16) & 0x3FFF, NULL, 10);
							ui->add_field(ui, ib_addr + 36, ib_vmid, "Z", stream->words[8] & 0x7FF, NULL, 10);
							ui->add_field(ui, ib_addr + 40, ib_vmid, "DST2_SW", (stream->words[9] >> 8) & 0x3, NULL, 10);
							if (has_cp_fields) {
								ui->add_field(ui, ib_addr + 40, ib_vmid, "DST2_CACHE_POLICY", (stream->words[9] >> 10) & 0x7, NULL, 10);
							}
							ui->add_field(ui, ib_addr + 40, ib_vmid, "LINEAR_SW", (stream->words[9] >> 16) & 0x3, NULL, 10);
							if (has_cp_fields) {
								ui->add_field(ui, ib_addr + 40, ib_vmid, "LINEAR_CACHE_POLICY", (stream->words[9] >> 18) & 0x7, NULL, 10);
							}
							ui->add_field(ui, ib_addr + 40, ib_vmid, "TILE_SW", (stream->words[9] >> 24) & 0x3, NULL, 10);
							if (has_cp_fields) {
								ui->add_field(ui, ib_addr + 40, ib_vmid, "TILE_CACHE_POLICY", (stream->words[9] >> 26) & 0x7, NULL, 10);
							}

							ui->add_field(ui, ib_addr + 44, ib_vmid, "LINEAR_ADDR_LO", stream->words[10], NULL, 16);
							ui->add_field(ui, ib_addr + 48, ib_vmid, "LINEAR_ADDR_HI", stream->words[11], NULL, 16);
							ui->add_field(ui, ib_addr + 52, ib_vmid, "LINEAR_PITCH", stream->words[12] & 0x7FFFF, NULL, 10);
							if (ver_maj >= ver_ai) {
								ui->add_field(ui, ib_addr + 56, ib_vmid, "LINEAR_SLICE_PITCH", stream->words[13], NULL, 10);
								ui->add_field(ui, ib_addr + 60, ib_vmid, "COUNT", stream->words[14] & 0x3FFFFF, NULL, 10);
							} else {
								ui->add_field(ui, ib_addr + 56, ib_vmid, "COUNT", stream->words[13] & 0xFFFFF, NULL, 10);
							}
							break;
						} else {
							ui->start_opcode(ui, ib_addr, ib_vmid, stream->opcode, stream->sub_opcode, stream->nwords + 1, "COPY (TILED)", stream->header_dw, stream->words);
							ui->add_field(ui, ib_addr + 0, ib_vmid, "ENCRYPT", (stream->header_dw >> 16) & 0x1, NULL, 10);
							ui->add_field(ui, ib_addr + 0, ib_vmid, "TMZ", (stream->header_dw >> 18) & 0x1, NULL, 10);
							if (ver_maj == ver_ai) {
								ui->add_field(ui, ib_addr + 0, ib_vmid, "MIP_MAX", (stream->header_dw >> 20) & 0xF, NULL, 10);
							}
							ui->add_field(ui, ib_addr + 0, ib_vmid, "VIDEOCOPY", (stream->header_dw >> 26) & 0x1, NULL, 10);
							ui->add_field(ui, ib_addr + 0, ib_vmid, "BROADCAST", (stream->header_dw >> 27) & 0x1, NULL, 10);
							ui->add_field(ui, ib_addr + 0, ib_vmid, "DETILE", (stream->header_dw >> 31) & 0x1, NULL, 10);
							ui->add_field(ui, ib_addr + 4, ib_vmid, "TILED_ADDR_LO", stream->words[0], NULL, 16);
							ui->add_field(ui, ib_addr + 8, ib_vmid, "TILED_ADDR_HI", stream->words[1], NULL, 16);
							if (ver_maj <= ver_vi) {
								ui->add_field(ui, ib_addr + 12, ib_vmid, "PITCH_IN_TILE", stream->words[2] & 0x7FF, NULL, 10);
								ui->add_field(ui, ib_addr + 12, ib_vmid, "HEIGHT", (stream->words[2] >> 16) & 0x3FFF, NULL, 10);
								ui->add_field(ui, ib_addr + 16, ib_vmid, "SLICE_PITCH", stream->words[3] & 0x3FFFFF, NULL, 10);
								ui->add_field(ui, ib_addr + 20, ib_vmid, "ELEMENT_SIZE", stream->words[4] & 0x7, NULL, 10);
								ui->add_field(ui, ib_addr + 20, ib_vmid, "ARRAY_MODE", (stream->words[4] >> 3) & 0xF, NULL, 10);
								ui->add_field(ui, ib_addr + 20, ib_vmid, "MIT_MODE", (stream->words[4] >> 8) & 0x7, NULL, 10);
								ui->add_field(ui, ib_addr + 20, ib_vmid, "TILESPLIT_SIZE", (stream->words[4] >> 11) & 0x7, NULL, 10);
								ui->add_field(ui, ib_addr + 20, ib_vmid, "BANK_W", (stream->words[4] >> 15) & 0x3, NULL, 10);
								ui->add_field(ui, ib_addr + 20, ib_vmid, "BANK_H", (stream->words[4] >> 18) & 0x3, NULL, 10);
								ui->add_field(ui, ib_addr + 20, ib_vmid, "NUM_BANK", (stream->words[4] >> 21) & 0x3, NULL, 10);
								ui->add_field(ui, ib_addr + 20, ib_vmid, "MAT_ASPT", (stream->words[4] >> 24) & 0x3, NULL, 10);
								ui->add_field(ui, ib_addr + 20, ib_vmid, "PIPE_CONFIG", (stream->words[4] >> 26) & 0x1F, NULL, 10);
							} else {
								ui->add_field(ui, ib_addr + 12, ib_vmid, "WIDTH", stream->words[2] & 0x3FFF, NULL, 10);
								ui->add_field(ui, ib_addr + 16, ib_vmid, "HEIGHT", stream->words[3] & 0x3FFF, NULL, 10);
								ui->add_field(ui, ib_addr + 16, ib_vmid, "DEPTH", (stream->words[3] >> 16) & 0x1FFF, NULL, 10);
								ui->add_field(ui, ib_addr + 20, ib_vmid, "ELEMENT_SIZE", stream->words[4] & 0x7, NULL, 10);
								ui->add_field(ui, ib_addr + 20, ib_vmid, "SWIZZLE_MODE", (stream->words[4] >> 3) & 0x1F, NULL, 10);
								ui->add_field(ui, ib_addr + 20, ib_vmid, "DIMENSION", (stream->words[4] >> 9) & 0x3, NULL, 10);
								if (ver_maj == ver_ai) {
									ui->add_field(ui, ib_addr + 20, ib_vmid, "EPITCH", (stream->words[4] >> 16) & 0xFFFF, NULL, 10);
								} else if (ver_maj >= ver_nv) {
									ui->add_field(ui, ib_addr + 20, ib_vmid, "MIP_MAX", (stream->words[4] >> 16) & 0xF, NULL, 10);
								}
							}

							ui->add_field(ui, ib_addr + 24, ib_vmid, "X", stream->words[5] & 0x3FFF, NULL, 10);
							ui->add_field(ui, ib_addr + 24, ib_vmid, "Y", (stream->words[5] >> 16) & 0x3FFF, NULL, 10);
							ui->add_field(ui, ib_addr + 28, ib_vmid, "Z", stream->words[6] & 0x1FFF, NULL, 10);
							ui->add_field(ui, ib_addr + 28, ib_vmid, "LINEAR_SW", (stream->words[6] >> 16) & 0x3, NULL, 10);
							if (has_cp_fields) {
								ui->add_field(ui, ib_addr + 28, ib_vmid, "LINEAR_CACHE_POLICY", (stream->words[6] >> 18) & 0x7, NULL, 10);
							}
							ui->add_field(ui, ib_addr + 28, ib_vmid, "TILE_SW", (stream->words[6] >> 24) & 0x3, NULL, 10);
							if (has_cp_fields) {
								ui->add_field(ui, ib_addr + 28, ib_vmid, "TILE_CACHE_POLICY", (stream->words[6] >> 26) & 0x7, NULL, 10);
							}
							ui->add_field(ui, ib_addr + 32, ib_vmid, "LINEAR_ADDR_LO", stream->words[7], NULL, 16);
							ui->add_field(ui, ib_addr + 36, ib_vmid, "LINEAR_ADDR_HI", stream->words[8], NULL, 16);
							ui->add_field(ui, ib_addr + 40, ib_vmid, "LINEAR_PITCH", stream->words[9] & 0x7FFFF, NULL, 10);
							if (ver_maj >= ver_ai) {
								ui->add_field(ui, ib_addr + 44, ib_vmid, "LINEAR_SLICE_PITCH", stream->words[10], NULL, 10);
								ui->add_field(ui, ib_addr + 48, ib_vmid, "COUNT", stream->words[11] & 0x3FFFFFFF, NULL, 10);
							} else {
								ui->add_field(ui, ib_addr + 44, ib_vmid, "COUNT", stream->words[10] & 0xFFFFF, NULL, 10);
							}
							break;
						}
					case 3: // SOA
						ui->start_opcode(ui, ib_addr, ib_vmid, stream->opcode, stream->sub_opcode, stream->nwords + 1, "COPY (STRUCT)", stream->header_dw, stream->words);
						ui->add_field(ui, ib_addr + 0, ib_vmid, "TMZ", (stream->header_dw >> 18) & 0x1, NULL, 10);
						if (has_cpv_flag) {
							ui->add_field(ui, ib_addr + 0, ib_vmid, "CPV", (stream->header_dw >> 28) & 0x1, NULL, 10);
						}
						ui->add_field(ui, ib_addr + 0, ib_vmid, "DETILE", (stream->header_dw >> 31) & 0x1, NULL, 10);
						ui->add_field(ui, ib_addr + 4, ib_vmid, "SB_ADDR_LO", stream->words[0], NULL, 16);
						ui->add_field(ui, ib_addr + 8, ib_vmid, "SB_ADDR_HI", stream->words[1], NULL, 16);
						ui->add_field(ui, ib_addr + 12, ib_vmid, "START_INDEX", stream->words[2], NULL, 10);
						ui->add_field(ui, ib_addr + 16, ib_vmid, "COUNT", stream->words[3], NULL, 10);
						ui->add_field(ui, ib_addr + 20, ib_vmid, "STRIDE", stream->words[4] & 0x7FF, NULL, 10);
						ui->add_field(ui, ib_addr + 20, ib_vmid, "LINEAR_SW", (stream->words[4] >> 16) & 0x3, NULL, 10);
						if (has_cp_fields) {
							ui->add_field(ui, ib_addr + 20, ib_vmid, "LINEAR_CACHE_POLICY", (stream->words[4] >> 18) & 0x7, NULL, 10);
						}
						ui->add_field(ui, ib_addr + 20, ib_vmid, "STRUCT_SW", (stream->words[4] >> 24) & 0x3, NULL, 10);
						if (has_cp_fields) {
							ui->add_field(ui, ib_addr + 20, ib_vmid, "STRUCT_CACHE_POLICY", (stream->words[4] >> 26) & 0x7, NULL, 10);
						}
						ui->add_field(ui, ib_addr + 24, ib_vmid, "LINEAR_ADDR_LO", stream->words[5], NULL, 16);
						ui->add_field(ui, ib_addr + 28, ib_vmid, "LINEAR_ADDR_HI", stream->words[6], NULL, 16);
						break;
					case 4: // LINEAR_SUB_WINDOW
						ui->start_opcode(ui, ib_addr, ib_vmid, stream->opcode, stream->sub_opcode, stream->nwords + 1, "COPY (LINEAR_SUB_WINDOW)", stream->header_dw, stream->words);
						ui->add_field(ui, ib_addr + 0, ib_vmid, "TMZ", (stream->header_dw >> 18) & 0x1, NULL, 10);
						if (has_cpv_flag) {
							ui->add_field(ui, ib_addr + 0, ib_vmid, "CPV", (stream->header_dw >> 19) & 0x1, NULL, 10);
						}
						ui->add_field(ui, ib_addr + 0, ib_vmid, "ELEMENTSIZE", (stream->header_dw >> 29) & 0x7, NULL, 10);

						ui->add_field(ui, ib_addr + 4, ib_vmid, "SRC_ADDR_LO", stream->words[0], NULL, 16);
						ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_ADDR_HI", stream->words[1], NULL, 16);
						ui->add_field(ui, ib_addr + 12, ib_vmid, "SRC_X", stream->words[2] & 0x3FFF, NULL, 10);
						ui->add_field(ui, ib_addr + 12, ib_vmid, "SRC_Y", (stream->words[2] >> 16) & 0x3FFF, NULL, 10);
						ui->add_field(ui, ib_addr + 16, ib_vmid, "SRC_Z", stream->words[3] & z_mask, NULL, 10);
						ui->add_field(ui, ib_addr + 16, ib_vmid, "SRC_PITCH", (stream->words[3] >> pitch_shift) & pitch_mask, NULL, 10);
						ui->add_field(ui, ib_addr + 20, ib_vmid, "SRC_SLICE_PITCH", stream->words[4] & 0xFFFFFFF, NULL, 10);
						ui->add_field(ui, ib_addr + 24, ib_vmid, "DST_ADDR_LO", stream->words[5], NULL, 16);
						ui->add_field(ui, ib_addr + 28, ib_vmid, "DST_ADDR_HI", stream->words[6], NULL, 16);
						ui->add_field(ui, ib_addr + 32, ib_vmid, "DST_X", stream->words[7] & 0x3FFF, NULL, 10);
						ui->add_field(ui, ib_addr + 32, ib_vmid, "DST_Y", (stream->words[7] >> 16) & 0x3FFF, NULL, 10);
						ui->add_field(ui, ib_addr + 36, ib_vmid, "DST_Z", stream->words[8] & z_mask, NULL, 10);
						ui->add_field(ui, ib_addr + 36, ib_vmid, "DST_PITCH", (stream->words[8] >> pitch_shift) & pitch_mask, NULL, 10);
						ui->add_field(ui, ib_addr + 40, ib_vmid, "DST_SLICE_PITCH", stream->words[9] & 0xFFFFFFF, NULL, 10);
						ui->add_field(ui, ib_addr + 44, ib_vmid, "RECT_X", stream->words[10] & 0x3FFF, NULL, 10);
						ui->add_field(ui, ib_addr + 44, ib_vmid, "RECT_Y", (stream->words[10] >> 16) & 0x3FFF, NULL, 10);
						ui->add_field(ui, ib_addr + 48, ib_vmid, "RECT_Z", stream->words[11] & 0x1FFF, NULL, 10);

						ui->add_field(ui, ib_addr + 48, ib_vmid, "DST_SW", (stream->words[11] >> 16) & 0x3, NULL, 10);
						if (has_cp_fields) {
							ui->add_field(ui, ib_addr + 48, ib_vmid, "DST_CACHE_POLICY", (stream->words[11] >> 18) & 0x7, NULL, 10);
						} else {
							ui->add_field(ui, ib_addr + 48, ib_vmid, "DST_HA", (stream->words[11] >> 22) & 0x1, NULL, 10);
						}

						ui->add_field(ui, ib_addr + 48, ib_vmid, "SRC_SW", (stream->words[11] >> 24) & 0x3, NULL, 10);
						if (has_cp_fields) {
							ui->add_field(ui, ib_addr + 48, ib_vmid, "SRC_CACHE_POLICY", (stream->words[11] >> 26) & 0x7, NULL, 10);
						} else {
							ui->add_field(ui, ib_addr + 48, ib_vmid, "SRC_HA", (stream->words[11] >> 30) & 0x1, NULL, 10);
						}
						break;
					case 5: // TILED_SUB_WINDOW
						ui->start_opcode(ui, ib_addr, ib_vmid, stream->opcode, stream->sub_opcode, stream->nwords + 1, "COPY (TILED_SUB_WINDOW)", stream->header_dw, stream->words);
						ui->add_field(ui, ib_addr + 0, ib_vmid, "TMZ", (stream->header_dw >> 18) & 0x1, NULL, 10);
						if (ver_maj == ver_ai) {
							ui->add_field(ui, ib_addr + 0, ib_vmid, "MIP_MAX", (stream->words[0] >> 20) & 0xF, NULL, 16);
							ui->add_field(ui, ib_addr + 0, ib_vmid, "MIP_ID", (stream->words[0] >> 24) & 0xF, NULL, 16);
						} else if (ver_maj >= ver_nv) {
							ui->add_field(ui, ib_addr + 0, ib_vmid, "DCC", (stream->header_dw >> 19) & 0x1, NULL, 10);
						}
						if (has_cpv_flag) {
							ui->add_field(ui, ib_addr + 0, ib_vmid, "CPV", (stream->header_dw >> 28) & 0x1, NULL, 10);
						}
						ui->add_field(ui, ib_addr + 0, ib_vmid, "DETILE", stream->header_dw >> 31, NULL, 16);
						ui->add_field(ui, ib_addr + 4, ib_vmid, "TILED_ADDR_LO", stream->words[0], NULL, 16);
						ui->add_field(ui, ib_addr + 8, ib_vmid, "TILED_ADDR_HI", stream->words[1], NULL, 16);
						ui->add_field(ui, ib_addr + 12, ib_vmid, "TILED_X", (stream->words[2] >> 0) & 0x3FFF, NULL, 10);
						ui->add_field(ui, ib_addr + 12, ib_vmid, "TILED_Y", (stream->words[2] >> 16) & 0x3FFF, NULL, 10);
						ui->add_field(ui, ib_addr + 16, ib_vmid, "TILED_Z", (stream->words[3] >> 0) & z_mask, NULL, 10);
						if (ver_maj <= ver_vi) {
							ui->add_field(ui, ib_addr + 16, ib_vmid, "TILED_PITCH", (stream->words[3] >> 16) & 0x3FFF, NULL, 10);
							ui->add_field(ui, ib_addr + 20, ib_vmid, "PITCH_IN_TILE", stream->words[4] & 0xFFFFFFF, NULL, 10);
							ui->add_field(ui, ib_addr + 24, ib_vmid, "ELEMENT_SIZE", (stream->words[5] >> 0) & 0x7, NULL, 10);
							ui->add_field(ui, ib_addr + 24, ib_vmid, "ARRAY_MODE", (stream->words[5] >> 3) & 0xF, NULL, 10);
							ui->add_field(ui, ib_addr + 24, ib_vmid, "MIT_MODE", (stream->words[5] >> 8) & 0x7, NULL, 10);
							ui->add_field(ui, ib_addr + 24, ib_vmid, "TILESPLIT_SIZE", (stream->words[5] >> 11) & 0x7, NULL, 10);
							ui->add_field(ui, ib_addr + 24, ib_vmid, "BANK_W", (stream->words[5] >> 15) & 0x3, NULL, 10);
							ui->add_field(ui, ib_addr + 24, ib_vmid, "BANK_H", (stream->words[5] >> 18) & 0x3, NULL, 10);
							ui->add_field(ui, ib_addr + 24, ib_vmid, "NUM_BANK", (stream->words[5] >> 21) & 0x3, NULL, 10);
							ui->add_field(ui, ib_addr + 24, ib_vmid, "MAT_ASPT", (stream->words[5] >> 24) & 0x3, NULL, 10);
							ui->add_field(ui, ib_addr + 24, ib_vmid, "PIPE_CONFIG", (stream->words[5] >> 26) & 0x1F, NULL, 10);
						} else {
							ui->add_field(ui, ib_addr + 16, ib_vmid, "WIDTH", (stream->words[3] >> 16) & 0x3FFF, NULL, 10);
							ui->add_field(ui, ib_addr + 20, ib_vmid, "HEIGHT", (stream->words[4] >> 0) & 0x3FFF, NULL, 10);
							ui->add_field(ui, ib_addr + 20, ib_vmid, "DEPTH", (stream->words[4] >> 16) & z_mask, NULL, 10);
							ui->add_field(ui, ib_addr + 24, ib_vmid, "ELEMENT_SIZE", (stream->words[5] >> 0) & 0x7, NULL, 10);
							ui->add_field(ui, ib_addr + 24, ib_vmid, "SWIZZLE_MODE", (stream->words[5] >> 3) & 0x1F, NULL, 10);
							ui->add_field(ui, ib_addr + 24, ib_vmid, "DIMENSION", (stream->words[5] >> 9) & 0x3, NULL, 10);
							if (ver_maj == ver_ai) {
								ui->add_field(ui, ib_addr + 24, ib_vmid, "EPITCH", (stream->words[5] >> 16) & 0xFFFF, NULL, 10);
							} else {
								ui->add_field(ui, ib_addr + 24, ib_vmid, "MIP_MAX", (stream->words[5] >> 16) & 0xF, NULL, 10);
								ui->add_field(ui, ib_addr + 24, ib_vmid, "MIP_ID", (stream->words[5] >> 20) & 0xF, NULL, 10);
							}
						}

						ui->add_field(ui, ib_addr + 28, ib_vmid, "LINEAR_ADDR_LO", stream->words[6], NULL, 16);
						ui->add_field(ui, ib_addr + 32, ib_vmid, "LINEAR_ADDR_HI", stream->words[7], NULL, 16);

						ui->add_field(ui, ib_addr + 36, ib_vmid, "LINEAR_X", (stream->words[8] >> 0) & 0x3FFF, NULL, 10);
						ui->add_field(ui, ib_addr + 36, ib_vmid, "LINEAR_Y", (stream->words[8] >> 0) & 0x3FFF, NULL, 10);
						ui->add_field(ui, ib_addr + 40, ib_vmid, "LINEAR_Z", (stream->words[9] >> 0) & z_mask, NULL, 10);
						ui->add_field(ui, ib_addr + 40, ib_vmid, "LINEAR_PITCH", (stream->words[9] >> 16) & 0x3FFF, NULL, 10);
						ui->add_field(ui, ib_addr + 44, ib_vmid, "LINEAR_SLICE_PITCH", stream->words[10] & 0xFFFFFFF, NULL, 10);

						ui->add_field(ui, ib_addr + 48, ib_vmid, "RECT_X", (stream->words[11] >> 0) & 0x3FFF, NULL, 10);
						ui->add_field(ui, ib_addr + 48, ib_vmid, "RECT_Y", (stream->words[11] >> 16) & 0x3FFF, NULL, 10);
						ui->add_field(ui, ib_addr + 52, ib_vmid, "RECT_Z", (stream->words[12] >> 0) & z_mask, NULL, 10);
						ui->add_field(ui, ib_addr + 52, ib_vmid, "LINEAR_SW", (stream->words[12] >> 16) & 0x3, NULL, 10);
						if (has_cp_fields) {
							ui->add_field(ui, ib_addr + 52, ib_vmid, "LINEAR_CACHE_POLICY", (stream->words[12] >> 18) & 0x7, NULL, 10);
						}
						ui->add_field(ui, ib_addr + 52, ib_vmid, "TILE_SW", (stream->words[12] >> 22) & 0x3, NULL, 10);
						if (has_cp_fields) {
							ui->add_field(ui, ib_addr + 52, ib_vmid, "TILE_CACHE_POLICY", (stream->words[12] >> 26) & 0x7, NULL, 10);
						}
						if (ver_maj >= ver_nv) {
							ui->add_field(ui, ib_addr + 56, ib_vmid, "META_ADDR_LO", stream->words[13], NULL, 16);
							ui->add_field(ui, ib_addr + 60, ib_vmid, "META_ADDR_HI", stream->words[14], NULL, 16);
							ui->add_field(ui, ib_addr + 64, ib_vmid, "DATA_FORMAT", stream->words[15] & 0x7F, NULL, 10);
							ui->add_field(ui, ib_addr + 64, ib_vmid, "COLOR_TRANSFORM_DISABLE", (stream->words[15] >> 7) & 0x1, NULL, 10);
							ui->add_field(ui, ib_addr + 64, ib_vmid, "ALPHA_IS_ON_MSB", (stream->words[15] >> 8) & 0x1, NULL, 10);
							ui->add_field(ui, ib_addr + 64, ib_vmid, "NUMBER_TYPE", (stream->words[15] >> 9) & 0x7, NULL, 10);
							ui->add_field(ui, ib_addr + 64, ib_vmid, "SURFACE_TYPE", (stream->words[15] >> 12) & 0x3, NULL, 10);
							if (has_cpv_flag) ui->add_field(ui, ib_addr + 64, ib_vmid, "META_LLC", (stream->words[15] >> 14) & 0x1, NULL, 10);
							ui->add_field(ui, ib_addr + 64, ib_vmid, "MAX_COMP_BLOCK_SIZE", (stream->words[15] >> 24) & 0x3, NULL, 10);
							ui->add_field(ui, ib_addr + 64, ib_vmid, "MAX_UNCOMP_BLOCK_SIZE", (stream->words[15] >> 26) & 0x3, NULL, 10);
							ui->add_field(ui, ib_addr + 64, ib_vmid, "WRITE_COMPRESS_ENABLE", (stream->words[15] >> 28) & 0x1, NULL, 10);
							ui->add_field(ui, ib_addr + 64, ib_vmid, "META_TMZ", (stream->words[15] >> 29) & 0x1, NULL, 10);
							ui->add_field(ui, ib_addr + 64, ib_vmid, "PIPE_ALIGNED", (stream->words[15] >> 31) & 0x1, NULL, 10);
						}
						break;
					case 6: // T2T_SUB_WINDOW
						ui->start_opcode(ui, ib_addr, ib_vmid, stream->opcode, stream->sub_opcode, stream->nwords + 1, "COPY (T2T_SUB_WINDOW)", stream->header_dw, stream->words);
						ui->add_field(ui, ib_addr + 0, ib_vmid, "TMZ", (stream->header_dw >> 18) & 0x1, NULL, 10);
						if (ver_maj == ver_ai) {
							ui->add_field(ui, ib_addr + 0, ib_vmid, "MIP_MAX", (stream->header_dw >> 20) & 0xF, NULL, 10);
						} else if (ver_maj >= ver_nv) {
							ui->add_field(ui, ib_addr + 0, ib_vmid, "DCC", (stream->header_dw >> 19) & 0x1, NULL, 10);
							if (has_cpv_flag) ui->add_field(ui, ib_addr + 0, ib_vmid, "CPV", (stream->header_dw >> 28) & 0x1, NULL, 10);
							ui->add_field(ui, ib_addr + 0, ib_vmid, "DCC_DIR", (stream->header_dw >> 31) & 0x1, NULL, 10);
						}
						ui->add_field(ui, ib_addr + 4, ib_vmid, "SRC_ADDR_LO", stream->words[0], NULL, 16);
						ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_ADDR_HI", stream->words[1], NULL, 16);
						ui->add_field(ui, ib_addr + 12, ib_vmid, "SRC_X", (stream->words[2] >> 0) & 0x3FFF, NULL, 10);
						ui->add_field(ui, ib_addr + 12, ib_vmid, "SRC_Y", (stream->words[2] >> 16) & 0x3FFF, NULL, 10);
						ui->add_field(ui, ib_addr + 16, ib_vmid, "SRC_Z", (stream->words[3] >> 0) & z_mask, NULL, 10);
						if (ver_maj <= ver_vi) {
							ui->add_field(ui, ib_addr + 16, ib_vmid, "SRC_PITCH", (stream->words[3] >> 16) & 0x3FFF, NULL, 10);
							ui->add_field(ui, ib_addr + 20, ib_vmid, "SRC_SLICE_PITCH", stream->words[4] & 0xFFFFFFF, NULL, 10);

							ui->add_field(ui, ib_addr + 24, ib_vmid, "SRC_ELEMENT_SIZE", (stream->words[5] >> 0) & 0x7, NULL, 10);
							ui->add_field(ui, ib_addr + 24, ib_vmid, "SRC_ARRAY_MODE", (stream->words[5] >> 3) & 0xF, NULL, 10);
							ui->add_field(ui, ib_addr + 24, ib_vmid, "SRC_MIT_MODE", (stream->words[5] >> 8) & 0x7, NULL, 10);
							ui->add_field(ui, ib_addr + 24, ib_vmid, "SRC_TILESPLIT_SIZE", (stream->words[5] >> 11) & 0x7, NULL, 10);
							ui->add_field(ui, ib_addr + 24, ib_vmid, "SRC_BANK_W", (stream->words[5] >> 15) & 0x3, NULL, 10);
							ui->add_field(ui, ib_addr + 24, ib_vmid, "SRC_BANK_H", (stream->words[5] >> 18) & 0x3, NULL, 10);
							ui->add_field(ui, ib_addr + 24, ib_vmid, "SRC_NUM_BANKS", (stream->words[5] >> 21) & 0x3, NULL, 10);
							ui->add_field(ui, ib_addr + 24, ib_vmid, "SRC_MAT_ASPT", (stream->words[5] >> 24) & 0x3, NULL, 10);
							ui->add_field(ui, ib_addr + 24, ib_vmid, "SRC_PIPE_CONFIG", (stream->words[5] >> 26) & 0x1F, NULL, 10);
						} else {
							ui->add_field(ui, ib_addr + 16, ib_vmid, "SRC_WIDTH", (stream->words[3] >> 16) & 0x3FFF, NULL, 10);
							ui->add_field(ui, ib_addr + 20, ib_vmid, "SRC_HEIGHT", (stream->words[4] >> 0) & 0x3FFF, NULL, 10);
							ui->add_field(ui, ib_addr + 20, ib_vmid, "SRC_DEPTH", (stream->words[4] >> 16) & z_mask, NULL, 10);
							ui->add_field(ui, ib_addr + 24, ib_vmid, "SRC_ELEMENT_SIZE", (stream->words[5] >> 0) & 0x7, NULL, 10);
							ui->add_field(ui, ib_addr + 24, ib_vmid, "SRC_SWIZZLE_MODE", (stream->words[5] >> 3) & 0x1F, NULL, 10);
							ui->add_field(ui, ib_addr + 24, ib_vmid, "SRC_DIMENSION", (stream->words[5] >> 9) & 0x3, NULL, 10);
							if (ver_maj == ver_ai) {
								ui->add_field(ui, ib_addr + 24, ib_vmid, "SRC_EPITCH", (stream->words[5] >> 16) & 0xFFFF, NULL, 10);
							} else {
								ui->add_field(ui, ib_addr + 24, ib_vmid, "SRC_MIP_MAX", (stream->words[5] >> 16) & 0xF, NULL, 10);
								ui->add_field(ui, ib_addr + 24, ib_vmid, "SRC_MIP_ID", (stream->words[5] >> 20) & 0xF, NULL, 10);
							}
						}
						ui->add_field(ui, ib_addr + 28, ib_vmid, "DST_ADDR_LO", stream->words[6], NULL, 16);
						ui->add_field(ui, ib_addr + 32, ib_vmid, "DST_ADDR_HI", stream->words[7], NULL, 16);
						ui->add_field(ui, ib_addr + 36, ib_vmid, "DST_X", (stream->words[8] >> 0) & 0x3FFF, NULL, 10);
						ui->add_field(ui, ib_addr + 36, ib_vmid, "DST_Y", (stream->words[8] >> 16) & 0x3FFF, NULL, 10);
						ui->add_field(ui, ib_addr + 40, ib_vmid, "DST_Z", (stream->words[9] >> 0) & z_mask, NULL, 10);
						if (ver_maj <= ver_vi) {
							ui->add_field(ui, ib_addr + 40, ib_vmid, "DST_PITCH", (stream->words[9] >> 16) & 0x3FFF, NULL, 10);
							ui->add_field(ui, ib_addr + 44, ib_vmid, "DST_SLICE_PITCH", stream->words[10] & 0xFFFFFFF, NULL, 10);

							ui->add_field(ui, ib_addr + 48, ib_vmid, "DST_ARRAY_MODE", (stream->words[11] >> 0) & 0x7, NULL, 10);
							ui->add_field(ui, ib_addr + 48, ib_vmid, "DST_ARRAY_MODE", (stream->words[11] >> 3) & 0xF, NULL, 10);
							ui->add_field(ui, ib_addr + 48, ib_vmid, "DST_MIT_MODE", (stream->words[11] >> 8) & 0x7, NULL, 10);
							ui->add_field(ui, ib_addr + 48, ib_vmid, "DST_TILESPLIT_SIZE", (stream->words[11] >> 11) & 0x7, NULL, 10);
							ui->add_field(ui, ib_addr + 48, ib_vmid, "DST_BANK_W", (stream->words[11] >> 15) & 0x3, NULL, 10);
							ui->add_field(ui, ib_addr + 48, ib_vmid, "DST_BANK_H", (stream->words[11] >> 18) & 0x3, NULL, 10);
							ui->add_field(ui, ib_addr + 48, ib_vmid, "DST_NUM_BANKS", (stream->words[11] >> 21) & 0x3, NULL, 10);
							ui->add_field(ui, ib_addr + 48, ib_vmid, "DST_MAT_ASPT", (stream->words[11] >> 24) & 0x3, NULL, 10);
							ui->add_field(ui, ib_addr + 48, ib_vmid, "DST_PIPE_CONFIG", (stream->words[11] >> 26) & 0x1F, NULL, 10);
						} else {
							ui->add_field(ui, ib_addr + 40, ib_vmid, "DST_WIDTH", (stream->words[9] >> 16) & 0x3FFF, NULL, 10);
							ui->add_field(ui, ib_addr + 44, ib_vmid, "DST_HEIGHT", (stream->words[10] >> 0) & 0x3FFF, NULL, 10);
							ui->add_field(ui, ib_addr + 44, ib_vmid, "DST_DEPTH", (stream->words[10] >> 16) & z_mask, NULL, 10);
							ui->add_field(ui, ib_addr + 48, ib_vmid, "DST_ELEMENT_SIZE", (stream->words[11] >> 0) & 0x7, NULL, 10);
							ui->add_field(ui, ib_addr + 48, ib_vmid, "DST_SWIZZLE_MODE", (stream->words[11] >> 3) & 0x1F, NULL, 10);
							ui->add_field(ui, ib_addr + 48, ib_vmid, "DST_DIMENSION", (stream->words[11] >> 9) & 0x3, NULL, 10);
							if (ver_maj == ver_ai) {
								ui->add_field(ui, ib_addr + 48, ib_vmid, "DST_EPITCH", (stream->words[11] >> 16) & 0xFFFF, NULL, 10);
							} else {
								ui->add_field(ui, ib_addr + 48, ib_vmid, "DST_MIP_MAX", (stream->words[11] >> 16) & 0xF, NULL, 10);
								ui->add_field(ui, ib_addr + 48, ib_vmid, "DST_MIP_ID", (stream->words[11] >> 20) & 0xF, NULL, 10);
							}
						}
						ui->add_field(ui, ib_addr + 52, ib_vmid, "RECT_X", (stream->words[12] >> 0) & 0x3FFF, NULL, 10);
						ui->add_field(ui, ib_addr + 52, ib_vmid, "RECT_Y", (stream->words[12] >> 16) & 0x3FFF, NULL, 10);
						ui->add_field(ui, ib_addr + 56, ib_vmid, "RECT_Z", (stream->words[13] >> 0) & z_mask, NULL, 10);
						ui->add_field(ui, ib_addr + 56, ib_vmid, "DST_SW", (stream->words[13] >> 16) & 0x3, NULL, 10);
						if (has_cp_fields) {
							ui->add_field(ui, ib_addr + 56, ib_vmid, "DST_CACHE_POLICY", (stream->words[13] >> 18) & 0x7, NULL, 10);
						}
						ui->add_field(ui, ib_addr + 56, ib_vmid, "SRC_SW", (stream->words[13] >> 22) & 0x3, NULL, 10);
						if (has_cp_fields) {
							ui->add_field(ui, ib_addr + 56, ib_vmid, "SRC_CACHE_POLICY", (stream->words[13] >> 26) & 0x7, NULL, 10);
						}
						if (ver_maj >= ver_nv) {
							ui->add_field(ui, ib_addr + 60, ib_vmid, "META_ADDR_LO", stream->words[14], NULL, 16);
							ui->add_field(ui, ib_addr + 64, ib_vmid, "META_ADDR_HI", stream->words[15], NULL, 16);
							ui->add_field(ui, ib_addr + 68, ib_vmid, "DATA_FORMAT", stream->words[16] & 0x7F, NULL, 10);
							ui->add_field(ui, ib_addr + 68, ib_vmid, "COLOR_TRANSFORM_DISABLE", (stream->words[16] >> 7) & 0x1, NULL, 10);
							ui->add_field(ui, ib_addr + 68, ib_vmid, "ALPHA_IS_ON_MSB", (stream->words[16] >> 8) & 0x1, NULL, 10);
							ui->add_field(ui, ib_addr + 68, ib_vmid, "NUMBER_TYPE", (stream->words[16] >> 9) & 0x7, NULL, 10);
							ui->add_field(ui, ib_addr + 68, ib_vmid, "SURFACE_TYPE", (stream->words[16] >> 12) & 0x3, NULL, 10);
							if (has_cpv_flag) ui->add_field(ui, ib_addr + 68, ib_vmid, "META_LLC", (stream->words[16] >> 14) & 0x1, NULL, 10);
							ui->add_field(ui, ib_addr + 68, ib_vmid, "MAX_COMP_BLOCK_SIZE", (stream->words[16] >> 24) & 0x3, NULL, 10);
							ui->add_field(ui, ib_addr + 68, ib_vmid, "MAX_UNCOMP_BLOCK_SIZE", (stream->words[16] >> 26) & 0x3, NULL, 10);
							ui->add_field(ui, ib_addr + 68, ib_vmid, "WRITE_COMPRESS_ENABLE", (stream->words[16] >> 28) & 0x1, NULL, 10);
							ui->add_field(ui, ib_addr + 68, ib_vmid, "META_TMZ", (stream->words[16] >> 29) & 0x1, NULL, 10);
							ui->add_field(ui, ib_addr + 68, ib_vmid, "PIPE_ALIGNED", (stream->words[16] >> 31) & 0x1, NULL, 10);
						}
						break;
					case 7: // DIRTY_PAGE
						ui->start_opcode(ui, ib_addr, ib_vmid, stream->opcode, stream->sub_opcode, stream->nwords + 1, "COPY (DIRTY_PAGE)", stream->header_dw, stream->words);
						ui->add_field(ui, ib_addr + 0, ib_vmid, "TMZ", (stream->header_dw >> 18) & 0x1, NULL, 10);
						if (has_cpv_flag) {
							ui->add_field(ui, ib_addr + 0, ib_vmid, "CPV", (stream->header_dw >> 19) & 0x1, NULL, 10);
						}
						ui->add_field(ui, ib_addr + 0, ib_vmid, "ALL", (stream->header_dw >> 31) & 0x1, NULL, 10);
						ui->add_field(ui, ib_addr + 4, ib_vmid, "COUNT", (stream->words[0] >> 0) & 0x3FFFFF, NULL, 10);
						if (ver_maj >= ver_nv) ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_MTYPE", (stream->words[1] >> 3) & 0x7, NULL, 10);
						if (has_cpv_flag) {
							ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_L2_POLICY", (stream->words[1] >> 6) & 0x3, NULL, 10);
							ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_LLC", (stream->words[1] >> 8) & 0x1, NULL, 10);
						} else if (has_cp_fields) {
							ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_CACHE_POLICY", (stream->words[1] >> 5) & 0x3, NULL, 10);
						}
						if (ver_maj >= ver_nv) ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_MTYPE", (stream->words[1] >> 11) & 0x7, NULL, 10);
						if (has_cpv_flag) {
							ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_L2_POLICY", (stream->words[1] >> 14) & 0x3, NULL, 10);
							ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_LLC", (stream->words[1] >> 16) & 0x1, NULL, 10);
						} else if (has_cp_fields) {
							ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_CACHE_POLICY", (stream->words[1] >> 13) & 0x3, NULL, 10);
						}
						if (ver_maj >= ver_nv) {
							ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_SW", (stream->words[1] >> 17) & 0x3, NULL, 10);
						} else {
							ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_SW", (stream->words[1] >> 16) & 0x3, NULL, 10);
						}
						ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_GCC", (stream->words[1] >> 19) & 0x1, NULL, 10);
						ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_SYS", (stream->words[1] >> 20) & 0x1, NULL, 10);
						ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_SNOOP", (stream->words[1] >> 22) & 0x1, NULL, 10);
						ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_GPA", (stream->words[1] >> 23) & 0x1, NULL, 10);
						if (has_cpv_flag) {
							ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_SW", (stream->words[1] >> 25) & 0x3, NULL, 10);
						} else {
							ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_SW", (stream->words[1] >> 24) & 0x3, NULL, 10);
						}
						ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_SYS", (stream->words[1] >> 28) & 0x1, NULL, 10);
						ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_SNOOP", (stream->words[1] >> 30) & 0x1, NULL, 10);
						ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_GPA", (stream->words[1] >> 31) & 0x1, NULL, 10);

						ui->add_field(ui, ib_addr + 12, ib_vmid, "SRC_ADDR_LO", stream->words[2], NULL, 16);
						ui->add_field(ui, ib_addr + 16, ib_vmid, "SRC_ADDR_HI", stream->words[3], NULL, 16);

						ui->add_field(ui, ib_addr + 20, ib_vmid, "DST_ADDR_LO", stream->words[4], NULL, 16);
						ui->add_field(ui, ib_addr + 24, ib_vmid, "DST_ADDR_HI", stream->words[5], NULL, 16);
						break;
					case 8: // LINEAR_PHY
						ui->start_opcode(ui, ib_addr, ib_vmid, stream->opcode, stream->sub_opcode, stream->nwords + 1, "COPY (LINEAR_PHY)", stream->header_dw, stream->words);
						ui->add_field(ui, ib_addr + 0, ib_vmid, "TMZ", (stream->header_dw >> 18) & 0x1, NULL, 10);
						if (has_cpv_flag) {
							ui->add_field(ui, ib_addr + 0, ib_vmid, "CPV", (stream->header_dw >> 19) & 0x1, NULL, 10);
						}
						ui->add_field(ui, ib_addr + 4, ib_vmid, "COUNT", (stream->words[0] >> 0) & 0x3FFFFF, NULL, 10);
						if (ver_maj >= ver_nv) ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_MTYPE", (stream->words[1] >> 3) & 0x7, NULL, 10);
						if (has_cpv_flag) {
							ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_L2_POLICY", (stream->words[1] >> 6) & 0x3, NULL, 10);
							ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_LLC", (stream->words[1] >> 8) & 0x1, NULL, 10);
						} else if (has_cp_fields) {
							ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_CACHE_POLICY", (stream->words[1] >> 5) & 0x3, NULL, 10);
						}
						if (ver_maj >= ver_nv) ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_MTYPE", (stream->words[1] >> 11) & 0x7, NULL, 10);
						if (has_cpv_flag) {
							ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_L2_POLICY", (stream->words[1] >> 14) & 0x3, NULL, 10);
							ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_LLC", (stream->words[1] >> 16) & 0x1, NULL, 10);
						} else if (has_cp_fields) {
							ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_CACHE_POLICY", (stream->words[1] >> 13) & 0x3, NULL, 10);
						}
						if (ver_maj == ver_ai) {
							ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_SW", (stream->words[1] >> 16) & 0x3, NULL, 10);
						}
						ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_GCC", (stream->words[1] >> 19) & 0x1, NULL, 10);
						ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_SYS", (stream->words[1] >> 20) & 0x1, NULL, 10);
						ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_LOG", (stream->words[1] >> 21) & 0x1, NULL, 10);
						ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_SNOOP", (stream->words[1] >> 22) & 0x1, NULL, 10);
						ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_GPA", (stream->words[1] >> 23) & 0x1, NULL, 10);
						if (ver_maj == ver_ai) {
							ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_SW", (stream->words[1] >> 24) & 0x3, NULL, 10);
						}
						ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_GCC", (stream->words[1] >> 27) & 0x1, NULL, 10);
						ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_SYS", (stream->words[1] >> 28) & 0x1, NULL, 10);
						ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_SNOOP", (stream->words[1] >> 30) & 0x1, NULL, 10);
						ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_GPA", (stream->words[1] >> 31) & 0x1, NULL, 10);
						for (n = 2; n + 3 < stream->nwords; n += 4) {
							uint32_t addr_idx = (n - 2) / 4;
							sprintf(str_buf, "SRC_ADDR%"PRIu32"_LO", addr_idx);
							ui->add_field(ui, ib_addr + 12 + 4 * (n - 2), ib_vmid, str_buf, stream->words[n], NULL, 16);
							sprintf(str_buf, "SRC_ADDR%"PRIu32"_HI", addr_idx);
							ui->add_field(ui, ib_addr + 16 + 4 * (n - 2), ib_vmid, str_buf, stream->words[n + 1], NULL, 16);
							sprintf(str_buf, "DST_ADDR%"PRIu32"_LO", addr_idx);
							ui->add_field(ui, ib_addr + 20 + 4 * (n - 2), ib_vmid, str_buf, stream->words[n + 2], NULL, 16);
							sprintf(str_buf, "DST_ADDR%"PRIu32"_HI", addr_idx);
							ui->add_field(ui, ib_addr + 24 + 4 * (n - 2), ib_vmid, str_buf, stream->words[n + 3], NULL, 16);
						}
						break;
					case 16: // LINEAR_BC
						ui->start_opcode(ui, ib_addr, ib_vmid, stream->opcode, stream->sub_opcode, stream->nwords + 1, "COPY (LINEAR_BC)", stream->header_dw, stream->words);
						ui->add_field(ui, ib_addr + 4, ib_vmid, "COUNT", (stream->words[0] >> 0) & 0x3FFFFF, NULL, 10);

						ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_SW", (stream->words[1] >> 16) & 0x3, NULL, 10);
						ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_HA", (stream->words[1] >> 19) & 0x1, NULL, 10);
						ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_SW", (stream->words[1] >> 24) & 0x3, NULL, 10);
						ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_HA", (stream->words[1] >> 27) & 0x1, NULL, 10);

						ui->add_field(ui, ib_addr + 12, ib_vmid, "SRC_ADDR_LO", stream->words[2], NULL, 16);
						ui->add_field(ui, ib_addr + 16, ib_vmid, "SRC_ADDR_HI", stream->words[3], NULL, 16);

						ui->add_field(ui, ib_addr + 20, ib_vmid, "DST_ADDR_LO", stream->words[4], NULL, 16);
						ui->add_field(ui, ib_addr + 24, ib_vmid, "DST_ADDR_HI", stream->words[5], NULL, 16);
						break;
					case 17: // TILED_BC
						if ((stream->header_dw >> 26) & 0x3) { // L2T BROADCAST or L2T FRAME TO FIELD
							if ((stream->header_dw >> 26) & 0x1) {
								ui->start_opcode(ui, ib_addr, ib_vmid, stream->opcode, stream->sub_opcode, stream->nwords + 1, "COPY (L2T_FRAME_TO_FIELD_BC)", stream->header_dw, stream->words);
							} else {
								ui->start_opcode(ui, ib_addr, ib_vmid, stream->opcode, stream->sub_opcode, stream->nwords + 1, "COPY (L2T_BROADCAST_BC)", stream->header_dw, stream->words);
							}
							ui->add_field(ui, ib_addr + 0, ib_vmid, "VIDEOCOPY", (stream->header_dw >> 26) & 0x1, NULL, 10);
							ui->add_field(ui, ib_addr + 0, ib_vmid, "BROADCAST", (stream->header_dw >> 27) & 0x1, NULL, 10);
							ui->add_field(ui, ib_addr + 0, ib_vmid, "DETILE", stream->header_dw >> 31, NULL, 16);
							ui->add_field(ui, ib_addr + 4, ib_vmid, "TILED_ADDR0_LO", stream->words[0], NULL, 16);
							ui->add_field(ui, ib_addr + 8, ib_vmid, "TILED_ADDR0_HI", stream->words[1], NULL, 16);
							ui->add_field(ui, ib_addr + 12, ib_vmid, "TILED_ADDR1_LO", stream->words[2], NULL, 16);
							ui->add_field(ui, ib_addr + 16, ib_vmid, "TILED_ADDR1_HI", stream->words[3], NULL, 16);
							ui->add_field(ui, ib_addr + 20, ib_vmid, "WIDTH", stream->words[4] & 0x3FFF, NULL, 10);
							ui->add_field(ui, ib_addr + 24, ib_vmid, "HEIGHT", stream->words[5] & 0x3FFF, NULL, 10);
							ui->add_field(ui, ib_addr + 24, ib_vmid, "DEPTH", (stream->words[5] >> 16) & 0x1FFF, NULL, 10);
							ui->add_field(ui, ib_addr + 28, ib_vmid, "ELEMENT_SIZE", stream->words[6] & 0x7, NULL, 10);
							ui->add_field(ui, ib_addr + 28, ib_vmid, "ARRAY_MODE", (stream->words[6] >> 3) & 0xF, NULL, 10);
							ui->add_field(ui, ib_addr + 28, ib_vmid, "MIT_MODE", (stream->words[6] >> 8) & 0x7, NULL, 10);
							ui->add_field(ui, ib_addr + 28, ib_vmid, "TILESPLIT_SIZE", (stream->words[6] >> 11) & 0x7, NULL, 10);
							ui->add_field(ui, ib_addr + 28, ib_vmid, "BANK_W", (stream->words[6] >> 15) & 0x3, NULL, 10);
							ui->add_field(ui, ib_addr + 28, ib_vmid, "BANK_H", (stream->words[6] >> 18) & 0x3, NULL, 10);
							ui->add_field(ui, ib_addr + 28, ib_vmid, "NUM_BANK", (stream->words[6] >> 21) & 0x3, NULL, 10);
							ui->add_field(ui, ib_addr + 28, ib_vmid, "MAT_ASPT", (stream->words[6] >> 24) & 0x3, NULL, 10);
							ui->add_field(ui, ib_addr + 28, ib_vmid, "PIPE_CONFIG", (stream->words[6] >> 26) & 0x1F, NULL, 10);
							ui->add_field(ui, ib_addr + 32, ib_vmid, "X", stream->words[7] & 0x3FFF, NULL, 10);
							ui->add_field(ui, ib_addr + 32, ib_vmid, "Y", (stream->words[7] >> 16) & 0x3FFF, NULL, 10);
							ui->add_field(ui, ib_addr + 36, ib_vmid, "Z", stream->words[8] & 0x7FF, NULL, 10);
							ui->add_field(ui, ib_addr + 40, ib_vmid, "DST2_SW", (stream->words[9] >> 8) & 0x3, NULL, 10);
							ui->add_field(ui, ib_addr + 40, ib_vmid, "LINEAR_SW", (stream->words[9] >> 16) & 0x3, NULL, 10);
							ui->add_field(ui, ib_addr + 40, ib_vmid, "TILE_SW", (stream->words[9] >> 24) & 0x3, NULL, 10);
							ui->add_field(ui, ib_addr + 44, ib_vmid, "LINEAR_ADDR_LO", stream->words[10], NULL, 16);
							ui->add_field(ui, ib_addr + 48, ib_vmid, "LINEAR_ADDR_HI", stream->words[11], NULL, 16);
							ui->add_field(ui, ib_addr + 52, ib_vmid, "LINEAR_PITCH", stream->words[12] & 0x7FFFF, NULL, 10);
							ui->add_field(ui, ib_addr + 56, ib_vmid, "LINEAR_SLICE_PITCH", stream->words[13], NULL, 10);
							ui->add_field(ui, ib_addr + 60, ib_vmid, "COUNT", stream->words[14] & 0x3FFFFF, NULL, 10);
						} else {
							ui->start_opcode(ui, ib_addr, ib_vmid, stream->opcode, stream->sub_opcode, stream->nwords + 1, "COPY (TILED_BC)", stream->header_dw, stream->words);
							ui->add_field(ui, ib_addr + 0, ib_vmid, "VIDEOCOPY", (stream->header_dw >> 26) & 0x1, NULL, 10);
							ui->add_field(ui, ib_addr + 0, ib_vmid, "BROADCAST", (stream->header_dw >> 27) & 0x1, NULL, 10);
							ui->add_field(ui, ib_addr + 0, ib_vmid, "DETILE", stream->header_dw >> 31, NULL, 16);
							ui->add_field(ui, ib_addr + 4, ib_vmid, "TILED_ADDR_LO", stream->words[0], NULL, 16);
							ui->add_field(ui, ib_addr + 8, ib_vmid, "TILED_ADDR_HI", stream->words[1], NULL, 16);
							ui->add_field(ui, ib_addr + 12, ib_vmid, "WIDTH", (stream->words[2] >> 0) & 0x3FFF, NULL, 10);
							ui->add_field(ui, ib_addr + 16, ib_vmid, "HEIGHT", (stream->words[3] >> 0) & 0x3FFF, NULL, 10);
							ui->add_field(ui, ib_addr + 16, ib_vmid, "DEPTH", (stream->words[3] >> 16) & 0x7FF, NULL, 10);

							ui->add_field(ui, ib_addr + 20, ib_vmid, "ELEMENT_SIZE", (stream->words[4] >> 0) & 0x7, NULL, 10);
							ui->add_field(ui, ib_addr + 20, ib_vmid, "ARRAY_MODE", (stream->words[4] >> 3) & 0xF, NULL, 10);
							ui->add_field(ui, ib_addr + 20, ib_vmid, "MIT_MODE", (stream->words[4] >> 8) & 0x7, NULL, 10);
							ui->add_field(ui, ib_addr + 20, ib_vmid, "TIMESPLIT_SIZE", (stream->words[4] >> 11) & 0x7, NULL, 10);
							ui->add_field(ui, ib_addr + 20, ib_vmid, "BANK_W", (stream->words[4] >> 15) & 0x3, NULL, 10);
							ui->add_field(ui, ib_addr + 20, ib_vmid, "BANK_H", (stream->words[4] >> 18) & 0x3, NULL, 10);
							ui->add_field(ui, ib_addr + 20, ib_vmid, "NUM_BANK", (stream->words[4] >> 21) & 0x3, NULL, 10);
							ui->add_field(ui, ib_addr + 20, ib_vmid, "MAT_ASPT", (stream->words[4] >> 24) & 0x3, NULL, 10);
							ui->add_field(ui, ib_addr + 20, ib_vmid, "PIPE_CONFIG", (stream->words[4] >> 26) & 0x1F, NULL, 10);

							ui->add_field(ui, ib_addr + 24, ib_vmid, "X", (stream->words[5] >> 0) & 0x3FFF, NULL, 10);
							ui->add_field(ui, ib_addr + 24, ib_vmid, "Y", (stream->words[5] >> 16) & 0x3FFF, NULL, 10);
							ui->add_field(ui, ib_addr + 28, ib_vmid, "Z", (stream->words[6] >> 0) & 0x7FF, NULL, 10);
							ui->add_field(ui, ib_addr + 28, ib_vmid, "LINEAR_SW", (stream->words[6] >> 16) & 0x3, NULL, 10);
							ui->add_field(ui, ib_addr + 28, ib_vmid, "TILE_SW", (stream->words[6] >> 24) & 0x3, NULL, 10);

							ui->add_field(ui, ib_addr + 32, ib_vmid, "LINEAR_ADDR_LO", stream->words[7], NULL, 16);
							ui->add_field(ui, ib_addr + 36, ib_vmid, "LINEAR_ADDR_HI", stream->words[8], NULL, 16);

							ui->add_field(ui, ib_addr + 40, ib_vmid, "LINEAR_PITCH", (stream->words[9] >> 0) & 0x7FFFF, NULL, 10);
							ui->add_field(ui, ib_addr + 44, ib_vmid, "COUNT", (stream->words[10] >> 2) & 0xFFFFF, NULL, 10);
						}
						break;
					case 20: // LINEAR_SUB_WINDOW_BC
						ui->start_opcode(ui, ib_addr, ib_vmid, stream->opcode, stream->sub_opcode, stream->nwords + 1, "COPY (LINEAR_SUB_WINDOW_BC)", stream->header_dw, stream->words);
						ui->add_field(ui, ib_addr + 0, ib_vmid, "ELEMENTSIZE", (stream->header_dw >> 29) & 0x7, NULL, 10);
						ui->add_field(ui, ib_addr + 4, ib_vmid, "SRC_ADDR_LO", stream->words[0], NULL, 16);
						ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_ADDR_HI", stream->words[1], NULL, 16);
						ui->add_field(ui, ib_addr + 12, ib_vmid, "SRC_X", (stream->words[2] >> 0) & 0x3FFF, NULL, 10);
						ui->add_field(ui, ib_addr + 12, ib_vmid, "SRC_Y", (stream->words[2] >> 16) & 0x3FFF, NULL, 10);
						ui->add_field(ui, ib_addr + 16, ib_vmid, "SRC_Z", (stream->words[3] >> 0) & 0x7FF, NULL, 10);
						ui->add_field(ui, ib_addr + 16, ib_vmid, "SRC_PITCH", (stream->words[3] >> 13) & 0x3FFF, NULL, 10);
						ui->add_field(ui, ib_addr + 20, ib_vmid, "SRC_SLICE_PITCH", (stream->words[4] >> 0) & 0xFFFFFFF, NULL, 10);

						ui->add_field(ui, ib_addr + 24, ib_vmid, "DST_ADDR_LO", stream->words[5], NULL, 16);
						ui->add_field(ui, ib_addr + 28, ib_vmid, "DST_ADDR_HI", stream->words[6], NULL, 16);
						ui->add_field(ui, ib_addr + 32, ib_vmid, "DST_X", (stream->words[7] >> 0) & 0x3FFF, NULL, 10);
						ui->add_field(ui, ib_addr + 32, ib_vmid, "DST_Y", (stream->words[7] >> 16) & 0x3FFF, NULL, 10);
						ui->add_field(ui, ib_addr + 36, ib_vmid, "DST_Z", (stream->words[8] >> 0) & 0x7FF, NULL, 10);
						ui->add_field(ui, ib_addr + 36, ib_vmid, "DST_PITCH", (stream->words[8] >> 13) & 0x3FFF, NULL, 10);
						ui->add_field(ui, ib_addr + 40, ib_vmid, "DST_SLICE_PITCH", (stream->words[9] >> 0) & 0xFFFFFFF, NULL, 10);

						ui->add_field(ui, ib_addr + 44, ib_vmid, "RECT_X", (stream->words[10] >> 0) & 0x3FFF, NULL, 10);
						ui->add_field(ui, ib_addr + 44, ib_vmid, "RECT_Y", (stream->words[10] >> 16) & 0x3FFF, NULL, 10);
						ui->add_field(ui, ib_addr + 48, ib_vmid, "RECT_Z", (stream->words[11] >> 0) & 0x7FF, NULL, 10);
						ui->add_field(ui, ib_addr + 48, ib_vmid, "DST_SW", (stream->words[11] >> 16) & 0x3, NULL, 10);
						ui->add_field(ui, ib_addr + 48, ib_vmid, "DST_HA", (stream->words[11] >> 19) & 0x1, NULL, 10);
						ui->add_field(ui, ib_addr + 48, ib_vmid, "SRC_SW", (stream->words[11] >> 24) & 0x3, NULL, 10);
						ui->add_field(ui, ib_addr + 48, ib_vmid, "SRC_HA", (stream->words[11] >> 27) & 0x1, NULL, 10);
						break;
					case 21: // TILED_SUB_WINDOW_BC
						ui->start_opcode(ui, ib_addr, ib_vmid, stream->opcode, stream->sub_opcode, stream->nwords + 1, "COPY (TILED_SUB_WINDOW_BC)", stream->header_dw, stream->words);
						ui->add_field(ui, ib_addr + 0, ib_vmid, "DETILE", stream->header_dw >> 31, NULL, 16);
						ui->add_field(ui, ib_addr + 4, ib_vmid, "TILED_ADDR_LO", stream->words[0], NULL, 16);
						ui->add_field(ui, ib_addr + 8, ib_vmid, "TILED_ADDR_HI", stream->words[1], NULL, 16);
						ui->add_field(ui, ib_addr + 12, ib_vmid, "TILED_X", (stream->words[2] >> 0) & 0x3FFF, NULL, 10);
						ui->add_field(ui, ib_addr + 12, ib_vmid, "TILED_Y", (stream->words[2] >> 16) & 0x3FFF, NULL, 10);
						ui->add_field(ui, ib_addr + 16, ib_vmid, "TILED_Z", (stream->words[3] >> 0) & 0x7FF, NULL, 10);
						ui->add_field(ui, ib_addr + 16, ib_vmid, "WIDTH", (stream->words[3] >> 16) & 0x3FFF, NULL, 10);
						ui->add_field(ui, ib_addr + 20, ib_vmid, "HEIGHT", (stream->words[3] >> 0) & 0x3FFF, NULL, 10);
						ui->add_field(ui, ib_addr + 20, ib_vmid, "DEPTH", (stream->words[3] >> 16) & 0x7FF, NULL, 10);

						ui->add_field(ui, ib_addr + 24, ib_vmid, "ELEMENT_SIZE", (stream->words[5] >> 0) & 0x7, NULL, 10);
						ui->add_field(ui, ib_addr + 24, ib_vmid, "ARRAY_MODE", (stream->words[5] >> 3) & 0xF, NULL, 10);
						ui->add_field(ui, ib_addr + 24, ib_vmid, "MIT_MODE", (stream->words[5] >> 8) & 0x7, NULL, 10);
						ui->add_field(ui, ib_addr + 24, ib_vmid, "TILESPLIT_SIZE", (stream->words[5] >> 11) & 0x7, NULL, 10);
						ui->add_field(ui, ib_addr + 24, ib_vmid, "BANK_W", (stream->words[5] >> 15) & 0x3, NULL, 10);
						ui->add_field(ui, ib_addr + 24, ib_vmid, "BANK_H", (stream->words[5] >> 18) & 0x3, NULL, 10);
						ui->add_field(ui, ib_addr + 24, ib_vmid, "NUM_BANK", (stream->words[5] >> 21) & 0x3, NULL, 10);
						ui->add_field(ui, ib_addr + 24, ib_vmid, "MAT_ASPT", (stream->words[5] >> 24) & 0x3, NULL, 10);
						ui->add_field(ui, ib_addr + 24, ib_vmid, "PIPE_CONFIG", (stream->words[5] >> 26) & 0x1F, NULL, 10);

						ui->add_field(ui, ib_addr + 28, ib_vmid, "LINEAR_ADDR_LO", stream->words[6], NULL, 16);
						ui->add_field(ui, ib_addr + 32, ib_vmid, "LINEAR_ADDR_HI", stream->words[7], NULL, 16);

						ui->add_field(ui, ib_addr + 36, ib_vmid, "LINEAR_X", (stream->words[8] >> 0) & 0x3FFF, NULL, 10);
						ui->add_field(ui, ib_addr + 36, ib_vmid, "LINEAR_Y", (stream->words[8] >> 0) & 0x3FFF, NULL, 10);
						ui->add_field(ui, ib_addr + 40, ib_vmid, "LINEAR_Z", (stream->words[9] >> 0) & 0x7FF, NULL, 10);
						ui->add_field(ui, ib_addr + 40, ib_vmid, "LINEAR_PITCH", (stream->words[9] >> 16) & 0x3FFF, NULL, 10);
						ui->add_field(ui, ib_addr + 44, ib_vmid, "LINEAR_SLICE_PITCH", stream->words[10] & 0xFFFFFFF, NULL, 10);

						ui->add_field(ui, ib_addr + 48, ib_vmid, "RECT_X", (stream->words[11] >> 0) & 0x3FFF, NULL, 10);
						ui->add_field(ui, ib_addr + 48, ib_vmid, "RECT_Y", (stream->words[11] >> 16) & 0x3FFF, NULL, 10);
						ui->add_field(ui, ib_addr + 52, ib_vmid, "RECT_Z", (stream->words[12] >> 0) & 0x7FF, NULL, 10);
						ui->add_field(ui, ib_addr + 52, ib_vmid, "LINEAR_SW", (stream->words[12] >> 16) & 0x3, NULL, 10);
						ui->add_field(ui, ib_addr + 52, ib_vmid, "TILE_SW", (stream->words[12] >> 24) & 0x3, NULL, 10);
						break;
					case 22: // T2T_SUB_WINDOW_BC
						ui->start_opcode(ui, ib_addr, ib_vmid, stream->opcode, stream->sub_opcode, stream->nwords + 1, "COPY (T2T_SUB_WINDOW_BC)", stream->header_dw, stream->words);
						ui->add_field(ui, ib_addr + 4, ib_vmid, "SRC_ADDR_LO", stream->words[0], NULL, 16);
						ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_ADDR_HI", stream->words[1], NULL, 16);
						ui->add_field(ui, ib_addr + 12, ib_vmid, "SRC_X", (stream->words[2] >> 0) & 0x3FFF, NULL, 10);
						ui->add_field(ui, ib_addr + 12, ib_vmid, "SRC_Y", (stream->words[2] >> 16) & 0x3FFF, NULL, 10);
						ui->add_field(ui, ib_addr + 16, ib_vmid, "SRC_Z", (stream->words[3] >> 0) & 0x7FF, NULL, 10);
						ui->add_field(ui, ib_addr + 16, ib_vmid, "SRC_WIDTH", (stream->words[3] >> 16) & 0x3FFF, NULL, 10);
						ui->add_field(ui, ib_addr + 20, ib_vmid, "SRC_HEIGHT", (stream->words[4] >> 0) & 0x3FFF, NULL, 10);
						ui->add_field(ui, ib_addr + 20, ib_vmid, "SRC_DEPTH", (stream->words[4] >> 16) & 0x7FF, NULL, 10);

						ui->add_field(ui, ib_addr + 24, ib_vmid, "SRC_ELEMENT_SIZE", (stream->words[5] >> 0) & 0x7, NULL, 10);
						ui->add_field(ui, ib_addr + 24, ib_vmid, "SRC_ARRAY_MODE", (stream->words[5] >> 3) & 0xF, NULL, 10);
						ui->add_field(ui, ib_addr + 24, ib_vmid, "SRC_MIT_MODE", (stream->words[5] >> 8) & 0x7, NULL, 10);
						ui->add_field(ui, ib_addr + 24, ib_vmid, "SRC_TILESPLIT_SIZE", (stream->words[5] >> 11) & 0x7, NULL, 10);
						ui->add_field(ui, ib_addr + 24, ib_vmid, "SRC_BANK_W", (stream->words[5] >> 15) & 0x3, NULL, 10);
						ui->add_field(ui, ib_addr + 24, ib_vmid, "SRC_BANK_H", (stream->words[5] >> 18) & 0x3, NULL, 10);
						ui->add_field(ui, ib_addr + 24, ib_vmid, "SRC_NUM_BANKS", (stream->words[5] >> 21) & 0x3, NULL, 10);
						ui->add_field(ui, ib_addr + 24, ib_vmid, "SRC_MAT_ASPT", (stream->words[5] >> 24) & 0x3, NULL, 10);
						ui->add_field(ui, ib_addr + 24, ib_vmid, "SRC_PIPE_CONFIG", (stream->words[5] >> 26) & 0x1F, NULL, 10);

						ui->add_field(ui, ib_addr + 28, ib_vmid, "DST_ADDR_LO", stream->words[6], NULL, 16);
						ui->add_field(ui, ib_addr + 32, ib_vmid, "DST_ADDR_HI", stream->words[7], NULL, 16);
						ui->add_field(ui, ib_addr + 36, ib_vmid, "DST_X", (stream->words[8] >> 0) & 0x3FFF, NULL, 10);
						ui->add_field(ui, ib_addr + 36, ib_vmid, "DST_Y", (stream->words[8] >> 16) & 0x3FFF, NULL, 10);
						ui->add_field(ui, ib_addr + 40, ib_vmid, "DST_Z", (stream->words[9] >> 0) & 0x7FF, NULL, 10);
						ui->add_field(ui, ib_addr + 40, ib_vmid, "SRC_WIDTH", (stream->words[9] >> 16) & 0x3FFF, NULL, 10);
						ui->add_field(ui, ib_addr + 44, ib_vmid, "SRC_HEIGHT", (stream->words[10] >> 0) & 0x3FFF, NULL, 10);
						ui->add_field(ui, ib_addr + 44, ib_vmid, "SRC_DEPTH", (stream->words[10] >> 16) & 0xFFF, NULL, 10);

						ui->add_field(ui, ib_addr + 48, ib_vmid, "DST_ELEMENT_SIZE", (stream->words[11] >> 0) & 0x7, NULL, 10);
						ui->add_field(ui, ib_addr + 48, ib_vmid, "DST_ARRAY_MODE", (stream->words[11] >> 3) & 0xF, NULL, 10);
						ui->add_field(ui, ib_addr + 48, ib_vmid, "DST_MIT_MODE", (stream->words[11] >> 8) & 0x7, NULL, 10);
						ui->add_field(ui, ib_addr + 48, ib_vmid, "DST_TILESPLIT_SIZE", (stream->words[11] >> 11) & 0x7, NULL, 10);
						ui->add_field(ui, ib_addr + 48, ib_vmid, "DST_BANK_W", (stream->words[11] >> 15) & 0x3, NULL, 10);
						ui->add_field(ui, ib_addr + 48, ib_vmid, "DST_BANK_H", (stream->words[11] >> 18) & 0x3, NULL, 10);
						ui->add_field(ui, ib_addr + 48, ib_vmid, "DST_NUM_BANK", (stream->words[11] >> 21) & 0x3, NULL, 10);
						ui->add_field(ui, ib_addr + 48, ib_vmid, "DST_MAT_ASPT", (stream->words[11] >> 24) & 0x3, NULL, 10);
						ui->add_field(ui, ib_addr + 48, ib_vmid, "DST_PIPE_CONFIG", (stream->words[11] >> 26) & 0x1F, NULL, 10);

						ui->add_field(ui, ib_addr + 52, ib_vmid, "RECT_X", (stream->words[12] >> 0) & 0x3FFF, NULL, 10);
						ui->add_field(ui, ib_addr + 52, ib_vmid, "RECT_Y", (stream->words[12] >> 16) & 0x3FFF, NULL, 10);
						ui->add_field(ui, ib_addr + 56, ib_vmid, "RECT_Z", (stream->words[13] >> 0) & 0x7FF, NULL, 10);
						ui->add_field(ui, ib_addr + 56, ib_vmid, "DST_SW", (stream->words[13] >> 16) & 0x3, NULL, 10);
						ui->add_field(ui, ib_addr + 56, ib_vmid, "SRC_SW", (stream->words[13] >> 22) & 0x3, NULL, 10);
						break;
					case 36: // SUBWIN_LARGE
						ui->start_opcode(ui, ib_addr, ib_vmid, stream->opcode, stream->sub_opcode, stream->nwords + 1, "COPY (SUBWIN_LARGE)", stream->header_dw, stream->words);
						ui->add_field(ui, ib_addr + 0, ib_vmid, "TMZ", (stream->header_dw >> 18) & 0x1, NULL, 10);
						ui->add_field(ui, ib_addr + 0, ib_vmid, "CPV", (stream->header_dw >> 19) & 0x1, NULL, 10);
						ui->add_field(ui, ib_addr + 4, ib_vmid, "SRC_ADDR_LO", stream->words[0], NULL, 16);
						ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_ADDR_HI", stream->words[1], NULL, 16);
						ui->add_field(ui, ib_addr + 12, ib_vmid, "SRC_X", stream->words[2], NULL, 10);
						ui->add_field(ui, ib_addr + 16, ib_vmid, "SRC_Y", stream->words[3], NULL, 10);
						ui->add_field(ui, ib_addr + 20, ib_vmid, "SRC_Z", stream->words[4], NULL, 10);
						ui->add_field(ui, ib_addr + 24, ib_vmid, "SRC_PITCH", stream->words[5], NULL, 10);
						ui->add_field(ui, ib_addr + 28, ib_vmid, "SRC_SLICE_PITCH_LO", stream->words[6], NULL, 10);
						ui->add_field(ui, ib_addr + 32, ib_vmid, "SRC_SLICE_PITCH_47_32", stream->words[7] & 0xFFFF, NULL, 10);
						ui->add_field(ui, ib_addr + 36, ib_vmid, "DST_ADDR_LO", stream->words[8], NULL, 16);
						ui->add_field(ui, ib_addr + 40, ib_vmid, "DST_ADDR_HI", stream->words[9], NULL, 16);
						ui->add_field(ui, ib_addr + 44, ib_vmid, "DST_X", stream->words[10], NULL, 10);
						ui->add_field(ui, ib_addr + 48, ib_vmid, "DST_Y", stream->words[11], NULL, 10);
						ui->add_field(ui, ib_addr + 52, ib_vmid, "DST_Z", stream->words[12], NULL, 10);
						ui->add_field(ui, ib_addr + 56, ib_vmid, "DST_PITCH", stream->words[13], NULL, 10);
						ui->add_field(ui, ib_addr + 60, ib_vmid, "DST_SLICE_PITCH_LO", stream->words[14], NULL, 10);
						ui->add_field(ui, ib_addr + 64, ib_vmid, "DST_SLICE_PITCH_47_32", stream->words[15] & 0xFFFF, NULL, 10);
						ui->add_field(ui, ib_addr + 64, ib_vmid, "DST_SW", (stream->words[15] >> 16) & 0x3, NULL, 10);
						ui->add_field(ui, ib_addr + 64, ib_vmid, "DST_POLICY", (stream->words[15] >> 18) & 0x7, NULL, 10);
						ui->add_field(ui, ib_addr + 64, ib_vmid, "SRC_SW", (stream->words[15] >> 24) & 0x3, NULL, 10);
						ui->add_field(ui, ib_addr + 64, ib_vmid, "SRC_POLICY", (stream->words[15] >> 26) & 0x7, NULL, 10);
						ui->add_field(ui, ib_addr + 68, ib_vmid, "RECT_X", stream->words[16], NULL, 10);
						ui->add_field(ui, ib_addr + 72, ib_vmid, "RECT_Y", stream->words[17], NULL, 10);
						ui->add_field(ui, ib_addr + 76, ib_vmid, "RECT_Z", stream->words[18], NULL, 10);
						break;
					default:
						if (ui->unhandled_subop)
							ui->unhandled_subop(ui, asic, ib_addr, ib_vmid, stream);
						break;
				}
				break;
			case 2: // WRITE
				switch (stream->sub_opcode) {
					case 0: // LINEAR
						ui->start_opcode(ui, ib_addr, ib_vmid, stream->opcode, stream->sub_opcode, stream->nwords + 1, "WRITE (LINEAR)", stream->header_dw, stream->words);
						ui->add_field(ui, ib_addr + 0, ib_vmid, "ENCRYPT", (stream->header_dw >> 16) & 0x1, NULL, 10);
						ui->add_field(ui, ib_addr + 0, ib_vmid, "TMZ", (stream->header_dw >> 18) & 0x1, NULL, 10);
						if (has_cpv_flag) {
							ui->add_field(ui, ib_addr + 0, ib_vmid, "CPV", (stream->header_dw >> 28) & 0x1, NULL, 10);
						}
						ui->add_field(ui, ib_addr + 4, ib_vmid, "DST_ADDR_LO", stream->words[0], NULL, 16);
						ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_ADDR_HI", stream->words[1], NULL, 16);
						ui->add_field(ui, ib_addr + 12, ib_vmid, "COUNT", stream->words[2], NULL, 10);
						ui->add_field(ui, ib_addr + 12, ib_vmid, "SWAP", (stream->words[2] >> 24) & 0x3, NULL, 10);
						if (has_cp_fields) {
							ui->add_field(ui, ib_addr + 12, ib_vmid, "CACHE_POLICY", (stream->words[2] >> 26) & 0x7, NULL, 10);
						}
						for (n = 3; n < stream->nwords; n++) {
							uint32_t data_idx = n - 3;
							sprintf(str_buf, "DATA_%"PRIu32, data_idx);
							ui->add_field(ui, ib_addr + 16 + 4 * (n - 3), ib_vmid, str_buf, stream->words[n], NULL, 16);
						}
						break;
					case 1: // TILED
						ui->start_opcode(ui, ib_addr, ib_vmid, stream->opcode, stream->sub_opcode, stream->nwords + 1, "WRITE (TILED)", stream->header_dw, stream->words);
						if (ver_maj >= ver_ai) {
							ui->add_field(ui, ib_addr + 0, ib_vmid, "ENCRYPT", (stream->header_dw >> 16) & 0x1, NULL, 10);
							ui->add_field(ui, ib_addr + 0, ib_vmid, "TMZ", (stream->header_dw >> 18) & 0x1, NULL, 10);
						}
						if (has_cpv_flag) {
							ui->add_field(ui, ib_addr + 0, ib_vmid, "CPV", (stream->header_dw >> 28) & 0x1, NULL, 10);
						}
						ui->add_field(ui, ib_addr + 4, ib_vmid, "DST_ADDR_LO", stream->words[0], NULL, 16);
						ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_ADDR_HI", stream->words[1], NULL, 16);
						if (ver_maj <= ver_vi) {
							ui->add_field(ui, ib_addr + 12, ib_vmid, "PITCH_IN_TILE", (stream->words[2] >> 0) & 0x7FF, NULL, 10);
							ui->add_field(ui, ib_addr + 12, ib_vmid, "HEIGHT", (stream->words[2] >> 16) & 0x3FFF, NULL, 10);
							ui->add_field(ui, ib_addr + 16, ib_vmid, "SLICE_PITCH", (stream->words[3] >> 0) & 0x3FFFFF, NULL, 10);
							ui->add_field(ui, ib_addr + 20, ib_vmid, "ELEMENT_SIZE", (stream->words[4] >> 0) & 0x7, NULL, 10);
							ui->add_field(ui, ib_addr + 20, ib_vmid, "ARRAY_MODE", (stream->words[4] >> 3) & 0xF, NULL, 10);
							ui->add_field(ui, ib_addr + 20, ib_vmid, "MIT_MODE", (stream->words[4] >> 8) & 0x7, NULL, 10);
							ui->add_field(ui, ib_addr + 20, ib_vmid, "TILESPLIT_SIZE", (stream->words[4] >> 11) & 0x7, NULL, 10);
							ui->add_field(ui, ib_addr + 20, ib_vmid, "BANK_W", (stream->words[4] >> 15) & 0x3, NULL, 10);
							ui->add_field(ui, ib_addr + 20, ib_vmid, "BANK_H", (stream->words[4] >> 18) & 0x3, NULL, 10);
							ui->add_field(ui, ib_addr + 20, ib_vmid, "NUM_BANK", (stream->words[4] >> 21) & 0x3, NULL, 10);
							ui->add_field(ui, ib_addr + 20, ib_vmid, "MAT_ASPT", (stream->words[4] >> 24) & 0x3, NULL, 10);
							ui->add_field(ui, ib_addr + 20, ib_vmid, "PIPE_CONFIG", (stream->words[4] >> 26) & 0x1F, NULL, 10);
						} else {
							ui->add_field(ui, ib_addr + 12, ib_vmid, "WIDTH", (stream->words[2] >> 16) & 0x3FFF, NULL, 10);
							ui->add_field(ui, ib_addr + 16, ib_vmid, "HEIGHT", (stream->words[3] >> 0) & 0x3FFF, NULL, 10);
							ui->add_field(ui, ib_addr + 16, ib_vmid, "DEPTH", (stream->words[3] >> 16) & z_mask, NULL, 10);
							ui->add_field(ui, ib_addr + 20, ib_vmid, "ELEMENT_SIZE", (stream->words[4] >> 0) & 0x7, NULL, 10);
							ui->add_field(ui, ib_addr + 20, ib_vmid, "SWIZZLE_MODE", (stream->words[4] >> 3) & 0x1F, NULL, 10);
							ui->add_field(ui, ib_addr + 20, ib_vmid, "DIMENSION", (stream->words[4] >> 9) & 0x3, NULL, 10);
							if (ver_maj == ver_ai) {
								ui->add_field(ui, ib_addr + 20, ib_vmid, "EPITCH", (stream->words[4] >> 16) & 0xFFFF, NULL, 10);
							} else {
								ui->add_field(ui, ib_addr + 20, ib_vmid, "MIP_MAX", (stream->words[4] >> 16) & 0xF, NULL, 10);
							}
						}
						ui->add_field(ui, ib_addr + 24, ib_vmid, "X", (stream->words[5] >> 0) & 0x3FFF, NULL, 10);
						ui->add_field(ui, ib_addr + 24, ib_vmid, "Y", (stream->words[5] >> 16) & 0x3FFF, NULL, 10);
						ui->add_field(ui, ib_addr + 28, ib_vmid, "Z", (stream->words[6] >> 0) & z_mask, NULL, 10);
						ui->add_field(ui, ib_addr + 28, ib_vmid, "SW", (stream->words[6] >> 24) & 0x3, NULL, 10);
						if (has_cp_fields) {
							ui->add_field(ui, ib_addr + 28, ib_vmid, "CACHE_POLICY", (stream->words[6] >> 26) & 0x7, NULL, 10);
						}
						ui->add_field(ui, ib_addr + 32, ib_vmid, "COUNT", (stream->words[7] >> 0) & 0xFFFFF, NULL, 10);
						for (n = 8; n < stream->nwords; n++) {
							uint32_t data_idx = n - 8;
							sprintf(str_buf, "DATA_%"PRIu32, data_idx);
							ui->add_field(ui, ib_addr + 36 + 4 * (n - 8), ib_vmid, str_buf, stream->words[n], NULL, 16);
						}
						break;
					case 17: // TILED_BC
						ui->start_opcode(ui, ib_addr, ib_vmid, stream->opcode, stream->sub_opcode, stream->nwords + 1, "WRITE (TILED_BC)", stream->header_dw, stream->words);
						ui->add_field(ui, ib_addr + 4, ib_vmid, "DST_ADDR_LO", stream->words[0], NULL, 16);
						ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_ADDR_HI", stream->words[1], NULL, 16);
						ui->add_field(ui, ib_addr + 12, ib_vmid, "WIDTH", (stream->words[2] >> 0) & 0x3FFF, NULL, 10);
						ui->add_field(ui, ib_addr + 16, ib_vmid, "HEIGHT", (stream->words[3] >> 0) & 0x3FFF, NULL, 10);
						ui->add_field(ui, ib_addr + 16, ib_vmid, "DEPTH", (stream->words[3] >> 16) & 0x7FF, NULL, 10);
						ui->add_field(ui, ib_addr + 20, ib_vmid, "ELEMENT_SIZE", (stream->words[4] >> 0) & 0x7, NULL, 10);
						ui->add_field(ui, ib_addr + 20, ib_vmid, "ARRAY_MODE", (stream->words[4] >> 3) & 0xF, NULL, 10);
						ui->add_field(ui, ib_addr + 20, ib_vmid, "MIT_MODE", (stream->words[4] >> 8) & 0x7, NULL, 10);
						ui->add_field(ui, ib_addr + 20, ib_vmid, "TILESPLIT_SIZE", (stream->words[4] >> 11) & 0x7, NULL, 10);
						ui->add_field(ui, ib_addr + 20, ib_vmid, "BANK_W", (stream->words[4] >> 15) & 0x3, NULL, 10);
						ui->add_field(ui, ib_addr + 20, ib_vmid, "BANK_H", (stream->words[4] >> 18) & 0x3, NULL, 10);
						ui->add_field(ui, ib_addr + 20, ib_vmid, "NUM_BANK", (stream->words[4] >> 21) & 0x3, NULL, 10);
						ui->add_field(ui, ib_addr + 20, ib_vmid, "MAT_ASPT", (stream->words[4] >> 24) & 0x3, NULL, 10);
						ui->add_field(ui, ib_addr + 20, ib_vmid, "PIPE_CONFIG", (stream->words[4] >> 26) & 0x1F, NULL, 10);
						ui->add_field(ui, ib_addr + 24, ib_vmid, "X", (stream->words[5] >> 0) & 0x3FFF, NULL, 10);
						ui->add_field(ui, ib_addr + 24, ib_vmid, "Y", (stream->words[5] >> 16) & 0x3FFF, NULL, 10);
						ui->add_field(ui, ib_addr + 28, ib_vmid, "Z", (stream->words[6] >> 0) & z_mask, NULL, 10);
						ui->add_field(ui, ib_addr + 28, ib_vmid, "SW", (stream->words[6] >> 24) & 0x3, NULL, 10);
						ui->add_field(ui, ib_addr + 32, ib_vmid, "COUNT", (stream->words[7] >> 0) & 0xFFFFF, NULL, 10);
						for (n = 8; n < stream->nwords; n++) {
							uint32_t data_idx = n - 8;
							sprintf(str_buf, "DATA_%"PRIu32, data_idx);
							ui->add_field(ui, ib_addr + 36 + 4 * (n - 8), ib_vmid, str_buf, stream->words[n], NULL, 16);
						}
						break;
					default:
						if (ui->unhandled_subop)
							ui->unhandled_subop(ui, asic, ib_addr, ib_vmid, stream);
						break;
				}
				break;
			case 4: // INDIRECT
				ui->start_opcode(ui, ib_addr, ib_vmid, stream->opcode, stream->sub_opcode, stream->nwords + 1, "INDIRECT_BUFFER", stream->header_dw, stream->words);
				ui->add_field(ui, ib_addr + 0, ib_vmid, "VMID", (stream->header_dw >> 16) & 0xF, NULL, 16);
				ui->add_field(ui, ib_addr + 0, ib_vmid, "PRIV", (stream->header_dw >> 31) & 0x1, NULL, 10);
				ui->add_field(ui, ib_addr + 4, ib_vmid, "IB_BASE_LO", stream->words[0], NULL, 16);
				ui->add_field(ui, ib_addr + 8, ib_vmid, "IB_BASE_HI", stream->words[1], NULL, 16);
				ui->add_field(ui, ib_addr + 12, ib_vmid, "IB_BASE_SIZE", stream->words[2], NULL, 10);
				ui->add_field(ui, ib_addr + 16, ib_vmid, "IB_CSA_ADDR_LO", stream->words[3], NULL, 16);
				ui->add_field(ui, ib_addr + 20, ib_vmid, "IB_CSA_ADDR_HI", stream->words[4], NULL, 16);
				if (follow && stream->next_ib)
					umr_sdma_decode_stream_opcodes(asic, ui, stream->next_ib, stream->ib.addr, stream->ib.vmid, stream->next_ib->from.addr, stream->next_ib->from.vmid, ~0UL, 1);
				break;
			case 5: // FENCE
				ui->start_opcode(ui, ib_addr, ib_vmid, stream->opcode, stream->sub_opcode, stream->nwords + 1, "FENCE", stream->header_dw, stream->words);
				if (ver_maj >= ver_nv) {
					ui->add_field(ui, ib_addr + 0, ib_vmid, "MTYPE", (stream->header_dw >> 16) & 0x7, NULL, 10);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "GCC", (stream->header_dw >> 19) & 0x1, NULL, 10);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "SYS", (stream->header_dw >> 20) & 0x1, NULL, 10);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "SNP", (stream->header_dw >> 22) & 0x1, NULL, 10);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "GPA", (stream->header_dw >> 23) & 0x1, NULL, 10);
				}
				if (has_cp_fields) {
					ui->add_field(ui, ib_addr + 0, ib_vmid, "L2_POLICY", (stream->header_dw >> 24) & 0x3, NULL, 10);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "LLC_POLICY", (stream->header_dw >> 26) & 0x1, NULL, 10);
				}
				if (has_cpv_flag) {
					ui->add_field(ui, ib_addr + 0, ib_vmid, "CPV", (stream->header_dw >> 28) & 0x1, NULL, 10);
				}
				ui->add_field(ui, ib_addr + 4, ib_vmid, "FENCE_ADDR_LO", stream->words[0], NULL, 16);
				ui->add_field(ui, ib_addr + 8, ib_vmid, "FENCE_ADDR_HI", stream->words[1], NULL, 16);
				ui->add_field(ui, ib_addr + 12, ib_vmid, "FENCE_DATA", stream->words[2], NULL, 16);
				break;
			case 6: // TRAP
				ui->start_opcode(ui, ib_addr, ib_vmid, stream->opcode, stream->sub_opcode, stream->nwords + 1, "TRAP", stream->header_dw, stream->words);
				ui->add_field(ui, ib_addr + 4, ib_vmid, "TRAP_INT_CONTEXT", stream->words[0] & 0xFFFFFF, NULL, 16);
				break;
			case 7: // SEM
				switch (stream->sub_opcode) {
					case 0: // SEM
						ui->start_opcode(ui, ib_addr, ib_vmid, stream->opcode, stream->sub_opcode, stream->nwords + 1, "SEM", stream->header_dw, stream->words);
						ui->add_field(ui, ib_addr + 0, ib_vmid, "WRITE_ONE", (stream->header_dw >> 29) & 1, NULL, 16);
						ui->add_field(ui, ib_addr + 0, ib_vmid, "SIGNAL", (stream->header_dw >> 30) & 1, NULL, 16);
						ui->add_field(ui, ib_addr + 0, ib_vmid, "MAILBOX", (stream->header_dw >> 31) & 1, NULL, 16);
						ui->add_field(ui, ib_addr + 4, ib_vmid, "SEMAPHORE_ADDR_LO", stream->words[0], NULL, 16);
						ui->add_field(ui, ib_addr + 8, ib_vmid, "SEMAPHORE_ADDR_HI", stream->words[1], NULL, 16);
						break;
					case 1: // MEM_INCR
						ui->start_opcode(ui, ib_addr, ib_vmid, stream->opcode, stream->sub_opcode, stream->nwords + 1, "SEM (MEM_INCR)", stream->header_dw, stream->words);
						if (has_cp_fields) {
							ui->add_field(ui, ib_addr + 0, ib_vmid, "L2_POLICY", (stream->header_dw >> 24) & 0x3, NULL, 10);
							ui->add_field(ui, ib_addr + 0, ib_vmid, "LLC_POLICY", (stream->header_dw >> 26) & 0x1, NULL, 10);
						}
						if (has_cpv_flag) {
							ui->add_field(ui, ib_addr + 0, ib_vmid, "CPV", (stream->header_dw >> 28) & 0x1, NULL, 10);
						}
						ui->add_field(ui, ib_addr + 4, ib_vmid, "ADDR_LO", stream->words[0], NULL, 16);
						ui->add_field(ui, ib_addr + 8, ib_vmid, "ADDR_HI", stream->words[1], NULL, 16);
						break;
					default:
						if (ui->unhandled_subop)
							ui->unhandled_subop(ui, asic, ib_addr, ib_vmid, stream);
						break;
				}

				break;
			case 8: // POLL_REGMEM
				switch (stream->sub_opcode) {
					case 0: // POLL_REGMEM
						ui->start_opcode(ui, ib_addr, ib_vmid, stream->opcode, stream->sub_opcode, stream->nwords + 1, "POLL_REGMEM", stream->header_dw, stream->words);
						if (has_cp_fields) ui->add_field(ui, ib_addr + 0, ib_vmid, "CACHE_POLICY", (stream->header_dw >> 20) & 0x7, NULL, 10);
						if (has_cpv_flag) ui->add_field(ui, ib_addr + 0, ib_vmid, "CPV", (stream->header_dw >> 24) & 0x1, NULL, 10);
						ui->add_field(ui, ib_addr + 0, ib_vmid, "HDP_FLUSH", (stream->header_dw >> 26) & 1, NULL, 16);
						ui->add_field(ui, ib_addr + 0, ib_vmid, "FUNCTION", 0, poll_regmem_funcs[(stream->header_dw >> 28) & 7], 0);
						ui->add_field(ui, ib_addr + 0, ib_vmid, "MEM_POLL", (stream->header_dw >> 31) & 1, NULL, 16);
						if (!(stream->header_dw & (1UL << 31))) {
							ui->add_field(ui, ib_addr + 4, ib_vmid, "REGISTER", BITS(stream->words[0], 2, 20), umr_reg_name(asic, BITS(stream->words[0], 2, 20)), 16);
							if (((stream->header_dw >> 26) & 3) == 1) { // if HDP_FLUSH, the write register is provided
								ui->add_field(ui, ib_addr + 8, ib_vmid, "REGISTER", BITS(stream->words[1], 2, 18), umr_reg_name(asic, BITS(stream->words[1], 2, 18)), 16);
							} else {
								ui->add_field(ui, ib_addr + 8, ib_vmid, NULL, stream->words[1], NULL, 16);
							}
						} else {
							ui->add_field(ui, ib_addr + 4, ib_vmid, "POLL_REGMEM_ADDR_LO", stream->words[0], NULL, 16);
							ui->add_field(ui, ib_addr + 8, ib_vmid, "POLL_REGMEM_ADDR_HI", stream->words[1], NULL, 16);
						}
						ui->add_field(ui, ib_addr + 12, ib_vmid, "VALUE", stream->words[2], NULL, 16);
						ui->add_field(ui, ib_addr + 16, ib_vmid, "MASK", stream->words[3], NULL, 16);
						ui->add_field(ui, ib_addr + 20, ib_vmid, "INTERVAL", stream->words[4] & 0xFFFF, NULL, 10);
						ui->add_field(ui, ib_addr + 20, ib_vmid, "RETRY_COUNT", (stream->words[4] >> 16) & 0xFFF, NULL, 10);
						break;
					case 1: // POLL_REG_WRITE_MEM
						ui->start_opcode(ui, ib_addr, ib_vmid, stream->opcode, stream->sub_opcode, stream->nwords + 1, "POLL_REG_WRITE_MEM", stream->header_dw, stream->words);
						if (has_cp_fields) ui->add_field(ui, ib_addr + 0, ib_vmid, "CACHE_POLICY", (stream->header_dw >> 24) & 0x7, NULL, 10);
						if (has_cpv_flag) ui->add_field(ui, ib_addr + 0, ib_vmid, "CPV", (stream->header_dw >> 28) & 0x1, NULL, 10);
						ui->add_field(ui, ib_addr + 4, ib_vmid, "SRC_ADDR", stream->words[0], NULL, 16);
						ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_ADDR_LO", stream->words[1], NULL, 16);
						ui->add_field(ui, ib_addr + 12, ib_vmid, "DST_ADDR_HI", stream->words[2], NULL, 16);
						break;
					case 2: // POLL_DBIT_WRITE_MEM
						ui->start_opcode(ui, ib_addr, ib_vmid, stream->opcode, stream->sub_opcode, stream->nwords + 1, "POLL_DBIT_WRITE_MEM", stream->header_dw, stream->words);
						ui->add_field(ui, ib_addr + 0, ib_vmid, "EA", (stream->header_dw >> 16) & 0x3, NULL, 10);
						if (has_cp_fields) ui->add_field(ui, ib_addr + 0, ib_vmid, "CACHE_POLICY", (stream->header_dw >> 24) & 0x7, NULL, 10);
						if (has_cpv_flag) ui->add_field(ui, ib_addr + 0, ib_vmid, "CPV", (stream->header_dw >> 28) & 0x1, NULL, 10);
						ui->add_field(ui, ib_addr + 4, ib_vmid, "DST_ADDR_LO", stream->words[0], NULL, 16);
						ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_ADDR_HI", stream->words[1], NULL, 16);
						ui->add_field(ui, ib_addr + 12, ib_vmid, "START_PAGE", (stream->words[2] >> 4) & 0xFFFFFFF, NULL, 16);
						ui->add_field(ui, ib_addr + 16, ib_vmid, "PAGE_NUM", stream->words[3], NULL, 16);
						break;
					case 3: // MEM_VERIFY
						ui->start_opcode(ui, ib_addr, ib_vmid, stream->opcode, stream->sub_opcode, stream->nwords + 1, "MEM_VERIFY", stream->header_dw, stream->words);
						if (has_cp_fields) ui->add_field(ui, ib_addr + 0, ib_vmid, "CACHE_POLICY", (stream->header_dw >> 24) & 0x7, NULL, 10);
						if (has_cpv_flag) ui->add_field(ui, ib_addr + 0, ib_vmid, "CPV", (stream->header_dw >> 28) & 0x1, NULL, 10);
						ui->add_field(ui, ib_addr + 0, ib_vmid, "MODE", (stream->header_dw >> 31) & 0x1, NULL, 10);
						ui->add_field(ui, ib_addr + 4, ib_vmid, "PATTERN", stream->words[0], NULL, 16);
						ui->add_field(ui, ib_addr + 8, ib_vmid, "CMP0_ADDR_START_LO", stream->words[1], NULL, 16);
						ui->add_field(ui, ib_addr + 12, ib_vmid, "CMP0_ADDR_START_HI", stream->words[2], NULL, 16);
						ui->add_field(ui, ib_addr + 16, ib_vmid, "CMP0_ADDR_END_LO", stream->words[3], NULL, 16);
						ui->add_field(ui, ib_addr + 20, ib_vmid, "CMP0_ADDR_END_HI", stream->words[4], NULL, 16);
						ui->add_field(ui, ib_addr + 24, ib_vmid, "CMP1_ADDR_START_LO", stream->words[5], NULL, 16);
						ui->add_field(ui, ib_addr + 28, ib_vmid, "CMP1_ADDR_START_HI", stream->words[6], NULL, 16);
						ui->add_field(ui, ib_addr + 32, ib_vmid, "CMP1_ADDR_END_LO", stream->words[7], NULL, 16);
						ui->add_field(ui, ib_addr + 36, ib_vmid, "CMP1_ADDR_END_HI", stream->words[8], NULL, 16);
						ui->add_field(ui, ib_addr + 40, ib_vmid, "REC_ADDR_LO", stream->words[9], NULL, 16);
						ui->add_field(ui, ib_addr + 44, ib_vmid, "REC_ADDR_HI", stream->words[10], NULL, 16);
						ui->add_field(ui, ib_addr + 48, ib_vmid, "RESERVED", stream->words[11], NULL, 10);
						break;
					case 4: // INVALIDATION
						ui->start_opcode(ui, ib_addr, ib_vmid, stream->opcode, stream->sub_opcode, stream->nwords + 1, "INVALIDATION", stream->header_dw, stream->words);
						if (ver_maj == ver_ai) {
							ui->add_field(ui, ib_addr + 4, ib_vmid, "INVALIDATEREQ", stream->words[0], NULL, 16);
							ui->add_field(ui, ib_addr + 8, ib_vmid, "ADDRESSRANGE_LO", stream->words[1], NULL, 16);
							ui->add_field(ui, ib_addr + 12, ib_vmid, "INVALIDATEACK", (stream->words[2] >> 0) & 0xFFFF, NULL, 16);
							ui->add_field(ui, ib_addr + 12, ib_vmid, "ADDRESSRANGE_HI", (stream->words[2] >> 16) & 0x1F, NULL, 16);
							ui->add_field(ui, ib_addr + 12, ib_vmid, "INVALIDATEGFXHUB", (stream->words[2] >> 21) & 0x1, NULL, 10);
							ui->add_field(ui, ib_addr + 12, ib_vmid, "INVALIDATEMMHUB", (stream->words[2] >> 22) & 0x1, NULL, 10);
							ui->add_field(ui, ib_addr + 12, ib_vmid, "RESERVED", (stream->words[2] >> 23) & 0x1FF, NULL, 16);
						} else if (ver_maj >= ver_nv) {
							ui->add_field(ui, ib_addr + 0, ib_vmid, "GFX_ENG_ID", (stream->header_dw >> 16) & 0x1F, NULL, 10);
							ui->add_field(ui, ib_addr + 0, ib_vmid, "MM_ENG_ID", (stream->header_dw >> 24) & 0x1F, NULL, 10);
							ui->add_field(ui, ib_addr + 4, ib_vmid, "INVALIDATEREQ", stream->words[0], NULL, 10);
							ui->add_field(ui, ib_addr + 8, ib_vmid, "ADDRESSRANGE_LO", stream->words[1], NULL, 16);
							ui->add_field(ui, ib_addr + 12, ib_vmid, "INVALIDATEACK", stream->words[2] & 0xFFFF, NULL, 10);
							ui->add_field(ui, ib_addr + 12, ib_vmid, "ADDRESSRANGE_HI", (stream->words[2] >> 16) & 0x1F, NULL, 16);
							ui->add_field(ui, ib_addr + 12, ib_vmid, "RESERVED", (stream->words[2] >> 23) & 0x1FF, NULL, 10);
						}
						break;
					default:
						if (ui->unhandled_subop)
							ui->unhandled_subop(ui, asic, ib_addr, ib_vmid, stream);
						break;
				}
				break;
			case 9:  // COND_EXE
				ui->start_opcode(ui, ib_addr, ib_vmid, stream->opcode, stream->sub_opcode, stream->nwords + 1, "COND_EXE", stream->header_dw, stream->words);
				if (has_cp_fields) ui->add_field(ui, ib_addr + 0, ib_vmid, "CACHE_POLICY", (stream->header_dw >> 24) & 0x7, NULL, 10);
				if (has_cpv_flag) ui->add_field(ui, ib_addr + 0, ib_vmid, "CPV", (stream->header_dw >> 28) & 0x1, NULL, 10);
				ui->add_field(ui, ib_addr + 4, ib_vmid, "ADDR_LO", stream->words[0], NULL, 16);
				ui->add_field(ui, ib_addr + 8, ib_vmid, "ADDR_HI", stream->words[1], NULL, 16);
				ui->add_field(ui, ib_addr + 12, ib_vmid, "REFERENCE", stream->words[2], NULL, 16);
				ui->add_field(ui, ib_addr + 16, ib_vmid, "EXEC_COUNT", stream->words[3], NULL, 10);
				break;
			case 10:  // ATOMIC
				ui->start_opcode(ui, ib_addr, ib_vmid, stream->opcode, stream->sub_opcode, stream->nwords + 1, "ATOMIC", stream->header_dw, stream->words);
				ui->add_field(ui, ib_addr + 0, ib_vmid, "LOOP", (stream->header_dw >> 16) & 1, NULL, 16);
				if (ver_maj >= ver_ai) ui->add_field(ui, ib_addr + 0, ib_vmid, "TMZ", (stream->header_dw >> 18) & 0x1, NULL, 10);
				if (has_cp_fields) ui->add_field(ui, ib_addr + 0, ib_vmid, "CACHE_POLICY", (stream->header_dw >> 20) & 0x7, NULL, 10);
				if (has_cpv_flag) ui->add_field(ui, ib_addr + 0, ib_vmid, "CPV", (stream->header_dw >> 24) & 0x1, NULL, 10);
				ui->add_field(ui, ib_addr + 0, ib_vmid, "OP", (stream->header_dw >> 25) & 0x7F, NULL, 16);
				ui->add_field(ui, ib_addr + 4, ib_vmid, "ADDR_LO", stream->words[0], NULL, 16);
				ui->add_field(ui, ib_addr + 8, ib_vmid, "ADDR_HI", stream->words[1], NULL, 16);
				ui->add_field(ui, ib_addr + 12, ib_vmid, "SRC_DATA_LO", stream->words[2], NULL, 16);
				ui->add_field(ui, ib_addr + 16, ib_vmid, "SRC_DATA_HI", stream->words[3], NULL, 16);
				ui->add_field(ui, ib_addr + 20, ib_vmid, "CMP_DATA_LO", stream->words[4], NULL, 16);
				ui->add_field(ui, ib_addr + 24, ib_vmid, "CMP_DATA_HI", stream->words[5], NULL, 16);
				ui->add_field(ui, ib_addr + 28, ib_vmid, "LOOP_INTERVAL", stream->words[6] & 0x1FFF, NULL, 16);
				break;
			case 11: // CONST_FILL
				switch (stream->sub_opcode) {
					case 0: // CONST_FILL
						ui->start_opcode(ui, ib_addr, ib_vmid, stream->opcode, stream->sub_opcode, stream->nwords + 1, "CONST_FILL", stream->header_dw, stream->words);
						ui->add_field(ui, ib_addr + 0, ib_vmid, "SWAP", (stream->header_dw >> 16) & 0x3, NULL, 10);
						if (has_cp_fields) ui->add_field(ui, ib_addr + 0, ib_vmid, "CACHE_POLICY", (stream->header_dw >> 24) & 0x7, NULL, 10);
						if (has_cpv_flag) ui->add_field(ui, ib_addr + 0, ib_vmid, "CPV", (stream->header_dw >> 28) & 0x1, NULL, 10);
						ui->add_field(ui, ib_addr + 0, ib_vmid, "FILL_SIZE", (stream->header_dw >> 30) & 0x3, NULL, 10);
						ui->add_field(ui, ib_addr + 4, ib_vmid, "CONST_FILL_DST_LO", stream->words[0], NULL, 16);
						ui->add_field(ui, ib_addr + 8, ib_vmid, "CONST_FILL_DST_HI", stream->words[1], NULL, 16);
						ui->add_field(ui, ib_addr + 12, ib_vmid, "CONST_FILL_DATA", stream->words[2], NULL, 16);
						ui->add_field(ui, ib_addr + 16, ib_vmid, "CONST_FILL_BYTE_COUNT", stream->words[3], NULL, 10);
						break;
					case 1: // CONST_FILL (DATA_FILL_MULTI)
						ui->start_opcode(ui, ib_addr, ib_vmid, stream->opcode, stream->sub_opcode, stream->nwords + 1, "CONST_FILL (DATA_FILL_MULTI)", stream->header_dw, stream->words);
						if (has_cp_fields) ui->add_field(ui, ib_addr + 0, ib_vmid, "CACHE_POLICY", (stream->header_dw >> 24) & 0x7, NULL, 10);
						if (has_cpv_flag) ui->add_field(ui, ib_addr + 0, ib_vmid, "CPV", (stream->header_dw >> 28) & 0x1, NULL, 10);
						ui->add_field(ui, ib_addr + 0, ib_vmid, "MEMLOG_CLR", (stream->header_dw >> 31) & 0x1, NULL, 10);
						ui->add_field(ui, ib_addr + 4, ib_vmid, "BYTE_STRIDE", stream->words[0], NULL, 10);
						ui->add_field(ui, ib_addr + 8, ib_vmid, "DMA_COUNT", stream->words[1], NULL, 10);
						ui->add_field(ui, ib_addr + 12, ib_vmid, "DST_ADDR_LO", stream->words[2], NULL, 16);
						ui->add_field(ui, ib_addr + 16, ib_vmid, "DST_ADDR_HI", stream->words[3], NULL, 16);
						ui->add_field(ui, ib_addr + 20, ib_vmid, "COUNT", (stream->words[4] >> 0) & 0x3FFFFFF, NULL, 10);
						break;
					default:
						if (ui->unhandled_subop)
							ui->unhandled_subop(ui, asic, ib_addr, ib_vmid, stream);
						break;
				}
				break;
			case 12: // GEN_PTEPDE
				switch (stream->sub_opcode) {
					case 0: // GEN_PTEPDE
						ui->start_opcode(ui, ib_addr, ib_vmid, stream->opcode, stream->sub_opcode, stream->nwords + 1, "GEN_PTEPDE", stream->header_dw, stream->words);
						ui->add_field(ui, ib_addr + 4, ib_vmid, "DST_ADDR_LO", stream->words[0], NULL, 16);
						ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_ADDR_HI", stream->words[1], NULL, 16);
						if (has_cp_fields) ui->add_field(ui, ib_addr + 0, ib_vmid, "CACHE_POLICY", (stream->header_dw >> 24) & 0x7, NULL, 10);
						if (has_cpv_flag) ui->add_field(ui, ib_addr + 0, ib_vmid, "CPV", (stream->header_dw >> 28) & 0x1, NULL, 10);
						ui->add_field(ui, ib_addr + 12, ib_vmid, "MASK_LO", stream->words[2], NULL, 16);
						ui->add_field(ui, ib_addr + 16, ib_vmid, "MASK_HI", stream->words[3], NULL, 16);
						ui->add_field(ui, ib_addr + 20, ib_vmid, "INIT_LO", stream->words[4], NULL, 16);
						ui->add_field(ui, ib_addr + 24, ib_vmid, "INIT_HI", stream->words[5], NULL, 16);
						ui->add_field(ui, ib_addr + 28, ib_vmid, "INCR_LO", stream->words[6], NULL, 16);
						ui->add_field(ui, ib_addr + 32, ib_vmid, "INCR_HI", stream->words[7], NULL, 16);
						ui->add_field(ui, ib_addr + 36, ib_vmid, "COUNT", stream->words[8] & 0x7FFFF, NULL, 10);
						break;
					case 1: // COPY
						ui->start_opcode(ui, ib_addr, ib_vmid, stream->opcode, stream->sub_opcode, stream->nwords + 1, "GEN_PTEPDE (COPY)", stream->header_dw, stream->words);
						if (ver_maj >= ver_ai) {
							ui->add_field(ui, ib_addr + 0, ib_vmid, "TMZ", (stream->header_dw >> 18) & 0x1, NULL, 10);
						}
						if (has_cpv_flag) {
							ui->add_field(ui, ib_addr + 0, ib_vmid, "CPV", (stream->header_dw >> 28) & 0x1, NULL, 10);
						}
						ui->add_field(ui, ib_addr + 0, ib_vmid, "PTEPDE_OP", (stream->header_dw >> 31) & 0x1, NULL, 10);
						ui->add_field(ui, ib_addr + 4, ib_vmid, "SRC_ADDR_LO", stream->words[0], NULL, 16);
						ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_ADDR_HI", stream->words[1], NULL, 16);
						ui->add_field(ui, ib_addr + 12, ib_vmid, "DST_ADDR_LO", stream->words[2], NULL, 16);
						ui->add_field(ui, ib_addr + 16, ib_vmid, "DST_ADDR_HI", stream->words[3], NULL, 16);
						ui->add_field(ui, ib_addr + 20, ib_vmid, "MASK_LO", stream->words[4], NULL, 16);
						ui->add_field(ui, ib_addr + 24, ib_vmid, "MASK_HI", stream->words[5], NULL, 16);
						ui->add_field(ui, ib_addr + 28, ib_vmid, "COUNT", stream->words[6] & 0x7FFFF, NULL, 10);
						if (has_cp_fields) {
							ui->add_field(ui, ib_addr + 28, ib_vmid, "DST_CACHE_POLICY", (stream->words[6] >> 22) & 0x7, NULL, 10);
							ui->add_field(ui, ib_addr + 28, ib_vmid, "SRC_CACHE_POLICY", (stream->words[6] >> 29) & 0x7, NULL, 10);
						}
						break;
					case 2: // RMW
						ui->start_opcode(ui, ib_addr, ib_vmid, stream->opcode, stream->sub_opcode, stream->nwords + 1, "GEN_PTEPDE (RMW)", stream->header_dw, stream->words);
						ui->add_field(ui, ib_addr + 0, ib_vmid, "MTYPE", (stream->header_dw >> 16) & 0x7, NULL, 10);
						ui->add_field(ui, ib_addr + 0, ib_vmid, "GCC", (stream->header_dw >> 19) & 0x1, NULL, 10);
						ui->add_field(ui, ib_addr + 0, ib_vmid, "SYS", (stream->header_dw >> 20) & 0x1, NULL, 10);
						ui->add_field(ui, ib_addr + 0, ib_vmid, "SNP", (stream->header_dw >> 22) & 0x1, NULL, 10);
						ui->add_field(ui, ib_addr + 0, ib_vmid, "GPA", (stream->header_dw >> 23) & 0x1, NULL, 10);
						if (has_cp_fields) {
							ui->add_field(ui, ib_addr + 0, ib_vmid, "L2_POLICY", (stream->header_dw >> 24) & 0x3, NULL, 10);
							ui->add_field(ui, ib_addr + 0, ib_vmid, "LLC_POLICY", (stream->header_dw >> 26) & 0x1, NULL, 10);
						}
						if (has_cpv_flag) {
							ui->add_field(ui, ib_addr + 0, ib_vmid, "CPV", (stream->header_dw >> 28) & 0x1, NULL, 10);
						}
						ui->add_field(ui, ib_addr + 4, ib_vmid, "ADDR_LO", stream->words[0], NULL, 16);
						ui->add_field(ui, ib_addr + 8, ib_vmid, "ADDR_HI", stream->words[1], NULL, 16);
						ui->add_field(ui, ib_addr + 12, ib_vmid, "MASK_LO", stream->words[2], NULL, 16);
						ui->add_field(ui, ib_addr + 16, ib_vmid, "MASK_HI", stream->words[3], NULL, 16);
						ui->add_field(ui, ib_addr + 20, ib_vmid, "VALUE_LO", stream->words[4], NULL, 16);
						ui->add_field(ui, ib_addr + 24, ib_vmid, "VALUE_HI", stream->words[5], NULL, 16);
						ui->add_field(ui, ib_addr + 28, ib_vmid, "NUM_OF_PTE", stream->words[6], NULL, 10);
						break;
					default:
						if (ui->unhandled_subop)
							ui->unhandled_subop(ui, asic, ib_addr, ib_vmid, stream);
						break;
				}
				break;
			case 13: // TIMESTAMP
				switch (stream->sub_opcode) {
					case 0: // TIMESTAMP_SET
						ui->start_opcode(ui, ib_addr, ib_vmid, stream->opcode, stream->sub_opcode, stream->nwords + 1, "TIMESTAMP (SET)", stream->header_dw, stream->words);
						ui->add_field(ui, ib_addr + 4, ib_vmid, "INIT_DATA_LO", stream->words[0], NULL, 16);
						ui->add_field(ui, ib_addr + 8, ib_vmid, "INIT_DATA_HI", stream->words[1], NULL, 16);
						break;
					case 1: // TIMESTAMP_GET
						ui->start_opcode(ui, ib_addr, ib_vmid, stream->opcode, stream->sub_opcode, stream->nwords + 1, "TIMESTAMP (GET)", stream->header_dw, stream->words);
						if (has_cp_fields) {
							ui->add_field(ui, ib_addr + 0, ib_vmid, "L2_POLICY", (stream->header_dw >> 24) & 0x3, NULL, 10);
							ui->add_field(ui, ib_addr + 0, ib_vmid, "LLC_POLICY", (stream->header_dw >> 26) & 0x1, NULL, 10);
						}
						if (has_cpv_flag) {
							ui->add_field(ui, ib_addr + 0, ib_vmid, "CPV", (stream->header_dw >> 28) & 0x1, NULL, 10);
						}
						ui->add_field(ui, ib_addr + 4, ib_vmid, "WRITE_ADDR_LO", stream->words[0], NULL, 16);
						ui->add_field(ui, ib_addr + 8, ib_vmid, "WRITE_ADDR_HI", stream->words[1], NULL, 16);
						break;
					case 2: // TIMESTAMP_GET_GLOBAL
						ui->start_opcode(ui, ib_addr, ib_vmid, stream->opcode, stream->sub_opcode, stream->nwords + 1, "TIMESTAMP (GET_GLOBAL)", stream->header_dw, stream->words);
						if (has_cp_fields) {
							ui->add_field(ui, ib_addr + 0, ib_vmid, "L2_POLICY", (stream->header_dw >> 24) & 0x3, NULL, 10);
							ui->add_field(ui, ib_addr + 0, ib_vmid, "LLC_POLICY", (stream->header_dw >> 26) & 0x1, NULL, 10);
						}
						if (has_cpv_flag) {
							ui->add_field(ui, ib_addr + 0, ib_vmid, "CPV", (stream->header_dw >> 28) & 0x1, NULL, 10);
						}
						ui->add_field(ui, ib_addr + 4, ib_vmid, "WRITE_ADDR_LO", stream->words[0], NULL, 16);
						ui->add_field(ui, ib_addr + 8, ib_vmid, "WRITE_ADDR_HI", stream->words[1], NULL, 16);
						break;
					default:
						if (ui->unhandled_subop)
							ui->unhandled_subop(ui, asic, ib_addr, ib_vmid, stream);
						break;
				}
				break;
			case 14: // WRITE REG
				switch (stream->sub_opcode) {
					case 0: // SRBM_WRITE
						ui->start_opcode(ui, ib_addr, ib_vmid, stream->opcode, stream->sub_opcode, stream->nwords + 1, "SRBM_WRITE", stream->header_dw, stream->words);
						ui->add_field(ui, ib_addr + 0, ib_vmid, "BYTE_ENABLE", (stream->header_dw >> 28), NULL, 10);
						if (ver_maj <= ver_vi)
							ui->add_field(ui, ib_addr + 4, ib_vmid, "SRBM_WRITE_ADDR", stream->words[0] & 0xFFFF, umr_reg_name(asic, stream->words[0] & 0xFFFF), 16);
						else
							ui->add_field(ui, ib_addr + 4, ib_vmid, "SRBM_WRITE_ADDR", stream->words[0] & 0x3FFFF, umr_reg_name(asic, stream->words[0] & 0x3FFFF), 16);
						if (ver_maj >= ver_nv)
							ui->add_field(ui, ib_addr + 4, ib_vmid, "APERTUREID", (stream->words[0] >> 20) & 0xFFF, NULL, 10);
						ui->add_field(ui, ib_addr + 8, ib_vmid, "SRBM_WRITE_DATA", stream->words[1], NULL, 16);
						break;
					case 1: // RMW_REGISTER
						ui->start_opcode(ui, ib_addr, ib_vmid, stream->opcode, stream->sub_opcode, stream->nwords + 1, "RMW_REGISTER", stream->header_dw, stream->words);
						ui->add_field(ui, ib_addr + 4, ib_vmid, "ADDR", stream->words[0] & 0xFFFFF, umr_reg_name(asic, stream->words[0] & 0xFFFFF), 16);
						ui->add_field(ui, ib_addr + 4, ib_vmid, "APERTURE_ID", (stream->words[0] >> 20) & 0xFFF, NULL, 10);
						ui->add_field(ui, ib_addr + 8, ib_vmid, "MASK", stream->words[1], NULL, 10);
						ui->add_field(ui, ib_addr + 12, ib_vmid, "VALUE", stream->words[2], NULL, 10);
						break;
					default:
						if (ui->unhandled_subop)
							ui->unhandled_subop(ui, asic, ib_addr, ib_vmid, stream);
						break;
				}
				break;
			case 15: // PRE_EXE
				ui->start_opcode(ui, ib_addr, ib_vmid, stream->opcode, stream->sub_opcode, stream->nwords + 1, "PRE_EXE", stream->header_dw, stream->words);
				ui->add_field(ui, ib_addr + 0, ib_vmid, "DEV_SEL", (stream->header_dw >> 16) & 0xFF, NULL, 10);
				ui->add_field(ui, ib_addr + 4, ib_vmid, "EXEC_COUNT", stream->words[0] & 0x3FFF, NULL, 10);
				break;
			case 16: // GPUVM_INV (NV and beyond)
				ui->start_opcode(ui, ib_addr, ib_vmid, stream->opcode, stream->sub_opcode, stream->nwords + 1, "GPUVM_INV", stream->header_dw, stream->words);
				ui->add_field(ui, ib_addr + 4, ib_vmid, "PER_VMID_INV_REQ", stream->words[0], NULL, 16);
				ui->add_field(ui, ib_addr + 4, ib_vmid, "FLUSH_TYPE", BITS(stream->words[0], 16, 19), NULL, 16);
				ui->add_field(ui, ib_addr + 4, ib_vmid, "INV_L2_PTES", BITS(stream->words[0], 19, 20), NULL, 16);
				ui->add_field(ui, ib_addr + 4, ib_vmid, "INV_L2_PDE0", BITS(stream->words[0], 20, 21), NULL, 16);
				ui->add_field(ui, ib_addr + 4, ib_vmid, "INV_L2_PDE1", BITS(stream->words[0], 21, 22), NULL, 16);
				ui->add_field(ui, ib_addr + 4, ib_vmid, "INV_L2_PDE2", BITS(stream->words[0], 22, 23), NULL, 16);
				ui->add_field(ui, ib_addr + 4, ib_vmid, "INV_L1_PTES", BITS(stream->words[0], 23, 24), NULL, 16);
				ui->add_field(ui, ib_addr + 4, ib_vmid, "CLEAR_PROTECTION_FAULT_STATUS_ADDR", BITS(stream->words[0], 24, 25), NULL, 16);
				ui->add_field(ui, ib_addr + 4, ib_vmid, "LOG_REQUEST", BITS(stream->words[0], 25, 26), NULL, 16);
				ui->add_field(ui, ib_addr + 4, ib_vmid, "4KB", BITS(stream->words[0], 26, 27), NULL, 16);
				ui->add_field(ui, ib_addr + 8, ib_vmid, "S_BIT", BITS(stream->words[1], 0, 1), NULL, 16);
				ui->add_field(ui, ib_addr + 8, ib_vmid, "PAGE_VM_ADDR_LO", BITS(stream->words[1], 1, 32), NULL, 16);
				ui->add_field(ui, ib_addr + 12, ib_vmid, "PAGE_VM_ADDR_HI", BITS(stream->words[2], 0, 6), NULL, 16);
				break;
			case 17: // GCR
				ui->start_opcode(ui, ib_addr, ib_vmid, stream->opcode, stream->sub_opcode, stream->nwords + 1, "GCR", stream->header_dw, stream->words);
				ui->add_field(ui, ib_addr + 4, ib_vmid, "BASE_VA_LO", BITS(stream->words[0], 7, 32) << 7, NULL, 16);
				ui->add_field(ui, ib_addr + 8, ib_vmid, "BASE_VA_HI", BITS(stream->words[1], 0, 16), NULL, 16);
				n = stream->words[1] >> 16;
				ui->add_field(ui, ib_addr + 8, ib_vmid, "GL2_WB", BITS(n, 15, 16), NULL, 10);
				ui->add_field(ui, ib_addr + 8, ib_vmid, "GL2_INV", BITS(n, 14, 15), NULL, 10);
				ui->add_field(ui, ib_addr + 8, ib_vmid, "GL2_DISCARD", BITS(n, 13, 14), NULL, 10);
				ui->add_field(ui, ib_addr + 8, ib_vmid, "GL2_RANGE", BITS(n, 11, 13), NULL, 10);
				ui->add_field(ui, ib_addr + 8, ib_vmid, "GL2_US", BITS(n, 10, 11), NULL, 10);
				ui->add_field(ui, ib_addr + 8, ib_vmid, "GL1_INV", BITS(n, 9, 10), NULL, 10);
				ui->add_field(ui, ib_addr + 8, ib_vmid, "GLV_INV", BITS(n, 8, 9), NULL, 10);
				ui->add_field(ui, ib_addr + 8, ib_vmid, "GLK_INV", BITS(n, 7, 8), NULL, 10);
				ui->add_field(ui, ib_addr + 8, ib_vmid, "GLK_WB", BITS(n, 6, 7), NULL, 10);
				ui->add_field(ui, ib_addr + 8, ib_vmid, "GLM_INV", BITS(n, 5, 6), NULL, 10);
				ui->add_field(ui, ib_addr + 8, ib_vmid, "GLM_WB", BITS(n, 4, 5), NULL, 10);
				ui->add_field(ui, ib_addr + 8, ib_vmid, "GL1_RANGE", BITS(n, 2, 4), NULL, 10);
				ui->add_field(ui, ib_addr + 8, ib_vmid, "GLI_INV", BITS(n, 0, 2), NULL, 10);

				ui->add_field(ui, ib_addr + 12, ib_vmid, "RANGE_IS_PA", BITS(stream->words[2], 2, 3), NULL, 10);
				ui->add_field(ui, ib_addr + 12, ib_vmid, "SEQ", BITS(stream->words[2], 0, 2), NULL, 10);
				ui->add_field(ui, ib_addr + 12, ib_vmid, "LIMIT_VA_LO", BITS(stream->words[2], 7, 32) << 7, NULL, 16);

				ui->add_field(ui, ib_addr + 16, ib_vmid, "LIMIT_VA_HI", BITS(stream->words[3], 0, 16), NULL, 16);
				ui->add_field(ui, ib_addr + 16, ib_vmid, "VMID", BITS(stream->words[3], 24, 28), NULL, 10);
				break;
			case 32: // DUMMY_TRAP (NV and beyond)
				ui->start_opcode(ui, ib_addr, ib_vmid, stream->opcode, stream->sub_opcode, stream->nwords + 1, "DUMMY_TRAP", stream->header_dw, stream->words);
				ui->add_field(ui, ib_addr + 4, ib_vmid, "INT_CONTEXT", (stream->words[0] >> 0) & 0xFFFFFFF, NULL, 10);
				break;
			default:
				if (ui->unhandled)
					ui->unhandled(ui, asic, ib_addr, ib_vmid, stream);
				break;
		}

		ib_addr += (1 + stream->nwords) * 4;
		stream = stream->next;
	}
	ui->done(ui);
	return stream;
}

// used for testing leave disabled for normal builds
#if 0

// example opaque data to keep track of offsets
struct demo_ui_data {
	uint64_t off[16]; // address of start of IB so we can compute offsets when printing opcodes/fields
	int i;
};

static void start_ib(struct umr_sdma_stream_decode_ui *ui, uint64_t ib_addr, uint32_t ib_vmid, uint64_t from_addr, uint32_t from_vmid, uint32_t size)
{
	struct demo_ui_data *data = ui->data;
	data->off[data->i++] = ib_addr;
	printf("Decoding IB at %lu@0x%llx from %lu@0x%llx of %lu words\n", (unsigned long)ib_vmid, (unsigned long long)ib_addr, (unsigned long)from_vmid, (unsigned long long)from_addr, (unsigned long)size);
}
static void start_opcode(struct umr_sdma_stream_decode_ui *ui, uint64_t ib_addr, uint32_t ib_vmid, uint32_t opcode, uint32_t sub_opcode, uint32_t nwords, char *opcode_name, uint32_t *raw_data)
{
	struct demo_ui_data *data = ui->data;
	printf("Opcode 0x%lx [%s] at %lu@[0x%llx + 0x%llx] (%lu words)\n", (unsigned long)opcode, opcode_name, (unsigned long)ib_vmid, (unsigned long long)data->off[data->i - 1], (unsigned long long)ib_addr - data->off[data->i - 1], (unsigned long)nwords);
	fflush(stdout);
}
static void add_field(struct umr_sdma_stream_decode_ui *ui, uint64_t ib_addr, uint32_t ib_vmid, const char *field_name, uint32_t value, char *str, int ideal_radix)
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
	printf("\n");
	fflush(stdout);
}

static void unhandled(struct umr_sdma_stream_decode_ui *ui, struct umr_asic *asic, uint64_t ib_addr, uint32_t ib_vmid, struct umr_sdma_stream *stream)
{
}

static int unhandled_size(struct umr_sdma_stream_decode_ui *ui, struct umr_asic *asic, struct umr_sdma_stream *stream)
{
	return 1;
}

static void *unhandled_subop(struct umr_sdma_stream_decode_ui *ui, struct umr_asic *asic, uint64_t ib_addr, uint32_t ib_vmid, struct umr_sdma_stream *stream)
{
}

static void done(struct umr_sdma_stream_decode_ui *ui)
{
	struct demo_ui_data *data = ui->data;
	data->i--;

	printf("Done decoding IB\n");
}

static struct  umr_sdma_stream_decode_ui demo_ui = { start_ib, start_opcode, add_field, unhandled, unhandled_size, unhandled_subop, done, NULL };

static const uint32_t gcr_data[] = {
0x11,
0x0,
0xc0000000,
0xffffff84,
0x0000ffff,
};

/** demo */
int umr__demo(struct umr_asic *asic)
{
	struct umr_sdma_stream *stream, *sstream;
	struct umr_sdma_stream_decode_ui myui;
	myui = demo_ui;

	// assign our opaque structure
	myui.data = calloc(1, sizeof(struct demo_ui_data));

//while (1) {
	memset(myui.data, 0, sizeof(struct demo_ui_data));
//	stream = umr_sdma_decode_ring(asic, "sdma0", -1, -1);
stream = umr_sdma_decode_stream(asic, myui, 0, &gcr_data[0], sizeof(gcr_data)/sizeof(gcr_data[0]));
	if (stream) {
		sstream = umr_sdma_decode_stream_opcodes(asic, &myui, stream, 0, 0, 0, 0, ~0UL, 1);
	//	printf("\nand now the rest...\n");
	//	umr_sdma_decode_stream_opcodes(asic, &myui, sstream, 0, 0, 0, 0, ~0UL, 1);
		umr_free_sdma_stream(stream);
	}
//}

	free(myui.data);
}


#endif

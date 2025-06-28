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
#ifndef UMR_PACKET_VPE_H_
#define UMR_PACKET_VPE_H_

/* vpe decoding */
struct umr_vpe_stream {
	uint32_t
		opcode,
		sub_opcode,
		nwords,
		header_dw,
		*words;

	struct {
		uint32_t vmid, size;
		uint64_t addr;
	} ib;

	struct {
		int vmid;
		uint64_t addr;
	} from;

	int invalid;

	struct umr_vpe_stream *next, *next_ib;
};

struct umr_vpe_stream *umr_vpe_decode_stream(struct umr_asic *asic, int vm_partition, uint64_t from_addr, uint32_t from_vmid, uint32_t *stream, uint32_t nwords);
void umr_free_vpe_stream(struct umr_vpe_stream *stream);
struct umr_vpe_stream *umr_vpe_decode_stream_opcodes(struct umr_asic *asic, struct umr_stream_decode_ui *ui, struct umr_vpe_stream *stream, uint64_t ib_addr, uint32_t ib_vmid, uint64_t from_addr, uint64_t from_vmid, unsigned long opcodes, int follow);

#endif

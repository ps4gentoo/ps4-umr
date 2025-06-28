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
#ifndef UMR_PACKET_HSA_H_
#define UMR_PACKET_HSA_H_

// HSA library
struct umr_hsa_stream {
	uint32_t *words,
		 nwords,
		 header,
		 type,
		 barrier,
		 acquire_fence_scope,
		 release_fence_scope;

	int invalid;

	struct umr_hsa_stream *next;
};

struct umr_hsa_stream *umr_hsa_decode_stream(struct umr_asic *asic, uint32_t *stream, uint32_t nwords);
struct umr_hsa_stream *umr_hsa_decode_stream_opcodes(struct umr_asic *asic, struct umr_stream_decode_ui *ui, struct umr_hsa_stream *stream, uint64_t ib_addr, uint32_t ib_vmid, unsigned long opcodes);
void umr_free_hsa_stream(struct umr_hsa_stream *stream);

#endif

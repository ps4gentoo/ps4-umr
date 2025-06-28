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
#ifndef UMR_PACKET_PM4_H_
#define UMR_PACKET_PM4_H_

// PM4 decoding library
struct umr_pm4_stream {
	uint32_t pkttype,				// packet type (0==simple write, 3 == packet)
			 pkt0off,				// base address for PKT0 writes
			 opcode,
			 header,				// header DWORD of packet
			 n_words,				// number of words ignoring header
			 *words;				// words following header word

	struct umr_pm4_stream *next,	// adjacent PM4 packet if any
			      *ib;				// IB this packet might point to

	struct {
		uint64_t addr;
		uint32_t vmid;
	} ib_source;					// where did an IB if any come from?

	struct umr_shaders_pgm *shader; // shader program if any
	struct umr_vcn_cmd_message *vcn; // VCN command message if any

	int invalid;
};

struct umr_pm4_stream *umr_pm4_decode_stream(struct umr_asic *asic, int vm_partition, uint32_t vmid, uint32_t *stream, uint32_t nwords);
void umr_free_pm4_stream(struct umr_pm4_stream *stream);
struct umr_shaders_pgm *umr_find_shader_in_stream(struct umr_pm4_stream *stream, unsigned vmid, uint64_t addr);
const char *umr_pm4_opcode_to_str(uint32_t header);

struct umr_pm4_stream *umr_pm4_decode_stream_opcodes(struct umr_asic *asic, struct umr_stream_decode_ui *ui, struct umr_pm4_stream *stream, uint64_t ib_addr, uint32_t ib_vmid, uint64_t from_addr, uint64_t from_vmid, unsigned long opcodes, int follow);

// PM4-lite
struct umr_pm4_stream *umr_pm4_lite_decode_stream(struct umr_asic *asic, int vm_partition, uint32_t vmid, uint32_t *stream, uint32_t nwords);
struct umr_pm4_stream *umr_pm4_lite_decode_stream_opcodes(struct umr_asic *asic, struct umr_stream_decode_ui *ui, struct umr_pm4_stream *stream, uint64_t ib_addr, uint32_t ib_vmid, uint64_t from_addr, uint64_t from_vmid, unsigned long opcodes, int follow);

#endif

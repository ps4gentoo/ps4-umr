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
#ifndef UMR_PACKET_VCN_H_
#define UMR_PACKET_VCN_H_

struct umr_vcn_enc_stream {
	uint32_t
		opcode,
		nwords,
		*words;
	struct umr_vcn_cmd_message *vcn;	// VCN command message if any
	int invalid;
	struct umr_vcn_enc_stream *next;
};
void umr_free_vcn_enc_stream(struct umr_vcn_enc_stream *stream);
struct umr_vcn_enc_stream *umr_vcn_enc_decode_stream(struct umr_asic *asic, uint32_t *stream, uint32_t nwords);
struct umr_pm4_stream *umr_vcn_dec_decode_stream(struct umr_asic *asic, uint32_t vmid, uint32_t *stream, uint32_t nwords);
int umr_vcn_decode(struct umr_asic *asic, uint32_t *p_curr, uint32_t size_in_byte, uint64_t ib_addr, uint32_t vcn_type, char ***opcode_strs);

struct umr_vcn_enc_stream *umr_vcn_enc_decode_stream_opcodes(struct umr_asic *asic, struct umr_stream_decode_ui *ui, struct umr_vcn_enc_stream *stream, uint64_t ib_addr, uint32_t ib_vmid, uint64_t from, uint64_t from_vmid, unsigned long opcodes, int follow);
struct umr_pm4_stream *umr_vcn_dec_decode_stream_opcodes(struct umr_asic *asic, struct umr_stream_decode_ui *ui, struct umr_pm4_stream *stream, uint64_t ib_addr, uint32_t ib_vmid, uint64_t from, uint64_t from_vmid, unsigned long opcodes, int follow);
void umr_vcn_dec_decode_unified_ring(struct umr_asic *asic, struct umr_vcn_cmd_message *vcn, FILE *pOut, struct umr_ip_block *ip, uint32_t *gui_inbuf, uint32_t gui_size, char ***out_buf);
void umr_parse_vcn_dec(struct umr_asic *asic, struct umr_vcn_cmd_message *vcn, FILE *pOut);
void umr_parse_vcn_enc(struct umr_asic *asic, struct umr_vcn_cmd_message *vcn, FILE *pOut);
void umr_print_dec_ib_msg(struct umr_asic *asic, struct umr_vcn_cmd_message *vcn, FILE * pOut, struct umr_ip_block *ip, uint32_t *in_buf, uint32_t size, char ***pBuf);

#endif

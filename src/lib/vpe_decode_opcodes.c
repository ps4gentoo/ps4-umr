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

static uint32_t fetch_word(struct umr_asic *asic, struct umr_vpe_stream *stream, uint32_t off)
{
	if (off >= stream->nwords) {
		if (!(stream->invalid))
			asic->err_msg("[ERROR]: VPE decoding of opcode (%"PRIx32":%"PRIx32") went out of bounds.\n", stream->opcode, stream->sub_opcode);
		stream->invalid = 1;
		return 0;
	} else {
		return stream->words[off];
	}
}

/**
 * umr_osssys_get_ip_ver - Get the IP major and minor revision of the OSSSYS block
 *
 * @asic: The ASIC to query
 * @maj: Where to store the major version
 * @min: Where to store the minor version
 *
 * Returns -1 if the block cannot be found, 0 otherwise.
 */
int umr_osssys_get_ip_ver(struct umr_asic *asic, int *maj, int *min)
{
	struct umr_ip_block *ip;

	ip = umr_find_ip_block(asic, "osssys", -1);
	if (!ip)
		ip = umr_find_ip_block(asic, "oss", -1); // for static models that call it OSS

	if (ip) {
		*maj = ip->discoverable.maj;
		*min = ip->discoverable.min;
		return 0;
	}
	return -1;
}

/**
 * umr_vpe_decode_stream_opcodes - decode a stream of VPE packets
 *
 * @asic: The ASIC the VPE packets are bound for
 * @ui: The user interface callback that will present the decoded packets to the user
 * @stream: The pre-processed stream of VPE packets
 * @ib_addr: The base VM address where the packets came from
 * @ib_vmid: The VMID the IB is mapped into
 * @from_addr: The address of the ring/IB that pointed to this VPE IB
 * @from_vmid: The VMID of the ring/IB that pointed to this VPE IB
 * @opcodes: The number of opcodes to decode
 * @follow: Follow any chained IBs
 *
 * Returns the address of the first packet that hasn't been decoded.
 */
struct umr_vpe_stream *umr_vpe_decode_stream_opcodes(struct umr_asic *asic, struct umr_stream_decode_ui *ui, struct umr_vpe_stream *stream,
						       uint64_t ib_addr, uint32_t ib_vmid, uint64_t from_addr, uint64_t from_vmid, unsigned long opcodes, int follow)
{
	uint32_t n, m;
	struct umr_vpe_stream *os = stream;
	static char *poll_regmem_funcs[] = { "always", "<", "<=", "==", "!=", ">=", ">", "N/A" };
	int maj, min;

	if (umr_osssys_get_ip_ver(asic, &maj, &min)) {
		asic->err_msg("[BUG]: Cannot determine OSSSYS version for this ASIC\n");
		maj = min = 0;
	}

	n = 0;
	while (os) {
		n += os->nwords;
		os = os->next;
	}

	ui->start_ib(ui, ib_addr, ib_vmid, from_addr, from_vmid, n, 0);

	while (stream && opcodes--) {
		switch (stream->opcode) {
			case 0:
				ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "NOP", stream->header_dw, stream->words);
				break;
			case 1: // VPE Descriptor
				ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "VPE Descriptor", stream->header_dw, stream->words);
				if (maj == 6 && min == 1) {
					ui->add_field(ui, ib_addr + 0, ib_vmid, "CD", (stream->header_dw >> 16) & 0x1F, NULL, 10, 32);
				} else {
					ui->add_field(ui, ib_addr + 0, ib_vmid, "CD", (stream->header_dw >> 16) & 0xF, NULL, 10, 32);
				}
				ui->add_field(ui, ib_addr + 4, ib_vmid, "TMZ", (fetch_word(asic, stream, 0)) & 0x1, NULL, 10, 32);
				ui->add_field(ui, ib_addr + 4, ib_vmid, "PLANE_DESCRIPTOR_ADDR[31:2]", (fetch_word(asic, stream, 0)) & ~0x3UL, NULL, 16, 32);
				ui->add_field(ui, ib_addr + 8, ib_vmid, "PLANE_DESCRIPTOR_ADDR[63:32]", fetch_word(asic, stream, 1), NULL, 16, 32);
				ui->add_field(ui, ib_addr + 12, ib_vmid, "NUM_CONFIG_DESCRIPTOR", fetch_word(asic, stream, 2) & 0xFF, NULL, 10, 32);
				for (n = 0; n < ((fetch_word(asic, stream, 2) & 0xFF) + 1); n++) {
					ui->add_field(ui, ib_addr + 16 + n * 8, ib_vmid, "TMZ", (fetch_word(asic, stream, 3 + 2 * n)) & 1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 16 + n * 8, ib_vmid, "REUSE", (fetch_word(asic, stream, 3 + 2 * n)) & 2 ? 1 : 0, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 16 + n * 8, ib_vmid, "CONFIG_DESCRIPTOR_ADDR[31:2]", (fetch_word(asic, stream, 3 + 2 * n)) & ~0x3UL, NULL, 16, 32);
					ui->add_field(ui, ib_addr + 20 + n * 8, ib_vmid, "CONFIG_DESCRIPTOR_ADDR[63:32]", fetch_word(asic, stream, 4 + 2 * n), NULL, 16, 32);
				}
				break;
			case 2: // PLANE CONFIG
				{
					//uint32_t NPS0, NPD0, NPS1, NPD1;
					uint32_t NPS0, NPD0;
					NPS0 = (stream->header_dw >> 16) & 3;
					NPD0 = (stream->header_dw >> 18) & 3;
					//NPS1 = (stream->header_dw >> 20) & 3;
					//NPD1 = (stream->header_dw >> 22) & 3;

					ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "Plane Config", stream->header_dw, stream->words);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "NPS0", (stream->header_dw >> 16) & 0x3, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "NPD0", (stream->header_dw >> 18) & 0x3, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "NPS1", (stream->header_dw >> 20) & 0x3, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 0, ib_vmid, "NPD1", (stream->header_dw >> 22) & 0x3, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "TMZ", (fetch_word(asic, stream, 0) >> 16) & 1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "SWIZZLE_MODE", (fetch_word(asic, stream, 0) >> 3) & 0x1F, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "ROTATE_ANGLE", fetch_word(asic, stream, 0) & 3, NULL, 10, 32);

					ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_PLANE0_BASE_ADDR[31:8]", (fetch_word(asic, stream, 1)) & ~0xFFUL, NULL, 16, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "SRC_PLANE0_BASE_ADDR[63:32]", fetch_word(asic, stream, 2), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 16, ib_vmid, "SRC_PLANE0_PITCH", (fetch_word(asic, stream, 3)) & ((1UL << 14) - 1), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "SRC_PLANE0_VIEWPORT_Y", (fetch_word(asic, stream, 4) >> 16) & ((1UL << 14) - 1), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "SRC_PLANE0_VIEWPORT_X", (fetch_word(asic, stream, 4)) & ((1UL << 14) - 1), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "SRC_PLANE0_VIEWPORT_HEIGHT", (fetch_word(asic, stream, 5) >> 16) & ((1UL << 13) - 1), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "SRC_PLANE0_VIEWPORT_WIDTH", (fetch_word(asic, stream, 5)) & ((1UL << 13) - 1), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "SRC_PLANE0_ELEMENT_SIZE", (fetch_word(asic, stream, 5) >> 13) & 7, NULL, 10, 32);

					n = 6;
					m = 28;

					if (NPS0) {
						ui->add_field(ui, ib_addr + m + 0, ib_vmid, "SRC_PLANE1_BASE_ADDR[31:8]", (fetch_word(asic, stream, n+0)) & ~0xFFUL, NULL, 16, 32);
						ui->add_field(ui, ib_addr + m + 4, ib_vmid, "SRC_PLANE1_BASE_ADDR[63:32]", fetch_word(asic, stream, n+1), NULL, 16, 32);
						ui->add_field(ui, ib_addr + m + 8, ib_vmid, "SRC_PLANE1_PITCH", (fetch_word(asic, stream, n+2)) & ((1UL << 14) - 1), NULL, 10, 32);
						ui->add_field(ui, ib_addr + m + 12, ib_vmid, "SRC_PLANE1_VIEWPORT_Y", (fetch_word(asic, stream, n+3) >> 16) & ((1UL << 14) - 1), NULL, 10, 32);
						ui->add_field(ui, ib_addr + m + 12, ib_vmid, "SRC_PLANE1_VIEWPORT_X", (fetch_word(asic, stream, n+3)) & ((1UL << 14) - 1), NULL, 10, 32);
						ui->add_field(ui, ib_addr + m + 16, ib_vmid, "SRC_PLANE1_VIEWPORT_HEIGHT", (fetch_word(asic, stream, n+4) >> 16) & ((1UL << 13) - 1), NULL, 10, 32);
						ui->add_field(ui, ib_addr + m + 16, ib_vmid, "SRC_PLANE1_VIEWPORT_WIDTH", (fetch_word(asic, stream, n+4)) & ((1UL << 13) - 1), NULL, 10, 32);
						ui->add_field(ui, ib_addr + m + 16, ib_vmid, "SRC_PLANE1_ELEMENT_SIZE", (fetch_word(asic, stream, n+4) >> 13) & 7, NULL, 10, 32);
						n += 5;
						m += 20;
					}

					// TODO: sub-opcode {1, 2} that have 2 source pipes

					ui->add_field(ui, ib_addr + m, ib_vmid, "TMZ", (fetch_word(asic, stream, n) >> 16) & 1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + m, ib_vmid, "SWIZZLE_MODE", (fetch_word(asic, stream, n) >> 3) & 0x1F, NULL, 10, 32);
					ui->add_field(ui, ib_addr + m, ib_vmid, "ROTATE_ANGLE", fetch_word(asic, stream, n) & 3, NULL, 10, 32);
					m += 4;
					++n;

					ui->add_field(ui, ib_addr + m + 0, ib_vmid, "DST_PLANE0_BASE_ADDR[31:8]", (fetch_word(asic, stream, n+0)) & ~0xFFUL, NULL, 16, 32);
					ui->add_field(ui, ib_addr + m + 4, ib_vmid, "DST_PLANE0_BASE_ADDR[63:32]", fetch_word(asic, stream, n+1), NULL, 16, 32);
					ui->add_field(ui, ib_addr + m + 8, ib_vmid, "DST_PLANE0_PITCH", (fetch_word(asic, stream, n+2)) & ((1UL << 14) - 1), NULL, 10, 32);
					ui->add_field(ui, ib_addr + m + 12, ib_vmid, "DST_PLANE0_VIEWPORT_Y", (fetch_word(asic, stream, n+3) >> 16) & ((1UL << 14) - 1), NULL, 10, 32);
					ui->add_field(ui, ib_addr + m + 12, ib_vmid, "DST_PLANE0_VIEWPORT_X", (fetch_word(asic, stream, n+3)) & ((1UL << 14) - 1), NULL, 10, 32);
					ui->add_field(ui, ib_addr + m + 16, ib_vmid, "DST_PLANE0_VIEWPORT_HEIGHT", (fetch_word(asic, stream, n+4) >> 16) & ((1UL << 13) - 1), NULL, 10, 32);
					ui->add_field(ui, ib_addr + m + 16, ib_vmid, "DST_PLANE0_VIEWPORT_WIDTH", (fetch_word(asic, stream, n+4)) & ((1UL << 13) - 1), NULL, 10, 32);
					ui->add_field(ui, ib_addr + m + 16, ib_vmid, "DST_PLANE0_ELEMENT_SIZE", (fetch_word(asic, stream, n+4) >> 13) & 7, NULL, 10, 32);
					n += 5;
					m += 20;

					if (NPD0) {
						ui->add_field(ui, ib_addr + m + 0, ib_vmid, "DST_PLANE1_BASE_ADDR[31:8]", (fetch_word(asic, stream, n+0)) & ~0xFFUL, NULL, 16, 32);
						ui->add_field(ui, ib_addr + m + 4, ib_vmid, "DST_PLANE1_BASE_ADDR[63:32]", fetch_word(asic, stream, n+1), NULL, 16, 32);
						ui->add_field(ui, ib_addr + m + 8, ib_vmid, "DST_PLANE1_PITCH", (fetch_word(asic, stream, n+2)) & ((1UL << 14) - 1), NULL, 10, 32);
						ui->add_field(ui, ib_addr + m + 12, ib_vmid, "DST_PLANE1_VIEWPORT_Y", (fetch_word(asic, stream, n+3) >> 16) & ((1UL << 14) - 1), NULL, 10, 32);
						ui->add_field(ui, ib_addr + m + 12, ib_vmid, "DST_PLANE1_VIEWPORT_X", (fetch_word(asic, stream, n+3)) & ((1UL << 14) - 1), NULL, 10, 32);
						ui->add_field(ui, ib_addr + m + 16, ib_vmid, "DST_PLANE1_VIEWPORT_HEIGHT", (fetch_word(asic, stream, n+4) >> 16) & ((1UL << 13) - 1), NULL, 10, 32);
						ui->add_field(ui, ib_addr + m + 16, ib_vmid, "DST_PLANE1_VIEWPORT_WIDTH", (fetch_word(asic, stream, n+4)) & ((1UL << 13) - 1), NULL, 10, 32);
						ui->add_field(ui, ib_addr + m + 16, ib_vmid, "DST_PLANE1_ELEMENT_SIZE", (fetch_word(asic, stream, n+4) >> 13) & 7, NULL, 10, 32);
						n += 5;
						m += 20;
					}

					// TODO: sub-opcode {2} that have 1 destination pipes

				}
				break;
			case 3: // VPEP CONFIG
				switch (stream->sub_opcode) {
					case 0: // Direct Config
					{
						uint32_t vpep_direct_config_array_size = (stream->header_dw >> 16) + 1, vpep_config_data_size;
						ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "VPEP Direct Config", stream->header_dw, stream->words);
						ui->add_field(ui, ib_addr + 0, ib_vmid, "VPEP_DIRECT_CONFIG_ARRAY_SIZE", ((stream->header_dw >> 16) + 1) & 0xFFFF, NULL, 10, 32);
						m = 4;
						n = 0;
						while (vpep_direct_config_array_size) {
							ui->add_field(ui, ib_addr + m, ib_vmid, "VPEP_CONFIG_DATA_SIZE", (fetch_word(asic, stream, n) >> 20) + 1, NULL, 10, 32);
							ui->add_field(ui, ib_addr + m, ib_vmid, "VPEP_CONFIG_REGISTER_OFFSET", fetch_word(asic, stream, n) & 0xFFFFC, umr_reg_name(asic, BITS(fetch_word(asic, stream, n), 2, 20)), 16, 32);
							ui->add_field(ui, ib_addr + m, ib_vmid, "INC", fetch_word(asic, stream, n) & 0x3, NULL, 10, 32);
							vpep_config_data_size = (fetch_word(asic, stream, n) >> 20) + 1;
							++n;
							m += 4;
							--vpep_direct_config_array_size;
							while (vpep_config_data_size--) {
								ui->add_field(ui, ib_addr + m, ib_vmid, "VPEP_CONFIG_DATA", fetch_word(asic, stream, n), NULL, 16, 32);
								m += 4;
								++n;
								--vpep_direct_config_array_size;
							}
						}
					}
						break;
					case 1: // Indirect Config
					{
						uint32_t num_dst = ((stream->header_dw >> 28) + 1) & 0xF;
						ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "VPEP Indirect Config", stream->header_dw, stream->words);
						ui->add_field(ui, ib_addr + 0, ib_vmid, "NUM_DST", ((stream->header_dw >> 28) + 1) & 0xF, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 4, ib_vmid, "DATA_ARRAY_SIZE", (fetch_word(asic, stream, 0) + 1) & ((1UL << 19) - 1), NULL, 10, 32);
						ui->add_field(ui, ib_addr + 8, ib_vmid, "DATA_ARRAY_ADDR[31:2]", fetch_word(asic, stream, 1) & ~3UL, NULL, 16, 32);
						ui->add_field(ui, ib_addr + 8, ib_vmid, "TMZ", fetch_word(asic, stream, 1) & 1, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 12, ib_vmid, "DATA_ARRAY_ADDR[63:32]", fetch_word(asic, stream, 2), NULL, 16, 32);
						m = 16;
						n = 3;
						while (num_dst--) {
							ui->add_field(ui, ib_addr + m + 0, ib_vmid, "INDEX_REGISTER OFFSET[19:2]", fetch_word(asic, stream, n + 0) & 0xFFFFC, umr_reg_name(asic, BITS(fetch_word(asic, stream, n + 0), 2, 20)), 16, 32);
							ui->add_field(ui, ib_addr + m + 4, ib_vmid, "INDEX_REGISTER DATA", fetch_word(asic, stream, n + 1), NULL, 16, 32);
							ui->add_field(ui, ib_addr + m + 8, ib_vmid, "DATA_REGISTER OFFSET[19:2]", fetch_word(asic, stream, n + 2) & 0xFFFFC, umr_reg_name(asic, BITS(fetch_word(asic, stream, n + 2), 2, 20)), 16, 32);
							m += 12;
							n += 3;
						}
					}
						break;
				}
				break;
			case 4: // INDIRECT
				ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "INDIRECT_BUFFER", stream->header_dw, stream->words);
				ui->add_field(ui, ib_addr + 0, ib_vmid, "VMID", (stream->header_dw >> 16) & 0xF, NULL, 16, 32);
				ui->add_field(ui, ib_addr + 4, ib_vmid, "IB_BASE_LO", fetch_word(asic, stream, 0), NULL, 16, 32);
				ui->add_field(ui, ib_addr + 8, ib_vmid, "IB_BASE_HI", fetch_word(asic, stream, 1), NULL, 16, 32);
				ui->add_field(ui, ib_addr + 12, ib_vmid, "IB_BASE_SIZE", fetch_word(asic, stream, 2), NULL, 10, 32);
				ui->add_field(ui, ib_addr + 16, ib_vmid, "IB_CSA_ADDR_LO", fetch_word(asic, stream, 3), NULL, 16, 32);
				ui->add_field(ui, ib_addr + 20, ib_vmid, "IB_CSA_ADDR_HI", fetch_word(asic, stream, 4), NULL, 16, 32);
				if (follow && stream->next_ib)
					umr_vpe_decode_stream_opcodes(asic, ui, stream->next_ib, stream->ib.addr, stream->ib.vmid, stream->next_ib->from.addr, stream->next_ib->from.vmid, ~0UL, 1);
				break;
			case 5: // FENCE
				ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "FENCE", stream->header_dw, stream->words);
				ui->add_field(ui, ib_addr + 4, ib_vmid, "FENCE_ADDR_LO", fetch_word(asic, stream, 0), NULL, 16, 32);
				ui->add_field(ui, ib_addr + 8, ib_vmid, "FENCE_ADDR_HI", fetch_word(asic, stream, 1), NULL, 16, 32);
				ui->add_field(ui, ib_addr + 12, ib_vmid, "FENCE_DATA", fetch_word(asic, stream, 2), NULL, 16, 32);
				break;
			case 6: // TRAP
				ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "TRAP", stream->header_dw, stream->words);
				ui->add_field(ui, ib_addr + 4, ib_vmid, "TRAP_INT_CONTEXT", fetch_word(asic, stream, 0) & 0xFFFFFF, NULL, 16, 32);
				break;
			case 7: // REGISTER WRITE
				ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "REGISTER_WRITE", stream->header_dw, stream->words);
				ui->add_field(ui, ib_addr + 4, ib_vmid, "REGISTER_ADDRESS[19:2]", BITS(fetch_word(asic, stream, 0), 2, 20) << 2,
					umr_reg_name(asic, (BITS(fetch_word(asic, stream, 0), 20, 32) << 18) | BITS(fetch_word(asic, stream, 0), 2, 20)), 16, 32);
				ui->add_field(ui, ib_addr + 4, ib_vmid, "APERTURE_ID", BITS(fetch_word(asic, stream, 0), 20, 32), NULL, 16, 32);
				ui->add_field(ui, ib_addr + 8, ib_vmid, "DATA", fetch_word(asic, stream, 1), NULL, 16, 32);
				break;
			case 8: // POLL_REGMEM
				switch (stream->sub_opcode) {
					case 0: // POLL_REGMEM
						ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "POLL_REGMEM", stream->header_dw, stream->words);
						ui->add_field(ui, ib_addr + 0, ib_vmid, "FUNCTION", (stream->header_dw >> 28) & 7, poll_regmem_funcs[(stream->header_dw >> 28) & 7], 0, 32);
						ui->add_field(ui, ib_addr + 0, ib_vmid, "MEM_POLL", (stream->header_dw >> 31) & 1, NULL, 16, 32);
						if (!(stream->header_dw & (1UL << 31))) {
							ui->add_field(ui, ib_addr + 4, ib_vmid, "REGISTER", BITS(fetch_word(asic, stream, 0), 2, 32) << 2, umr_reg_name(asic, BITS(fetch_word(asic, stream, 0), 2, 32)), 16, 32);
						} else {
							ui->add_field(ui, ib_addr + 4, ib_vmid, "POLL_REGMEM_ADDR_LO", BITS(fetch_word(asic, stream, 0), 2, 32) << 2, NULL, 16, 32);
							ui->add_field(ui, ib_addr + 8, ib_vmid, "POLL_REGMEM_ADDR_HI", fetch_word(asic, stream, 1), NULL, 16, 32);
						}
						ui->add_field(ui, ib_addr + 12, ib_vmid, "REFERENCE", fetch_word(asic, stream, 2), NULL, 16, 32);
						ui->add_field(ui, ib_addr + 16, ib_vmid, "MASK", fetch_word(asic, stream, 3), NULL, 16, 32);
						ui->add_field(ui, ib_addr + 20, ib_vmid, "POLL_INTERVAL", BITS(fetch_word(asic, stream, 4), 0, 16), NULL, 16, 32);
						ui->add_field(ui, ib_addr + 20, ib_vmid, "RETRY_COUNT", BITS(fetch_word(asic, stream, 4), 16, 28), NULL, 16, 32);
						break;
					case 1: // POLL_REG_WRITE_MEM
						ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "POLL_REG_WRITE_MEM", stream->header_dw, stream->words);
						ui->add_field(ui, ib_addr + 4, ib_vmid, "SRC_REG_ADDR", BITS(fetch_word(asic, stream, 0), 2, 32) << 2, umr_reg_name(asic, BITS(fetch_word(asic, stream, 0), 2, 32)), 16, 32);
						ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_MEM_ADDR_LO", fetch_word(asic, stream, 1), NULL, 16, 32);
						ui->add_field(ui, ib_addr + 12, ib_vmid, "DST_MEM_ADDR_HI", fetch_word(asic, stream, 2), NULL, 16, 32);
						break;
					default:
						if (ui->unhandled_subop)
							ui->unhandled_subop(ui, asic, ib_addr, ib_vmid, stream, UMR_RING_VPE);
						break;
				}
				break;
			case 9:  // COND_EXE
				ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "COND_EXE", stream->header_dw, stream->words);
				ui->add_field(ui, ib_addr + 4, ib_vmid, "ADDR_LO", BITS(fetch_word(asic, stream, 0), 2, 32) << 2, NULL, 16, 32);
				ui->add_field(ui, ib_addr + 8, ib_vmid, "ADDR_HI", fetch_word(asic, stream, 1), NULL, 16, 32);
				ui->add_field(ui, ib_addr + 12, ib_vmid, "REFERENCE", fetch_word(asic, stream, 2), NULL, 16, 32);
				ui->add_field(ui, ib_addr + 16, ib_vmid, "EXEC_COUNT", fetch_word(asic, stream, 3), NULL, 10, 32);
				break;
			case 10:  // ATOMIC
				ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "ATOMIC", stream->header_dw, stream->words);
				ui->add_field(ui, ib_addr + 0, ib_vmid, "LOOP", (stream->header_dw >> 16) & 1, NULL, 16, 32);
				ui->add_field(ui, ib_addr + 0, ib_vmid, "OP", (stream->header_dw >> 25) & 0x7F, NULL, 16, 32);
				ui->add_field(ui, ib_addr + 4, ib_vmid, "ADDR_LO", BITS(fetch_word(asic, stream, 0), 2, 32) << 2, NULL, 16, 32);
				ui->add_field(ui, ib_addr + 8, ib_vmid, "ADDR_HI", fetch_word(asic, stream, 1), NULL, 16, 32);
				ui->add_field(ui, ib_addr + 12, ib_vmid, "SRC_DATA_LO", fetch_word(asic, stream, 2), NULL, 16, 32);
				ui->add_field(ui, ib_addr + 16, ib_vmid, "SRC_DATA_HI", fetch_word(asic, stream, 3), NULL, 16, 32);
				ui->add_field(ui, ib_addr + 20, ib_vmid, "CMP_DATA_LO", fetch_word(asic, stream, 4), NULL, 16, 32);
				ui->add_field(ui, ib_addr + 24, ib_vmid, "CMP_DATA_HI", fetch_word(asic, stream, 5), NULL, 16, 32);
				ui->add_field(ui, ib_addr + 28, ib_vmid, "LOOP_INTERVAL", fetch_word(asic, stream, 6) & 0x1FFF, NULL, 16, 32);
				break;
			case 13: // TIMESTAMP
				switch (stream->sub_opcode) {
					case 0: // TIMESTAMP_SET
						ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "TIMESTAMP (SET)", stream->header_dw, stream->words);
						ui->add_field(ui, ib_addr + 4, ib_vmid, "INIT_DATA_LO", fetch_word(asic, stream, 0), NULL, 16, 32);
						ui->add_field(ui, ib_addr + 8, ib_vmid, "INIT_DATA_HI", fetch_word(asic, stream, 1), NULL, 16, 32);
						break;
					case 1: // TIMESTAMP_GET
						ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "TIMESTAMP (GET)", stream->header_dw, stream->words);
						ui->add_field(ui, ib_addr + 4, ib_vmid, "WRITE_ADDR_LO", BITS(fetch_word(asic, stream, 0), 3, 32)  << 3, NULL, 16, 32);
						ui->add_field(ui, ib_addr + 8, ib_vmid, "WRITE_ADDR_HI", fetch_word(asic, stream, 1), NULL, 16, 32);
						break;
					case 2: // TIMESTAMP_GET_GLOBAL
						ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, stream->sub_opcode, stream->nwords + 1, "TIMESTAMP (GET_GLOBAL)", stream->header_dw, stream->words);
						ui->add_field(ui, ib_addr + 4, ib_vmid, "WRITE_ADDR_LO", BITS(fetch_word(asic, stream, 0), 3, 32)  << 3, NULL, 16, 32);
						ui->add_field(ui, ib_addr + 8, ib_vmid, "WRITE_ADDR_HI", fetch_word(asic, stream, 1), NULL, 16, 32);
						break;
					default:
						if (ui->unhandled_subop)
							ui->unhandled_subop(ui, asic, ib_addr, ib_vmid, stream, UMR_RING_VPE);
						break;
				}
				break;
			default:
				if (ui->unhandled)
					ui->unhandled(ui, asic, ib_addr, ib_vmid, stream, UMR_RING_VPE);
				break;
		}

		if (stream->invalid)
			break;

		ib_addr += (1 + stream->nwords) * 4;
		stream = stream->next;
	}
	ui->done(ui);
	(void)n; // silence warnings
	(void)m;
	return stream;
}

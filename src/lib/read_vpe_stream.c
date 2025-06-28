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

/**
 * umr_vpe_decode_stream - Decode an array of vpe packets into a vpe stream
 *
 * @vmid:  The VMID (or zero) that this array comes from (if say an IB)
 * @stream: An array of DWORDS which contain the vpe packets
 * @nwords:  The number of words in the stream
 *
 * Returns a vpe stream if successfully decoded.
 */
struct umr_vpe_stream *umr_vpe_decode_stream(struct umr_asic *asic, int vm_partition, uint64_t from_addr, uint32_t from_vmid, uint32_t *stream, uint32_t nwords)
{
	struct umr_vpe_stream *ops, *ps, *prev_ps = NULL;
	uint32_t *ostream = stream;

	ps = ops = calloc(1, sizeof *ops);
	if (!ps) {
		asic->err_msg("[ERROR]: Out of memory\n");
		return NULL;
	}

	while (nwords) {
		ps->opcode = *stream & 0xFF;
		ps->sub_opcode = (*stream >> 8) & 0xFF;
		ps->header_dw = *stream++;

		switch (ps->opcode) {
			case 0: // NOP
				ps->nwords = 0; // no words other than header
				break;
			case 1: // VPE DESCRIPTOR
				ps->nwords = 3 + (((stream[2] + 1) & 0xFF) * 2); // (it's DW3 but we advanced *stream above)
				break;
			case 2: // VPE PLANE CONFIG
				{
					uint32_t NPS0, NPD0, NPS1, NPD1;
					NPS0 = (ps->header_dw >> 16) & 3;
					NPD0 = (ps->header_dw >> 18) & 3;
					NPS1 = (ps->header_dw >> 20) & 3;
					NPD1 = (ps->header_dw >> 22) & 3;
					switch (ps->sub_opcode) {
						case 0: // 0 => 1 src + 1 dst
							ps->nwords = 2 + 5 * (1 + NPS0) + 5 * (1 + NPD0);
							break;
						case 1: // 1 => 2 src + 1 dst
							ps->nwords = 2 + 5 * (1 + NPS0) + 5 * (1 + NPS1) + 5 * (1 + NPD0);
							break;
						case 2: // 2 => 2 src + 2 dst
							ps->nwords = 2 + 5 * (1 + NPS0) + 5 * (1 + NPS1) + 5 * (1 + NPD0) + 5 * (1 + NPD1);
							break;
					}
				}
				break;
			case 3: // VPEP Config
			{
				switch (ps->sub_opcode) {
					case 0: // DIRECT
						ps->nwords = (ps->header_dw >> 16) + 1;
						break;
					case 1: // INDIRECT
						ps->nwords = 3 + ((ps->header_dw >> 28) + 1) * 3;
						break;
				}
			}
				break;
			case 4: // INDIRECT
				ps->ib.vmid = (ps->header_dw >> 16) & 0xF;
				ps->ib.addr = ((uint64_t)stream[1] << 32) | stream[0];
				ps->ib.size = stream[2];
				if (asic->family >= FAMILY_AI)
					ps->ib.vmid |= UMR_MM_HUB;
				ps->nwords = 5;
				if (!asic->options.no_follow_ib) {
					uint32_t *data = calloc(ps->ib.size, sizeof(*data));
					if (umr_read_vram(asic, vm_partition, ps->ib.vmid, ps->ib.addr, ps->ib.size * sizeof(*data), data) == 0) {
						ps->next_ib = umr_vpe_decode_stream(asic, vm_partition, from_addr + (((intptr_t)(stream - ostream)) << 2), ps->ib.vmid, data, ps->ib.size);
						if (ps->next_ib) {
							ps->next_ib->from.addr = from_addr + (((intptr_t)(stream - ostream)) << 2);
							ps->next_ib->from.vmid = from_vmid;
						}
					}
					free(data);
				}
				break;
			case 5: // FENCE
				ps->nwords = 3;
				break;
			case 6: // TRAP
				ps->nwords = 1;
				break;
			case 7: // REGISTER WRITE
				ps->nwords = 2;
				break;
			case 8: // POLL_REGMEM
				ps->nwords = ps->sub_opcode ? 3 : 5;
				break;
			case 9: // COND_EXE
				ps->nwords = 4;
				break;
			case 10: // ATOMIC
				ps->nwords = 7;
				break;
			case 13: // TIMESTAMP
				switch (ps->sub_opcode) {
					case 0:
						ps->nwords = 2;
						break;
					case 1:
						ps->nwords = 2;
						break;
					case 2:
						ps->nwords = 2;
						break;
				}
				break;
			default:
				asic->err_msg("[ERROR]: Invalid vpe opcode in umr_vpe_decode_ring(): opcode [%x]\n", (unsigned)ps->opcode);
				umr_free_vpe_stream(ops);
				return NULL;
		}

		if (nwords < 1 + ps->nwords) {
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
		ps->words = calloc(ps->nwords, sizeof(ps->words[0]));
		memcpy(ps->words, stream, ps->nwords * sizeof(ps->words[0]));

		// advance stream
		stream += ps->nwords;
		nwords -= 1 + ps->nwords;

		if (nwords) {
			ps->next = calloc(1, sizeof(*ps));
			prev_ps = ps;
			ps = ps->next;
		}
	}
	return ops;
}

/**
 * umr_free_vpe_stream - Free a vpe stream object
 */
void umr_free_vpe_stream(struct umr_vpe_stream *stream)
{
	while (stream) {
		struct umr_vpe_stream *n;
		n = stream->next;
		if (stream->next_ib)
			umr_free_vpe_stream(stream->next_ib);
		free(stream->words);
		free(stream);
		stream = n;
	}
}

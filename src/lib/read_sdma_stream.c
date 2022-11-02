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

/**
 * umr_sdma_decode_ring - Read a GPU ring and decode into a sdma stream
 *
 * @ringname - Common name of the ring, e.g., 'gfx' or 'comp_1.0.0'
 * @no_halt - Set to 0 to issue an SQ_CMD halt command
 *
 * Return a sdma stream if successful.
 */
struct umr_sdma_stream *umr_sdma_decode_ring(struct umr_asic *asic, struct umr_sdma_stream_decode_ui *ui, char *ringname, int start, int stop)
{
	void *ps;
	uint32_t *ringdata, ringsize;
	int only_active = 1;

	// read ring data and reduce indeices modulo ring size
	// since the kernel returned values might be unwrapped.
	ringdata = umr_read_ring_data(asic, ringname, &ringsize);
	ringsize /= 4;
	ringdata[0] %= ringsize;
	ringdata[1] %= ringsize;

	if (start == -1)
		start = ringdata[0];
	else
		only_active = 0;
	if (stop == -1)
		stop = ringdata[1];
	else
		only_active = 0;

	// only proceed if there is data to read
	// and then linearize it so that the stream
	// decoder can do it's thing
	if (!only_active || start != stop) { // rptr != wptr
		uint32_t *lineardata, linearsize;

		// copy ring data into linear array
		lineardata = calloc(ringsize, sizeof(*lineardata));
		for (linearsize = 0;
		     start != stop && linearsize < ringsize;
		     linearsize++, start = (start + 1) % ringsize)
			lineardata[linearsize] = ringdata[3 + start];  // first 3 words are rptr/wptr/dwptr

		ps = umr_sdma_decode_stream(asic, ui, -1, 0, 0, lineardata, linearsize);
		free(lineardata);
		free(ringdata);
	} else {
		ps = NULL;
	}

	return ps;
}

/**
 * umr_sdma_decode_stream - Decode an array of sdma packets into a sdma stream
 *
 * @vmid:  The VMID (or zero) that this array comes from (if say an IB)
 * @ui: UI callbacks for tracking and modifying parse state (e.g. handling private op codes)
 * @stream: An array of DWORDS which contain the sdma packets
 * @nwords:  The number of words in the stream
 *
 * Returns a sdma stream if successfully decoded.
 */
struct umr_sdma_stream *umr_sdma_decode_stream(struct umr_asic *asic, struct umr_sdma_stream_decode_ui *ui, int vm_partition,
					       uint64_t from_addr, uint32_t from_vmid, uint32_t *stream, uint32_t nwords)
{
	struct umr_sdma_stream *ops, *ps, *prev_ps = NULL;
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
				ps->nwords += (ps->header_dw >> 16) & 0x3FFF;
				break;
			case 1: // COPY
				switch (ps->sub_opcode) {
					case 0: // LINEAR
						ps->nwords = 6;

						// BROADCAST
						if (ps->header_dw & (1UL << 27)) {
							ps->nwords += 2;
						}
						break;
					case 1: // TILED
						if (ps->header_dw & (3UL << 26)) { // L2T Broadcast/F2F
							ps->nwords = asic->family >= FAMILY_AI ? 15 : 14;
						} else {
							ps->nwords = asic->family >= FAMILY_AI ? 12 : 11;
						}
						break;
					case 3: // STRUCTURE/SOA
						ps->nwords = 7;
						break;
					case 4: // LINEAR_SUB_WINDOW
						ps->nwords = 12;
						break;
					case 5: // TILED_SUB_WINDOW
						ps->nwords = asic->family >= FAMILY_NV ? 16 : 13;
						break;
					case 6: // T2T_SUB_WIND
						ps->nwords = asic->family >= FAMILY_NV ? 17 : 14;
						break;
					case 7: // DIRTY_PAGE
						ps->nwords = 6;
						break;
					case 8: // LINEAR_PHY
						ps->nwords = 6;
						ps->nwords += 4 * (stream[0] >> 24);
						break;
					case 16: // LINEAR_BC
						ps->nwords = 6;
						break;
					case 17: // TILED_BC
						if (ps->header_dw & (3UL << 26)) {
							ps->nwords = 15; // L2T Broadcast/F2F
						} else {
							ps->nwords = 12;
						}
						break;
					case 20: // LINEAR_SUB_WINDOW_BC
						ps->nwords = 12;
						break;
					case 21: // TILED_SUB_WIMDOW_BC
						ps->nwords = 13;
						break;
					case 22: // T2T_SUB_WIND_BC
						ps->nwords = 14;
						break;
					case 36: // LINEAR_SUB_WINDOW_LARGE
						ps->nwords = 19;
						break;
				}
				break;
			case 2:  // WRITE
				switch (ps->sub_opcode) {
					case 0: // LINEAR
						ps->nwords = 4;
						ps->nwords += stream[2] & 0xFFFFF;
						break;
					case 1: // TILED
						ps->nwords = 9;
						ps->nwords += stream[7] & 0xFFFFF;
						break;
					case 2: // TILED_BC
						ps->nwords = 9;
						ps->nwords += stream[7] & 0xFFFFF;
						break;
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
					uint32_t *data = calloc(sizeof(*data), ps->ib.size);
					if (umr_read_vram(asic, vm_partition, ps->ib.vmid, ps->ib.addr, ps->ib.size * sizeof(*data), data) == 0) {
						ps->next_ib = umr_sdma_decode_stream(asic, ui, vm_partition, from_addr + (((intptr_t)(stream - ostream)) << 2), ps->ib.vmid, data, ps->ib.size);
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
			case 7: // SEM and MEM_INCR
				ps->nwords = 2;
				break;
			case 8: // POLL_REGMEM
				switch (ps->sub_opcode) {
					case 0: // MEM
						ps->nwords = 5;
						break;
					case 1: // REG
						ps->nwords = 3;
						break;
					case 2: // DBIT
						ps->nwords = 4;
						break;
					case 3: // MEM_VERIFY
						ps->nwords = 12;
						break;
					case 4: // INVALIDATION
						ps->nwords = 3;
						break;
				}
				break;
			case 9: // COND_EXE
				ps->nwords = 4;
				break;
			case 10: // ATOMIC
				ps->nwords = 7;
				break;
			case 11: // FILL
				switch (ps->sub_opcode) {
					case 0: // CONST_FILL
						ps->nwords = 4;
						break;
					case 1: // FILL_MULTI
						ps->nwords = 5;
						break;
				}
				break;
			case 12: // PTE
				switch (ps->sub_opcode) {
					case 0: // GEN_PTEPDE (aka WRITE_INCR)
						ps->nwords = 9;
						break;
					case 1: // COPY_PTEPDE
						ps->nwords = 7;
						break;
					case 2: // RMW_PTEPDE
						ps->nwords = 7;
						break;
				}
				break;
			case 13: // TIMESTAMP
				switch (ps->sub_opcode) {
					case 0: // SET_LOCAL
						ps->nwords = 2;
						break;
					case 1: // GET_LOCAL
						ps->nwords = 2;
						break;
					case 2: // GET_GLOBAL
						ps->nwords = 2;
						break;
				}
				break;
			case 14:
				switch (ps->sub_opcode) {
					case 0: // SRBM_WRITE
						ps->nwords = 2;
						break;
					case 1: // RMW_REGISTER
						ps->nwords = 3;
						break;
				}
				break;
			case 15: // PRE_EXE
				ps->nwords = 1;
				break;
			case 16: // GPUVM_TLB_INV
				ps->nwords = 3;
				break;
			case 17: // GCR
				ps->nwords = 4;
				break;
			default:
				if (!ui || !ui->unhandled_size || ui->unhandled_size(ui, asic, ps)) {
					asic->err_msg("[ERROR]: Invalid SDMA opcode in umr_sdma_decode_ring(): opcode [%x]\n", (unsigned)ps->opcode);
					umr_free_sdma_stream(ops);
					return NULL;
				}
				// Callback succeeded to populate size in stream->nwrods.
				break;
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

struct umr_sdma_stream *umr_sdma_decode_stream_vm(struct umr_asic *asic, struct umr_sdma_stream_decode_ui *ui, int vm_partition, uint32_t vmid, uint64_t addr, uint32_t nwords, enum umr_ring_type rt)
{
	uint32_t *words;
	struct umr_sdma_stream *str;

	(void)rt;

	words = calloc(sizeof *words, nwords);
	if (!words) {
		asic->err_msg("[ERROR]: Out of memory\n");
		return NULL;
	}
	if (umr_read_vram(asic, vm_partition, vmid, addr, nwords * 4, words)) {
		asic->err_msg("[ERROR]: Could not read vram %" PRIx32 "@0x%"PRIx64"\n", vmid, addr);
		free(words);
		return NULL;
	}
	str = umr_sdma_decode_stream(asic, ui, vm_partition, addr, vmid, words, nwords);
	free(words);
	return str;
}

/**
 * umr_free_sdma_stream - Free a sdma stream object
 */
void umr_free_sdma_stream(struct umr_sdma_stream *stream)
{
	while (stream) {
		struct umr_sdma_stream *n;
		n = stream->next;
		if (stream->next_ib)
			umr_free_sdma_stream(stream->next_ib);
		free(stream->words);
		free(stream);
		stream = n;
	}
}

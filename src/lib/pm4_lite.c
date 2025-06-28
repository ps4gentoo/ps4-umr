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

/**
 * umr_pm4_lite_decode_stream - Decode an array of PM4-formed packets into a PM4 stream
 *
 * @vm_partition: What VM partition does it come from (-1 is default)
 * @vmid:  The VMID (or zero) that this array comes from (if say an IB)
 * @stream: An array of DWORDS which contain the PM4 packets
 * @nwords:  The number of words in the stream
 *
 * Returns a PM4 stream if successfully decoded.
 */
struct umr_pm4_stream *umr_pm4_lite_decode_stream(struct umr_asic *asic, int vm_partition, uint32_t vmid, uint32_t *stream, uint32_t nwords)
{
	struct umr_pm4_stream *ops, *ps, *prev_ps = NULL;

	(void)asic;
	(void)vm_partition;
	(void)vmid;

	ps = ops = calloc(1, sizeof *ops);
	if (!ps) {
		asic->err_msg("[ERROR]: Out of memory\n");
		return NULL;
	}

	while (nwords) {
		// fetch basics out of header
		ps->header = *stream;
		ps->pkttype = *stream >> 30;
		ps->n_words = ((*stream >> 16) + 1) & 0x3FFF;

		// grab type specific header data
		if (ps->pkttype == 0)
			ps->pkt0off = *stream & 0xFFFF;
		else
			ps->opcode = (*stream >> 8) & 0xFF;

		if (nwords < 1 + ps->n_words) {
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
		ps->words = calloc(ps->n_words, sizeof(ps->words[0]));
		memcpy(ps->words, &stream[1], ps->n_words * sizeof(stream[0]));

		// advance stream
		nwords -= 1 + ps->n_words;
		stream += 1 + ps->n_words;
		if (nwords) {
			ps->next = calloc(1, sizeof(*ps));
			prev_ps = ps;
			ps = ps->next;
		}
	}

	return ops;
}

#define BITS(x, a, b) (unsigned long)((x >> (a)) & ((1ULL << ((b)-(a)))-1))

static void decode_pkt0(struct umr_asic *asic, struct umr_stream_decode_ui *ui, struct umr_pm4_stream *stream, uint64_t ib_addr, uint32_t ib_vmid)
{
	uint32_t n;
	for (n = 0; n < stream->n_words; n++)
		ui->add_field(ui, ib_addr + 4 * (n + 1), ib_vmid, umr_reg_name(asic, stream->pkt0off + n), stream->words[n], NULL, 16, 32);
}

static void decode_pkt3(struct umr_asic *asic, struct umr_stream_decode_ui *ui, struct umr_pm4_stream *stream, uint64_t ib_addr, uint32_t ib_vmid)
{
	switch (stream->opcode) {
		default:
			if (ui->unhandled)
				ui->unhandled(ui, asic, ib_addr, ib_vmid, stream, UMR_RING_PM4_LITE);
			break;
	}
}

/** umr_pm4_lite_decode_stream_opcodes -- Decode sequence of PM4-formed opcodes from a stream
 *
 * 	asic: The asic this stream belongs to
 * 	ui: The user interface callbacks that will be fed with decoded data
 * 	stream:  The stream of PM4 packets to read
 * 	ib_addr: The address the stream begins at in the VM space
 * 	ib_vmid:  The VMID of the stream
 * 	from_addr: The address of the packet this stream was pointed to from (e.g., address of the IB opcode that points to this)
 * 	from_vmid: The VMID of the packet this stream was pointed to from
 * 	opcodes: The number of opcodes to decode (~0UL for max)
 * 	follow: Boolean controlling whether to follow into IBs pointed to by the stream or not.
 *
 * Returns the address of the stream object of any yet to be decoded opcodes or NULL
 * if the end of the stream has been reached.  In the case opcodes is not set
 * to max this value is a pointer to the next opcode to decode.
 */
struct umr_pm4_stream *umr_pm4_lite_decode_stream_opcodes(struct umr_asic *asic, struct umr_stream_decode_ui *ui, struct umr_pm4_stream *stream, uint64_t ib_addr, uint32_t ib_vmid, uint64_t from_addr, uint64_t from_vmid, unsigned long opcodes, int follow)
{
	uint32_t nwords, ncodes;
	struct umr_pm4_stream *s;
	char opcode_name[16];

	s = stream;
	nwords = 0;
	ncodes = opcodes;
	while (s && ncodes--) {
		nwords += 1 + s->n_words;
		s = s->next;
	}

	ui->start_ib(ui, ib_addr, ib_vmid, from_addr, from_vmid, nwords, 4);
	ncodes = opcodes;
	while (stream && ncodes--) {
		if (stream->pkttype != 3) {
			strcpy(opcode_name, "PKT0");
		} else {
			sprintf(opcode_name, "PKT3=0x%02x\n", stream->opcode);
		}

		ui->start_opcode(ui, ib_addr, ib_vmid, stream->pkttype, stream->opcode, 0, stream->n_words, opcode_name, stream->header, stream->words);

		if (stream->pkttype == 3)
			decode_pkt3(asic, ui, stream, ib_addr, ib_vmid);
		else
			decode_pkt0(asic, ui, stream, ib_addr, ib_vmid);

		if (stream->shader)
			ui->add_shader(ui, asic, ib_addr, ib_vmid, stream->shader);

		if (follow && stream->ib)
			umr_pm4_lite_decode_stream_opcodes(asic, ui, stream->ib, stream->ib_source.addr, stream->ib_source.vmid, ib_addr, ib_vmid, ~0UL, follow);

		ib_addr += 4 + stream->n_words * 4;
		stream = stream->next;
	}
	ui->done(ui);
	return stream;
}

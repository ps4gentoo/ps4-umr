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
#include <umr.h>

static const char *hsa_types[] = {
	"HSA_VENDOR_SPECIFIC",
	"HSA_INVALID",
	"HSA_KERNEL_DISPATCH",
	"HSA_BARRIER_AND",
	"HSA_AGENT_DISPATCH",
	"HSA_BARRIER_OR",
};

#define STR_LOOKUP(str_lut, idx, default) \
	((idx) < sizeof(str_lut) / sizeof(str_lut[0]) ? str_lut[(idx)] : (default))

/**
 * umr_hsa_decode_stream - Decode an array of 32-bit words into an HSA stream
 *
 * @asic: The ASIC the HSA stream is bound to
 * @stream: The array of 32-bit words
 * @nwords: The number of 32-bit words.
 *
 * Returns a pointer to a umr_hsa_stream structure, or NULL on error.
 */
struct umr_hsa_stream *umr_hsa_decode_stream(struct umr_asic *asic, uint32_t *stream, uint32_t nwords)
{
	struct umr_hsa_stream *ms, *oms, *prev_ms = NULL;
	uint32_t n;
	uint16_t t16, *s;

	oms = ms = calloc(1, sizeof *ms);
	if (!ms)
		goto error;

	// hsa uses 16-bit words a lot so let's view the stream that way
	s = (uint16_t *)stream;
	nwords *= 2;

	while (nwords) {
		// read hsa_packet_header_t
		t16 = *s++;
			ms->header = t16;
			ms->type = t16 & 0xFF;
			ms->barrier = (t16 >> 8) & 1;
			ms->acquire_fence_scope = (t16 >> 9) & 3;
			ms->release_fence_scope = (t16 >> 11) & 3;

		// # of **16-bit** words depends on packet type
		switch (ms->type) {
			case 0: // vendor specific
				ms->nwords = 1;
				break;
			case 1: // Invalid
				ms->nwords = 1;
				break;
			case 2: // kernel_dispatch
				ms->nwords = 32; // 31 + header
				break;
			case 3: // barrier_and
				ms->nwords = 32;
				break;
			case 4: // agent_dispatch
				ms->nwords = 32;
				break;
			case 5: // barrier_or
				ms->nwords = 32;
				break;
		}

		// if not enough stream for packet or reach 0, stop parsing
		if (nwords < ms->nwords || !ms->nwords) {
			free(ms);
			if (prev_ms) {
				prev_ms->next = NULL;
			} else {
				oms = NULL;
			}
			return oms;
		}

		ms->words = calloc(ms->nwords - 1, sizeof *(ms->words)); // don't need copy of header
		if (!ms->words)
			goto error;
		for (n = 0; n < ms->nwords - 1; n++) {
			ms->words[n] = *s++;
		}
		nwords -= ms->nwords;
		if (nwords) {
			ms->next = calloc(1, sizeof *(ms->next));
			if (!ms->next)
				goto error;
			prev_ms = ms;
			ms = ms->next;
		}
	}
	return oms;
error:
	asic->err_msg("[ERROR]: Out of memory\n");
	while (oms) {
		free(oms->words);
		ms = oms->next;
		free(oms);
		oms = ms;
	}
	return NULL;
}

static uint32_t fetch_word(struct umr_asic *asic, struct umr_hsa_stream *stream, uint32_t off)
{
	if (off >= stream->nwords) {
		if (!(stream->invalid))
			asic->err_msg("[ERROR]: HSA decoding of type (%"PRIx32") went out of bounds.\n", stream->type);
		stream->invalid = 1;
		return 0;
	} else {
		return stream->words[off];
	}
}

/**
 * umr_hsa_decode_stream_opcodes - decode a stream of HSA packets
 *
 * @asic: The ASIC the HSA packets are bound for
 * @ui: The user interface callback that will present the decoded packets to the user
 * @stream: The pre-processed stream of HSA packets
 * @ib_addr: The base VM address where the packets came from
 * @ib_vmid: The VMID the IB is mapped into
 * @from_addr: The address of the ring/IB that pointed to this HSA IB
 * @from_vmid: The VMID of the ring/IB that pointed to this HSA IB
 * @opcodes: The number of opcodes to decode
 * @follow: Follow any chained IBs
 *
 * Returns the address of the first packet that hasn't been decoded.
 */
struct umr_hsa_stream *umr_hsa_decode_stream_opcodes(struct umr_asic *asic, struct umr_stream_decode_ui *ui, struct umr_hsa_stream *stream, uint64_t ib_addr, uint32_t ib_vmid, unsigned long opcodes)
{
	const char* opcode_name;
	uint32_t i, j;
	uint64_t t64;

// todo: from_* and size
	ui->start_ib(ui, ib_addr, ib_vmid, 0, 0, 0, 0);
	while (stream && opcodes-- && stream->nwords) {
		opcode_name = STR_LOOKUP(hsa_types, stream->type, "HSA_UNK");
		ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->type, 0, stream->nwords, opcode_name, stream->header, stream->words);

		// recall HSA is viewed as 16-bit words which is also use
		// negative "field_size" values
		// even though fetch_word(asic, stream, ) is 32-bits we only store
		// 16-bits per entry
		i = 0;
		ib_addr += 2; // skip over header
		switch (stream->type) {
			case 2: // kernel dispatch
				ui->add_field(ui, ib_addr + 2 * i, ib_vmid, "setup_dimensions", fetch_word(asic, stream, i) & 3, NULL, 10, -16); ++i;
				ui->add_field(ui, ib_addr + 2 * i, ib_vmid, "workgroup_size_x", fetch_word(asic, stream, i), NULL, 10, -16); ++i;
				ui->add_field(ui, ib_addr + 2 * i, ib_vmid, "workgroup_size_y", fetch_word(asic, stream, i), NULL, 10, -16); ++i;
				ui->add_field(ui, ib_addr + 2 * i, ib_vmid, "workgroup_size_z", fetch_word(asic, stream, i), NULL, 10, -16); ++i;
				ui->add_field(ui, ib_addr + 2 * i, ib_vmid, "reserved0", fetch_word(asic, stream, i), NULL, 10, -16); ++i;
				ui->add_field(ui, ib_addr + 2 * i, ib_vmid, "grid_size_x", ((uint32_t)fetch_word(asic, stream, i)) | ((uint32_t)fetch_word(asic, stream, i+1)<<16UL), NULL, 10, -32);
				i += 2;
				ui->add_field(ui, ib_addr + 2 * i, ib_vmid, "grid_size_y", ((uint32_t)fetch_word(asic, stream, i)) | ((uint32_t)fetch_word(asic, stream, i+1)<<16UL), NULL, 10, -32);
				i += 2;
				ui->add_field(ui, ib_addr + 2 * i, ib_vmid, "grid_size_z", ((uint32_t)fetch_word(asic, stream, i)) | ((uint32_t)fetch_word(asic, stream, i+1)<<16UL), NULL, 10, -32);
				i += 2;
				ui->add_field(ui, ib_addr + 2 * i, ib_vmid, "private_segment_size", ((uint32_t)fetch_word(asic, stream, i)) | ((uint32_t)fetch_word(asic, stream, i+1)<<16UL), NULL, 10, -32);
				i += 2;
				ui->add_field(ui, ib_addr + 2 * i, ib_vmid, "group_segment_size", ((uint32_t)fetch_word(asic, stream, i)) | ((uint32_t)fetch_word(asic, stream, i+1)<<16UL), NULL, 10, -32);
				i += 2;
				t64 = ((uint64_t)fetch_word(asic, stream, i)) | ((uint64_t)fetch_word(asic, stream, i+1)<<16ULL) | ((uint64_t)fetch_word(asic, stream, i+2)<<32ULL) | ((uint64_t)fetch_word(asic, stream, i+3)<<48ULL);
				ui->add_field(ui, ib_addr + 2 * i, ib_vmid, "kernel_object", t64, NULL, 16, -64); i += 4;
				t64 = ((uint64_t)fetch_word(asic, stream, i)) | ((uint64_t)fetch_word(asic, stream, i+1)<<16ULL) | ((uint64_t)fetch_word(asic, stream, i+2)<<32ULL) | ((uint64_t)fetch_word(asic, stream, i+3)<<48ULL);
				ui->add_field(ui, ib_addr + 2 * i, ib_vmid, "kernarg_address", t64, NULL, 16, -64); i += 4;
				t64 = ((uint64_t)fetch_word(asic, stream, i)) | ((uint64_t)fetch_word(asic, stream, i+1)<<16ULL) | ((uint64_t)fetch_word(asic, stream, i+2)<<32ULL) | ((uint64_t)fetch_word(asic, stream, i+3)<<48ULL);
				ui->add_field(ui, ib_addr + 2 * i, ib_vmid, "reserved2", t64, NULL, 16, -64); i += 4;
				t64 = ((uint64_t)fetch_word(asic, stream, i)) | ((uint64_t)fetch_word(asic, stream, i+1)<<16ULL) | ((uint64_t)fetch_word(asic, stream, i+2)<<32ULL) | ((uint64_t)fetch_word(asic, stream, i+3)<<48ULL);
				ui->add_field(ui, ib_addr + 2 * i, ib_vmid, "completion_signal", t64, NULL, 16, -64); i += 4;
				break;
			case 4: // agent dispatch
				ui->add_field(ui, ib_addr + 2 * i, ib_vmid, "type", fetch_word(asic, stream, i), NULL, 10, -16); ++i;
				ui->add_field(ui, ib_addr + 2 * i, ib_vmid, "reserved0", ((uint32_t)fetch_word(asic, stream, i)) | ((uint32_t)fetch_word(asic, stream, i+1)<<16UL), NULL, 10, -32);
				i += 2;
				t64 = ((uint64_t)fetch_word(asic, stream, i)) | ((uint64_t)fetch_word(asic, stream, i+1)<<16ULL) | ((uint64_t)fetch_word(asic, stream, i+2)<<32ULL) | ((uint64_t)fetch_word(asic, stream, i+3)<<48ULL);
				ui->add_field(ui, ib_addr + 2 * i, ib_vmid, "return_address", t64, NULL, 16, -64); i += 4;
				for (j = 0; j < 4; j++) {
					char str[32];
					t64 = ((uint64_t)fetch_word(asic, stream, i)) | ((uint64_t)fetch_word(asic, stream, i+1)<<16ULL) | ((uint64_t)fetch_word(asic, stream, i+2)<<32ULL) | ((uint64_t)fetch_word(asic, stream, i+3)<<48ULL);
					sprintf(str, "arg[%"PRIu32"]", j);
					ui->add_field(ui, ib_addr + 2 * i, ib_vmid, str, t64, NULL, 16, -64); i += 4;
				}
				t64 = ((uint64_t)fetch_word(asic, stream, i)) | ((uint64_t)fetch_word(asic, stream, i+1)<<16ULL) | ((uint64_t)fetch_word(asic, stream, i+2)<<32ULL) | ((uint64_t)fetch_word(asic, stream, i+3)<<48ULL);
				ui->add_field(ui, ib_addr + 2 * i, ib_vmid, "reserved2", t64, NULL, 16, -64); i += 4;
				t64 = ((uint64_t)fetch_word(asic, stream, i)) | ((uint64_t)fetch_word(asic, stream, i+1)<<16ULL) | ((uint64_t)fetch_word(asic, stream, i+2)<<32ULL) | ((uint64_t)fetch_word(asic, stream, i+3)<<48ULL);
				ui->add_field(ui, ib_addr + 2 * i, ib_vmid, "completion_signal", t64, NULL, 16, -64); i += 4;
				break;
			case 3: // barrier and test
			case 5: // barrier or test (these have the same decoding)
				ui->add_field(ui, ib_addr + 2 * i, ib_vmid, "reserved0", fetch_word(asic, stream, i), NULL, 10, -16); ++i;
				ui->add_field(ui, ib_addr + 2 * i, ib_vmid, "reserved1", ((uint32_t)fetch_word(asic, stream, i)) | ((uint32_t)fetch_word(asic, stream, i+1)<<16UL), NULL, 10, -32);
				i += 2;
				for (j = 0; j < 5; j++) {
					char str[32];
					t64 = ((uint64_t)fetch_word(asic, stream, i)) | ((uint64_t)fetch_word(asic, stream, i+1)<<16ULL) | ((uint64_t)fetch_word(asic, stream, i+2)<<32ULL) | ((uint64_t)fetch_word(asic, stream, i+3)<<48ULL);
					sprintf(str, "dep_signal[%"PRIu32"]", j);
					ui->add_field(ui, ib_addr + 2 * i, ib_vmid, str, t64, NULL, 16, -64); i += 4;
				}
				t64 = ((uint64_t)fetch_word(asic, stream, i)) | ((uint64_t)fetch_word(asic, stream, i+1)<<16ULL) | ((uint64_t)fetch_word(asic, stream, i+2)<<32ULL) | ((uint64_t)fetch_word(asic, stream, i+3)<<48ULL);
				ui->add_field(ui, ib_addr + 2 * i, ib_vmid, "reserved2", t64, NULL, 16, -64); i += 4;
				t64 = ((uint64_t)fetch_word(asic, stream, i)) | ((uint64_t)fetch_word(asic, stream, i+1)<<16ULL) | ((uint64_t)fetch_word(asic, stream, i+2)<<32ULL) | ((uint64_t)fetch_word(asic, stream, i+3)<<48ULL);
				ui->add_field(ui, ib_addr + 2 * i, ib_vmid, "completion_signal", t64, NULL, 16, -64); i += 4;
				break;
		}

		if (stream->invalid)
			break;

		ib_addr += 2 * (stream->nwords - 1);
		stream = stream->next;
	}
	ui->done(ui);
	(void)i; // silence warnings
	return stream;
}

/**
 * umr_free_hsa_stream - Free a hsa stream object
 */
void umr_free_hsa_stream(struct umr_hsa_stream *stream)
{
	while (stream) {
		struct umr_hsa_stream *n;
		n = stream->next;
		free(stream);
		stream = n;
	}
}

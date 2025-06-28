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

/**
 * The "packet" routines are meant to be a wrapper around all of the
 * different packet functions supported (pm4, sdma, etc).  Ideally,
 * applications should call the umr_packet_*() functions where possible
 * instead of calling the lower level functions directly.
 */


/**
 * umr_packet_decode_buffer - Decode packets from a process mapped buffer
 * @asic: The ASIC model the packet decoding corresponds to
 * @ui: A user interface to provide sizing and other information for unhandled opcodes
 * @from_vmid: Which VMID space did this buffer come from
 * @from_addr: The address this buffer came from
 * @stream: An array of 32-bit words corresponding to the packet data to decode
 * @nwords: How many words are in the @stream array
 * @rt: What type of packets are to be decoded?
 *
 * Returns a pointer to a umr_packet_stream structure if successful.
 */
struct umr_packet_stream *umr_packet_decode_buffer(struct umr_asic *asic, struct umr_stream_decode_ui *ui,
						   uint32_t from_vmid, uint64_t from_addr,
						   uint32_t *stream, uint32_t nwords, enum umr_ring_type rt)
{
	struct umr_packet_stream *str;
	void *p = NULL;

	str = calloc(1, sizeof *str);
	str->type = rt;
	str->ui = ui;
	str->asic = asic;

	switch (rt) {
		case UMR_RING_PM4:
			p = str->stream.pm4 = umr_pm4_decode_stream(asic, asic->options.vm_partition, from_vmid, stream, nwords);
			break;
		case UMR_RING_PM4_LITE:
			p = str->stream.pm4 = umr_pm4_lite_decode_stream(asic, asic->options.vm_partition, from_vmid, stream, nwords);
			break;
		case UMR_RING_SDMA:
			p = str->stream.sdma = umr_sdma_decode_stream(asic, ui, asic->options.vm_partition, from_addr, from_vmid, stream, nwords);
			break;
		case UMR_RING_MES:
			p = str->stream.mes = umr_mes_decode_stream(asic, stream, nwords);
			break;
		case UMR_RING_VPE:
			p = str->stream.vpe = umr_vpe_decode_stream(asic, asic->options.vm_partition, from_addr, from_vmid, stream, nwords);
			break;
		case UMR_RING_UMSCH:
			p = str->stream.umsch = umr_umsch_decode_stream(asic, asic->options.vm_partition, from_addr, from_vmid, stream, nwords);
			break;
		case UMR_RING_HSA:
			p = str->stream.hsa = umr_hsa_decode_stream(asic, stream, nwords);
			break;
		case UMR_RING_VCN_ENC:
			p = str->stream.enc = umr_vcn_enc_decode_stream(asic, stream, nwords);
			break;
		case UMR_RING_VCN_DEC:
			p = str->stream.pm4 = umr_vcn_dec_decode_stream(asic, from_vmid, stream, nwords);
			break;
		case UMR_RING_UNK:
		default:
			free(str);
			asic->err_msg("[BUG]: Invalid ring type in packet_decode_buffer()\n");
			return NULL;
	}

	if (!p) {
		asic->err_msg("[ERROR]: Could not create packet stream object in packet_decode_buffer()\n");
		free(str);
		return NULL;
	}
	str->cont = p;

	return str;
}

/**
 * umr_packet_decode_ring - Decode packets from a system kernel ring
 * @asic: The ASIC model the packet decoding corresponds to
 * @ui: A user interface to provide sizing and other information for unhandled opcodes
 * @ringname: The filename of the ring to read from
 * @halt_waves: Should we issue an SQ_CMD HALT command?
 * @start: Where to start reading from in the rings words
 * @stop: Where to stop reading from in the rings words
 * @rt: What type of packets are to be decoded?
 *
 * Returns a pointer to a umr_packet_stream structure if successful.
 */
struct umr_packet_stream *umr_packet_decode_ring(struct umr_asic *asic, struct umr_stream_decode_ui *ui,
						char *ringname, int halt_waves, int *start, int *stop, enum umr_ring_type rt)
{
	void *ps = NULL;
	uint32_t *ringdata, ringsize;
	int only_active = 1;

	if (rt == UMR_RING_GUESS) {
		// only decode PM4 packets on certain rings
		if (!memcmp(ringname, "gfx", 3) ||
			!memcmp(ringname, "uvd", 3) ||
			!memcmp(ringname, "mes_kiq", 7) ||
			!memcmp(ringname, "kiq", 3) ||
			!memcmp(ringname, "comp", 4)) {
			rt = UMR_RING_PM4;
		} else if (!memcmp(ringname, "vcn_enc", 7) ||
			!memcmp(ringname, "vcn_unified_", 12)) {
			rt = UMR_RING_VCN_ENC;
		} else if (!memcmp(ringname, "vcn_dec", 7)) {
			rt = UMR_RING_VCN_DEC;
		} else if (!memcmp(ringname, "sdma", 4) ||
			   !memcmp(ringname, "page", 4)) {
			rt = UMR_RING_SDMA;
		} else if (!memcmp(ringname, "mes", 3)) {
			rt = UMR_RING_MES;
		} else if (!memcmp(ringname, "vpe", 3)) {
			rt = UMR_RING_VPE;
		} else if (!memcmp(ringname, "umsch", 5)) {
			rt = UMR_RING_UMSCH;
		} else {
			asic->err_msg("[ERROR]: Unknown ring type <%s> for umr_packet_decode_ring()\n", ringname);
			return NULL;
		}
	}

	if (halt_waves && asic->options.halt_waves) {
		strcpy(asic->options.ring_name, ringname);
		umr_sq_cmd_halt_waves(asic, UMR_SQ_CMD_HALT, 100);
	}

	// read ring data and reduce indeices modulo ring size
	// since the kernel returned values might be unwrapped.
	ringdata = asic->ring_func.read_ring_data(asic, ringname, &ringsize);

	if ((*stop != -1) && (uint32_t)(*stop * 4) >= ringsize)
		*stop = (ringsize / 4);

	if (ringdata) {
		ringsize /= 4;
		ringdata[0] %= ringsize;
		ringdata[1] %= ringsize;

		if (*start != -1 || *stop != -1) {
			only_active = 0;
		}

		if (*start == -1 && *stop != -1) {
			*start = ringdata[0]; // use rptr
			*stop   = *start + *stop; // read k words from RPTR
		} else if (*start != -1 && *stop == -1) {
			*stop = ringdata[1]; // use wptr
			*start = *stop - *start; // read k words before WPTR
		} else if (*start == -1 && *stop == -1) {
			*start = ringdata[0]; // use rptr
			*stop = ringdata[1]; // use wptr
		} else if (*start == -1) {
			*start = ringdata[0]; // use rptr
		} else if (*stop == -1) {
			*stop = ringdata[1]; // use wptr
		}

		if (*start < 0) {
			*start = ringsize + *start;
		}

		if ((uint32_t)*stop > ringsize) {
			*stop = *stop - ringsize;
		}

		// only proceed if there is data to read
		// and then linearize it so that the stream
		// decoder can do it's thing
		if (!only_active || *start != *stop) { // rptr != wptr
			int o_start = *start;
			uint32_t *lineardata, linearsize;

			// copy ring data into linear array
			lineardata = calloc(ringsize, sizeof(*lineardata));
			linearsize = 0;
			while (*start != *stop && linearsize < ringsize) {
				lineardata[linearsize++] = ringdata[3 + *start];  // first 3 words are rptr/wptr/dwptr
				*start = (*start + 1) % ringsize;
			}
			*start = o_start;
			ps = umr_packet_decode_buffer(asic, ui, 0, 0, lineardata, linearsize, rt);
			free(lineardata);
		}
	}
	free(ringdata);

	if (halt_waves && asic->options.halt_waves)
		umr_sq_cmd_halt_waves(asic, UMR_SQ_CMD_RESUME, 0);

	return ps;
}

/**
 * umr_packet_decode_vm_buffer - Decode packets from a GPU mapped buffer
 * @asic: The ASIC model the packet decoding corresponds to
 * @ui: A user interface to provide sizing and other information for unhandled opcodes
 * @vmid: Which VMID space did this buffer come from
 * @addr: The address this buffer came from
 * @nwords: How many words are in the @stream array
 * @rt: What type of packets are to be decoded?
 *
 * Returns a pointer to a umr_packet_stream structure if successful.
 */
struct umr_packet_stream *umr_packet_decode_vm_buffer(struct umr_asic *asic, struct umr_stream_decode_ui *ui,
						      uint32_t vmid, uint64_t addr, uint32_t nwords, enum umr_ring_type rt)
{
	uint32_t *words;
	struct umr_packet_stream *str;

	words = calloc(nwords, sizeof *words);
	if (!words) {
		asic->err_msg("[ERROR]: Out of memory\n");
		return NULL;
	}
	if (umr_read_vram(asic, asic->options.vm_partition, vmid, addr, nwords * 4, words)) {
		asic->err_msg("[ERROR]: Could not read vram %" PRIx32 "@0x%"PRIx64"\n", vmid, addr);
		free(words);
		return NULL;
	}
	str = umr_packet_decode_buffer(asic, ui, vmid, addr, words, nwords, rt);
	free(words);
	return str;
}

/**
 * umr_packet_free - Free memory allocated for packet stream
 * @stream: Stream to free memory from.
 *
 */
void umr_packet_free(struct umr_packet_stream *stream)
{
	if (stream) {
		switch (stream->type) {
			case UMR_RING_PM4:
			case UMR_RING_PM4_LITE:
			case UMR_RING_VCN_DEC:
				umr_free_pm4_stream(stream->stream.pm4);
				break;
			case UMR_RING_SDMA:
				umr_free_sdma_stream(stream->stream.sdma);
				break;
			case UMR_RING_MES:
				umr_free_mes_stream(stream->stream.mes);
				break;
			case UMR_RING_VPE:
				umr_free_vpe_stream(stream->stream.vpe);
				break;
			case UMR_RING_UMSCH:
				umr_free_umsch_stream(stream->stream.umsch);
				break;
			case UMR_RING_HSA:
				umr_free_hsa_stream(stream->stream.hsa);
				break;
			case UMR_RING_VCN_ENC:
				umr_free_vcn_enc_stream(stream->stream.enc);
				break;
			case UMR_RING_UNK:
			default:
				stream->asic->err_msg("[BUG]: Invalid ring type in packet_free() call.\n");
		}
		free(stream);
	}
}


/**
 * umr_packet_find_shader - Find a shader or compute kernel in a stream
 * @stream: An array of 32-bit words corresponding to the packet data to decode
 * @vmid: Which VMID space does the kernel belong to
 * @addr: An address inside the kernel program (doesn't have to be start of program)
 *
 * Returns a poiner to a umr_shaders_pgm structure if the shader program is found.
 */
struct umr_shaders_pgm *umr_packet_find_shader(struct umr_packet_stream *stream, unsigned vmid, uint64_t addr)
{
	switch (stream->type) {
		case UMR_RING_PM4:
		case UMR_RING_PM4_LITE:
			return umr_find_shader_in_stream(stream->stream.pm4, vmid, addr);

		case UMR_RING_SDMA:
		case UMR_RING_MES:
		case UMR_RING_VPE:
		case UMR_RING_UMSCH:
		case UMR_RING_HSA:
		case UMR_RING_VCN_ENC:
		case UMR_RING_VCN_DEC:
			stream->asic->err_msg("[BUG]: Cannot find shader in UMSCH, VPE, MES, HSA, or SDMA types of streams\n");
			return NULL;

		case UMR_RING_UNK:
		default:
			stream->asic->err_msg("[BUG]: Invalid ring type in packet_find_shader()\n");
			return NULL;
	}
}

/**
 * umr_packet_disassemble_stream - Disassemble a stream's packets into quantized data
 * @stream: The pre decoded packet stream
 * @ib_addr: The address the stream resides at
 * @ib_vmid: The VMID the stream comes from
 * @from_addr: The address of another packet if any that points to this stream
 * @from_vmid: The VMID of another packet if any that points to this stream
 * @opcodes: How many packets to decode (~0UL for infinite)
 * @follow: Should we follow IBs and BOs to further decode
 * @cont: Are we continuing disassembly or starting at the start of the stream?
 *
 * Returns the pointer to the umr_packet_stream being processed.
 */
struct umr_packet_stream *umr_packet_disassemble_stream(struct umr_packet_stream *stream, uint64_t ib_addr, uint32_t ib_vmid,
							uint64_t from_addr, uint64_t from_vmid, unsigned long opcodes, int follow, int cont)
{
	switch (stream->type) {
		case UMR_RING_PM4:
			stream->cont = umr_pm4_decode_stream_opcodes(stream->asic, stream->ui, cont ? stream->cont : stream->stream.pm4, ib_addr, ib_vmid,
							     from_addr, from_vmid, opcodes, follow);
			break;
		case UMR_RING_PM4_LITE:
			stream->cont = umr_pm4_lite_decode_stream_opcodes(stream->asic, stream->ui, cont ? stream->cont : stream->stream.pm4, ib_addr, ib_vmid,
							     from_addr, from_vmid, opcodes, follow);
			break;
		case UMR_RING_SDMA:
			stream->cont = umr_sdma_decode_stream_opcodes(stream->asic, stream->ui, cont ? stream->cont : stream->stream.sdma, ib_addr, ib_vmid,
							     from_addr, from_vmid, opcodes, follow);
			break;
		case UMR_RING_MES:
			stream->cont = umr_mes_decode_stream_opcodes(stream->asic, stream->ui, cont ? stream->cont : stream->stream.mes, ib_addr, ib_vmid, opcodes);
			break;
		case UMR_RING_VPE:
			stream->cont = umr_vpe_decode_stream_opcodes(stream->asic, stream->ui, cont ? stream->cont : stream->stream.vpe, ib_addr, ib_vmid,
														 from_addr, from_vmid, opcodes, follow);
			break;
		case UMR_RING_UMSCH:
			stream->cont = umr_umsch_decode_stream_opcodes(stream->asic, stream->ui, cont ? stream->cont : stream->stream.umsch, ib_addr, ib_vmid,
														 from_addr, from_vmid, opcodes, follow);
			break;
		case UMR_RING_HSA:
			stream->cont = umr_hsa_decode_stream_opcodes(stream->asic, stream->ui, cont ? stream->cont : stream->stream.hsa, ib_addr, ib_vmid, opcodes);
			break;
		case UMR_RING_VCN_ENC:
			stream->cont = umr_vcn_enc_decode_stream_opcodes(stream->asic, stream->ui, cont ? stream->cont : stream->stream.enc, ib_addr, ib_vmid,
							     from_addr, from_vmid, opcodes, follow);
			break;
		case UMR_RING_VCN_DEC:
			stream->cont = umr_vcn_dec_decode_stream_opcodes(stream->asic, stream->ui, cont ? stream->cont : stream->stream.pm4, ib_addr, ib_vmid,
							     from_addr, from_vmid, opcodes, follow);
			break;
		case UMR_RING_UNK:
		default:
			stream->asic->err_msg("[BUG]: Invalid ring type in packet_disassemble_stream() call.\n");
			return NULL;
	}
	return stream;
}

/**
 * umr_packet_disassemble_stream - Disassemble a stream's packets into quantized data
 * @asic: The ASIC model the packet decoding corresponds to
 * @ui: A user interface cabllack to present data
 * @ib_addr: The address the stream resides at
 * @ib_vmid: The VMID the stream comes from
 * @from_addr: The address of another packet if any that points to this stream
 * @from_vmid: The VMID of another packet if any that points to this stream
 * @follow: Should we follow IBs and BOs to further decode
 * @rt: What type of packets are to be decoded?
 *
 * Returns -1 on error.
 */
int umr_packet_disassemble_opcodes_vm(struct umr_asic *asic, struct umr_stream_decode_ui *ui, uint64_t ib_addr, uint32_t ib_vmid, uint32_t nwords, uint64_t from_addr, uint64_t from_vmid, int follow, enum umr_ring_type rt)
{
	struct umr_packet_stream *str;

	str = umr_packet_decode_vm_buffer(asic, ui, ib_vmid, ib_addr, nwords, rt);
	if (str) {
		umr_packet_disassemble_stream(str, ib_addr, ib_vmid, from_addr, from_vmid, ~0ULL, follow, 0);
		umr_packet_free(str);
		return 0;
	}
	return -1;
}

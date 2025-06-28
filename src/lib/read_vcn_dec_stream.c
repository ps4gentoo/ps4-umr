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
 * Nov. 2024 David Wu added VCN dec ring support
 */

#include <stdbool.h>

#include "umr.h"
#include "import/ac_vcn_dec.h"

static void decode_pkt0(struct umr_asic *asic, struct umr_stream_decode_ui *ui, struct umr_pm4_stream *stream, uint64_t ib_addr, uint32_t ib_vmid)
{
	uint32_t n;
	for (n = 0; n < stream->n_words; n++)
		ui->add_field(ui, ib_addr + 4 * (n + 1), ib_vmid, umr_reg_name(asic, stream->pkt0off + n), stream->words[n], NULL, 16, 32);
}

static struct umr_ip_block * umr_vcn_get_ip_ver(struct umr_asic *asic)
{
	struct umr_ip_block *ip;

	ip = umr_find_ip_block(asic, "vcn", 0); // for multi instance
	if (!ip)
		ip = umr_find_ip_block(asic, "vcn", -1); // for single instance
	if (!ip) {
		asic->err_msg("\n[ERROR]: Cannot get VCN IP info\n");
		return NULL;
	}
	return ip;
}

static uint32_t fetch_word(struct umr_asic *asic, struct umr_pm4_stream *stream, uint32_t off)
{
	if (off >= stream->n_words) {
		if (!(stream->invalid))
			asic->err_msg("[ERROR]: VCN DEC decoding of opcode (%"PRIx32") went out of bounds.\n", stream->opcode);
		stream->invalid = 1;
		return 0;
	} else {
		return stream->words[off];
	}
}

static char * get_cmd_name(uint32_t cmd)
{
	switch ((cmd & ~VCN_DEC_KMD_CMD) >> 1){
	case VCN_DEC_CMD_FENCE: return "VCN_DEC_CMD_FENCE";
	case VCN_DEC_CMD_TRAP: return "VCN_DEC_CMD_TRAP";
	case VCN_DEC_CMD_WRITE_REG: return "VCN_DEC_CMD_WRITE_REG";
	case VCN_DEC_CMD_REG_READ_COND_WAIT: return "VCN_DEC_CMD_REG_READ_COND_WAIT";
	case VCN_DEC_CMD_PACKET_START: return "VCN_DEC_CMD_PACKET_START";
	case VCN_DEC_CMD_PACKET_END: return "VCN_DEC_CMD_PACKET_END";
	default: return NULL;
	}
}

static char * get_msg_cmd_name(uint32_t cmd)
{
	switch (cmd >> 1){
	case RDECODE_CMD_MSG_BUFFER: return "RDECODE_CMD_MSG_BUFFER";
	case RDECODE_CMD_DPB_BUFFER: return "RDECODE_CMD_DPB_BUFFER";
	case RDECODE_CMD_DECODING_TARGET_BUFFER: return "RDECODE_CMD_DECODING_TARGET_BUFFER";
	case RDECODE_CMD_FEEDBACK_BUFFER: return "RDECODE_CMD_FEEDBACK_BUFFER";
	case RDECODE_CMD_PROB_TBL_BUFFER: return "RDECODE_CMD_PROB_TBL_BUFFER";
	case RDECODE_CMD_SESSION_CONTEXT_BUFFER: return "RDECODE_CMD_SESSION_CONTEXT_BUFFER";
	case RDECODE_CMD_BITSTREAM_BUFFER: return "RDECODE_CMD_BITSTREAM_BUFFER";
	case RDECODE_CMD_IT_SCALING_TABLE_BUFFER: return "RDECODE_CMD_IT_SCALING_TABLE_BUFFER";
	case RDECODE_CMD_CONTEXT_BUFFER: return "RDECODE_CMD_CONTEXT_BUFFER";
	default: return NULL;
	}
}

/*
 * Renoir - VCN 2.2
 */
static char *get_reg_name(struct umr_pm4_stream *s, uint32_t major, uint32_t minor)
{
	if(major == 2 && (minor == 0 || minor == 2)) { // vcn 2.0, 2.2
		switch (s->pkt0off){
		case 0x503:
			return "mmUVD_GPCOM_VCPU_CMD";
			/* the next word is VCN_DEC_CMD_PACKET_START or VCN_DEC_CMD_PACKET_END */
		case 0x504:
			return "mmUVD_GPCOM_VCPU_DATA0";
		case 0x505:
			return "mmUVD_GPCOM_VCPU_DATA1";
		case 0x54c:
			return "mmUVD_GPCOM_VCPU_DATA2";
		case 0x506:
			return "mmUVD_ENGINE_CNTL";
		case 0x1fd:
			return "mmUVD_CONTEXT_ID";
		case 0x53f:
		case 0x81ff:
			return "mmUVD_NO_OP";
		case 0x54a:
			return "mmUVD_GP_SCRATCH8";
		case 0xc01d:
			return "mmUVD_SCRATCH9";
		case 0x1e1:
			return "mmUVD_LMI_RBC_IB_VMID";
		case 0x5a7:
			return "mmUVD_LMI_RBC_IB_64BIT_BAR_LOW";
		case 0x5a6:
			return "mmUVD_LMI_RBC_IB_64BIT_BAR_HIGH";
		case 0x1e2:
			return "mmUVD_RBC_IB_SIZE";
		default:
			return "UNK";
		}
	} else switch (s->pkt0off){ // vcn 2.x and 3
	case 0x0f:
		return "mmUVD_GPCOM_VCPU_CMD";
		/* the next word is VCN_DEC_CMD_PACKET_START or VCN_DEC_CMD_PACKET_END */
	case 0x10:
		return "mmUVD_GPCOM_VCPU_DATA0";
	case 0x11:
		return "mmUVD_GPCOM_VCPU_DATA1";
	case 0x68:
		return "mmUVD_GPCOM_VCPU_DATA2";
	case 0x27:
		return "mmUVD_CONTEXT_ID";
	case 0x29:
	case 0x81ff:
		return "mmUVD_NO_OP";
	case 0x66:
		return "mmUVD_GP_SCRATCH8";
	case 0xc01d:
		return "mmUVD_SCRATCH9";
	case 0x431:
		return "mmUVD_LMI_RBC_IB_VMID";
	case 0x3b4:
		return "mmUVD_LMI_RBC_IB_64BIT_BAR_LOW";
	case 0x3b5:
		return "mmUVD_LMI_RBC_IB_64BIT_BAR_HIGH";
	case 0x25c:
		return "mmUVD_RBC_IB_SIZE";
	case 0x26d:
		return "mmUVD_ENGINE_CNTL";
	default:
		return "UNK";
	}
}

static bool is_valid_PACKET0(struct umr_pm4_stream *ps, uint32_t major, uint32_t minor)
{
	//if we cannot recognize it then we cannot handle it
	return strcmp(get_reg_name(ps, major, minor), "UNK");
}


/**
 * umr_vcn_dec_decode_stream - Decode an array of VCN DEC (PM4 type 0) packets into a PM4 stream
 *
 * @vm_partition: What VM partition does it come from (-1 is default)
 * @vmid:  The VMID (or zero) that this array comes from (if say an IB)
 * @stream: An array of DWORDS which contain the VCN DEC packets
 * @nwords:  The number of words in the stream
 *
 * Returns a PM4 stream if successfully decoded.
 */
struct umr_pm4_stream *umr_vcn_dec_decode_stream(struct umr_asic *asic, uint32_t vmid, uint32_t *stream, uint32_t nwords)
{
	struct umr_pm4_stream *ops, *ps, *prev_ps = NULL;
	struct umr_vcn_cmd_message *vcn, *vcn_head = NULL;
	uint32_t *ostream = stream;
	struct umr_ip_block *ip;
	uint32_t o_nwords = nwords;
	char *name;
	struct {
		int n;
		uint32_t
			size,
			vmid;
		uint64_t
			addr;
	} uvd_ib;

	struct {
		int n;
		uint32_t cmd;
		uint64_t addr;
	} dec_ib = {0, 0, 0};

	ip = umr_vcn_get_ip_ver(asic);

	if (!ip)
		return NULL;

	if (ip->discoverable.maj > 3) { // should not come here
		asic->err_msg("[ERROR]: Only apply to VCN 1/2/3. VCN %d should use unified ring\n", ip->discoverable.maj);
		return NULL;
	}
	ps = ops = calloc(1, sizeof *ops);
	if (!ps) {
		asic->err_msg("[ERROR]: Out of memory\n");
		return NULL;
	}

	memset(&uvd_ib, 0, sizeof uvd_ib);

	while (nwords) {
		// only PACKET0
		ps->header = *stream;
		ps->pkttype = *stream >> 30;
		ps->n_words = ((*stream >> 16) + 1) & 0x3FFF;
		ps->opcode = ps->pkt0off = *stream & 0xFFFF;

		// check if this is a valid PACKET0
		if (ip->discoverable.maj != 1 && !is_valid_PACKET0(ps, ip->discoverable.maj, ip->discoverable.min)){
			asic->err_msg("[ERROR]: unknown packet[0x%08x] at offset[0x%04x]\n", ps->header, o_nwords - nwords);
			ps->n_words = 0;
			ps->pkt0off = 0;
		}
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
		if (ps->n_words) {
			ps->words = calloc(ps->n_words, sizeof(ps->words[0]));
			memcpy(ps->words, &stream[1], ps->n_words * sizeof(stream[0]));
		}

		if (ps->n_words) {
			if (ip->discoverable.maj == 1) { // VCN 2 and 3 cases
				name = umr_reg_name(asic, ps->pkt0off);
			} else {
				name = get_reg_name(ps, ip->discoverable.maj, ip->discoverable.min);
			}
			// look for UVD IBs which are marked by 3-4 distinct
			// register writes.  They can occur in any order
			// except for the SIZE so we use a bitfield to keep
			// track of them
			if (strstr(name, "mmUVD_LMI_RBC_IB_VMID")) {
				uvd_ib.vmid = fetch_word(asic, ps, 0) | ((asic->family <= FAMILY_VI) ? 0 : UMR_MM_HUB);
				uvd_ib.n |= 1;
			} else if (strstr(name, "mmUVD_LMI_RBC_IB_64BIT_BAR_LOW")) {
				uvd_ib.addr |= fetch_word(asic, ps, 0);
				uvd_ib.n |= 2;
			} else if (strstr(name, "mmUVD_LMI_RBC_IB_64BIT_BAR_HIGH")) {
				uvd_ib.addr |= (uint64_t)fetch_word(asic, ps, 0) << 32;
				uvd_ib.n |= 4;
			} else if (strstr(name, "mmUVD_RBC_IB_SIZE")) {
				uvd_ib.size = fetch_word(asic, ps, 0) * 4;
				uvd_ib.n |= 8;
			}

			// look for VCN IB Messages which are marked by 3 distinct
			// register writes.  Assume 2 of them can occur in any order
			// except for the CMD. It is not valid if addr is not fully
			// set before CMD register.
			if (!asic->options.no_follow_ib && (vmid & ~UMR_MM_HUB)) {
				if (strstr(name, "mmUVD_GPCOM_VCPU_DATA0")) {
					dec_ib.addr |= (uint64_t)fetch_word(asic, ps, 0);
					dec_ib.n |= 1;
				} else if (strstr(name, "mmUVD_GPCOM_VCPU_DATA1")) {
					dec_ib.addr |= (uint64_t)fetch_word(asic, ps, 0) << 32;
					dec_ib.n |= 2;
				} else if (strstr(name, "mmUVD_GPCOM_VCPU_CMD")) {
					if (dec_ib.n == (1 | 2)) {
						dec_ib.cmd = fetch_word(asic, ps, 0) >> 1;
						if (dec_ib.cmd == 0) {
							uint32_t size = sizeof(rvcn_dec_message_header_t);
							rvcn_dec_message_header_t *mh = calloc(1, size);
							vcn = calloc(1, sizeof(struct umr_vcn_cmd_message));
							vcn->vmid = vmid;
							vcn->addr = dec_ib.addr;
							vcn->cmd = dec_ib.cmd;
							vcn->type = 0;
							vcn->from = (stream - ostream  - 1) * 4;  /* back 1 dwords to mmUVD_GPCOM_VCPU_DATA1 */
							if (umr_read_vram(asic, asic->options.vm_partition, vcn->vmid, vcn->addr, size, mh) < 0) {
								asic->err_msg("[ERROR]: Could not read IB at 0x%"PRIx32":0x%" PRIx64 "\n", vcn->vmid, vcn->addr);
								free(vcn);
							} else {
								vcn->size = mh->total_size < mh->header_size ? mh->header_size : mh->total_size;
								if (!ops->vcn)
									ops->vcn = vcn;
								else
									vcn_head->next = vcn;
								vcn_head = vcn;
							}
							free(mh);
						}
					}
					/* reset for next IB message if any */
					dec_ib.addr = 0;
					dec_ib.n = 0;
				}
			}

			// if we have everything but the VMID assume vmid 0
			if (uvd_ib.n == (2|4|8)) {
				uvd_ib.vmid = 0;
				uvd_ib.n = 15;
			}

			// we have everything we need to point to an IB
			if (!asic->options.no_follow_ib && uvd_ib.n == 15) {
				void *buf;
				buf = calloc(1, uvd_ib.size);
				if (umr_read_vram(asic, asic->options.vm_partition, uvd_ib.vmid, uvd_ib.addr, uvd_ib.size, buf) < 0) {
					asic->err_msg("[ERROR]: Could not read IB at 0x%"PRIx32":0x%" PRIx64 "\n", uvd_ib.vmid, uvd_ib.addr);
				} else {
					ps->ib = umr_vcn_dec_decode_stream(asic, uvd_ib.vmid, buf, uvd_ib.size / 4);
					ps->ib_source.addr = uvd_ib.addr;
					ps->ib_source.vmid = uvd_ib.vmid;
				}
				free(buf);
				memset(&uvd_ib, 0, sizeof uvd_ib);
			}
		}

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

/** umr_vcn_dec_decode_stream_opcodes -- Decode sequence of ENC opcodes from a stream
 *
 * 	@asic: The asic this stream belongs to
 * 	@ui: The user interface callbacks that will be fed with decoded data
 * 	@stream:  The stream of VCN dec ring packets (PM4) to read
 * 	@ib_addr: The address the stream begins at in the VM space
 * 	@ib_vmid:  The VMID of the stream
 * 	@from_addr: The address of the packet this stream was pointed to from (e.g., address of the IB opcode that points to this)
 * 	@from_vmid: The VMID of the packet this stream was pointed to from
 * 	@opcodes: The number of opcodes to decode (~0UL for max)
 * 	@follow: Boolean controlling whether to follow into IBs pointed to by the stream or not.
 *
 * Returns the address of the stream object of any yet to be decoded opcodes or NULL
 * if the end of the stream has been reached.  In the case opcodes is not set
 * to max this value is a pointer to the next opcode to decode.
 *
 */
struct umr_pm4_stream *umr_vcn_dec_decode_stream_opcodes(struct umr_asic *asic, struct umr_stream_decode_ui *ui,
		struct umr_pm4_stream *stream, uint64_t ib_addr, uint32_t ib_vmid, uint64_t from_addr,
		uint64_t from_vmid, unsigned long opcodes, int follow)
{
	uint32_t nwords, ncodes;
	struct umr_pm4_stream *s;
	const char *opcode_name;
	uint64_t oib_addr = ib_addr;
	struct umr_ip_block *ip;

	s = stream;
	nwords = 0;
	ncodes = opcodes;
	while (s && ncodes--) {
		nwords += 1 + s->n_words;
		s = s->next;
	}

	ip = umr_vcn_get_ip_ver(asic);

	if (!ip)
		return NULL;

	ui->start_ib(ui, ib_addr, ib_vmid, from_addr, from_vmid, nwords, 4);
	ncodes = opcodes;
	while (stream && ncodes--) {
		if (ip->discoverable.maj == 1)
			opcode_name = umr_reg_name(asic, stream->pkt0off);
		else
			opcode_name = get_reg_name(stream, ip->discoverable.maj, ip->discoverable.min);

		ui->start_opcode(ui, ib_addr, ib_vmid, stream->pkttype, stream->opcode, 0, stream->n_words, opcode_name, stream->header, stream->words);

		if (ip->discoverable.maj == 1)
			decode_pkt0(asic, ui, stream, ib_addr, ib_vmid);
		else {
			if (strstr(opcode_name, "mmUVD_GPCOM_VCPU_CMD")) {
				char * name = ib_vmid ? get_msg_cmd_name(stream->words[0]) : get_cmd_name(stream->words[0]);
				ui->add_field(ui, ib_addr + 4, ib_vmid, name ? name : opcode_name, (stream->words[0] & ~VCN_DEC_KMD_CMD) >> 1, NULL, 16, 32);
			} else if (stream->n_words)
				ui->add_field(ui, ib_addr + 4, ib_vmid, opcode_name, stream->words[0], NULL, 16, 32);
		}
		if (stream->invalid)
			break;

		if (follow && stream->ib)
			umr_vcn_dec_decode_stream_opcodes(asic, ui, stream->ib, stream->ib_source.addr, stream->ib_source.vmid, ib_addr, ib_vmid, ~0UL, follow);

		if (follow && stream->vcn && ui->add_vcn) {
			stream->vcn->from += oib_addr;
			ui->add_vcn(ui, asic, stream->vcn);
		}

		ib_addr += 4 + stream->n_words * 4;
		stream = stream->next;
	}

	ui->done(ui);
	return stream;
}

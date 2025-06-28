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
 * Oct. 2024 David Wu added VCN encode and decode support
 */
#include "umr.h"
#include <inttypes.h>

#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <stdbool.h>
#include "import/ac_vcn_enc.h"

#define MAX_VALUE_SIZE 32 /* enough for a 32 bit hex string - 11 total at most: 8 bytes + 0x + null */
#define FORMAT10  "=%"PRIu32
#define FORMAT16  "=0x%"PRIx32
#define CFORMAT10 "=%s%"PRIu32"%s"
#define CFORMAT16 "=%s0x%"PRIx32"%s"
#define CFORMAT_32b "\n[%s0x%"PRIx32"%s@%s0x%08"PRIx64"%s + %s0x%04"PRIx32"%s]\t[%s%8s0x%08"PRIx32"%s]\t|---> "

#define COMPARE_ERROR(a, b) \
	{ if (a != b) fprintf(pOut?pOut:stderr, "\n[ERROR] FIXME - %s size(%d - %d) does not match!", __func__, a, b); }

#define STRUCT_WARNING(type) \
	if (sizeof (type) != ib_size - 8) { \
		fprintf(pOut?pOut:stderr, "\nFIXME: %s incorrect strucure(%s) size [%zd] should be [%d] bytes\n",\
			       __func__, #type, sizeof(type), ib_size - 8);\
	}

#define ADD_IB_TYPE(type) \
	add_field_2(asic, vmid, vcn_addr, &offset, "IB_TYPE", #type, "", ib_type, 0, pOut, pBuf);

struct vcn_gui_message {
	uint32_t *in_buf;
	uint32_t size;
	char *** out_buf;
};

static void vcn_enc_decode_ib_v1(struct umr_asic *asic, struct umr_vcn_cmd_message *vcn, FILE *pOut, struct umr_ip_block *ip, struct vcn_gui_message *vgui);
static void vcn_enc_decode_ib(struct umr_asic *asic, struct umr_vcn_cmd_message *vcn, FILE *pOut, struct umr_ip_block *ip, struct vcn_gui_message *vgui);

static void add_field_1(struct umr_asic *asic, uint32_t vmid, uint64_t addr, uint32_t *offset,
			char *str, uint32_t value, uint32_t ideal_radix, FILE *fp, char ***buf)
{
	if (buf) {
		char *t = calloc(1, strlen(str) + MAX_VALUE_SIZE);
		int r = 0;
		char *p = t;
		(*buf)[*offset/4] = t;
		r = sprintf(p, "%s", str);
		p += r;
		if (ideal_radix == 10)
			sprintf(p, FORMAT10, value);
		else if (ideal_radix == 16)
			sprintf(p, FORMAT16, value);
	}
	if (fp) {
		fprintf(fp, CFORMAT_32b "%s",
			BLUE, vmid, RST,
			YELLOW, addr, RST,
			YELLOW, *offset, RST,
			BMAGENTA, "", value, RST, str);
		if (ideal_radix == 10)
			fprintf(fp, CFORMAT10, BBLUE, value, RST);
		else if (ideal_radix == 16)
			fprintf(fp, CFORMAT16, YELLOW, value, RST);
	}
	*offset += 4;
}

static void add_field_2(struct umr_asic *asic, uint32_t vmid, uint64_t addr, uint32_t *offset, const char *str,
			const char *str1, const char *str2, uint32_t value, uint32_t ideal_radix, FILE *fp, char ***buf)
{
	if (buf) {
		char *t = calloc(1, strlen(str) + strlen(str1) + strlen(str2) + MAX_VALUE_SIZE);
		char *p = t;
		int r = 0;
		(*buf)[*offset/4] = t;
		if (ideal_radix == 0)
			r = sprintf(p, "%s(%s%s%s)", str, str1, str2[0]? "," : "", str2);
		else
			r = sprintf(p, "%s%s%s", str, str1, str2);
		p += r;
		if (ideal_radix == 10)
			sprintf(p, FORMAT10, value);
		else if (ideal_radix == 16)
			sprintf(p, FORMAT16, value);
	}
	if (fp) {
		if (ideal_radix == 0)
			fprintf(fp, CFORMAT_32b "%s(%s%s%s)",
				BLUE, vmid, RST,
				YELLOW, addr, RST,
				YELLOW, *offset, RST,
				BMAGENTA, "", value, RST, str, str1, str2[0]? "," : "", str2);
		else
			fprintf(fp, CFORMAT_32b "%s%s%s",
				BLUE, vmid, RST,
				YELLOW, addr, RST,
				YELLOW, *offset, RST,
				BMAGENTA, "", value, RST, str, str1, str2);
		if (ideal_radix == 10)
			fprintf(fp, CFORMAT10, BBLUE, value, RST);
		else if (ideal_radix == 16)
			fprintf(fp, CFORMAT16, YELLOW, value, RST);
	}
	*offset += 4;
}

static void add_field_3(struct umr_asic *asic, uint32_t vmid, uint64_t addr, uint32_t *offset, char *str,
			uint32_t idx, uint32_t value, uint32_t ideal_radix, FILE *fp, char ***buf)
{
	if (buf) {
		char *t = calloc(1, strlen(str) + MAX_VALUE_SIZE);
		char *p = t;
		int r = 0;
		(*buf)[*offset/4] = t;
		r = sprintf(p, "%s[%d]", str, idx);
		p += r;
		if (ideal_radix == 10)
			sprintf(p, FORMAT10, value);
		else if (ideal_radix == 16)
			sprintf(p, FORMAT16, value);
	}
	if (fp) {
		fprintf(fp, CFORMAT_32b "%s[%d]",
			BLUE, vmid, RST,
			YELLOW, addr, RST,
			YELLOW, *offset, RST,
			BMAGENTA, "", value, RST, str, idx);
		if (ideal_radix == 10)
			fprintf(fp, CFORMAT10, BBLUE, value, RST);
		else
			fprintf(fp, CFORMAT16, YELLOW, value, RST);
	}
	*offset += 4;
}

static void add_field_4(struct umr_asic *asic, uint32_t vmid, uint64_t addr, uint32_t *offset, char *name,
			uint32_t idx, char *str, uint32_t value, uint32_t ideal_radix, FILE *fp, char ***buf)
{
	if (buf) {
		char *t = calloc(1, strlen(str) + strlen(name) + MAX_VALUE_SIZE);
		char *p = t;
		int r = 0;
		(*buf)[*offset/4] = t;
		if (ideal_radix == 0)
			r = sprintf(p, "%s[%d](%s)", name, idx, str);
		else
			r = sprintf(p, "%s[%d] %s", name, idx, str);
		p += r;
		if (ideal_radix == 10)
			sprintf(p, FORMAT10, value);
		else if (ideal_radix == 16)
			sprintf(p, FORMAT16, value);
	}
	if (fp) {
		if (ideal_radix == 0)
			fprintf(fp, CFORMAT_32b "%s[%d](%s)",
				BLUE, vmid, RST,
				YELLOW, addr, RST,
				YELLOW, *offset, RST,
				BMAGENTA, "", value, RST, name, idx, str);
		else
			fprintf(fp, CFORMAT_32b "%s[%d] %s",
				BLUE, vmid, RST,
				YELLOW, addr, RST,
				YELLOW, *offset, RST,
				BMAGENTA, "", value, RST, name, idx, str);
		if (ideal_radix == 10)
			fprintf(fp, CFORMAT10, BBLUE, value, RST);
		else if (ideal_radix == 16)
			fprintf(fp, CFORMAT16, YELLOW, value, RST);
	}
	*offset += 4;
}

static void add_field_bit(struct umr_asic *asic, uint32_t offset, char *name, uint32_t value,
			  char *bits, uint32_t ideal_radix, FILE *fp, char ***buf)
{
	if (buf) {
		char *t0 = (*buf)[offset/4];
		int size = strlen(t0) + strlen(name) + strlen(bits) + MAX_VALUE_SIZE;
		char *t = calloc(1, size);
		char *p = t;
		(*buf)[offset/4] = t;
		if (ideal_radix == 10)
			sprintf(p, "%s,bits[%s] %s=%"PRIu32, t0, bits, name, value);
		else
			sprintf(p, "%s,bits[%s] %s=0x%"PRIx32, t0, bits, name, value);
		free(t0);
	}
	if (fp) {
		if (ideal_radix == 10)
			fprintf(fp, ",%sbits[%s] %s=%"PRIu32"%s", BBLUE, bits, name, value, RST);
		else
			fprintf(fp, ",%sbits[%s] %s=0x%"PRIx32"%s", BBLUE, bits, name, value, RST);
	}
}

static void dump_ib(struct umr_asic *asic, struct umr_vcn_cmd_message *vcn, uint32_t *buf, uint32_t size, FILE *fp)
{
	uint32_t j = 0;
	while (j < size) {
		fprintf(fp, CFORMAT_32b,
			BLUE, vcn->vmid, RST,
			YELLOW, vcn->addr, RST,
			YELLOW, (uint32_t)(buf - vcn->buf) * 4, RST,
			BMAGENTA, "", *buf, RST);
		j++;
		buf++;
	}
	fprintf(fp, "\n");
}

static void decode_enc_ring(struct umr_asic *asic, struct umr_stream_decode_ui *ui,
			    struct umr_vcn_enc_stream *stream, uint64_t ib_addr, uint32_t ib_vmid)
{
	switch (stream->opcode){
	case VCN_ENC_CMD_IB:
		ui->add_field(ui, ib_addr + 4, ib_vmid, "VMID", stream->words[0], NULL, 16, 32);
		ui->add_field(ui, ib_addr + 8, ib_vmid, "GPU_ADDR_LO32", stream->words[1], NULL, 16, 32);
		ui->add_field(ui, ib_addr + 12, ib_vmid, "GPU_ADDR_HI32", stream->words[2], NULL, 16, 32);
		ui->add_field(ui, ib_addr + 16, ib_vmid, "IB SIZE IN DWORDS", stream->words[3], NULL, 16, 32);
		break;
	case VCN_ENC_CMD_REG_WAIT:
		ui->add_field(ui, ib_addr + 4, ib_vmid, umr_reg_name(asic, stream->words[0] >> 2), stream->words[0], NULL, 16, 32);
		ui->add_field(ui, ib_addr + 8, ib_vmid, "MASK", stream->words[1], NULL, 16, 32);
		ui->add_field(ui, ib_addr + 12, ib_vmid, "WAIT TIME", stream->words[2], NULL, 16, 32);
		break;
	case VCN_ENC_CMD_FENCE:
		ui->add_field(ui, ib_addr + 4, ib_vmid, "ADDR_LO32", stream->words[0], NULL, 16, 32);
		ui->add_field(ui, ib_addr + 8, ib_vmid, "ADDR_HI32", stream->words[1], NULL, 16, 32);
		ui->add_field(ui, ib_addr + 12, ib_vmid, "SEQUENCE NUMBER", stream->words[2], NULL, 16, 32);
		ui->add_field(ui, ib_addr + 16, ib_vmid, "VCN_ENC_CMD_TRAP", stream->words[3], NULL, 16, 32);
		break;
	case VCN_ENC_CMD_REG_WRITE:
		ui->add_field(ui, ib_addr + 4, ib_vmid, umr_reg_name(asic, stream->words[0] >> 2), stream->words[0], NULL, 16, 32);
		ui->add_field(ui, ib_addr + 8, ib_vmid, "VALUE TO WRITE", stream->words[1], NULL, 16, 32);
		break;
	case VCN_ENC_CMD_END:
	case VCN_ENC_CMD_NO_OP:
	case VCN_ENC_CMD_TRAP:
		break;
	default:
		asic->err_msg("[ERROR]: unknown opcode [0x%0x] \n", stream->opcode);
		break;
	}
}

/** umr_vcn_enc_decode_stream_opcodes -- Decode sequence of ENC opcodes from a stream
 *
 * 	@asic: The asic this stream belongs to
 * 	@ui: The user interface callbacks that will be fed with decoded data
 * 	@stream:  The stream of VCN unified ring packets to read
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
struct umr_vcn_enc_stream *umr_vcn_enc_decode_stream_opcodes(struct umr_asic *asic, struct umr_stream_decode_ui *ui,
		struct umr_vcn_enc_stream *stream, uint64_t ib_addr, uint32_t ib_vmid, uint64_t from_addr,
		uint64_t from_vmid, unsigned long opcodes, int follow)
{
	uint32_t nwords, ncodes;
	struct umr_vcn_enc_stream *s;
	const char *opcode_name;
	uint64_t oib_addr = ib_addr;

	s = stream;
	nwords = 0;
	ncodes = opcodes;
	while (s && ncodes--) {
		nwords += 1 + s->nwords;
		s = s->next;
	}
	s = stream;

	ui->start_ib(ui, ib_addr, ib_vmid, from_addr, from_vmid, nwords, 0);
	ncodes = opcodes;
	while (stream && ncodes--) {
		switch (stream->opcode) {
			case VCN_ENC_CMD_IB:
				opcode_name = "VCN_ENC_CMD_IB";
				break;
			case VCN_ENC_CMD_REG_WAIT:
				opcode_name = "VCN_ENC_CMD_REG_WAIT";
				break;
			case VCN_ENC_CMD_END:
				opcode_name = "VCN_ENC_CMD_END";
				break;
			case VCN_ENC_CMD_NO_OP:
				opcode_name = "VCN_ENC_CMD_NO_OP";
				break;
			case VCN_ENC_CMD_FENCE:
				opcode_name = "VCN_ENC_CMD_FENCE";
				break;
			case VCN_ENC_CMD_TRAP: /* in case - should not be separated without fence */
				opcode_name = "VCN_ENC_CMD_TRAP";
				break;
			case VCN_ENC_CMD_REG_WRITE:
				opcode_name = "VCN_ENC_CMD_REG_WRITE";
				break;
			default:
				opcode_name = "Unknown";
				break;
		}
		ui->start_opcode(ui, ib_addr, ib_vmid, 0, stream->opcode, 0, stream->nwords, opcode_name, stream->opcode, stream->words);
		decode_enc_ring(asic, ui, stream, ib_addr, ib_vmid);

		if (stream->invalid)
			break;

		if (follow && stream->vcn && ui->add_vcn) {
			stream->vcn->from += oib_addr;
			ui->add_vcn(ui, asic, stream->vcn);
		}

		ib_addr += (1 + stream->nwords) * 4;
		stream = stream->next;
	}

	ui->done(ui);
	return stream;
}

static void print_ib_signature(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset,
			       void *p_curr, uint32_t ib_size, FILE *pOut, char ***pBuf)
{
	vcn_cmn_signature_t *p = (vcn_cmn_signature_t *)p_curr;
	uint32_t o_offset = offset;

	add_field_1(asic, tvmid, addr, &offset, "IB_CHECKSUM", p->ib_checksum, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "NUMBER_OF_DWORDS", p->num_dwords, 10, pOut, pBuf);

	COMPARE_ERROR((offset - o_offset), ib_size - 8);
	STRUCT_WARNING(vcn_cmn_signature_t);
}

static void print_ib_engine_info(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset,
				 void *p_curr, uint32_t ib_size, FILE *pOut, char ***pBuf)
{
	vcn_cmn_engine_info_t *p = (vcn_cmn_engine_info_t *)p_curr;
	char *engine_type[] = {"invalid", "common", "encode", "decode", "encode_queue"};
	uint32_t o_offset = offset;

	add_field_2(asic, tvmid, addr, &offset, "ENGINE_TYPE", p->engine_type > 4 ? "unknown": engine_type[p->engine_type], "", p->engine_type, 0, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "SIZE_OF_PACKAGES", p->size_of_packages, 10, pOut, pBuf);

	COMPARE_ERROR((offset - o_offset), ib_size - 8);
	STRUCT_WARNING(vcn_cmn_engine_info_t);
}

static void print_ib_session_info(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset,
				  void *p_curr, uint32_t ib_size, FILE *pOut, char ***pBuf)
{
	rvcn_enc_session_info_t *p = (rvcn_enc_session_info_t *)p_curr;
	uint32_t o_offset = offset;
	uint32_t major;
	uint32_t minor;

	major = (p->interface_version & RENCODE_IF_MAJOR_VERSION_MASK) >> RENCODE_IF_MAJOR_VERSION_SHIFT;
	minor = (p->interface_version & RENCODE_IF_MINOR_VERSION_MASK) >> RENCODE_IF_MINOR_VERSION_SHIFT;

	add_field_1(asic, tvmid, addr, &offset, "interface_version", p->interface_version, 16, pOut, pBuf);
	add_field_bit(asic, offset - 4,  "major", major, "31:16", 10, pOut, pBuf);
	add_field_bit(asic, offset - 4,  "minor", minor, "15:0", 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "sw_context_address_hi", p->sw_context_address_hi, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "sw_context_address_lo", p->sw_context_address_lo, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "engine_type", p->engine_type, 10, pOut, pBuf);

	COMPARE_ERROR((offset - o_offset), ib_size - 8);
	STRUCT_WARNING(rvcn_enc_session_info_t);
}

static void print_v5_0_ib_session_info(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset,
				       void *p_curr, uint32_t ib_size, FILE *pOut, char ***pBuf)
{
	rvcn_v5_0_enc_session_info_t *p = (rvcn_v5_0_enc_session_info_t *)p_curr;
	uint32_t o_offset = offset;
	uint32_t major;
	uint32_t minor;

	major = (p->interface_version & RENCODE_IF_MAJOR_VERSION_MASK) >> RENCODE_IF_MAJOR_VERSION_SHIFT;
	minor = (p->interface_version & RENCODE_IF_MINOR_VERSION_MASK) >> RENCODE_IF_MINOR_VERSION_SHIFT;

	add_field_1(asic, tvmid, addr, &offset, "interface_version", p->interface_version, 16, pOut, pBuf);
	add_field_bit(asic, offset - 4, "major", major, "31:16", 10, pOut, pBuf);
	add_field_bit(asic, offset - 4, "minor", minor, "15:0", 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "sw_context_address_hi", p->sw_context_address_hi, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "sw_context_address_lo", p->sw_context_address_lo, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "engine_type", p->engine_type, 10, pOut, pBuf);

	COMPARE_ERROR((offset - o_offset), ib_size - 8);
	STRUCT_WARNING(rvcn_v5_0_enc_session_info_t);
}

static void print_ib_task_info(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset,
			       void *p_curr, uint32_t ib_size, FILE *pOut, char ***pBuf)
{
	rvcn_enc_task_info_t *p = p_curr;
	uint32_t o_offset = offset;

	add_field_1(asic, tvmid, addr, &offset, "total_size_of_all_packages", p->total_size_of_all_packages, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "task_id", p->task_id, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "allowed_max_num_feedbacks", p->allowed_max_num_feedbacks, 10, pOut, pBuf);

	COMPARE_ERROR((offset - o_offset), ib_size - 8);
	STRUCT_WARNING(rvcn_enc_task_info_t);
}

//vcn 1-2
static void print_v1_ib_session_init(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset,
				     void *p_curr, uint32_t ib_size, FILE *pOut, char ***pBuf)
{
	rvcn_v1_enc_session_init_t *p = (rvcn_v1_enc_session_init_t *)p_curr;
	uint32_t o_offset = offset;

	add_field_1(asic, tvmid, addr, &offset, "encode_standard", p->encode_standard, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "aligned_picture_width", p->aligned_picture_width, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "aligned_picture_height", p->aligned_picture_height, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "padding_width", p->padding_width, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "padding_height", p->padding_height, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "pre_encode_mode", p->pre_encode_mode, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "pre_encode_chroma_enabled", p->pre_encode_chroma_enabled, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "display_remote", p->display_remote, 10, pOut, pBuf);

	COMPARE_ERROR((offset - o_offset), ib_size - 8);
	STRUCT_WARNING(rvcn_v1_enc_session_init_t);
}

//vcn_3/4/5
static void print_ib_session_init(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset,
				  void *p_curr, uint32_t ib_size, FILE *pOut, char ***pBuf, uint32_t vcn_ip_version)
{
	rvcn_enc_session_init_t *p = (rvcn_enc_session_init_t *)p_curr;
	uint32_t *int_field = (uint32_t *)p_curr;
	uint32_t o_offset = offset;

	add_field_1(asic, tvmid, addr, &offset, "encode_standard", p->encode_standard, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "aligned_picture_width", p->aligned_picture_width, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "aligned_picture_height", p->aligned_picture_height, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "padding_width", p->padding_width, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "padding_height", p->padding_height, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "pre_encode_mode", p->pre_encode_mode, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "pre_encode_chroma_enabled", p->pre_encode_chroma_enabled, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "slice_output_enabled", p->slice_output_enabled, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "display_remote", p->display_remote, 10, pOut, pBuf);

	if ((vcn_ip_version & VCN_IP_VERSION(4, 0, 0)) == VCN_IP_VERSION(4, 0, 0)) {
		add_field_1(asic, tvmid, addr, &offset, "padding", *(int_field + (offset - o_offset) / 4), 0, pOut, pBuf);
	} else {
		STRUCT_WARNING(rvcn_enc_session_init_t);
	}
	COMPARE_ERROR((offset - o_offset), ib_size - 8);
}

static void print_ib_layer_control(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset,
				   void *p_curr, uint32_t ib_size, FILE *pOut, char ***pBuf)
{
	rvcn_enc_layer_control_t *p = p_curr;
	uint32_t o_offset = offset;

	add_field_1(asic, tvmid, addr, &offset, "max_num_temporal_layers", p->max_num_temporal_layers, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "num_temporal_layers", p->num_temporal_layers, 10, pOut, pBuf);

	COMPARE_ERROR((offset - o_offset), ib_size - 8);
	STRUCT_WARNING(rvcn_enc_layer_control_t);
}

static void print_ib_layer_select(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset,
				  void *p_curr, uint32_t ib_size, FILE *pOut, char ***pBuf)
{
	rvcn_enc_layer_select_t *p = p_curr;
	uint32_t o_offset = offset;

	add_field_1(asic, tvmid, addr, &offset, "temporal_layer_index", p->temporal_layer_index, 10, pOut, pBuf);

	COMPARE_ERROR((offset - o_offset), ib_size - 8);
	STRUCT_WARNING(rvcn_enc_layer_select_t);
}

static void print_ib_rate_control_session_init(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset,
					       void *p_curr, uint32_t ib_size, FILE *pOut, char ***pBuf)
{
	rvcn_enc_rate_ctl_session_init_t *p = p_curr;
	uint32_t o_offset = offset;
	char *method[] = {"cqp",
			   "latency constrained vbr",
			   "peak constrained vbr",
			   "cbr",
			   "quality vbr" };

	add_field_2(asic, tvmid, addr, &offset, "rate_control_method", p->rate_control_method > 4? "unknown" : method[p->rate_control_method], "", p->rate_control_method, 0, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "vbv_buffer_level", p->vbv_buffer_level, 10, pOut, pBuf);

	COMPARE_ERROR((offset - o_offset), ib_size - 8);
	STRUCT_WARNING(rvcn_enc_rate_ctl_session_init_t);
}

static void print_ib_rate_control_layer_init(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset,
					     void *p_curr, uint32_t ib_size, FILE *pOut, char ***pBuf)
{
	rvcn_enc_rate_ctl_layer_init_t *p = p_curr;
	uint32_t o_offset = offset;

	add_field_1(asic, tvmid, addr, &offset, "target_bit_rate", p->target_bit_rate, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "peak_bit_rate", p->peak_bit_rate, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "frame_rate_num", p->frame_rate_num, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "frame_rate_den", p->frame_rate_den, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "vbv_buffer_size", p->vbv_buffer_size, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "avg_target_bits_per_picture", p->avg_target_bits_per_picture, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "peak_bits_per_picture_integer", p->peak_bits_per_picture_integer, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "peak_bits_per_picture_fractional", p->peak_bits_per_picture_fractional, 10, pOut, pBuf);

	COMPARE_ERROR((offset - o_offset), ib_size - 8);
	STRUCT_WARNING(rvcn_enc_rate_ctl_layer_init_t);
}

static void print_ib_rate_control_per_picture(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset,
					      void *p_curr, uint32_t ib_size, FILE *pOut, char ***pBuf)
{
	rvcn_enc_rate_ctl_per_picture_t *p = (rvcn_enc_rate_ctl_per_picture_t *)p_curr;
	uint32_t o_offset = offset;

	add_field_1(asic, tvmid, addr, &offset, "qp_obs", p->qp_obs, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "min_qp_app_obs", p->min_qp_app_obs, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "max_qp_app_obs", p->max_qp_app_obs, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "max_au_size_obs", p->max_au_size_obs, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "enabled_filler_data", p->enabled_filler_data, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "skip_frame_enable", p->skip_frame_enable, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "enforce_hrd", p->enforce_hrd, 10, pOut, pBuf);

	COMPARE_ERROR((offset - o_offset), ib_size - 8);
	STRUCT_WARNING(rvcn_enc_rate_ctl_per_picture_t);
}

//vcn 3/4 (vcn5 should not use this)
static void print_v5_0_ib_rate_control_per_picture_ex(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset,
						      void *p_curr, uint32_t ib_size, FILE *pOut, char ***pBuf)
{
	rvcn_v5_0_enc_rate_ctl_per_picture_tx *p = (rvcn_v5_0_enc_rate_ctl_per_picture_tx *)p_curr;
	uint32_t o_offset = offset;

	add_field_1(asic, tvmid, addr, &offset, "qp_i", p->qp_i, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "qp_p", p->qp_p, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "qp_b", p->qp_b, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "min_qp_i", p->min_qp_i, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "max_qp_i", p->max_qp_i, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "min_qp_p", p->min_qp_p, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "max_qp_p", p->max_qp_p, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "min_qp_b", p->min_qp_b, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "max_qp_b", p->max_qp_b, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "max_au_size_i", p->max_au_size_i, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "max_au_size_p", p->max_au_size_p, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "max_au_size_b", p->max_au_size_b, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "enabled_filler_data", p->enabled_filler_data, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "skip_frame_enable", p->skip_frame_enable, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "enforce_hrd", p->enforce_hrd, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "qvbr_quality_level", p->qvbr_quality_level, 10, pOut, pBuf);

	COMPARE_ERROR((offset - o_offset), ib_size - 8);
	STRUCT_WARNING(rvcn_v5_0_enc_rate_ctl_per_picture_tx);
}

//vcn 1/2
static void print_ib_rate_control_per_picture_ex(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset,
						 void *p_curr, uint32_t ib_size, FILE *pOut, char ***pBuf)
{
	rvcn_enc_rate_ctl_per_picture_tx *p = (rvcn_enc_rate_ctl_per_picture_tx *)p_curr;
	uint32_t o_offset = offset;

	add_field_1(asic, tvmid, addr, &offset, "qp_i", p->qp_i, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "qp_p", p->qp_p, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "qp_b", p->qp_b, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "min_qp_i", p->min_qp_i, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "max_qp_i", p->max_qp_i, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "min_qp_p", p->min_qp_p, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "max_qp_p", p->max_qp_p, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "min_qp_b", p->min_qp_b, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "max_qp_b", p->max_qp_b, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "max_au_size_i", p->max_au_size_i, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "max_au_size_p", p->max_au_size_p, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "max_au_size_b", p->max_au_size_b, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "enabled_filler_data", p->enabled_filler_data, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "skip_frame_enable", p->skip_frame_enable, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "enforce_hrd", p->enforce_hrd, 10, pOut, pBuf);

	COMPARE_ERROR((offset - o_offset), ib_size - 8);
	STRUCT_WARNING(rvcn_enc_rate_ctl_per_picture_tx);
}

static void print_v1_ib_quality_params(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset,
				       void *p_curr, uint32_t ib_size, FILE *pOut, char ***pBuf)
{
	rvcn_v1_enc_quality_params_t *p = (rvcn_v1_enc_quality_params_t *)p_curr;
	uint32_t o_offset = offset;
	char *vbaq_mode[] = {"disable", "auto mode", "constant quality map", "variance of variance"};

	add_field_2(asic, tvmid, addr, &offset, "vbaq_mode", p->vbaq_mode > 3? "unknown" : vbaq_mode[p->vbaq_mode], "", p->vbaq_mode, 0, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "scene_change_sensitivity", p->scene_change_sensitivity, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "scene_change_min_idr_interval", p->scene_change_min_idr_interval, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "two_pass_search_center_map_mode", p->two_pass_search_center_map_mode, 10, pOut, pBuf);

	COMPARE_ERROR((offset - o_offset), ib_size - 8);
	STRUCT_WARNING(rvcn_v1_enc_quality_params_t);
}

static void print_ib_quality_params(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset,
				    void *p_curr, uint32_t ib_size, FILE *pOut, char ***pBuf)
{
	rvcn_enc_quality_params_t *p = (rvcn_enc_quality_params_t *)p_curr;
	uint32_t o_offset = offset;
	char *vbaq_mode[] = {"disable", "auto mode", "constant quality map", "variance of variance"};

	add_field_2(asic, tvmid, addr, &offset, "vbaq_mode", p->vbaq_mode > 3? "unknown" : vbaq_mode[p->vbaq_mode], "", p->vbaq_mode, 0, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "scene_change_sensitivity", p->scene_change_sensitivity, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "scene_change_min_idr_interval", p->scene_change_min_idr_interval, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "two_pass_search_center_map_mode", p->two_pass_search_center_map_mode, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "vbaq_strength", p->vbaq_strength, 10, pOut, pBuf);

	COMPARE_ERROR((offset - o_offset), ib_size - 8);
	STRUCT_WARNING(rvcn_enc_quality_params_t);
}

static void print_ib_direct_output_nalu(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset,
					void *p_curr, uint32_t ib_size, FILE *pOut, char ***pBuf)
{
	rvcn_enc_direct_output_nalu_t *p = p_curr;
	uint32_t o_offset = offset, i;
	char *nalu_type[] = { "aud",
	                      "vps",
	                      "sps",
	                      "pps",
	                      "prefix",
	                      "end_of_sequence",
	                      "sei_display_color_volume",
	                      "sei_content_light_level_info"};

	add_field_2(asic, tvmid, addr, &offset, "type", p->type > 7? "unknown" : nalu_type[p->type], "", p->type, 0, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "size", p->size, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "data 1", p->data[0], 10, pOut, pBuf);
	for (i = 1; i < p->size; i++)
		add_field_3(asic, tvmid, addr, &offset, "data", i+1, p->data[i], 10, pOut, pBuf);

	COMPARE_ERROR((offset - o_offset), ib_size - 8);
}

static void print_ib_slice_header(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset,
				  void *p_curr, uint32_t ib_size, FILE *pOut, char ***pBuf)
{
	rvcn_enc_slice_header_t *p = p_curr;
	uint32_t o_offset = offset;
	char *msg = "unknown";
	uint32_t i, j;

	struct slice_instruction {
		uint32_t instruction;
		char *msg;
	};

	struct slice_instruction instructions[] = {
			{0x00000000, "common end"},
			{0x00000001, "common copy"},
			{0x00010000, "hevc dependent slice end"},
			{0x00010001, "hevc first slice"},
			{0x00010002, "hevc slice segment"},
			{0x00010003, "hevc qp delta"},
			{0x00010004, "hevc sao enable"},
			{0x00010005, "hevc loop filter across slices enable"},
			{0x00020000, "h264 frist mb"},
			{0x00020001, "h264 slice qp delta"},
		};

	for (i = 0; i < 16; i++)
		add_field_3(asic, tvmid, addr, &offset, "bitstream_template", i+1, p->bitstream_template[i], 16, pOut, pBuf);
	for (i = 0; i < 16; i++) {
		for (j = 0; j < ARRAY_SIZE(instructions); j++)
			if (p->instructions[i].instruction == instructions[j].instruction) {
				msg = instructions[j].msg;
				break;
			}
		add_field_4(asic, tvmid, addr, &offset, "instruction", i+1, msg, p->instructions[i].instruction, 0, pOut, pBuf);
		add_field_3(asic, tvmid, addr, &offset, "num_bits", i+1, p->instructions[i].num_bits, 10, pOut, pBuf);
	}
	COMPARE_ERROR((offset - o_offset), ib_size - 8);
	STRUCT_WARNING(rvcn_enc_slice_header_t);
}

static void print_ib_bitstream_instruction(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset,
					   void *p_curr, uint32_t ib_size, FILE *pOut, char ***pBuf)
{
	uint32_t o_offset = offset;
	uint32_t *p = p_curr;
	uint32_t i, size, type;
	uint32_t obu_type;

	struct bitstream_instruction {
		uint32_t instruction;
		char *msg;
	};

	struct bitstream_instruction instructions[] = {
			{0x00000000, "end"},
			{0x00000001, "copy"},
			{0x00000002, "obu_start"},
			{0x00000003, "obu_size"},
			{0x00000004, "obu_end"},
			{0x00000005, "allow_high_precision_mv"},
			{0x00000006, "delta_lf_params"},
			{0x00000007, "read_interpolation_filter"},
			{0x00000008, "loop_filter_params"},
			{0x00000009, "tile_info/CONTEXT UPDATE TILE ID(vcn5)"},
			{0x0000000a, "quantization_params/BASE Q IDX(vcn5)"},
			{0x0000000b, "delta_q_params"},
			{0x0000000c, "cdf_params"},
			{0x0000000d, "read_tx_mode"},
			{0x0000000e, "tile_group_obu"},
		};

	while (1)
	{
		size = *p++;
		type = *p++;
		add_field_1(asic, tvmid, addr, &offset, "instruction size", size, 10, pOut, pBuf);
		switch (type)
		{
			case RENCODE_AV1_BITSTREAM_INSTRUCTION_COPY:
				add_field_2(asic, tvmid, addr, &offset, "instruction type", instructions[type].msg, "", type, 0, pOut, pBuf);
				{
					uint32_t data;
					uint32_t bits = *p++;
					add_field_1(asic, tvmid, addr, &offset, "bits", bits, 10, pOut, pBuf);
					for (i = 0; i < bits; i += 32) {
						data = *p++;
						add_field_1(asic, tvmid, addr, &offset, "data", data, 16, pOut, pBuf);
					}
				}
				break;
			case RENCODE_AV1_BITSTREAM_INSTRUCTION_OBU_START:
				add_field_1(asic, tvmid, addr, &offset, instructions[type].msg, type, 16, pOut, pBuf);
				{
					obu_type = *p++;
					add_field_1(asic, tvmid, addr, &offset, "obu_type", obu_type, 16, pOut, pBuf);
				}
				break;
			case RENCODE_AV1_BITSTREAM_INSTRUCTION_OBU_END:
				add_field_1(asic, tvmid, addr, &offset, instructions[type].msg, type, 16, pOut, pBuf);
				break;
			case RENCODE_AV1_BITSTREAM_INSTRUCTION_ALLOW_HIGH_PRECISION_MV:
				add_field_1(asic, tvmid, addr, &offset, instructions[type].msg, type, 16, pOut, pBuf);
				break;
			case RENCODE_AV1_BITSTREAM_INSTRUCTION_DELTA_LF_PARAMS:
				add_field_1(asic, tvmid, addr, &offset, instructions[type].msg, type, 16, pOut, pBuf);
				break;
			case RENCODE_AV1_BITSTREAM_INSTRUCTION_READ_INTERPOLATION_FILTER:
				add_field_1(asic, tvmid, addr, &offset, instructions[type].msg, type, 16, pOut, pBuf);
				break;
			case RENCODE_AV1_BITSTREAM_INSTRUCTION_LOOP_FILTER_PARAMS:
				add_field_1(asic, tvmid, addr, &offset, instructions[type].msg, type, 16, pOut, pBuf);
				break;
			case RENCODE_V4_AV1_BITSTREAM_INSTRUCTION_TILE_INFO:// case RENCODE_V5_AV1_BITSTREAM_INSTRUCTION_CONTEXT_UPDATE_TILE_ID:
				add_field_1(asic, tvmid, addr, &offset, instructions[type].msg, type, 16, pOut, pBuf);
				break;
			case RENCODE_V4_AV1_BITSTREAM_INSTRUCTION_QUANTIZATION_PARAMS:// case RENCODE_V5_AV1_BITSTREAM_INSTRUCTION_BASE_Q_IDX:
				add_field_1(asic, tvmid, addr, &offset, instructions[type].msg, type, 16, pOut, pBuf);
				break;
			case RENCODE_AV1_BITSTREAM_INSTRUCTION_DELTA_Q_PARAMS:
				add_field_1(asic, tvmid, addr, &offset, instructions[type].msg, type, 16, pOut, pBuf);
				break;
			case RENCODE_AV1_BITSTREAM_INSTRUCTION_CDEF_PARAMS:
				add_field_1(asic, tvmid, addr, &offset, instructions[type].msg, type, 16, pOut, pBuf);
				break;
			case RENCODE_AV1_BITSTREAM_INSTRUCTION_READ_TX_MODE:
				add_field_1(asic, tvmid, addr, &offset, instructions[type].msg, type, 16, pOut, pBuf);
				break;
			case RENCODE_AV1_BITSTREAM_INSTRUCTION_TILE_GROUP_OBU:
				add_field_1(asic, tvmid, addr, &offset, instructions[type].msg, type, 16, pOut, pBuf);
				break;
			case RENCODE_AV1_BITSTREAM_INSTRUCTION_END:
				add_field_1(asic, tvmid, addr, &offset, instructions[type].msg, type, 16, pOut, pBuf);
				break;
			case RENCODE_AV1_BITSTREAM_INSTRUCTION_OBU_SIZE:
				add_field_1(asic, tvmid, addr, &offset, instructions[type].msg, type, 16, pOut, pBuf);
				break;
			default:
				add_field_1(asic, tvmid, addr, &offset, "instructions type: unknown", type, 16, pOut, pBuf);
				break;
		};
		if (type == RENCODE_AV1_BITSTREAM_INSTRUCTION_END)
			break;
	}
	COMPARE_ERROR((offset - o_offset), ib_size - 8);
}

static void print_ib_encode_input_format(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset,
					 void *p_curr, uint32_t ib_size, FILE *pOut, char ***pBuf)
{
	rvcn_enc_input_format_t *p = p_curr;
	const char *trans_func[] = { "G22", "G10", "G2084" };
	const char *gamut[] = { "BT709", "SCRGB", "BT2020", "BT601" };
	const char *color_space[] = { "YUV", "RGB" };
	const char *color_range[] = { "full", "studio" };
	const char *chroma_subsampling[] = { "420", "444" };
	const char *chroma_location[] = { "interstitial", "co site" };
	const char *bit_depth[] = { "8 bit", "10 bit", "fp 16" };
	const char *packing_format[] = { "NV12", "P010", "AYUV", "Y410", "A8R8G8B8", "A2R10G10B10", "A16B16G16R16F", "A8B8G8R8", "A2B10G10R10" };
	uint32_t t = RVCN_GET_TRANSFER_FUNCTION(p->input_color_volume);
	uint32_t g = RVCN_GET_GAMUT(p->input_color_volume);
	const char *mt = t < 3? trans_func[t] : "unknown trans_func";
	const char *mg = g < 4? gamut[g] : "unknown gamut";
	uint32_t o_offset = offset;

	add_field_2(asic, tvmid, addr, &offset, "input_color_volume", mt, mg, p->input_color_volume, 0, pOut, pBuf);
	add_field_2(asic, tvmid, addr, &offset, "input_color_space", p->input_color_space < 2? color_space[p->input_color_space] : "unknown", "", p->input_color_space, 0, pOut, pBuf);
	add_field_2(asic, tvmid, addr, &offset, "input_color_range", p->input_color_range < 2? color_range[p->input_color_range] : "unknown", "", p->input_color_range, 0, pOut, pBuf);
	add_field_2(asic, tvmid, addr, &offset, "input_chroma_subsampling", p->input_chroma_subsampling < 2? chroma_subsampling[p->input_chroma_subsampling] : "unknown", "", p->input_chroma_subsampling, 0, pOut, pBuf);
	add_field_2(asic, tvmid, addr, &offset, "input_chroma_location", p->input_chroma_location < 2? chroma_location[p->input_chroma_location] : "unknown", "", p->input_chroma_location, 0, pOut, pBuf);
	add_field_2(asic, tvmid, addr, &offset, "input_color_bit_depth", p->input_color_bit_depth < 3? bit_depth[p->input_color_bit_depth] : "unknown", "", p->input_color_bit_depth, 0, pOut, pBuf);
	add_field_2(asic, tvmid, addr, &offset, "input_color_packing_format", p->input_color_packing_format >= 0x31? "B10G10R10A2" : (p->input_color_packing_format < 9? packing_format[p->input_color_packing_format] : "unknown"), "", p->input_color_packing_format, 0, pOut, pBuf);

	COMPARE_ERROR((offset - o_offset), ib_size - 8);
	STRUCT_WARNING(rvcn_enc_input_format_t);
}

// vcn 2/3/4
static void print_ib_encode_output_format(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset,
					  void *p_curr, uint32_t ib_size, FILE *pOut, char ***pBuf)
{
	rvcn_enc_output_format_t *p = p_curr;
	const char *trans_func[] = { "G22", "G10", "G2084" };
	const char *gamut[] = { "BT709", "SCRGB", "BT2020", "BT601" };
	const char *color_range[] = { "full", "studio" };
	const char *bit_depth[] = { "8 bit", "10 bit", "fp 16" };
	const char *chroma_location[] = { "interstitial", "co site" };
	uint32_t t = RVCN_GET_TRANSFER_FUNCTION(p->output_color_volume);
	uint32_t g = RVCN_GET_GAMUT(p->output_color_volume);
	const char *mt = t < 3? trans_func[t] : "unknown trans_func";
	const char *mg = g < 4? gamut[g] : "unknown gamut";
	uint32_t o_offset = offset;

	add_field_2(asic, tvmid, addr, &offset, "output_color_volume", mt, mg, p->output_color_volume, 0, pOut, pBuf);
	add_field_2(asic, tvmid, addr, &offset, "output_color_range", p->output_color_range < 2? color_range[p->output_color_range] : "unknown", "", p->output_color_range, 0, pOut, pBuf);
	add_field_2(asic, tvmid, addr, &offset, "output_chroma_location", p->output_chroma_location < 2? chroma_location[p->output_chroma_location] : "unknown", "", p->output_chroma_location, 0, pOut, pBuf);
	add_field_2(asic, tvmid, addr, &offset, "output_color_bit_depth", p->output_color_bit_depth < 3? bit_depth[p->output_color_bit_depth] : "unknown", "", p->output_color_bit_depth, 0, pOut, pBuf);

	COMPARE_ERROR((offset - o_offset), ib_size - 8);
	STRUCT_WARNING(rvcn_enc_output_format_t);
}

// vcn 5
static void print_v5_0_ib_encode_output_format(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset,
					       void *p_curr, uint32_t ib_size, FILE *pOut, char ***pBuf)
{
	rvcn_v5_0_enc_output_format_t *p = (rvcn_v5_0_enc_output_format_t *)p_curr;
	const char *trans_func[] = { "G22", "G10", "G2084" };
	const char *gamut[] = { "BT709", "SCRGB", "BT2020", "BT601" };
	const char *color_range[] = { "full", "studio" };
	const char *bit_depth[] = { "8 bit", "10 bit", "fp 16" };
	const char *chroma_location[] = { "interstitial", "co site" };

	uint32_t t = RVCN_GET_TRANSFER_FUNCTION(p->output_color_volume);
	uint32_t g = RVCN_GET_GAMUT(p->output_color_volume);
	const char *mt = t < 3? trans_func[t] : "unknown trans_func";
	const char *mg = g < 4? gamut[g] : "unknown gamut";
	uint32_t o_offset = offset;

	add_field_2(asic, tvmid, addr, &offset, "output_color_volume", mt, mg, p->output_color_volume, 0, pOut, pBuf);
	add_field_2(asic, tvmid, addr, &offset, "output_color_range", p->output_color_range < 2? color_range[p->output_color_range] : "unknown", "", p->output_color_range, 0, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "output_chroma_subsampling", p->output_chroma_subsampling, 10, pOut, pBuf);
	add_field_2(asic, tvmid, addr, &offset, "output_chroma_location", p->output_chroma_location < 2? chroma_location[p->output_chroma_location] : "unknown", "", p->output_chroma_location, 0, pOut, pBuf);
	add_field_2(asic, tvmid, addr, &offset, "output_color_bit_depth", p->output_color_bit_depth < 3? bit_depth[p->output_color_bit_depth] : "unknown", "", p->output_color_bit_depth, 0, pOut, pBuf);

	COMPARE_ERROR((offset - o_offset), ib_size - 8);
	STRUCT_WARNING(rvcn_v5_0_enc_output_format_t);
}

#if SUPPORT
static void print_ib_efc_config(void *p_curr, uint32_t ib_size, FILE *pOut, char ***pBuf)
{
	rvcn_enc_efc_config_t *p = p_curr;

	fprintf(pOut, "\n==efc config  (ib_size = 0x%x)==\n", ib_size);
	STRUCT_WARNING(rvcn_enc_efc_config_t);

	fprintf(pOut, "coef_buffer_address_hi = 0x%08x\n",p->coef_buffer_address_hi);
	fprintf(pOut, "coef_buffer_address_lo = 0x%08x\n",p->coef_buffer_address_lo);
	fprintf(pOut, "coef_buffer_size = %d\n",p->coef_buffer_size);
	fprintf(pOut, "cm_program_register_data_size = %d\n",p->cm_program_register_data_size);
}
#endif

static void print_v1_ib_encode_params(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset,
				      void *p_curr, uint32_t ib_size, FILE *pOut, char ***pBuf)
{
	rvcn_v1_enc_encode_params_t *p = (rvcn_v1_enc_encode_params_t *)p_curr;
	const char *pic_type[] = { "B_PIC", "P_PIC", "I_PIC", "P_SKIP", "B_SKIP" };
	uint32_t o_offset = offset;

	add_field_2(asic, tvmid, addr, &offset, "pic_type", p->pic_type < 5? pic_type[p->pic_type] : "unknown", "", p->pic_type, 0, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "allowed_max_bitstream_size", p->allowed_max_bitstream_size, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "input_picture_luma_address_hi", p->input_picture_luma_address_hi, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "input_picture_luma_address_lo", p->input_picture_luma_address_lo, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "input_picture_chroma_address_hi", p->input_picture_chroma_address_hi, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "input_picture_chroma_address_lo", p->input_picture_chroma_address_lo, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "input_pic_luma_pitch", p->input_pic_luma_pitch, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "input_pic_chroma_pitch", p->input_pic_chroma_pitch, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "input_pic_swizzle_mode", p->input_pic_swizzle_mode, 10, pOut, pBuf); /* uint8_t */
	add_field_1(asic, tvmid, addr, &offset, "reference_picture_index", p->reference_picture_index, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "reconstructed_picture_index", p->reconstructed_picture_index, 10, pOut, pBuf);

	COMPARE_ERROR((offset - o_offset), ib_size - 8);
	STRUCT_WARNING(rvcn_v1_enc_encode_params_t);
}

//vcn2+
static void print_ib_encode_params(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset,
				   void *p_curr, uint32_t ib_size, FILE *pOut, char ***pBuf)
{
	rvcn_enc_encode_params_t *p = (rvcn_enc_encode_params_t *)p_curr;
	const char *pic_type[] = { "B_PIC", "P_PIC", "I_PIC", "P_SKIP", "B_SKIP" };
	uint32_t o_offset = offset;

	add_field_2(asic, tvmid, addr, &offset, "pic_type", p->pic_type < 5? pic_type[p->pic_type] : "unknown", "", p->pic_type, 0, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "allowed_max_bitstream_size", p->allowed_max_bitstream_size, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "input_picture_luma_address_hi", p->input_picture_luma_address_hi, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "input_picture_luma_address_lo", p->input_picture_luma_address_lo, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "input_picture_chroma_address_hi", p->input_picture_chroma_address_hi, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "input_picture_chroma_address_lo", p->input_picture_chroma_address_lo, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "input_pic_luma_pitch", p->input_pic_luma_pitch, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "input_pic_chroma_pitch", p->input_pic_chroma_pitch, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "input_pic_swizzle_mode", p->input_pic_swizzle_mode, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "reconstructed_picture_index", p->reconstructed_picture_index, 10, pOut, pBuf);

	COMPARE_ERROR((offset - o_offset), ib_size - 8);
	STRUCT_WARNING(rvcn_enc_encode_params_t);
}

static void print_ib_intra_refresh(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset,
				   void *p_curr, uint32_t ib_size, FILE *pOut, char ***pBuf)
{
	rvcn_enc_intra_refresh_t *p = p_curr;
	const char *intra_refresh_mode[] = { "NONE", "ROWS mode", "column mode" };
	uint32_t o_offset = offset;

	add_field_2(asic, tvmid, addr, &offset, "intra_refresh_mode", p->intra_refresh_mode < 3? intra_refresh_mode[p->intra_refresh_mode] : "unknown", "", p->intra_refresh_mode, 0, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "offset", p->offset, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "region_size", p->region_size, 10, pOut, pBuf);

	COMPARE_ERROR((offset - o_offset), ib_size - 8);
	STRUCT_WARNING(rvcn_enc_intra_refresh_t);
}

static void print_v1_ib_encode_context_buffer(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset,
					      void *p_curr, uint32_t ib_size, FILE *pOut, char ***pBuf)
{
	rvcn_v1_encode_context_buffer_t *p = (rvcn_v1_encode_context_buffer_t *)p_curr;
	uint32_t o_offset = offset;
	int i;

	add_field_1(asic, tvmid, addr, &offset, "encode_context_address_hi", p->encode_context_address_hi, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "encode_context_address_lo", p->encode_context_address_lo, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "swizzle_mode", p->swizzle_mode, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "rec_luma_pitch", p->rec_luma_pitch, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "rec_chroma_pitch", p->rec_chroma_pitch, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "num_reconstructed_pictures", p->num_reconstructed_pictures, 10, pOut, pBuf);
	for (i = 0; i < 34; ++i) {
		rvcn_v1_reconstructed_picture_t *r_p = &p->reconstructed_pictures[i];
		add_field_4(asic, tvmid, addr, &offset, "reconstructed_pictures", i+1, "luma_offset", r_p->luma_offset, 16, pOut, pBuf);
		add_field_1(asic, tvmid, addr, &offset, "chroma_offset", r_p->chroma_offset, 16, pOut, pBuf);
	}
	add_field_1(asic, tvmid, addr, &offset, "pre_encode_picture_luma_pitch", p->pre_encode_picture_luma_pitch, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "pre_encode_picture_chroma_pitch", p->pre_encode_picture_chroma_pitch, 10, pOut, pBuf);
	for (i = 0; i < 34; ++i) {
		rvcn_v1_reconstructed_picture_t *r_p = &p->pre_encode_reconstructed_pictures[i];
		add_field_4(asic, tvmid, addr, &offset, "pre_encode_reconstructed_pictures", i+1, "luma_offset", r_p->luma_offset, 16, pOut, pBuf);
		add_field_1(asic, tvmid, addr, &offset, "chroma_offset", r_p->chroma_offset, 16, pOut, pBuf);
	}
	add_field_2(asic, tvmid, addr, &offset, "pre_encode_input_picture|", "luma_offset", "", p->pre_encode_input_picture.yuv.luma_offset, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "chroma_offset", p->pre_encode_input_picture.yuv.chroma_offset, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "two_pass_search_center_map_offset", p->two_pass_search_center_map_offset, 10, pOut, pBuf);

	COMPARE_ERROR((offset - o_offset), ib_size - 8);
	STRUCT_WARNING(rvcn_v1_encode_context_buffer_t);
}

static void print_v4_0_ib_encode_context_buffer(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset,
						void *p_curr, uint32_t ib_size, FILE *pOut, char ***pBuf)
{
	rvcn_enc_encode_context_buffer_t *p = (rvcn_enc_encode_context_buffer_t *)p_curr;
	uint32_t o_offset = offset;
	int i;

	add_field_1(asic, tvmid, addr, &offset, "encode_context_address_hi", p->encode_context_address_hi, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "encode_context_address_lo", p->encode_context_address_lo, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "swizzle_mode", p->swizzle_mode, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "rec_luma_pitch", p->rec_luma_pitch, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "rec_chroma_pitch", p->rec_chroma_pitch, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "num_reconstructed_pictures", p->num_reconstructed_pictures, 10, pOut, pBuf);
	for (i = 0; i < 34; ++i) {
		rvcn_enc_reconstructed_picture_t *r_p = &p->reconstructed_pictures[i];
		add_field_4(asic, tvmid, addr, &offset, "reconstructed_pictures", i+1, "luma_offset", r_p->luma_offset, 16, pOut, pBuf);
		add_field_1(asic, tvmid, addr, &offset, "chroma_offset", r_p->chroma_offset, 16, pOut, pBuf);
		add_field_1(asic, tvmid, addr, &offset, "av1_cdf_frame_context_offset", r_p->av1.av1_cdf_frame_context_offset, 16, pOut, pBuf);
		add_field_1(asic, tvmid, addr, &offset, "av1_cdef_algorithm_context_offset", r_p->av1.av1_cdef_algorithm_context_offset, 16, pOut, pBuf);
	}
	add_field_1(asic, tvmid, addr, &offset, "pre_encode_picture_luma_pitch", p->pre_encode_picture_luma_pitch, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "pre_encode_picture_chroma_pitch", p->pre_encode_picture_chroma_pitch, 10, pOut, pBuf);
	for (i = 0; i < 34; ++i) {
		rvcn_enc_reconstructed_picture_t *r_p = &p->pre_encode_reconstructed_pictures[i];
		add_field_4(asic, tvmid, addr, &offset, "pre_encode_reconstructed_pictures", i+1, "luma_offset", r_p->luma_offset, 16, pOut, pBuf);
		add_field_1(asic, tvmid, addr, &offset, "chroma_offset", r_p->chroma_offset, 16, pOut, pBuf);
		add_field_1(asic, tvmid, addr, &offset, "av1_cdf_frame_context_offset", r_p->av1.av1_cdf_frame_context_offset, 16, pOut, pBuf);
		add_field_1(asic, tvmid, addr, &offset, "av1_cdef_algorithm_context_offset", r_p->av1.av1_cdef_algorithm_context_offset, 16, pOut, pBuf);
	}
	add_field_2(asic, tvmid, addr, &offset, "pre_encode_input_picture|", "red_offset", "", p->pre_encode_input_picture.rgb.red_offset, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "green_offset", p->pre_encode_input_picture.rgb.green_offset, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "blue_offset", p->pre_encode_input_picture.rgb.blue_offset, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "two_pass_search_center_map_offset", p->two_pass_search_center_map_offset, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "colloc_buffer_offset/av1_sdb_intermediate_context_offset", p->av1.av1_sdb_intermediate_context_offset, 16, pOut, pBuf);

	COMPARE_ERROR((offset - o_offset), ib_size - 8);
	STRUCT_WARNING(rvcn_enc_encode_context_buffer_t);
}

static void print_v3_0_ib_encode_context_buffer(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset,
						void *p_curr, uint32_t ib_size, FILE *pOut, char ***pBuf)
{
	rvcn_v3_0_enc_encode_context_buffer_t *p = (rvcn_v3_0_enc_encode_context_buffer_t *)p_curr;
	uint32_t o_offset = offset;
	int i;

	add_field_1(asic, tvmid, addr, &offset, "encode_context_address_hi", p->encode_context_address_hi, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "encode_context_address_lo", p->encode_context_address_lo, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "swizzle_mode", p->swizzle_mode, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "rec_luma_pitch", p->rec_luma_pitch, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "rec_chroma_pitch", p->rec_chroma_pitch, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "num_reconstructed_pictures", p->num_reconstructed_pictures, 10, pOut, pBuf);
	for (i = 0; i < 34; ++i) {
		rvcn_v1_reconstructed_picture_t *r_p = &p->reconstructed_pictures[i];
		add_field_4(asic, tvmid, addr, &offset, "reconstructed_pictures", i+1, "luma_offset", r_p->luma_offset, 16, pOut, pBuf);
		add_field_1(asic, tvmid, addr, &offset, "chroma_offset", r_p->chroma_offset, 16, pOut, pBuf);
	}
	add_field_1(asic, tvmid, addr, &offset, "colloc_buffer_offset", p->colloc_buffer_offset, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "pre_encode_picture_luma_pitch", p->pre_encode_picture_luma_pitch, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "pre_encode_picture_chroma_pitch", p->pre_encode_picture_chroma_pitch, 10, pOut, pBuf);
	for (i = 0; i < 34; ++i) {
		rvcn_v1_reconstructed_picture_t *r_p = &p->pre_encode_reconstructed_pictures[i];
		add_field_4(asic, tvmid, addr, &offset, "pre_encode_reconstructed_pictures", i+1, "luma_offset", r_p->luma_offset, 16, pOut, pBuf);
		add_field_1(asic, tvmid, addr, &offset, "chroma_offset", r_p->chroma_offset, 16, pOut, pBuf);
	}
	add_field_2(asic, tvmid, addr, &offset, "pre_encode_input_picture|", "red_offset", "", p->pre_encode_input_picture.rgb.red_offset, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "green_offset", p->pre_encode_input_picture.rgb.green_offset, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "blue_offset", p->pre_encode_input_picture.rgb.blue_offset, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "two_pass_search_center_map_offset", p->two_pass_search_center_map_offset, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "padding", p->padding[0], 0, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "padding", p->padding[1], 0, pOut, pBuf);

	COMPARE_ERROR((offset - o_offset), ib_size - 8);
	STRUCT_WARNING(rvcn_v3_0_enc_encode_context_buffer_t);
}

//vcn 2
static void print_ib_encode_context_buffer(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset,
					   void *p_curr, uint32_t ib_size, FILE *pOut, char ***pBuf)
{
	rvcn_v2_0_enc_encode_context_buffer_t *p = (rvcn_v2_0_enc_encode_context_buffer_t *)p_curr;
	uint32_t o_offset = offset;
	int i;

	add_field_1(asic, tvmid, addr, &offset, "encode_context_address_hi", p->encode_context_address_hi, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "encode_context_address_lo", p->encode_context_address_lo, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "swizzle_mode", p->swizzle_mode, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "rec_luma_pitch", p->rec_luma_pitch, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "rec_chroma_pitch", p->rec_chroma_pitch, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "num_reconstructed_pictures", p->num_reconstructed_pictures, 10, pOut, pBuf);
	for (i = 0; i < 34; ++i) {
		rvcn_v1_reconstructed_picture_t *r_p = &p->reconstructed_pictures[i];
		add_field_4(asic, tvmid, addr, &offset, "reconstructed_pictures", i+1, "luma_offset", r_p->luma_offset, 16, pOut, pBuf);
		add_field_1(asic, tvmid, addr, &offset, "chroma_offset", r_p->chroma_offset, 16, pOut, pBuf);
	}
	add_field_1(asic, tvmid, addr, &offset, "pre_encode_picture_luma_pitch", p->pre_encode_picture_luma_pitch, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "pre_encode_picture_chroma_pitch", p->pre_encode_picture_chroma_pitch, 10, pOut, pBuf);
	for (i = 0; i < 34; ++i) {
		rvcn_v1_reconstructed_picture_t *r_p = &p->pre_encode_reconstructed_pictures[i];
		add_field_4(asic, tvmid, addr, &offset, "pre_encode_reconstructed_pictures", i+1, "luma_offset", r_p->luma_offset, 16, pOut, pBuf);
		add_field_1(asic, tvmid, addr, &offset, "chroma_offset", r_p->chroma_offset, 16, pOut, pBuf);
	}
	add_field_2(asic, tvmid, addr, &offset, "pre_encode_input_picture|", "luma_offset", "", p->pre_encode_input_picture_yuv.yuv.luma_offset, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "chroma_offset", p->pre_encode_input_picture_yuv.yuv.chroma_offset, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "two_pass_search_center_map_offset", p->two_pass_search_center_map_offset, 10, pOut, pBuf);
	add_field_2(asic, tvmid, addr, &offset, "pre_encode_input_picture|", "red_offset", "", p->pre_encode_input_picture.rgb.red_offset, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "green_offset", p->pre_encode_input_picture.rgb.green_offset, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "blue_offset", p->pre_encode_input_picture.rgb.blue_offset, 16, pOut, pBuf);

	COMPARE_ERROR((offset - o_offset), ib_size - 8);
	STRUCT_WARNING(rvcn_v2_0_enc_encode_context_buffer_t);
}

// vcn 5.0
static void print_v5_0_ib_encode_context_buffer(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset,
						void *p_curr, uint32_t ib_size, FILE *pOut, char ***pBuf)
{
	rvcn_v5_0_enc_encode_context_buffer_t *p = (rvcn_v5_0_enc_encode_context_buffer_t *)p_curr;
	uint32_t o_offset = offset;
	int i;

	add_field_1(asic, tvmid, addr, &offset, "encode_context_address_hi", p->encode_context_address_hi, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "encode_context_address_lo", p->encode_context_address_lo, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "num_reconstructed_pictures", p->num_reconstructed_pictures, 10, pOut, pBuf);
	for (i = 0; i < 34; ++i) {
		rvcn_v5_0_enc_reconstructed_picture_t *r_p = &p->reconstructed_pictures[i];
		add_field_4(asic, tvmid, addr, &offset, "reconstructed_pictures", i+1, "frame_context_buffer_addr_hi0", r_p->frame_context_buffer_addr_hi0, 16, pOut, pBuf);
		add_field_1(asic, tvmid, addr, &offset, "frame_context_buffer_addr_lo0", r_p->frame_context_buffer_addr_lo0, 16, pOut, pBuf);
		add_field_1(asic, tvmid, addr, &offset, "rec_luma_pitch", r_p->rec_luma_pitch, 10, pOut, pBuf);
		add_field_1(asic, tvmid, addr, &offset, "frame_context_buffer_addr_hi1", r_p->frame_context_buffer_addr_hi1, 16, pOut, pBuf);
		add_field_1(asic, tvmid, addr, &offset, "frame_context_buffer_addr_lo1", r_p->frame_context_buffer_addr_lo1, 16, pOut, pBuf);
		add_field_1(asic, tvmid, addr, &offset, "rec_chroma_pitch", r_p->rec_chroma_pitch, 10, pOut, pBuf);
		add_field_1(asic, tvmid, addr, &offset, "frame_context_buffer_addr_hi2", r_p->frame_context_buffer_addr_hi2, 16, pOut, pBuf);
		add_field_1(asic, tvmid, addr, &offset, "frame_context_buffer_addr_lo2", r_p->frame_context_buffer_addr_lo2, 16, pOut, pBuf);
		add_field_1(asic, tvmid, addr, &offset, "reserved", r_p->FIXMEuknown0, 0, pOut, pBuf);
		add_field_1(asic, tvmid, addr, &offset, "swizzle_mode", r_p->swizzle_mode, 16, pOut, pBuf);
		add_field_1(asic, tvmid, addr, &offset, "frame_context_buffer_addr_hi", r_p->frame_context_buffer_addr_hi, 16, pOut, pBuf);
		add_field_1(asic, tvmid, addr, &offset, "frame_context_buffer_addr_lo", r_p->frame_context_buffer_addr_lo, 16, pOut, pBuf);
		add_field_1(asic, tvmid, addr, &offset, "colloc_buffer_offset(h264)/av1_cdf_frame_context_offset", r_p->h264.colloc_buffer_offset, 16, pOut, pBuf);
		add_field_1(asic, tvmid, addr, &offset, "av1_cdef_algorithm_context_offset", r_p->av1.av1_cdef_algorithm_context_offset, 16, pOut, pBuf);
		add_field_1(asic, tvmid, addr, &offset, "encode_metadata_offset", r_p->encode_metadata_offset, 16, pOut, pBuf);
	}
	for (i = 0; i < 34; ++i) {
		rvcn_v5_0_enc_reconstructed_picture_t *r_p = &p->pre_encode_reconstructed_pictures[i];
		add_field_4(asic, tvmid, addr, &offset, "pre_encode_reconstructed_pictures", i+1, "frame_context_buffer_addr_hi0", r_p->frame_context_buffer_addr_hi0, 16, pOut, pBuf);
		add_field_1(asic, tvmid, addr, &offset, "frame_context_buffer_addr_lo0", r_p->frame_context_buffer_addr_lo0, 16, pOut, pBuf);
		add_field_1(asic, tvmid, addr, &offset, "rec_luma_pitch", r_p->rec_luma_pitch, 10, pOut, pBuf);
		add_field_1(asic, tvmid, addr, &offset, "frame_context_buffer_addr_hi1", r_p->frame_context_buffer_addr_hi1, 16, pOut, pBuf);
		add_field_1(asic, tvmid, addr, &offset, "frame_context_buffer_addr_lo1", r_p->frame_context_buffer_addr_lo1, 16, pOut, pBuf);
		add_field_1(asic, tvmid, addr, &offset, "rec_chroma_pitch", r_p->rec_chroma_pitch, 10, pOut, pBuf);
		add_field_1(asic, tvmid, addr, &offset, "frame_context_buffer_addr_hi2", r_p->frame_context_buffer_addr_hi2, 16, pOut, pBuf);
		add_field_1(asic, tvmid, addr, &offset, "frame_context_buffer_addr_lo2", r_p->frame_context_buffer_addr_lo2, 16, pOut, pBuf);
		add_field_1(asic, tvmid, addr, &offset, "reserved", r_p->FIXMEuknown0, 0, pOut, pBuf);
		add_field_1(asic, tvmid, addr, &offset, "swizzle_mode", r_p->swizzle_mode, 16, pOut, pBuf);
		add_field_1(asic, tvmid, addr, &offset, "frame_context_buffer_addr_hi", r_p->frame_context_buffer_addr_hi, 16, pOut, pBuf);
		add_field_1(asic, tvmid, addr, &offset, "frame_context_buffer_addr_lo", r_p->frame_context_buffer_addr_lo, 16, pOut, pBuf);
		add_field_1(asic, tvmid, addr, &offset, "colloc_buffer_offset(h264)/av1_cdf_frame_context_offset", r_p->h264.colloc_buffer_offset, 16, pOut, pBuf);
		add_field_1(asic, tvmid, addr, &offset, "av1_cdef_algorithm_context_offset", r_p->av1.av1_cdef_algorithm_context_offset, 16, pOut, pBuf);
		add_field_1(asic, tvmid, addr, &offset, "encode_metadata_offset", r_p->encode_metadata_offset, 16, pOut, pBuf);
	}
	add_field_1(asic, tvmid, addr, &offset, "pre_encode_picture_luma_pitch", p->pre_encode_picture_luma_pitch, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "pre_encode_picture_chroma_pitch", p->pre_encode_picture_chroma_pitch, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "red_offset", p->rgb.red_offset, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "green_offset", p->rgb.green_offset, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "blue_offset", p->rgb.blue_offset, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "av1_sdb_intermediate_context_offset", p->av1_sdb_intermediate_context_offset, 16, pOut, pBuf);

	COMPARE_ERROR((offset - o_offset), ib_size - 8);
	STRUCT_WARNING(rvcn_v5_0_enc_encode_context_buffer_t);
}

static void print_ib_encode_context_buffer_override(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset,
						    void *p_curr, uint32_t ib_size, FILE *pOut, char ***pBuf)
{
	rvcn_enc_encode_context_buffer_override_t *p = (rvcn_enc_encode_context_buffer_override_t *)p_curr;
	uint32_t o_offset = offset;
	int i;

	for (i = 0; i < 34; ++i) {
		add_field_4(asic, tvmid, addr, &offset, "reconstructed_pictures", i+1, "luma_offset", p->reconstructed_pictures[i].luma_offset, 16, pOut, pBuf);
		add_field_1(asic, tvmid, addr, &offset, "chroma_offset", p->reconstructed_pictures[i].chroma_offset, 16, pOut, pBuf);
		add_field_1(asic, tvmid, addr, &offset, "chroma_v_offset", p->reconstructed_pictures[i].chroma_v_offset, 16, pOut, pBuf);
	}
	for (i = 0; i < 34; ++i) {
		add_field_4(asic, tvmid, addr, &offset, "pre_encode_reconstructed_pictures", i+1, "luma_offset", p->pre_encode_reconstructed_pictures[i].luma_offset, 16, pOut, pBuf);
		add_field_1(asic, tvmid, addr, &offset, "chroma_offset", p->pre_encode_reconstructed_pictures[i].chroma_offset, 16, pOut, pBuf);
		add_field_1(asic, tvmid, addr, &offset, "chroma_v_offset", p->pre_encode_reconstructed_pictures[i].chroma_v_offset, 16, pOut, pBuf);
	}
	COMPARE_ERROR((offset - o_offset), ib_size - 8);
	STRUCT_WARNING(rvcn_enc_encode_context_buffer_override_t);
}

static void print_ib_video_bitstream_buffer(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset,
					    void *p_curr, uint32_t ib_size, FILE *pOut, char ***pBuf)
{
	rvcn_enc_video_bitstream_buffer_t *p = p_curr;
	uint32_t o_offset = offset;

	add_field_1(asic, tvmid, addr, &offset, "mode(0: linear, 1: circular)", p->mode, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "video_bitstream_buffer_address_hi", p->video_bitstream_buffer_address_hi, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "video_bitstream_buffer_address_lo", p->video_bitstream_buffer_address_lo, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "video_bitstream_buffer_size", p->video_bitstream_buffer_size, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "video_bitstream_data_offset", p->video_bitstream_data_offset, 16, pOut, pBuf);

	COMPARE_ERROR((offset - o_offset), ib_size - 8);
	STRUCT_WARNING(rvcn_enc_video_bitstream_buffer_t);
}

#if SUPPORT
static void print_ib_search_center_map(void *p_curr, uint32_t ib_size, FILE *pOut, char ***pBuf)
{
	rvcn_search_center_map_t *p = p_curr;

	fprintf(pOut, "\n==search center map (ib_size = 0x%x)==\n", ib_size);
	STRUCT_WARNING(rvcn_search_center_map_t);

	fprintf(pOut, "search_center_map_enabled = %d\n",p->search_center_map_enabled);
	fprintf(pOut, "search_center_map_buffer_address_hi = %d\n",p->search_center_map_buffer_address_hi);
	fprintf(pOut, "search_center_map_buffer_address_lo = %d\n",p->search_center_map_buffer_address_lo);
}
#endif

static void print_ib_qp_map(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset,
			    void *p_curr, uint32_t ib_size, FILE *pOut, char ***pBuf)
{
	rvcn_enc_qp_map_t *p = p_curr;
	uint32_t o_offset = offset;
	const char *qp_map_type[] = { "none", "delta", "absolute", "roi", "pa", "float_pa" };

	add_field_2(asic, tvmid, addr, &offset, "qp_map_type", p->qp_map_type < 6 ? qp_map_type[p->qp_map_type] : "unknown", "", p->qp_map_type, 0, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "qp_map_buffer_address_hi", p->qp_map_buffer_address_hi, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "qp_map_buffer_address_lo", p->qp_map_buffer_address_lo, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "qp_map_pitch", p->qp_map_pitch, 10, pOut, pBuf);

	COMPARE_ERROR((offset - o_offset), ib_size - 8);
	STRUCT_WARNING(rvcn_enc_qp_map_t);
}

static void print_ib_feedback_buffer(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset,
				     void *p_curr, uint32_t ib_size, FILE *pOut, char ***pBuf)
{
	rvcn_enc_feedback_buffer_t *p = p_curr;
	uint32_t o_offset = offset;

	add_field_1(asic, tvmid, addr, &offset, "mode", p->mode, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "feedback_buffer_address_hi", p->feedback_buffer_address_hi, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "feedback_buffer_address_lo", p->feedback_buffer_address_lo, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "feedback_buffer_size", p->feedback_buffer_size, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "feedback_data_size", p->feedback_data_size, 10, pOut, pBuf);

	COMPARE_ERROR((offset - o_offset), ib_size - 8);
	STRUCT_WARNING(rvcn_enc_feedback_buffer_t);
}

#if SUPPORT
static void print_ib_feedback_buffer_additional(void *p_curr, uint32_t ib_size, FILE *pOut, char ***pBuf)
{
	rvcn_enc_feedback_buffer_additional_t *p = p_curr;
	int i;

	fprintf(pOut, "\n==feedback buffer additional (ib_size = 0x%x)==\n", ib_size);
	STRUCT_WARNING(rvcn_enc_feedback_buffer_additional_t);

	for (i = 0; i < 32; ++i) {
		fprintf(pOut, "feedback_type[%d] = 0x%08x\n",i, p->additional_feedback_table[i].feedback_type);
		fprintf(pOut, "feedback_data_size[%d] = %d\n", i, p->additional_feedback_table[i].feedback_data_size);
	}
}

static void print_ib_asw_buffer(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset,
				void *p_curr, uint32_t ib_size, FILE *pOut, char ***pBuf)
{
	rvcn_asw_buffer_t *p = p_curr;

	struct type_map
	{
		uint32_t type;
		const char *msg;
	} out_buffer_type[] = {
		{0,  "mv_legacy"},
		{1,  "mv"},
		{2,  "cost"},
		{4,  "bitcounts"},
		{8,  "rdintracost"},
		{16, "rdintercost"}
	};
	struct type_map grid_size[] = {
		{1, "grid 8x8"},
		{2, "grid 16x16"},
		{3, "grid 32x32"},
		{4, "grid 64x64"}
	};
	uint32_t i;

	fprintf(pOut, "\n==asw buffer (ib_size = 0x%x)==\n", ib_size);
	STRUCT_WARNING(rvcn_asw_buffer_t);

	for (i = 0; i < ARRAY_SIZE(grid_size); ++i) {
		if (p->grid_size == grid_size[i].type)
			break;
	}

	fprintf(pOut, "grid_size = %d (%s)\n",p->grid_size, grid_size[i].msg);

	for (i = 0; i < ARRAY_SIZE(out_buffer_type); ++i) {
		if (p->asw_output_buffer_type == out_buffer_type[i].type)
			break;
	}

	fprintf(pOut, "asw_output_buffer_type = 0x%x, (%s)\n",p->asw_output_buffer_type, out_buffer_type[i].msg);
	fprintf(pOut, "asw_output_buffer_address_hi = 0x%08x\n",p->asw_output_buffer_address_hi);
	fprintf(pOut, "asw_output_buffer_address_lo = 0x%08x\n",p->asw_output_buffer_address_lo);
	fprintf(pOut, "asw_ref_picture_luma_address_hi = 0x%08x\n",p->asw_ref_picture_luma_address_hi);
	fprintf(pOut, "asw_ref_picture_luma_address_lo = 0x%08x\n",p->asw_ref_picture_luma_address_lo);
	fprintf(pOut, "asw_ref_picture_chroma_address_hi = 0x%08x\n",p->asw_ref_picture_chroma_address_hi);
	fprintf(pOut, "asw_ref_picture_chroma_address_lo = 0x%08x\n",p->asw_ref_picture_chroma_address_lo);
	fprintf(pOut, "asw_ref_picture_luma_pitch = %d\n",p->asw_ref_picture_luma_pitch);
	fprintf(pOut, "asw_ref_picture_chroma_pitch = %d\n",p->asw_ref_picture_chroma_pitch);
	fprintf(pOut, "asw_ref_picture_swizzle_mode = %d\n",p->asw_ref_picture_swizzle_mode);
	fprintf(pOut, "asw_enable_tmz = %d\n",p->asw_enable_tmz);
}
#endif

static void print_ib_encode_latency(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset,
				    void *p_curr, uint32_t ib_size, FILE *pOut, char ***pBuf)
{
	rvcn_enc_latency_t *p = p_curr;
	uint32_t o_offset = offset;

	add_field_1(asic, tvmid, addr, &offset, "encode_latency", p->encode_latency, 10, pOut, pBuf);

	COMPARE_ERROR((offset - o_offset), ib_size - 8);
	STRUCT_WARNING(rvcn_enc_latency_t);
}

static void print_ib_encode_statistics(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset,
				       void *p_curr, uint32_t ib_size, FILE *pOut, char ***pBuf)
{
	rvcn_enc_stats_t *p = p_curr;
	uint32_t o_offset = offset;

	add_field_1(asic, tvmid, addr, &offset, "encode_stats_type", p->encode_stats_type, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "encode_stats_buffer_address_hi", p->encode_stats_buffer_address_hi, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "encode_stats_buffer_address_lo", p->encode_stats_buffer_address_lo, 16, pOut, pBuf);

	COMPARE_ERROR((offset - o_offset), ib_size - 8);
	STRUCT_WARNING(rvcn_enc_stats_t);
}

#if SUPPORT
static void print_ib_block_qp_dump(void *p_curr, uint32_t ib_size, FILE *pOut, char ***pBuf)
{
	rvcn_block_qp_dump_t *p = p_curr;

	fprintf(pOut, "\n==block qp dump (ib_size = 0x%x)==\n", ib_size);
	STRUCT_WARNING(rvcn_block_qp_dump_t);

	fprintf(pOut, "block_qp_dump_enabled = %d\n",p->block_qp_dump_enabled);
	fprintf(pOut, "block_qp_dump_buffer_address_hi = 0x%08x\n",p->block_qp_dump_buffer_address_hi);
	fprintf(pOut, "block_qp_dump_buffer_address_lo = 0x%08x\n",p->block_qp_dump_buffer_address_lo);
	fprintf(pOut, "block_qp_dump_buffer_pitch = %d\n",p->block_qp_dump_buffer_pitch);
}
#endif

static void print_ib_metadata_buffer(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset,
				     void *p_curr, uint32_t ib_size, FILE *pOut, char ***pBuf)
{
	rvcn_enc_metadata_buffer_t *p = p_curr;
	uint32_t o_offset = offset;

	add_field_1(asic, tvmid, addr, &offset, "metadata_buffer_address_hi", p->metadata_buffer_address_hi, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "metadata_buffer_address_lo", p->metadata_buffer_address_lo, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "two_pass_search_center_map_offset", p->two_pass_search_center_map_offset, 10, pOut, pBuf);

	COMPARE_ERROR((offset - o_offset), ib_size - 8);
	STRUCT_WARNING(rvcn_enc_metadata_buffer_t);
}

static void print_ib_hevc_slice_control(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset,
					void *p_curr, uint32_t ib_size, FILE *pOut, char ***pBuf)
{
	rvcn_enc_hevc_slice_control_t *p = p_curr;
	uint32_t o_offset = offset;

	if (p->slice_control_mode) {
		add_field_1(asic, tvmid, addr, &offset, "slice_control_mode: bits", p->slice_control_mode, 10, pOut, pBuf);
		add_field_1(asic, tvmid, addr, &offset, "num_bits_per_slice", p->fixed_bits_per_slice.num_bits_per_slice, 10, pOut, pBuf);
		add_field_1(asic, tvmid, addr, &offset, "num_bits_per_slice_segment", p->fixed_bits_per_slice.num_bits_per_slice_segment, 10, pOut, pBuf);
	} else {
		add_field_1(asic, tvmid, addr, &offset, "slice_control_mode: mb", p->slice_control_mode, 10, pOut, pBuf);
		add_field_1(asic, tvmid, addr, &offset, "num_ctbs_per_slice", p->fixed_ctbs_per_slice.num_ctbs_per_slice, 10, pOut, pBuf);
		add_field_1(asic, tvmid, addr, &offset, "num_ctbs_per_slice_segment", p->fixed_ctbs_per_slice.num_ctbs_per_slice_segment, 10, pOut, pBuf);
	}

	COMPARE_ERROR((offset - o_offset), ib_size - 8);
	STRUCT_WARNING(rvcn_enc_hevc_slice_control_t);
}

static void print_ib_hevc_spec_misc(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset,
				    void *p_curr, uint32_t ib_size, FILE *pOut, char ***pBuf)
{
	rvcn_enc_hevc_spec_misc_t *p = p_curr;
	uint32_t o_offset = offset;

	add_field_1(asic, tvmid, addr, &offset, "log2_min_luma_coding_block_size_minus3", p->log2_min_luma_coding_block_size_minus3, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "amp_disabled", p->amp_disabled, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "strong_intra_smoothing_enabled", p->strong_intra_smoothing_enabled, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "constrained_intra_pred_flag", p->constrained_intra_pred_flag, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "cabac_init_flag", p->cabac_init_flag, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "half_pel_enabled", p->half_pel_enabled, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "quarter_pel_enabled", p->quarter_pel_enabled, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "transform_skip_disabled", p->transform_skip_disabled, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "cu_qp_delta_enabled_flag", p->cu_qp_delta_enabled_flag, 10, pOut, pBuf);

	COMPARE_ERROR((offset - o_offset), ib_size - 8);
	STRUCT_WARNING(rvcn_enc_hevc_spec_misc_t);
}

static void print_v5_0_ib_hevc_spec_misc(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset,
					 void *p_curr, uint32_t ib_size, FILE *pOut, char ***pBuf)
{
	rvcn_v5_0_enc_hevc_spec_misc_t *p = (rvcn_v5_0_enc_hevc_spec_misc_t *)p_curr;
	uint32_t o_offset = offset;

	add_field_1(asic, tvmid, addr, &offset, "log2_min_luma_coding_block_size_minus3", p->log2_min_luma_coding_block_size_minus3, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "amp_disabled", p->amp_disabled, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "strong_intra_smoothing_enabled", p->strong_intra_smoothing_enabled, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "constrained_intra_pred_flag", p->constrained_intra_pred_flag, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "cabac_init_flag", p->cabac_init_flag, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "half_pel_enabled", p->half_pel_enabled, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "quarter_pel_enabled", p->quarter_pel_enabled, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "transform_skip_disabled", p->transform_skip_disabled, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "reserved0", p->reserved0, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "cu_qp_delta_enabled_flag", p->cu_qp_delta_enabled_flag, 10, pOut, pBuf);

	COMPARE_ERROR((offset - o_offset), ib_size - 8);
	STRUCT_WARNING(rvcn_v5_0_enc_hevc_spec_misc_t);
}

static void print_v1_ib_hevc_spec_misc(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset,
				       void *p_curr, uint32_t ib_size, FILE *pOut, char ***pBuf)
{
	rvcn_v1_enc_hevc_spec_misc_t *p = (rvcn_v1_enc_hevc_spec_misc_t *)p_curr;
	uint32_t o_offset = offset;

	add_field_1(asic, tvmid, addr, &offset, "log2_min_luma_coding_block_size_minus3", p->log2_min_luma_coding_block_size_minus3, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "amp_disabled", p->amp_disabled, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "strong_intra_smoothing_enabled", p->strong_intra_smoothing_enabled, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "constrained_intra_pred_flag", p->constrained_intra_pred_flag, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "cabac_init_flag", p->cabac_init_flag, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "half_pel_enabled", p->half_pel_enabled, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "quarter_pel_enabled", p->quarter_pel_enabled, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "cu_qp_delta_enabled_flag", p->cu_qp_delta_enabled_flag, 10, pOut, pBuf);

	COMPARE_ERROR((offset - o_offset), ib_size - 8);
	STRUCT_WARNING(rvcn_v1_enc_hevc_spec_misc_t);
}

static void print_ib_hevc_loop_filter(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset,
				      void *p_curr, uint32_t ib_size, FILE *pOut, char ***pBuf)
{
	rvcn_enc_hevc_deblocking_filter_t *p = p_curr;
	uint32_t o_offset = offset;

	add_field_1(asic, tvmid, addr, &offset, "loop_filter_across_slices_enabled", p->loop_filter_across_slices_enabled, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "deblocking_filter_disabled", p->deblocking_filter_disabled, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "beta_offset_div2", p->beta_offset_div2, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "tc_offset_div2", p->tc_offset_div2, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "cb_qp_offset", p->cb_qp_offset, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "cr_qp_offset", p->cr_qp_offset, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "disable_sao", p->disable_sao, 10, pOut, pBuf);

	COMPARE_ERROR((offset - o_offset), ib_size - 8);
	STRUCT_WARNING(rvcn_enc_hevc_deblocking_filter_t);
}

static void print_ib_h264_slice_control(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset,
					void *p_curr, uint32_t ib_size, FILE *pOut, char ***pBuf)
{
	rvcn_enc_h264_slice_control_t *p = p_curr;
	uint32_t o_offset = offset;

	if (p->slice_control_mode) {
		add_field_1(asic, tvmid, addr, &offset, "slice_control_mode:bits", p->slice_control_mode, 10, pOut, pBuf);
		add_field_1(asic, tvmid, addr, &offset, "num_bits_per_slice", p->num_bits_per_slice, 10, pOut, pBuf);
	} else {
		add_field_1(asic, tvmid, addr, &offset, "slice_control_mode:mb", p->slice_control_mode, 10, pOut, pBuf);
		add_field_1(asic, tvmid, addr, &offset, "num_mbs_per_slice", p->num_mbs_per_slice, 10, pOut, pBuf);
	}

	COMPARE_ERROR((offset - o_offset), ib_size - 8);
	STRUCT_WARNING(rvcn_enc_h264_slice_control_t);
}

static void print_v4_0_ib_av1_spec_misc(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset,
					void *p_curr, uint32_t ib_size, FILE *pOut, char ***pBuf)
{
	rvcn_v4_0_enc_av1_spec_misc_t *p = p_curr;
	uint32_t o_offset = offset;

	add_field_1(asic, tvmid, addr, &offset, "palette_mode_enable", p->palette_mode_enable, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "mv_precision", p->mv_precision, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "cdef_mode", p->cdef_mode, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "disable_cdf_update", p->disable_cdf_update, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "disable_frame_end_update_cdf", p->disable_frame_end_update_cdf, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "num_of_tiles", p->num_of_tiles, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "padding", p->padding[0], 0, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "padding", p->padding[1], 0, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "padding", p->padding[2], 0, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "padding", p->padding[3], 0, pOut, pBuf);

	COMPARE_ERROR((offset - o_offset), ib_size - 8);
	STRUCT_WARNING(rvcn_v4_0_enc_av1_spec_misc_t);
}

static void print_ib_av1_spec_misc(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset,
				   void *p_curr, uint32_t ib_size, FILE *pOut, char ***pBuf)
{
	rvcn_enc_av1_spec_misc_t *p = p_curr;
	uint32_t o_offset = offset;

	add_field_1(asic, tvmid, addr, &offset, "palette_mode_enable", p->palette_mode_enable, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "mv_precision", p->mv_precision, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "cdef_mode", p->cdef_mode, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "disable_cdf_update", p->disable_cdf_update, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "disable_frame_end_update_cdf", p->disable_frame_end_update_cdf, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "num_tiles_per_picture", p->num_tiles_per_picture, 10, pOut, pBuf);

	COMPARE_ERROR((offset - o_offset), ib_size - 8);
	STRUCT_WARNING(rvcn_enc_av1_spec_misc_t);
}

static void print_v5_0_ib_av1_spec_misc(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset,
					void *p_curr, uint32_t ib_size, FILE *pOut, char ***pBuf)
{
	rvcn_v5_0_enc_av1_spec_misc_t *p = p_curr;
	uint32_t o_offset = offset;
	int i;

	add_field_1(asic, tvmid, addr, &offset, "palette_mode_enable", p->palette_mode_enable, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "mv_precision", p->mv_precision, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "cdef_mode", p->cdef_mode, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "cdef_bits", p->cdef_bits, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "cdef_damping_minus3", p->cdef_damping_minus3, 10, pOut, pBuf);
	for (i = 0; i < 8; i++)
		add_field_3(asic, tvmid, addr, &offset, "cdef_y_pri_strength", i+1, p->cdef_y_pri_strength[i], 10, pOut, pBuf);
	for (i = 0; i < 8; i++)
		add_field_3(asic, tvmid, addr, &offset, "cdef_y_sec_strength", i+1, p->cdef_y_sec_strength[i], 10, pOut, pBuf);
	for (i = 0; i < 8; i++)
		add_field_3(asic, tvmid, addr, &offset, "cdef_uv_pri_strength", i+1, p->cdef_uv_pri_strength[i], 10, pOut, pBuf);
	for (i = 0; i < 8; i++)
		add_field_3(asic, tvmid, addr, &offset, "cdef_uv_sec_strength", i+1, p->cdef_uv_sec_strength[i], 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "reserved", p->reserved0, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "disable_cdf_update", p->disable_cdf_update, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "disable_frame_end_update_cdf", p->disable_frame_end_update_cdf, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "reserved", p->reserved1, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "delta_q_y_dc", p->delta_q_y_dc, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "delta_q_u_dc", p->delta_q_u_dc, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "delta_q_u_ac", p->delta_q_u_ac, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "delta_q_v_dc", p->delta_q_v_dc, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "delta_q_v_ac", p->delta_q_v_ac, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "reserved", p->reserved2, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "reserved", p->reserved3, 10, pOut, pBuf);

	COMPARE_ERROR((offset - o_offset), ib_size - 8);
	STRUCT_WARNING(rvcn_v5_0_enc_av1_spec_misc_t);
}

//vcn 1/2
static void print_v1_ib_h264_spec_misc(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset,
				       void *p_curr, uint32_t ib_size, FILE *pOut, char ***pBuf)
{
	rvcn_v1_enc_h264_spec_misc_t *p = (rvcn_v1_enc_h264_spec_misc_t *)p_curr;
	uint32_t o_offset = offset;

	add_field_1(asic, tvmid, addr, &offset, "constrained_intra_pred_flag", p->constrained_intra_pred_flag, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "cabac_enable", p->cabac_enable, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "cabac_init_idc", p->cabac_init_idc, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "half_pel_enabled", p->half_pel_enabled, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "quarter_pel_enabled", p->quarter_pel_enabled, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "profile_idc", p->profile_idc, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "level_idc", p->level_idc, 10, pOut, pBuf);

	COMPARE_ERROR((offset - o_offset), ib_size - 8);
	STRUCT_WARNING(rvcn_v1_enc_h264_spec_misc_t);
}

//vcn 3/4
static void print_v3_4_ib_h264_spec_misc(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset,
					 void *p_curr, uint32_t ib_size, FILE *pOut, char ***pBuf)
{
	rvcn_enc_h264_spec_misc_t *p = (rvcn_enc_h264_spec_misc_t *)p_curr;
	uint32_t o_offset = offset;

	add_field_1(asic, tvmid, addr, &offset, "constrained_intra_pred_flag", p->constrained_intra_pred_flag, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "cabac_enable", p->cabac_enable, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "cabac_init_idc", p->cabac_init_idc, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "half_pel_enabled", p->half_pel_enabled, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "quarter_pel_enabled", p->quarter_pel_enabled, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "profile_idc", p->profile_idc, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "level_idc", p->level_idc, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "b_picture_enabled", p->b_picture_enabled, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "weighted_bipred_idc", p->weighted_bipred_idc, 10, pOut, pBuf);

	COMPARE_ERROR((offset - o_offset), ib_size - 8);
	STRUCT_WARNING(rvcn_enc_h264_spec_misc_t);
}

//vcn 5
static void print_v5_0_ib_h264_spec_misc(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset,
					 void *p_curr, uint32_t ib_size, FILE *pOut, char ***pBuf)
{
	rvcn_v5_0_enc_h264_spec_misc_t *p = (rvcn_v5_0_enc_h264_spec_misc_t *)p_curr;
	uint32_t o_offset = offset;

	add_field_1(asic, tvmid, addr, &offset, "constrained_intra_pred_flag", p->constrained_intra_pred_flag, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "cabac_enable", p->cabac_enable, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "cabac_init_idc", p->cabac_init_idc, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "transform_8x8_mode", p->transform_8x8_mode, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "half_pel_enabled", p->half_pel_enabled, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "quarter_pel_enabled", p->quarter_pel_enabled, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "profile_idc", p->profile_idc, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "level_idc", p->level_idc, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "b_picture_enabled", p->b_picture_enabled, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "weighted_bipred_idc", p->weighted_bipred_idc, 10, pOut, pBuf);

	COMPARE_ERROR((offset - o_offset), ib_size - 8);
	STRUCT_WARNING(rvcn_v5_0_enc_h264_spec_misc_t);
}

static void print_v5_0_ib_hevc_encode_params(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset,
					     void *p_curr, uint32_t ib_size, FILE *pOut, char ***pBuf)
{
	rvcn_enc_hevc_encode_params_t *p = (rvcn_enc_hevc_encode_params_t *)p_curr;
	uint32_t o_offset = offset;
	int i;

	for (i = 0; i < 15; i++)
		add_field_3(asic, tvmid, addr, &offset, "ref_list0", i+1, p->ref_list0[i], 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "num_active_references_l0", p->num_active_references_l0, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "lsm_reference_pictures_list_index", p->lsm_reference_pictures_list_index, 16, pOut, pBuf);

	COMPARE_ERROR((offset - o_offset), ib_size - 8);
	STRUCT_WARNING(rvcn_enc_hevc_encode_params_t);
}

//vcn 5.0
static void print_v5_0_ib_h264_encode_params(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset,
					     void *p_curr, uint32_t ib_size, FILE *pOut, char ***pBuf)
{
	rvcn_v5_0_enc_h264_encode_params_t *p = (rvcn_v5_0_enc_h264_encode_params_t *)p_curr;
	const char *picture_structure[] = { "frame", "top field", "bottom field" };
	const char *interlaced_mode[] = { "progressive", "interlaced stacked", "interlaced interleaved" };
	uint32_t o_offset = offset;
	int i;

	add_field_2(asic, tvmid, addr, &offset, "input_picture_structure", p->input_picture_structure < 3 ? picture_structure[p->input_picture_structure]: "unknown", "", p->input_picture_structure, 0, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "input_pic_order_cnt", p->input_pic_order_cnt, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "is_reference", p->is_reference, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "is_long_term", p->is_long_term, 10, pOut, pBuf);
	add_field_2(asic, tvmid, addr, &offset, "interlaced_mode", p->interlaced_mode < 3? interlaced_mode[p->interlaced_mode] : "unknown", "", p->interlaced_mode, 0, pOut, pBuf);
	for (i = 0; i < 32; i++)
		add_field_3(asic, tvmid, addr, &offset, "ref_list0", i+1, p->ref_list0[i], 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "num_active_references_l0", p->num_active_references_l0, 10, pOut, pBuf);
	for (i = 0; i < 32; i++)
		add_field_3(asic, tvmid, addr, &offset, "ref_list1", i+1, p->ref_list1[i], 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "num_active_references_l1", p->num_active_references_l1, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "lsm_reference_pictures[1] list", p->lsm_reference_pictures[0].list, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "lsm_reference_pictures[1] list_index", p->lsm_reference_pictures[0].list_index, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "lsm_reference_pictures[2] list", p->lsm_reference_pictures[1].list, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "lsm_reference_pictures[2] list_index", p->lsm_reference_pictures[1].list_index, 16, pOut, pBuf);

	COMPARE_ERROR((offset - o_offset), ib_size - 8);
	STRUCT_WARNING(rvcn_v5_0_enc_h264_encode_params_t);
}

//vcn 3.0/4.0
static void print_ib_h264_encode_params(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset,
					void *p_curr, uint32_t ib_size, FILE *pOut, char ***pBuf)
{
	rvcn_enc_h264_encode_params_t *p = (rvcn_enc_h264_encode_params_t *)p_curr;
	const char *picture_structure[] = { "frame", "top field", "bottom field" };
	const char *interlaced_mode[] = { "progressive", "interlaced stacked", "interlaced interleaved" };
	uint32_t o_offset = offset;

	add_field_2(asic, tvmid, addr, &offset, "input_picture_structure", p->input_picture_structure < 3 ? picture_structure[p->input_picture_structure]: "unknown", "", p->input_picture_structure, 0, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "input_pic_order_cnt", p->input_pic_order_cnt, 10, pOut, pBuf);
	add_field_2(asic, tvmid, addr, &offset, "interlaced_mode", p->interlaced_mode < 3? interlaced_mode[p->interlaced_mode] : "unknown", "", p->interlaced_mode, 0, pOut, pBuf);

	add_field_1(asic, tvmid, addr, &offset, "l0 picture 0 pic_type", p->picture_info_l0_reference_picture0.pic_type, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "l0 picture 0 is_long_term", p->picture_info_l0_reference_picture0.is_long_term, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "l0 picture 0 picture_structure", p->picture_info_l0_reference_picture0.picture_structure, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "l0 picture 0 pic_order_cnt", p->picture_info_l0_reference_picture0.pic_order_cnt, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "l0_reference_picture1_index", p->l0_reference_picture1_index, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "l0 picture 1 pic_type", p->picture_info_l0_reference_picture1.pic_type, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "l0 picture 1 is_long_term", p->picture_info_l0_reference_picture1.is_long_term, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "l0 picture 1 picture_structure", p->picture_info_l0_reference_picture1.picture_structure, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "l0 picture 1 pic_order_cnt", p->picture_info_l0_reference_picture1.pic_order_cnt, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "l1_reference_picture0_index", p->l1_reference_picture0_index, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "l1 picture 0 pic_type", p->picture_info_l1_reference_picture0.pic_type, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "l1 picture 0 is_long_term", p->picture_info_l1_reference_picture0.is_long_term, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "l1 picture 0 picture_structure", p->picture_info_l1_reference_picture0.picture_structure, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "l1 picture 0 pic_order_cnt", p->picture_info_l1_reference_picture0.pic_order_cnt, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "is_reference", p->is_reference, 10, pOut, pBuf);

	COMPARE_ERROR((offset - o_offset), ib_size - 8);
	STRUCT_WARNING(rvcn_enc_h264_encode_params_t);
}

//vcn 1.0 to 2.0
static void print_v1_ib_h264_encode_params(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset,
					   void *p_curr, uint32_t ib_size, FILE *pOut, char ***pBuf)
{
	rvcn_v1_enc_h264_encode_params_t *p = (rvcn_v1_enc_h264_encode_params_t *)p_curr;
	const char *picture_structure[] = { "frame", "top field", "bottom field" };
	const char *interlaced_mode[] = { "progressive", "interlaced stacked", "interlaced interleaved" };
	uint32_t o_offset = offset;

	add_field_2(asic, tvmid, addr, &offset, "input_picture_structure", p->input_picture_structure < 3 ? picture_structure[p->input_picture_structure]: "unknown", "", p->input_picture_structure, 0, pOut, pBuf);
	add_field_2(asic, tvmid, addr, &offset, "interlaced_mode", p->interlaced_mode < 3? interlaced_mode[p->interlaced_mode] : "unknown", "", p->interlaced_mode, 0, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "reference_picture_structure", p->reference_picture_structure, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "reference_picture1_index", p->reference_picture1_index, 10, pOut, pBuf);

	COMPARE_ERROR((offset - o_offset), ib_size - 8);
	STRUCT_WARNING(rvcn_v1_enc_h264_encode_params_t);
}

static void print_ib_h264_deblocking_filter(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset,
					    void *p_curr, uint32_t ib_size, FILE *pOut, char ***pBuf)
{
	rvcn_enc_h264_deblocking_filter_t *p = p_curr;
	uint32_t o_offset = offset;

	add_field_1(asic, tvmid, addr, &offset, "disable_deblocking_filter_idc", p->disable_deblocking_filter_idc, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "alpha_c0_offset_div2", p->alpha_c0_offset_div2, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "beta_offset_div2", p->beta_offset_div2, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "cb_qp_offset", p->cb_qp_offset, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "cr_qp_offset", p->cr_qp_offset, 10, pOut, pBuf);

	COMPARE_ERROR((offset - o_offset), ib_size - 8);
	STRUCT_WARNING(rvcn_enc_h264_deblocking_filter_t);
}

//vcn4+
static void print_ib_av1_cdf_default_buffer(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset,
					    void *p_curr, uint32_t ib_size, FILE *pOut, char ***pBuf, uint32_t vcn_ip_version)
{
	rvcn_enc_av1_cdf_default_table_t *p= p_curr;
	uint32_t o_offset = offset;

	add_field_1(asic, tvmid, addr, &offset, "use_cdf_default", p->use_cdf_default, 10, pOut, pBuf);
	if (vcn_ip_version < VCN_IP_VERSION(5,0,0)) {
		add_field_1(asic, tvmid, addr, &offset, "cdf_default_buffer_address_lo", p->cdf_default_buffer_address_hi, 16, pOut, pBuf);
		add_field_1(asic, tvmid, addr, &offset, "cdf_default_buffer_address_hi", p->cdf_default_buffer_address_lo, 16, pOut, pBuf);
	} else {
		add_field_1(asic, tvmid, addr, &offset, "cdf_default_buffer_address_hi", p->cdf_default_buffer_address_hi, 16, pOut, pBuf);
		add_field_1(asic, tvmid, addr, &offset, "cdf_default_buffer_address_lo", p->cdf_default_buffer_address_lo, 16, pOut, pBuf);
	}

	COMPARE_ERROR((offset - o_offset), ib_size - 8);
	STRUCT_WARNING(rvcn_enc_av1_cdf_default_table_t);
}

static void print_ib_av1_tile_config(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset,
				     void *p_curr, uint32_t ib_size, FILE *pOut, char ***pBuf)
{
	rvcn_enc_av1_tile_config_t *p= p_curr;
	uint32_t o_offset = offset;
	uint32_t i;

	add_field_1(asic, tvmid, addr, &offset, "num_tile_cols", p->num_tile_cols, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "num_tile_rows", p->num_tile_rows, 10, pOut, pBuf);
	for (i = 0; i < 2; i++)
		add_field_3(asic, tvmid, addr, &offset, "tile_widths", i+1, p->tile_widths[i], 10, pOut, pBuf);
	for (i = 0; i < 16; i++)
		add_field_3(asic, tvmid, addr, &offset, "tile_heights", i+1, p->tile_heights[i], 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "num_tile_groups", p->num_tile_groups, 10, pOut, pBuf);
	for (i = 0; i < 32; i++) {
		add_field_3(asic, tvmid, addr, &offset, "start", i+1, p->tile_groups[i].start, 10, pOut, pBuf);
		add_field_3(asic, tvmid, addr, &offset, "end  ", i+1, p->tile_groups[i].end, 10, pOut, pBuf);
	}
	add_field_1(asic, tvmid, addr, &offset, "context_update_tile_id_mode", p->context_update_tile_id_mode, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "context_update_tile_id", p->context_update_tile_id, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "tile_size_bytes_minus_1", p->tile_size_bytes_minus_1, 10, pOut, pBuf);
	COMPARE_ERROR((offset - o_offset), ib_size - 8);
	STRUCT_WARNING(rvcn_enc_av1_tile_config_t);
}

// vcn5
static void print_ib_av1_encode_param(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset,
				      void *p_curr, uint32_t ib_size, FILE *pOut, char ***pBuf)
{
	rvcn_av1_encode_params_t *p= p_curr;
	uint32_t o_offset = offset;

	for (int i = 0; i < 7; i++)
		add_field_3(asic, tvmid, addr, &offset, "ref_frames", i+1, p->ref_frames[i], 16, pOut, pBuf);
	for (int i = 0; i < 2; i++)
		add_field_3(asic, tvmid, addr, &offset, "lsm_reference_frame_index", i+1, p->lsm_reference_frame_index[i], 16, pOut, pBuf);

	COMPARE_ERROR((offset - o_offset), ib_size - 8);
	STRUCT_WARNING(rvcn_av1_encode_params_t);
}

static void retrieve_engine_info(uint32_t *p_curr, uint32_t *p_end, uint32_t *engine_type)
{
	uint32_t ib_size, ib_type;

	while (p_curr < p_end) {
		ib_size = *p_curr++;
		ib_type = *p_curr++;

		if (!ib_size) { /* empty */
			break;
		}
		if (ib_type == RADEON_VCN_ENGINE_INFO) {
			vcn_cmn_engine_info_t *p = (vcn_cmn_engine_info_t *)p_curr;
			*engine_type = p->engine_type;
			return;
		}
		p_curr += ib_size /4 - 2;
	}
	*engine_type = 0xFFFFFFFF;
}

static void retrieve_session_info(uint32_t *p_curr, uint32_t *p_end, int32_t *major, int32_t *minor)
{
	uint32_t ib_size, ib_type;

	while (p_curr < p_end) {
		ib_size = *p_curr++;
		ib_type = *p_curr++;
		if (!ib_size) { /* empty */
			break;
		}
		if (ib_type == RENCODE_IB_PARAM_SESSION_INFO) {
			rvcn_enc_session_info_t *p = (rvcn_enc_session_info_t *)p_curr;
			*major = (p->interface_version & RENCODE_IF_MAJOR_VERSION_MASK) >> RENCODE_IF_MAJOR_VERSION_SHIFT;
			*minor = (p->interface_version & RENCODE_IF_MINOR_VERSION_MASK) >> RENCODE_IF_MINOR_VERSION_SHIFT;
			return;
		}
		p_curr += ib_size /4 - 2;
	}
}

/** umr_parse_vcn_enc - Handle common IB types for all VCN encode versions
 *
 * @asic: The ASIC model the packet decoding corresponds to
 * @vcn: VCN command message to be parsed
 * @pOut: The output file stream
 *
 */
void umr_parse_vcn_enc(struct umr_asic *asic, struct umr_vcn_cmd_message *vcn, FILE *pOut)
{
	uint32_t nwords = vcn->size / 4;
	uint32_t *p_curr = vcn->buf;
	uint32_t *p_end = p_curr + nwords;
	struct umr_ip_block *ip;
	uint32_t vcn_ip_version;
	uint32_t engine_type;
	int32_t major = -1;
	int32_t minor = -1;

	//FIXME: some hw has both uvd and vcn
	ip = umr_find_ip_block(asic, "vcn", 0); // for multi instance
	if (!ip)
		ip = umr_find_ip_block(asic, "vcn", -1); // for single instance
	if (!ip) {
		asic->err_msg("[ERROR]: Cannot get VCN IP info\n");
		return;
	}

	vcn_ip_version = VCN_IP_VERSION(ip->discoverable.maj, ip->discoverable.min, ip->discoverable.rev);

	if (vcn_ip_version >= VCN_IP_VERSION(4, 0, 0)) { //for unified ring
		retrieve_session_info(p_curr, p_end, &major, &minor);
		retrieve_engine_info(p_curr, p_end, &engine_type);
		if (engine_type == RADEON_VCN_ENGINE_TYPE_DECODE) {
			umr_vcn_dec_decode_unified_ring(asic, vcn, pOut, ip, NULL, 0, NULL);
			return;
		}

		fprintf(pOut, "\nDecoding VCN message at 0x%" PRIx32 "@0x%" PRIx64 " from 0x%" PRIx32 "@0x%" PRIx64 " of %" PRIu32 " words",
			vcn->vmid, vcn->addr, 0, vcn->from, nwords);

		if((major == -1 && minor == -1) || engine_type == 0xFFFFFFFF) {
			dump_ib(asic, vcn, p_curr, nwords, pOut);
			fprintf(pOut, "[ERROR] invalid message for VCN v%d_%d_%d\n",
				ip->discoverable.maj, ip->discoverable.min, ip->discoverable.rev);
			goto done;
		}

		if (engine_type > 3) { /* 4 is encode_queue */
			dump_ib(asic, vcn, p_curr, nwords, pOut);
			fprintf(pOut, "[ERROR] FIXME Unknown Engine[0x%08x] for VCN v%d_%d_%d\n",
				engine_type, ip->discoverable.maj, ip->discoverable.min, ip->discoverable.rev);
			goto done;
		}
	} else {
		fprintf(pOut, "\nDecoding VCN message at 0x%" PRIx32 "@0x%" PRIx64 " from 0x%" PRIx32 "@0x%" PRIx64 " of %" PRIu32 " words",
			vcn->vmid, vcn->addr, 0, vcn->from, nwords);
	}

	/* RADEON_VCN_ENGINE_TYPE_ENCODE */
	switch(vcn_ip_version) {
	case VCN_IP_VERSION(1, 0, 0):
	case VCN_IP_VERSION(1, 0, 1):
		vcn_enc_decode_ib_v1(asic, vcn, pOut, ip, NULL);
		break;
	default:
		vcn_enc_decode_ib(asic, vcn, pOut, ip, NULL);
		break;
	}
done:
	fprintf(pOut, "\nDone Decoding VCN message at 0x%" PRIx32 "@0x%" PRIx64 " from 0x%" PRIx32 "@0x%" PRIx64 " of %" PRIu32 " words\n",
		vcn->vmid, vcn->addr, 0, vcn->from, nwords);
}

/* cover both command line and GUI
 *  - command line case: vcn and pOut must be valid
 *  - GUI case: vcn is NULL and vgui must be valid
 */
static void vcn_enc_decode_ib(struct umr_asic *asic, struct umr_vcn_cmd_message *vcn,
			      FILE *pOut, struct umr_ip_block *ip, struct vcn_gui_message *vgui)
{
	uint32_t ib_size, ib_type, nwords = vcn ? vcn->size / 4 : vgui->size / 4;
	uint32_t *p_curr = vcn ? vcn->buf : vgui->in_buf;
	char     ***pBuf = vgui ? vgui->out_buf : NULL;
	uint32_t *p_end = p_curr + nwords;
	uint32_t vcn_addr = vcn ? vcn->addr : 0;
	uint32_t vmid = vcn? vcn->vmid: 0;
	uint32_t offset = 0;
	uint32_t vcn_ip_version = VCN_IP_VERSION(ip->discoverable.maj, ip->discoverable.min, ip->discoverable.rev);

	while (p_curr < p_end) {
		ib_size = *p_curr++;
		ib_type = *p_curr++;
		add_field_1(asic, vmid, vcn_addr, &offset, "IB_SIZE", ib_size, 10, pOut, pBuf);
		if (!ib_size) { /* empty */
			if (pOut)
				fprintf(pOut, "Empty messages!\n");
			break;
		}
		switch(ib_type)
		{
			case RADEON_VCN_SIGNATURE:
				ADD_IB_TYPE(RADEON_VCN_SIGNATURE);
				print_ib_signature(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf);
				break;
			case RADEON_VCN_ENGINE_INFO:
				ADD_IB_TYPE(RADEON_VCN_ENGINE_INFO);
				print_ib_engine_info(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf);
				break;
			case RENCODE_IB_PARAM_SESSION_INFO:
				ADD_IB_TYPE(RENCODE_IB_PARAM_SESSION_INFO);
				if ( vcn_ip_version >= VCN_IP_VERSION(3, 0, 0))
					print_v5_0_ib_session_info(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf);
				else
					print_ib_session_info(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf);
				break;
			case RENCODE_IB_PARAM_TASK_INFO:
				ADD_IB_TYPE(RENCODE_IB_PARAM_TASK_INFO);
				print_ib_task_info(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf);
				break;
			case RENCODE_IB_OP_INITIALIZE:
				ADD_IB_TYPE(RENCODE_IB_OP_INITIALIZE);
				break;
			case RENCODE_IB_PARAM_SESSION_INIT:
				ADD_IB_TYPE(RENCODE_IB_PARAM_SESSION_INIT);
				if ( vcn_ip_version < VCN_IP_VERSION(3, 0, 0))
					print_v1_ib_session_init(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf);
				else
					print_ib_session_init(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf, vcn_ip_version);
				break;
			case RENCODE_IB_PARAM_LAYER_CONTROL:
				ADD_IB_TYPE(RENCODE_IB_PARAM_LAYER_CONTROL);
				print_ib_layer_control(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf);
				break;
			case RENCODE_IB_PARAM_LAYER_SELECT:
				ADD_IB_TYPE(RENCODE_IB_PARAM_LAYER_SELECT);
				print_ib_layer_select(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf);
				break;
			case RENCODE_IB_PARAM_RATE_CONTROL_SESSION_INIT:
				ADD_IB_TYPE(RENCODE_IB_PARAM_RATE_CONTROL_SESSION_INIT);
				print_ib_rate_control_session_init(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf);
				break;
			case RENCODE_IB_PARAM_RATE_CONTROL_LAYER_INIT:
				ADD_IB_TYPE(RENCODE_IB_PARAM_RATE_CONTROL_LAYER_INIT);
				print_ib_rate_control_layer_init(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf);
				break;
			case RENCODE_IB_PARAM_RATE_CONTROL_PER_PICTURE:
				ADD_IB_TYPE(RENCODE_IB_PARAM_RATE_CONTROL_PER_PICTURE);
				if ( vcn_ip_version >= VCN_IP_VERSION(3, 0, 0))
					print_v5_0_ib_rate_control_per_picture_ex(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf);
				else
					print_ib_rate_control_per_picture_ex(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf);
				break;
			case RENCODE_IB_PARAM_QUALITY_PARAMS:
				ADD_IB_TYPE(RENCODE_IB_PARAM_QUALITY_PARAMS);
				print_ib_quality_params(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf);
				break;
			case RENCODE_IB_PARAM_DIRECT_OUTPUT_NALU:
				ADD_IB_TYPE(RENCODE_IB_PARAM_DIRECT_OUTPUT_NALU);
				print_ib_direct_output_nalu(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf);
				break;
			case RENCODE_IB_PARAM_SLICE_HEADER:
				ADD_IB_TYPE(RENCODE_IB_PARAM_SLICE_HEADER);
				print_ib_slice_header(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf);
				break;
			case RENCODE_AV1_IB_PARAM_BITSTREAM_INSTRUCTION: //vcn4
				ADD_IB_TYPE(RENCODE_AV1_IB_PARAM_BITSTREAM_INSTRUCTION);
				if ( vcn_ip_version >= VCN_IP_VERSION(5, 0, 0)) // RENCODE_V5_AV1_IB_PARAM_TILE_CONFIG
					print_ib_av1_tile_config(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf);
				else
					print_ib_bitstream_instruction(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf);
				break;
			case RENCODE_V5_AV1_IB_PARAM_BITSTREAM_INSTRUCTION: //vcn5
				ADD_IB_TYPE(RENCODE_V5_AV1_IB_PARAM_BITSTREAM_INSTRUCTION);
			        print_ib_bitstream_instruction(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf);
				break;
			case RENCODE_IB_PARAM_INPUT_FORMAT:
				ADD_IB_TYPE(RENCODE_IB_PARAM_INPUT_FORMAT);
				print_ib_encode_input_format(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf);
				break;
			case RENCODE_IB_PARAM_OUTPUT_FORMAT:
				ADD_IB_TYPE(RENCODE_IB_PARAM_OUTPUT_FORMAT);
				if ( vcn_ip_version >= VCN_IP_VERSION(5, 0, 0))
					print_v5_0_ib_encode_output_format(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf);
				else
					print_ib_encode_output_format(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf);
				break;
#if SUPPORT
			case RENCODE_IB_PARAM_EFC_CONFIG:// not in mesa
				ADD_IB_TYPE(RENCODE_IB_PARAM_EFC_CONFIG);
				print_ib_efc_config(p_curr, ib_size, pOut, pBuf);
				break;
#endif
			case RENCODE_IB_PARAM_ENCODE_PARAMS:
				ADD_IB_TYPE(RENCODE_IB_PARAM_ENCODE_PARAMS);
				if ( vcn_ip_version >= VCN_IP_VERSION(5, 0, 0))
					print_ib_encode_params(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf);
				else
					print_v1_ib_encode_params(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf);
				break;
			case RENCODE_IB_PARAM_INTRA_REFRESH:
				ADD_IB_TYPE(RENCODE_IB_PARAM_INTRA_REFRESH);
				print_ib_intra_refresh(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf);
				break;
			case RENCODE_IB_PARAM_ENCODE_CONTEXT_BUFFER:
				ADD_IB_TYPE(RENCODE_IB_PARAM_ENCODE_CONTEXT_BUFFER);
				if ( vcn_ip_version >= VCN_IP_VERSION(5, 0, 0))
					print_v5_0_ib_encode_context_buffer(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf);
				else if ( vcn_ip_version >= VCN_IP_VERSION(4, 0, 0))
					print_v4_0_ib_encode_context_buffer(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf);
				else if ( vcn_ip_version >= VCN_IP_VERSION(3, 0, 0))
					print_v3_0_ib_encode_context_buffer(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf);
				else //vcn 2
					print_ib_encode_context_buffer(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf);
				break;
			case RENCODE_IB_PARAM_VIDEO_BITSTREAM_BUFFER:
				ADD_IB_TYPE(RENCODE_IB_PARAM_VIDEO_BITSTREAM_BUFFER);
				print_ib_video_bitstream_buffer(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf);
				break;
#if SUPPORT
			case RENCODE_IB_PARAM_SEARCH_CENTER_MAP:
				ADD_IB_TYPE(RENCODE_IB_PARAM_SEARCH_CENTER_MAP);
				print_ib_search_center_map(p_curr, ib_size, pOut, pBuf);
				break;
#endif
			case RENCODE_IB_PARAM_QP_MAP:
				ADD_IB_TYPE(RENCODE_IB_PARAM_QP_MAP);
				print_ib_qp_map(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf);
				break;
			case RENCODE_IB_PARAM_FEEDBACK_BUFFER:
				ADD_IB_TYPE(RENCODE_IB_PARAM_FEEDBACK_BUFFER);
				print_ib_feedback_buffer(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf);
				break;
#if SUPPORT
			case RENCODE_IB_PARAM_FEEDBACK_BUFFER_ADDITIONAL:
				ADD_IB_TYPE(RENCODE_IB_PARAM_FEEDBACK_BUFFER_ADDITIONAL);
				print_ib_feedback_buffer_additional(p_curr, ib_size, pOut, pBuf);
				break;
			case RENCODE_IB_PARAM_ASW_BUFFER:
				ADD_IB_TYPE(RENCODE_IB_PARAM_ASW_BUFFER);
				print_ib_asw_buffer(p_curr, ib_size, pOut, pBuf);
				break;
#endif
			case RENCODE_IB_PARAM_ENCODE_LATENCY:
				ADD_IB_TYPE(RENCODE_IB_PARAM_ENCODE_LATENCY);
				print_ib_encode_latency(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf);
				break;
#if SUPPORT
			case UVE__IB_PARAM__BLOCK_QP_DUMP:
				ADD_IB_TYPE(UVE__IB_PARAM__BLOCK_QP_DUMP);
				print_ib_block_qp_dump(p_curr, ib_size, pOut, pBuf);
				break;
#endif
			case RENCODE_V5_IB_PARAM_METADATA_BUFFER:
				ADD_IB_TYPE(RENCODE_V5_IB_PARAM_METADATA_BUFFER);
				print_ib_metadata_buffer(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf);
				break;
			case RENCODE_HEVC_IB_PARAM_SLICE_CONTROL:
				ADD_IB_TYPE(RENCODE_HEVC_IB_PARAM_SLICE_CONTROL);
				print_ib_hevc_slice_control(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf);
				break;
			case RENCODE_HEVC_IB_PARAM_SPEC_MISC:
				ADD_IB_TYPE(RENCODE_HEVC_IB_PARAM_SPEC_MISC);
				if ( vcn_ip_version >= VCN_IP_VERSION(5, 0, 0))
					print_v5_0_ib_hevc_spec_misc(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf);
				else
					print_ib_hevc_spec_misc(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf);
				break;
			case RENCODE_V5_IB_PARAM_HEVC_ENCODE_PARAMS:
				ADD_IB_TYPE(RENCODE_V5_IB_PARAM_HEVC_ENCODE_PARAMS);
				print_v5_0_ib_hevc_encode_params(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf);
				break;
			case RENCODE_HEVC_IB_PARAM_LOOP_FILTER:
				ADD_IB_TYPE(RENCODE_HEVC_IB_PARAM_LOOP_FILTER);
				print_ib_hevc_loop_filter(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf);
				break;
			case RENCODE_H264_IB_PARAM_SLICE_CONTROL:
				ADD_IB_TYPE(RENCODE_H264_IB_PARAM_SLICE_CONTROL);
				print_ib_h264_slice_control(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf);
				break;
			case RENCODE_H264_IB_PARAM_SPEC_MISC:
				ADD_IB_TYPE(RENCODE_H264_IB_PARAM_SPEC_MISC);
				if ( vcn_ip_version >= VCN_IP_VERSION(5, 0, 0))
					print_v5_0_ib_h264_spec_misc(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf);
				else
					print_v3_4_ib_h264_spec_misc(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf);
				break;
			case RENCODE_H264_IB_PARAM_ENCODE_PARAMS:
				ADD_IB_TYPE(RENCODE_H264_IB_PARAM_ENCODE_PARAMS);
				if ( vcn_ip_version >= VCN_IP_VERSION(5, 0, 0))
					print_v5_0_ib_h264_encode_params(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf);
				else
					print_ib_h264_encode_params(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf);
				break;
			case RENCODE_H264_IB_PARAM_DEBLOCKING_FILTER:
				ADD_IB_TYPE(RENCODE_H264_IB_PARAM_DEBLOCKING_FILTER);
				print_ib_h264_deblocking_filter(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf);
				break;
			case RENCODE_AV1_IB_PARAM_SPEC_MISC:
				ADD_IB_TYPE(RENCODE_AV1_IB_PARAM_SPEC_MISC);
				if ( vcn_ip_version >= VCN_IP_VERSION(5, 0, 0))
					print_v5_0_ib_av1_spec_misc(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf);
				else if ( vcn_ip_version >= VCN_IP_VERSION(4, 0, 0))
					print_v4_0_ib_av1_spec_misc(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf);
				else // should be an error
					print_ib_av1_spec_misc(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf);
				break;
			case RENCODE_IB_OP_CLOSE_SESSION:
				ADD_IB_TYPE(RENCODE_IB_OP_CLOSE_SESSION);
				break;
			case RENCODE_IB_OP_ENCODE:
				ADD_IB_TYPE(RENCODE_IB_OP_ENCODE);
				break;
			case RENCODE_IB_OP_INIT_RC:
				ADD_IB_TYPE(RENCODE_IB_OP_INIT_RC);
				break;
			case RENCODE_IB_OP_INIT_RC_VBV_BUFFER_LEVEL:
				ADD_IB_TYPE(RENCODE_IB_OP_INIT_RC_VBV_BUFFER_LEVEL);
				break;
			case RENCODE_IB_OP_SET_SPEED_ENCODING_MODE:
				ADD_IB_TYPE(RENCODE_IB_OP_SET_SPEED_ENCODING_MODE);
				break;
			case RENCODE_IB_OP_SET_BALANCE_ENCODING_MODE:
				ADD_IB_TYPE(RENCODE_IB_OP_SET_BALANCE_ENCODING_MODE);
				break;
			case RENCODE_IB_OP_SET_QUALITY_ENCODING_MODE:
				ADD_IB_TYPE(RENCODE_IB_OP_SET_QUALITY_ENCODING_MODE);
				break;
			case RENCODE_IB_OP_SET_HIGH_QUALITY_ENCODING_MODE:
				ADD_IB_TYPE(RENCODE_IB_OP_SET_HIGH_QUALITY_ENCODING_MODE);
				break;
			case RENCODE_V5_IB_PARAM_ENCODE_CONTEXT_BUFFER_OVERRIDE:
				ADD_IB_TYPE(RENCODE_V5_IB_PARAM_ENCODE_CONTEXT_BUFFER_OVERRIDE);
				if ( vcn_ip_version >= VCN_IP_VERSION(5, 0, 0))
					print_ib_encode_context_buffer_override(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf);
				else if ( vcn_ip_version >= VCN_IP_VERSION(3, 0, 0))
					print_v5_0_ib_rate_control_per_picture_ex(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf);
				else
					print_ib_rate_control_per_picture_ex(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf);
				break;
			case RENC_UVD_IB_OP_INITIALIZE:
				ADD_IB_TYPE(RENC_UVD_IB_OP_INITIALIZE);
				break;
			case RENC_UVD_IB_OP_CLOSE_SESSION:
				ADD_IB_TYPE(RENC_UVD_IB_OP_CLOSE_SESSION);
				break;
			case RENCODE_V5_AV1_IB_PARAM_ENCODE_PARAMS:
				ADD_IB_TYPE(RENCODE_V5_AV1_IB_PARAM_ENCODE_PARAMS);
				print_ib_av1_encode_param(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf);
				break;
			default:
				if (ip->discoverable.maj >= 4) {
					if (ib_type == RENCODE_V4_IB_PARAM_CDF_DEFAULT_TABLE_BUFFER) {
						ADD_IB_TYPE(RENCODE_V4_IB_PARAM_CDF_DEFAULT_TABLE_BUFFER);
						print_ib_av1_cdf_default_buffer(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf, vcn_ip_version);
						break;
					}
					if (ib_type == RENCODE_V4_IB_PARAM_ENCODE_STATISTICS) {
						ADD_IB_TYPE(RENCODE_V4_IB_PARAM_ENCODE_STATISTICS);
						print_ib_encode_statistics(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf);
						break;
					}
				} else {
					if (ib_type == RENCODE_IB_PARAM_ENCODE_STATISTICS) {
						ADD_IB_TYPE(RENCODE_IB_PARAM_ENCODE_STATISTICS);
						print_ib_encode_statistics(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf);
						break;
					}
				}
				add_field_1(asic, vmid, vcn_addr, &offset, "ib_type: ", ib_type, 10, pOut, pBuf);
				fprintf(pOut, "[ERROR] FIXME Unknown type[0x%08x] size[%d] for VCN v%d_%d_%d\n",
					ib_type, ib_size, ip->discoverable.maj, ip->discoverable.min, ip->discoverable.rev);
				break;
		};
		p_curr += ib_size / 4 - 2;
		offset += ib_size - 8;
	}
}

static void vcn_enc_decode_ib_v1(struct umr_asic *asic, struct umr_vcn_cmd_message *vcn,
				 FILE *pOut, struct umr_ip_block *ip, struct vcn_gui_message *vgui)
{
	uint32_t ib_size, ib_type, nwords = vcn ? vcn->size / 4 : vgui->size / 4;
	uint32_t *p_curr = vcn ? vcn->buf : vgui->in_buf;
	char     ***pBuf = vgui ? vgui->out_buf : NULL;
	uint32_t *p_end = p_curr + nwords;
	uint32_t vcn_addr = vcn ? vcn->addr : 0;
	uint32_t vmid = vcn? vcn->vmid: 0;
	uint32_t offset = 0;

	while (p_curr < p_end) {
		ib_size = *p_curr++;
		ib_type = *p_curr++;
		add_field_1(asic, vmid, vcn_addr, &offset, "ib_size", ib_size, 10, pOut, pBuf);
		if (!ib_size) { /* empty */
			if (pOut)
				fprintf(pOut, "Empty messages!\n");
			break;
		}
		switch(ib_type)
		{
			case RADEON_VCN_SIGNATURE:
				ADD_IB_TYPE(RADEON_VCN_SIGNATURE);
				print_ib_signature(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf);
				break;
			case RADEON_VCN_ENGINE_INFO:
				ADD_IB_TYPE(RADEON_VCN_ENGINE_INFO);
				print_ib_engine_info(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf);
				break;
			case RENCODE_IB_PARAM_SESSION_INFO:
				ADD_IB_TYPE(RENCODE_IB_PARAM_SESSION_INFO);
				print_ib_session_info(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf);
				break;
			case RENCODE_V1_IB_PARAM_TASK_INFO:
				ADD_IB_TYPE(RENCODE_V1_IB_PARAM_TASK_INFO);
				print_ib_task_info(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf);
				break;
			case RENCODE_V1_IB_PARAM_SESSION_INIT:
				ADD_IB_TYPE(RENCODE_V1_IB_PARAM_SESSION_INIT);
				print_v1_ib_session_init(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf);
				break;
			case RENCODE_V1_IB_PARAM_LAYER_CONTROL:
				ADD_IB_TYPE(RENCODE_V1_IB_PARAM_LAYER_CONTROL);
				print_ib_layer_control(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf);
				break;
			case RENCODE_V1_IB_PARAM_LAYER_SELECT:
				ADD_IB_TYPE(RENCODE_V1_IB_PARAM_LAYER_SELECT);
				print_ib_layer_select(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf);
				break;
			case RENCODE_V1_IB_PARAM_RATE_CONTROL_SESSION_INIT:
				ADD_IB_TYPE(RENCODE_V1_IB_PARAM_RATE_CONTROL_SESSION_INIT);
				print_ib_rate_control_session_init(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf);
				break;
			case RENCODE_V1_IB_PARAM_RATE_CONTROL_LAYER_INIT:
				ADD_IB_TYPE(RENCODE_V1_IB_PARAM_RATE_CONTROL_LAYER_INIT);
				print_ib_rate_control_layer_init(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf);
				break;
			case RENCODE_V1_IB_PARAM_RATE_CONTROL_PER_PICTURE:
				ADD_IB_TYPE(RENCODE_V1_IB_PARAM_RATE_CONTROL_PER_PICTURE);
				print_ib_rate_control_per_picture(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf);
				break;
			case RENCODE_V1_IB_PARAM_QUALITY_PARAMS:
				ADD_IB_TYPE(RENCODE_V1_IB_PARAM_QUALITY_PARAMS);
				print_v1_ib_quality_params(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf);
				break;
			case RENCODE_V1_IB_PARAM_RATE_CONTROL_PER_PIC_EX:
				ADD_IB_TYPE(RENCODE_V1_IB_PARAM_RATE_CONTROL_PER_PIC_EX);
				print_ib_rate_control_per_picture_ex(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf);
				break;
			case RENCODE_V1_IB_PARAM_DIRECT_OUTPUT_NALU:
				ADD_IB_TYPE(RENCODE_V1_IB_PARAM_DIRECT_OUTPUT_NALU);
				print_ib_direct_output_nalu(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf);
				break;
			case RENCODE_V1_IB_PARAM_SLICE_HEADER:
				ADD_IB_TYPE(RENCODE_V1_IB_PARAM_SLICE_HEADER);
				print_ib_slice_header(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf);
				break;
			case RENCODE_V1_IB_PARAM_ENCODE_PARAMS:
				ADD_IB_TYPE(RENCODE_V1_IB_PARAM_ENCODE_PARAMS);
				print_v1_ib_encode_params(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf);
				break;
			case RENCODE_V1_IB_PARAM_INTRA_REFRESH:
				ADD_IB_TYPE(RENCODE_V1_IB_PARAM_INTRA_REFRESH);
				print_ib_intra_refresh(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf);
				break;
			case RENCODE_V1_IB_PARAM_ENCODE_CONTEXT_BUFFER:
				ADD_IB_TYPE(RENCODE_V1_IB_PARAM_ENCODE_CONTEXT_BUFFER);
				print_v1_ib_encode_context_buffer(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf);
				break;
			case RENCODE_V1_IB_PARAM_VIDEO_BITSTREAM_BUFFER:
				ADD_IB_TYPE(RENCODE_V1_IB_PARAM_VIDEO_BITSTREAM_BUFFER);
				print_ib_video_bitstream_buffer(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf);
				break;
			case RENCODE_V1_IB_PARAM_QP_MAP:
				ADD_IB_TYPE(RENCODE_V1_IB_PARAM_QP_MAP);
				print_ib_qp_map(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf);
				break;
			case RENCODE_V1_IB_PARAM_FEEDBACK_BUFFER:
				ADD_IB_TYPE(RENCODE_V1_IB_PARAM_FEEDBACK_BUFFER);
				print_ib_feedback_buffer(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf);
				break;
			case RENCODE_V1_IB_PARAM_ENCODE_LATENCY:
				ADD_IB_TYPE(RENCODE_V1_IB_PARAM_ENCODE_LATENCY);
				print_ib_encode_latency(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf);
				break;
			case RENCODE_V1_IB_PARAM_ENCODE_STATISTICS:
				ADD_IB_TYPE(RENCODE_V1_IB_PARAM_ENCODE_STATISTICS);
				print_ib_encode_statistics(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf);
				break;
			case RENCODE_HEVC_IB_PARAM_SLICE_CONTROL:
				ADD_IB_TYPE(RENCODE_HEVC_IB_PARAM_SLICE_CONTROL);
				print_ib_hevc_slice_control(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf);
				break;
			case RENCODE_HEVC_IB_PARAM_SPEC_MISC:
				ADD_IB_TYPE(RENCODE_HEVC_IB_PARAM_SPEC_MISC);
				print_v1_ib_hevc_spec_misc(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf);
				break;
			case RENCODE_V1_H264_IB_PARAM_SLICE_CONTROL:
				ADD_IB_TYPE(RENCODE_V1_H264_IB_PARAM_SLICE_CONTROL);
				print_ib_h264_slice_control(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf);
				break;
			case RENCODE_V1_H264_IB_PARAM_SPEC_MISC:
				ADD_IB_TYPE(RENCODE_V1_H264_IB_PARAM_SPEC_MISC);
				print_v1_ib_h264_spec_misc(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf);
				break;
			case RENCODE_V1_H264_IB_PARAM_ENCODE_PARAMS:
				ADD_IB_TYPE(RENCODE_V1_H264_IB_PARAM_ENCODE_PARAMS);
				print_v1_ib_h264_encode_params(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf);
				break;
			case RENCODE_V1_H264_IB_PARAM_DEBLOCKING_FILTER:
				ADD_IB_TYPE(RENCODE_V1_H264_IB_PARAM_DEBLOCKING_FILTER);
				print_ib_h264_deblocking_filter(asic, vmid, vcn_addr, offset, p_curr, ib_size, pOut, pBuf);
				break;
			case RENCODE_V1_IB_OP_INITIALIZE:
				ADD_IB_TYPE(RENCODE_V1_IB_OP_INITIALIZE);
				break;
			case RENCODE_IB_OP_INITIALIZE:
				ADD_IB_TYPE(RENCODE_IB_OP_INITIALIZE);
				break;
			case RENCODE_V1_IB_OP_CLOSE_SESSION:
				ADD_IB_TYPE(RENCODE_V1_IB_OP_CLOSE_SESSION);
				break;
			case RENCODE_IB_OP_CLOSE_SESSION:
				ADD_IB_TYPE(RENCODE_IB_OP_CLOSE_SESSION);
				break;
			case RENCODE_V1_IB_OP_ENCODE:
				ADD_IB_TYPE();
				break;
			case RENCODE_IB_OP_ENCODE:
				ADD_IB_TYPE(RENCODE_IB_OP_ENCODE);
				break;
			case RENCODE_V1_IB_OP_INIT_RC:
				ADD_IB_TYPE(RENCODE_V1_IB_OP_INIT_RC);
				break;
			case RENCODE_IB_OP_INIT_RC:
				ADD_IB_TYPE(RENCODE_IB_OP_INIT_RC);
				break;
			case RENCODE_V1_IB_OP_INIT_RC_VBV_BUFFER_LEVEL:
				ADD_IB_TYPE(RENCODE_V1_IB_OP_INIT_RC_VBV_BUFFER_LEVEL);
				break;
			case RENCODE_IB_OP_INIT_RC_VBV_BUFFER_LEVEL:
				ADD_IB_TYPE(RENCODE_IB_OP_INIT_RC_VBV_BUFFER_LEVEL);
				break;
			case RENCODE_V1_IB_OP_SET_SPEED_ENCODING_MODE:
				ADD_IB_TYPE(RENCODE_V1_IB_OP_SET_SPEED_ENCODING_MODE);
				break;
			case RENCODE_IB_OP_SET_SPEED_ENCODING_MODE:
				ADD_IB_TYPE(RENCODE_IB_OP_SET_SPEED_ENCODING_MODE);
				break;
			case RENCODE_V1_IB_OP_SET_BALANCE_ENCODING_MODE:
				ADD_IB_TYPE(RENCODE_V1_IB_OP_SET_BALANCE_ENCODING_MODE);
				break;
			case RENCODE_IB_OP_SET_BALANCE_ENCODING_MODE:
				ADD_IB_TYPE(RENCODE_IB_OP_SET_BALANCE_ENCODING_MODE);
				break;
			case RENCODE_V1_IB_OP_SET_QUALITY_ENCODING_MODE:
				ADD_IB_TYPE(RENCODE_V1_IB_OP_SET_QUALITY_ENCODING_MODE);
				break;
			case RENCODE_IB_OP_SET_QUALITY_ENCODING_MODE:
				ADD_IB_TYPE(RENCODE_IB_OP_SET_QUALITY_ENCODING_MODE);
				break;
			default:
				add_field_1(asic, vmid, vcn_addr, &offset, "ib_type: ", ib_type, 10, pOut, pBuf);
				if (pOut) {
					fprintf(pOut, "[ERROR] FIXME Unknown type[0x%08x] size[0x%08x] for VCN v%d_%d_%d\n",
						ib_type, ib_size, ip->discoverable.maj, ip->discoverable.min, ip->discoverable.rev);
					if (ib_type == 0xFFFFFFFF || ib_size == 0xFFFFFFFF) {
						dump_ib(asic, vcn, p_curr, p_end - p_curr, pOut);
						fprintf(pOut, "[ERROR] Invalid data received [%"PRIu32"] bytes not handled for VCN v%d_%d_%d\n",
							(uint32_t)((p_end - p_curr) * 4), ip->discoverable.maj, ip->discoverable.min, ip->discoverable.rev);
					}
				}
				break;
		};
		p_curr += ib_size / 4 - 2;
		offset += ib_size - 8;
	}
}

/* umr_vcn_decode - Decode VCN IB message called from GUI interface
 *
 * @asic: The ASIC model the packet decoding corresponds to
 * @p_curr: Pointer to the IB message from GUI
 * @size_in_byte: Size in bytes of the IB message in p_curr
 * @ib_addr: Reserved
 * @vcn_type: VCN type (0 for decode, 1 for encode)
 * @opcode_strs: Uninitialized output buffers for decoded IB message in in_buf
 *
 * Return 0 if sucessfully decode the IB, -1 otherwise.
 */
int umr_vcn_decode(struct umr_asic *asic, uint32_t *p_curr, uint32_t size_in_byte,
		   uint64_t ib_addr, uint32_t vcn_type, char ***opcode_strs)
{
	uint32_t nwords = size_in_byte / 4;
	uint32_t *p_end = p_curr + nwords;
	struct vcn_gui_message vgui;
	struct umr_ip_block *ip;
	uint32_t vcn_ip_version;
	uint32_t engine_type;
	int32_t major = -1;
	int32_t minor = -1;
	(void) ib_addr;

	//FIXME: some hw has both uvd and vcn
	ip = umr_find_ip_block(asic, "vcn", 0); // for multi instance
	if (!ip)
		ip = umr_find_ip_block(asic, "vcn", -1); // for single instance
	if (!ip) {
		asic->err_msg("[ERROR]: Cannot get VCN IP info\n");
		return -1;
	}
	vcn_ip_version = VCN_IP_VERSION(ip->discoverable.maj, ip->discoverable.min, ip->discoverable.rev);

	/* opcode_strs will be freed in rings_panel.cpp */
	*opcode_strs = calloc(nwords, sizeof(**opcode_strs));
	if (!*opcode_strs) {
		asic->err_msg("[ERROR]: Out of memory\n");
		return -1;
	}

	if (vcn_type == 0) {
		umr_print_dec_ib_msg(asic, NULL, NULL, ip, p_curr, size_in_byte, opcode_strs);
		return 0;
	}

	if (vcn_ip_version >= VCN_IP_VERSION(4, 0, 0)) { //for unified ring
		retrieve_engine_info(p_curr, p_end, &engine_type);
		if (engine_type == RADEON_VCN_ENGINE_TYPE_DECODE) {
			umr_vcn_dec_decode_unified_ring(asic, NULL, NULL, ip, p_curr, size_in_byte, opcode_strs);
			return 0;
		}

		retrieve_session_info(p_curr, p_end, &major, &minor);
		if((major == -1 && minor == -1) || engine_type == 0xFFFFFFFF) {
			/* we dnon't need to show ERROR message as the GUI interface should report it instead */
			return -1;
		}

		if (engine_type > 3) { /* 4 is encode_queue */
			asic->err_msg("[FIXME] Engine Type[0x%08x] for VCN v%d_%d_%d\n",
				engine_type, ip->discoverable.maj, ip->discoverable.min, ip->discoverable.rev);
			return -1;
		}
	}

	vgui.in_buf = p_curr;
	vgui.size = size_in_byte;
	vgui.out_buf = opcode_strs;

	/* RADEON_VCN_ENGINE_TYPE_ENCODE */
	switch(vcn_ip_version) {
	case VCN_IP_VERSION(1, 0, 0):
	case VCN_IP_VERSION(1, 0, 1):
		vcn_enc_decode_ib_v1(asic, NULL, NULL, ip, &vgui);
		break;
	default:
		vcn_enc_decode_ib(asic, NULL, NULL, ip, &vgui);
		break;
	}
	return 0;
}

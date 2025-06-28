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
 */
#include <stdio.h>
#include <string.h>
#include <malloc.h>

#include "umr.h"
#include "import/ac_vcn_dec.h"

#define SUPPORT 0

#define PRINT(arg) {/* fprintf(stdout, arg); */}

#define VCN_IP_VERSION(mj, mn, rv) (((mj) << 16) | ((mn) << 8) | (rv))

#define MAX_VALUE_SIZE 32
#define FORMAT10  "=%"PRIu32
#define FORMAT16  "=0x%"PRIx32
#define CFORMAT10 "=%s%"PRIu32"%s"
#define CFORMAT16 "=%s0x%"PRIx32"%s"
#define CFORMAT_8b  "\n[%s0x%"PRIx32"%s@%s0x%08"PRIx64"%s + %s0x%04"PRIx32"%s]\t[%s%14s0x%02"PRIx8"%s]\t|---> "
#define CFORMAT_16b "\n[%s0x%"PRIx32"%s@%s0x%08"PRIx64"%s + %s0x%04"PRIx32"%s]\t[%s%12s0x%04"PRIx16"%s]\t|---> "
#define CFORMAT_32b "\n[%s0x%"PRIx32"%s@%s0x%08"PRIx64"%s + %s0x%04"PRIx32"%s]\t[%s%8s0x%08"PRIx32"%s]\t|---> "

#define STR_HEVC_MESSAGE          "<<HEVC/H265 MESSAGE>> "
#define STR_HEADER_MESSAGE        "<<HEADER MESSAGE>> "
#define STR_CREATE_MESSAGE        "<<CREATE MESSAGE>> "
#define STR_DECODE_MESSAGE        "<<DECODE MESSAGE>> "
#define STR_VP9_MESSAGE           "<<VP9 MESSAGE>> "
#define STR_VC1_MESSAGE           "<<VC1 MESSAGE>> "
#define STR_MPEG2_VLD_MESSAGE     "<<MPEG2 VLD MESSAGE>> "
#define STR_MPEG4_ASP_VLD_MESSAGE "<<MPEG4 ASP VLD MESSAGE>> "
#define STR_AV1_MESSAGE           "<<AV1 MESSAGE>> "
#define STR_AVC_MESSAGE           "<<AVC/H264 MESSAGE>> "
#define STR_HEVC_DRL_MESSAGE      "<<HEVC DIRECT REFERENCE LIST MESSAGE>> "
#define STR_DPB_T1_MESSAGE        "<<DYNAMIC_DPB_T1 MESSAGE>> "
#define STR_DPB_T2_MESSAGE        "<<DYNAMIC_DPB_T2 MESSAGE>> "

static void print_hevc_message(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset, uint32_t size, rvcn_dec_message_hevc_t *msg, FILE *pOut, char ***pBuf);
static void print_header_msg(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset, uint32_t size, rvcn_dec_message_header_t *mh, FILE *pOut, char ***pBuf);
static void print_create_msg(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset, uint32_t size, rvcn_dec_message_create_t *pm, FILE *pOut, char ***pBuf);
static void print_decode_msg(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset, uint32_t size, rvcn_dec_message_decode_t *pm, FILE *pOut, char ***pBuf);
static void print_vp9_msg(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset, uint32_t size, rvcn_dec_message_vp9_t *pm, FILE *pOut, char ***pBuf);
static void print_vc1_msg(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset, uint32_t size, rvcn_dec_message_vc1_t *pm, FILE *pOut, char ***pBuf);
static void print_mpeg2_vld_msg(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset, uint32_t size, rvcn_dec_message_mpeg2_vld_t *pm, FILE *pOut, char ***pBuf);
static void print_mpeg4_asp_vld_msg(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset, uint32_t size, rvcn_dec_message_mpeg4_asp_vld_t *pm, FILE *pOut, char ***pBuf);
static void print_avc_msg(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset, uint32_t size, rvcn_dec_message_avc_t *pm, FILE *pOut, char ***pBuf);
static void print_file_grain_msg(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset, uint32_t size, rvcn_dec_film_grain_params_t *pm, FILE *pOut, char ***pBuf);
static void print_av1_msg(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset, uint32_t size, rvcn_dec_message_av1_t *pm, FILE *pOut, char ***pBuf);
static void print_hevc_direct_ref_list_msg(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset, uint32_t size, rvcn_dec_message_hevc_direct_ref_list_t *pm, FILE *pOut, char ***pBuf);
static void print_dynamic_dpb_msg(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset, uint32_t size, rvcn_dec_message_dynamic_dpb_t2_t *pm, FILE *pOut, char ***pBuf);
static void print_dynamic_dpb_t1_msg(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset, uint32_t size, rvcn_dec_message_dynamic_dpb_t *pm, FILE *pOut, char ***pBuf);

#if SUPPORT
static void print_wmv_msg(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset, uint32_t size, uvd_message_wmv_t *pm, FILE *pOut, char ***pBuf);
static void print_scaler_msg(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset, uint32_t size, uvd_message_scaler_t *pm, FILE *pOut, char ***pBuf);
static void print_mpeg2_idct_msg(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset, uint32_t size, uvd_message_mpeg2_idct_t *pm, FILE *pOut, char ***pBuf);
#endif

#define COMPARE_ERROR(a, b) \
	{ if (a != b) fprintf(pOut?pOut: stderr, "\n[ERROR] FIXME - %s size(%d - %d) does not match!", __func__, a, b); }

#define FIELD_SIZE_WARNING(a, b, c) \
	if ( (a + 2) != b) { \
		fprintf(pOut?pOut: stderr, "\nFIXME: %s incorrect field (%s) size [%d] dwords should be [%d] dwords\n",\
			       __func__, c, a + 2, b);\
	}

#define STRUCT_WARNING_REF(type) \
	{ uint32_t n = ((type *) (p_ctxt + pi->offset))->num_direct_reflist; \
	  n = n == 0? 1: n; \
	  if ((n * 30 + 3 )/4 * 4 + 4 != pi->size) { \
		fprintf(pOut?pOut: stderr, "\nFIXME: %s incorrect strucure (%s) size [%zd] bytes should be [%d] bytes for %d num_direct_reflist\n",\
			       __func__, #type, sizeof(type), pi->size, ((type *) (p_ctxt + pi->offset))->num_direct_reflist);\
		dump_ib((uint32_t *)(p_ctxt + pi->offset), pi->size/4, pOut);\
	  } \
	}

#define STRUCT_WARNING(type) \
	if (sizeof (type) != pi->size) { \
		fprintf(pOut?pOut: stderr, "\nFIXME: %s incorrect strucure (%s) size [%zd] bytes should be [%d] bytes\n",\
			       __func__, #type, sizeof(type), pi->size);\
		dump_ib((uint32_t *)(p_ctxt + pi->offset), pi->size/4, pOut);\
	}

static char *get_wmtype(rvcn_dec_transformation_type_e wmtype)
{
	switch(wmtype){
	case RVCN_DEC_AV1_IDENTITY: return "AV1_IDENTITY";
	case RVCN_DEC_AV1_TRANSLATION: return "AV1_TRANSLATION";
	case RVCN_DEC_AV1_ROTZOOM: return "AV1_ROTZOOM";
	case RVCN_DEC_AV1_AFFINE: return "AV1_AFFINE";
	case RVCN_DEC_AV1_HORTRAPEZOID: return "AV1_HORTRAPEZOID";
	case RVCN_DEC_AV1_VERTRAPEZOID: return "AV1_VERTRAPEZOID";
	case RVCN_DEC_AV1_HOMOGRAPHY: return "AV1_HOMOGRAPHY";
	case RVCN_DEC_AV1_TRANS_TYPES: return "AV1_TRANS_TYPES";
	default: return "AV1_INVALIDE";
	}
}

static void add_header(uint32_t vmid, uint64_t addr, uint32_t offset, uint32_t size, char *msg, FILE *pOut, char ***pBuf)
{
	(void) pBuf;
	if(pOut)
		fprintf(pOut, "\n%s at 0x%" PRIx32 "@0x%" PRIx64 " offset 0x%" PRIx32 " of %" PRIu32 " words", msg, vmid, addr, offset, size);
}

static void add_tail(uint32_t vmid, uint64_t addr, uint32_t offset, char *msg, FILE *pOut, char ***pBuf)
{
	(void) pBuf;
	if(pOut)
		fprintf(pOut, "\nDone %s at 0x%" PRIx32 "@0x%" PRIx64 " offset 0x%" PRIx32, msg, vmid, addr, offset);
}

static void add_field(struct umr_asic *asic, uint32_t vmid, uint64_t addr, uint32_t *offset, char *str, uint32_t value, uint32_t ideal_radix, FILE *fp, char ***buf)
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
	if (fp) { /* assume vcn is valid */
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

static void add_field_1(struct umr_asic *asic, uint32_t vmid, uint64_t addr, uint32_t *offset, char *str, uint32_t value, uint32_t bits, uint32_t ideal_radix, FILE *fp, char ***buf)
{
	if (buf) {
		int r = 0;
		char *t0 = (*buf)[*offset/4];
		int size = strlen(str) + MAX_VALUE_SIZE;
		if (t0)
			size += strlen(t0);
		char *t = calloc(1, size);
		char *p = t;
		(*buf)[*offset/4] = t;
		if (t0) {
			r = sprintf(p, "%s,", t0);
			p += r;
			free(t0);
		}
		r = sprintf(p, "%s", str);
		p += r;
		if (ideal_radix == 10)
			sprintf(p, FORMAT10, value);
		else if (ideal_radix == 16)
			sprintf(p, FORMAT16, value);
		*offset += bits/8;
	}
	if (fp) { /* assume vcn is valid */
		if(bits == 8) {
			fprintf(fp, CFORMAT_8b "%s",
				BLUE, vmid, RST,
				YELLOW, addr, RST,
				YELLOW, *offset, RST,
				BMAGENTA, "", (uint8_t)value, RST, str);
			*offset += 1;
		} else if(bits == 16) {
			fprintf(fp, CFORMAT_16b "%s",
				BLUE, vmid, RST,
				YELLOW, addr, RST,
				YELLOW, *offset, RST,
				BMAGENTA, "", (uint16_t)value, RST, str);
			*offset += 2;
		} else if(bits == 32) {
			fprintf(fp, CFORMAT_32b "%s",
				BLUE, vmid, RST,
				YELLOW, addr, RST,
				YELLOW, *offset, RST,
				BMAGENTA, "", value, RST, str);
			*offset += 4;
		}
		if (ideal_radix == 10)
			fprintf(fp, CFORMAT10, BBLUE, value, RST);
		else if (ideal_radix == 16)
			fprintf(fp, CFORMAT16, YELLOW, value, RST);
	}
}

static void add_field_2(struct umr_asic *asic, uint32_t vmid, uint64_t addr, uint32_t *offset,
			uint32_t id, char *name, uint32_t value, uint32_t ideal_radix, FILE *fp, char ***buf)
{
	if (buf) {
		char *t = calloc(1, strlen(name) + MAX_VALUE_SIZE);
		int r = 0;
		char *p = t;
		(*buf)[*offset/4] = t;
		r = sprintf(p, "MESSAGE[%d]:%s", id, name);
		p += r;
		if (ideal_radix == 10)
			sprintf(p, FORMAT10, value);
		else if (ideal_radix == 16)
			sprintf(p, FORMAT16, value);
	}
	if (fp) { /* assume vcn is valid */
		fprintf(fp, CFORMAT_32b "MESSAGE[%d]:%s",
			BLUE, vmid, RST,
			YELLOW, addr, RST,
			YELLOW, *offset, RST,
			BMAGENTA, "", value, RST, id, name);
		if (ideal_radix == 10)
			fprintf(fp, CFORMAT10, BBLUE, value, RST);
		else if (ideal_radix == 16)
			fprintf(fp, CFORMAT16, YELLOW, value, RST);
	}
	*offset += 4;
}

// name + index
static void add_field_3_i(struct umr_asic *asic, uint32_t vmid, uint64_t addr, uint32_t *offset, char *name,
			 int idx0, uint32_t idx, uint32_t value, uint32_t bits, uint32_t ideal_radix, FILE *fp, char ***buf)
{
	if (buf) {
		int r = 0;
		char *t0 = (*buf)[*offset/4];
		int size = strlen(name) + MAX_VALUE_SIZE;
		if (t0)
			size += strlen(t0);
		char *t = calloc(1, size);
		char *p = t;
		(*buf)[*offset/4] = t;
		if (t0) {
			r = sprintf(p, "%s,", t0);
			p += r;
			free(t0);
		}
		if (idx0 > 0)
			r = sprintf(p, "%s[%d][%d]", name, idx0, idx);
		else
			r = sprintf(p, "%s[%d]", name, idx);
		p += r;
		if (ideal_radix == 10)
			sprintf(p, FORMAT10, value);
		else if (ideal_radix == 16)
			sprintf(p, FORMAT16, value);
		*offset += bits/8;
	}
	if (fp) { /* assume vcn is valid */
		if(bits == 8) {
			if (idx0 > 0)
				fprintf(fp, CFORMAT_8b "%s[%d][%d]",
					BLUE, vmid, RST,
					YELLOW, addr, RST,
					YELLOW, *offset, RST,
					BMAGENTA, "", (uint8_t)value, RST, name, idx0, idx);
			else
				fprintf(fp, CFORMAT_8b "%s[%d]",
					BLUE, vmid, RST,
					YELLOW, addr, RST,
					YELLOW, *offset, RST,
					BMAGENTA, "", (uint8_t)value, RST, name, idx);
			*offset += 1;
		} else	if(bits == 16) {
			if (idx0 > 0)
				fprintf(fp, CFORMAT_16b "%s[%d][%d]",
					BLUE, vmid, RST,
					YELLOW, addr, RST,
					YELLOW, *offset, RST,
					BMAGENTA, "", (uint16_t)value, RST, name, idx0, idx);
			else
				fprintf(fp, CFORMAT_16b "%s[%d]",
					BLUE, vmid, RST,
					YELLOW, addr, RST,
					YELLOW, *offset, RST,
					BMAGENTA, "", (uint16_t)value, RST, name, idx);
			*offset += 2;
		} else {
			if (idx0 > 0)
				fprintf(fp, CFORMAT_32b "%s[%d][%d]",
					BLUE, vmid, RST,
					YELLOW, addr, RST,
					YELLOW, *offset, RST,
					BMAGENTA, "", value, RST, name, idx0, idx);
			else
				fprintf(fp, CFORMAT_32b "%s[%d]",
					BLUE, vmid, RST,
					YELLOW, addr, RST,
					YELLOW, *offset, RST,
					BMAGENTA, "", value, RST, name, idx);
			*offset += 4;
		}
		if (ideal_radix == 10)
			fprintf(fp, CFORMAT10, BBLUE, value, RST);
		else if (ideal_radix == 16)
			fprintf(fp, CFORMAT16, YELLOW, value, RST);
	}
}

static void add_field_3(struct umr_asic *asic, uint32_t vmid, uint64_t addr, uint32_t *offset, char *name,
			uint32_t idx, uint32_t value, uint32_t bits, uint32_t ideal_radix, FILE *fp, char ***buf)
{
	add_field_3_i(asic, vmid, addr, offset, name, -1, idx, value, bits, ideal_radix, fp, buf);
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
	if (fp) { /* assume vcn is valid */
		if (ideal_radix == 10)
			fprintf(fp, ",%sbits[%s] %s=%"PRIu32"%s", BBLUE, bits, name, value, RST);
		else
			fprintf(fp, ",%sbits[%s] %s=0x%"PRIx32"%s", BBLUE, bits, name, value, RST);
	}
}

static void dump_ib(uint32_t *buf, uint32_t size, FILE *fp)
{
	uint32_t j = 0;
	if (fp)
		while (j < size) {
			fprintf(fp, "\n[0x%04x] 0x%08x", j*4, buf[j]);
			j++;
		}
}

/* umr_parse_vcn_dec - Parse VCN decode IB message
 *
 * @asic: The ASIC model the packet decoding corresponds to
 * @vcn: VCN command message to be parsed
 * @pOut: The output file stream
 *
 */
void umr_parse_vcn_dec(struct umr_asic *asic, struct umr_vcn_cmd_message *vcn, FILE *pOut)
{
	struct umr_ip_block *ip;

	ip = umr_find_ip_block(asic, "vcn", 0); // for multi instance
	if (!ip)
		ip = umr_find_ip_block(asic, "vcn", -1); // for single instance
	if (!ip) {
		asic->err_msg("\n[ERROR]: Cannot get VCN IP info\n");
		return;
	}

	/* ignore VMID 0, only for multimedia decoding messages from userland */
	if (vcn->vmid == 0)
	       return;

	if (vcn->type == 0) { /* dec ring case */
		switch (vcn->cmd) {
			case RDECODE_CMD_MSG_BUFFER:
				umr_print_dec_ib_msg(asic, vcn, pOut, ip, NULL, 0, NULL);
				break;
			case RDECODE_CMD_DPB_BUFFER:
				PRINT("RDECODE_CMD_DPB_BUFFER");
				break;
			case RDECODE_CMD_DECODING_TARGET_BUFFER:
				PRINT("RDECODE_CMD_DECODING_TARGET_BUFFER");
				break;
			case RDECODE_CMD_FEEDBACK_BUFFER:
				PRINT("RDECODE_CMD_FEEDBACK_BUFFER");
				break;
			case RDECODE_CMD_PROB_TBL_BUFFER:
				PRINT("RDECODE_CMD_PROB_TBL_BUFFER");
				break;
			case RDECODE_CMD_SESSION_CONTEXT_BUFFER:
				PRINT("RDECODE_CMD_SESSION_CONTEXT_BUFFER");
				break;
			case RDECODE_CMD_BITSTREAM_BUFFER:
				PRINT("RDECODE_CMD_BITSTREAM_BUFFER");
				break;
			case RDECODE_CMD_IT_SCALING_TABLE_BUFFER:
				PRINT("RDECODE_CMD_IT_SCALING_TABLE_BUFFER");
				break;
			case RDECODE_CMD_CONTEXT_BUFFER:
				PRINT("RDECODE_CMD_CONTEXT_BUFFER");
				break;
			default:
				break;
		}
	} else { /* unified ring - same as enc ring */
		fprintf(pOut, "\n[ERROR]: FIXME - should not be here\n");
	}
}

static char *get_ib_type(uint32_t ib_type)
{
	switch (ib_type){
	case RADEON_VCN_SIGNATURE: return "VCN_SIGNATURE";
	case RADEON_VCN_ENGINE_INFO: return "VCN_ENGINE_INFO";
	case RDECODE_IB_PARAM_DECODE_BUFFER: return "VCN_IB_PARAM_DECODE_BUFFER";
	default: return "Unknown IB TYPE";
	}
}

static void vcn_dec_decode_ib_buffer(struct umr_asic *asic, uint32_t vmid, uint64_t addr,
				     uint32_t offset, uint32_t *p_curr, FILE *pOut, char ***pBuf)
{
	rvcn_decode_buffer_t *p = (rvcn_decode_buffer_t *)p_curr;

	add_field(asic, vmid, addr, &offset, "VALID_BUF_FLAG", p->valid_buf_flag, 16, pOut, pBuf);
	add_field(asic, vmid, addr, &offset, "MSG_BUFFER_ADDRESS_HI", p->msg_buffer_address_hi, 16, pOut, pBuf);
	add_field(asic, vmid, addr, &offset, "MSG_BUFFER_ADDRESS_LO", p->msg_buffer_address_lo, 16, pOut, pBuf);
	add_field(asic, vmid, addr, &offset, "DPB_BUFFER_ADDRESS_HI", p->dpb_buffer_address_hi, 16, pOut, pBuf);
	add_field(asic, vmid, addr, &offset, "DPB_BUFFER_ADDRESS_LO", p->dpb_buffer_address_lo, 16, pOut, pBuf);
	add_field(asic, vmid, addr, &offset, "TARGET_BUFFER_ADDRESS_HI", p->target_buffer_address_hi, 16, pOut, pBuf);
	add_field(asic, vmid, addr, &offset, "TARGET_BUFFER_ADDRESS_LO", p->target_buffer_address_lo, 16, pOut, pBuf);
	add_field(asic, vmid, addr, &offset, "SESSION_CONTEX_BUFFER_ADDRESS_HI", p->session_contex_buffer_address_hi, 16, pOut, pBuf);
	add_field(asic, vmid, addr, &offset, "SESSION_CONTEX_BUFFER_ADDRESS_LO", p->session_contex_buffer_address_lo, 16, pOut, pBuf);
	add_field(asic, vmid, addr, &offset, "BITSTREAM_BUFFER_ADDRESS_HI", p->bitstream_buffer_address_hi, 16, pOut, pBuf);
	add_field(asic, vmid, addr, &offset, "BITSTREAM_BUFFER_ADDRESS_LO", p->bitstream_buffer_address_lo, 16, pOut, pBuf);
	add_field(asic, vmid, addr, &offset, "CONTEXT_BUFFER_ADDRESS_HI", p->context_buffer_address_hi, 16, pOut, pBuf);
	add_field(asic, vmid, addr, &offset, "CONTEXT_BUFFER_ADDRESS_LO", p->context_buffer_address_lo, 16, pOut, pBuf);
	add_field(asic, vmid, addr, &offset, "FEEDBACK_BUFFER_ADDRESS_HI", p->feedback_buffer_address_hi, 16, pOut, pBuf);
	add_field(asic, vmid, addr, &offset, "FEEDBACK_BUFFER_ADDRESS_LO", p->feedback_buffer_address_lo, 16, pOut, pBuf);
	add_field(asic, vmid, addr, &offset, "LUMA_HIST_BUFFER_ADDRESS_HI", p->luma_hist_buffer_address_hi, 16, pOut, pBuf);
	add_field(asic, vmid, addr, &offset, "LUMA_HIST_BUFFER_ADDRESS_LO", p->luma_hist_buffer_address_lo, 16, pOut, pBuf);
	add_field(asic, vmid, addr, &offset, "PROB_TBL_BUFFER_ADDRESS_HI", p->prob_tbl_buffer_address_hi, 16, pOut, pBuf);
	add_field(asic, vmid, addr, &offset, "PROB_TBL_BUFFER_ADDRESS_LO", p->prob_tbl_buffer_address_lo, 16, pOut, pBuf);
	add_field(asic, vmid, addr, &offset, "SCLR_COEFF_BUFFER_ADDRESS_HI", p->sclr_coeff_buffer_address_hi, 16, pOut, pBuf);
	add_field(asic, vmid, addr, &offset, "SCLR_COEFF_BUFFER_ADDRESS_LO", p->sclr_coeff_buffer_address_lo, 16, pOut, pBuf);
	add_field(asic, vmid, addr, &offset, "IT_SCLR_TABLE_BUFFER_ADDRESS_HI", p->it_sclr_table_buffer_address_hi, 16, pOut, pBuf);
	add_field(asic, vmid, addr, &offset, "IT_SCLR_TABLE_BUFFER_ADDRESS_LO", p->it_sclr_table_buffer_address_lo, 16, pOut, pBuf);
	add_field(asic, vmid, addr, &offset, "SCLR_TARGET_BUFFER_ADDRESS_HI", p->sclr_target_buffer_address_hi, 16, pOut, pBuf);
	add_field(asic, vmid, addr, &offset, "SCLR_TARGET_BUFFER_ADDRESS_LO", p->sclr_target_buffer_address_lo, 16, pOut, pBuf);
	add_field(asic, vmid, addr, &offset, "RESERVED_SIZE_INFO_BUFFER_ADDRESS_HI", p->reserved_size_info_buffer_address_hi, 16, pOut, pBuf);
	add_field(asic, vmid, addr, &offset, "RESERVED_SIZE_INFO_BUFFER_ADDRESS_LO", p->reserved_size_info_buffer_address_lo, 16, pOut, pBuf);
	add_field(asic, vmid, addr, &offset, "MPEG2_PIC_PARAM_BUFFER_ADDRESS_HI", p->mpeg2_pic_param_buffer_address_hi, 16, pOut, pBuf);
	add_field(asic, vmid, addr, &offset, "MPEG2_PIC_PARAM_BUFFER_ADDRESS_LO", p->mpeg2_pic_param_buffer_address_lo, 16, pOut, pBuf);
	add_field(asic, vmid, addr, &offset, "MPEG2_MB_CONTROL_BUFFER_ADDRESS_HI", p->mpeg2_mb_control_buffer_address_hi, 16, pOut, pBuf);
	add_field(asic, vmid, addr, &offset, "MPEG2_MB_CONTROL_BUFFER_ADDRESS_LO", p->mpeg2_mb_control_buffer_address_lo, 16, pOut, pBuf);
	add_field(asic, vmid, addr, &offset, "MPEG2_IDCT_COEFF_BUFFER_ADDRESS_HI", p->mpeg2_idct_coeff_buffer_address_hi, 16, pOut, pBuf);
	add_field(asic, vmid, addr, &offset, "MPEG2_IDCT_COEFF_BUFFER_ADDRESS_LO", p->mpeg2_idct_coeff_buffer_address_lo, 16, pOut, pBuf);
}

/* umr_vcn_dec_decode_unified_ring - Decode IB message from unified ring
 *
 * covers command line and GUI cases
 *    - for Command Line: vcn and pOut must be valid
 *    - for GUI: gui_inbuf and out_buf must be valid
 *
 * @asic: The ASIC model the packet decoding corresponds to
 * @vcn: VCN command message to be parsed
 * @pOut: The output file stream
 * @ip: IP block info
 * @gui_inbuf: Pointer to the IB message from GUI
 * @gui_size: Size in bytes of the IB message in gui_inbuf
 * @out_buf: Output buffers for decoded messages in gui_inbuf
 *
 */
void umr_vcn_dec_decode_unified_ring(struct umr_asic *asic, struct umr_vcn_cmd_message *vcn, FILE *pOut,
				     struct umr_ip_block *ip, uint32_t *gui_inbuf, uint32_t gui_size, char ***out_buf)
{
	uint32_t ib_size, ib_type, nwords = vcn ? vcn->size / 4 : gui_size / 4;
	uint32_t *p_curr = vcn ? vcn->buf : gui_inbuf;
	char     ***pBuf = out_buf;
	uint32_t *p_end = p_curr + nwords;
	uint64_t vcn_addr = vcn? vcn->addr : 0;
	uint32_t vmid = vcn? vcn->vmid: 0;
	uint32_t offset = 0;

	if (pOut)
		fprintf(pOut, "\nDecoding VCN message at 0x%" PRIx32 "@0x%" PRIx64 " from 0x%" PRIx32 "@0x%" PRIx64 " of %" PRIu32 " words",
			vmid, vcn_addr, 0, vcn ? vcn->from : 0, nwords);
	while (p_curr < p_end) {
		ib_size = *p_curr++;
		ib_type = *p_curr++;
		if (!ib_size) { /* empty */
			if (pOut)
				fprintf(pOut, "Empty messages!\n");
			break;
		}
		add_field(asic, vmid, vcn_addr, &offset, "IB_SIZE", ib_size, 10, pOut, pBuf);
		add_field(asic, vmid, vcn_addr, &offset, get_ib_type(ib_type), ib_type, 16, pOut, pBuf);
		switch(ib_type)
		{
			case RADEON_VCN_SIGNATURE:
				add_field(asic, vmid, vcn_addr, &offset, "IB_CHECKSUM", *p_curr, 16, pOut, pBuf);
				add_field(asic, vmid, vcn_addr, &offset, "NUMBER_OF_DWORDS", *(p_curr + 1), 10, pOut, pBuf);
				FIELD_SIZE_WARNING(2, ib_size / 4, "RADEON_VCN_SIGNATURE");
				break;
			case RADEON_VCN_ENGINE_INFO:
				add_field(asic, vmid, vcn_addr, &offset, "ENGINE_TYPE(decode)", *p_curr, 10, pOut, pBuf);
				add_field(asic, vmid, vcn_addr, &offset, "SIZE_OF_PACKAGES", *(p_curr + 1), 10, pOut, pBuf);
				FIELD_SIZE_WARNING(2, ib_size / 4, "RADEON_VCN_ENGINE_INFO");
				break;
			case RDECODE_IB_PARAM_DECODE_BUFFER:
				FIELD_SIZE_WARNING((uint32_t) (sizeof(rvcn_decode_buffer_t) / 4), ib_size / 4, "RDECODE_IB_PARAM_DECODE_BUFFER");
				vcn_dec_decode_ib_buffer(asic, vmid, vcn_addr, offset, p_curr, pOut, pBuf);
				offset += ib_size - 8;
				break;
			default:
				if (pOut)
					fprintf(pOut, "\n[ERROR] FIXME Unknown type[0x%08x] size[0x%08x] for VCN v%d_%d_%d",
						ib_type, ib_size, ip->discoverable.maj, ip->discoverable.min, ip->discoverable.rev);
				if (ib_type == 0xFFFFFFFF || ib_size == 0xFFFFFFFF) {
					dump_ib(p_curr, p_end - p_curr, pOut);
					if (pOut)
						fprintf(pOut, "\n[ERROR] Invalid data received [%"PRIu32"] bytes not handled for VCN v%d_%d_%d",
							(uint32_t)((p_end - p_curr) * 4), ip->discoverable.maj, ip->discoverable.min, ip->discoverable.rev);
				}
				offset += ib_size - 8;
				break;
		};
		p_curr += ib_size / 4 - 2;
	}
	if (pOut)
		fprintf(pOut, "\nDone Decoding VCN message at 0x%" PRIx32 "@0x%" PRIx64 "\n", vcn->vmid, vcn->addr);
}

/* umr_print_dec_ib_msg  - Dump the decoded decode IB message
 *
 * cover both cases:
 *   1 Command Line
 *      - pOut must be valid
 *   2 GUI
 *    - in_buf, in_size, pBuf must be valid
 *
 * @asic: The ASIC model the packet decoding corresponds to
 * @vcn: VCN command message
 * @pOut: The output file stream
 * @ip: IP block info
 * @in_buf: Pointer to the IB message from GUI
 * @in_size: Size in bytes of the IB message in in_buf
 * @pBuf: Output buffers for decoded IB message in in_buf
 *
 */
void umr_print_dec_ib_msg(struct umr_asic *asic, struct umr_vcn_cmd_message *vcn, FILE * pOut,
		          struct umr_ip_block *ip, uint32_t *in_buf, uint32_t in_size, char ***pBuf)
{
	uint32_t size = sizeof(rvcn_dec_message_header_t);
	rvcn_dec_message_header_t *mh;
	int partition = asic->options.vm_partition;
	rvcn_dec_message_index_t *pi = NULL;
	uint32_t tvmid = vcn? vcn->vmid : 0; // not used for GUI use case
	uint64_t addr = vcn? vcn->addr : 0;  // not used for GUI use case
	uint64_t from = vcn? vcn->from : 0;  // not used for GUI use case
	uint8_t *p_ctxt = NULL;
	int message_vp9 = 0;
	uint32_t i = 0;
	(void) in_size;

	if (pOut) {
		if (!vcn || !vcn->size) {
			asic->err_msg("\n[ERROR]: invalid VCN message\n");
			return;
		}
		p_ctxt = malloc(vcn->size);
		if (!p_ctxt) {
			asic->err_msg("\n[ERROR]: Running out memory\n");
			return;
		}
		if (umr_read_vram(asic, partition, tvmid, addr, vcn->size, p_ctxt) < 0) {
			asic->err_msg("\n[ERROR]: Could not read IB Message at 0x%" PRIx32 "@0x%" PRIx64 "\n", tvmid, addr);
			free(p_ctxt);
			return;
		}
		mh = (rvcn_dec_message_header_t *) p_ctxt;
	} else {
		mh = (rvcn_dec_message_header_t *) in_buf;
		p_ctxt = (uint8_t *)in_buf;
	}

	if (pOut)
		fprintf(pOut, "\nDecoding VCN message at 0x%" PRIx32 "@0x%" PRIx64 " from 0x%" PRIx32 "@0x%" PRIx64 " of %" PRIu32 " words",
			tvmid, addr, tvmid, from, vcn->size / 4);

	if (mh->header_size == sizeof(rvcn_dec_message_header_t)) {
		do {
			if (mh->num_buffers == 0) { /* DESTROY message */
				print_header_msg(asic, tvmid, addr, 0, size, mh, pOut, pBuf);
				break;
			} else {
				size += (mh->num_buffers - 1) * sizeof(rvcn_dec_message_index_t);
			}
			print_header_msg(asic, tvmid, addr, 0, size, mh, pOut, pBuf);

			pi = &mh->index[0];

			if (size != pi->offset) {
				/* buffer may not be updated due to race condition */
				fprintf(pOut ? pOut : stderr, "\n[WARN]: Decode IB Message(num_buffers=%d) has incorrect message offset: should be [0x%x] instead of [0x%x]", mh->num_buffers, size, pi->offset);
				if (size < pi->offset) /* for debugging only */
					dump_ib((uint32_t *)(p_ctxt + size), (pi->offset - size) / 4, pOut);
			}
			for (i = 0; i < mh->num_buffers; i++, pi++) {
				switch (pi->message_id) {
				case RDECODE_MESSAGE_CREATE:
					STRUCT_WARNING(rvcn_dec_message_create_t);
					print_create_msg(asic, tvmid, addr, pi->offset, pi->size, (rvcn_dec_message_create_t *)(p_ctxt + pi->offset), pOut, pBuf);
					break;
				case RDECODE_MESSAGE_DECODE:
					STRUCT_WARNING(rvcn_dec_message_decode_t);
					print_decode_msg(asic, tvmid, addr, pi->offset, pi->size, (rvcn_dec_message_decode_t *)(p_ctxt + pi->offset), pOut, pBuf);
					break;
				case RDECODE_MESSAGE_AVC:
					STRUCT_WARNING(rvcn_dec_message_avc_t);
					print_avc_msg(asic, tvmid, addr, pi->offset, pi->size, (rvcn_dec_message_avc_t *)(p_ctxt + pi->offset), pOut, pBuf);
					break;
				case RDECODE_MESSAGE_DYNAMIC_DPB:
					if (message_vp9 && VCN_IP_VERSION(ip->discoverable.maj, ip->discoverable.min, 0) <= VCN_IP_VERSION(2, 6, 0)) { /* DPB_DYNAMIC_TIER_1 */
						STRUCT_WARNING(rvcn_dec_message_dynamic_dpb_t);
						print_dynamic_dpb_t1_msg(asic, tvmid, addr, pi->offset, pi->size, (rvcn_dec_message_dynamic_dpb_t *)(p_ctxt + pi->offset), pOut, pBuf);
					} else {
						STRUCT_WARNING(rvcn_dec_message_dynamic_dpb_t2_t);
						print_dynamic_dpb_msg(asic, tvmid, addr, pi->offset, pi->size, (rvcn_dec_message_dynamic_dpb_t2_t *)(p_ctxt + pi->offset), pOut, pBuf);
					}
					break;
				case RDECODE_MESSAGE_VC1:
					STRUCT_WARNING(rvcn_dec_message_vc1_t);
					print_vc1_msg(asic, tvmid, addr, pi->offset, pi->size, (rvcn_dec_message_vc1_t *)(p_ctxt + pi->offset), pOut, pBuf);
					break;
#if SUPPORT
				case RDECODE_MESSAGE_WMV:
					print_wmv_msg(asic, tvmid, addr, pi->offset, pi->size, (uvd_message_wmv_t *)(p_ctxt + pi->offset), pOut, pBuf);
					break;
				case RDECODE_MESSAGE_MPEG2_IDCT:
					print_mpeg2_idct_msg(asic, tvmid, addr, pi->offset, pi->size, (uvd_message_mpeg2_idct_t *)(p_ctxt + pi->offset), pOut, pBuf);
					break;
#endif
				case RDECODE_MESSAGE_MPEG2_VLD:
					STRUCT_WARNING(rvcn_dec_message_mpeg2_vld_t);
					print_mpeg2_vld_msg(asic, tvmid, addr, pi->offset, pi->size, (rvcn_dec_message_mpeg2_vld_t *)(p_ctxt + pi->offset), pOut, pBuf);
					break;
				case RDECODE_MESSAGE_MPEG4_ASP_VLD:
					STRUCT_WARNING(rvcn_dec_message_mpeg4_asp_vld_t);
					print_mpeg4_asp_vld_msg(asic, tvmid, addr, pi->offset, pi->size, (rvcn_dec_message_mpeg4_asp_vld_t *)(p_ctxt + pi->offset), pOut, pBuf);
					break;
				case RDECODE_MESSAGE_HEVC:
					STRUCT_WARNING(rvcn_dec_message_hevc_t);
					print_hevc_message(asic, tvmid, addr, pi->offset, pi->size, (rvcn_dec_message_hevc_t *)(p_ctxt + pi->offset), pOut, pBuf);
					break;
				case RDECODE_MESSAGE_VP9:
					STRUCT_WARNING(rvcn_dec_message_vp9_t);
					print_vp9_msg(asic, tvmid, addr, pi->offset, pi->size, (rvcn_dec_message_vp9_t *)(p_ctxt + pi->offset), pOut, pBuf);
					message_vp9 = 1;
					break;
#if SUPPORT
				case RDECODE_MESSAGE_SCALER:
					print_scaler_msg(asic, tvmid, addr, pi->offset, pi->size, (uvd_message_scaler_t *)(p_ctxt + pi->offset), pOut, pBuf);
					break;
#endif
				case RDECODE_MESSAGE_AV1:
					STRUCT_WARNING(rvcn_dec_message_av1_t);
					print_av1_msg(asic, tvmid, addr, pi->offset, pi->size, (rvcn_dec_message_av1_t *)(p_ctxt + pi->offset), pOut, pBuf);
					break;
				case RDECODE_MESSAGE_HEVC_DIRECT_REF_LIST:
					STRUCT_WARNING_REF(rvcn_dec_message_hevc_direct_ref_list_t);
					print_hevc_direct_ref_list_msg(asic, tvmid, addr, pi->offset, pi->size, (rvcn_dec_message_hevc_direct_ref_list_t *)(p_ctxt + pi->offset), pOut, pBuf);
					break;
				case RDECODE_MESSAGE_NOT_SUPPORTED:
				default:
					asic->err_msg("\n[ERROR]: Find invalid or unsupported message at 0x%" PRIx32 ":0x%" PRIx64 "\n", tvmid, addr);
					break;
				}
			}
		} while (0);
	} else {
		print_header_msg(asic, tvmid, addr, 0, size, mh, pOut, pBuf);
	}
	if (pOut)
		fprintf(pOut, "\nDone Decoding VCN message at 0x%" PRIx32 "@0x%" PRIx64 "\n", tvmid, addr);

	if (pOut)
		free(p_ctxt);
}

static void print_hevc_message(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset,
			       uint32_t size, rvcn_dec_message_hevc_t *msg, FILE *pOut, char ***pBuf)
{
	uint32_t o_offset = offset;
	unsigned int i, j;

	add_header(tvmid, addr, o_offset, size/4, "HEVC/H265 MESSAGE",  pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, STR_HEVC_MESSAGE "SPS_INFO_FLAGS", msg->sps_info_flags, 32, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "PPS_INFO_FLAGS", msg->pps_info_flags, 32, 16, pOut, pBuf);

	add_field_1(asic, tvmid, addr, &offset, "CHROMA_FORMAT", msg->chroma_format, 8, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "BIT_DEPTH_LUMA_MINUS8", msg->bit_depth_luma_minus8, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "BIT_DEPTH_CHROMA_MINUS8", msg->bit_depth_chroma_minus8, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "LOG2_MAX_PIC_ORDER_CNT_LSB_MINUS4", msg->log2_max_pic_order_cnt_lsb_minus4, 8, 10, pOut, pBuf);

	add_field_1(asic, tvmid, addr, &offset, "SPS_MAX_DEC_PIC_BUFFERING_MINUS1", msg->sps_max_dec_pic_buffering_minus1, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "LOG2_MIN_LUMA_CODING_BLOCK_SIZE_MINUS3", msg->log2_min_luma_coding_block_size_minus3, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "LOG2_DIFF_MAX_MIN_LUMA_CODING_BLOCK_SIZE", msg->log2_diff_max_min_luma_coding_block_size, 8, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "LOG2_MIN_TRANSFORM_BLOCK_SIZE_MINUS2", msg->log2_min_transform_block_size_minus2, 8, 10, pOut, pBuf);

	add_field_1(asic, tvmid, addr, &offset, "LOG2_DIFF_MAX_MIN_TRANSFORM_BLOCK_SIZE", msg->log2_diff_max_min_transform_block_size, 8, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "MAX_TRANSFORM_HIERARCHY_DEPTH_INTER", msg->max_transform_hierarchy_depth_inter, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "MAX_TRANSFORM_HIERARCHY_DEPTH_INTRA", msg->max_transform_hierarchy_depth_intra, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "PCM_SAMPLE_BIT_DEPTH_LUMA_MINUS1", msg->pcm_sample_bit_depth_luma_minus1, 8, 10, pOut, pBuf);

	add_field_1(asic, tvmid, addr, &offset, "PCM_SAMPLE_BIT_DEPTH_CHROMA_MINUS1", msg->pcm_sample_bit_depth_chroma_minus1, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "LOG2_MIN_PCM_LUMA_CODING_BLOCK_SIZE_MINUS3", msg->log2_min_pcm_luma_coding_block_size_minus3, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "LOG2_DIFF_MAX_MIN_PCM_LUMA_CODING_BLOCK_SIZE", msg->log2_diff_max_min_pcm_luma_coding_block_size, 8, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "NUM_EXTRA_SLICE_HEADER_BITS", msg->num_extra_slice_header_bits, 8, 10, pOut, pBuf);

	add_field_1(asic, tvmid, addr, &offset, "NUM_SHORT_TERM_REF_PIC_SETS", msg->num_short_term_ref_pic_sets, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "NUM_LONG_TERM_REF_PIC_SPS", msg->num_long_term_ref_pic_sps, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "NUM_REF_IDX_L0_DEFAULT_ACTIVE_MINUS1", msg->num_ref_idx_l0_default_active_minus1, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "NUM_REF_IDX_L1_DEFAULT_ACTIVE_MINUS1", msg->num_ref_idx_l1_default_active_minus1, 8, 10, pOut, pBuf);

	add_field_1(asic, tvmid, addr, &offset, "PPS_CB_QP_OFFSET", msg->pps_cb_qp_offset, 8, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "PPS_CR_QP_OFFSET", msg->pps_cr_qp_offset, 8, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "PPS_BETA_OFFSET_DIV2", msg->pps_beta_offset_div2, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "PPS_TC_OFFSET_DIV2", msg->pps_tc_offset_div2, 8, 10, pOut, pBuf);

	add_field_1(asic, tvmid, addr, &offset, "DIFF_CU_QP_DELTA_DEPTH", msg->diff_cu_qp_delta_depth, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "NUM_TILE_COLUMNS_MINUS1", msg->num_tile_columns_minus1, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "NUM_TILE_ROWS_MINUS1", msg->num_tile_rows_minus1, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "LOG2_PARALLEL_MERGE_LEVEL_MINUS2", msg->log2_parallel_merge_level_minus2, 8, 10, pOut, pBuf);

	for (i = 0; i < 19; i++)
		add_field_3(asic, tvmid, addr, &offset, "COLUMN_WIDTH_MINUS1", i+1, msg->column_width_minus1[i], 16, 10, pOut, pBuf);
	for (i = 0; i < 21; i++)
		add_field_3(asic, tvmid, addr, &offset, "ROW_HEIGHT_MINUS1", i+1, msg->row_height_minus1[i], 16, 10, pOut, pBuf);

	add_field_1(asic, tvmid, addr, &offset, "INIT_QT_MINUS26", msg->init_qp_minus26, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "NUM_DELTA_POCS_REF_RPS_IDX", msg->num_delta_pocs_ref_rps_idx, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "CURR_IDX", msg->curr_idx, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "RESERVED", msg->reserved[0], 8, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "CURR_POC", msg->curr_poc, 32, 10, pOut, pBuf);

	for (i = 0; i < 16; i++)
		add_field_3(asic, tvmid, addr, &offset, "REF_PIC_LIST", i+1, msg->ref_pic_list[i], 8, 16, pOut, pBuf);
	for (i = 0; i < 16; i++)
		add_field_3(asic, tvmid, addr, &offset, "POC_LIST", i+1, msg->poc_list[i], 32, 16, pOut, pBuf);
	for (i = 0; i < 8; i++)
		add_field_3(asic, tvmid, addr, &offset, "REF_PIC_SET_ST_CURR_BEFORE", i+1, msg->ref_pic_set_st_curr_before[i], 8, 16, pOut, pBuf);
	for (i = 0; i < 8; i++)
		add_field_3(asic, tvmid, addr, &offset, "REF_PIC_SET_ST_CURR_AFTER", i+1, msg->ref_pic_set_st_curr_after[i], 8, 16, pOut, pBuf);
	for (i = 0; i < 8; i++)
		add_field_3(asic, tvmid, addr, &offset, "REF_PIC_SET_LT_CURR", i+1, msg->ref_pic_set_lt_curr[i], 8, 16, pOut, pBuf);
	for (i = 0; i < 6; i++)
		add_field_3(asic, tvmid, addr, &offset, "ucScalingListDCCoefSizeID2", i+1, msg->ucScalingListDCCoefSizeID2[i], 8, 10, pOut, pBuf);
	for (i = 0; i < 2; i++)
		add_field_3(asic, tvmid, addr, &offset, "ucScalingListDCCoefSizeID3", i+1, msg->ucScalingListDCCoefSizeID3[i], 8, 10, pOut, pBuf);

	add_field_1(asic, tvmid, addr, &offset, "HIGHEST_TID", msg->highestTid, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "IS_NON_REF", msg->isNonRef, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "P010_MODE", msg->p010_mode, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "MSB_MODE", msg->msb_mode, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "LUMA_10TO8", msg->luma_10to8, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "CHROMA_10TO8", msg->chroma_10to8, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "RESERVED", msg->hevc_reserved[0], 8, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "RESERVED", msg->hevc_reserved[1], 8, 16, pOut, pBuf);

	for (j = 0; j < 2; j++)
		for (i = 0; i < 15; i++)
			add_field_3_i(asic, tvmid, addr, &offset, "direct_reflist", j+1, i+1, msg->direct_reflist[j][i], 8, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "padding", msg->padding[0], 8, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "padding", msg->padding[1], 8, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "ST_RPS_BITS", msg->st_rps_bits, 32, 10, pOut, pBuf);
	for (i = 0; i < 16; i++)
		add_field_3(asic, tvmid, addr, &offset, "RESERVED", i+1, msg->reserved_1[i], 8, 16, pOut, pBuf);

	add_tail(tvmid, addr, o_offset, "HEVC/H265 MESSAGE",  pOut, pBuf);
	COMPARE_ERROR((offset - o_offset), size);
}

static void print_header_msg(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset,
			     uint32_t size, rvcn_dec_message_header_t *mh, FILE *pOut, char ***pBuf)
{
	rvcn_dec_message_index_t *pi = &mh->index[0];
	uint32_t o_offset = offset;
	uint32_t i;

	add_header(tvmid, addr, 0, size/4, "HEADER MESSAGE",  pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, STR_HEADER_MESSAGE "HEADER_SIZE", mh->header_size, 32, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "TOTAL_SIZE", mh->total_size, 32, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "NUM_BUFFERS", mh->num_buffers, 32, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "MSG_TYPE: 0 CREATE, 1 DECODE, 2 DESTROY", mh->msg_type, 32, 0, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "STREAM_HANDLE", mh->stream_handle, 32, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "STATUS_REPORT_FEEDBACK_NUMBER", mh->status_report_feedback_number, 32, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "MESSAGE[1]:ID", mh->index[0].message_id, 32, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "MESSAGE[1]:OFFSET", mh->index[0].offset, 32, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "MESSAGE[1]:SIZE", mh->index[0].size, 32, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "MESSAGE[1]:FILLED", mh->index[0].filled, 32, 0, pOut, pBuf);
	pi++; /* bypass 0 as it has printed above */
	for (i = 1; i < mh->num_buffers; i++, pi++) {
		add_field_2(asic, tvmid, addr, &offset, i+1, "ID", pi->message_id, 16, pOut, pBuf);
		add_field_2(asic, tvmid, addr, &offset, i+1, "OFFSET", pi->offset, 16, pOut, pBuf);
		add_field_2(asic, tvmid, addr, &offset, i+1, "SIZE", pi->size, 16, pOut, pBuf);
		add_field_2(asic, tvmid, addr, &offset, i+1, "FILLED", pi->filled, 0, pOut, pBuf);
	}
	add_tail(tvmid, addr, o_offset, "HEADER MESSAGE",  pOut, pBuf);
	COMPARE_ERROR((offset - o_offset), size);
}

static void print_create_msg(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset,
			     uint32_t size, rvcn_dec_message_create_t *pm, FILE *pOut, char ***pBuf)
{
	uint32_t o_offset = offset;

	add_header(tvmid, addr, o_offset, size/4, "CREATE MESSAGE",  pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, STR_CREATE_MESSAGE "STREAM_TYPE", pm->stream_type, 32, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "SESSION_FLAGS", pm->session_flags, 32, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "WIDTH_IN_SAMPLES", pm->width_in_samples, 32, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "HEIGHT_IN_SAMPLE", pm->height_in_samples, 32, 10, pOut, pBuf);
	add_tail(tvmid, addr, o_offset, "CREATE MESSAGE",  pOut, pBuf);
	COMPARE_ERROR((offset - o_offset), size);
}

static void print_decode_msg(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset,
			     uint32_t size, rvcn_dec_message_decode_t *pm, FILE *pOut, char ***pBuf)
{
	uint32_t o_offset = offset;
	int32_t i;

	add_header(tvmid, addr, o_offset, size/4, "DECODE MESSAGE",  pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, STR_DECODE_MESSAGE "STREAM_TYPE", pm->stream_type, 32, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "DECODE_FLAGS", pm->decode_flags, 32, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "WIDTH_IN_SAMPLES", pm->width_in_samples, 32, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "HEIGHT_IN_SAMPLE", pm->height_in_samples, 32, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "BSD_SIZE", pm->bsd_size, 32, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "DPB_SIZE", pm->dpb_size, 32, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "DT_SIZE", pm->dt_size, 32, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "SCT_SIZE", pm->sct_size, 32, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "SC_COEFF_SIZE", pm->sc_coeff_size, 32, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "HW_CTXT_SIZE", pm->hw_ctxt_size, 32, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "SW_CTXT_SIZE", pm->sw_ctxt_size, 32, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "PIC_PARAM_SIZE", pm->pic_param_size, 32, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "MB_CNTL_SIZE", pm->mb_cntl_size, 32, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "RESERVED", pm->reserved0[0], 32, 0, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "RESERVED", pm->reserved0[1], 32, 0, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "RESERVED", pm->reserved0[2], 32, 0, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "RESERVED", pm->reserved0[3], 32, 0, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "DECODE_BUFFER_FLAGS", pm->decode_buffer_flags, 32, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "DB_PITCH", pm->db_pitch, 32, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "DB_ALIGNED_HEIGHT", pm->db_aligned_height, 32, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "DB_TILING_MODE", pm->db_tiling_mode, 32, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "DB_SWIZZLE_MODE", pm->db_swizzle_mode, 32, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "DB_ARRAY_MODE", pm->db_array_mode, 32, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "DB_FIELD_MODE", pm->db_field_mode, 32, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "DB_SURF_TILE_CONFIG", pm->db_surf_tile_config, 32, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "DT_PITCH", pm->dt_pitch, 32, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "DT_UV_PITCH", pm->dt_uv_pitch, 32, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "DT_TILING_MODE", pm->dt_tiling_mode, 32, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "DT_SWIZZLE_MODE", pm->dt_swizzle_mode, 32, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "DT_ARRAY_MODE", pm->dt_array_mode, 32, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "DT_FIELD_MODE", pm->dt_field_mode, 32, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "DT_OUT_FORMAT", pm->dt_out_format, 32, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "DT_SURF_TILE_CONFIG", pm->dt_surf_tile_config, 32, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "DT_UV_SURF_TILE_CONFIG", pm->dt_uv_surf_tile_config, 32, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "DT_LUMA_TOP_OFFSET", pm->dt_luma_top_offset, 32, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "DT_LUMA_BOTTOM_OFFSET", pm->dt_luma_bottom_offset, 32, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "DT_CHROMA_TOP_OFFSET", pm->dt_chroma_top_offset, 32, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "DT_CHROMA_BOTTOM_OFFSET", pm->dt_chroma_bottom_offset, 32, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "DT_CHROMAV_TOP_OFFSET", pm->dt_chromaV_top_offset, 32, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "DT_CHROMAV_BOTTOM_OFFSET", pm->dt_chromaV_bottom_offset, 32, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "MIF_WRC_EN", pm->mif_wrc_en, 32, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "DB_PITCH_UV", pm->db_pitch_uv, 32, 10, pOut, pBuf);

	for (i = 0; i < 5; i++)
		add_field_1(asic, tvmid, addr, &offset, "RESERVED", pm->reserved1[i], 32, 0, pOut, pBuf);

	add_tail(tvmid, addr, o_offset, "DECODE MESSAGE",  pOut, pBuf);
	COMPARE_ERROR((offset - o_offset), size);
}

static void print_vp9_msg(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset,
			  uint32_t size, rvcn_dec_message_vp9_t *pm, FILE *pOut, char ***pBuf)
{
	uint32_t o_offset = offset;
	int32_t i, j, k;

	add_header(tvmid, addr, o_offset, size/4, "VP9 MESSAGE",  pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, STR_VP9_MESSAGE "FRAME_HEADER_FLAGS", pm->frame_header_flags, 32, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "FRAME_CONTEXT_IDX", pm->frame_context_idx, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "RESET_FRAME_CONTEXT", pm->reset_frame_context, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "CURR_PIC_IDX", pm->curr_pic_idx, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "INTERP_FILTER", pm->interp_filter, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "FILTER_LEVEL", pm->filter_level, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "SHARPNESS_LEVEL", pm->sharpness_level, 8, 10, pOut, pBuf);

	for (i = 0; i < 8; i++)
		for (j = 0; j < 4; j++)
			for (k = 0; k < 2; k++)
				add_field_1(asic, tvmid, addr, &offset, "LF_ADJ_LEVEL", pm->lf_adj_level[i][j][k], 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "BASE_QINDEX", pm->base_qindex, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "Y_DC_DELTA_Q", pm->y_dc_delta_q, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "UV_AC_DELTA_Q", pm->uv_ac_delta_q, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "UV_DC_DELTA_Q", pm->uv_dc_delta_q, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "LOG2_TILE_COLS", pm->log2_tile_cols, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "LOG2_TILE_ROWS", pm->log2_tile_rows, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "TX_MODE", pm->tx_mode, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "REFERENCE_MODE", pm->reference_mode, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "CHROMA_FORMAT", pm->chroma_format, 8, 16, pOut, pBuf);

	for (i = 0; i < 8; i++)
		add_field_3(asic, tvmid, addr, &offset, "REF_FRAME_MAP", i+1, pm->ref_frame_map[i], 8, 10, pOut, pBuf);
	for (i = 0; i < 3; i++)
		add_field_3(asic, tvmid, addr, &offset, "FRAME_REFS", i+1, pm->frame_refs[i], 8, 10, pOut, pBuf);
	for (i = 0; i < 3; i++)
		add_field_3(asic, tvmid, addr, &offset, "REF_FRAME_SIGN_BIAS", i+1, pm->ref_frame_sign_bias[i], 8, 10, pOut, pBuf);

	add_field_1(asic, tvmid, addr, &offset, "FRAME_TO_SHOW", pm->frame_to_show, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "BIT_DEPTH_LUMA_MINUS8", pm->bit_depth_luma_minus8, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "BIT_DEPTH_CHROMA_MINUS8", pm->bit_depth_chroma_minus8, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "P010_MODE", pm->p010_mode, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "MSB_MODE", pm->msb_mode, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "LUMA_10TO8", pm->luma_10to8, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "CHROMA_10TO8", pm->chroma_10to8, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "VP9_FRAME_SIZE", pm->vp9_frame_size, 32, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "COMPRESSED_HEADER_SIZE", pm->compressed_header_size, 32, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "UNCOMPRESSED_HEADER_SIZE", pm->uncompressed_header_size, 32, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "RESERVED", pm->reserved[0], 8, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "RESERVED", pm->reserved[1], 8, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "RESERVED", pm->reserved[3], 8, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "RESERVED", pm->reserved[4], 8, 16, pOut, pBuf);
	add_tail(tvmid, addr, o_offset, "VP9 MESSAGE",  pOut, pBuf);
	COMPARE_ERROR((offset - o_offset), size);
}

static void print_vc1_msg(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset,
			  uint32_t size, rvcn_dec_message_vc1_t *pm, FILE *pOut, char ***pBuf)
{
	uint32_t o_offset = offset;

	add_header(tvmid, addr, o_offset, size/4, "VC1 MESSAGE",  pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, STR_VC1_MESSAGE "profile", pm->profile, 32, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "level", pm->level, 32, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "sps_info_flags", pm->sps_info_flags, 32, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "pps_info_flags", pm->pps_info_flags, 32, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "pic_structure", pm->chroma_format, 32, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "chroma_format", pm->chroma_format, 32, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "decoded_pic_idx", pm->decoded_pic_idx, 16, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "deblocked_pic_idx", pm->deblocked_pic_idx, 16, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "forward_ref_idx", pm->forward_ref_idx, 16, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "backward_ref_idx", pm->backward_ref_idx, 16, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "cached_frame_flag", pm->cached_frame_flag, 32, 16, pOut, pBuf);
	add_tail(tvmid, addr, o_offset, "VC1 MESSAGE",  pOut, pBuf);
	COMPARE_ERROR((offset - o_offset), size);
}

#if SUPPORT
static void print_wmv_msg(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset,
			  uint32_t size, uvd_message_wmv_t *pm, FILE *pOut, char ***pBuf)
{
	fprintf(pOut, "\n==wmv message==\n");
	fprintf(pOut, "idct_coef_scramble_enable = 0x%x\n", pm->idct_coef_scramble_enable);
	fprintf(pOut, "idct_coef_scramble_channel = 0x%x\n", pm->idct_coef_scramble_channel);
	fprintf(pOut, "reserved_16bit_2 = 0x%x\n", pm->reserved_16bit_2);
}
#endif

#if SUPPORT
static void print_mpeg2_idct_msg(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset,
				 uint32_t size, uvd_message_mpeg2_idct_t *pm, FILE *pOut, char ***pBuf)
{
	fprintf(pOut, "\n==mpeg2 idct message==\n");
	fprintf(pOut, "idct_coef_scramble_enable = 0x%x\n", pm->idct_coef_scramble_enable);
	fprintf(pOut, "idct_coef_scramble_channel = 0x%x\n", pm->idct_coef_scramble_channel);
	fprintf(pOut, "reserved_16bit_2 = 0x%x\n", pm->reserved_16bit_2);
}
#endif

static void print_mpeg2_vld_msg(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset,
				uint32_t size, rvcn_dec_message_mpeg2_vld_t *pm, FILE *pOut, char ***pBuf)
{
	uint32_t o_offset = offset;
	int32_t i;

	add_header(tvmid, addr, o_offset, size/4, "MPEG2 VLD MESSAGE",  pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, STR_MPEG2_VLD_MESSAGE "decoded_pic_idx", pm->decoded_pic_idx, 32, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "forward_ref_pic_idx", pm->forward_ref_pic_idx, 32, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "backward_ref_pic_idx", pm->backward_ref_pic_idx, 32, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "load_intra_quantiser_matrix", pm->load_intra_quantiser_matrix, 8, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "load_nonintra_quantiser_matrix", pm->load_nonintra_quantiser_matrix, 8, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "reserved_quantiser_alignement[0]", pm->reserved_quantiser_alignement[0], 8, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "reserved_quantiser_alignement[1]", pm->reserved_quantiser_alignement[1], 8, 16, pOut, pBuf);
	for ( i = 0; i < 64; i++)
		add_field_3(asic, tvmid, addr, &offset, "intra_quantiser_matrix", i+1, pm->intra_quantiser_matrix[i], 8, 16, pOut, pBuf);
	for ( i = 0; i < 64; i++)
		add_field_3(asic, tvmid, addr, &offset, "nonintra_quantiser_matrix", i+1, pm->nonintra_quantiser_matrix[i], 8, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "profile_and_level_indication", pm->profile_and_level_indication, 8, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "chroma_format", pm->chroma_format, 8, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "picture_coding_type", pm->picture_coding_type, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "reserved_1", pm->reserved_1, 8, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "f_code [0][0]", pm->f_code[0][0], 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "f_code [0][1]", pm->f_code[0][1], 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "f_code [1][0]", pm->f_code[1][0], 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "f_code [1][1]", pm->f_code[1][1], 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "intra_dc_precision", pm->intra_dc_precision, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "pic_structure", pm->pic_structure, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "top_field_first", pm->top_field_first, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "frame_pred_frame_dct", pm->frame_pred_frame_dct, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "concealment_motion_vectors", pm->concealment_motion_vectors, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "q_scale_type", pm->q_scale_type, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "intra_vlc_format", pm->intra_vlc_format, 8, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "alternate_scan", pm->alternate_scan, 8, 10, pOut, pBuf);
	add_tail(tvmid, addr, o_offset, "MPEG2 VLD MESSAGE",  pOut, pBuf);
	COMPARE_ERROR((offset - o_offset), size);
}

static void print_mpeg4_asp_vld_msg(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset,
				    uint32_t size, rvcn_dec_message_mpeg4_asp_vld_t *pm, FILE *pOut, char ***pBuf)
{
	uint32_t *bit_field = (uint32_t *)pm;
	uint8_t *byte_field = (uint8_t *)pm;
	uint32_t o_offset = offset;
	int32_t i;

	add_header(tvmid, addr, o_offset, size/4, "MPEG4 ASP VLD MESSAGE",  pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, STR_MPEG4_ASP_VLD_MESSAGE "decoded_pic_idx", pm->decoded_pic_idx, 32, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "forward_ref_pic_idx", pm->forward_ref_pic_idx, 32, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "backward_ref_pic_idx", pm->backward_ref_pic_idx, 32, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "variant_type", pm->variant_type, 32, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "profile_and_level_indication", pm->profile_and_level_indication, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "video_object_layer_verid", pm->video_object_layer_verid, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "video_object_layer_shape", pm->video_object_layer_shape, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "reserved_1", pm->reserved_1, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "video_object_layer_width", pm->video_object_layer_width, 16, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "video_object_layer_height", pm->video_object_layer_height, 16, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "vop_time_increment_resolution", pm->vop_time_increment_resolution, 16, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "reserved_2", pm->reserved_2, 16, 0, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "BIT_FIELD",  *(bit_field + (offset - o_offset) / 4), 32, 16, pOut, pBuf); //bit map
	add_field_bit(asic, offset - 4, "short_video_header",  pm->short_video_header, "0:0", 16, pOut, pBuf); //bit map
	add_field_bit(asic, offset - 4, "obmc_disable",  pm->obmc_disable, "1:1", 16, pOut, pBuf); //bit map
	add_field_bit(asic, offset - 4, "interlaced",  pm->interlaced, "2:2", 16, pOut, pBuf); //bit map
	add_field_bit(asic, offset - 4, "load_intra_quant_mat",  pm->load_intra_quant_mat, "3:3", 16, pOut, pBuf); //bit map
	add_field_bit(asic, offset - 4, "load_nonintra_quant_mat",  pm->load_nonintra_quant_mat, "4:4", 16, pOut, pBuf); //bit map
	add_field_bit(asic, offset - 4, "quarter_sample",  pm->quarter_sample, "5:5", 16, pOut, pBuf); //bit map
	add_field_bit(asic, offset - 4, "complexity_estimation_disable",  pm->complexity_estimation_disable, "6:6", 16, pOut, pBuf); //bit map
	add_field_bit(asic, offset - 4, "resync_marker_disable",  pm->resync_marker_disable, "7:7", 16, pOut, pBuf); //bit map
	add_field_bit(asic, offset - 4, "data_partitioned",  pm->data_partitioned, "8:8", 16, pOut, pBuf); //bit map
	add_field_bit(asic, offset - 4, "reversible_vlc",  pm->reversible_vlc, "9:9", 16, pOut, pBuf); //bit map
	add_field_bit(asic, offset - 4, "newpred_enable",  pm->newpred_enable, "10:10", 16, pOut, pBuf); //bit map
	add_field_bit(asic, offset - 4, "reduced_resolution_vop_enable",  pm->reduced_resolution_vop_enable, "11:11", 16, pOut, pBuf); //bit map
	add_field_bit(asic, offset - 4, "scalability",  pm->scalability, "12:12", 16, pOut, pBuf); //bit map
	add_field_bit(asic, offset - 4, "is_object_layer_identifier",  pm->is_object_layer_identifier, "13:13", 16, pOut, pBuf); //bit map
	add_field_bit(asic, offset - 4, "fixed_vop_rate",  pm->fixed_vop_rate, "14:14", 16, pOut, pBuf); //bit map
	add_field_bit(asic, offset - 4, "newpred_segment_type",  pm->newpred_segment_type, "15:15", 16, pOut, pBuf); //bit map
	add_field_bit(asic, offset - 4, "reserved_bits",  pm->reserved_bits, "31:16", 0, pOut, pBuf); //bit map
	add_field_1(asic, tvmid, addr, &offset, "quant_type", pm->quant_type, 8, 10, pOut, pBuf);
	for ( i = 0; i < 3; i++)
		add_field_3(asic, tvmid, addr, &offset, "reserved_3", i+1, pm->reserved_3[i], 8, 16, pOut, pBuf);
	for ( i = 0; i < 64; i++)
		add_field_3(asic, tvmid, addr, &offset, "intra_quant_mat", i+1, pm->intra_quant_mat[i], 8, 16, pOut, pBuf);
	for ( i = 0; i < 64; i++)
		add_field_3(asic, tvmid, addr, &offset, "nonintra_quant_mat", i+1, pm->nonintra_quant_mat[i], 8, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "sprite_enable", pm->sprite_config.sprite_enable, 8, 10, pOut, pBuf);
	for ( i = 0; i < 3; i++)
		add_field_3(asic, tvmid, addr, &offset, "reserved_4", i+1, pm->sprite_config.reserved_4[i], 8, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "sprite_width", pm->sprite_config.sprite_width, 16, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "sprite_height", pm->sprite_config.sprite_height, 16, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "sprite_left_coordinate", pm->sprite_config.sprite_left_coordinate, 16, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "sprite_top_coordinate", pm->sprite_config.sprite_top_coordinate, 16, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "no_of_sprite_warping_points", pm->sprite_config.no_of_sprite_warping_points, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "sprite_warping_accuracy", pm->sprite_config.sprite_warping_accuracy, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "sprite_brightness_change", pm->sprite_config.sprite_brightness_change, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "low_latency_sprite_enable", pm->sprite_config.low_latency_sprite_enable, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "BIT_FIELD divx_311_config", *(bit_field + (offset - o_offset) / 4), 32, 16, pOut, pBuf);
	add_field_bit(asic, offset - 4, "check_skip",  pm->divx_311_config.check_skip, "0:0", 16, pOut, pBuf);
	add_field_bit(asic, offset - 4, "switch_rounding",  pm->divx_311_config.switch_rounding, "1:1", 16, pOut, pBuf);
	add_field_bit(asic, offset - 4, "t311",  pm->divx_311_config.t311, "2:2", 16, pOut, pBuf);
	add_field_bit(asic, offset - 4, "reserved_bits",  pm->divx_311_config.reserved_bits, "31:3", 0, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "vol_mode", pm->divx_311_config.vol_mode, 8, 10, pOut, pBuf);
	for ( i = 0; i < 3; i++)
		add_field_3(asic, tvmid, addr, &offset, "reserved_5", i+1, pm->divx_311_config.reserved_5[i], 8, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "vop_data_present", pm->vop.vop_data_present, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "vop_coding_type", pm->vop.vop_coding_type, 8, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "vop_quant", pm->vop.vop_quant, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "vop_coded", pm->vop.vop_coded, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "vop_rounding_type", pm->vop.vop_rounding_type, 8, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "intra_dc_vlc_thr", pm->vop.intra_dc_vlc_thr, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "top_field_first", pm->vop.top_field_first, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "alternate_vertical_scan_flag", pm->vop.alternate_vertical_scan_flag, 8, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "vop_fcode_forward", pm->vop.vop_fcode_forward, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "vop_fcode_backward", pm->vop.vop_fcode_backward, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "reserved", *(byte_field + (offset - o_offset)), 8, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "reserved", *(byte_field + (offset - o_offset)), 8, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "TRB 1", pm->vop.TRB[0], 32, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "TRB 2", pm->vop.TRB[1], 32, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "TRD 1", pm->vop.TRD[0], 32, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "TRD 2", pm->vop.TRD[1], 32, 16, pOut, pBuf);

	add_tail(tvmid, addr, o_offset, "MPEG4 ASP VLD MESSAGE",  pOut, pBuf);
	COMPARE_ERROR((offset - o_offset), size);
}

#if SUPPORT
static void print_scaler_msg(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset,
			     uint32_t size, uvd_message_scaler_t *pm, FILE *pOut, char ***pBuf)
{
	fprintf(pOut, "\n==scaler message==\n");
	fprintf(pOut, "scaler_buffer = 0x%x\n", pm->scaler_buffer);
	fprintf(pOut, "scaler_pitch = 0x%x\n", pm->scaler_pitch);
	fprintf(pOut, "scaler_pitch_uv = 0x%x\n", pm->scaler_pitch_uv);
	fprintf(pOut, "scaler_tiling_mode = 0x%x\n", pm->scaler_tiling_mode);
	fprintf(pOut, "scaler_array_mode = 0x%x\n", pm->scaler_array_mode);
	fprintf(pOut, "scaler_field_mode = 0x%x\n", pm->scaler_field_mode);
	fprintf(pOut, "scaler_luma_top_offset = 0x%x\n", pm->scaler_luma_top_offset);
	fprintf(pOut, "scaler_luma_bottom_offset = 0x%x\n", pm->scaler_luma_bottom_offset);
	fprintf(pOut, "scaler_chroma_top_offset = 0x%x\n", pm->scaler_chroma_top_offset);
	fprintf(pOut, "scaler_chroma_bottom_offset = 0x%x\n", pm->scaler_chroma_bottom_offset);
	fprintf(pOut, "scaler_surf_tile_config = 0x%x\n", pm->scaler_surf_tile_config);
	fprintf(pOut, "scaler_dbw_drop = 0x%x\n", pm->scaler_dbw_drop);
	fprintf(pOut, "scaler_enable = 0x%x\n", pm->scaler_enable);
	fprintf(pOut, "scaler_coef_unlock = 0x%x\n", pm->scaler_coef_unlock);
	fprintf(pOut, "scaler_drst_coef_dis = 0x%x\n", pm->scaler_drst_coef_dis);
	fprintf(pOut, "scaler_luma_horz_coef0 = 0x%x\n", pm->scaler_luma_horz_coef0);
	fprintf(pOut, "scaler_luma_horz_coef1 = 0x%x\n", pm->scaler_luma_horz_coef1);
	fprintf(pOut, "scaler_luma_horz_coef2 = 0x%x\n", pm->scaler_luma_horz_coef2);
	fprintf(pOut, "scaler_luma_horz_coef3 = 0x%x\n", pm->scaler_luma_horz_coef3);
	fprintf(pOut, "scaler_luma_vert_coef0 = 0x%x\n", pm->scaler_luma_vert_coef0);
	fprintf(pOut, "scaler_luma_vert_coef1 = 0x%x\n", pm->scaler_luma_vert_coef1);
	fprintf(pOut, "scaler_luma_vert_coef2 = 0x%x\n", pm->scaler_luma_vert_coef2);
	fprintf(pOut, "scaler_luma_vert_coef3 = 0x%x\n", pm->scaler_luma_vert_coef3);
	fprintf(pOut, "scaler_chroma_horz_coef0 = 0x%x\n", pm->scaler_chroma_horz_coef0);
	fprintf(pOut, "scaler_chroma_horz_coef1 = 0x%x\n", pm->scaler_chroma_horz_coef1);
	fprintf(pOut, "scaler_chroma_horz_coef2 = 0x%x\n", pm->scaler_chroma_horz_coef2);
	fprintf(pOut, "scaler_chroma_horz_coef3 = 0x%x\n", pm->scaler_chroma_horz_coef3);
	fprintf(pOut, "scaler_chroma_vert_coef0 = 0x%x\n", pm->scaler_chroma_vert_coef0);
	fprintf(pOut, "scaler_chroma_vert_coef1 = 0x%x\n", pm->scaler_chroma_vert_coef1);
	fprintf(pOut, "scaler_chroma_vert_coef2 = 0x%x\n", pm->scaler_chroma_vert_coef2);
	fprintf(pOut, "scaler_chroma_vert_coef3 = 0x%x\n", pm->scaler_chroma_vert_coef3);
	fprintf(pOut, "scaler_swizzle_mode = 0x%x\n", pm->scaler_swizzle_mode);
	fprintf(pOut, "scaler_left_luma_top_offset = 0x%x\n", pm->scaler_left_luma_top_offset);
	fprintf(pOut, "scaler_left_luma_bottom_offset = 0x%x\n", pm->scaler_left_luma_bottom_offset);
	fprintf(pOut, "scaler_left_chroma_top_offset = 0x%x\n", pm->scaler_left_chroma_top_offset);
	fprintf(pOut, "scaler_left_chroma_bottom_offset = 0x%x\n", pm->scaler_left_chroma_bottom_offset);
	fprintf(pOut, "scaler_v_init = 0x%x\n", pm->scaler_v_init);
	fprintf(pOut, "scaler_v_inc = 0x%x\n", pm->scaler_v_inc);
	fprintf(pOut, "scaler_h_init = 0x%x\n", pm->scaler_h_init);
	fprintf(pOut, "scaler_h_inc = 0x%x\n", pm->scaler_h_inc);
	fprintf(pOut, "scaler_p010_mode = 0x%x\n", pm->scaler_p010_mode);
	fprintf(pOut, "scaler_msb_mode = 0x%x\n", pm->scaler_msb_mode);
	fprintf(pOut, "scaler_luma_10to8 = 0x%x\n", pm->scaler_luma_10to8);
	fprintf(pOut, "scaler_chroma_10to8 = 0x%x\n", pm->scaler_chroma_10to8);
	fprintf(pOut, "scaler_surf_uv_tile_config = 0x%x\n", pm->scaler_surf_uv_tile_config);
}
#endif

static void print_file_grain_msg(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset,
				 uint32_t size, rvcn_dec_film_grain_params_t *pm, FILE *pOut, char ***pBuf)
{
	uint32_t o_offset = offset;
	int32_t i;

	add_field_1(asic, tvmid, addr, &offset, "apply_grain", pm->apply_grain, 8, 10, pOut, pBuf);
	for (i = 0; i < 14; i++) {
		add_field_3(asic, tvmid, addr, &offset, "scaling_points_y", i+1, pm->scaling_points_y[i][0], 8, 10, pOut, pBuf);
		add_field_3(asic, tvmid, addr, &offset, "scaling_points_y", i+1, pm->scaling_points_y[i][1], 8, 10, pOut, pBuf);
	}
	add_field_1(asic, tvmid, addr, &offset, "num_y_points", pm->num_y_points, 8, 10, pOut, pBuf);
	for (i = 0; i < 10; i++) {
		add_field_3(asic, tvmid, addr, &offset, "scaling_points_cb", i+1, pm->scaling_points_cb[i][0], 8, 10, pOut, pBuf);
		add_field_3(asic, tvmid, addr, &offset, "scaling_points_cb", i+1, pm->scaling_points_cb[i][1], 8, 10, pOut, pBuf);
	}
	add_field_1(asic, tvmid, addr, &offset, "num_cb_points", pm->num_cb_points, 8, 10, pOut, pBuf);
	for (i = 0; i < 10; i++) {
		add_field_3(asic, tvmid, addr, &offset, "scaling_points_cr", i+1, pm->scaling_points_cr[i][0], 8, 10, pOut, pBuf);
		add_field_3(asic, tvmid, addr, &offset, "scaling_points_cr", i+1, pm->scaling_points_cr[i][1], 8, 10, pOut, pBuf);
	}
	add_field_1(asic, tvmid, addr, &offset, "num_cr_points", pm->num_cr_points, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "scaling_shift", pm->scaling_shift, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "ar_coeff_lag", pm->ar_coeff_lag, 8, 16, pOut, pBuf);
	for (i = 0; i < 24; i++)
		add_field_3(asic, tvmid, addr, &offset, "ar_coeffs_y", i+1, pm->ar_coeffs_y[i], 8, 10, pOut, pBuf);
	for (i = 0; i < 25; i++)
		add_field_3(asic, tvmid, addr, &offset, "ar_coeffs_cb", i+1, pm->ar_coeffs_cb[i], 8, 10, pOut, pBuf);
	for (i = 0; i < 25; i++)
		add_field_3(asic, tvmid, addr, &offset, "ar_coeffs_cr", i+1, pm->ar_coeffs_cr[i], 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "ar_coeff_shift", pm->ar_coeff_shift, 8, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "cb_mult", pm->cb_mult, 8, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "cb_luma_mult", pm->cb_luma_mult, 8, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "padding", pm->padding1, 8, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "cb_offset", pm->cb_offset, 16, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "cr_mult", pm->cr_mult, 8, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "cr_luma_mult", pm->cr_luma_mult, 8, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "cr_offset", pm->cr_offset, 16, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "overlap_flag", pm->overlap_flag, 8, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "clip_to_restricted_range", pm->clip_to_restricted_range, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "bit_depth_minus_8", pm->bit_depth_minus_8, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "chroma_scaling_from_luma", pm->chroma_scaling_from_luma, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "grain_scale_shift", pm->grain_scale_shift, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "padding", pm->padding2, 8, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "random_seed", pm->random_seed, 16, 16, pOut, pBuf);
	COMPARE_ERROR((offset - o_offset), size);
}

static void print_av1_msg(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset,
			  uint32_t size, rvcn_dec_message_av1_t *pm, FILE *pOut, char ***pBuf)
{
	uint32_t o_offset = offset;
	int32_t i = 0, j = 0;

	add_header(tvmid, addr, o_offset, size/4, "AV1 MESSAGE",  pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, STR_AV1_MESSAGE "frame_header_flags", pm->frame_header_flags, 32, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "current_frame_id", pm->current_frame_id, 32, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "frame_offset", pm->frame_offset, 32, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "profile", pm->profile, 8, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "is_annexb", pm->is_annexb, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "frame_type", pm->frame_type, 8, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "primary_ref_frame", pm->primary_ref_frame, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "curr_pic_idx", pm->curr_pic_idx, 8, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "sb_size", pm->sb_size, 8, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "interp_filter", pm->interp_filter, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "filter_level 1", pm->filter_level[0], 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "filter_level 2", pm->filter_level[1], 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "filter_level_u", pm->filter_level_u, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "filter_level_v", pm->filter_level_v, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "sharpness_level", pm->sharpness_level, 8, 10, pOut, pBuf);
	for (i = 0; i < 8; i++)
		add_field_3(asic, tvmid, addr, &offset, "ref_deltas", i+1, pm->ref_deltas[i], 8, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "mode_deltas 1", pm->mode_deltas[0], 8, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "mode_deltas 2", pm->mode_deltas[1], 8, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "base_qindex", pm->base_qindex, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "y_dc_delta_q", pm->y_dc_delta_q, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "u_dc_delta_q", pm->u_dc_delta_q, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "v_dc_delta_q", pm->v_dc_delta_q, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "u_ac_delta_q", pm->u_ac_delta_q, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "v_ac_delta_q", pm->v_ac_delta_q, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "qm_y", pm->qm_y, 8, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "qm_u", pm->qm_u, 8, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "qm_v", pm->qm_v, 8, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "delta_q_res", pm->delta_q_res, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "delta_lf_res", pm->delta_lf_res, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "tile_cols", pm->tile_cols, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "tile_rows", pm->tile_rows, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "tx_mode", pm->tx_mode, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "reference_mode", pm->reference_mode, 8, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "chroma_format", pm->chroma_format, 8, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "padding", pm->padding[0], 8, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "padding", pm->padding[1], 8, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "tile_size_bytes", pm->tile_size_bytes, 32, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "context_update_tile_id", pm->context_update_tile_id, 32, 16, pOut, pBuf);
	for (i = 0; i < 65; i++)
		add_field_3(asic, tvmid, addr, &offset, "tile_col_start_sb", i+1, pm->tile_col_start_sb[i], 32, 10, pOut, pBuf);
	for (i = 0; i < 65; i++)
		add_field_3(asic, tvmid, addr, &offset, "tile_row_start_sb", i+1, pm->tile_row_start_sb[i], 32, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "max_width", pm->max_width, 32, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "max_height", pm->max_height, 32, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "width", pm->width, 32, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "height", pm->height, 32, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "superres_upscaled_width", pm->superres_upscaled_width, 32, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "superres_scale_denominator", pm->superres_scale_denominator, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "order_hint_bits", pm->order_hint_bits, 8, 10, pOut, pBuf);
	for (i = 0; i < 8; i++)
		add_field_3(asic, tvmid, addr, &offset, "ref_frame_map", i+1, pm->ref_frame_map[i], 8, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "padding", pm->padding1[0], 8, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "padding", pm->padding1[1], 8, 16, pOut, pBuf);
	for (i = 0; i < 8; i++)
		add_field_3(asic, tvmid, addr, &offset, "ref_frame_offset", i+1, pm->ref_frame_offset[i], 32, 16, pOut, pBuf);
	for (i = 0; i < 7; i++)
		add_field_3(asic, tvmid, addr, &offset, "frame_refs", i+1, pm->frame_refs[i], 8, 10, pOut, pBuf);
	for (i = 0; i < 7; i++)
		add_field_3(asic, tvmid, addr, &offset, "ref_frame_sign_bias", i+1, pm->ref_frame_sign_bias[i], 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "bit_depth_luma_minus8", pm->bit_depth_luma_minus8, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "bit_depth_chroma_minus8", pm->bit_depth_chroma_minus8, 8, 10, pOut, pBuf);
	for (i = 0; i < 8; i++)
		for (j = 0; j < 8; j++)
			add_field_3_i(asic, tvmid, addr, &offset, "feature_data", i+1, j+1, pm->feature_data[i][j], 32, 16, pOut, pBuf);
	for (i = 0; i < 8; i++)
		add_field_3(asic, tvmid, addr, &offset, "feature_mask", i+1, pm->feature_mask[i], 8, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "cdef_damping", pm->cdef_damping, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "cdef_bits", pm->cdef_bits, 8, 10, pOut, pBuf);
	for (i = 0; i < 16; i++)
		add_field_3(asic, tvmid, addr, &offset, "cdef_strengths", i+1, pm->cdef_strengths[i], 16, 10, pOut, pBuf);
	for (i = 0; i < 16; i++)
		add_field_3(asic, tvmid, addr, &offset, "cdef_uv_strengths", i+1, pm->cdef_uv_strengths[i], 16, 10, pOut, pBuf);
	for (i = 0; i < 3; i++)
		add_field_3(asic, tvmid, addr, &offset, "frame_restoration_type", i+1, pm->frame_restoration_type[i], 8, 16, pOut, pBuf);
	for (i = 0; i < 3; i++)
		add_field_3(asic, tvmid, addr, &offset, "log2_restoration_unit_size_minus5", i+1, pm->log2_restoration_unit_size_minus5[i], 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "p010_mode", pm->p010_mode, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "msb_mode", pm->msb_mode, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "luma_10to8", pm->luma_10to8, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "chroma_10to8", pm->chroma_10to8, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "preskip_segid", pm->preskip_segid, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "last_active_segid", pm->last_active_segid, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "seg_lossless_flag", pm->seg_lossless_flag, 8, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "coded_lossless", pm->coded_lossless, 8, 10, pOut, pBuf);
	print_file_grain_msg(asic, tvmid, addr, offset, sizeof(rvcn_dec_film_grain_params_t), &pm->film_grain, pOut, pBuf);
	offset += sizeof(rvcn_dec_film_grain_params_t);
	add_field_1(asic, tvmid, addr, &offset, "padding", pm->padding2[0], 8, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "padding", pm->padding2[1], 8, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "uncompressed_header_size", pm->uncompressed_header_size, 32, 10, pOut, pBuf);
	for (i = 0; i < 8; i++) {
		rvcn_dec_warped_motion_params_t *gl = &pm->global_motion[i];
		add_field_3(asic, tvmid, addr, &offset, get_wmtype(gl->wmtype), i+1, gl->wmtype, 32, 16, pOut, pBuf);
		for (j = 0; j < 8; j++)
			add_field_3(asic, tvmid, addr, &offset, "wmmat", j+1, gl->wmmat[j], 32, 16, pOut, pBuf);
		add_field_3(asic, tvmid, addr, &offset, "alpha", i+1, gl->alpha, 16, 16, pOut, pBuf);
		add_field_3(asic, tvmid, addr, &offset, "beta", i+1, gl->beta, 16, 16, pOut, pBuf);
		add_field_3(asic, tvmid, addr, &offset, "gamma", i+1, gl->gamma, 16, 16, pOut, pBuf);
		add_field_3(asic, tvmid, addr, &offset, "delta", i+1, gl->delta, 16, 16, pOut, pBuf);
	}
	for (i = 0; i < 256; i++) {
		add_field_3(asic, tvmid, addr, &offset, "offset", i+1, pm->tile_info[i].offset, 32, 16, pOut, pBuf);
		add_field_3(asic, tvmid, addr, &offset, "size", i+1, pm->tile_info[i].size, 32, 10, pOut, pBuf);
	}
	add_field_1(asic, tvmid, addr, &offset, "reserved", pm->reserved[0], 8, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "reserved", pm->reserved[1], 8, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "reserved", pm->reserved[2], 8, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "reserved", pm->reserved[3], 8, 16, pOut, pBuf);
	add_tail(tvmid, addr, o_offset, "AV1 MESSAGE",  pOut, pBuf);
	COMPARE_ERROR((offset - o_offset), size); //aligment issue again
}

static void print_avc_msg(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset,
			  uint32_t size, rvcn_dec_message_avc_t *pm, FILE *pOut, char ***pBuf)
{
	uint32_t o_offset = offset;
	int32_t i, j;
	(void) size;

	add_header(tvmid, addr, o_offset, size/4, "AVC/H264 MESSAGE",  pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, STR_AVC_MESSAGE "profile", pm->profile, 32, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "level", pm->level, 32, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "sps_info_flags", pm->sps_info_flags, 32, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "pps_info_flags", pm->pps_info_flags, 32, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "chroma_format", pm->chroma_format, 8, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "bit_depth_luma_minus8", pm->bit_depth_luma_minus8, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "bit_depth_chroma_minus8", pm->bit_depth_chroma_minus8, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "log2_max_frame_num_minus4", pm->log2_max_frame_num_minus4, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "pic_order_cnt_type", pm->pic_order_cnt_type, 8, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "log2_max_pic_order_cnt_lsb_minus4", pm->log2_max_pic_order_cnt_lsb_minus4, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "num_ref_frames", pm->num_ref_frames, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "reserved_8bit", pm->reserved_8bit, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "pic_init_qp_minus26", pm->pic_init_qp_minus26, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "pic_init_qs_minus26", pm->pic_init_qs_minus26, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "chroma_qp_index_offset", pm->chroma_qp_index_offset, 8, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "second_chroma_qp_index_offset", pm->second_chroma_qp_index_offset, 8, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "num_slice_groups_minus1", pm->num_slice_groups_minus1, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "slice_group_map_type", pm->slice_group_map_type, 8, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "num_ref_idx_l0_active_minus1", pm->num_ref_idx_l0_active_minus1, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "num_ref_idx_l1_active_minus1", pm->num_ref_idx_l1_active_minus1, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "slice_group_change_rate_minus1", pm->slice_group_change_rate_minus1, 16, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "reserved_16bit_1", pm->reserved_16bit_1, 16, 16, pOut, pBuf);

	for (i = 0; i < 6; i++)
		for (j = 0; j < 16; j++)
			add_field_3_i(asic, tvmid, addr, &offset, "scaling_list_4x4", i+1, j+1, pm->scaling_list_4x4[i][j], 8, 16, pOut, pBuf);
	for (i = 0; i < 2; i++)
		for (j = 0; j < 64; j++)
			add_field_3_i(asic, tvmid, addr, &offset, "scaling_list_8x8", i+1, j+1, pm->scaling_list_8x8[i][j], 8, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "frame_num", pm->frame_num, 32, 16, pOut, pBuf);
	for (i = 0; i < 16; i++)
		add_field_3(asic, tvmid, addr, &offset, "frame_num_list", i+1, pm->frame_num_list[i], 32, 16, pOut, pBuf);
	for (i = 0; i < 2; i++)
		add_field_3(asic, tvmid, addr, &offset, "curr_field_order_cnt_list", i+1, pm->curr_field_order_cnt_list[i], 32, 16, pOut, pBuf);
	for (i = 0; i < 16; i++)
		for (j = 0; j < 2; j++)
			add_field_3_i(asic, tvmid, addr, &offset, "field_order_cnt_list", i+1, j+1, pm->field_order_cnt_list[i][j], 32, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "decoded_pic_idx", pm->decoded_pic_idx, 32, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "curr_pic_ref_frame_num", pm->curr_pic_ref_frame_num, 32, 16, pOut, pBuf);
	for (i = 0; i < 16; i++)
		add_field_3(asic, tvmid, addr, &offset, "ref_frame_list", i+1, pm->ref_frame_list[i], 8, 16, pOut, pBuf);
	for (i = 0; i < 122; i++)
		add_field_3(asic, tvmid, addr, &offset, "reserved", i+1, pm->reserved[i], 32, 0, pOut, pBuf);

	add_field_1(asic, tvmid, addr, &offset, "numViews", pm->mvc.numViews, 32, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "viewId0", pm->mvc.viewId0, 32, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "viewOrderIndex", pm->mvc.mvcElements[0].viewOrderIndex, 16, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "viewId", pm->mvc.mvcElements[0].viewId, 16, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "numOfAnchorRefsInL0", pm->mvc.mvcElements[0].numOfAnchorRefsInL0, 16, 16, pOut, pBuf);
	for (i = 0; i < 15; i++)
		add_field_3(asic, tvmid, addr, &offset, "viewIdOfAnchorRefsInL0", i+1, pm->mvc.mvcElements[0].viewIdOfAnchorRefsInL0[i], 16, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "numOfAnchorRefsInL1", pm->mvc.mvcElements[0].numOfAnchorRefsInL1, 16, 16, pOut, pBuf);
	for (i = 0; i < 15; i++)
		add_field_3(asic, tvmid, addr, &offset, "viewIdOfAnchorRefsInL1", i+1, pm->mvc.mvcElements[0].viewIdOfAnchorRefsInL1[i], 16, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "numOfNonAnchorRefsInL0", pm->mvc.mvcElements[0].numOfNonAnchorRefsInL0, 16, 16, pOut, pBuf);
	for (i = 0; i < 15; i++)
		add_field_3(asic, tvmid, addr, &offset, "viewIdOfNonAnchorRefsInL0", i+1, pm->mvc.mvcElements[0].viewIdOfNonAnchorRefsInL0[i], 16, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "numOfNonAnchorRefsInL1", pm->mvc.mvcElements[0].numOfNonAnchorRefsInL1, 16, 16, pOut, pBuf);
	for (i = 0; i < 15; i++)
		add_field_3(asic, tvmid, addr, &offset, "viewIdOfNonAnchorRefsInL1", i+1, pm->mvc.mvcElements[0].viewIdOfNonAnchorRefsInL1[i], 16, 16, pOut, pBuf);

	add_field_1(asic, tvmid, addr, &offset, "non_existing_frame_flags", pm->non_existing_frame_flags, 16, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "padding", pm->padding, 16, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "used_for_reference_flags", pm->used_for_reference_flags, 32, 16, pOut, pBuf);

	add_tail(tvmid, addr, o_offset, "AVC/H264 MESSAGE",  pOut, pBuf);
	COMPARE_ERROR((offset - o_offset), size);
}

typedef struct uvd_message_hevc_direct_ref_list_entry_s
{
	 unsigned char           multi_direct_reflist[2][15];
} uvd_message_hevc_direct_ref_list_entry_t;

static void print_hevc_direct_ref_list_msg(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset,
					   uint32_t size, rvcn_dec_message_hevc_direct_ref_list_t *pm, FILE *pOut, char ***pBuf)
{
	uvd_message_hevc_direct_ref_list_entry_t *entry = (uvd_message_hevc_direct_ref_list_entry_t *)pm->multi_direct_reflist;
	uint32_t num_direct_reflist = pm->num_direct_reflist? pm->num_direct_reflist : 1;
	uint8_t *byte_field = (uint8_t *)pm;
	uint32_t o_offset = offset;
	unsigned int i, j, k;

	add_header(tvmid, addr, o_offset, size/4, "HEVC DIRECT REFERENCE LIST MESSAGE",  pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, STR_HEVC_DRL_MESSAGE "num_direct_reflist", pm->num_direct_reflist, 32, 10, pOut, pBuf);

	for (i = 0; i < num_direct_reflist; i++) {
		for (j = 0; j < 2; j++)
			for (k = 0; k < 15; k++)
				add_field_3_i(asic, tvmid, addr, &offset, "reflist", j+1, k+1, entry->multi_direct_reflist[j][k], 8, 16, pOut, pBuf);
		entry++;
	}
	if (num_direct_reflist % 2 == 1) {
		add_field_1(asic, tvmid, addr, &offset, "padding", *(byte_field + offset), 8, 16, pOut, pBuf);
		add_field_1(asic, tvmid, addr, &offset, "padding", *(byte_field + offset), 8, 16, pOut, pBuf);
	}
	add_tail(tvmid, addr, o_offset, "HEVC DIRECT REFERENCE LIST MESSAGE",  pOut, pBuf);
	COMPARE_ERROR((offset - o_offset), size);
}

static void print_dynamic_dpb_t1_msg(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset,
				     uint32_t size, rvcn_dec_message_dynamic_dpb_t *pm, FILE *pOut, char ***pBuf)
{
	uint32_t o_offset = offset;
	int32_t i;

	add_header(tvmid, addr, o_offset, size/4, "DYNAMIC_DPB_T1 MESSAGE",  pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, STR_DPB_T1_MESSAGE "DPB_CONFIG_FLAGS", pm->dpbConfigFlags, 32, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "dpbLumaPitch", pm->dpbLumaPitch, 32, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "dpbLumaAlignedHeight", pm->dpbLumaAlignedHeight, 32, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "dpbLumaAlignedSize", pm->dpbLumaAlignedSize, 32, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "dpbChromaPitch", pm->dpbChromaPitch, 32, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "dpbChromaAlignedHeight", pm->dpbChromaAlignedHeight, 32, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "dpbChromaAlignedSize", pm->dpbChromaAlignedSize, 32, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "dpbArraySize", pm->dpbArraySize, 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "dpbCurArraySlice", pm->dpbCurArraySlice, 8, 10, pOut, pBuf);
	for( i = 0; i < 16; i++)
		add_field_3(asic, tvmid, addr, &offset, "dpbRefArraySlice", i+1, pm->dpbRefArraySlice[i], 8, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "dpbReserved0", pm->dpbReserved0[0], 8, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "dpbReserved0", pm->dpbReserved0[1], 8, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "dpbCurrOffset", pm->dpbCurrOffset, 32, 16, pOut, pBuf);
	for( i = 0; i < 16; i++)
		add_field_3(asic, tvmid, addr, &offset, "dpbAddrOffset", i+1, pm->dpbAddrOffset[i], 32, 16, pOut, pBuf);

	add_tail(tvmid, addr, o_offset, "DYNAMIC_DPB_T1 MESSAGE",  pOut, pBuf);
	COMPARE_ERROR((offset - o_offset), size);
}

static void print_dynamic_dpb_msg(struct umr_asic *asic, uint32_t tvmid, uint64_t addr, uint32_t offset,
				  uint32_t size, rvcn_dec_message_dynamic_dpb_t2_t *pm, FILE *pOut, char ***pBuf)
{
	uint32_t o_offset = offset;
	unsigned int i = 0;
	(void) size;

	add_header(tvmid, addr, o_offset, size/4, "DYNAMIC_DPB_T2 MESSAGE",  pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, STR_DPB_T2_MESSAGE "DPB_CONFIG_FLAGS", pm->dpbConfigFlags, 32, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "dpbLumaPitch", pm->dpbLumaPitch, 32, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "dpbLumaAlignedHeight", pm->dpbLumaAlignedHeight, 32, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "dpbLumaAlignedSize", pm->dpbLumaAlignedSize, 32, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "dpbChromaPitch", pm->dpbChromaPitch, 32, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "dpbChromaAlignedHeight", pm->dpbChromaAlignedHeight, 32, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "dpbChromaAlignedSize", pm->dpbChromaAlignedSize, 32, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "dpbArraySize", pm->dpbArraySize, 32, 10, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "dpbCurrLo", pm->dpbCurrLo, 32, 16, pOut, pBuf);
	add_field_1(asic, tvmid, addr, &offset, "dpbCurrHi", pm->dpbCurrHi, 32, 16, pOut, pBuf);
	for( i = 0; i < 16; i++)
		add_field_3(asic, tvmid, addr, &offset, "dpbAddrLo", i+1, pm->dpbAddrLo[i], 32, 16, pOut, pBuf);
	for( i = 0; i < 16; i++)
		add_field_3(asic, tvmid, addr, &offset, "dpbAddrHi", i+1, pm->dpbAddrHi[i], 32, 16, pOut, pBuf);
	add_tail(tvmid, addr, o_offset, "DYNAMIC_DPB_T2 MESSAGE",  pOut, pBuf);
	COMPARE_ERROR((offset - o_offset), size);
}

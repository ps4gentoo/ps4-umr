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
#include <stdbool.h>

#include "umr.h"
#include "import/ac_vcn_enc.h"
#include "import/ac_vcn_dec.h"

static struct umr_vcn_cmd_message *find_next_vcn_ib(struct umr_asic *asic, struct umr_vcn_cmd_message *vcn);

static uint32_t fetch_word(struct umr_asic *asic, struct umr_vcn_enc_stream *stream, uint32_t off)
{
	if (off > stream->nwords) {
		if (!(stream->invalid)) {
			asic->err_msg("[ERROR]: VCN ENC decoding of opcode (%"PRIx32") went out of bounds.\n", stream->opcode);
		}
		stream->invalid = 1;
		return 0;
	} else {
		return stream->words[off];
	}
}

/**
 * find_nwords - assign nwords for ENC IB or DEC IB for unified ring
 */
static void find_nwords(struct umr_asic *asic, struct umr_vcn_enc_stream *s)
{
	switch (s->opcode){
	case VCN_ENC_CMD_IB:
		s->nwords = 4;
		break;
	case VCN_ENC_CMD_REG_WAIT:
		s->nwords = 3;
		break;
	case VCN_ENC_CMD_END:
		s->nwords = 0;
		break;
	case VCN_ENC_CMD_NO_OP:
		s->nwords = 0;
		break;
	case VCN_ENC_CMD_FENCE:
		s->nwords = 4; /* fence and trap are together */
		break;
	case VCN_ENC_CMD_TRAP: /* in case - should not be separated without fence */
		s->nwords = 0;
		break;
	case VCN_ENC_CMD_REG_WRITE:
		s->nwords = 2;
		break;
	default:
		asic->err_msg("[ERROR]: unknown opcode [0x%0x] \n", s->opcode);
		s->nwords = 0;
		break;
	}
}

/**
 * umr_free_vcn_enc_stream - Free an ENC stream object
 */
void umr_free_vcn_enc_stream(struct umr_vcn_enc_stream *stream)
{
	while (stream) {
		struct umr_vcn_enc_stream *n;
		n = stream->next;
		free(stream->words);
		free(stream);
		stream = n;
	}
}

/**
 * umr_vcn_enc_decode_stream - Decode an array of VCN ENC packets into a VCN ENC stream
 *
 * @asic: The ASIC model the packet decoding corresponds to
 * @stream: An array of DWORDS which contain the VCN ENC packets
 * @nwords:  The number of words in the stream
 *
 * Returns a VCN ENC stream if successfully decoded.
 */
struct umr_vcn_enc_stream *umr_vcn_enc_decode_stream(struct umr_asic *asic, uint32_t *stream, uint32_t nwords)
{
	struct umr_vcn_enc_stream *ops, *ps, *prev_ps = NULL;
	struct umr_vcn_cmd_message *vcn = NULL;
	uint32_t *ostream = stream;
	struct {
		uint32_t
			size,
			vmid;
		uint64_t
			addr;
	} uvd_ib;

	ps = ops = calloc(1, sizeof *ops);
	if (!ps) {
		asic->err_msg("[ERROR]: Out of memory\n");
		return NULL;
	}

	memset(&uvd_ib, 0, sizeof uvd_ib);

	while (nwords) {
		ps->opcode = *stream;
		find_nwords(asic, ps);

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

		// grab all words
		if (ps->nwords) {
			ps->words = calloc(ps->nwords, sizeof(ps->words[0]));
			memcpy(ps->words, &stream[1], ps->nwords * sizeof(ps->words[0]));
		}
		uvd_ib.addr = 0;
		// decode specific packets
		if (ps->opcode == VCN_ENC_CMD_IB) {
			uvd_ib.vmid = fetch_word(asic, ps, 0) | ((asic->family <= FAMILY_VI) ? 0 : UMR_MM_HUB);
			uvd_ib.addr |= fetch_word(asic, ps, 1);
			uvd_ib.addr |= (uint64_t)fetch_word(asic, ps, 2) << 32;
			uvd_ib.size = fetch_word(asic, ps, 3) * 4;

			// we have everything we need to point to an IB
			if (!asic->options.no_follow_ib) {
				uint32_t *buf;
				buf = calloc(1, uvd_ib.size);
				if (umr_read_vram(asic, asic->options.vm_partition, uvd_ib.vmid, uvd_ib.addr, uvd_ib.size, (void *)buf) < 0) {
					asic->err_msg("[ERROR]: Could not read IB at 0x%"PRIx32":0x%" PRIx64 "\n", uvd_ib.vmid, uvd_ib.addr);
					free(buf);
				} else {
					ps->vcn = vcn = calloc(1, sizeof(struct umr_vcn_cmd_message));
					vcn->vmid = uvd_ib.vmid;
					vcn->addr = uvd_ib.addr;
					vcn->buf = buf;
					vcn->cmd = ps->opcode;
					vcn->size = uvd_ib.size;
					vcn->type = 1; /* ENC or DEC for unified ring */
					vcn->from = (stream - ostream + 3) * 4 ;  /* uvd_ib.addr is off by 3 dwords then convert to bytes */

					/* find decode IB in unified ring */
					struct umr_vcn_cmd_message * nvcn = find_next_vcn_ib(asic, vcn);
					if (nvcn)
						vcn->next = nvcn;
				}
			}
		}
		// advance stream
		nwords -= 1 + ps->nwords;
		stream += 1 + ps->nwords;
		if (nwords) {
			ps->next = calloc(1, sizeof(*ps));
			prev_ps = ps;
			ps = ps->next;
		}
	}
	return ops;
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

static struct umr_vcn_cmd_message *retrieve_decode_buffer(struct umr_asic *asic, uint32_t *p_curr,
							  uint32_t *p_end, struct umr_vcn_cmd_message *vcn)
{
	uint32_t ib_size, ib_type;
	uint32_t offset = 0;

	while (p_curr < p_end) {
		ib_size = *p_curr++;
		ib_type = *p_curr++;

		if (!ib_size) { /* empty */
			break;
		}
		if (ib_type == RDECODE_IB_PARAM_DECODE_BUFFER) {
			struct umr_vcn_cmd_message * nvcn;
			rvcn_decode_buffer_t *p = (rvcn_decode_buffer_t *)p_curr;
			uint32_t valid_buf_flag = p->valid_buf_flag;
			if (valid_buf_flag & RDECODE_CMDBUF_FLAGS_MSG_BUFFER) {
				nvcn = calloc(1, sizeof(struct umr_vcn_cmd_message));
				nvcn->vmid = vcn->vmid;
				nvcn->addr = (uint64_t)p->msg_buffer_address_hi << 32 | p->msg_buffer_address_lo;
				nvcn->cmd = RDECODE_CMD_MSG_BUFFER;
				nvcn->type = 0;
				nvcn->from = vcn->addr + offset + 8 + 4;
				uint32_t size = sizeof(rvcn_dec_message_header_t);
				rvcn_dec_message_header_t *mh = calloc(1, size);
				if (umr_read_vram(asic, asic->options.vm_partition, nvcn->vmid, nvcn->addr, size, mh) < 0) {
					asic->err_msg("[ERROR]: Could not read IB at 0x%"PRIx32":0x%" PRIx64 "\n", nvcn->vmid, nvcn->addr);
					free(mh);
					free(nvcn);
					return NULL;
				} else {
					/* calculate the total_size in case it is wrong from the header */
					uint32_t total_size = mh->header_size;
					rvcn_dec_message_index_t *p0 = &mh->index[0]; /* first message */
					total_size += p0->size; /* add the first message size */
					/* read in all messages if num_buffers is 2+ */
					if (mh->num_buffers > 1) {
						uint32_t size_ex = (mh->num_buffers - 1) * sizeof(rvcn_dec_message_index_t);
						rvcn_dec_message_index_t *pi = calloc(1, size_ex); /* all other messages exept the first one */
						total_size += size_ex;
						if (umr_read_vram(asic, asic->options.vm_partition, nvcn->vmid, nvcn->addr + mh->header_size, size_ex, pi) < 0) {
							asic->err_msg("[ERROR]: Could not read IB at 0x%"PRIx32":0x%" PRIx64 "\n", nvcn->vmid, nvcn->addr);
							free(pi);
							free(mh);
							free(nvcn);
							return NULL;
						} else {
							uint32_t i;
							for (i = 0; i < mh->num_buffers - 1; i++) {
								total_size += pi[i].size;
							}
							free(pi);
						}
					}
					nvcn->size = total_size;
					if (mh->total_size != total_size )
						asic->err_msg("[WARN]: Invalid IB size reported [%d], should be [%d] at 0x%"PRIx32":0x%" PRIx64 "\n", mh->total_size, total_size, nvcn->vmid, nvcn->addr);

					free(mh);
				}
				return nvcn;
			}
			return NULL;
		}
		p_curr += ib_size /4 - 2;
		offset += ib_size;
	}
	return NULL;
}

static struct umr_vcn_cmd_message *find_next_vcn_ib(struct umr_asic *asic, struct umr_vcn_cmd_message *vcn)
{
	struct umr_ip_block *ip;
	uint32_t nwords = vcn->size / 4;
	uint32_t *p_curr = vcn->buf;
	uint32_t *p_end = p_curr + nwords;
	uint32_t vcn_ip_version;
	uint32_t engine_type;

	ip = umr_find_ip_block(asic, "vcn", 0); // for multi instance
	if (!ip)
		ip = umr_find_ip_block(asic, "vcn", -1); // for single instance
	if (!ip) {
		asic->err_msg("[ERROR]: Cannot get VCN IP info\n");
		return NULL;
	}
	vcn_ip_version = VCN_IP_VERSION(ip->discoverable.maj, ip->discoverable.min, ip->discoverable.rev);
	if (vcn_ip_version >= VCN_IP_VERSION(4, 0, 0)) { //for unified ring
		retrieve_engine_info(p_curr, p_end, &engine_type);
		if (engine_type == RADEON_VCN_ENGINE_TYPE_DECODE) {
			return retrieve_decode_buffer(asic, p_curr, p_end, vcn);
		}
	}
	return NULL;
}

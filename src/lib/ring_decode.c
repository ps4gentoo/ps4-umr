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
#include "umrapp.h"
#include <inttypes.h>

static const char *pm4_pkt3_opcode_names[] = {
	"UNK", // 00
	"UNK", // 01
	"UNK", // 02
	"UNK", // 03
	"UNK", // 04
	"UNK", // 05
	"UNK", // 06
	"UNK", // 07
	"UNK", // 08
	"UNK", // 09
	"UNK", // 0a
	"UNK", // 0b
	"UNK", // 0c
	"UNK", // 0d
	"UNK", // 0e
	"UNK", // 0f
	"PKT3_NOP", // 10
	"PKT3_SET_BASE", // 11
	"PKT3_CLEAR_STATE", // 12
	"PKT3_INDEX_BUFFER_SIZE", // 13
	"UNK", // 14
	"PKT3_DISPATCH_DIRECT", // 15
	"PKT3_DISPATCH_INDIRECT", // 16
	"UNK", // 17
	"UNK", // 18
	"UNK", // 19
	"UNK", // 1a
	"UNK", // 1b
	"UNK", // 1c
	"PKT3_ATOMIC_GDS", // 1d
	"PKT3_ATOMIC_MEM", // 1e
	"PKT3_OCCLUSION_QUERY", // 1f
	"PKT3_SET_PREDICATION", // 20
	"PKT3_REG_RMW", // 21
	"PKT3_COND_EXEC", // 22
	"PKT3_PRED_EXEC", // 23
	"PKT3_DRAW_INDIRECT", // 24
	"PKT3_DRAW_INDEX_INDIRECT", // 25
	"PKT3_INDEX_BASE", // 26
	"PKT3_DRAW_INDEX_2", // 27
	"PKT3_CONTEXT_CONTROL", // 28
	"UNK", // 29
	"PKT3_INDEX_TYPE", // 2a
	"UNK", // 2b
	"PKT3_DRAW_INDIRECT_MULTI", // 2c
	"PKT3_DRAW_INDEX_AUTO", // 2d
	"PKT3_DRAW_INDEX_IMMD", // 2e
	"PKT3_NUM_INSTANCES", // 2f
	"PKT3_DRAW_INDEX_MULTI_AUTO", // 30
	"UNK", // 31
	"PKT3_INDIRECT_BUFFER_SI", // 32
	"PKT3_INDIRECT_BUFFER_CONST", // 33
	"PKT3_STRMOUT_BUFFER_UPDATE", // 34
	"PKT3_DRAW_INDEX_OFFSET_2", // 35
	"PKT3_DRAW_PREAMBLE", // 36
	"PKT3_WRITE_DATA", // 37
	"PKT3_DRAW_INDEX_INDIRECT_MULTI", // 38
	"PKT3_MEM_SEMAPHORE", // 39
	"PKT3_MPEG_INDEX", // 3a
	"UNK", // 3b
	"PKT3_WAIT_REG_MEM", // 3c
	"PKT3_MEM_WRITE", // 3d
	"UNK", // 3e
	"PKT3_INDIRECT_BUFFER_CIK", // 3f
	"PKT3_COPY_DATA", // 40
	"PKT3_CP_DMA", // 41
	"PKT3_PFP_SYNC_ME", // 42
	"PKT3_SURFACE_SYNC", // 43
	"PKT3_ME_INITIALIZE", // 44
	"PKT3_COND_WRITE", // 45
	"PKT3_EVENT_WRITE", // 46
	"PKT3_EVENT_WRITE_EOP", // 47
	"PKT3_EVENT_WRITE_EOS", // 48
	"PKT3_RELEASE_MEM", // 49
	"PKT3_PREAMBLE_CNTL", // 4a
	"UNK", // 4b
	"PKT3_DISPATCH_MESH_INDIRECT_MULTI", // 4c
	"PKT3_DISPATCH_TASKMESH_GFX", // 4d
	"UNK", // 4e
	"UNK", // 4f
	"PKT3_DMA_DATA", // 50
	"PKT3_CONTEXT_REG_RMW", // 51
	"UNK", // 52
	"UNK", // 53
	"UNK", // 54
	"UNK", // 55
	"UNK", // 56
	"PKT3_ONE_REG_WRITE", // 57
	"PKT3_ACQUIRE_MEM", // 58
	"PKT3_REWIND", // 59
	"UNK", // 5a
	"UNK", // 5b
	"UNK", // 5c
	"UNK", // 5d
	"PKT3_LOAD_UCONFIG_REG", // 5e
	"PKT3_LOAD_SH_REG", // 5f
	"PKT3_LOAD_CONFIG_REG", // 60
	"PKT3_LOAD_CONTEXT_REG", // 61
	"UNK", // 62
	"PKT3_LOAD_SH_REG_INDEX", // 63
	"UNK", // 64
	"UNK", // 65
	"UNK", // 66
	"UNK", // 67
	"PKT3_SET_CONFIG_REG", // 68
	"PKT3_SET_CONTEXT_REG", // 69
	"UNK", // 6a
	"UNK", // 6b
	"UNK", // 6c
	"UNK", // 6d
	"UNK", // 6e
	"UNK", // 6f
	"UNK", // 70
	"UNK", // 71
	"UNK", // 72
	"PKT3_SET_CONTEXT_REG_INDIRECT", // 73
	"UNK", // 74
	"UNK", // 75
	"PKT3_SET_SH_REG", // 76
	"PKT3_SET_SH_REG_OFFSET", // 77
	"PKT3_SET_QUEUE_REG", // 78
	"PKT3_SET_UCONFIG_REG", // 79
	"PKT3_SET_UCONFIG_REG_INDEX", // 7a
	"UNK", // 7b
	"UNK", // 7c
	"UNK", // 7d
	"UNK", // 7e
	"UNK", // 7f
	"PKT3_LOAD_CONST_RAM", // 80
	"PKT3_WRITE_CONST_RAM", // 81
	"UNK", // 82
	"PKT3_DUMP_CONST_RAM", // 83
	"PKT3_INCREMENT_CE_COUNTER", // 84
	"PKT3_INCREMENT_DE_COUNTER", // 85
	"PKT3_WAIT_ON_CE_COUNTER", // 86
	"UNK", // 87
	"PKT3_WAIT_ON_DE_COUNTER_DIFF", // 88
	"UNK", // 89
	"UNK", // 8a
	"PKT3_SWITCH_BUFFER", // 8b
	"UNK", // 8c
	"UNK", // 8d
	"UNK", // 8e
	"UNK", // 8f
	"PKT3_FRAME_CONTROL", // 90
	"PKT3_INDEX_ATTRIBUTES_INDIRECT", // 91
	"UNK", // 92
	"UNK", // 93
	"UNK", // 94
	"UNK", // 95
	"UNK", // 96
	"UNK", // 97
	"UNK", // 98
	"UNK", // 99
	"PKT3_DMA_DATA_FILL_MULTI", // 9a
	"PKT3_SET_SH_REG_INDEX", // 9b
	"UNK", // 9c
	"UNK", // 9d
	"UNK", // 9e
	"PKT3_LOAD_CONTEXT_REG_INDEX", // 9f
	"PKT3_SET_RESOURCES", // a0
	"PKT3_MAP_PROCESS", // a1
	"PKT3_MAP_QUEUES", // a2
	"PKT3_UNMAP_QUEUES", // a3
	"PKT3_QUERY_STATUS", // a4
	"UNK", // a5
	"UNK", // a6
	"UNK", // a7
	"UNK", // a8
	"PKT3_DISPATCH_TASK_STATE_INIT", // a9
	"PKT3_DISPATCH_TASKMESH_DIRECT_ACE", // aa
	"UNK", // ab
	"UNK", // ac
	"PKT3_DISPATCH_TASKMESH_INDIRECT_MULTI_ACE", // ad
	"UNK", // ae
	"UNK", // af
	"UNK", // b0
	"UNK", // b1
	"UNK", // b2
	"UNK", // b3
	"UNK", // b4
	"UNK", // b5
	"UNK", // b6
	"UNK", // b7
	"UNK", // b8
	"UNK", // b9
	"UNK", // ba
	"UNK", // bb
	"UNK", // bc
	"UNK", // bd
	"UNK", // be
	"UNK", // bf
	"UNK", // c0
	"UNK", // c1
	"UNK", // c2
	"UNK", // c3
	"UNK", // c4
	"UNK", // c5
	"UNK", // c6
	"UNK", // c7
	"UNK", // c8
	"UNK", // c9
	"UNK", // ca
	"UNK", // cb
	"UNK", // cc
	"UNK", // cd
	"UNK", // ce
	"UNK", // cf
	"UNK", // d0
	"UNK", // d1
	"UNK", // d2
	"UNK", // d3
	"UNK", // d4
	"UNK", // d5
	"UNK", // d6
	"UNK", // d7
	"UNK", // d8
	"UNK", // d9
	"UNK", // da
	"UNK", // db
	"UNK", // dc
	"UNK", // dd
	"UNK", // de
	"UNK", // df
	"UNK", // e0
	"UNK", // e1
	"UNK", // e2
	"UNK", // e3
	"UNK", // e4
	"UNK", // e5
	"UNK", // e6
	"UNK", // e7
	"UNK", // e8
	"UNK", // e9
	"UNK", // ea
	"UNK", // eb
	"UNK", // ec
	"UNK", // ed
	"UNK", // ee
	"UNK", // ef
	"UNK", // f0
	"UNK", // f1
	"UNK", // f2
	"UNK", // f3
	"UNK", // f4
	"UNK", // f5
	"UNK", // f6
	"UNK", // f7
	"UNK", // f8
	"UNK", // f9
	"UNK", // fa
	"UNK", // fb
	"UNK", // fc
	"UNK", // fd
	"UNK", // fe
	"UNK", // ff
};

static const struct {
	char *name;
	unsigned event_no;
} vgt_event_tags[] = {
	{ "SAMPLE_STREAMOUTSTATS1", 1 },
	{ "SAMPLE_STREAMOUTSTATS2", 2 },
	{ "SAMPLE_STREAMOUTSTATS3", 3 },
	{ "CACHE_FLUSH_TS", 4 },
	{ "CACHE_FLUSH", 6 },
	{ "CS_PARTIAL_FLUSH", 7 },
	{ "VGT_STREAMOUT_RESET", 10 },
	{ "END_OF_PIPE_INCR_DE", 11 },
	{ "END_OF_PIPE_IB_END", 12 },
	{ "RST_PIX_CNT", 13 },
	{ "VS_PARTIAL_FLUSH", 15 },
	{ "PS_PARTIAL_FLUSH", 16 },
	{ "CACHE_FLUSH_AND_INV_TS_EVENT", 20 },
	{ "ZPASS_DONE", 21 },
	{ "CACHE_FLUSH_AND_INV_EVENT", 22 },
	{ "PERFCOUNTER_START", 23 },
	{ "PERFCOUNTER_STOP", 24 },
	{ "PIPELINESTAT_START", 25 },
	{ "PIPELINESTAT_STOP", 26 },
	{ "PERFCOUNTER_SAMPLE", 27 },
	{ "SAMPLE_PIPELINESTAT", 30 },
	{ "SAMPLE_STREAMOUTSTATS", 32 },
	{ "RESET_VTX_CNT", 33 },
	{ "VGT_FLUSH", 36 },
	{ "BOTTOM_OF_PIPE_TS", 40 },
	{ "DB_CACHE_FLUSH_AND_INV", 42 },
	{ "FLUSH_AND_INV_DB_DATA_TS", 43 },
	{ "FLUSH_AND_INV_DB_META", 44 },
	{ "FLUSH_AND_INV_CB_DATA_TS", 45 },
	{ "FLUSH_AND_INV_CB_META", 46 },
	{ "CS_DONE", 47 },
	{ "PS_DONE", 48 },
	{ "FLUSH_AND_INV_CB_PIXEL_DATA", 49 },
	{ "THREAD_TRACE_START", 51 },
	{ "THREAD_TRACE_STOP", 52 },
	{ "THREAD_TRACE_FLUSH", 54 },
	{ "THREAD_TRACE_FINISH", 55 },
	{ NULL, 0 },
};

static char *vgt_event_decode(unsigned tag)
{
	unsigned x;
	for (x = 0; vgt_event_tags[x].name; x++) {
		if (vgt_event_tags[x].event_no == tag)
			return vgt_event_tags[x].name;
	}
	return "<unknown event>";
}

const char *umr_pm4_opcode_to_str(uint32_t header)
{
	return pm4_pkt3_opcode_names[(header >> 8) & 0xFF];
}

#define BITS(x, a, b) (unsigned long)((x >> (a)) & ((1ULL << ((b)-(a)))-1))

/**
 * add_shader - Add a shader to the list of shaders found
 */
static void add_shader(struct umr_asic *asic, struct umr_ring_decoder *decoder)
{
	struct umr_shaders_pgm *pshader;

	if (!decoder->shader) {
		pshader = decoder->shader = calloc(1, sizeof *pshader);
		if (!pshader)
			goto error;
	} else {
		pshader = decoder->shader;
		while (pshader->next)
			pshader = pshader->next;

		pshader->next = calloc(1, sizeof *pshader);
		if (!pshader->next)
			goto error;
		pshader = pshader->next;
	}

	pshader->vmid = decoder->pm4.next_ib_state.ib_vmid;
	pshader->addr = (((uint64_t)decoder->pm4.next_ib_state.ib_addr_hi << 32) |
					 decoder->pm4.next_ib_state.ib_addr_lo) << 8;
	pshader->size = umr_compute_shader_size(asic, asic->options.vm_partition, pshader);
	pshader->src.ib_offset = decoder->next_ib_info.addr;
	pshader->src.ib_base = decoder->next_ib_info.ib_addr;
	return;
error:
	asic->err_msg("[ERROR]: Out of memory in add_shader()\n");
}

/**
 * add_ib_pm4 - Add an indirect buffer to the links list of buffers
 */
static void add_ib_pm4(struct umr_ring_decoder *decoder)
{
	struct umr_ring_decoder *pdecoder;

	pdecoder = decoder;
	while (pdecoder->next_ib)
		pdecoder = pdecoder->next_ib;

	pdecoder->next_ib = calloc(1, sizeof(*(pdecoder->next_ib)));
	pdecoder = pdecoder->next_ib;
	pdecoder->pm = 4;
	pdecoder->next_ib_info.ib_addr = ((uint64_t)decoder->pm4.next_ib_state.ib_addr_hi << 32) |
					 decoder->pm4.next_ib_state.ib_addr_lo;
	pdecoder->next_ib_info.size    = decoder->pm4.next_ib_state.ib_size;
	pdecoder->next_ib_info.vmid    = decoder->pm4.next_ib_state.ib_vmid;
	pdecoder->next_ib_info.vm_base_addr = ~0ULL; // not used yet.


	pdecoder->src.ib_addr = decoder->next_ib_info.ib_addr;
	pdecoder->src.vmid    = decoder->next_ib_info.vmid;
	pdecoder->src.addr    = decoder->next_ib_info.addr;

	memset(&decoder->pm4.next_ib_state, 0, sizeof(decoder->pm4.next_ib_state));
}

static void add_data_block_pm4(struct umr_asic *asic, struct umr_ring_decoder *decoder, enum UMR_DATABLOCK_ENUM type)
{
	struct umr_pm4_data_block *p = decoder->datablock;

	if (!p) {
		// start list
		decoder->datablock = p = calloc(1, sizeof(*p));
		if (!p) {
			asic->err_msg("[ERROR]: Out of memory in add_data_block_pm4()\n");
			return;
		}
	} else {
		// append to list
		while (p->next) {
			p = p->next;
		}
		p->next = calloc(1, sizeof(*p));
		p = p->next;
		if (!p) {
			asic->err_msg("[ERROR]: Out of memory in add_data_block_pm4()\n");
			return;
		}
	}

	p->type = type;
	p->addr = ((uint64_t)decoder->pm4.next_ib_state.ib_addr_hi << 32) | decoder->pm4.next_ib_state.ib_addr_lo;
	p->vmid = decoder->pm4.next_ib_state.ib_vmid;
	p->extra = decoder->pm4.next_ib_state.ib_size;

}

/**
 * add_ib_pm3 - Add an SDMA indirect buffer to the linked list
 */
static void add_ib_pm3(struct umr_ring_decoder *decoder)
{
	struct umr_ring_decoder *pdecoder;

	pdecoder = decoder;
	while (pdecoder->next_ib)
		pdecoder = pdecoder->next_ib;

	pdecoder->next_ib = calloc(1, sizeof(*(pdecoder->next_ib)));
	pdecoder = pdecoder->next_ib;
	pdecoder->pm = 3;
	pdecoder->next_ib_info.ib_addr = ((uint64_t)decoder->sdma.next_ib_state.ib_addr_hi << 32) |
					 decoder->sdma.next_ib_state.ib_addr_lo;
	pdecoder->next_ib_info.size    = decoder->sdma.next_ib_state.ib_size;
	pdecoder->next_ib_info.vmid    = decoder->sdma.next_ib_state.ib_vmid;
	pdecoder->next_ib_info.vm_base_addr = ~0ULL; // not used yet.


	pdecoder->src.ib_addr = decoder->next_ib_info.ib_addr;
	pdecoder->src.vmid    = decoder->next_ib_info.vmid;
	pdecoder->src.addr    = decoder->next_ib_info.addr;

	memset(&decoder->sdma.next_ib_state, 0, sizeof(decoder->sdma.next_ib_state));
}

/**
 * print_bits - Print the bitfields of a register nicely indented.
 */
static void print_bits(struct umr_asic *asic, uint32_t regno, uint32_t value, int tabs, int (*print)(const char *fmt, ...))
{
	struct umr_ip_block *ip;
	struct umr_reg *reg = umr_find_reg_by_addr(asic, regno, &ip);

	if (reg && reg->bits && reg->no_bits && asic->options.bitfields) {
		int k;
		print("\n");
		for (k = 0; k < reg->no_bits; k++) {
			uint32_t v;
			if (tabs) print("\t\t");
			if (k == (reg->no_bits - 1))
				print("\t\t\t\t\t\t\t\t\\----+ ");
			else
				print("\t\t\t\t\t\t\t\t|----+ ");
			v = (value >> reg->bits[k].start) & ((1ULL << ((reg->bits[k].stop + 1) - reg->bits[k].start)) - 1);
			reg->bits[k].bitfield_print(asic, asic->asicname, ip->ipname, reg->regname, reg->bits[k].regname, reg->bits[k].start, reg->bits[k].stop, v);
		}
	}
}

/**
 * print_decode_pm4_pkt3 - Print out the decoding of a PKT3 packet
 */
static void print_decode_pm4_pkt3(struct umr_asic *asic, struct umr_ring_decoder *decoder, uint32_t ib, int (*print)(const char *fmt, ...))
{
	static const char *op_3c_functions[] = { "true", "<", "<=", "==", "!=", ">=", ">", "reserved" };
	static const char *op_37_engines[] = { "ME", "PFP", "CE", "DE" };
	static const char *op_37_dst_sel[] = { "mem-mapped reg", "memory sync", "TC/L2", "GDS", "reserved", "memory async", "reserved", "reserved", "preemption meta memory" };
	static const char *op_40_mem_sel[] = { "mem-mapped reg", "memory", "tc_l2", "gds", "perfcounters", "immediate data", "atomic return data", "gds_atomic_return_data_0", "gds_atomic_return_data1", "gpu_clock_count", "system_clock_count" };
	static const char *op_84_cntr_sel[] = { "invalid", "ce", "cs", "ce and cs" };
	static const char *op_7a_index_str[] = { "default", "prim_type", "index_type", "num_instance", "multi_vgt_param", "reserved", "reserved", "reserved" };
	struct umr_reg *reg;
	char buf[4];

	if (decoder->pm4.n_words == 1)
		print("\\---+ ");
	else
		print("|---+ ");

	print("PKT3 OPCODE 0x%02x, word %u: ", (unsigned)decoder->pm4.cur_opcode, (unsigned)decoder->pm4.cur_word);
	switch (decoder->pm4.cur_opcode) {
		case 0x10: // NOP
			switch (decoder->pm4.cur_word) {
				case 0:
					// comment NOPs have a MAGIC word first
					if (ib == 0x3337F77D)
						decoder->pm4.nop.magic = 2; // BINARY data
					else if (ib == 0x1337F77D)
						decoder->pm4.nop.magic = 1; // TEXT data
					else
						decoder->pm4.nop.magic = 0;
					print("MAGIC: %s%d%s", BLUE, (int)decoder->pm4.nop.magic, RST);
					break;
				case 1:
					if (decoder->pm4.nop.magic == 1) {
						decoder->pm4.nop.pktlen = ib - 1; // number of data words (we don't care about the PM4 header)
						print("PKT_SIZE: %s%lu%s", BLUE, (unsigned long)decoder->pm4.nop.pktlen, RST);
						break;
					}
					// fall through
				case 2:
					if (decoder->pm4.nop.magic == 1) {
						decoder->pm4.nop.pkttype = ib; // type of packet
						print("PKT_TYPE: %s%lu%s", BLUE, (unsigned long)decoder->pm4.nop.pkttype, RST);
						decoder->pm4.nop.str = calloc(1, decoder->pm4.nop.pktlen * 4 - 12 + 1);
						break;
					}
					// fall through
				default:
					// process 'string' type comments
					if (decoder->pm4.nop.magic == 1 && decoder->pm4.nop.pkttype == 7 && decoder->pm4.nop.pktlen > decoder->pm4.cur_word) {
						memcpy(decoder->pm4.nop.str + decoder->pm4.cur_word * 4 - 12, &ib, 4);

						// if this was the last word print out the string
						if ((decoder->pm4.cur_word + 1) == decoder->pm4.nop.pktlen) {
							print("CommentString: [%s]", decoder->pm4.nop.str);
							free(decoder->pm4.nop.str);
							decoder->pm4.nop.magic = 0;
						}
						break;
					} else if (decoder->pm4.nop.magic == 2) {
						print("BINARY_DATA: %s0x%lx%s", BLUE, (unsigned long)ib, RST);
						break;
					}
			}
			break;
		case 0x12: // CLEAR_STATE
			switch (decoder->pm4.cur_word) {
				case 0: print("CMD: %s0x%08lu%s", BLUE, (unsigned long)BITS(ib, 0, 4), RST);
					break;
				default: print("Invalid word for opcode 0x%02lx", (unsigned long)decoder->pm4.cur_opcode);
			}
			break;
		case 0x15: // DISPATCH DIRECT
			switch (decoder->pm4.cur_word) {
				case 0: print("DIM_X: %s0x%08lx%s", BLUE, (unsigned long)ib, RST);
					break;
				case 1: print("DIM_Y: %s0x%08lx%s", BLUE, (unsigned long)ib, RST);
					break;
				case 2: print("DIM_Z: %s0x%08lx%s", BLUE, (unsigned long)ib, RST);
					break;
				case 3: print("INITIATOR: %s0x%08lx%s", BLUE, (unsigned long)ib, RST);
					break;
				default: print("Invalid word for opcode 0x%02lx", (unsigned long)decoder->pm4.cur_opcode);
			}
			break;
		case 0x22: // COND_EXEC
			switch (decoder->pm4.cur_word) {
				case 0: print("GPU_ADDR_LO32: %s0x%08lx%s", YELLOW, (unsigned long)BITS(ib, 2, 32) << 2, RST);
					break;
				case 1: print("GPU_ADDR_HI32: %s0x%08lx%s", YELLOW, (unsigned long)ib, RST);
					break;
				case 2: print("TEST_VALUE: %s0x%08lx%s", YELLOW, (unsigned long)ib, RST);
					break;
				case 3: print("PATCH_VALUE: %s0x%08lx%s", YELLOW, (unsigned long)ib, RST);
					break;
				default: print("Invalid word for opcode 0x%02lx", (unsigned long)decoder->pm4.cur_opcode);
			}
			break;
		case 0x27: // DRAW_INDEX_2
			switch (decoder->pm4.cur_word) {
				case 0: print("MAX_SIZE: %s%lu%s", BLUE, (unsigned long) BITS(ib, 0, 32), RST); break;
				case 1: print("INDEX_BASE_LO: %s0x%08lx%s", YELLOW, (unsigned long) BITS(ib, 0, 32), RST);
						decoder->pm4.next_ib_state.ib_addr_lo = ib;
						break;
				case 2: print("INDEX_BASE_HI: %s0x%08lx%s", YELLOW, (unsigned long) BITS(ib, 0, 32), RST);
						decoder->pm4.next_ib_state.ib_addr_hi = ib;
						if (!asic->options.no_follow_ib) {
							if (umr_read_vram(asic, asic->options.vm_partition, decoder->next_ib_info.vmid,
											  ((uint64_t)decoder->pm4.next_ib_state.ib_addr_hi << 32) | decoder->pm4.next_ib_state.ib_addr_lo, 4, buf) < 0)
								print(" [%sUNMAPPED%s]", RED, RST);
							else
								print(" [%sMAPPED%s]", GREEN, RST);
						}
						break;
				case 3: print("INDEX_COUNT: %s%lu%s", BLUE, (unsigned long) BITS(ib, 0, 32), RST); break;
				case 4: print("DRAW_INITIATOR: %s0x%08lx%s", YELLOW, (unsigned long) BITS(ib, 0, 32), RST); break;
				default: print("Invalid word for opcode 0x%02lx", (unsigned long)decoder->pm4.cur_opcode);
			}
			break;
		case 0x28: // CONTEXT_CONTROL
			switch (decoder->pm4.cur_word) {
				case 0: print("LOAD_EN: %s%lu%s, LOAD_CS: %s%lu%s, LOAD_GFX: %s%lu%s, LOAD_MULTI: %s%lu%s, LOAD_SINGLE: %s%lu%s",
						BLUE, BITS(ib, 31, 32), RST,
						BLUE, BITS(ib, 24, 25), RST,
						BLUE, BITS(ib, 16,17), RST,
						BLUE, BITS(ib, 1,2), RST,
						BLUE, BITS(ib, 0, 1), RST);
					break;
				case 1: print("SHADOW_EN: %s%lu%s, SHADOW_CS: %s%lu%s, SHADOW_GFX: %s%lu%s, SHADOW_MULTI: %s%lu%s, SHADOW_SINGLE: %s%lu%s",
						BLUE, BITS(ib, 31, 32), RST,
						BLUE, BITS(ib, 24, 25), RST,
						BLUE, BITS(ib, 16,17), RST,
						BLUE, BITS(ib, 1,2), RST,
						BLUE, BITS(ib, 0, 1), RST);
					break;
				default: print("Invalid word for opcode 0x%02lx", (unsigned long)decoder->pm4.cur_opcode);
			}
			break;
		case 0x2d: // DRAW_INDEX_AUTO
			switch (decoder->pm4.cur_word) {
				case 0: print("INDEX_COUNT: %s%lu%s", BLUE, (unsigned long)ib, RST);
					break;
				case 1: print("DRAW_INITIATOR: %s0x%lx%s", YELLOW, (unsigned long)ib, RST);
					break;
				default: print("Invalid word for opcode 0x%02lx", (unsigned long)decoder->pm4.cur_opcode);
			}
			break;
		case 0x2f: // NUM_INSTANCES
			switch (decoder->pm4.cur_word) {
				case 0: print("NUM_INSTANCES: %s%lu%s", BLUE, (unsigned long)ib, RST);
					break;
				default: print("Invalid word for opcode 0x%02lx", (unsigned long)decoder->pm4.cur_opcode);
			}
			break;
		case 0x3f: // INDIRECT_BUFFER_CIK
		case 0x33: // INDIRECT_BUFFER_CONST
			switch (decoder->pm4.cur_word) {
				case 0: print("IB_BASE_LO: %s0x%08lx%s, SWAP:%lu", YELLOW, BITS(ib, 2, 32) << 2, RST, BITS(ib, 0, 2));
					decoder->pm4.next_ib_state.ib_addr_lo = BITS(ib, 2, 32) << 2;
					break;
				case 1: print("IB_BASE_HI: %s0x%08lx%s", YELLOW, BITS(ib, 0, 16), RST);
					decoder->pm4.next_ib_state.ib_addr_hi = BITS(ib, 0, 16);
					break;
				case 2: print("IB_SIZE:%s%lu%s, VMID: %s%lu%s", BLUE, BITS(ib, 0, 20), RST, BLUE, BITS(ib, 24, 28), RST);
					decoder->pm4.next_ib_state.ib_size = BITS(ib, 0, 20) * 4;
					decoder->pm4.next_ib_state.ib_vmid = decoder->next_ib_info.vmid ? decoder->next_ib_info.vmid : BITS(ib, 24, 28);
					if (asic->family >= FAMILY_AI) {
						print(", CHAIN: %s%u%s, PRE_ENA: %s%u%s, CACHE_POLICY: %s%u%s, PRE_RESUME: %s%u%s PRIV: %s%u%s",
							   BLUE, (unsigned)BITS(ib, 20, 21), RST,
							   BLUE, (unsigned)BITS(ib, 21, 22), RST,
							   BLUE, (unsigned)BITS(ib, 28, 30), RST,
							   BLUE, (unsigned)BITS(ib, 30, 31), RST,
							   BLUE, (unsigned)BITS(ib, 31, 32), RST);
					}
					if (!asic->options.no_follow_ib) {
						if (umr_read_vram(asic, asic->options.vm_partition, decoder->pm4.next_ib_state.ib_vmid,
										  ((uint64_t)decoder->pm4.next_ib_state.ib_addr_hi << 32) | decoder->pm4.next_ib_state.ib_addr_lo, 4, buf) < 0) {
							print(" [%sUNMAPPED%s]", RED, RST);
						} else {
							print(" [%sMAPPED%s]", GREEN, RST);
							add_ib_pm4(decoder);
						}
					}
					break;
				default: print("Invalid word for opcode 0x%02lx", (unsigned long)decoder->pm4.cur_opcode);
			}
			break;
		case 0x37: // WRITE_DATA
			switch (decoder->pm4.cur_word) {
				case 0: print("ENGINE:[%s%s%s], WR_CONFIRM:%s%lu%s, WR_ONE_ADDR:%s%lu%s, DST_SEL:[%s%s%s]",
						BLUE, op_37_engines[BITS(ib,30,32)], RST,
						BLUE, BITS(ib,20,21), RST,
						BLUE, BITS(ib,16,17), RST,
						BLUE, op_37_dst_sel[BITS(ib, 8, 12)], RST);
					decoder->pm4.control = ib;
					decoder->pm4.next_write_mem.type = BITS(ib, 8, 12);
					break;
				case 1: print("DST_ADDR_LO: %s0x%08lx%s", YELLOW, (unsigned long)BITS(ib, 2, 32) << 2, RST);
					decoder->pm4.next_write_mem.addr_lo = ib;
					break;
				case 2: print("DST_ADDR_HI: %s0x%08lx%s", YELLOW, (unsigned long)ib, RST);
					decoder->pm4.next_write_mem.addr_hi = ib;
					break;
				default:
					if (decoder->pm4.next_write_mem.type == 0) { // mem-mapped reg
						print("%s <= %s0x%08lx%s",
							umr_reg_name(asic, ((uint64_t)decoder->pm4.next_write_mem.addr_hi << 32) | decoder->pm4.next_write_mem.addr_lo),
							YELLOW, (unsigned long)ib, RST);
						print_bits(asic, decoder->pm4.next_write_mem.addr_lo, ib, 1, print);
						decoder->pm4.next_write_mem.addr_lo++;
						if (!decoder->pm4.next_write_mem.addr_lo)
							decoder->pm4.next_write_mem.addr_hi++;
					} else {
						print("DATA: %s0x%08lx%s", YELLOW, (unsigned long)ib, RST);
					}
			}
			break;
		case 0x3C: // WAIT_MEM_REG
			switch(decoder->pm4.cur_word) {
				case 0: print("ENGINE:%s%s%s, MEMSPACE:%s%s%s, FUNC:[%s%s%s]",
						BLUE, BITS(ib, 8, 9) ? "PFP" : "ME", RST,
						BLUE, BITS(ib, 4, 5) ? "MEM" : "REG", RST,
						BLUE, op_3c_functions[BITS(ib, 0, 4)], RST);
					break;
				case 1: print("POLL_ADDRESS_LO: %s0x%08lx%s, SWAP: %s%lu%s",
						YELLOW, BITS(ib, 2, 32) << 2, RST,
						BLUE, BITS(ib, 0, 2), RST);
					break;
				case 2: print("POLL_ADDRESS_HI: %s0x%08lx%s", YELLOW, (unsigned long)ib, RST);
					break;
				case 3: print("REFERENCE: %s0x%08lx%s", BLUE, (unsigned long)ib, RST);
					break;
				case 4: print("MASK: %s0x%08lx%s", BLUE, (unsigned long)ib, RST);
					break;
				case 5: print("POLL INTERVAL: %s0x%08lx%s", BLUE, BITS(ib, 0, 16), RST);
					break;
				default: print("Invalid word for opcode 0x%02lx", (unsigned long)decoder->pm4.cur_opcode);
			}
			break;
		case 0x40: // COPY_DATA
			switch (decoder->pm4.cur_word) {
				case 0:
					decoder->pm4.next_write_mem.type = ib;
					print("SRC_SEL: %s%lu%s [%s%s%s], DST_SEL: %s%lu%s [%s%s%s], SRC_CACHE_POLICY: %s%lu%s, COUNT_SEL: %s%lu%s, WR_CONFIRM: %s%lu%s, DST_CACHE_POLICY: %s%lu%s, PQ_EXE_STATUS: %s%lu%s",
						BLUE, BITS(ib, 0, 4), RST,
						CYAN, op_40_mem_sel[BITS(ib, 0, 4)], RST,
						BLUE, BITS(ib, 8, 12), RST,
						CYAN, op_40_mem_sel[BITS(ib, 8, 12)], RST,
						BLUE, BITS(ib, 13, 15), RST,
						BLUE, BITS(ib, 16, 17), RST,
						BLUE, BITS(ib, 20, 21), RST,
						BLUE, BITS(ib, 25, 27), RST,
						BLUE, BITS(ib, 29, 30), RST);
					break;
				case 1:
					switch (BITS(decoder->pm4.next_write_mem.type, 0, 4)) {
						case 0: print("SRC_REG_OFFSET: %s", umr_reg_name(asic, BITS(ib, 0, 18))); break;
						case 5: print("IMM_DATA: %s0x%08lx%s", BLUE, (unsigned long)ib, RST); break;
						default: print("SRC_ADDR_LO: %s0x%08lx%s", YELLOW, (unsigned long)BITS(ib, 2, 32) << 2, RST); break;
					}
					break;
				case 2:
					if (BITS(decoder->pm4.next_write_mem.type, 0, 4) == 5 && BITS(decoder->pm4.next_write_mem.type, 16, 17) == 1)
						print("IMM_DATA_HI: %s0x%08lx%s", YELLOW, (unsigned long)ib, RST);
					else
						print("SRC_ADDR_HI: %s0x%08lx%s", YELLOW, (unsigned long)ib, RST);
					break;
				case 3:
					switch (BITS(decoder->pm4.next_write_mem.type, 0, 4)) {
						case 0: print("DST_REG_OFFSET: %s", umr_reg_name(asic, BITS(ib, 0, 18))); break;
						default: print("DST_ADDR_LO: %s0x%08lx%s", YELLOW, (unsigned long)ib, RST); break;
					}
					break;
				case 4:
					print("DST_ADDR_HI: %s0x%08lx%s", YELLOW, (unsigned long)ib, RST);
					break;
				default: print("Invalid word for opcode 0x%02lx", (unsigned long)decoder->pm4.cur_opcode);
			}
			break;
		case 0x43: // SURFACE_SYNC
			switch (decoder->pm4.cur_word) {
				case 0: print("ENGINE: %s%s%s, COHER_CNTL: %s0x%08lx%s",
						BLUE, BITS(ib, 31, 32) ? "ME" : "PFP", RST,
						BLUE, BITS(ib, 0, 29), RST);
					reg = umr_find_reg_data(asic, "mmCP_COHER_CNTL");
					if (reg && reg->bits) {
						int i, k;
						k = 0;
						print(" (");
						for (i = 0; i < reg->no_bits; i++) {
							if (ib & (1UL << reg->bits[i].start)) {
								print("%s%s%s%s", k ? ", " : "", RED, reg->bits[i].regname, RST);
								++k;
							}
						}
						print(")");
					}
					break;
				case 1:
					print("COHER_SIZE: %s0x%08lx%s", BLUE, (unsigned long)ib, RST);
					break;
				case 2:
					print("COHER_BASE: %s0x%08lx%s", YELLOW, (unsigned long)ib, RST);
					break;
				case 3:
					print("POLL_INTERVAL: %s%lu%s", BLUE, BITS(ib, 0, 16), RST);
					break;
				default: print("Invalid word for opcode 0x%02lx", (unsigned long)decoder->pm4.cur_opcode);
			}
			break;
		case 0x46: // EVENT_WRITE
			switch (decoder->pm4.cur_word) {
				case 0: print("EVENT_TYPE: %s0x%lx%s, EVENT_INDEX: %s0x%lx%s",
					       BLUE, (unsigned long)BITS(ib, 0, 6), RST,
					       BLUE, (unsigned long)BITS(ib, 8,12), RST);
					break;
				case 1: print("ADDRESS_LO: %s0x%08lx%s", YELLOW, (unsigned long)BITS(ib, 3, 32) << 3, RST);
					break;
				case 2: print("ADDRESS_HI: %s0x%08lx%s", YELLOW, (unsigned long)ib, RST);
					break;
				default: print("Invalid word for opcode 0x%02lx", (unsigned long)decoder->pm4.cur_opcode);
			}
			break;
		case 0x47: // EVENT_WRITE_EOP
			switch(decoder->pm4.cur_word) {
				case 0: print("INV_L2:%s%lu%s, EVENT_INDEX:%s%lu%s, EVENT_TYPE:%s%lu%s",
						BLUE, BITS(ib, 20, 21), RST,
						BLUE, BITS(ib, 8, 12), RST,
						BLUE, BITS(ib, 0, 6), RST);
					break;
				case 1: print("ADDRESS_LO: %s0x%08lx%s", YELLOW, BITS(ib, 2, 32) << 2, RST);
					break;
				case 2: print("DATA_SEL:%s%lu%s, INT_SEL:%s%lu%s, ADDRESS_HI: %s0x%08lx%s",
						BLUE, BITS(ib, 29, 32), RST,
						BLUE, BITS(ib, 24, 26), RST,
						YELLOW, BITS(ib, 0,  16), RST);
					break;
				case 3: print("DATA_LO: %s0x%08lx%s", BLUE, (unsigned long)ib, RST);
					break;
				case 4: print("DATA_HI: %s0x%08lx%s", BLUE, (unsigned long)ib, RST);
					break;
				default: print("Invalid word for opcode 0x%02lx", (unsigned long)decoder->pm4.cur_opcode);
			}
			break;
		case 0x49: // RELEASE_MEM
			switch(decoder->pm4.cur_word) {
				case 0: print("EVENT_TYPE: %s%lu%s [%s%s%s], EVENT_INDEX: %s%lu%s, TCL1_VOL_ACTION_ENA: %s%lu%s, TC_VOL_ACTION_ENA: %s%lu%s, TC_WB_ACTION_ENA: %s%lu%s, TCL1_ACTION_ENA: %s%lu%s, TC_ACTION_ENA: %s%lu%s, TC_NC_ACTION_ENA: %s%lu%s, TC_WC_ACTION_ENA: %s%lu%s, TC_MD_ACTION_ENA: %s%lu%s, CACHE_POLICY: %s%lu%s, EXECUTE: %s%lu%s",
						BLUE, BITS(ib, 0, 6), RST, CYAN, vgt_event_decode(BITS(ib, 0, 6)), RST,
						BLUE, BITS(ib, 8, 12), RST,
						BLUE, BITS(ib, 12, 13), RST,
						BLUE, BITS(ib, 13, 14), RST,
						BLUE, BITS(ib, 15, 16), RST,
						BLUE, BITS(ib, 16, 17), RST,
						BLUE, BITS(ib, 17, 18), RST,
						BLUE, BITS(ib, 19, 20), RST,
						BLUE, BITS(ib, 20, 21), RST,
						BLUE, BITS(ib, 21, 22), RST,
						BLUE, BITS(ib, 25, 27), RST,
						BLUE, BITS(ib, 28, 29), RST);
					break;
				case 1:
					decoder->pm4.next_write_mem.type = ib;
					print("DST_SEL: %s%lu%s, INT_SEL: %s%lu%s, DATA_SEL: %s%lu%s",
						BLUE, BITS(ib, 16, 18), RST,
						BLUE, BITS(ib, 24, 27), RST,
						BLUE, BITS(ib, 29, 32), RST);
					break;
				case 2: print("ADDR_LO: %s0x%08lx%s", YELLOW, (unsigned long)ib, RST);
					break;
				case 3: print("ADDR_HI: %s0x%08lx%s", YELLOW, (unsigned long)ib, RST);
					break;
				case 4:
					if (BITS(decoder->pm4.next_write_mem.type, 24, 27) == 5 ||
					    BITS(decoder->pm4.next_write_mem.type, 24, 27) == 6)
						print("CMP_DATA_LO: %s0x%08lx%s", BLUE, (unsigned long)ib, RST);
					else if (BITS(decoder->pm4.next_write_mem.type, 29, 32) == 5)
						print("DW_OFFSET: %s%lu%s, NUM_DWORDS: %s%lu%s",
							BLUE, BITS(ib, 0, 16), RST,
							BLUE, BITS(ib, 16, 32), RST);
					else
						print("DATA_LO: %s0x%08lx%s", BLUE, (unsigned long)ib, RST);
					break;
				case 5:
					if (BITS(decoder->pm4.next_write_mem.type, 24, 27) == 5 ||
					    BITS(decoder->pm4.next_write_mem.type, 24, 27) == 6)
						print("CMP_DATA_HI: %s0x%08lx%s", BLUE, (unsigned long)ib, RST);
					else
						print("DATA_HI: %s0x%08lx%s", BLUE, (unsigned long)ib, RST);
					break;
				case 6:
					if (asic->family >= FAMILY_AI) {
						// decode additional words
						print("INT_CTXID: %s0x%08lx%s", BLUE, (unsigned long)ib, RST);
						break;
					} else {
						print("Invalid word for opcode 0x%02lx", (unsigned long)decoder->pm4.cur_opcode);
					}
					break;
				default: print("Invalid word for opcode 0x%02lx", (unsigned long)decoder->pm4.cur_opcode);
			}
			break;
		case 0x4C: // DISPATCH_MESH_INDIRECT_MULTI
			switch(decoder->pm4.cur_word) {
				case 0: print("DATA_OFFSET: %s0x%lx%s", BLUE, (unsigned long)ib, RST); break;
				case 1: print("XYZ_DIM_LOC: %s0x%lx%s (%s%s%s), RING_ENTRY_LOC: %s0x%lx%s (%s%s%s)",
							   BLUE, (unsigned long)BITS(ib, 0, 16), RST, RED, umr_reg_name(asic, BITS(ib, 0, 16) + 0x2C00), RST,
							   BLUE, (unsigned long)BITS(ib, 16, 32), RST, RED, umr_reg_name(asic, BITS(ib, 16, 32) + 0x2C00), RST);
						break;
				case 2: print("USE_VGPRS: %s%lu%s, THREAD_TRACE_MARKER_ENABLE: %s%lu%s, COUNT_INDIRECT_ENABLE: %s%lu%s, DRAW_INDEX_ENABLE: %s%lu%s",
							   BLUE, (unsigned long)BITS(ib, 28, 29), RST,
							   BLUE, (unsigned long)BITS(ib, 29, 30), RST,
							   BLUE, (unsigned long)BITS(ib, 30, 31), RST,
							   BLUE, (unsigned long)BITS(ib, 31, 32), RST);
						break;
				case 3: print("COUNT: %s%lu%s", BLUE, (unsigned long)ib, RST); break;
				case 4: print("COUNT_ADDR_LO: %s0x%lx%s", YELLOW, (unsigned long)BITS(ib, 2, 32) << 2, RST); break;
				case 5: print("COUNT_ADDR_HI: %s0x%lx%s", YELLOW, (unsigned long)ib, RST); break;
				case 6: print("STRIDE: %s%lu%s", BLUE, (unsigned long)ib, RST); break;
				case 7: print("DRAW_INITIATOR: %s0x%lx%s", BLUE, (unsigned long)ib, RST); break;
			}
			break;
		case 0x4D: // DISPATCH_TASKMESH_GFX
			switch(decoder->pm4.cur_word) {
				case 0: print("XYZ_DIM_LOC: %s0x%lx%s (%s%s%s), RING_ENTRY_LOC: %s0x%lx%s (%s%s%s)",
							   BLUE, (unsigned long)BITS(ib, 0, 16), RST, RED, umr_reg_name(asic, BITS(ib, 0, 16) + 0x2C00), RST,
							   BLUE, (unsigned long)BITS(ib, 16, 32), RST, RED, umr_reg_name(asic, BITS(ib, 16, 32) + 0x2C00), RST);
						break;
				case 1: print("THREAD_TRACE_MARKER_ENABLE: %s%lu%s", BLUE, (unsigned long)BITS(ib, 31, 32), RST); break;
				case 2: print("DRAW_INITIATOR: %s0x%lx%s", BLUE, (unsigned long)ib, RST); break;
			}
			break;
		case 0x50: // DMA_DATA
			switch(decoder->pm4.cur_word) {
				case 0: print("ENG_SEL: %s%d%s, SRC_CACHE: %s%d%s, DST_SEL: %s%d%s, DST_CACHE: %s%d%s, SRC_SEL: %s%d%s, CP_SYNC: %s%d%s",
						BLUE, (int)BITS(ib, 0, 1), RST,
						BLUE, (int)BITS(ib, 1+12, 1+12+2), RST,
						BLUE, (int)BITS(ib, 1+12+2+5,1+12+2+5+2), RST,
						BLUE, (int)BITS(ib, 1+12+2+5+2+3, 1+12+2+5+2+3+2), RST,
						BLUE, (int)BITS(ib, 1+12+2+5+2+3+2+2, 1+12+2+5+2+3+2+2+2), RST,
						BLUE, (int)BITS(ib, 1+12+2+5+2+3+2+2+2, 1+12+2+5+2+3+2+2+2+1), RST);
					break;
				case 1: print("SRC_ADDR_LO_OR_DATA: %s0x%08lx%s", YELLOW, (unsigned long)BITS(ib, 0, 32), RST); break;
				case 2: print("SRC_ADDR_HI: %s0x%08lx%s", YELLOW, (unsigned long)BITS(ib, 0, 32), RST); break;
				case 3: print("DST_ADDR_LO: %s0x%08lx%s", YELLOW, (unsigned long)BITS(ib, 0, 32), RST); break;
				case 4: print("DST_ADDR_HI: %s0x%08lx%s", YELLOW, (unsigned long)BITS(ib, 0, 32), RST); break;
				case 5:
						if (asic->family <= FAMILY_VI) {
							print("BYTE COUNT: %s%lu%s, SAS: %s%d%s, DAS: %s%d%s, SAIC: %s%d%s, DAIC: %s%d%s, RAW_WAIT: %s%d%s, DIS_WC: %s%d%s",
							BLUE, (unsigned long)BITS(ib, 0, 26), RST,
							BLUE, (int)BITS(ib, 26, 26+1), RST,
							BLUE, (int)BITS(ib, 26+1, 26+1+1), RST,
							BLUE, (int)BITS(ib, 26+1+1, 26+1+1+1), RST,
							BLUE, (int)BITS(ib, 26+1+1+1, 26+1+1+1+1), RST,
							BLUE, (int)BITS(ib, 26+1+1+1+1, 26+1+1+1+1+1), RST,
							BLUE, (int)BITS(ib, 26+1+1+1+1+1, 26+1+1+1+1+1+1), RST);
						} else {
							// .. AI or above
							print("BYTE COUNT: %s%lu%s, DIS_WC: %s%d%s, SAS: %s%d%s, DAS: %s%d%s, SAIC: %s%d%s, DAIC: %s%d%s, RAW_WAIT: %s%d%s",
							BLUE, (unsigned long)BITS(ib, 0, 21), RST,
							BLUE, (int)BITS(ib, 21, 21+1), RST,
							BLUE, (int)BITS(ib, 26, 26+1), RST,
							BLUE, (int)BITS(ib, 26+1, 26+1+1), RST,
							BLUE, (int)BITS(ib, 26+1+1, 26+1+1+1), RST,
							BLUE, (int)BITS(ib, 26+1+1+1, 26+1+1+1+1), RST,
							BLUE, (int)BITS(ib, 26+1+1+1+1, 26+1+1+1+1+1), RST);
						}
					break;
				default: print("Invalid word for opcode 0x%02lx", (unsigned long)decoder->pm4.cur_opcode);
			}
			break;
		case 0x51: // CONTEXT_REG_RMW
			switch (decoder->pm4.cur_word) {
				case 0: print("REG: %s", umr_reg_name(asic, BITS(ib, 0, 16) + 0xA000));
					break;
				case 1: print("MASK: %s0x%08lx%s", BLUE, (unsigned long)ib, RST);
					break;
				case 2: print("DATA: %s0x%08lx%s", BLUE, (unsigned long)ib, RST);
					break;
				default: print("Invalid word for opcode 0x%02lx", (unsigned long)decoder->pm4.cur_opcode);
			}
			break;
		case 0x58: // ACQUIRE_MEM
			switch(decoder->pm4.cur_word) {
				case 0: print("ENGINE: %s%s%s, COHER_CNTL: %s0x%08lx%s",
						BLUE, BITS(ib, 31, 32) ? "ME" : "PFP", RST,
						BLUE, BITS(ib, 0, 29), RST);
					reg = umr_find_reg_data(asic, "mmCP_COHER_CNTL");
					if (reg && reg->bits) {
						int i, k;
						k = 0;
						print(" (");
						for (i = 0; i < reg->no_bits; i++) {
							if (ib & (1UL << reg->bits[i].start)) {
								print("%s%s%s%s", k ? ", " : "", RED, reg->bits[i].regname, RST);
								++k;
							}
						}
						print(")");
					}
					break;
				case 1: print("CP_COHER_SIZE: %s0x%08lx%s", BLUE, (unsigned long)ib, RST); break;
				case 2: print("CP_COHER_SIZE_HI: %s0x%08lx%s", BLUE, (unsigned long)ib, RST); break;
				case 3: print("CP_COHER_BASE: %s0x%08lx%s", YELLOW, (unsigned long)ib, RST); break;
				case 4: print("CP_COHER_BASE_HI: %s0x%08lx%s", YELLOW, (unsigned long)ib, RST); break;
				case 5: print("POLL_INTERVAL: %s0x%08lx%s", BLUE, (unsigned long)ib, RST); break;
			}
			break;
		case 0x5F: // LOAD_SH_REG
			switch(decoder->pm4.cur_word) {
				case 0: decoder->pm4.next_write_mem.addr_lo = ib << 2;
						print("BASE_ADDRESS_LO: %s0x%lx%s", YELLOW, (unsigned long)BITS(ib, 2, 32) << 2, RST);
						break;
				case 1: decoder->pm4.next_write_mem.addr_hi = ib;
						print("BASE_ADDRESS_HI: %s0x%lx%s", YELLOW, (unsigned long)ib, RST);
						break;
				default:
					if (!(decoder->pm4.cur_word & 1)) {
						print("REG_OFFSET: %s%s%s (%s0x%lx%s)", RED, umr_reg_name(asic, 0x2c00 + BITS(ib, 0, 16)), RST, BLUE, BITS(ib, 0, 16), RST);
					} else {
						print("NUM_DWORD: %s%lu%s", BLUE, (unsigned long)BITS(ib, 0, 14), RST);
					}
					break;
			}
			break;
		case 0x63: // LOAD_SH_REG_INDEX
			switch(decoder->pm4.cur_word) {
				case 0: decoder->pm4.next_write_mem.addr_lo = BITS(ib, 0, 31) & ~0x3UL;
					decoder->pm4.next_write_mem.type = BITS(ib, 0, 1); // INDEX bit
					if (BITS(ib, 0, 1))
						print("INDEX: %s1%s", BLUE, RST);
					else
						print("MEM_ADDR_LO: %s0x%lx%s",
							YELLOW, (unsigned long)decoder->pm4.next_write_mem.addr_lo, RST);
					break;
				case 1: decoder->pm4.next_write_mem.addr_lo = BITS(ib, 0, 32);
					if (decoder->pm4.next_write_mem.type) // INDEX bit
						print("SH_BASE_ADDR: %s0x%lx%s", YELLOW, (unsigned long)decoder->pm4.next_write_mem.addr_lo, RST);
					else
						print("MEM_ADDR_HI: %s0x%lx%s", YELLOW, (unsigned long)decoder->pm4.next_write_mem.addr_lo, RST);
					break;
				case 2: decoder->pm4.next_write_mem.type |= BITS(ib, 31, 32) << 1; // DATA_FORMAT
					if (decoder->pm4.next_write_mem.type & 2)
						print("REG: %s\n", umr_reg_name(asic, 0x2c00 + BITS(ib, 0, 16)));
					else
						print("REG: (ignored)\n");
					break;
				case 3: print("NUM_DWORDS: %s0x%lx%s\n", BLUE, (unsigned long)BITS(ib, 0, 14), RST);
					break;
				default:
					if (decoder->pm4.next_write_mem.type & 2) {
						print("%s <= %s0x%08lx%s", umr_reg_name(asic, decoder->pm4.next_write_mem.addr_lo++), YELLOW, (unsigned long)ib, RST);
						print_bits(asic, decoder->pm4.next_write_mem.addr_lo - 1, ib, 0, print);
					} else {
						print("DATA: %s0x%lx%s\n", BLUE, (unsigned long)ib, RST);
					}
					break;
			}
			break;
		case 0x68: // SET_CONFIG_REG
			switch(decoder->pm4.cur_word) {
				case 0: decoder->pm4.next_write_mem.addr_lo = BITS(ib, 0, 16) + 0x2000;
					print("OFFSET: %s0x%lx%s", BLUE, (unsigned long)BITS(ib, 0, 16), RST);
					break;
				default: print("%s <= %s0x%08lx%s", umr_reg_name(asic, decoder->pm4.next_write_mem.addr_lo++), YELLOW, (unsigned long)ib, RST);
					print_bits(asic, decoder->pm4.next_write_mem.addr_lo - 1, ib, 0, print);
					break;
			}
			break;
		case 0x69: // SET_CONTEXT_REG
			switch(decoder->pm4.cur_word) {
				case 0: decoder->pm4.next_write_mem.addr_lo = BITS(ib, 0, 16) + 0xA000;
					print("OFFSET: %s0x%lx%s", BLUE, (unsigned long)BITS(ib, 0, 16), RST);
					break;
				default: print("%s <= %s0x%08lx%s", umr_reg_name(asic, decoder->pm4.next_write_mem.addr_lo++), YELLOW, (unsigned long)ib, RST);
					print_bits(asic, decoder->pm4.next_write_mem.addr_lo - 1, ib, 0, print);
					break;
			}
			break;
		case 0x76: // SET_SH_REG
			switch(decoder->pm4.cur_word) {
				case 0: decoder->pm4.next_write_mem.addr_lo = BITS(ib, 0, 16) + 0x2C00;
					print("OFFSET: %s0x%lx%s", BLUE, (unsigned long)BITS(ib, 0, 16), RST);
					break;
				default:
					{
						char *tmp = umr_reg_name(asic, decoder->pm4.next_write_mem.addr_lo);
						print("%s <= %s0x%08lx%s", tmp, YELLOW, (unsigned long)ib, RST);
						if (asic->family <= FAMILY_VI) {
							if (strstr(tmp, "SPI_SHADER_PGM_LO_") || strstr(tmp, "COMPUTE_PGM_LO")) {
								decoder->pm4.next_ib_state.ib_addr_lo = ib;
							} else if (strstr(tmp, "SPI_SHADER_PGM_HI_") || strstr(tmp, "COMPUTE_PGM_HI")) {
								decoder->pm4.next_ib_state.ib_addr_hi = ib;
								decoder->pm4.next_ib_state.ib_vmid = decoder->next_ib_info.vmid;
								if (!asic->options.no_follow_ib) {
									if (umr_read_vram(asic, asic->options.vm_partition, decoder->pm4.next_ib_state.ib_vmid,
													  (((uint64_t)decoder->pm4.next_ib_state.ib_addr_hi << 32) | decoder->pm4.next_ib_state.ib_addr_lo) << 8,
													  4, buf) < 0) {
										print(" [%sUNMAPPED%s]", RED, RST);
									} else {
										print(" [%sMAPPED%s]", GREEN, RST);
										add_shader(asic, decoder);
									}
								}
							}
						} else {
							// on gfx9+ HS and GS shaders are opaque
							if ((strstr(tmp, "SPI_SHADER_PGM_LO_") || strstr(tmp, "COMPUTE_PGM_LO")) &&
								 (asic->options.shader_enable.enable_hs_shader || strcmp(tmp, "SPI_SHADER_PGM_LO_HS")) &&
								 (asic->options.shader_enable.enable_gs_shader || strcmp(tmp, "SPI_SHADER_PGM_LO_GS"))) {
								decoder->pm4.next_ib_state.ib_addr_lo = ib;
							} else if ((strstr(tmp, "SPI_SHADER_PGM_HI_") || strstr(tmp, "COMPUTE_PGM_HI")) &&
								 strcmp(tmp, "SPI_SHADER_PGM_HI_HS") && strcmp(tmp, "SPI_SHADER_PGM_HI_GS")) {
								decoder->pm4.next_ib_state.ib_addr_hi = ib;
								decoder->pm4.next_ib_state.ib_vmid = decoder->next_ib_info.vmid;
								if (!asic->options.no_follow_ib) {
									if (umr_read_vram(asic, asic->options.vm_partition, decoder->pm4.next_ib_state.ib_vmid,
													  (((uint64_t)decoder->pm4.next_ib_state.ib_addr_hi << 32) | decoder->pm4.next_ib_state.ib_addr_lo) << 8,
													  4, buf) < 0) {
										print(" [%sUNMAPPED%s]", RED, RST);
									} else {
										print(" [%sMAPPED%s]", GREEN, RST);
										add_shader(asic, decoder);
									}
								}
							}
						}
						print_bits(asic, decoder->pm4.next_write_mem.addr_lo++, ib, 0, print);
					}
					break;
			}
			break;
		case 0x79: // SET_UCONFIG_REG
			switch(decoder->pm4.cur_word) {
				case 0: decoder->pm4.next_write_mem.addr_lo = BITS(ib, 0, 16) + 0xC000;
					print("OFFSET: %s0x%lx%s", BLUE, (unsigned long)BITS(ib, 0, 16), RST);
					break;
				default: print("%s <= %s0x%08lx%s", umr_reg_name(asic, decoder->pm4.next_write_mem.addr_lo++), YELLOW, (unsigned long)ib, RST);
					print_bits(asic, decoder->pm4.next_write_mem.addr_lo - 1, ib, 0, print);
					break;
			}
			break;
		case 0x7A: // SET_UCONFIG_REG_INDEX
			switch(decoder->pm4.cur_word) {
				case 0: decoder->pm4.next_write_mem.addr_lo = BITS(ib, 0, 16) + 0xC000;
					print("OFFSET: %s0x%lx%s, INDEX: [%s%s%s]",
						BLUE, (unsigned long)BITS(ib, 0, 16), RST,
						CYAN, op_7a_index_str[BITS(ib, 28, 32)], RST);
					break;
				default: print("%s <= %s0x%08lx%s", umr_reg_name(asic, decoder->pm4.next_write_mem.addr_lo++), YELLOW, (unsigned long)ib, RST);
					print_bits(asic, decoder->pm4.next_write_mem.addr_lo - 1, ib, 0, print);
					break;
			}
			break;
		case 0x80: // LOAD_CONST_RAM
			switch(decoder->pm4.cur_word) {
				case 0: print("ADDR_LO: %s0x%08lx%s", YELLOW, (unsigned long)ib, RST);
					break;
				case 1: print("ADDR_HI: %s0x%08lx%s", YELLOW, (unsigned long)ib, RST);
					break;
				case 2: print("NUM_DW: %s0x%08lx%s", BLUE, (unsigned long)BITS(ib, 0, 15), RST);
					break;
				case 3: print("START_ADDR: %s0x%08lx%s, CACHE_POLICY: %s%s%s",
					       YELLOW, (unsigned long)BITS(ib, 0, 16), RST,
					       CYAN, BITS(ib, 25, 27) ? "stream" : "lru", RST);
					break;
				default: print("Invalid word for opcode 0x%02lx", (unsigned long)decoder->pm4.cur_opcode);
			}
			break;
		case 0x81: // WRITE_CONST_RAM
			switch(decoder->pm4.cur_word) {
				case 0: decoder->pm4.next_write_mem.addr_lo = BITS(ib, 0, 16);
					print("OFFSET: %s0x%lx%s", YELLOW, (unsigned long)BITS(ib, 0, 16), RST);
					break;
				default: print("CONST_RAM[%s0x%lx%s] <= %s0x%08lx%s",
						BLUE, (unsigned long)decoder->pm4.next_write_mem.addr_lo, RST,
						YELLOW, (unsigned long)ib, RST);
					 decoder->pm4.next_write_mem.addr_lo += 4;
					break;
			}
			break;
		case 0x83: // DUMP_CONST_RAM
			switch(decoder->pm4.cur_word) {
				case 0: print("OFFSET: %s0x%lx%s, CACHE_POLICY: [%s%s%s], INC_CE: %s%d%s, INC_CS: %s%d%s",
						YELLOW, (unsigned long)BITS(ib, 0, 16), RST,
						CYAN, BITS(ib, 25, 26) ? "stream" : "lru", RST,
						BLUE, (int)BITS(ib, 30, 31), RST,
						BLUE, (int)BITS(ib, 31, 32), RST);
					break;
				case 1:
					print("NUM_DW: %s0x%lx%s", BLUE, (unsigned long)BITS(ib, 0, 15), RST);
					break;
				case 2:
					print("ADDR_LO: %s0x%08lx%s", YELLOW, (unsigned long)ib, RST);
					break;
				case 3:
					print("ADDR_HI: %s0x%08lx%s", YELLOW, (unsigned long)ib, RST);
					break;
				default: print("Invalid word for opcode 0x%02lx", (unsigned long)decoder->pm4.cur_opcode);
			}
			break;
		case 0x84: // INCREMENT_CE_COUNTER
			switch(decoder->pm4.cur_word) {
				case 0: print("CNTRSEL: [%s%s%s]",
						CYAN, op_84_cntr_sel[BITS(ib, 0, 2)], RST);
					break;
				default: print("Invalid word for opcode 0x%02lx", (unsigned long)decoder->pm4.cur_opcode);
			}
			break;
		case 0x86: // WAIT_ON_CE_COUNTER
			switch(decoder->pm4.cur_word) {
				case 0: print("COND_ACQUIRE_MEM: %s%d%s, FORCE_SYNC: %s%d%s, MEM_VOLATILE: %s%d%s",
						BLUE, (int)BITS(ib, 0, 1), RST,
						BLUE, (int)BITS(ib, 1, 2), RST,
						BLUE, (int)BITS(ib, 27, 28), RST);
					break;
				default: print("Invalid word for opcode 0x%02lx", (unsigned long)decoder->pm4.cur_opcode);
			}
			break;
		case 0x90: // FRAME_CONTROL
			switch(decoder->pm4.cur_word) {
				case 0: print("TMZ: %s%lu%s, COMMAND: %s%lu%s",
						BLUE, BITS(ib, 0, 1), RST,
						BLUE, BITS(ib, 28, 32), RST);
					break;
				default: print("Invalid word for opcode 0x%02lx", (unsigned long)decoder->pm4.cur_opcode);
			}
			break;
		case 0x91: // INDEX_ATTRIBUTES_INDIRECT
			switch(decoder->pm4.cur_word) {
				case 0: print("ATTRIBUTE_BASE_LO: %s0x%lx%s", YELLOW, (unsigned long)BITS(ib, 4, 32) << 4, RST); break;
				case 1: print("ATTRIBUTE_BASE_HI: %s0x%lx%s", YELLOW, (unsigned long)ib, RST); break;
				case 2: print("ATTRIBUTE_INDEX: %s%lu%s", BLUE, (unsigned long)BITS(ib, 0, 16), RST); break;
				case 3: print("INDEX_BASE_LO: %s0x%lx%s", YELLOW, (unsigned long)ib, RST); break;
				case 4: print("INDEX_BASE_HI: %s0x%lx%s", YELLOW, (unsigned long)ib, RST); break;
				case 5: print("INDEX_BUFFER_SIZE: %s%lu%s", BLUE, (unsigned long)ib, RST); break;
				case 6: print("INDEX_TYPE: %s%lu%s", BLUE, (unsigned long)ib, RST); break;
			}
			break;
		case 0x9A: // DMA_DATA_FILL_MULTI
			switch(decoder->pm4.cur_word) {
				case 0: print("ENGINE_SEL: %s%lu%s, MEMLOG_CLEAR: %s%lu%s, DST_SEL: %s%lu%s, DST_CACHE_POLICY: %s%lu%s, SRC_SEL: %s%lu%s, CP_SYNC: %s%lu%s",
						BLUE, (unsigned long)BITS(ib, 0, 1), RST,
						BLUE, (unsigned long)BITS(ib, 10, 11), RST,
						BLUE, (unsigned long)BITS(ib, 20, 22), RST,
						BLUE, (unsigned long)BITS(ib, 25, 27), RST,
						BLUE, (unsigned long)BITS(ib, 29, 31), RST,
						BLUE, (unsigned long)BITS(ib, 31, 32), RST);
					break;
				case 1: print("BYTE_STRIDE: %s%lu%s", BLUE, (unsigned long)BITS(ib, 0, 32), RST);
					break;
				case 2: print("DMA_COUNT: %s%lu%s", BLUE, (unsigned long)BITS(ib, 0, 32), RST);
					break;
				case 3: print("DST_ADDR_LO: %s0x%lx%s", YELLOW, (unsigned long)BITS(ib, 0, 32), RST);
					break;
				case 4: print("DST_ADDR_HI: %s0x%lx%s", YELLOW, (unsigned long)BITS(ib, 0, 32), RST);
					break;
				case 5: print("BYTE_COUNT: %s%lu%s", BLUE, (unsigned long)BITS(ib, 0, 26), RST);
					break;
				default: print("Invalid word for opcode 0x%02lx", (unsigned long)decoder->pm4.cur_opcode);
			}
			break;
		case 0x9B: // SET_SH_REG_INDEX
			switch(decoder->pm4.cur_word) {
				case 0:
					decoder->pm4.next_write_mem.addr_lo = BITS(ib, 0, 16) + 0x2C00;
					print("REG_OFFSET: %s0x%lx%s, INDEX: %s%lu%s",
						BLUE, (unsigned long)decoder->pm4.next_write_mem.addr_lo, RST,
						BLUE, BITS(ib, 28, 32), RST);
					break;
				default: print("%s <= %s0x%08lx%s", umr_reg_name(asic, decoder->pm4.next_write_mem.addr_lo++), YELLOW, (unsigned long)ib, RST);
					print_bits(asic, decoder->pm4.next_write_mem.addr_lo - 1, ib, 0, print);
					break;
			}
			break;
		case 0x9F: // LOAD_CONTEXT_REG_INDEX
			switch(decoder->pm4.cur_word) {
				case 0: decoder->pm4.next_write_mem.addr_lo = BITS(ib, 0, 32) & ~0x3UL;
					decoder->pm4.next_write_mem.type = BITS(ib, 0, 1); // INDEX bit
					if (BITS(ib, 0, 1))
						print("INDEX: %s1%s", BLUE, RST);
					else
						print("MEM_ADDR_LO: %s0x%lx%s",
							YELLOW, (unsigned long)decoder->pm4.next_write_mem.addr_lo, RST);
					break;
				case 1: decoder->pm4.next_write_mem.addr_lo = BITS(ib, 0, 32);
					if (decoder->pm4.next_write_mem.type) // INDEX bit
						print("CONTEXT_BASE_ADDR: %s0x%lx%s", YELLOW, (unsigned long)decoder->pm4.next_write_mem.addr_lo, RST);
					else
						print("MEM_ADDR_HI: %s0x%lx%s", YELLOW, (unsigned long)decoder->pm4.next_write_mem.addr_lo, RST);
					break;
				case 2: decoder->pm4.next_write_mem.type |= BITS(ib, 31, 32) << 1; // DATA_FORMAT
					if (decoder->pm4.next_write_mem.type & 2)
						print("REG: %s\n", umr_reg_name(asic, 0xA000 + BITS(ib, 0, 16)));
					else
						print("REG: (ignored)\n");
					break;
				case 3: print("NUM_DWORDS: %s0x%lx%s", BLUE, (unsigned long)BITS(ib, 0, 14), RST);
					break;
				default:
					if (decoder->pm4.next_write_mem.type & 2) {
						print("%s <= %s0x%08lx%s", umr_reg_name(asic, decoder->pm4.next_write_mem.addr_lo++), YELLOW, (unsigned long)ib, RST);
						print_bits(asic, decoder->pm4.next_write_mem.addr_lo - 1, ib, 0, print);
					} else {
						print("DATA: %s0x%lx%s", BLUE, (unsigned long)ib, RST);
					}
					break;
			}
			break;
		case 0xA0: // SET_RESOURCES
			switch(decoder->pm4.cur_word) {
				case 0: print("VMID_MASK: %s0x%lx%s, UNMAP_LATENCY: %s%lu%s, QUEUE_TYPE: %s%lu%s",
							BLUE, (unsigned long)BITS(ib, 0, 16), RST,
							BLUE, (unsigned long)BITS(ib, 16, 24), RST,
							BLUE, (unsigned long)BITS(ib, 29, 32), RST);
						break;
				case 1: print("QUEUE_MASK_LO: %s0x%lx%s", YELLOW, (unsigned long)ib, RST); break;
				case 2: print("QUEUE_MASK_HI: %s0x%lx%s", YELLOW, (unsigned long)ib, RST); break;
				case 3: print("GWS_MASK_LO: %s0x%lx%s", YELLOW, (unsigned long)ib, RST); break;
				case 4: print("GWS_MASK_HI: %s0x%lx%s", YELLOW, (unsigned long)ib, RST); break;
				case 5: print("OAC_MASK: %s0x%lx%s", YELLOW, (unsigned long)BITS(ib, 0, 16), RST); break;
				case 6: print("GDS_HEAP_BASE: %s%lu%s, GDS_HEAP_SIZE: %s%lu%s",
							BLUE, (unsigned long)BITS(ib, 0, 6), RST,
							BLUE, (unsigned long)BITS(ib, 11, 17), RST);
						break;
			}
			break;
		case 0xA1: // PKT3_MAP_PROCESS
			if (asic->family <= FAMILY_VI) {
				switch(decoder->pm4.cur_word) {
					case 0:
						print("PASID: %s%u%s, DIQ_ENABLE: %s%u%s",
							BLUE, (unsigned)BITS(ib, 0, 16), RST,
							BLUE, (unsigned)BITS(ib, 24, 25), RST);
						break;
					case 1: print("PAGE_TABLE_BASE: %s0x%lx%s", YELLOW, (unsigned long)BITS(ib, 0, 28), RST); break;
					case 2: print("SH_MEM_BASES: %s0x%lx%s", YELLOW, (unsigned long)ib, RST); break;
					case 3: print("SH_MEM_APE1_BASE: %s0x%lx%s", YELLOW, (unsigned long)ib, RST); break;
					case 4: print("SH_MEM_APE1_LIMIT: %s0x%lx%s", YELLOW, (unsigned long)ib, RST); break;
					case 5: print("GDS_ADDR_LO: %s0x%lx%s", YELLOW, (unsigned long)ib, RST); break;
					case 6: print("GDS_ADDR_HI: %s0x%lx%s", YELLOW, (unsigned long)ib, RST); break;
					case 7:
						print("NUM_GWS: %s%u%s, NUM_OAC: %s%u%s, GDS_SIZE: %s%u%s",
							BLUE, (unsigned)BITS(ib, 0, 6), RST,
							BLUE, (unsigned)BITS(ib, 8, 12), RST,
							BLUE, (unsigned)BITS(ib, 16, 22), RST);
						break;
				}
			} else if (asic->family <= FAMILY_NV) {
				switch(decoder->pm4.cur_word) {
					case 0:
						print("PASID: %s%u%s, DEBUG_VMID: %s%u%s, DEBUG_FLAG: %s%u%s",
							BLUE, (unsigned)BITS(ib, 0, 16), RST,
							BLUE, (unsigned)BITS(ib, 18, 22), RST,
							BLUE, (unsigned)BITS(ib, 22, 23), RST);
						if (asic->family < FAMILY_NV)
							print(", TMZ: %s%u%s", BLUE, (unsigned)BITS(ib, 23, 24), RST);
						print(", DIQ_ENABLE: %s%u%s, PROCESS_QUANTUM: %s%u%s",
							BLUE, (unsigned)BITS(ib, 24, 25), RST,
							BLUE, (unsigned)BITS(ib, 25, 32), RST);
						break;
					case 1: print("VM_CONTEXT_PAGE_TABLE_BASE_ADDR_LO32: %s0x%lx%s", YELLOW, (unsigned long)ib, RST); break;
					case 2: print("VM_CONTEXT_PAGE_TABLE_BASE_ADDR_HI32: %s0x%lx%s", YELLOW, (unsigned long)ib, RST); break;
					case 3: print("SH_MEM_BASES: %s0x%lx%s", YELLOW, (unsigned long)ib, RST); break;
					case 4: print("SH_MEM_CONFIG: %s0x%lx%s", YELLOW, (unsigned long)ib, RST); break;
					case 5: print("SQ_SHADER_TBA_LO: %s0x%lx%s", YELLOW, (unsigned long)ib, RST); break;
					case 6: print("SQ_SHADER_TBA_HI: %s0x%lx%s", YELLOW, (unsigned long)ib, RST); break;
					case 7: print("SQ_SHADER_TMA_LO: %s0x%lx%s", YELLOW, (unsigned long)ib, RST); break;
					case 8: print("SQ_SHADER_TMA_HI: %s0x%lx%s", YELLOW, (unsigned long)ib, RST); break;
					case 9: print("RESERVED"); break;
					case 10: print("GDS_ADDR_LO: %s0x%lx%s", YELLOW, (unsigned long)ib, RST); break;
					case 11: print("GDS_ADDR_HI: %s0x%lx%s", YELLOW, (unsigned long)ib, RST); break;
					case 12:
						print("NUM_GWS: %s%u%s, SDMA_ENABLE: %s%u%s, NUM_OAC: %s%u%s, GDS_SIZE: %s%u%s, NUM_QUEUES: %s%u%s",
							BLUE, (unsigned)BITS(ib, 0, 6), RST,
							BLUE, (unsigned)BITS(ib, 7, 8), RST,
							BLUE, (unsigned)BITS(ib, 8, 12), RST,
							BLUE, (unsigned)BITS(ib, 16, 22), RST,
							BLUE, (unsigned)BITS(ib, 22, 23), RST);
						break;
					case 13: print("COMPLETION_SIGNAL_LO32: %s0x%lx%s", YELLOW, (unsigned long)ib, RST); break;
					case 14: print("COMPLETION_SIGNAL_HI32: %s0x%lx%s", YELLOW, (unsigned long)ib, RST); break;
				}
			}
			break;
		case 0xA2: // MAP_QUEUES
			if (asic->family <= FAMILY_VI) {
				switch(decoder->pm4.cur_word) {
					case 0:
						print("QUEUE_SEL: %s%u%s, VMID: %s%u%s, VIDMEM: %s%u%s, ALLOC_FORMAT: %s%u%s, ENGINE_SEL: %s%u%s, NUM_QUEUES: %s%u%s",
							BLUE, (unsigned)BITS(ib, 4, 6), RST,
							BLUE, (unsigned)BITS(ib, 8, 12), RST,
							BLUE, (unsigned)BITS(ib, 16, 18), RST,
							BLUE, (unsigned)BITS(ib, 24, 26), RST,
							BLUE, (unsigned)BITS(ib, 26, 29), RST,
							BLUE, (unsigned)BITS(ib, 29, 32), RST);
// TODO: is this only for QUEUE_SEL=0?
						decoder->pm4.next_ib_state.ib_vmid = decoder->next_ib_info.vmid ? decoder->next_ib_info.vmid : BITS(ib, 8, 12);
						decoder->pm4.next_ib_state.ib_size = BITS(ib, 26, 29);
						break;
					case 1:
						print("DOORBELL_OFFSET: %s0x%lx%s, QUEUE: %s%u%s",
							YELLOW, (unsigned long)BITS(ib, 2, 23), RST,
							BLUE, (unsigned)BITS(ib, 26, 32), RST);
						break;
				}
				if (decoder->pm4.cur_word > 1) {
					switch ((decoder->pm4.cur_word - 2) % 4) {
						case 0: print("MQD_ADDR_LO: %s0x%lx%s", YELLOW, (unsigned long)ib, RST);
								decoder->pm4.next_ib_state.ib_addr_lo = ib;
								break;
						case 1: print("MQD_ADDR_HI: %s0x%lx%s", YELLOW, (unsigned long)ib, RST);
								decoder->pm4.next_ib_state.ib_addr_hi = ib;
								break;
						case 2: print("WPTR_ADDR_LO: %s0x%lx%s", YELLOW, (unsigned long)ib, RST);
								break;
						case 3: print("WPTR_ADDR_HI: %s0x%lx%s", YELLOW, (unsigned long)ib, RST);
								if (!asic->options.no_follow_ib) {
									if (umr_read_vram(asic, asic->options.vm_partition, decoder->pm4.next_ib_state.ib_vmid,
													  ((uint64_t)decoder->pm4.next_ib_state.ib_addr_hi << 32) | decoder->pm4.next_ib_state.ib_addr_lo, 4, buf) < 0) {
										print(" [%sUNMAPPED%s]", RED, RST);
									} else {
										print(" [%sMAPPED%s]", GREEN, RST);
										add_data_block_pm4(asic, decoder, UMR_DATABLOCK_MQD_VI);
									}
								}
								break;
					}
				}
			} else if (asic->family <= FAMILY_NV) {
				switch(decoder->pm4.cur_word) {
					case 0:
						print("QUEUE_SEL: %s%u%s, VMID: %s%u%s, QUEUE: %s%u%s, QUEUE_TYPE: %s%u%s, STATIC_QUEUE_GROUP: %s%u%s, ENGINE_SEL: %s%u%s, NUM_QUEUES: %s%u%s",
							BLUE, (unsigned)BITS(ib, 4, 6), RST,
							BLUE, (unsigned)BITS(ib, 8, 12), RST,
							BLUE, (unsigned)BITS(ib, 13, 21), RST,
							BLUE, (unsigned)BITS(ib, 21, 24), RST,
							BLUE, (unsigned)BITS(ib, 24, 26), RST,
							BLUE, (unsigned)BITS(ib, 26, 29), RST,
							BLUE, (unsigned)BITS(ib, 29, 32), RST);
// TODO: is this only for QUEUE_SEL=0?
						decoder->pm4.next_ib_state.ib_vmid = decoder->next_ib_info.vmid ? decoder->next_ib_info.vmid : BITS(ib, 8, 12);
						decoder->pm4.next_ib_state.ib_size = BITS(ib, 26, 29);
						break;
					case 1:
						print("CHECK_DISABLE: %s%u%s, DOORBELL_OFFSET: %s0x%lx%s",
							BLUE, (unsigned)BITS(ib, 1, 2), RST,
							YELLOW, (unsigned long)BITS(ib, 2, 28), RST);
						break;
				}
				if (decoder->pm4.cur_word > 1) {
					switch ((decoder->pm4.cur_word - 2) % 4) {
						case 0: print("MQD_ADDR_LO: %s0x%lx%s", YELLOW, (unsigned long)ib, RST);
								decoder->pm4.next_ib_state.ib_addr_lo = ib;
								break;
						case 1: print("MQD_ADDR_HI: %s0x%lx%s", YELLOW, (unsigned long)ib, RST);
								decoder->pm4.next_ib_state.ib_addr_hi = ib;
								break;
						case 2: print("WPTR_ADDR_LO: %s0x%lx%s", YELLOW, (unsigned long)ib, RST);
								break;
						case 3: print("WPTR_ADDR_HI: %s0x%lx%s", YELLOW, (unsigned long)ib, RST);
								if (!asic->options.no_follow_ib) {
									if (umr_read_vram(asic, asic->options.vm_partition, decoder->pm4.next_ib_state.ib_vmid,
													  ((uint64_t)decoder->pm4.next_ib_state.ib_addr_hi << 32) | decoder->pm4.next_ib_state.ib_addr_lo, 4, buf) < 0) {
										print(" [%sUNMAPPED%s]", RED, RST);
									} else {
										print(" [%sMAPPED%s]", GREEN, RST);
										add_data_block_pm4(asic, decoder, UMR_DATABLOCK_MQD_NV);
									}
								}
								break;
					}
				}
			}
			break;
		case 0xA3: // UNMAP_QUEUES
			if (asic->family <= FAMILY_VI) {
				switch(decoder->pm4.cur_word) {
					case 0: decoder->pm4.next_write_mem.addr_lo = ib;
						print("ACTION: %s%u%s, QUEUE_SEL: %s%u%s, ENGINE_SEL: %s%u%s, NUM_QUEUES: %s%u%s",
								BLUE, (unsigned)BITS(ib, 0, 2), RST,
								BLUE, (unsigned)BITS(ib, 4, 6), RST,
								BLUE, (unsigned)BITS(ib, 26, 29), RST,
								BLUE, (unsigned)BITS(ib, 29, 32), RST);
						break;
					case 1:
						if (BITS(decoder->pm4.next_write_mem.addr_lo, 4, 6) == 1)
							print("PASID: %s%u%s", BLUE, (unsigned)BITS(ib, 0, 16), RST);
						else
							print("DOORBELL_OFFSET0: %s%lx%s", YELLOW, BITS(ib, 2, 23), RST);
						break;
					case 2:
						print("DOORBELL_OFFSET1: %s%lx%s", YELLOW, BITS(ib, 2, 23), RST);
						break;
					case 3:
						print("DOORBELL_OFFSET2: %s%lx%s", YELLOW, BITS(ib, 2, 23), RST);
						break;
					case 4:
						print("DOORBELL_OFFSET3: %s%lx%s", YELLOW, BITS(ib, 2, 23), RST);
						break;
				}
			} else if (asic->family <= FAMILY_AI) {
				switch(decoder->pm4.cur_word) {
					case 0: decoder->pm4.next_write_mem.addr_lo = ib;
						print("ACTION: %s%u%s, QUEUE_SEL: %s%u%s, ENGINE_SEL: %s%u%s, NUM_QUEUES: %s%u%s",
								BLUE, (unsigned)BITS(ib, 0, 2), RST,
								BLUE, (unsigned)BITS(ib, 4, 6), RST,
								BLUE, (unsigned)BITS(ib, 26, 29), RST,
								BLUE, (unsigned)BITS(ib, 29, 32), RST);
						break;
					case 1:
						if (BITS(decoder->pm4.next_write_mem.addr_lo, 4, 6) == 1)
							print("PASID: %s%u%s", BLUE, (unsigned)BITS(ib, 0, 16), RST);
						else
							print("DOORBELL_OFFSET0: %s0x%lx%s", YELLOW, BITS(ib, 2, 28), RST);
						break;
					case 2:
						if (BITS(decoder->pm4.next_write_mem.addr_lo, 26, 29) == 4 && BITS(decoder->pm4.next_write_mem.addr_lo, 0, 2) == 3)
							print("RB_WPTR: %s0x%lx%s", YELLOW, BITS(ib, 0, 20), RST);
						else
							print("DOORBELL_OFFSET1: %s%lx%s", YELLOW, BITS(ib, 2, 28), RST);
						break;
					case 3:
						print("DOORBELL_OFFSET2: %s%lx%s", YELLOW, BITS(ib, 2, 28), RST);
						break;
					case 4:
						print("DOORBELL_OFFSET3: %s%lx%s", YELLOW, BITS(ib, 2, 28), RST);
						break;
				}
			} else if (asic->family <= FAMILY_NV) {
				switch(decoder->pm4.cur_word) {
					case 0: decoder->pm4.next_write_mem.addr_lo = ib;
						print("ACTION: %s%u%s, QUEUE_SEL: %s%u%s, ENGINE_SEL: %s%u%s, NUM_QUEUES: %s%u%s",
								BLUE, (unsigned)BITS(ib, 0, 2), RST,
								BLUE, (unsigned)BITS(ib, 4, 6), RST,
								BLUE, (unsigned)BITS(ib, 26, 29), RST,
								BLUE, (unsigned)BITS(ib, 29, 32), RST);
						break;
					case 1:
						if (BITS(decoder->pm4.next_write_mem.addr_lo, 4, 6) == 1)
							print("PASID: %s%u%s", BLUE, (unsigned)BITS(ib, 0, 16), RST);
						else
							print("DOORBELL_OFFSET0: %s0x%lx%s", YELLOW, BITS(ib, 2, 28), RST);
						break;
					case 2:
						if (BITS(decoder->pm4.next_write_mem.addr_lo, 0, 2) == 3)
							print("TF_ADDR_LO32: %s0x%lx%s", YELLOW, BITS(ib, 2, 32), RST);
						else
							print("DOORBELL_OFFSET1: %s%lx%s", YELLOW, BITS(ib, 2, 28), RST);
						break;
					case 3:
						if (BITS(decoder->pm4.next_write_mem.addr_lo, 0, 2) == 3)
							print("TF_ADDR_HI32: %s0x%lx%s", YELLOW, BITS(ib, 0, 32), RST);
						else
							print("DOORBELL_OFFSET2: %s%lx%s", YELLOW, BITS(ib, 2, 28), RST);
						break;
					case 4:
						if (BITS(decoder->pm4.next_write_mem.addr_lo, 0, 2) == 3)
							print("TF_DATA: %s0x%lx%s", YELLOW, BITS(ib, 0, 32), RST);
						else
							print("DOORBELL_OFFSET3: %s%lx%s", YELLOW, BITS(ib, 2, 28), RST);
						break;
				}
			}
			break;
		case 0xA4: // PKT3_QUERY_STATUS
			if (asic->family <= FAMILY_VI) {
				switch(decoder->pm4.cur_word) {
					case 0: decoder->pm4.next_write_mem.addr_lo = ib;
						print("CONTEXT_ID: %s%u%s, INTERRUPT_SEL: %s%u%s, COMMAND: %s%u%s",
							BLUE, (unsigned)BITS(ib, 0, 28), RST,
							BLUE, (unsigned)BITS(ib, 28, 30), RST,
							BLUE, (unsigned)BITS(ib, 30, 32), RST);
						break;
					case 1:
						if (BITS(decoder->pm4.next_write_mem.addr_lo, 28, 30) == 1) {
							print("PASID: %s%u%s", BLUE, (unsigned)BITS(ib, 0, 16), RST);
						} else {
							print("DOORBELL_OFFSET: %s0x%lx%s, ENGINE_SEL: %s%u%s",
								YELLOW, (unsigned long)BITS(ib, 2, 23), RST,
								BLUE, (unsigned)BITS(ib, 26, 29), RST);
						}
						break;
					case 2: print("ADDR_LO: %s0x%lx%s", YELLOW, (unsigned long)ib, RST); break;
					case 3: print("ADDR_HI: %s0x%lx%s", YELLOW, (unsigned long)ib, RST); break;
					case 4: print("DATA_LO: %s0x%lx%s", YELLOW, (unsigned long)ib, RST); break;
					case 5: print("DATA_HI: %s0x%lx%s", YELLOW, (unsigned long)ib, RST); break;
				}
			} else if (asic->family <= FAMILY_NV) {
				switch(decoder->pm4.cur_word) {
					case 0: decoder->pm4.next_write_mem.addr_lo = ib;
						print("CONTEXT_ID: %s%u%s, INTERRUPT_SEL: %s%u%s, COMMAND: %s%u%s",
							BLUE, (unsigned)BITS(ib, 0, 28), RST,
							BLUE, (unsigned)BITS(ib, 28, 30), RST,
							BLUE, (unsigned)BITS(ib, 30, 32), RST);
						break;
					case 1:
						if (BITS(decoder->pm4.next_write_mem.addr_lo, 28, 30) == 1) {
							print("PASID: %s%u%s", BLUE, (unsigned)BITS(ib, 0, 16), RST);
						} else {
							print("DOORBELL_OFFSET: %s0x%lx%s, ENGINE_SEL: %s%u%s",
								YELLOW, (unsigned long)BITS(ib, 2, 28), RST,
								BLUE, (unsigned)BITS(ib, 28, 31), RST);
						}
						break;
					case 2: print("ADDR_LO: %s0x%lx%s", YELLOW, (unsigned long)ib, RST); break;
					case 3: print("ADDR_HI: %s0x%lx%s", YELLOW, (unsigned long)ib, RST); break;
					case 4: print("DATA_LO: %s0x%lx%s", YELLOW, (unsigned long)ib, RST); break;
					case 5: print("DATA_HI: %s0x%lx%s", YELLOW, (unsigned long)ib, RST); break;
				}
			}
			break;
		case 0xA9: 	// PKT3_DISPATCH_TASK_STATE_INIT
			switch (decoder->pm4.cur_word) {
				case 0: print("CONTROL_BUF_ADDR_LO: %s0x%08lx%s", YELLOW, (unsigned long)ib & 0xFFFFFF00UL, RST); break;
				case 1: print("CONTROL_BUF_ADDR_HI: %s0x%08lx%s", YELLOW, (unsigned long)ib, RST); break;
			}
			break;
		case 0xAA:	// DISPATCH_TASKMESH_DIRECT_ACE
			switch (decoder->pm4.cur_word) {
				case 0: print("DIM_X: %s%lu%s", BLUE, (unsigned long)ib, RST); break;
				case 1: print("DIM_Y: %s%lu%s", BLUE, (unsigned long)ib, RST); break;
				case 2: print("DIM_Z: %s%lu%s", BLUE, (unsigned long)ib, RST); break;
				case 3: print("DISPATCH_INITIATOR: %s0x%lx%s", BLUE, (unsigned long)ib, RST); break;
				case 4: print("RING_ENTRY_LOC: %s0x%lx%s (%s%s%s)",
								BLUE, (unsigned long)ib, RST,
								RED, umr_reg_name(asic, BITS(ib, 0, 16) + 0x2C00), RST);
						break;
			}
			break;
		case 0xAD: // DISPATCH_TASKMESH_INDIRECT_MULTI_ACE
			switch (decoder->pm4.cur_word) {
				case 0: print("DATA_ADDR_LO: %s0x%lx%s", YELLOW, (unsigned long)BITS(ib, 2, 32) << 2, RST); break;
				case 1: print("DATA_ADDR_HI: %s0x%lx%s", YELLOW, (unsigned long)ib, RST); break;
				case 2: print("RING_ENTRY_LOC: %s0x%lx%s (%s%s%s)",
								BLUE, (unsigned long)ib, RST,
								RED, umr_reg_name(asic, BITS(ib, 0, 16) + 0x2C00), RST);
						break;
				case 3: print("THREAD_TRACE_MARKER_ENABLE: %s%u%s, COUNT_INDIRECT_ENABLE: %s%u%s, "
				               "DISPATCH_INDEX_ENABLE: %s%u%s, COMPUTE_XYZ_DIM_ENABLE: %s%u%s, "
				               "DISPATCH_INDEX_LOC: %s0x%lx%s (%s%s%s)",
				               BLUE, (unsigned)BITS(ib, 0, 1), RST,
				               BLUE, (unsigned)BITS(ib, 1, 2), RST,
				               BLUE, (unsigned)BITS(ib, 2, 3), RST,
				               BLUE, (unsigned)BITS(ib, 3, 4), RST,
				               BLUE, (unsigned long)BITS(ib, 16, 32), RST, RED, umr_reg_name(asic, BITS(ib, 16, 32) + 0x2C00), RST);
						break;
				case 4: print("COMPUTE_XYZ_DIM_LOC: %s0x%lx%s (%s%s%s)",
								BLUE, (unsigned long)ib, RST,
								RED, umr_reg_name(asic, BITS(ib, 0, 16) + 0x2C00), RST);
						break;
				case 5: print("COUNT: %s%lu%s", BLUE, (unsigned long)ib, RST); break;
				case 6: print("COUNT_ADDR_LO: %s0x%lx%s", YELLOW, (unsigned long)BITS(ib, 2, 32) << 2, RST); break;
				case 7: print("COUNT_ADDR_HI: %s0x%lx%s", YELLOW, (unsigned long)ib, RST); break;
				case 8: print("STRIDE: %s%lu%s", BLUE, (unsigned long)ib, RST); break;
				case 9: print("DISPATCH_INITIATOR: %s0x%lx%s", BLUE, (unsigned long)ib, RST); break;
			}
			break;
		default:
			print("PKT3 DATA");
			break;
	}
}

/**
 * print_decode_pm4 - Handle the FSM when decoding PM4 packets
 *
 * This function handles a stream of PM4 words and farms them
 * off to the correct decoders based on the current state.
 */
static void print_decode_pm4(struct umr_asic *asic, struct umr_ring_decoder *decoder, uint32_t ib, int (*print)(const char *fmt, ...))
{
	char namecpy[128], *name;
	switch (decoder->pm4.cur_opcode) {
		case 0xFFFFFFFF: // initial decode
			decoder->pm4.pkt_type = ib >> 30;
			decoder->pm4.n_words  = (((ib >> 16) + 1) & 0x3FFF);
			decoder->pm4.cur_word = 0;

			if (decoder->pm4.pkt_type == 0) {
				print("PKT0, COUNT:%lu, BASE_INDEX:0x%lx", (unsigned long)decoder->pm4.n_words, (unsigned long)(ib & 0xFFFF));
				decoder->pm4.cur_opcode = 0x80000000; // TYPE-0 opcode...
				decoder->pm4.next_write_mem.addr_lo = ib & 0xFFFF;
			} else if (decoder->pm4.pkt_type == 2) {
				print("PKT2");
			} else if (decoder->pm4.pkt_type == 3) {
				decoder->pm4.cur_opcode = (ib >> 8) & 0xFF;
				print("PKT3, COUNT:%lu, PREDICATE:%lu, SHADER_TYPE:%lu, OPCODE:%02lx[%s%s%s]",
				(unsigned long)decoder->pm4.n_words, (unsigned long)(ib&1), (unsigned long)((ib>>1)&1), (unsigned long)decoder->pm4.cur_opcode,
				CYAN, pm4_pkt3_opcode_names[decoder->pm4.cur_opcode&0xFF], RST);
			}
			if (!decoder->pm4.n_words)
				decoder->pm4.cur_opcode = 0xFFFFFFFF;
			return;

		// PKT0 type, simple register writes
		case 0x80000000:
			name = umr_reg_name(asic, decoder->pm4.next_write_mem.addr_lo);
			print("   word (%lu): %s(%s0x%lx%s) <= %s0x%lx%s",
				(unsigned long)decoder->pm4.cur_word++, name,
				BLUE, (unsigned long)decoder->pm4.next_write_mem.addr_lo, RST,
				YELLOW, (unsigned long)ib, RST);
			print_bits(asic, decoder->pm4.next_write_mem.addr_lo, ib, 1, print);

			// strip off IP name
			name = strstr(name, ".");
			if (name) {
				if (name[0] == '.') ++name;
				strcpy(namecpy, umr_find_reg_by_addr(asic, decoder->pm4.next_write_mem.addr_lo, NULL)->regname);

				// detect VCN/UVD IBs and chain them once all
				// 4 pieces of information are found
				if (strstr(name, "mmUVD_LMI_RBC_IB_VMID")) {
					decoder->pm4.next_ib_state.ib_vmid = ib | ((asic->family <= FAMILY_VI) ? 0 : UMR_MM_HUB);
					decoder->pm4.next_ib_state.tally |= 1;
				} else if (strstr(name, "mmUVD_LMI_RBC_IB_64BIT_BAR_LOW")) {
					decoder->pm4.next_ib_state.ib_addr_lo = ib;
					decoder->pm4.next_ib_state.tally |= 2;
				} else if (strstr(name, "mmUVD_LMI_RBC_IB_64BIT_BAR_HIGH")) {
					decoder->pm4.next_ib_state.ib_addr_hi = ib;
					decoder->pm4.next_ib_state.tally |= 4;
				} else if (strstr(name, "mmUVD_RBC_IB_SIZE")) {
					decoder->pm4.next_ib_state.ib_size = ib * 4;
					decoder->pm4.next_ib_state.tally |= 8;
				}

				if (decoder->pm4.next_ib_state.tally == (2|4|8)) {
					decoder->pm4.next_ib_state.ib_vmid = 0;
					decoder->pm4.next_ib_state.tally = 15;
				}

				if (decoder->pm4.next_ib_state.tally == 15) {
					decoder->pm4.next_ib_state.tally = 0;
					add_ib_pm4(decoder);
				}

				if (strstr(namecpy, "GPCOM_VCPU_CMD")) {
					// print out command
					print(", OPCODE [%s0x%x%s, %s",
						BLUE, (unsigned)umr_bitslice_reg_by_name(asic, namecpy, "CMD", ib), RST,
						CYAN);
					switch (umr_bitslice_reg_by_name(asic, namecpy, "CMD", ib)) {
						case 0: print("CMD_MSG_BUFFER"); break;
						case 1: print("DPB_MSG_BUFFER"); break;
						case 2: print("DECODING_TARGET_BUFFER"); break;
						case 3: print("FEEDBACK_BUFFER"); break;
						case 5: print("SESSSION_CONTEXT_BUFFER"); break;
						case 0x100: print("BITSTREAM_BUFFER"); break;
						case 0x204: print("ITSCALING_TABLE_BUFFER"); break;
						case 0x206: print("CONTEXT_BUFFER"); break;
						default: print("UNKNOWN"); break;
					}
					print("%s]", RST);
				}
			}

			decoder->pm4.next_write_mem.addr_lo++;
			break;

		// default means a PKT3 opcode
		default:
			print_decode_pm4_pkt3(asic, decoder, ib, print);
			++(decoder->pm4.cur_word);
			break;
	}
	if (!--(decoder->pm4.n_words) ) {
		decoder->pm4.cur_opcode = 0xFFFFFFFF;
	}
}

static const char *sdma_opcodes[] = {
	"NOP", // 0
	"COPY", // 1
	"WRITE", // 2
	"",
	"INDIRECT_BUFFER", // 4
	"FENCE", //5
	"TRAP", //6
	"SEM", //7
	"POLL_REGMEM", //8
	"COND_EXE", //9
	"ATOMIC", //10
	"CONST_FILL", //11
	"GEN_PTEPDE", //12
	"TIMESTAMP", //13
	"SRBM WRITE", //14
	"PRE EXE",//15
	"",//16
	"GCR_REQ", //17
};

static void parse_next_sdma_pkt(struct umr_asic *asic, struct umr_ring_decoder *decoder, uint32_t ib, int (*print)(const char *fmt, ...))
{
	char buf[4];

	if (decoder->sdma.n_words == 1)
		print("\\---+ ");
	else
		print("|---+ ");

	print("WORD [%s%u%s]: ", BLUE, (unsigned)decoder->sdma.cur_word, RST);
	switch (decoder->sdma.cur_opcode) {
		case 1: // COPY
			switch (decoder->sdma.cur_sub_opcode) {
				case 0: // LINEAR
					switch (decoder->sdma.header_dw & (1UL << 27)) {
						case 0: // not broadcast
							switch (decoder->sdma.cur_word) {
								case 1: print("COPY_COUNT: %s0x%08lx%s", BLUE, (unsigned long)ib, RST);
									break;
								case 2: print("DST_SW: %s%u%s, DST_HA: %s%u%s, SRC_SW: %s%u%s, SRC_HA: %s%u%s",
										BLUE, ((unsigned)(ib >> 16) & 3), RST,
										BLUE, ((unsigned)(ib >> 22) & 1), RST,
										BLUE, ((unsigned)(ib >> 24) & 3), RST,
										BLUE, ((unsigned)(ib >> 30) & 1), RST);
									break;
								case 3: print("SRC_ADDR_LO: %s0x%08lx%s", YELLOW, (unsigned long)ib, RST);
									break;
								case 4: print("SRC_ADDR_HI: %s0x%08lx%s", YELLOW, (unsigned long)ib, RST);
									break;
								case 5: print("DST_ADDR_LO: %s0x%08lx%s", YELLOW, (unsigned long)ib, RST);
									break;
								case 6: print("DST_ADDR_HI: %s0x%08lx%s", YELLOW, (unsigned long)ib, RST);
									break;
							}
							break;
						default: // broadcast
							break;
					}
					break;
				case 1: // TILED
					break;
				case 3: // SOA
					break;
				case 4: // LINEAR_SUB_WINDOW
					switch (decoder->sdma.cur_word) {
						case 1: print("SRC_ADDR_LO: %s0x%08lx%s", YELLOW, (unsigned long)ib, RST);
							break;
						case 2: print("SRC_ADDR_HI: %s0x%08lx%s", YELLOW, (unsigned long)ib, RST);
							break;
						case 3: print("SRC_X: %s%u%s, SRC_Y: %s%u%s",
								BLUE, ((unsigned)(ib >> 0) & 0x3FFF), RST,
								BLUE, ((unsigned)(ib >> 16) & 0x3FFF), RST);
							break;
						case 4: print("SRC_Z: %s%u%s, SRC_PITCH: %s%u%s",
								BLUE, ((unsigned)(ib >> 0) & 0x7FF), RST,
								BLUE, ((unsigned)(ib >> 16) & 0x3FFF), RST);
							break;
						case 5: print("SRC_SLICE_PITCH: %s%u%s", BLUE, ib & 0xFFFFFFF, RST);
							break;
						case 6: print("DST_ADDR_LO: %s0x%08lx%s", YELLOW, (unsigned long)ib, RST);
							break;
						case 7: print("DST_ADDR_HI: %s0x%08lx%s", YELLOW, (unsigned long)ib, RST);
							break;
						case 8: print("DST_X: %s%u%s, DST_Y: %s%u%s",
								BLUE, ((unsigned)(ib >> 0) & 0x3FFF), RST,
								BLUE, ((unsigned)(ib >> 16) & 0x3FFF), RST);
							break;
						case 9: print("DST_Z: %s%u%s, DST_PITCH: %s%u%s",
								BLUE, ((unsigned)(ib >> 0) & 0x7FF), RST,
								BLUE, ((unsigned)(ib >> 16) & 0x3FFF), RST);
							break;
						case 10: print("DST_SLICE_PITCH: %s%u%s", BLUE, ib & 0xFFFFFFF, RST);
							break;
						case 11: print("RECT_X: %s%u%s, RECT_Y: %s%u%s",
								BLUE, ((unsigned)(ib >> 0) & 0x3FFF), RST,
								BLUE, ((unsigned)(ib >> 16) & 0x3FFF), RST);
							break;
						case 12: print("RECT_Z: %s%u%s, DST_SW: %s%u%s, DST_HA: %s%u%s, SRC_SW: %s%u%s, SRC_HA: %s%u%s",
								BLUE, ((unsigned)(ib >> 0) & 0x3FFF), RST,
								BLUE, ((unsigned)(ib >> 16) & 0x3), RST,
								BLUE, ((unsigned)(ib >> 22) & 0x1), RST,
								BLUE, ((unsigned)(ib >> 24) & 0x3), RST,
								BLUE, ((unsigned)(ib >> 30) & 0x1), RST);
							break;
					}
					break;
				case 5: // TILED_SUB_WINDOW (TODO bitfields)
					switch (decoder->sdma.cur_word) {
						case 1: print("TILED_ADDR_LO: %s0x%08lx%s", YELLOW, (unsigned long)ib, RST);
							break;
						case 2: print("TILED_ADDR_HI: %s0x%08lx%s", YELLOW, (unsigned long)ib, RST);
							break;
						case 3: print("TILED_X: %s%u%s, TILED_Y: %s%u%s",
								BLUE, ((unsigned)(ib >> 0) & 0x3FFF), RST,
								BLUE, ((unsigned)(ib >> 16) & 0x3FFF), RST);
							break;
						case 4: print("TILED_Z: %s%u%s, TILED_PITCH: %s%u%s",
								BLUE, ((unsigned)(ib >> 0) & 0x7FF), RST,
								BLUE, ((unsigned)(ib >> 16) & 0x3FFF), RST);
							break;
						case 5: print("PITCH_IN_TILE: %s%u%s", BLUE, ib & 0xFFFFFFF, RST);
							break;
						case 6: print("ELEMENT_SIZE: %s%u%s, ARRAY_MODE: %s%u%s, MIT_MODE: %s%u%s, TILESPLIT_SIZE: %s%u%s, BANK_W: %s%u%s, BANK_H: %s%u%s, NUM_BANK: %s%u%s, MAT_ASPT: %s%u%s, PIPE_CONFIG: %s%u%s",
								BLUE, ((unsigned)(ib >> 0) & 0x7), RST,
								BLUE, ((unsigned)(ib >> 3) & 0xF), RST,
								BLUE, ((unsigned)(ib >> 8) & 0x7), RST,
								BLUE, ((unsigned)(ib >> 11) & 0x7), RST,
								BLUE, ((unsigned)(ib >> 15) & 0x3), RST,
								BLUE, ((unsigned)(ib >> 18) & 0x3), RST,
								BLUE, ((unsigned)(ib >> 21) & 0x3), RST,
								BLUE, ((unsigned)(ib >> 24) & 0x3), RST,
								BLUE, ((unsigned)(ib >> 26) & 0x1F), RST);
							break;
						case 7: print("LINEAR_ADDR_LO: %s0x%08lx%s", YELLOW, (unsigned long)ib, RST);
							break;
						case 8: print("LINEAR_ADDR_HI: %s0x%08lx%s", YELLOW, (unsigned long)ib, RST);
							break;
						case 9: print("LINEAR_X: %s%u%s, LINEAR_Y: %s%u%s",
								BLUE, ((unsigned)(ib >> 0) & 0x3FFF), RST,
								BLUE, ((unsigned)(ib >> 16) & 0x3FFF), RST);
							break;
						case 10: print("LINEAR_Z: %s%u%s, LINEAR_PITCH: %s%u%s",
								BLUE, ((unsigned)(ib >> 0) & 0x7FF), RST,
								BLUE, ((unsigned)(ib >> 16) & 0x3FFF), RST);
							break;
						case 11: print("LINEAR_SLICE_PITCH: %s%u%s", BLUE, ib & 0xFFFFFFF, RST);
							break;
						case 12: print("RECT_X: %s%u%s, RECT_Y: %s%u%s",
								BLUE, ((unsigned)(ib >> 0) & 0x3FFF), RST,
								BLUE, ((unsigned)(ib >> 16) & 0x3FFF), RST);
							break;
						case 13: print("RECT_Z: %s%u%s, LINEAR_SW: %s%u%s, TILE_SW: %s%u%s",
								BLUE, ((unsigned)(ib >> 0) & 0x7FF), RST,
								BLUE, ((unsigned)(ib >> 16) & 0x3), RST,
								BLUE, ((unsigned)(ib >> 24) & 0x3), RST);
							break;
					}
					break;
				case 6: // T2T_SUB_WINDOW
					switch (decoder->sdma.cur_word) {
						case 1: print("SRC_ADDR_LO: %s0x%08lx%s", YELLOW, (unsigned long)ib, RST);
							break;
						case 2: print("SRC_ADDR_HI: %s0x%08lx%s", YELLOW, (unsigned long)ib, RST);
							break;
						case 3: print("SRC_X: %s%u%s, SRC_Y: %s%u%s",
								BLUE, ((unsigned)(ib >> 0) & 0x3FFF), RST,
								BLUE, ((unsigned)(ib >> 16) & 0x3FFF), RST);
							break;
						case 4: print("SRC_Z: %s%u%s, SRC_PITCH: %s%u%s",
								BLUE, ((unsigned)(ib >> 0) & 0x7FF), RST,
								BLUE, ((unsigned)(ib >> 16) & 0x3FFF), RST);
							break;
						case 5: print("SRC_SLICE_PITCH: %s%u%s", BLUE, ib & 0xFFFFFFF, RST);
							break;
						case 6: print("SRC_ELEMENT_SIZE: %s%u%s, SRC_ARRAY_MODE: %s%u%s, SRC_MIT_MODE: %s%u%s, SRC_TILESPLIT_SIZE: %s%u%s, SRC_BANK_W: %s%u%s, SRC_BANK_H: %s%u%s, NUM_BANKS: %s%u%s, MAT_ASPT: %s%u%s, PIPE_CONFIG: %s%u%s",
								BLUE, ((unsigned)(ib >> 0) & 0x7), RST,
								BLUE, ((unsigned)(ib >> 3) & 0xF), RST,
								BLUE, ((unsigned)(ib >> 8) & 0x7), RST,
								BLUE, ((unsigned)(ib >> 11) & 0x7), RST,
								BLUE, ((unsigned)(ib >> 15) & 0x3), RST,
								BLUE, ((unsigned)(ib >> 18) & 0x3), RST,
								BLUE, ((unsigned)(ib >> 21) & 0x3), RST,
								BLUE, ((unsigned)(ib >> 24) & 0x3), RST,
								BLUE, ((unsigned)(ib >> 26) & 0x1F), RST);
							break;
						case 7: print("DST_ADDR_LO: %s0x%08lx%s", YELLOW, (unsigned long)ib, RST);
							break;
						case 8: print("DST_ADDR_HI: %s0x%08lx%s", YELLOW, (unsigned long)ib, RST);
							break;
						case 9: print("DW9: %s0x%08lx%s", BLUE, (unsigned long)ib, RST);
							break;
						case 10: print("DST_Z: %s%u%s, DST_PITCH: %s%u%s",
								BLUE, ((unsigned)(ib >> 0) & 0x7FF), RST,
								BLUE, ((unsigned)(ib >> 16) & 0x3FFF), RST);
							break;
						case 11: print("DST_SLICE_PITCH: %s%u%s", BLUE, ib & 0xFFFFFFF, RST);
							break;
						case 12: print("ARRAY_MODE: %s%u%s, MIT_MODE: %s%u%s, TILESPLIT_SIZE: %s%u%s, BANK_W: %s%u%s, BANK_H: %s%u%s, NUM_BANK: %s%u%s, MAT_ASPT: %s%u%s, PIPE_CONFIG: %s%u%s",
								BLUE, ((unsigned)(ib >> 3) & 0xF), RST,
								BLUE, ((unsigned)(ib >> 8) & 0x7), RST,
								BLUE, ((unsigned)(ib >> 11) & 0x7), RST,
								BLUE, ((unsigned)(ib >> 15) & 0x3), RST,
								BLUE, ((unsigned)(ib >> 18) & 0x3), RST,
								BLUE, ((unsigned)(ib >> 21) & 0x3), RST,
								BLUE, ((unsigned)(ib >> 24) & 0x3), RST,
								BLUE, ((unsigned)(ib >> 26) & 0x1F), RST);
							break;
						case 13: print("RECT_X: %s%u%s, RECT_Y: %s%u%s",
								BLUE, ((unsigned)(ib >> 0) & 0x3FFF), RST,
								BLUE, ((unsigned)(ib >> 16) & 0x3FFF), RST);
							break;
						case 14: print("RECT_Z: %s%u%s, DST_SW: %s%u%s, SRC_SW: %s%u%s",
								BLUE, ((unsigned)(ib >> 0) & 0x7FF), RST,
								BLUE, ((unsigned)(ib >> 16) & 0x3), RST,
								BLUE, ((unsigned)(ib >> 24) & 0x3), RST);
							break;
					}
					break;
			}
			break;
		case 2: // WRITE
			switch (decoder->sdma.cur_sub_opcode) {
				case 0: // LINEAR
					switch (decoder->sdma.cur_word) {
						case 1: print("DST_ADDR_LO: %s0x%08lx%s", YELLOW, (unsigned long)ib, RST);
							break;
						case 2: print("DST_ADDR_HI: %s0x%08lx%s", YELLOW, (unsigned long)ib, RST);
							break;
						case 3: print("COUNT: %s0x%08lx%s", BLUE, (unsigned long)ib, RST);
							decoder->sdma.n_words += ib;
							break;
						default: print("DATA: %s0x%08lx%s", BLUE, (unsigned long)ib, RST);
							break;
					}
					break;

				case 1: // TILED (TODO bit decodings...)
					switch (decoder->sdma.cur_word) {
						case 1: print("DST_ADDR_LO: %s0x%08lx%s", YELLOW, (unsigned long)ib, RST);
							break;
						case 2: print("DST_ADDR_HI: %s0x%08lx%s", YELLOW, (unsigned long)ib, RST);
							break;
						case 3: print("DW3: %s0x%08lx%s", BLUE, (unsigned long)ib, RST);
							break;
						case 4: print("DW4: %s0x%08lx%s", BLUE, (unsigned long)ib, RST);
							break;
						case 5: print("DW5: %s0x%08lx%s", BLUE, (unsigned long)ib, RST);
							break;
						case 6: print("DW6: %s0x%08lx%s", BLUE, (unsigned long)ib, RST);
							break;
						case 7: print("DW7: %s0x%08lx%s", BLUE, (unsigned long)ib, RST);
							break;
					}
					break;
			}
			break;
		case 4: // INDIRECT
			switch (decoder->sdma.cur_word) {
				case 1: print("IB_BASE_LO: %s0x%08lx%s", YELLOW, (unsigned long)ib, RST);
					decoder->sdma.next_ib_state.ib_addr_lo = ib;
					break;
				case 2: print("IB_BASE_HI: %s0x%08lx%s", YELLOW, (unsigned long)ib, RST);
					decoder->sdma.next_ib_state.ib_addr_hi = ib;
					break;
				case 3: print("IB_BASE_SIZE: %s0x%08lx%s", BLUE, (unsigned long)ib, RST);
					decoder->sdma.next_ib_state.ib_size = ib * 4; // number of bytesq
					break;
				case 4: print("IB_CSA_ADDR_LO: %s0x%08lx%s", YELLOW, (unsigned long)ib, RST);
					decoder->sdma.next_ib_state.csa_addr_lo = ib;
					break;
				case 5: print("IB_CSA_ADDR_HI: %s0x%08lx%s", YELLOW, (unsigned long)ib, RST);
					decoder->sdma.next_ib_state.csa_addr_hi = ib;
					if (!asic->options.no_follow_ib) {
						if (umr_read_vram(asic, asic->options.vm_partition, decoder->sdma.next_ib_state.ib_vmid,
										  ((uint64_t)decoder->sdma.next_ib_state.ib_addr_hi << 32) | decoder->sdma.next_ib_state.ib_addr_lo,
										  4, buf) < 0) {
							print(" [%sUNMAPPED%s]", RED, RST);
						} else {
							print(" [%sMAPPED%s]", GREEN, RST);
							add_ib_pm3(decoder);
						}
					}
					break;
			}
			break;
		case 5: // FENCE
			switch (decoder->sdma.cur_word) {
				case 1: print("FENCE_ADDR_LO: %s0x%08lx%s", YELLOW, (unsigned long)ib, RST);
					break;
				case 2: print("FENCE_ADDR_HI: %s0x%08lx%s", YELLOW, (unsigned long)ib, RST);
					break;
				case 3: print("FENCE_DATA: %s0x%08lx%s", BLUE, (unsigned long)ib, RST);
					break;
			}
			break;
		case 6: // TRAP
			switch (decoder->sdma.cur_word) {
				case 1: print("TRAP_INT_CONTEXT: %s0x%08lx%s", YELLOW, (unsigned long)ib & 0xFFFFFFF, RST);
					break;
			}
			break;
		case 7: // SEM
			switch (decoder->sdma.cur_word) {
				case 1: print("SEMAPHORE_ADDR_LO: %s0x%08lx%s", YELLOW, (unsigned long)ib, RST);
					break;
				case 2: print("SEMAPHORE_ADDR_HI: %s0x%08lx%s", YELLOW, (unsigned long)ib, RST);
					break;
			}
			break;
		case 8: // POLL_REGMEM
			switch (decoder->sdma.cur_sub_opcode) {
				case 0: // WAIT_REG_MEM
					switch (decoder->sdma.cur_word) {
						case 1: print("POLL_REGMEM_ADDR_LO: %s0x%08lx%s", YELLOW, (unsigned long)ib, RST);
							if (!(decoder->sdma.header_dw & (1UL << 31))) print("(%s)", umr_reg_name(asic, BITS(ib, 2, 20)));
							break;
						case 2: print("POLL_REGMEM_ADDR_HI: %s0x%08lx%s", YELLOW, (unsigned long)ib, RST);
							if (((decoder->sdma.header_dw >> 26) & 3) == 1) print("(%s)", umr_reg_name(asic, BITS(ib, 2, 18)));
							break;
						case 3: print("POLL_REGMEM_ADDR_VALUE: %s0x%08lx%s", BLUE, (unsigned long)ib, RST);
							break;
						case 4: print("POLL_REGMEM_ADDR_MASK: %s0x%08lx%s", BLUE, (unsigned long)ib, RST);
							break;
						case 5: print("POLL_REGMEM_ADDR_DW5: %s0x%08lx%s", BLUE, (unsigned long)ib, RST);
							break;
					}
					break;
				case 1: // WRITE WAIT_REG_MEM
					switch (decoder->sdma.cur_word) {
						case 1: print("SRC_ADDR: %s0x%08lx%s", YELLOW, (unsigned long)ib, RST);
							if (!(decoder->sdma.header_dw & (1UL << 31))) print("(%s)", umr_reg_name(asic, ib));
							break;
						case 2: print("DST_ADDR_LO: %s0x%08lx%s", YELLOW, (unsigned long)ib, RST);
							break;
						case 3: print("DST_ADDR_HI: %s0x%08lx%s", YELLOW, (unsigned long)ib, RST);
							break;
					}
					break;
			}
			break;
		case 9:  // COND_EXE
			switch (decoder->sdma.cur_word) {
				case 1: print("ADDR_LO: %s0x%08lx%s", YELLOW, (unsigned long)ib, RST);
					break;
				case 2: print("ADDR_HI: %s0x%08lx%s", YELLOW, (unsigned long)ib, RST);
					break;
				case 3: print("REFERENCE: %s0x%08lx%s", BLUE, (unsigned long)ib, RST);
					break;
				case 4: print("EXEC_COUNT: %s0x%08lx%s", BLUE, (unsigned long)ib, RST);
					break;
			}
			break;
		case 10:  // ATOMIC
			switch (decoder->sdma.cur_word) {
				case 1: print("ADDR_LO: %s0x%08lx%s", YELLOW, (unsigned long)ib, RST);
					break;
				case 2: print("ADDR_HI: %s0x%08lx%s", YELLOW, (unsigned long)ib, RST);
					break;
				case 3: print("SRC_DATA_LO: %s0x%08lx%s", BLUE, (unsigned long)ib, RST);
					break;
				case 4: print("SRC_DATA_HI: %s0x%08lx%s", BLUE, (unsigned long)ib, RST);
					break;
				case 5: print("CMP_DATA_LO: %s0x%08lx%s", BLUE, (unsigned long)ib, RST);
					break;
				case 6: print("CMP_DATA_HI: %s0x%08lx%s", BLUE, (unsigned long)ib, RST);
					break;
				case 7: print("LOOP_INTERVAL: %s0x%08lx%s", BLUE, (unsigned long)ib & 0x1FFF, RST);
					break;
			}
			break;
		case 11: // CONST_FILL
			switch (decoder->sdma.cur_word) {
				case 1: print("CONST_FILL_DST_LO: %s0x%08lx%s", YELLOW, (unsigned long)ib, RST);
					break;
				case 2: print("CONST_FILL_DST_HI: %s0x%08lx%s", YELLOW, (unsigned long)ib, RST);
					break;
				case 3: print("CONST_FILL_DATA: %s0x%08lx%s", BLUE, (unsigned long)ib, RST);
					break;
				case 4: print("CONST_FILL_BYTE_COUNT: %s0x%08lx%s", BLUE, (unsigned long)ib, RST);
					break;
			}
			break;
		case 12: // GEN_PTEPDE
			switch (decoder->sdma.cur_word) {
				case 1: print("GEN_PTEPDE_PE_ADDR_LO: %s0x%08lx%s", YELLOW, (unsigned long)ib, RST);
					break;
				case 2: print("GEN_PTEPDE_PE_ADDR_HI: %s0x%08lx%s", YELLOW, (unsigned long)ib, RST);
					break;
				case 3: print("GEN_PTEPDE_FLAGS_LO: %s0x%08lx%s", BLUE, (unsigned long)ib, RST);
					break;
				case 4: print("GEN_PTEPDE_FLAGS_HI: %s0x%08lx%s", BLUE, (unsigned long)ib, RST);
					break;
				case 5: print("GEN_PTEPDE_ADDR_LO: %s0x%08lx%s", YELLOW, (unsigned long)ib, RST);
					break;
				case 6: print("GEN_PTEPDE_ADDR_HI: %s0x%08lx%s", YELLOW, (unsigned long)ib, RST);
					break;
				case 7: print("GEN_PTEPDE_INC_SIZE: %s0x%08lx%s", BLUE, (unsigned long)ib, RST);
					break;
				case 8: print("GEN_PTEPDE_DW8: %s0x%08lx%s", BLUE, (unsigned long)ib, RST);
					break;
				case 9: print("GEN_PTEPDE_COUNT: %s0x%08lx%s", BLUE, (unsigned long)ib, RST);
					break;
			}
			break;
		case 14: // SRBM_WRITE
			switch (decoder->sdma.cur_word) {
				case 1:
					if (asic->family <= FAMILY_VI)
						print("SRBM_WRITE_ADDR: %s0x%08lx%s(%s)", YELLOW, (unsigned long)ib & 0xFFFF, RST, umr_reg_name(asic, ib & 0xFFFF));
					else
						print("SRBM_WRITE_ADDR: %s0x%08lx%s(%s)", YELLOW, (unsigned long)ib & 0x3FFFF, RST, umr_reg_name(asic, ib & 0x3FFFF));
					decoder->sdma.next_write_mem = ib;
					break;
				case 2: print("SRBM_WRITE_DATA: %s0x%08lx%s", BLUE, (unsigned long)ib, RST);
					print_bits(asic, decoder->sdma.next_write_mem, ib, 1, print);
					break;
			}
			break;
		case 15: // PRE_EXE
			switch (decoder->sdma.cur_word) {
				case 1: print("COUNT: %s0x%08lu%s", BLUE, (unsigned long)ib & 0x3FFF, RST);
					break;
			}
			break;
		case 16: // GPUVM_INC
			switch (decoder->sdma.cur_word) {
				case 1:
					print("PER_VMID_INV_REQ: %s%x%s, FLUSH_TYPE: %s%x%s, INV_L2_PTES: %s%u%s, INV_L2_PDE0: %s%u%s, "
					       "INV_L2_PDE1: %s%u%s, INV_L2_PDE2: %s%u%s, INV_L1_PTES: %s%u%s, "
					       "CLEAR_PROTECTION_FAULT_STATUS_ADDR: %s%u%s, LOG_REQUEST: %s%u%s, 4KB: %s%u%s",
							BLUE, (unsigned)ib, RST,
							BLUE, (unsigned)BITS(ib, 16, 19), RST,
							BLUE, (unsigned)BITS(ib, 19, 20), RST,
							BLUE, (unsigned)BITS(ib, 20, 21), RST,
							BLUE, (unsigned)BITS(ib, 21, 22), RST,
							BLUE, (unsigned)BITS(ib, 22, 23), RST,
							BLUE, (unsigned)BITS(ib, 23, 24), RST,
							BLUE, (unsigned)BITS(ib, 24, 25), RST,
							BLUE, (unsigned)BITS(ib, 25, 26), RST,
							BLUE, (unsigned)BITS(ib, 26, 27), RST);
					break;
				case 2:
					print("S_BIT: %s%u%s, PAGE_VM_ADDR_LO: %s0x%lx%s",
						BLUE, (unsigned)BITS(ib, 0, 1), RST,
						YELLOW, (unsigned long)BITS(ib, 1, 32), RST);
					break;
				case 3:
					print("PAGE_VM_ADDR_HI: %s0x%lx%s", YELLOW, (unsigned long)BITS(ib, 0, 6), RST);
					break;
			}
			break;
		case 17: // GCR
			switch (decoder->sdma.cur_word) {
				case 1: print("BASE_VA_LO: %s0x%08lx%s", YELLOW, (unsigned long)(decoder->sdma.next_write_mem = BITS(ib, 7, 32) << 7), RST); break;
				case 2: print("BASE_VA_HI: %s0x%08lx%s (BASE_VA: %s0x%llx%s), ",
							   YELLOW, (unsigned long)BITS(ib, 0, 16), RST,
							   YELLOW, (unsigned long long)(((unsigned long long)BITS(ib, 0, 16) << 39) | decoder->sdma.next_write_mem ), RST);
						ib >>= 16;
						print("GL2_WB: %s%u%s, GL2_INV: %s%u%s, GL2_DISCARD: %s%u%s, "
							   "GL2_RANGE: %s%u%s, GL2_US: %s%u%s, GL1_INV: %s%u%s, "
							   "GLV_INV: %s%u%s, GLK_INV: %s%u%s, GLK_WB: %s%u%s, "
							   "GLM_INV: %s%u%s, GLM_WB: %s%u%s, GL1_RANGE: %s%u%s, GLI_INV: %s%u%s",
							   BLUE, (unsigned)BITS(ib, 15, 16), RST,
							   BLUE, (unsigned)BITS(ib, 14, 15), RST,
							   BLUE, (unsigned)BITS(ib, 13, 14), RST,
							   BLUE, (unsigned)BITS(ib, 11, 13), RST,
							   BLUE, (unsigned)BITS(ib, 10, 11), RST,
							   BLUE, (unsigned)BITS(ib, 9, 10), RST,
							   BLUE, (unsigned)BITS(ib, 8, 9), RST,
							   BLUE, (unsigned)BITS(ib, 7, 8), RST,
							   BLUE, (unsigned)BITS(ib, 6, 7), RST,
							   BLUE, (unsigned)BITS(ib, 5, 6), RST,
							   BLUE, (unsigned)BITS(ib, 4, 5), RST,
							   BLUE, (unsigned)BITS(ib, 2, 4), RST,
							   BLUE, (unsigned)BITS(ib, 0, 2), RST);
						break;
				case 3: print("RANGE_IS_PA: %s%u%s, SEQ: %s%u%s, LIMIT_VA_LO: %s0x%08lx%s",
							   BLUE, (unsigned)BITS(ib, 2, 3), RST,
							   BLUE, (unsigned)BITS(ib, 0, 2), RST,
							   YELLOW, (unsigned long)(decoder->sdma.next_write_mem = BITS(ib, 7, 32) << 7), RST);
						break;
				case 4: print("LIMIT_VA_HI: %s0x%08lx%s (LIMIT_VA: %s0x%llx%s), VMID: %s%u%s",
							   YELLOW, (unsigned long)BITS(ib, 0, 16), RST,
							   YELLOW, (unsigned long long)(((unsigned long long)BITS(ib, 0, 16) << 39) | decoder->sdma.next_write_mem), RST,
							   BLUE, (unsigned)BITS(ib, 24, 28), RST);
						break;
			}
			break;
	}

	decoder->sdma.cur_word++;
}

static void print_decode_sdma(struct umr_asic *asic, struct umr_ring_decoder *decoder, uint32_t ib, int (*print)(const char *fmt, ...))
{
	static const char *poll_regmem_funcs[] = { "always", "<", "<=", "==", "!=", ">=", ">", "N/A" };
	switch (decoder->sdma.cur_opcode) {
		case 0xFFFFFFFF: // initial decode
			decoder->sdma.cur_opcode = ib & 0xFF;
			decoder->sdma.cur_sub_opcode = (ib >> 8) & 0xFF;
			decoder->sdma.cur_word = 1;
			decoder->sdma.header_dw = ib;

			// sanity check
			if (decoder->sdma.cur_opcode > 17) {
				// invalid
				decoder->sdma.cur_opcode = 0xFFFFFFFF;
				break;
			}

			print("OPCODE: [%s%s%s], SUB-OPCODE: [%s%u%s]",
				CYAN, sdma_opcodes[decoder->sdma.cur_opcode], RST,
				BLUE, (unsigned)decoder->sdma.cur_sub_opcode, RST);

			// handle decoding "extra information" from header word
			switch (decoder->sdma.cur_opcode) {
				case 0: // NOP
					decoder->sdma.n_words = 1;
					break;
				case 1:  // COPY
					switch (decoder->sdma.cur_sub_opcode) {
						case 0: // LINEAR
							print(", %sLINEAR", CYAN);
							decoder->sdma.n_words = 7;

							// BROADCAST
							if (ib & (1UL << 27)) {
								decoder->sdma.n_words += 2;
								print("_BROADCAST");
							}
							print("_COPY%s", RST);
							break;
						case 1: // TILED
							print(", %sTILED_COPY%s", CYAN, RST);
							decoder->sdma.n_words = 12;
							break;
						case 3: // STRUCTURE/SOA
							print(", %sSTRUCTURE_COPY%s", CYAN, RST);
							decoder->sdma.n_words = 8;
							break;
						case 4: // LINEAR_SUB_WINDOW
							print(", %sLINEAR_SUB_WINDOW_COPY%s", CYAN, RST);
							decoder->sdma.n_words = 13;
							break;
						case 5: // TILED_SUB_WINDOW
							print(", %sTILED_SUB_WINDOW_COPY%s, DETILE: %s%u%s", CYAN, RST, BLUE, (unsigned)(ib >> 31), RST);
							decoder->sdma.n_words = 14;
							break;
						case 6: // T2T_SUB_WIND
							print(", %sT2T_SUB_WINDOW_COPY%s", CYAN, RST);
							decoder->sdma.n_words = 15;
							break;
					}
					break;
				case 2:  // WRITE
					switch (decoder->sdma.cur_sub_opcode) {
						case 0: // LINEAR
							print(", %sLINEAR_WRITE%s", CYAN, RST);
							decoder->sdma.n_words = 5;
							break;
						case 1: // TILED
							print(", %sTILED_WRITE%s", CYAN, RST);
							decoder->sdma.n_words = 10;
							break;
					}
					break;
				case 4: // INDIRECT
					decoder->sdma.next_ib_state.ib_vmid = (ib >> 16) & 0xF;
					if (asic->family >= FAMILY_AI)
						decoder->sdma.next_ib_state.ib_vmid |= UMR_MM_HUB;
					print(", VMID: %s%u%s", BLUE, decoder->sdma.next_ib_state.ib_vmid & 0xFF, RST);
					decoder->sdma.n_words = 6;
					break;
				case 5: // FENCE
					decoder->sdma.n_words = 4;
					break;
				case 6: // TRAP
					decoder->sdma.n_words = 2;
					break;
				case 7: // SEM
					print(", WRITE_ONE: %s%u%s, SIGNAL: %s%u%s, MAILBOX: %s%u%s",
						BLUE, (unsigned)((ib >> 29) & 1), RST,
						BLUE, (unsigned)((ib >> 30) & 1), RST,
						BLUE, (unsigned)((ib >> 31) & 1), RST);
					decoder->sdma.n_words = 3;
					break;
				case 8: // POLL_REGMEM
					print(", HDP_FLUSH: %s%u%s, FUNCTION: %s%u%s (%s%s%s), MEM_POLL: %s%u%s",
						BLUE, (unsigned)((ib >> 26) & 1), RST,
						BLUE, (unsigned)((ib >> 28) & 7), RST,
						CYAN, poll_regmem_funcs[((ib >> 28) & 7)], RST,
						BLUE, (unsigned)((ib >> 31) & 1), RST);
					decoder->sdma.n_words = decoder->sdma.cur_sub_opcode ? 4 : 6;
					break;
				case 9: // COND_EXE
					decoder->sdma.n_words = 5;
					break;
				case 10: // ATOMIC
					print(", LOOP: %s%u%s, OP: %s%u%s",
						BLUE, (unsigned)((ib >> 16) & 1), RST,
						BLUE, (unsigned)((ib >> 25) & 0x7F), RST);
					decoder->sdma.n_words = 8;
					break;
				case 11: // CONST_FILL
					print(", FILL_SIZE: %s%u%s", BLUE, (unsigned)(ib >> 30), RST);
					decoder->sdma.n_words = 5;
					break;
				case 12: // GEN_PTEPDE
					decoder->sdma.n_words = 10;
					break;
				case 13: // TIMESTAMP
					switch (decoder->sdma.cur_sub_opcode) {
						case 0:
							print(", %sTIMESTAMP_SET%s", CYAN, RST);
							decoder->sdma.n_words = 3;
							break;
						case 1:
							print(", %sTIMESTAMP_GET%s", CYAN, RST);
							decoder->sdma.n_words = 3;
							break;
						case 2:
							print(", %sTIMESTAMP_GET_GLOBAL%s", CYAN, RST);
							decoder->sdma.n_words = 3;
							break;
					}
					break;
				case 14: // SRBM_WRITE
					print(", BYTE ENABLE: %s0x%x%s", BLUE, (unsigned)(ib >> 28), RST);
					decoder->sdma.n_words = 3;
					break;
				case 15: // PRE_EXE
					print(", DEV_SEL: %s%u%s",
						BLUE, (unsigned)((ib >> 16) & 0xFF), RST);
					decoder->sdma.n_words = 2;
					break;
				case 16: // GPUVM_INV
					print(", GPUVM_INV");
					decoder->sdma.n_words = 4;
					break;
				case 17: // GCR
					print(", GCR");
					decoder->sdma.n_words = 5;
					break;
				default:
					break; // nothing to print
			}
			break;
		default:
			parse_next_sdma_pkt(asic, decoder, ib, print);
			break;
	}
	if (!--(decoder->sdma.n_words) ) {
		decoder->sdma.cur_opcode = 0xFFFFFFFF;
	}
}

/**
 * umr_print_decode - Print the next word of an IB
 */
void umr_print_decode(struct umr_asic *asic, struct umr_ring_decoder *decoder, uint32_t ib, int (*print)(const char *fmt, ...))
{
	if (!print)
		print = printf;
	switch (decoder->pm) {
		case 4:
			print_decode_pm4(asic, decoder, ib, print);
			break;
		case 3:
			print_decode_sdma(asic, decoder, ib, print);
			break;
	}
}

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

static char *op_3c_functions[] = { "true", "<", "<=", "==", "!=", ">=", ">", "reserved",
								   NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
static char *op_37_engines[] = { "ME", "PFP", "CE", "DE" };
static char *op_37_dst_sel[] = { "mem-mapped reg", "memory sync", "TC/L2", "GDS", "reserved", "memory async", "reserved", "reserved",
								 "preemption meta memory", NULL, NULL, NULL, NULL, NULL, NULL, NULL };
static char *op_40_mem_sel[] = { "mem-mapped reg", "memory", "tc_l2", "gds", "perfcounters", "immediate data", "atomic return data", "gds_atomic_return_data_0",
								 "gds_atomic_return_data1", "gpu_clock_count", "system_clock_count", NULL, NULL, NULL, NULL, NULL };
static char *op_84_cntr_sel[] = { "invalid", "ce", "cs", "ce and cs" };
static char *op_7a_index_str[] = { "default", "prim_type", "index_type", "num_instance", "multi_vgt_param", "reserved", "reserved", "reserved",
								   NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
static const struct {
	const char *name;
	int maj, min;
} pm4_pkt_names[] = {
	{ "UNK", 0, 0 }, // 00
	{ "UNK", 0, 0 }, // 01
	{ "UNK", 0, 0 }, // 02
	{ "UNK", 0, 0 }, // 03
	{ "UNK", 0, 0 }, // 04
	{ "UNK", 0, 0 }, // 05
	{ "UNK", 0, 0 }, // 06
	{ "UNK", 0, 0 }, // 07
	{ "UNK", 0, 0 }, // 08
	{ "UNK", 0, 0 }, // 09
	{ "UNK", 0, 0 }, // 0a
	{ "UNK", 0, 0 }, // 0b
	{ "UNK", 0, 0 }, // 0c
	{ "UNK", 0, 0 }, // 0d
	{ "UNK", 0, 0 }, // 0e
	{ "UNK", 0, 0 }, // 0f
	{ "PKT3_NOP", 8, 0 }, // 10
	{ "PKT3_SET_BASE", 9, 0 }, // 11
	{ "PKT3_CLEAR_STATE", 8, 0 }, // 12
	{ "PKT3_INDEX_BUFFER_SIZE", 9, 0 },// 13
	{ "UNK", 0, 0 }, // 14
	{ "PKT3_DISPATCH_DIRECT", 8, 0 }, // 15
	{ "PKT3_DISPATCH_INDIRECT", 9, 0 }, // 16
	{ "UNK", 0, 0 }, // 17
	{ "UNK", 0, 0 }, // 18
	{ "UNK", 0, 0 }, // 19
	{ "UNK", 0, 0 }, // 1a
	{ "UNK", 0, 0 }, // 1b
	{ "UNK", 0, 0 }, // 1c
	{ "PKT3_ATOMIC_GDS", 8, 0 }, // 1d
	{ "PKT3_ATOMIC_MEM", 8, 0 }, // 1e
	{ "UNK", 0, 0 }, // 1f
	{ "PKT3_SET_PREDICATION", 9, 0 }, // 20
	{ "UNK", 0, 0 }, // 21
	{ "PKT3_COND_EXEC", 8, 0 }, // 22
	{ "UNK", 0, 0 }, // 23
	{ "UNK", 0, 0 }, // 24
	{ "PKT3_DRAW_INDEX_INDIRECT", 9, 0 }, // 25
	{ "PKT3_INDEX_BASE", 9, 0 }, // 26
	{ "PKT3_DRAW_INDEX_2", 8, 0 }, // 27
	{ "PKT3_CONTEXT_CONTROL", 8, 0 }, // 28
	{ "UNK", 0, 0 }, // 29
	{ "UNK", 0, 0 }, // 2a
	{ "UNK", 0, 0 }, // 2b
	{ "UNK", 0, 0 }, // 2c
	{ "PKT3_DRAW_INDEX_AUTO", 8, 0 }, // 2d
	{ "UNK", 0, 0 }, // 2e
	{ "PKT3_NUM_INSTANCES", 8, 0 }, // 2f
	{ "UNK", 0, 0 }, // 30
	{ "UNK", 0, 0 }, // 31
	{ "UNK", 0, 0 }, // 32
	{ "PKT3_INDIRECT_BUFFER_CONST", 8, 0 }, // 33
	{ "UNK", 0, 0 }, // 34
	{ "UNK", 0, 0 }, // 35
	{ "UNK", 0, 0 }, // 36
	{ "PKT3_WRITE_DATA", 8, 0 }, // 37
	{ "PKT3_DRAW_INDEX_INDIRECT_MULTI", 9, 0 }, // 38
	{ "UNK", 0, 0 }, // 39
	{ "UNK", 0, 0 }, // 3a
	{ "UNK", 0, 0 }, // 3b
	{ "PKT3_WAIT_REG_MEM", 8, 0 }, // 3c
	{ "UNK", 0, 0 }, // 3d
	{ "UNK", 0, 0 }, // 3e
	{ "PKT3_INDIRECT_BUFFER_CIK", 8, 0 }, // 3f
	{ "PKT3_COPY_DATA", 8, 0 },// 40
	{ "UNK", 0, 0 }, // 41
	{ "PKT3_PFP_SYNC_ME", 8, 0 }, // 42
	{ "PKT3_SURFACE_SYNC", 8, 0 }, // 43
	{ "UNK", 0, 0 }, // 44
	{ "UNK", 0, 0 }, // 45
	{ "PKT3_EVENT_WRITE", 8, 0 }, // 46
	{ "PKT3_EVENT_WRITE_EOP", 8, 0 }, // 47
	{ "UNK", 0, 0 }, // 48
	{ "PKT3_RELEASE_MEM", 8, 0 }, // 49
	{ "PKT3_PREAMBLE_CNTL", 8, 0 }, // 4a
	{ "UNK", 0, 0 }, // 4b
	{ "PKT3_DISPATCH_MESH_INDIRECT_MULTI", 10, 0 }, // 4c
	{ "PKT3_DISPATCH_TASKMESH_GFX", 10, 0 }, // 4d
	{ "UNK", 0, 0 }, // 4e
	{ "UNK", 0, 0 }, // 4f
	{ "PKT3_DMA_DATA", 8, 0},  // 50
	{ "PKT3_CONTEXT_REG_RMW", 9, 0 }, // 51
	{ "UNK", 0, 0 }, // 52
	{ "UNK", 0, 0 }, // 53
	{ "UNK", 0, 0 }, // 54
	{ "UNK", 0, 0 }, // 55
	{ "UNK", 0, 0 }, // 56
	{ "UNK", 0, 0 }, // 57
	{ "PKT3_ACQUIRE_MEM", 8, 0 }, // 58
	{ "UNK", 0, 0 }, // 59
	{ "UNK", 0, 0 }, // 5a
	{ "UNK", 0, 0 }, // 5b
	{ "UNK", 0, 0 }, // 5c
	{ "PKT3_PRIME_UTCL2", 9, 0 }, // 5d
	{ "PKT3_LOAD_UCONFIG_REG", 8, 0 }, // 5e
	{ "PKT3_LOAD_SH_REG", 8, 0 }, // 5f
	{ "PKT3_LOAD_CONFIG_REG", 8, 0 }, // 60
	{ "PKT3_LOAD_CONTEXT_REG", 8, 0 }, // 61
	{ "UNK", 0, 0 }, // 62
	{ "PKT3_LOAD_SH_REG_INDEX", 8, 0 }, // 63
	{ "UNK", 0, 0 }, // 64
	{ "UNK", 0, 0 }, // 65
	{ "UNK", 0, 0 }, // 66
	{ "UNK", 0, 0 }, // 67
	{ "PKT3_SET_CONFIG_REG", 8, 0 }, // 68
	{ "PKT3_SET_CONTEXT_REG", 8, 0 }, // 69
	{ "UNK", 0, 0 }, // 6a
	{ "UNK", 0, 0 }, // 6b
	{ "UNK", 0, 0 }, // 6c
	{ "UNK", 0, 0 }, // 6d
	{ "UNK", 0, 0 }, // 6e
	{ "UNK", 0, 0 }, // 6f
	{ "UNK", 0, 0 }, // 70
	{ "UNK", 0, 0 }, // 71
	{ "UNK", 0, 0 }, // 72
	{ "UNK", 0, 0 }, // 73
	{ "UNK", 0, 0 }, // 74
	{ "UNK", 0, 0 }, // 75
	{ "PKT3_SET_SH_REG", 8, 0 }, // 76
	{ "UNK", 0, 0 }, // 77
	{ "UNK", 0, 0 }, // 78
	{ "PKT3_SET_UCONFIG_REG", 8, 0 }, // 79
	{ "PKT3_SET_UCONFIG_REG_INDEX", 8, 0 }, // 7a
	{ "UNK", 0, 0 }, // 7b
	{ "UNK", 0, 0 }, // 7c
	{ "UNK", 0, 0 }, // 7d
	{ "UNK", 0, 0 }, // 7e
	{ "UNK", 0, 0 }, // 7f
	{ "PKT3_LOAD_CONST_RAM", 8, 0 }, // 80
	{ "PKT3_WRITE_CONST_RAM", 8, 0 }, // 81
	{ "UNK", 0, 0 }, // 82
	{ "PKT3_DUMP_CONST_RAM", 8, 0 }, // 83
	{ "PKT3_INCREMENT_CE_COUNTER", 8, 0 }, // 84
	{ "UNK", 0, 0 }, // 85
	{ "PKT3_WAIT_ON_CE_COUNTER", 8, 0 }, // 86
	{ "UNK", 0, 0 }, // 87
	{ "UNK", 0, 0 }, // 88
	{ "UNK", 0, 0 }, // 89
	{ "UNK", 0, 0 }, // 8a
	{ "PKT3_SWITCH_BUFFER", 8, 0 }, // 8b
	{ "UNK", 0, 0 }, // 8c
	{ "UNK", 0, 0 }, // 8d
	{ "UNK", 0, 0 }, // 8e
	{ "UNK", 0, 0 }, // 8f
	{ "PKT3_FRAME_CONTROL", 8, 0 }, // 90
	{ "PKT3_INDEX_ATTRIBUTES_INDIRECT", 8, 0 }, // 91
	{ "UNK", 0, 0 }, // 92
	{ "UNK", 0, 0 }, // 93
	{ "UNK", 0, 0 }, // 94
	{ "PKT3_HDP_FLUSH", 8, 0 }, // 95
	{ "UNK", 0, 0 }, // 96
	{ "UNK", 0, 0 }, // 97
	{ "UNK", 0, 0 }, // 98
	{ "UNK", 0, 0 }, // 99
	{ "PKT3_DMA_DATA_FILL_MULTI", 8, 0 }, // 9a
	{ "PKT3_SET_SH_REG_INDEX", 8, 0 }, // 9b
	{ "UNK", 0, 0 }, // 9c
	{ "UNK", 0, 0 }, // 9d
	{ "UNK", 0, 0 }, // 9e
	{ "PKT3_LOAD_CONTEXT_REG_INDEX", 8, 0 }, // 9f
	{ "PKT3_SET_RESOURCES", 8, 0 }, // a0
	{ "PKT3_MAP_PROCESS", 8, 0 }, // a1
	{ "PKT3_MAP_QUEUES", 8, 0 }, // a2
	{ "PKT3_UNMAP_QUEUES", 8, 0 }, // a3
	{ "PKT3_QUERY_STATUS", 8, 0 }, // a4
	{ "PKT3_MES_RUN_LIST", 8, 0 }, // a5
	{ "UNK", 0, 0 }, // a6
	{ "PKT3_DISPATCH_DIRECT_INTERLEAVED", 12, 0 }, // a7
	{ "UNK", 0, 0 }, // a8
	{ "PKT3_DISPATCH_TASK_STATE_INIT", 10, 0 }, // a9
	{ "PKT3_DISPATCH_TASKMESH_DIRECT_ACE", 10, 0 }, // aa
	{ "UNK", 0, 0 }, // ab
	{ "UNK", 0, 0 }, // ac
	{ "PKT3_DISPATCH_TASKMESH_INDIRECT_MULTI_ACE", 10, 0 }, // ad
	{ "UNK", 0, 0 }, // ae
	{ "UNK", 0, 0 }, // af
	{ "UNK", 0, 0 }, // b0
	{ "UNK", 0, 0 }, // b1
	{ "UNK", 0, 0 }, // b2
	{ "UNK", 0, 0 }, // b3
	{ "UNK", 0, 0 }, // b4
	{ "UNK", 0, 0 }, // b5
	{ "UNK", 0, 0 }, // b6
	{ "UNK", 0, 0 }, // b7
	{ "PKT3_SET_CONTEXT_REG_PAIRS", 12, 0 }, // b8
	{ "PKT3_SET_CONTEXT_REG_PAIRS_PACKED", 11, 0 }, // b9
	{ "PKT3_SET_SH_REG_PAIRS", 12, 0 }, // ba
	{ "PKT3_SET_SH_REG_PAIRS_PACKED", 11, 0 }, // bb
	{ "PKT3_SET_SH_REG_PAIRS_PACKED", 12, 0 }, // bc
	{ "PKT3_SET_SH_REG_PAIRS_PACKED_N", 11, 0 }, // bd
	{ "PKT3_SET_UCONFIG_REG_PAIRS", 12, 0 }, // be
	{ "UNK", 0, 0 }, // bf
	{ "UNK", 0, 0 }, // c0
	{ "UNK", 0, 0 }, // c1
	{ "UNK", 0, 0 }, // c2
	{ "UNK", 0, 0 }, // c3
	{ "UNK", 0, 0 }, // c4
	{ "UNK", 0, 0 }, // c5
	{ "UNK", 0, 0 }, // c6
	{ "UNK", 0, 0 }, // c7
	{ "UNK", 0, 0 }, // c8
	{ "UNK", 0, 0 }, // c9
	{ "UNK", 0, 0 }, // ca
	{ "UNK", 0, 0 }, // cb
	{ "UNK", 0, 0 }, // cc
	{ "UNK", 0, 0 }, // cd
	{ "UNK", 0, 0 }, // ce
	{ "UNK", 0, 0 }, // cf
	{ "UNK", 0, 0 }, // d0
	{ "UNK", 0, 0 }, // d1
	{ "UNK", 0, 0 }, // d2
	{ "UNK", 0, 0 }, // d3
	{ "UNK", 0, 0 }, // d4
	{ "UNK", 0, 0 }, // d5
	{ "UNK", 0, 0 }, // d6
	{ "UNK", 0, 0 }, // d7
	{ "UNK", 0, 0 }, // d8
	{ "UNK", 0, 0 }, // d9
	{ "UNK", 0, 0 }, // da
	{ "UNK", 0, 0 }, // db
	{ "UNK", 0, 0 }, // dc
	{ "UNK", 0, 0 }, // dd
	{ "UNK", 0, 0 }, // de
	{ "UNK", 0, 0 }, // df
	{ "UNK", 0, 0 }, // e0
	{ "UNK", 0, 0 }, // e1
	{ "UNK", 0, 0 }, // e2
	{ "UNK", 0, 0 }, // e3
	{ "UNK", 0, 0 }, // e4
	{ "UNK", 0, 0 }, // e5
	{ "UNK", 0, 0 }, // e6
	{ "UNK", 0, 0 }, // e7
	{ "UNK", 0, 0 }, // e8
	{ "UNK", 0, 0 }, // e9
	{ "UNK", 0, 0 }, // ea
	{ "UNK", 0, 0 }, // eb
	{ "UNK", 0, 0 }, // ec
	{ "UNK", 0, 0 }, // ed
	{ "UNK", 0, 0 }, // ee
	{ "PKT3_UPDATE_DB_SUMMARIZER_TIMEOUTS", 12, 0 }, // ef
	{ "UNK", 0, 0 }, // f0
	{ "UNK", 0, 0 }, // f1
	{ "UNK", 0, 0 }, // f2
	{ "UNK", 0, 0 }, // f3
	{ "UNK", 0, 0 }, // f4
	{ "UNK", 0, 0 }, // f5
	{ "UNK", 0, 0 }, // f6
	{ "UNK", 0, 0 }, // f7
	{ "UNK", 0, 0 }, // f8
	{ "UNK", 0, 0 }, // f9
	{ "UNK", 0, 0 }, // fa
	{ "UNK", 0, 0 }, // fb
	{ "UNK", 0, 0 }, // fc
	{ "UNK", 0, 0 }, // fd
	{ "UNK", 0, 0 }, // fe
	{ "UNK", 0, 0 }, // ff
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

/**
 * umr_gfx_get_ip_ver - Get the version of the GC/GFX IP block
 *
 * @asic: The ASIC to query
 * @maj: Where to store the major revision
 * @min: Where to store the minor revision
 *
 * Returns -1 on error.
 */
int umr_gfx_get_ip_ver(struct umr_asic *asic, int *maj, int *min)
{
	struct umr_ip_block *ip;

	// try by GC version
	ip = umr_find_ip_block(asic, "gfx", 0); // for multi instance
	if (!ip)
		ip = umr_find_ip_block(asic, "gfx", -1); // for single instance

	if (ip) {
		*maj = ip->discoverable.maj;
		*min = ip->discoverable.min;
		return 0;
	}
	return -1;
}

static char *vgt_event_decode(unsigned tag)
{
	unsigned x;
	for (x = 0; vgt_event_tags[x].name; x++) {
		if (vgt_event_tags[x].event_no == tag)
			return vgt_event_tags[x].name;
	}
	return "<unknown event>";
}

#define BITS(x, a, b) (unsigned long)((x >> (a)) & ((1ULL << ((b)-(a)))-1))

const char *umr_pm4_opcode_to_str(uint32_t header)
{
       return pm4_pkt_names[(header >> 8) & 0xFF].name;
}

static uint32_t fetch_word(struct umr_asic *asic, struct umr_pm4_stream *stream, uint32_t off)
{
	if (off >= stream->n_words) {
		if (!(stream->invalid))
			asic->err_msg("[ERROR]: PM4 decoding of opcode (%"PRIx32") went out of bounds.\n", stream->opcode);
		stream->invalid = 1;
		return 0;
	} else {
		return stream->words[off];
	}
}

static void decode_pkt0(struct umr_asic *asic, struct umr_stream_decode_ui *ui, struct umr_pm4_stream *stream, uint64_t ib_addr, uint32_t ib_vmid)
{
	uint32_t n;
	for (n = 0; n < stream->n_words; n++)
		ui->add_field(ui, ib_addr + 4 * (n + 1), ib_vmid, umr_reg_name(asic, stream->pkt0off + n), fetch_word(asic, stream, n), NULL, 16, 32);
}

// for packets 0x5E, 0x5F, 0x60, 0x61
static void load_X_reg(struct umr_asic *asic, struct umr_stream_decode_ui *ui, struct umr_pm4_stream *stream, uint64_t ib_addr, uint32_t ib_vmid)
{
	char *str, tmpstr[256];
	uint32_t str_size, *data, j, k, m, n, reg_base;
	uint64_t base_addr;

	switch (stream->opcode) {
		case 0x5E: reg_base = 0xC000; break; // LOAD_UCONFIG_REG
		case 0x5F: reg_base = 0x2C00; break; // LOAD_SH_REG
		case 0x60: reg_base = 0x2000; break; // LOAD_CONFIG_REG
		case 0x61: reg_base = 0xA000; break; // LOAD_CONTEXT_REG
	}

	base_addr = fetch_word(asic, stream, 0) & ~2UL;
	base_addr |= ((uint64_t)fetch_word(asic, stream, 1)) << 32;

	ui->add_field(ui, ib_addr + 4, ib_vmid, "BASE_ADDR_LO", BITS(fetch_word(asic, stream, 0), 2, 32) << 2, NULL, 16, 32);
	ui->add_field(ui, ib_addr + 8, ib_vmid, "BASE_ADDR_HI", fetch_word(asic, stream, 1), NULL, 16, 32);

	for (n = 2; n < stream->n_words; n += 2) {
		k = BITS(fetch_word(asic, stream, n), 0, 16); // REG_OFFSET
		m = BITS(fetch_word(asic, stream, n + 1), 0, 14); // NUM_DWORDS

		if (!asic->options.no_follow_loadx) {
			str_size = 4096;
			str = calloc(1, str_size);
			if (!str) {
				asic->err_msg("[ERROR]: Out of memory");
				return;
			}
			strcat(str, "\n");

			// fetch data
			data = calloc(m, sizeof data[0]);
			if (!data) {
				asic->err_msg("[ERROR]: Out of memory");
				free(str);
				return;
			}

			if (!umr_read_vram(asic, asic->options.vm_partition, ib_vmid, base_addr + 4 * k, 4 * m, data)) {
				// turn into data string
				for (j = 0; j < m; j++) {
					snprintf(tmpstr, sizeof tmpstr, "#%4"PRIx32":%s <= 0x%"PRIx32"\n", k + j, umr_reg_name(asic, reg_base + k + j), data[j]);
					while (strlen(tmpstr) + strlen(str) >= str_size) {
						char *tmp;
						str_size += 4096;
						tmp = realloc(str, str_size);
						if (!tmp) {
							asic->err_msg("[ERROR]: Out of memory\n");
							free(data);
							free(str);
							return;
						}
						str = tmp;
					}
					strcat(str, tmpstr);
				}
			}
			free(data);
		} else {
			str = NULL;
		}

		ui->add_field(ui, ib_addr + 12 + ((n - 2) * 4), ib_vmid, "REG_OFFSET", k, umr_reg_name(asic, reg_base + k), 16, 32);
		ui->add_field(ui, ib_addr + 16 + ((n - 2) * 4), ib_vmid, "NUM_DWORD", m, str, 10, 32);
		free(str);
	}
}

static void decode_pkt3_gfx8(struct umr_asic *asic, struct umr_stream_decode_ui *ui, struct umr_pm4_stream *stream, uint64_t ib_addr, uint32_t ib_vmid)
{
	switch (stream->opcode) {
		case 0x10: // NOP
			if (stream->n_words == 0)
				break;
			if (fetch_word(asic, stream, 0) == 0x1337F77D) { // magic value for comments
				uint32_t pktlen = fetch_word(asic, stream, 1) - 1; // number of words in NOP sequence
				uint32_t pkttype = fetch_word(asic, stream, 2);
				char *str;

				ui->add_field(ui, ib_addr + 4, ib_vmid, "COMMENT_PACKET_LEN", pktlen, NULL, 10, 32);
				ui->add_field(ui, ib_addr + 8, ib_vmid, "COMMENT_PACKET_TYPE", pkttype, NULL, 10, 32);
				switch (pkttype) {
					case 7:
						str = calloc(1, 1 + pktlen * 4 - 12);
						if (str) {
							memcpy(str, &stream->words[3], pktlen * 4 - 12);
							ui->add_field(ui, ib_addr + 12, ib_vmid, "COMMENT_STRING", 0, str, 0, 0);
							free(str);
						}	
						break;
				}
			} else if (fetch_word(asic, stream, 0) == 0x3337F77D) { // magic value for BINARY data
				uint32_t n;

				for (n = 1; n < stream->n_words; n++) {
					ui->add_field(ui, ib_addr + 4 + (4 * n), ib_vmid, "BINARY DATA", fetch_word(asic, stream, n), NULL, 16, 32);
				}
			}
			break;
		case 0x12: // CLEAR_STATE
			break;
		case 0x15: // DISPATCH_DIRECT
			ui->add_field(ui, ib_addr + 4, ib_vmid, "DIM_X", fetch_word(asic, stream, 0), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "DIM_Y", fetch_word(asic, stream, 1), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "DIM_Z", fetch_word(asic, stream, 2), NULL, 10, 32);
			break;
		case 0x1d: // ATOMIC_GDS
			// TODO: fill in
			break;
		case 0x1e: // ATOMIC_MEM
			// TODO: fill in
			break;
		case 0x22: // COND_EXEC
			ui->add_field(ui, ib_addr + 4, ib_vmid, "GPU_ADDR_LO32", BITS(fetch_word(asic, stream, 0), 2, 32) << 2, NULL, 16, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "GPU_ADDR_HI32", fetch_word(asic, stream, 1), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "COMMAND", BITS(fetch_word(asic, stream, 2), 28, 32), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 16, ib_vmid, "EXEC_COUNT", BITS(fetch_word(asic, stream, 3), 0, 14), NULL, 10, 32);
			break;
		case 0x27: // DRAW_INDEX_2
			ui->add_field(ui, ib_addr + 4, ib_vmid, "MAX_SIZE", fetch_word(asic, stream, 0), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "INDEX_BASE_LO", fetch_word(asic, stream, 1), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "INDEX_BASE_HI", fetch_word(asic, stream, 2), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 16, ib_vmid, "INDEX_COUNT", fetch_word(asic, stream, 3), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 20, ib_vmid, "DRAW_INITIATOR", fetch_word(asic, stream, 4), NULL, 10, 32);
			break;
		case 0x28: // CONTEXT_CONTROL
			ui->add_field(ui, ib_addr + 4, ib_vmid, "LOAD_EN", BITS(fetch_word(asic, stream, 0), 31, 32), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "LOAD_CS", BITS(fetch_word(asic, stream, 0), 24, 25), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "LOAD_GFX", BITS(fetch_word(asic, stream, 0), 16, 17), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "LOAD_GLOBAL", BITS(fetch_word(asic, stream, 0), 15, 16), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "LOAD_MULTI", BITS(fetch_word(asic, stream, 0), 1, 2), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "LOAD_SINGLE", BITS(fetch_word(asic, stream, 0), 0, 1), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "SHADOW_EN", BITS(fetch_word(asic, stream, 1), 31, 32), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "SHADOW_CS", BITS(fetch_word(asic, stream, 1), 24, 25), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "SHADOW_GFX", BITS(fetch_word(asic, stream, 1), 16, 17), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "SHADOW_GLOBAL", BITS(fetch_word(asic, stream, 1), 15, 16), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "SHADOW_MULTI", BITS(fetch_word(asic, stream, 1), 1, 2), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "SHADOW_SINGLE", BITS(fetch_word(asic, stream, 1), 0, 1), NULL, 10, 32);
			break;
		case 0x2D: // DRAW_INDEX_AUTO
			ui->add_field(ui, ib_addr + 4, ib_vmid, "INDEX_COUNT", fetch_word(asic, stream, 0), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "DRAW_INITIATOR", fetch_word(asic, stream, 1), NULL, 10, 32);
			break;
		case 0x2F: // NUM_INSTANCES
			ui->add_field(ui, ib_addr + 4, ib_vmid, "NUM_INSTANCES", fetch_word(asic, stream, 0), NULL, 16, 32);
			break;
		case 0x33: // INDIRECT_BUFFER_CONST
		case 0x3F: // INDIRECT_BUFFER_CIK
			if (stream->opcode == 0x3F && stream->n_words == 13) {
				// COND packet
				ui->add_field(ui, ib_addr + 4, ib_vmid, "MODE", BITS(fetch_word(asic, stream, 0), 0, 2), NULL, 10, 32);
				ui->add_field(ui, ib_addr + 4, ib_vmid, "FUNCTION", BITS(fetch_word(asic, stream, 0), 8, 11), NULL, 10, 32);
				ui->add_field(ui, ib_addr + 8, ib_vmid, "COMPARE_ADDR_LO", BITS(fetch_word(asic, stream, 1), 3, 32), NULL, 16, 32);
				ui->add_field(ui, ib_addr + 12, ib_vmid, "COMPARE_ADDR_HI", fetch_word(asic, stream, 2), NULL, 16, 32);
				ui->add_field(ui, ib_addr + 16, ib_vmid, "MASK_LO", fetch_word(asic, stream, 3), NULL, 16, 32);
				ui->add_field(ui, ib_addr + 20, ib_vmid, "MASK_HI", fetch_word(asic, stream, 4), NULL, 16, 32);
				ui->add_field(ui, ib_addr + 24, ib_vmid, "REFERENCE_LO", fetch_word(asic, stream, 5), NULL, 16, 32);
				ui->add_field(ui, ib_addr + 28, ib_vmid, "REFERENCE_HI", fetch_word(asic, stream, 6), NULL, 16, 32);
				ui->add_field(ui, ib_addr + 32, ib_vmid, "IB_BASE1_LO", BITS(fetch_word(asic, stream, 7), 2, 32), NULL, 16, 32);
				ui->add_field(ui, ib_addr + 36, ib_vmid, "IB_BASE1_HI", fetch_word(asic, stream, 8), NULL, 16, 32);
				ui->add_field(ui, ib_addr + 40, ib_vmid, "IB_SIZE1", BITS(fetch_word(asic, stream, 9), 0, 20), NULL, 16, 32);
				ui->add_field(ui, ib_addr + 40, ib_vmid, "CACHE_POLICY1", BITS(fetch_word(asic, stream, 9), 28, 30), NULL, 10, 32);
				ui->add_field(ui, ib_addr + 44, ib_vmid, "IB_BASE2_LO", BITS(fetch_word(asic, stream, 10), 2, 32), NULL, 16, 32);
				ui->add_field(ui, ib_addr + 48, ib_vmid, "IB_BASE2_HI", fetch_word(asic, stream, 11), NULL, 16, 32);
				ui->add_field(ui, ib_addr + 52, ib_vmid, "IB_SIZE2", BITS(fetch_word(asic, stream, 12), 0, 20), NULL, 16, 32);
				ui->add_field(ui, ib_addr + 52, ib_vmid, "CACHE_POLICY2", BITS(fetch_word(asic, stream, 12), 28, 30), NULL, 10, 32);
			} else {
				// not COND packet
				ui->add_field(ui, ib_addr + 4, ib_vmid, "IB_BASE_LO", BITS(fetch_word(asic, stream, 0), 2, 32) << 2, NULL, 16, 32);
				ui->add_field(ui, ib_addr + 4, ib_vmid, "SWAP", BITS(fetch_word(asic, stream, 0), 0, 2), NULL, 10, 32);
				ui->add_field(ui, ib_addr + 8, ib_vmid, "IB_BASE_HI", BITS(fetch_word(asic, stream, 1), 0, 16), NULL, 16, 32);
				ui->add_field(ui, ib_addr + 12, ib_vmid, "IB_SIZE", BITS(fetch_word(asic, stream, 2), 0, 20), NULL, 10, 32);
				ui->add_field(ui, ib_addr + 12, ib_vmid, "IB_VMID", BITS(fetch_word(asic, stream, 2), 24, 28), NULL, 10, 32);
				ui->add_field(ui, ib_addr + 12, ib_vmid, "CHAIN", BITS(fetch_word(asic, stream, 2), 20, 21), NULL, 10, 32);
				ui->add_field(ui, ib_addr + 12, ib_vmid, "PRE_ENA", BITS(fetch_word(asic, stream, 2), 21, 22), NULL, 10, 32);
				ui->add_field(ui, ib_addr + 12, ib_vmid, "CACHE_POLICY", BITS(fetch_word(asic, stream, 2), 28, 30), NULL, 10, 32);
				ui->add_field(ui, ib_addr + 12, ib_vmid, "PRE_RESUME", BITS(fetch_word(asic, stream, 2), 30, 31), NULL, 10, 32);
				ui->add_field(ui, ib_addr + 12, ib_vmid, "PRIV", BITS(fetch_word(asic, stream, 2), 31, 32), NULL, 10, 32);
			}
			break;
		case 0x37: // WRITE_DATA
			ui->add_field(ui, ib_addr + 4, ib_vmid, "ENGINE", BITS(fetch_word(asic, stream, 0), 30, 32), op_37_engines[BITS(fetch_word(asic, stream, 0), 30, 32)], 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "WR_CONFIRM", BITS(fetch_word(asic, stream, 0), 20, 21), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "RESUME_VF", BITS(fetch_word(asic, stream, 0), 19, 20), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "WR_ONE_ADDR", BITS(fetch_word(asic, stream, 0), 16, 17), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "DST_SEL", BITS(fetch_word(asic, stream, 0), 8, 12), op_37_dst_sel[BITS(fetch_word(asic, stream, 0), 8, 12)],  10, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_ADDR_LO", BITS(fetch_word(asic, stream, 1), 2, 32) << 2, NULL, 16, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "DST_ADDR_HI", fetch_word(asic, stream, 2), NULL, 16, 32);
			if (BITS(fetch_word(asic, stream, 0), 8, 12) == 0) { // mem-mapped reg
				uint32_t n;
				uint64_t reg_addr = ((uint64_t)fetch_word(asic, stream, 2) << 32) | fetch_word(asic, stream, 1);
				for (n = 3; n < stream->n_words; n++) {
					ui->add_field(ui, ib_addr + 16 + (n - 3) * 4, ib_vmid, umr_reg_name(asic, reg_addr), fetch_word(asic, stream, n), NULL, 16, 32);
					reg_addr += 1;
				}
			}
			break;
		case 0x3C: // WAIT_REG_MEM
			ui->add_field(ui, ib_addr + 4, ib_vmid, "ENGINE", BITS(fetch_word(asic, stream, 0), 8, 9), BITS(fetch_word(asic, stream, 0), 8, 9) ? "PFP" : "ME", 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "MEMSPACE", BITS(fetch_word(asic, stream, 0), 4, 5), BITS(fetch_word(asic, stream, 0), 4, 5) ? "MEM" : "REG", 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "OPERATION", BITS(fetch_word(asic, stream, 0), 6, 8), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "FUNCTION", BITS(fetch_word(asic, stream, 0), 0, 3), op_3c_functions[BITS(fetch_word(asic, stream, 0), 0, 3)], 10, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "POLL_ADDRESS_LO", BITS(fetch_word(asic, stream, 1), 2, 32) << 2, NULL, 16, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "POLL_ADDRESS_HI", fetch_word(asic, stream, 2), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 16, ib_vmid, "REFERENCE", fetch_word(asic, stream, 3), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 20, ib_vmid, "MASK", fetch_word(asic, stream, 4), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 24, ib_vmid, "POLL INTERVAL", fetch_word(asic, stream, 5), NULL, 16, 32);
			break;
		case 0x40: // PKT3_COPY_DATA
			ui->add_field(ui, ib_addr + 4, ib_vmid, "SRC_SEL", BITS(fetch_word(asic, stream, 0), 0, 4), op_40_mem_sel[BITS(fetch_word(asic, stream, 0), 0, 4)], 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "DST_SEL", BITS(fetch_word(asic, stream, 0), 8, 12), op_40_mem_sel[BITS(fetch_word(asic, stream, 0), 8, 12)], 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "SRC_CACHE_POLICY", BITS(fetch_word(asic, stream, 0), 13, 15), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "COUNT_SEL", BITS(fetch_word(asic, stream, 0), 16, 17), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "WR_CONFIRM", BITS(fetch_word(asic, stream, 0), 20, 21), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "DST_CACHE_POLICY", BITS(fetch_word(asic, stream, 0), 25, 27), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "PQ_EXE_STATUS", BITS(fetch_word(asic, stream, 0), 29, 30), NULL, 10, 32);

			switch (BITS(fetch_word(asic, stream, 0), 0, 4)) {
				case 0: ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_REG_OFFSET", BITS(fetch_word(asic, stream, 1), 0, 18), umr_reg_name(asic, BITS(fetch_word(asic, stream, 1), 0, 18)), 16, 32); break;
				case 5: ui->add_field(ui, ib_addr + 8, ib_vmid, "IMM_DATA", fetch_word(asic, stream, 1), NULL, 16, 32); break;
				default: ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_ADDR_LO", BITS(fetch_word(asic, stream, 1), 2, 32) << 2, NULL, 16, 32); break;
			}

			if (BITS(fetch_word(asic, stream, 0), 0, 4) == 5 && BITS(fetch_word(asic, stream, 0), 16, 17) == 1)
				ui->add_field(ui, ib_addr + 12, ib_vmid, "IMM_DATA_HI", fetch_word(asic, stream, 2), NULL, 16, 32);
			else
				ui->add_field(ui, ib_addr + 12, ib_vmid, "SRC_DATA_HI", fetch_word(asic, stream, 2), NULL, 16, 32);

			switch (BITS(fetch_word(asic, stream, 0), 0, 4)) {
				case 0: ui->add_field(ui, ib_addr + 16, ib_vmid, "DST_REG_OFFSET", BITS(fetch_word(asic, stream, 3), 0, 18), umr_reg_name(asic, BITS(fetch_word(asic, stream, 3), 0, 18)), 16, 32); break;
				default: ui->add_field(ui, ib_addr + 16, ib_vmid, "DST_ADDR_LO", fetch_word(asic, stream, 3), NULL, 16, 32); break;
			}
			ui->add_field(ui, ib_addr + 20, ib_vmid, "DST_ADDR_HI", fetch_word(asic, stream, 4), NULL, 16, 32);
			break;
		case 0x42: // PFP_SYNC_ME
			ui->add_field(ui, ib_addr + 4, ib_vmid, "DUMMY_DATA", fetch_word(asic, stream, 0), NULL, 16, 32);
			break;
		case 0x43: // SURFACE_SYNC
			ui->add_field(ui, ib_addr + 4, ib_vmid, "ENGINE", BITS(fetch_word(asic, stream, 0), 31, 32), BITS(fetch_word(asic, stream, 0), 31, 32) ? "ME" : "PFP", 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "COHER_CNTL", BITS(fetch_word(asic, stream, 0), 0, 29), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "COHER_SIZE", fetch_word(asic, stream, 1), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "COHER_BASE", fetch_word(asic, stream, 2), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 16, ib_vmid, "POLL_INTERVAL", BITS(fetch_word(asic, stream, 3), 0, 16), NULL, 10, 32);
			break;
		case 0x46: // EVENT_WRITE
			ui->add_field(ui, ib_addr + 4, ib_vmid, "EVENT_TYPE", BITS(fetch_word(asic, stream, 0), 0, 6), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "EVENT_INDEX", BITS(fetch_word(asic, stream, 0), 8, 12), NULL, 10, 32);
			if (stream->n_words > 2) {
				ui->add_field(ui, ib_addr + 8, ib_vmid, "ADDRESS_LO", BITS(fetch_word(asic, stream, 1), 3, 32) << 3, NULL, 16, 32);
				ui->add_field(ui, ib_addr + 12, ib_vmid, "ADDRESS_HI", fetch_word(asic, stream, 2), NULL, 16, 32);
			}
			break;
		case 0x47: // EVENT_WRITE_EOP
			ui->add_field(ui, ib_addr + 4, ib_vmid, "EVENT_TYPE", BITS(fetch_word(asic, stream, 0), 0, 6), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "EVENT_INDEX", BITS(fetch_word(asic, stream, 0), 8, 12), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "INV_L2", BITS(fetch_word(asic, stream, 0), 20, 21), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "ADDRESS_LO", BITS(fetch_word(asic, stream, 1), 2, 32) << 2, NULL, 16, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "ADDRESS_HI", BITS(fetch_word(asic, stream, 2), 0, 16), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "DATA_SEL", BITS(fetch_word(asic, stream, 2), 29, 32), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "INT_SEL", BITS(fetch_word(asic, stream, 2), 24, 26), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 16, ib_vmid, "DATA_LO", fetch_word(asic, stream, 3), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 20, ib_vmid, "DATA_HI", fetch_word(asic, stream, 4), NULL, 16, 32);
			break;
		case 0x49: // RELEASE_MEM
			ui->add_field(ui, ib_addr + 4, ib_vmid, "EVENT_TYPE", BITS(fetch_word(asic, stream, 0), 0, 6), vgt_event_decode(BITS(fetch_word(asic, stream, 0), 0, 6)), 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "EVENT_INDEX", BITS(fetch_word(asic, stream, 0), 8, 12), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "TCL1_VOL_ACTION_ENA", BITS(fetch_word(asic, stream, 0), 12, 13), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "TC_VOL_ACTION_ENA", BITS(fetch_word(asic, stream, 0), 13, 14), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "TC_WB_ACTION_ENA", BITS(fetch_word(asic, stream, 0), 15, 16), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "TCL1_ACTION_ENA", BITS(fetch_word(asic, stream, 0), 16, 17), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "TC_ACTION_ENA", BITS(fetch_word(asic, stream, 0), 17, 18), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "TC_NC_ACTION_ENA", BITS(fetch_word(asic, stream, 0), 19, 20), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "CACHE_POLICY", BITS(fetch_word(asic, stream, 0), 25, 27), NULL, 10, 32);

			ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_SEL", BITS(fetch_word(asic, stream, 1), 16, 18), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "INT_SEL", BITS(fetch_word(asic, stream, 1), 24, 27), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "DATA_SEL", BITS(fetch_word(asic, stream, 1), 29, 32), NULL, 10, 32);

			ui->add_field(ui, ib_addr + 12, ib_vmid, "ADDR_LO", fetch_word(asic, stream, 2), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 16, ib_vmid, "ADDR_HI", fetch_word(asic, stream, 3), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 20, ib_vmid, "DATA_LO", fetch_word(asic, stream, 4), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 24, ib_vmid, "DATA_HI", fetch_word(asic, stream, 5), NULL, 16, 32);
			break;
		case 0x4A: // PREAMBLE_CNTL
			ui->add_field(ui, ib_addr + 4, ib_vmid, "COMMAND", BITS(fetch_word(asic, stream, 0), 28, 32), NULL, 16, 32);
			break;
		case 0x50: // DMA_DATA
			ui->add_field(ui, ib_addr + 4, ib_vmid, "ENGINE_SEL", BITS(fetch_word(asic, stream, 0), 0, 1), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "SRC_CACHE_POLICY", BITS(fetch_word(asic, stream, 0), 13, 15), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "DST_SEL", BITS(fetch_word(asic, stream, 0), 20, 22), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "DST_CACHE_POLICY", BITS(fetch_word(asic, stream, 0), 25, 27), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "SRC_SEL", BITS(fetch_word(asic, stream, 0), 29, 31), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "CP_SYNC", BITS(fetch_word(asic, stream, 0), 31, 32), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_ADDR_LO_OR_DATA", fetch_word(asic, stream, 1), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "SRC_ADDR_HI", fetch_word(asic, stream, 2), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 16, ib_vmid, "DST_ADDR_LO", fetch_word(asic, stream, 3), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 20, ib_vmid, "DST_ADDR_HI", fetch_word(asic, stream, 4), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 24, ib_vmid, "BYTE_COUNT", BITS(fetch_word(asic, stream, 5), 0, 21), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 24, ib_vmid, "DIS_WC", BITS(fetch_word(asic, stream, 5), 21, 22), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 24, ib_vmid, "SAS", BITS(fetch_word(asic, stream, 5), 26, 27), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 24, ib_vmid, "DAS", BITS(fetch_word(asic, stream, 5), 27, 28), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 24, ib_vmid, "SAIC", BITS(fetch_word(asic, stream, 5), 28, 29), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 24, ib_vmid, "DAIC", BITS(fetch_word(asic, stream, 5), 29, 30), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 24, ib_vmid, "RAW_WAIT", BITS(fetch_word(asic, stream, 5), 30, 31), NULL, 10, 32);
			break;
		case 0x58: // ACQUIRE_MEM
			ui->add_field(ui, ib_addr + 4, ib_vmid, "ENGINE", BITS(fetch_word(asic, stream, 0), 31, 32), BITS(fetch_word(asic, stream, 0), 31, 32) ? "ME" : "PFP", 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "COHER_CNTL", BITS(fetch_word(asic, stream, 0), 0, 30), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "CP_COHER_SIZE", fetch_word(asic, stream, 1), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "CP_COHER_SIZE_HI", BITS(fetch_word(asic, stream, 2), 0, 8), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 16, ib_vmid, "CP_COHER_BASE", fetch_word(asic, stream, 3), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 20, ib_vmid, "CP_COHER_BASE_HI", BITS(fetch_word(asic, stream, 4), 0, 25), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 24, ib_vmid, "POLL_INTERVAL", BITS(fetch_word(asic, stream, 5), 0, 16), NULL, 10, 32);
			break;
		case 0x5E: // LOAD_UCONFIG_REG
		case 0x5F: // LOAD_SH_REG
		case 0x60: // LOAD_CONFIG_REG
		case 0x61: // LOAD_CONTEXT_REG
			load_X_reg(asic, ui, stream, ib_addr, ib_vmid);
			break;
		case 0x63: // LOAD_SH_REG_INDEX
			if (BITS(fetch_word(asic, stream, 0), 0, 1))
				ui->add_field(ui, ib_addr + 4, ib_vmid, "INDEX", 1, NULL, 10, 32);
			else
				ui->add_field(ui, ib_addr + 4, ib_vmid, "MEM_ADDR_LO", BITS(fetch_word(asic, stream, 0), 0, 31) & ~0x3UL, NULL, 16, 32);
			if (BITS(fetch_word(asic, stream, 0), 0, 1))
				ui->add_field(ui, ib_addr + 8, ib_vmid, "SH_BASE_ADDR", fetch_word(asic, stream, 1), NULL, 16, 32);
			else
				ui->add_field(ui, ib_addr + 8, ib_vmid, "MEM_ADDR_HI", fetch_word(asic, stream, 1), NULL, 16, 32);
			if (!BITS(fetch_word(asic, stream, 2), 31, 32))
				ui->add_field(ui, ib_addr + 12, ib_vmid, "REG", BITS(fetch_word(asic, stream, 2), 0, 16), umr_reg_name(asic, 0x2C00 + BITS(fetch_word(asic, stream, 2), 0, 16)), 16, 32);
			ui->add_field(ui, ib_addr + 16, ib_vmid, "NUM_DWORDS", fetch_word(asic, stream, 3), NULL, 10, 32);
			break;
		case 0x68: // SET_CONFIG_REG
			{
				uint64_t addr = BITS(fetch_word(asic, stream, 0), 0, 16) + 0x2000;
				uint32_t n;
				for (n = 1; n < stream->n_words; n++) {
					ui->add_field(ui, ib_addr + 4 + 4 * n, ib_vmid, "REG", fetch_word(asic, stream, n), umr_reg_name(asic, addr), 16, 32);
					addr += 1;
				}
			}
			break;
		case 0x69: // SET_CONTEXT_REG
			{
				uint64_t addr = BITS(fetch_word(asic, stream, 0), 0, 16) + 0xA000;
				uint32_t n;
				for (n = 1; n < stream->n_words; n++) {
					ui->add_field(ui, ib_addr + 4 + 4 * n, ib_vmid, "REG", fetch_word(asic, stream, n), umr_reg_name(asic, addr), 16, 32);
					addr += 1;
				}
			}
			break;
		case 0x76: // SET_SH_REG
			{
				uint64_t addr = BITS(fetch_word(asic, stream, 0), 0, 16) + 0x2C00;
				uint32_t n;
				for (n = 1; n < stream->n_words; n++) {
					ui->add_field(ui, ib_addr + 4 + 4 * n, ib_vmid, "REG", fetch_word(asic, stream, n), umr_reg_name(asic, addr), 16, 32);
					addr += 1;
				}
			}
			break;
		case 0x79: // SET_UCONTEXT_REG
			{
				uint64_t addr = BITS(fetch_word(asic, stream, 0), 0, 16) + 0xC000;
				uint32_t n;
				for (n = 1; n < stream->n_words; n++) {
					ui->add_field(ui, ib_addr + 4 + 4 * n, ib_vmid, "REG", fetch_word(asic, stream, n), umr_reg_name(asic, addr), 16, 32);
					addr += 1;
				}
			}
			break;
		case 0x7A: // SET_UCONFIG_REG_INDEX
			{
				uint64_t addr = BITS(fetch_word(asic, stream, 0), 0, 16) + 0xC000;
				uint32_t n;
				ui->add_field(ui, ib_addr + 4, ib_vmid, "INDEX", BITS(fetch_word(asic, stream, 0), 28, 32), op_7a_index_str[BITS(fetch_word(asic, stream, 0), 28, 32)], 10, 32);
				for (n = 1; n < stream->n_words; n++) {
					ui->add_field(ui, ib_addr + 4 + 4 * n, ib_vmid, "REG", fetch_word(asic, stream, n), umr_reg_name(asic, addr), 16, 32);
					addr += 1;
				}
			}
			break;
		case 0x80: // LOAD_CONST_RAM
			ui->add_field(ui, ib_addr + 4, ib_vmid, "ADDR_LO", fetch_word(asic, stream, 0), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "ADDR_HI", fetch_word(asic, stream, 1), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "NUM_DW", fetch_word(asic, stream, 2), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 16, ib_vmid, "START_ADDR", BITS(fetch_word(asic, stream, 3), 0, 16), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 16, ib_vmid, "CACHE_POLICY", BITS(fetch_word(asic, stream, 3), 25, 27), NULL, 10, 32);
			break;
		case 0x81: // WRITE_CONST_RAM
			{
				uint32_t addr = BITS(fetch_word(asic, stream, 0), 0, 16);
				uint32_t n;
				char buf[32];
				for (n = 1; n < stream->n_words; n++) {
					sprintf(buf, "CONST_RAM[%lx]", (unsigned long)addr);
					ui->add_field(ui, ib_addr + 4 + 4 * n, ib_vmid, buf, fetch_word(asic, stream, n), NULL, 16, 32);
					addr += 4;
				}
			}
			break;
		case 0x83: // DUMP_CONST_RAM
			ui->add_field(ui, ib_addr + 4, ib_vmid, "OFFSET", BITS(fetch_word(asic, stream, 0), 0, 16), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "CACHE_POLICY", BITS(fetch_word(asic, stream, 0), 25, 26), BITS(fetch_word(asic, stream, 0), 25, 26) ? "stream" : "lru", 10, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "NUM_DW", BITS(fetch_word(asic, stream, 1), 0, 15), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "ADDR_LO", fetch_word(asic, stream, 2), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 16, ib_vmid, "ADDR_HI", fetch_word(asic, stream, 3), NULL, 16, 32);
			break;
		case 0x84: // INCREMENT_CE_COUNTER
			ui->add_field(ui, ib_addr + 4, ib_vmid, "CNTRSEL", BITS(fetch_word(asic, stream, 0), 0, 2), op_84_cntr_sel[BITS(fetch_word(asic, stream, 0), 0, 2)], 10, 32);
			break;
		case 0x86: // WAIT_ON_CE_COUNTER
			ui->add_field(ui, ib_addr + 4, ib_vmid, "COND_ACQUIRE_MEM", BITS(fetch_word(asic, stream, 0), 0, 1), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "FORCE_SYNC", BITS(fetch_word(asic, stream, 0), 1, 2), NULL, 10, 32);
			break;
		case 0x8B: // SWITCH_BUFFER
			ui->add_field(ui, ib_addr + 4, ib_vmid, "DUMMY", fetch_word(asic, stream, 0), NULL, 16, 32);
			break;
		case 0x9B: // SET_SH_REG_INDEX
			{
				uint64_t addr = BITS(fetch_word(asic, stream, 0), 0, 16) + 0x2C00;
				uint32_t n;
				ui->add_field(ui, ib_addr + 4, ib_vmid, "INDEX", BITS(fetch_word(asic, stream, 0), 28, 32), NULL, 10, 32);
				for (n = 1; n < stream->n_words; n++) {
					ui->add_field(ui, ib_addr + 4 + 4 * n, ib_vmid, umr_reg_name(asic, addr), fetch_word(asic, stream, n), NULL, 16, 32);
					addr += 1;
				}
			}
			break;
		case 0x9F: // LOAD_CONTEXT_REG_INDEX
			{
				uint32_t index = BITS(fetch_word(asic, stream, 0), 0, 1);
				if (index)
					ui->add_field(ui, ib_addr + 4, ib_vmid, "INDEX", index, NULL, 10, 32);
				else
					ui->add_field(ui, ib_addr + 4, ib_vmid, "MEM_ADDR_LO", BITS(fetch_word(asic, stream, 0), 0, 32) & ~0x3UL, NULL, 16, 32);
				if (index)
					ui->add_field(ui, ib_addr + 8, ib_vmid, "CONTEXT_BASE_ADDR", fetch_word(asic, stream, 1), NULL, 16, 32);
				else
					ui->add_field(ui, ib_addr + 8, ib_vmid, "MEM_ADDR_HI", fetch_word(asic, stream, 1), NULL, 16, 32);
				if (BITS(fetch_word(asic, stream, 2), 31, 32))
					ui->add_field(ui, ib_addr + 12, ib_vmid, "REG", BITS(fetch_word(asic, stream, 2), 0, 16), umr_reg_name(asic, 0xA000 + BITS(fetch_word(asic, stream, 2), 0, 16)), 16, 32);
				ui->add_field(ui, ib_addr + 16, ib_vmid, "NUM_DWORDS", BITS(fetch_word(asic, stream, 3), 0, 14), NULL, 10, 32);
				if (BITS(fetch_word(asic, stream, 2), 31, 32)) {
					uint32_t n;
					uint32_t addr = 0xA000 + BITS(fetch_word(asic, stream, 2), 0, 16);
					for (n = 4; n < stream->n_words; n++)
						ui->add_field(ui, ib_addr + n * 4, ib_vmid, umr_reg_name(asic, addr + n - 4), fetch_word(asic, stream, n), NULL, 16, 32);
				}
			}
			break;
		case 0xA0: // SET_RESOURCES
			ui->add_field(ui, ib_addr + 4, ib_vmid, "VMID_MASK", BITS(fetch_word(asic, stream, 0), 0, 16), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "QUEUE_TYPE", BITS(fetch_word(asic, stream, 0), 29, 32), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "QUEUE_MASK_LO", fetch_word(asic, stream, 1), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "QUEUE_MASK_HI", fetch_word(asic, stream, 2), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 16, ib_vmid, "GWS_MASK_LO", fetch_word(asic, stream, 3), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 20, ib_vmid, "GWS_MASK_HI", fetch_word(asic, stream, 4), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 24, ib_vmid, "OAC_MASK", BITS(fetch_word(asic, stream, 5), 0, 16), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 28, ib_vmid, "GDS_HEAP_BASE", BITS(fetch_word(asic, stream, 6), 0, 6), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 28, ib_vmid, "GDS_HEAP_SIZE", BITS(fetch_word(asic, stream, 6), 11, 17), NULL, 10, 32);
			break;
		case 0xA1: // PKT3_MAP_PROCESS
			ui->add_field(ui, ib_addr + 4, ib_vmid, "PASID", BITS(fetch_word(asic, stream, 0), 0, 16), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "DIQ_ENABLE", BITS(fetch_word(asic, stream, 0), 24, 25), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "PAGE_TABLE_BASE", BITS(fetch_word(asic, stream, 1), 0, 28), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "SH_MEM_BASES", fetch_word(asic, stream, 2), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 16, ib_vmid, "SH_MEM_APE1_BASE", fetch_word(asic, stream, 3), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 20, ib_vmid, "SH_MEM_APE1_LIMIT", fetch_word(asic, stream, 4), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 24, ib_vmid, "SH_MEM_CONFIG", fetch_word(asic, stream, 5), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 28, ib_vmid, "GDS_ADDR_LO", fetch_word(asic, stream, 6), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 32, ib_vmid, "GDS_ADDR_HI", fetch_word(asic, stream, 7), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 36, ib_vmid, "NUM_GWS", BITS(fetch_word(asic, stream, 8), 0, 6), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 36, ib_vmid, "NUM_OAC", BITS(fetch_word(asic, stream, 8), 8, 12), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 36, ib_vmid, "GDS_SIZE", BITS(fetch_word(asic, stream, 8), 16, 22), NULL, 10, 32);
			break;
		case 0xA2: // PKT3_MAP_QUEUES
			ui->add_field(ui, ib_addr + 4, ib_vmid, "QUEUE_SEL", BITS(fetch_word(asic, stream, 0), 4, 6), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "VMID", BITS(fetch_word(asic, stream, 0), 8, 12), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "VIDMEM", BITS(fetch_word(asic, stream, 0), 16, 18), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "ALLOC_FORMAT", BITS(fetch_word(asic, stream, 0), 24, 26), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "ENGINE_SEL", BITS(fetch_word(asic, stream, 0), 26, 29), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "NUM_QUEUES", BITS(fetch_word(asic, stream, 0), 29, 32), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "DOORBELL_OFFSET", BITS(fetch_word(asic, stream, 1), 2, 23), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "QUEUE", BITS(fetch_word(asic, stream, 1), 26, 32), NULL, 10, 32);
			{
				uint32_t n;
				for (n = 2; n + 4 <= stream->n_words; n += 4) {
					ui->add_field(ui, ib_addr + 12 + 16 * ((n - 2) / 4), ib_vmid, "MQD_ADDR_LO", fetch_word(asic, stream, n), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 16 + 16 * ((n - 2) / 4), ib_vmid, "MQD_ADDR_HI", fetch_word(asic, stream, n + 1), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 20 + 16 * ((n - 2) / 4), ib_vmid, "WPTR_ADDR_LO", fetch_word(asic, stream, n + 2), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 24 + 16 * ((n - 2) / 4), ib_vmid, "WPTR_ADDR_HI", fetch_word(asic, stream, n + 3), NULL, 16, 32);
					if (ui->add_data)
						ui->add_data(ui, asic,
									 ib_addr + 12 + 16 * ((n - 2) / 4), ib_vmid,
									 ((uint64_t)fetch_word(asic, stream, n)) | (((uint64_t)fetch_word(asic, stream, n + 1)) << 32), BITS(fetch_word(asic, stream, 0), 8, 12),
									 UMR_DATABLOCK_MQD_VI, BITS(fetch_word(asic, stream, 0), 26, 29));
				}
			}
			break;
		case 0xA3: // PKT3_UNMAP_QUEUES
			{
				uint32_t queue_sel, num_queues, engine_sel;

				queue_sel = BITS(fetch_word(asic, stream, 0), 4, 6);
				engine_sel = BITS(fetch_word(asic, stream, 0), 26, 29);
				num_queues = BITS(fetch_word(asic, stream, 0), 29, 32);

				ui->add_field(ui, ib_addr + 4, ib_vmid, "ACTION", BITS(fetch_word(asic, stream, 0), 0, 2), NULL, 10, 32);
				ui->add_field(ui, ib_addr + 4, ib_vmid, "QUEUE_SEL", queue_sel, NULL, 10, 32);
				ui->add_field(ui, ib_addr + 4, ib_vmid, "ENGINE_SEL", engine_sel, NULL, 10, 32);
				ui->add_field(ui, ib_addr + 4, ib_vmid, "NUM_QUEUES", num_queues, NULL, 10, 32);
				if (queue_sel == 1)
					ui->add_field(ui, ib_addr + 8, ib_vmid, "PASID", BITS(fetch_word(asic, stream, 1), 0, 16), NULL, 10, 32);
				else
					ui->add_field(ui, ib_addr + 8, ib_vmid, "DOORBELL_OFFSET0", BITS(fetch_word(asic, stream, 1), 2, 23), NULL, 16, 32);
				ui->add_field(ui, ib_addr + 12, ib_vmid, "DOORBELL_OFFSET1", BITS(fetch_word(asic, stream, 2), 2, 23), NULL, 16, 32);
				ui->add_field(ui, ib_addr + 16, ib_vmid, "DOORBELL_OFFSET2", BITS(fetch_word(asic, stream, 3), 2, 23), NULL, 16, 32);
				ui->add_field(ui, ib_addr + 20, ib_vmid, "DOORBELL_OFFSET3", BITS(fetch_word(asic, stream, 4), 2, 23), NULL, 16, 32);
			}
			break;
		case 0xA4: // PKT3_QUERY_STATUS
			ui->add_field(ui, ib_addr + 4, ib_vmid, "CONTEXT_ID", BITS(fetch_word(asic, stream, 0), 0, 28), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "INTERRUPT_SEL", BITS(fetch_word(asic, stream, 0), 28, 30), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "COMMAND", BITS(fetch_word(asic, stream, 0), 30, 32), NULL, 10, 32);
			if (BITS(fetch_word(asic, stream, 0), 28, 30) == 1) {
				ui->add_field(ui, ib_addr + 8, ib_vmid, "PASID", BITS(fetch_word(asic, stream, 1), 0, 16), NULL, 10, 32);
			} else {
				ui->add_field(ui, ib_addr + 8, ib_vmid, "DOORBELL_OFFSET", BITS(fetch_word(asic, stream, 1), 2, 23), NULL, 16, 32);
				ui->add_field(ui, ib_addr + 8, ib_vmid, "ENGINE_SEL", BITS(fetch_word(asic, stream, 1), 26, 29), NULL, 10, 32);
			}
			ui->add_field(ui, ib_addr + 12, ib_vmid, "ADDR_LO", fetch_word(asic, stream, 2), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 16, ib_vmid, "ADDR_HI", fetch_word(asic, stream, 3), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 20, ib_vmid, "DATA_LO", fetch_word(asic, stream, 4), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 24, ib_vmid, "DATA_HI", fetch_word(asic, stream, 5), NULL, 16, 32);
			break;
		case 0xA5:	// PKT3_MES_RUN_LIST
			ui->add_field(ui, ib_addr + 4, ib_vmid, "IB_BASE_LO", BITS(fetch_word(asic, stream, 0), 2, 32) << 2, NULL, 16, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "IB_BASE_HI", fetch_word(asic, stream, 1), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "IB_SIZE", BITS(fetch_word(asic, stream, 2), 0, 20), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "CHAIN", BITS(fetch_word(asic, stream, 2), 20, 21), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "OFFLOAD_POLLING", BITS(fetch_word(asic, stream, 2), 21, 22), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "VALID", BITS(fetch_word(asic, stream, 2), 23, 24), NULL, 10, 32);
			break;
		default:
			if (ui->unhandled)
				ui->unhandled(ui, asic, ib_addr, ib_vmid, stream, UMR_RING_PM4);
			break;
	}
}

static void decode_pkt3_gfx9(struct umr_asic *asic, struct umr_stream_decode_ui *ui, struct umr_pm4_stream *stream, uint64_t ib_addr, uint32_t ib_vmid)
{
	switch (stream->opcode) {
		case 0x11: // SET_BASE
			{
				uint32_t base_index = BITS(fetch_word(asic, stream, 0), 0, 4);
				ui->add_field(ui, ib_addr + 4, ib_vmid, "BASE_INDEX", base_index, NULL, 10, 32);
				if (base_index == 2) {
					ui->add_field(ui, ib_addr + 8, ib_vmid, "ADDRESS_LO", BITS(fetch_word(asic, stream, 1), 3, 32) << 3, NULL, 16, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "ADDRESS_HI", fetch_word(asic, stream, 2), NULL, 16, 32);
				} else if (base_index == 3) {
					ui->add_field(ui, ib_addr + 8, ib_vmid, "CS1_INDEX", BITS(fetch_word(asic, stream, 1), 0, 16), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "CS2_INDEX", BITS(fetch_word(asic, stream, 2), 0, 16), NULL, 16, 32);
				}
			}
			break;
		case 0x12: // CLEAR_STATE
			ui->add_field(ui, ib_addr + 4, ib_vmid, "CMD", BITS(fetch_word(asic, stream, 0), 0, 4), NULL, 10, 32);
			break;
		case 0x13: // INDEX_BUFFER_SIZE
			ui->add_field(ui, ib_addr + 4, ib_vmid, "INDEX_BUFFER_SIZE", fetch_word(asic, stream, 0), NULL, 10, 32);
			break;
		case 0x16: // DISPATCH_INDIRECT
			ui->add_field(ui, ib_addr + 4, ib_vmid, "DATA_OFFSET", fetch_word(asic, stream, 0), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "DISPATCH_INITIATOR", fetch_word(asic, stream, 1), NULL, 16, 32);
			break;
		case 0x1d: // ATOMIC_GDS
			// TODO: fill in
			break;
		case 0x1e: // ATOMIC_MEM
			// TODO: fill in
			break;
		case 0x20: // SET_PREDICATION
			ui->add_field(ui, ib_addr + 4, ib_vmid, "PRED_BOOL", BITS(fetch_word(asic, stream, 0), 8, 9), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "HINT", BITS(fetch_word(asic, stream, 0), 12, 13), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "PRED_OP", BITS(fetch_word(asic, stream, 0), 16, 19), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "CONTINUE_BIT", BITS(fetch_word(asic, stream, 0), 31, 32), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "START_ADDR_LO", BITS(fetch_word(asic, stream, 1), 4, 32) << 4, NULL, 16, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "START_ADDR_HI", fetch_word(asic, stream, 2), NULL, 16, 32);
			break;
		case 0x25: // DRAW_INDEX_INDIRECT
			ui->add_field(ui, ib_addr + 4, ib_vmid, "DATA_OFFSET", fetch_word(asic, stream, 0), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "BASE_VTX_LOC", BITS(fetch_word(asic, stream, 1), 0, 16), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "START_INDX_LOC", BITS(fetch_word(asic, stream, 1), 16, 32), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "START_INST_LOC", BITS(fetch_word(asic, stream, 2), 0, 16), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "START_INDX_ENABLE", BITS(fetch_word(asic, stream, 2), 28, 29), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 16, ib_vmid, "DRAW_INITIATOR", fetch_word(asic, stream, 3), NULL, 16, 32);
			break;
		case 0x26: // INDEX_BASE
			ui->add_field(ui, ib_addr + 4, ib_vmid, "INDEX_BASE_LO", BITS(fetch_word(asic, stream, 0), 1, 32) << 1, NULL, 16, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "INDEX_BASE_HI", fetch_word(asic, stream, 1), NULL, 16, 32);
			break;
		case 0x37: // WRITE_DATA
			ui->add_field(ui, ib_addr + 4, ib_vmid, "ENGINE", BITS(fetch_word(asic, stream, 0), 30, 32), op_37_engines[BITS(fetch_word(asic, stream, 0), 30, 32)], 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "CACHE_POLICY", BITS(fetch_word(asic, stream, 0), 25, 27), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "WR_CONFIRM", BITS(fetch_word(asic, stream, 0), 20, 21), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "RESUME_VF", BITS(fetch_word(asic, stream, 0), 19, 20), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "WR_ONE_ADDR", BITS(fetch_word(asic, stream, 0), 16, 17), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "DST_SEL", BITS(fetch_word(asic, stream, 0), 8, 12), op_37_dst_sel[BITS(fetch_word(asic, stream, 0), 8, 12)],  10, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_ADDR_LO", BITS(fetch_word(asic, stream, 1), 2, 32) << 2, NULL, 16, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "DST_ADDR_HI", fetch_word(asic, stream, 2), NULL, 16, 32);
			if (BITS(fetch_word(asic, stream, 0), 8, 12) == 0) { // mem-mapped reg
				uint32_t n;
				uint64_t reg_addr = ((uint64_t)fetch_word(asic, stream, 2) << 32) | fetch_word(asic, stream, 1);
				for (n = 3; n < stream->n_words; n++) {
					ui->add_field(ui, ib_addr + 16 + (n - 3) * 4, ib_vmid, umr_reg_name(asic, reg_addr), fetch_word(asic, stream, n), NULL, 16, 32);
					reg_addr += 1;
				}
			}
			break;
		case 0x38: // DRAW_INDEX_INDIRECT_MULTI
			ui->add_field(ui, ib_addr + 4, ib_vmid, "DATA_OFFSET", fetch_word(asic, stream, 0), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "BASE_VTX_LOC", BITS(fetch_word(asic, stream, 1), 0, 16), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "START_INDX_LOC", BITS(fetch_word(asic, stream, 1), 16, 32), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "START_INST_LOC", BITS(fetch_word(asic, stream, 2), 0, 16), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 16, ib_vmid, "DRAW_INDEX_LOC", BITS(fetch_word(asic, stream, 3), 0, 16), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 16, ib_vmid, "START_INDX_ENABLE", BITS(fetch_word(asic, stream, 3), 28, 29), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 16, ib_vmid, "COUNT_INDIRECT_ENABLE", BITS(fetch_word(asic, stream, 3), 30, 31), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 16, ib_vmid, "DRAW_INDEX_ENABLE", BITS(fetch_word(asic, stream, 3), 31, 32), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 20, ib_vmid, "COUNT", fetch_word(asic, stream, 4), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 24, ib_vmid, "COUNT_ADDR_LO", BITS(fetch_word(asic, stream, 5), 2, 32) << 2, NULL, 16, 32);
			ui->add_field(ui, ib_addr + 28, ib_vmid, "COUNT_ADDR_HI", fetch_word(asic, stream, 6), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 32, ib_vmid, "STRIDE", fetch_word(asic, stream, 7), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 36, ib_vmid, "DRAW_INITIATOR", fetch_word(asic, stream, 8), NULL, 16, 32);
			break;
		case 0x46: // EVENT_WRITE
			ui->add_field(ui, ib_addr + 4, ib_vmid, "EVENT_TYPE", BITS(fetch_word(asic, stream, 0), 0, 6), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "EVENT_INDEX", BITS(fetch_word(asic, stream, 0), 8, 12), NULL, 10, 32);
			if (stream->n_words > 2) {
				ui->add_field(ui, ib_addr + 8, ib_vmid, "ADDRESS_LO", BITS(fetch_word(asic, stream, 1), 3, 32) << 3, NULL, 16, 32);
				ui->add_field(ui, ib_addr + 12, ib_vmid, "ADDRESS_HI", fetch_word(asic, stream, 2), NULL, 16, 32);
			}
			break;
		case 0x47: // EVENT_WRITE_EOP
			ui->add_field(ui, ib_addr + 4, ib_vmid, "EVENT_TYPE", BITS(fetch_word(asic, stream, 0), 0, 6), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "EVENT_INDEX", BITS(fetch_word(asic, stream, 0), 8, 12), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "INV_L2", BITS(fetch_word(asic, stream, 0), 20, 21), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "ADDRESS_LO", BITS(fetch_word(asic, stream, 1), 2, 32) << 2, NULL, 16, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "ADDRESS_HI", BITS(fetch_word(asic, stream, 2), 0, 16), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "DATA_SEL", BITS(fetch_word(asic, stream, 2), 29, 32), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "INT_SEL", BITS(fetch_word(asic, stream, 2), 24, 26), NULL, 10, 32);
			if (BITS(fetch_word(asic, stream, 0), 8, 12) != 1) {
				ui->add_field(ui, ib_addr + 16, ib_vmid, "DATA_LO", fetch_word(asic, stream, 3), NULL, 16, 32);
				ui->add_field(ui, ib_addr + 20, ib_vmid, "DATA_HI", fetch_word(asic, stream, 4), NULL, 16, 32);
			} else {
				ui->add_field(ui, ib_addr + 16, ib_vmid, "COUNTER_ID", BITS(fetch_word(asic, stream, 3), 3, 9), NULL, 10, 32);
				ui->add_field(ui, ib_addr + 16, ib_vmid, "STRIDE", BITS(fetch_word(asic, stream, 3), 9, 11), NULL, 10, 32);
				ui->add_field(ui, ib_addr + 16, ib_vmid, "INSTANCE_ENABLE", BITS(fetch_word(asic, stream, 3), 11, 27), NULL, 10, 32);
			}
			break;
		case 0x49: // RELEASE_MEM
			ui->add_field(ui, ib_addr + 4, ib_vmid, "EVENT_TYPE", BITS(fetch_word(asic, stream, 0), 0, 6), vgt_event_decode(BITS(fetch_word(asic, stream, 0), 0, 6)), 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "EVENT_INDEX", BITS(fetch_word(asic, stream, 0), 8, 12), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "TCL1_VOL_ACTION_ENA", BITS(fetch_word(asic, stream, 0), 12, 13), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "TC_VOL_ACTION_ENA", BITS(fetch_word(asic, stream, 0), 13, 14), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "TC_WB_ACTION_ENA", BITS(fetch_word(asic, stream, 0), 15, 16), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "TCL1_ACTION_ENA", BITS(fetch_word(asic, stream, 0), 16, 17), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "TC_ACTION_ENA", BITS(fetch_word(asic, stream, 0), 17, 18), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "TC_NC_ACTION_ENA", BITS(fetch_word(asic, stream, 0), 19, 20), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "TC_WC_ACTION_ENA", BITS(fetch_word(asic, stream, 0), 20, 21), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "TC_MD_ACTION_ENA", BITS(fetch_word(asic, stream, 0), 21, 22), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "CACHE_POLICY", BITS(fetch_word(asic, stream, 0), 25, 27), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "EXECUTE", BITS(fetch_word(asic, stream, 0), 28, 29), NULL, 10, 32);

			ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_SEL", BITS(fetch_word(asic, stream, 1), 16, 18), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "INT_SEL", BITS(fetch_word(asic, stream, 1), 24, 27), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "DATA_SEL", BITS(fetch_word(asic, stream, 1), 29, 32), NULL, 10, 32);

			ui->add_field(ui, ib_addr + 12, ib_vmid, "ADDR_LO", fetch_word(asic, stream, 2), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 16, ib_vmid, "ADDR_HI", fetch_word(asic, stream, 3), NULL, 16, 32);

			if (BITS(fetch_word(asic, stream, 1), 24, 27) == 5 ||
			    BITS(fetch_word(asic, stream, 1), 24, 27) == 6) {
				ui->add_field(ui, ib_addr + 20, ib_vmid, "CMP_DATA_LO", fetch_word(asic, stream, 4), NULL, 16, 32);
			} else if (BITS(fetch_word(asic, stream, 1), 29, 32) == 5) {
				ui->add_field(ui, ib_addr + 20, ib_vmid, "DW_OFFSET", BITS(fetch_word(asic, stream, 4), 0, 16), NULL, 16, 32);
				ui->add_field(ui, ib_addr + 20, ib_vmid, "NUM_WORDS", BITS(fetch_word(asic, stream, 4), 16, 32), NULL, 10, 32);
			} else {
				ui->add_field(ui, ib_addr + 20, ib_vmid, "DATA_LO", fetch_word(asic, stream, 4), NULL, 16, 32);
			}

			if (BITS(fetch_word(asic, stream, 1), 24, 27) == 5 ||
			    BITS(fetch_word(asic, stream, 1), 24, 27) == 6)
				ui->add_field(ui, ib_addr + 24, ib_vmid, "CMP_DATA_HI", fetch_word(asic, stream, 5), NULL, 16, 32);
			else
				ui->add_field(ui, ib_addr + 24, ib_vmid, "DATA_HI", fetch_word(asic, stream, 5), NULL, 16, 32);

			if (asic->family >= FAMILY_AI)
				ui->add_field(ui, ib_addr + 28, ib_vmid, "INT_CTXID", fetch_word(asic, stream, 6), NULL, 16, 32);
			break;
		case 0x50: // DMA_DATA
			ui->add_field(ui, ib_addr + 4, ib_vmid, "ENGINE_SEL", BITS(fetch_word(asic, stream, 0), 0, 1), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "SRC_CACHE_POLICY", BITS(fetch_word(asic, stream, 0), 13, 15), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "DST_SEL", BITS(fetch_word(asic, stream, 0), 20, 22), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "DST_CACHE_POLICY", BITS(fetch_word(asic, stream, 0), 25, 27), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "SRC_SEL", BITS(fetch_word(asic, stream, 0), 29, 31), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "CP_SYNC", BITS(fetch_word(asic, stream, 0), 31, 32), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_ADDR_LO_OR_DATA", fetch_word(asic, stream, 1), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "SRC_ADDR_HI", fetch_word(asic, stream, 2), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 16, ib_vmid, "DST_ADDR_LO", fetch_word(asic, stream, 3), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 20, ib_vmid, "DST_ADDR_HI", fetch_word(asic, stream, 4), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 24, ib_vmid, "BYTE_COUNT", BITS(fetch_word(asic, stream, 5), 0, 26), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 24, ib_vmid, "SAS", BITS(fetch_word(asic, stream, 5), 26, 27), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 24, ib_vmid, "DAS", BITS(fetch_word(asic, stream, 5), 27, 28), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 24, ib_vmid, "SAIC", BITS(fetch_word(asic, stream, 5), 28, 29), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 24, ib_vmid, "DAIC", BITS(fetch_word(asic, stream, 5), 29, 30), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 24, ib_vmid, "RAW_WAIT", BITS(fetch_word(asic, stream, 5), 30, 31), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 24, ib_vmid, "DIS_WC", BITS(fetch_word(asic, stream, 5), 31, 32), NULL, 10, 32);
			break;
		case 0x51: // CONTEXT_REG_RMW
			ui->add_field(ui, ib_addr + 4, ib_vmid, "REG", fetch_word(asic, stream, 0), umr_reg_name(asic, fetch_word(asic, stream, 0)), 16, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "MASK", fetch_word(asic, stream, 1), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "DATA", fetch_word(asic, stream, 2), NULL, 16, 32);
			break;
		case 0x5D: // PRIME_UTCL2
			ui->add_field(ui, ib_addr + 4, ib_vmid, "CACHE_PERM", BITS(fetch_word(asic, stream, 0), 0, 3), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "PRIME_MODE", BITS(fetch_word(asic, stream, 0), 3, 4), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "ENGINE_SEL", BITS(fetch_word(asic, stream, 0), 30, 32), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "ADDR_LO", fetch_word(asic, stream, 1), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "ADDR_HI", fetch_word(asic, stream, 2), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 16, ib_vmid, "REQUESTED_PAGES", BITS(fetch_word(asic, stream, 3), 0, 14), NULL, 10, 32);
			break;
		case 0x81: // WRITE_CONST_RAM
			{
				uint32_t addr = BITS(fetch_word(asic, stream, 0), 0, 16);
				uint32_t n;
				char buf[32];
				for (n = 1; n < stream->n_words; n++) {
					sprintf(buf, "CONST_RAM[%lx]", (unsigned long)addr);
					ui->add_field(ui, ib_addr + 4 + 4 * n, ib_vmid, buf, fetch_word(asic, stream, n), NULL, 16, 32);
					addr += 4;
				}
			}
			break;
		case 0x86: // WAIT_ON_CE_COUNTER
			ui->add_field(ui, ib_addr + 4, ib_vmid, "COND_ACQUIRE_MEM", BITS(fetch_word(asic, stream, 0), 0, 1), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "FORCE_SYNC", BITS(fetch_word(asic, stream, 0), 1, 2), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "MEM_VOLATILE", BITS(fetch_word(asic, stream, 0), 27, 28), NULL, 10, 32);
			break;
		case 0x8B: // SWITCH_BUFFER
			ui->add_field(ui, ib_addr + 4, ib_vmid, "TMZ", BITS(fetch_word(asic, stream, 0), 0, 1), NULL, 16, 32);
			break;
		case 0x90: // FRAME_CONTROL
			ui->add_field(ui, ib_addr + 4, ib_vmid, "TMZ", BITS(fetch_word(asic, stream, 0), 0, 1), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "COMMAND", BITS(fetch_word(asic, stream, 0), 28, 32), NULL, 10, 32);
			break;
		case 0x91: // INDEX_ATTRIBUTES_INDIRECT
			ui->add_field(ui, ib_addr + 4, ib_vmid, "ATTRIBUTE_BASE_LO", BITS(fetch_word(asic, stream, 0), 4, 32) << 4, NULL, 16, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "ATTRIBUTE_BASE_HI", fetch_word(asic, stream, 1), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "ATTRIBUTE_INDEX", BITS(fetch_word(asic, stream, 2), 0, 16), NULL, 10, 32);
			break;
		case 0x95: // HDP_FLUSH
			ui->add_field(ui, ib_addr + 4, ib_vmid, "DUMMY", fetch_word(asic, stream, 0), NULL, 16, 32);
			break;
		case 0x9A: // DMA_DATA_FILL_MULTI
			ui->add_field(ui, ib_addr + 4, ib_vmid, "ENGINE_SEL", BITS(fetch_word(asic, stream, 0), 0, 1), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "MEMLOG_CLEAR", BITS(fetch_word(asic, stream, 0), 10, 11), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "DST_SEL", BITS(fetch_word(asic, stream, 0), 20, 22), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "DST_CACHE_POLICY", BITS(fetch_word(asic, stream, 0), 25, 27), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "SRC_SEL", BITS(fetch_word(asic, stream, 0), 29, 31), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "CP_SYNC", BITS(fetch_word(asic, stream, 0), 31, 32), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "BYTE_STRIDE", fetch_word(asic, stream, 1), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "DMA_COUNT", fetch_word(asic, stream, 2), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 16, ib_vmid, "DST_ADDR_LO", fetch_word(asic, stream, 3), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 20, ib_vmid, "DST_ADDR_HI", fetch_word(asic, stream, 4), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 24, ib_vmid, "BYTE_COUNT", BITS(fetch_word(asic, stream, 5), 0, 26), NULL, 10, 32);
			break;
		case 0xA1: // PKT3_MAP_PROCESS
			ui->add_field(ui, ib_addr + 4, ib_vmid, "PASID", BITS(fetch_word(asic, stream, 0), 0, 16), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "DEBUG_VMID", BITS(fetch_word(asic, stream, 0), 18, 22), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "DEBUG_FLAG", BITS(fetch_word(asic, stream, 0), 22, 23), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "TMZ", BITS(fetch_word(asic, stream, 0), 23, 24), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "DIQ_ENABLE", BITS(fetch_word(asic, stream, 0), 24, 25), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "PROCESS_QUANTUM", BITS(fetch_word(asic, stream, 0), 25, 32), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "VM_CONTEXT_PAGE_TABLE_BASE_ADDR_LO32", fetch_word(asic, stream, 1), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "VM_CONTEXT_PAGE_TABLE_BASE_ADDR_HI32", fetch_word(asic, stream, 2), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 16, ib_vmid, "SH_MEM_BASES", fetch_word(asic, stream, 3), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 20, ib_vmid, "SH_MEM_CONFIG", fetch_word(asic, stream, 4), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 24, ib_vmid, "SQ_SHADER_TBA_LO", fetch_word(asic, stream, 5), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 28, ib_vmid, "SQ_SHADER_TBA_HI", fetch_word(asic, stream, 6), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 32, ib_vmid, "SQ_SHADER_TMA_LO", fetch_word(asic, stream, 7), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 36, ib_vmid, "SQ_SHADER_TMA_HI", fetch_word(asic, stream, 8), NULL, 16, 32);
			// offset 40 is reserved...
			ui->add_field(ui, ib_addr + 44, ib_vmid, "GDS_ADDR_LO", fetch_word(asic, stream, 10), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 48, ib_vmid, "GDS_ADDR_HI", fetch_word(asic, stream, 11), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 52, ib_vmid, "NUM_GWS", BITS(fetch_word(asic, stream, 12), 0, 6), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 52, ib_vmid, "SDMA_ENABLE", BITS(fetch_word(asic, stream, 12), 7, 8), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 52, ib_vmid, "NUM_OAC", BITS(fetch_word(asic, stream, 12), 8, 12), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 52, ib_vmid, "GDS_SIZE", BITS(fetch_word(asic, stream, 12), 16, 22), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 52, ib_vmid, "NUM_QUEUES", BITS(fetch_word(asic, stream, 12), 22, 32), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 56, ib_vmid, "COMPLETION_SIGNAL_LO32", fetch_word(asic, stream, 13), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 60, ib_vmid, "COMPLETION_SIGNAL_HI32", fetch_word(asic, stream, 14), NULL, 16, 32);
			break;
		case 0xA2: // PKT3_MAP_QUEUES
			ui->add_field(ui, ib_addr + 4, ib_vmid, "QUEUE_SEL", BITS(fetch_word(asic, stream, 0), 4, 6), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "VMID", BITS(fetch_word(asic, stream, 0), 8, 12), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "QUEUE", BITS(fetch_word(asic, stream, 0), 13, 21), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "QUEUE_TYPE", BITS(fetch_word(asic, stream, 0), 21, 24), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "STATIC_QUEUE_GROUP", BITS(fetch_word(asic, stream, 0), 24, 26), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "ENGINE_SEL", BITS(fetch_word(asic, stream, 0), 26, 29), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "NUM_QUEUES", BITS(fetch_word(asic, stream, 0), 29, 32), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "CHECK_DISABLE", BITS(fetch_word(asic, stream, 1), 1, 2), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "DOORBELL_OFFSET", BITS(fetch_word(asic, stream, 1), 2, 28), NULL, 16, 32);
			{
				uint32_t n;
				for (n = 2; n + 4 <= stream->n_words; n += 4) {
					ui->add_field(ui, ib_addr + 12 + 16 * ((n - 2) / 4), ib_vmid, "MQD_ADDR_LO", fetch_word(asic, stream, n), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 16 + 16 * ((n - 2) / 4), ib_vmid, "MQD_ADDR_HI", fetch_word(asic, stream, n + 1), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 20 + 16 * ((n - 2) / 4), ib_vmid, "WPTR_ADDR_LO", fetch_word(asic, stream, n + 2), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 24 + 16 * ((n - 2) / 4), ib_vmid, "WPTR_ADDR_HI", fetch_word(asic, stream, n + 3), NULL, 16, 32);
					if (ui->add_data)
						ui->add_data(ui, asic,
									 ib_addr + 12 + 16 * ((n - 2) / 4), ib_vmid,
									 ((uint64_t)fetch_word(asic, stream, n)) | (((uint64_t)fetch_word(asic, stream, n + 1)) << 32), BITS(fetch_word(asic, stream, 0), 8, 12),
									 UMR_DATABLOCK_MQD_NV, BITS(fetch_word(asic, stream, 0), 26, 29));
				}
			}
			break;
		case 0xA3: // PKT3_UNMAP_QUEUES
			{
				uint32_t action, queue_sel, num_queues, engine_sel;

				queue_sel = BITS(fetch_word(asic, stream, 0), 4, 6);
				engine_sel = BITS(fetch_word(asic, stream, 0), 26, 29);
				num_queues = BITS(fetch_word(asic, stream, 0), 29, 32);
				action = BITS(fetch_word(asic, stream, 0), 0, 2);
				ui->add_field(ui, ib_addr + 4, ib_vmid, "ACTION", BITS(fetch_word(asic, stream, 0), 0, 2), NULL, 10, 32);
				ui->add_field(ui, ib_addr + 4, ib_vmid, "QUEUE_SEL", queue_sel, NULL, 10, 32);
				ui->add_field(ui, ib_addr + 4, ib_vmid, "ENGINE_SEL", engine_sel, NULL, 10, 32);
				ui->add_field(ui, ib_addr + 4, ib_vmid, "NUM_QUEUES", num_queues, NULL, 10, 32);
				if (queue_sel == 1)
					ui->add_field(ui, ib_addr + 8, ib_vmid, "PASID", BITS(fetch_word(asic, stream, 1), 0, 16), NULL, 10, 32);
				else
					ui->add_field(ui, ib_addr + 8, ib_vmid, "DOORBELL_OFFSET0", BITS(fetch_word(asic, stream, 1), 2, 28), NULL, 16, 32);
				if (engine_sel == 4 && action == 3)
					ui->add_field(ui, ib_addr + 12, ib_vmid, "RB_WPTR", BITS(fetch_word(asic, stream, 2), 0, 20), NULL, 16, 32);
				else
					ui->add_field(ui, ib_addr + 12, ib_vmid, "DOORBELL_OFFSET1", BITS(fetch_word(asic, stream, 2), 2, 28), NULL, 16, 32);
				ui->add_field(ui, ib_addr + 16, ib_vmid, "DOORBELL_OFFSET2", BITS(fetch_word(asic, stream, 3), 2, 28), NULL, 16, 32);
				ui->add_field(ui, ib_addr + 20, ib_vmid, "DOORBELL_OFFSET3", BITS(fetch_word(asic, stream, 4), 2, 28), NULL, 16, 32);
			}
			break;
		case 0xA5:	// PKT3_MES_RUN_LIST
			ui->add_field(ui, ib_addr + 4, ib_vmid, "IB_BASE_LO", BITS(fetch_word(asic, stream, 0), 2, 32) << 2, NULL, 16, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "IB_BASE_HI", fetch_word(asic, stream, 1), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "IB_SIZE", BITS(fetch_word(asic, stream, 2), 0, 20), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "CHAIN", BITS(fetch_word(asic, stream, 2), 20, 21), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "OFFLOAD_POLLING", BITS(fetch_word(asic, stream, 2), 21, 22), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "VALID", BITS(fetch_word(asic, stream, 2), 23, 24), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "PROCESS_CNT", BITS(fetch_word(asic, stream, 2), 24, 28), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 16, ib_vmid, "LEVEL_1_STATIC_QUEUE_CNT", BITS(fetch_word(asic, stream, 3), 0, 4), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 16, ib_vmid, "LEVEL_2_STATIC_QUEUE_CNT", BITS(fetch_word(asic, stream, 3), 4, 8), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 16, ib_vmid, "LEVEL_3_STATIC_QUEUE_CNT", BITS(fetch_word(asic, stream, 3), 8, 12), NULL, 10, 32);
			break;
		default:
			decode_pkt3_gfx8(asic, ui, stream, ib_addr, ib_vmid);
			break;
	}
}

static void decode_pkt3_gfx10(struct umr_asic *asic, struct umr_stream_decode_ui *ui, struct umr_pm4_stream *stream, uint64_t ib_addr, uint32_t ib_vmid)
{
	switch (stream->opcode) {
		case 0x1d: // ATOMIC_GDS
			// TODO: fill in
			break;
		case 0x1e: // ATOMIC_MEM
			// TODO: fill in
			break;
		case 0x25: // DRAW_INDEX_INDIRECT
			ui->add_field(ui, ib_addr + 4, ib_vmid, "DATA_OFFSET", fetch_word(asic, stream, 0), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "BASE_VTX_LOC", BITS(fetch_word(asic, stream, 1), 0, 16), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "START_INDX_LOC", BITS(fetch_word(asic, stream, 1), 16, 32), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "START_INST_LOC", BITS(fetch_word(asic, stream, 2), 0, 16), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "DISABLE_CPVGTDMA_SM", BITS(fetch_word(asic, stream, 2), 26, 27), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "START_INDX_ENABLE", BITS(fetch_word(asic, stream, 2), 28, 29), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 16, ib_vmid, "DRAW_INITIATOR", fetch_word(asic, stream, 3), NULL, 16, 32);
			break;
		case 0x37: // WRITE_DATA
			ui->add_field(ui, ib_addr + 4, ib_vmid, "ENGINE", BITS(fetch_word(asic, stream, 0), 30, 32), op_37_engines[BITS(fetch_word(asic, stream, 0), 30, 32)], 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "CACHE_POLICY", BITS(fetch_word(asic, stream, 0), 25, 27), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "WR_CONFIRM", BITS(fetch_word(asic, stream, 0), 20, 21), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "RESUME_VF", BITS(fetch_word(asic, stream, 0), 19, 20), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "WR_ONE_ADDR", BITS(fetch_word(asic, stream, 0), 16, 17), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "DST_SEL", BITS(fetch_word(asic, stream, 0), 8, 12), op_37_dst_sel[BITS(fetch_word(asic, stream, 0), 8, 12)],  10, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_ADDR_LO", BITS(fetch_word(asic, stream, 1), 2, 32) << 2, NULL, 16, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "DST_ADDR_HI", fetch_word(asic, stream, 2), NULL, 16, 32);
			if (BITS(fetch_word(asic, stream, 0), 8, 12) == 0) { // mem-mapped reg
				uint32_t n;
				uint64_t reg_addr = ((uint64_t)fetch_word(asic, stream, 2) << 32) | fetch_word(asic, stream, 1);
				for (n = 3; n < stream->n_words; n++) {
					ui->add_field(ui, ib_addr + 16 + (n - 3) * 4, ib_vmid, umr_reg_name(asic, reg_addr), fetch_word(asic, stream, n), NULL, 16, 32);
					reg_addr += 1;
				}
			}
			break;
		case 0x38: // DRAW_INDEX_INDIRECT_MULTI
			ui->add_field(ui, ib_addr + 4, ib_vmid, "DATA_OFFSET", fetch_word(asic, stream, 0), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "BASE_VTX_LOC", BITS(fetch_word(asic, stream, 1), 0, 16), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "START_INDX_LOC", BITS(fetch_word(asic, stream, 1), 16, 32), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "START_INST_LOC", BITS(fetch_word(asic, stream, 2), 0, 16), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 16, ib_vmid, "DRAW_INDEX_LOC", BITS(fetch_word(asic, stream, 3), 0, 16), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 16, ib_vmid, "DISABLE_CPVGTDMA_SM", BITS(fetch_word(asic, stream, 3), 26, 27), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 16, ib_vmid, "USE_VGPRS", BITS(fetch_word(asic, stream, 3), 27, 28), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 16, ib_vmid, "START_INDX_ENABLE", BITS(fetch_word(asic, stream, 3), 28, 29), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 16, ib_vmid, "THREAD_TRACE_MARKER_ENABLE", BITS(fetch_word(asic, stream, 3), 29, 30), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 16, ib_vmid, "COUNT_INDIRECT_ENABLE", BITS(fetch_word(asic, stream, 3), 30, 31), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 16, ib_vmid, "DRAW_INDEX_ENABLE", BITS(fetch_word(asic, stream, 3), 31, 32), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 20, ib_vmid, "COUNT", fetch_word(asic, stream, 4), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 24, ib_vmid, "COUNT_ADDR_LO", BITS(fetch_word(asic, stream, 5), 2, 32) << 2, NULL, 16, 32);
			ui->add_field(ui, ib_addr + 28, ib_vmid, "COUNT_ADDR_HI", fetch_word(asic, stream, 6), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 32, ib_vmid, "STRIDE", fetch_word(asic, stream, 7), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 36, ib_vmid, "DRAW_INITIATOR", fetch_word(asic, stream, 8), NULL, 16, 32);
			break;
		case 0x3C: // WAIT_REG_MEM
			ui->add_field(ui, ib_addr + 4, ib_vmid, "ENGINE", BITS(fetch_word(asic, stream, 0), 8, 9), BITS(fetch_word(asic, stream, 0), 8, 9) ? "PFP" : "ME", 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "MEMSPACE", BITS(fetch_word(asic, stream, 0), 4, 5), BITS(fetch_word(asic, stream, 0), 4, 5) ? "MEM" : "REG", 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "OPERATION", BITS(fetch_word(asic, stream, 0), 6, 8), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "FUNCTION", BITS(fetch_word(asic, stream, 0), 0, 4), op_3c_functions[BITS(fetch_word(asic, stream, 0), 0, 4)], 10, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "POLL_ADDRESS_LO", BITS(fetch_word(asic, stream, 1), 2, 32) << 2, NULL, 16, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "SWAP", BITS(fetch_word(asic, stream, 1), 0, 2), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "POLL_ADDRESS_HI", fetch_word(asic, stream, 2), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 16, ib_vmid, "REFERENCE", fetch_word(asic, stream, 3), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 20, ib_vmid, "MASK", fetch_word(asic, stream, 4), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 24, ib_vmid, "POLL INTERVAL", fetch_word(asic, stream, 5), NULL, 16, 32);
			break;
		case 0x40: // PKT3_COPY_DATA
			ui->add_field(ui, ib_addr + 4, ib_vmid, "SRC_SEL", BITS(fetch_word(asic, stream, 0), 0, 4), op_40_mem_sel[BITS(fetch_word(asic, stream, 0), 0, 4)], 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "DST_SEL", BITS(fetch_word(asic, stream, 0), 8, 12), op_40_mem_sel[BITS(fetch_word(asic, stream, 0), 8, 12)], 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "SRC_CACHE_POLICY", BITS(fetch_word(asic, stream, 0), 13, 15), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "COUNT_SEL", BITS(fetch_word(asic, stream, 0), 16, 17), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "WR_CONFIRM", BITS(fetch_word(asic, stream, 0), 20, 21), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "DST_CACHE_POLICY", BITS(fetch_word(asic, stream, 0), 25, 27), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "PQ_EXE_STATUS", BITS(fetch_word(asic, stream, 0), 29, 30), NULL, 10, 32);

			switch (BITS(fetch_word(asic, stream, 0), 0, 4)) {
				case 0: ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_REG_OFFSET", BITS(fetch_word(asic, stream, 1), 0, 18), umr_reg_name(asic, BITS(fetch_word(asic, stream, 1), 0, 18)), 16, 32); break;
				case 5: ui->add_field(ui, ib_addr + 8, ib_vmid, "IMM_DATA", fetch_word(asic, stream, 1), NULL, 16, 32); break;
				default: ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_ADDR_LO", BITS(fetch_word(asic, stream, 1), 2, 32) << 2, NULL, 16, 32); break;
			}

			if (BITS(fetch_word(asic, stream, 0), 0, 4) == 5 && BITS(fetch_word(asic, stream, 0), 16, 17) == 1)
				ui->add_field(ui, ib_addr + 12, ib_vmid, "IMM_DATA_HI", fetch_word(asic, stream, 2), NULL, 16, 32);
			else
				ui->add_field(ui, ib_addr + 12, ib_vmid, "SRC_DATA_HI", fetch_word(asic, stream, 2), NULL, 16, 32);

			switch (BITS(fetch_word(asic, stream, 0), 0, 4)) {
				case 0: ui->add_field(ui, ib_addr + 16, ib_vmid, "DST_REG_OFFSET", BITS(fetch_word(asic, stream, 3), 0, 18), umr_reg_name(asic, BITS(fetch_word(asic, stream, 3), 0, 18)), 16, 32); break;
				default: ui->add_field(ui, ib_addr + 16, ib_vmid, "DST_ADDR_LO", fetch_word(asic, stream, 3), NULL, 16, 32); break;
			}
			ui->add_field(ui, ib_addr + 20, ib_vmid, "DST_ADDR_HI", fetch_word(asic, stream, 4), NULL, 16, 32);
			break;
		case 0x42: // PFP_SYNC_ME
			ui->add_field(ui, ib_addr + 4, ib_vmid, "DUMMY_DATA", fetch_word(asic, stream, 0), NULL, 16, 32);
			break;
		case 0x43: // SURFACE_SYNC
			ui->add_field(ui, ib_addr + 4, ib_vmid, "ENGINE", BITS(fetch_word(asic, stream, 0), 31, 32), BITS(fetch_word(asic, stream, 0), 31, 32) ? "ME" : "PFP", 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "COHER_CNTL", BITS(fetch_word(asic, stream, 0), 0, 29), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "COHER_SIZE", fetch_word(asic, stream, 1), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "COHER_BASE", fetch_word(asic, stream, 2), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 16, ib_vmid, "POLL_INTERVAL", fetch_word(asic, stream, 3), NULL, 10, 32);
			break;
		case 0x46: // EVENT_WRITE
			ui->add_field(ui, ib_addr + 4, ib_vmid, "EVENT_TYPE", BITS(fetch_word(asic, stream, 0), 0, 6), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "EVENT_INDEX", BITS(fetch_word(asic, stream, 0), 8, 12), NULL, 10, 32);
			if (stream->n_words > 2) {
				ui->add_field(ui, ib_addr + 8, ib_vmid, "ADDRESS_LO", BITS(fetch_word(asic, stream, 1), 3, 32) << 3, NULL, 16, 32);
				ui->add_field(ui, ib_addr + 12, ib_vmid, "ADDRESS_HI", fetch_word(asic, stream, 2), NULL, 16, 32);
			}
			break;
		case 0x47: // EVENT_WRITE_EOP
			ui->add_field(ui, ib_addr + 4, ib_vmid, "EVENT_TYPE", BITS(fetch_word(asic, stream, 0), 0, 6), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "EVENT_INDEX", BITS(fetch_word(asic, stream, 0), 8, 12), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "INV_L2", BITS(fetch_word(asic, stream, 0), 20, 21), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "ADDRESS_LO", BITS(fetch_word(asic, stream, 1), 2, 32) << 2, NULL, 16, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "ADDRESS_HI", BITS(fetch_word(asic, stream, 2), 0, 16), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "DATA_SEL", BITS(fetch_word(asic, stream, 2), 29, 32), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "INT_SEL", BITS(fetch_word(asic, stream, 2), 24, 26), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 16, ib_vmid, "DATA_LO", fetch_word(asic, stream, 3), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 20, ib_vmid, "DATA_HI", fetch_word(asic, stream, 4), NULL, 16, 32);
			break;
		case 0x49: // RELEASE_MEM
			ui->add_field(ui, ib_addr + 4, ib_vmid, "EVENT_TYPE", BITS(fetch_word(asic, stream, 0), 0, 6), vgt_event_decode(BITS(fetch_word(asic, stream, 0), 0, 6)), 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "EVENT_INDEX", BITS(fetch_word(asic, stream, 0), 8, 12), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "TCL1_VOL_ACTION_ENA", BITS(fetch_word(asic, stream, 0), 12, 13), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "TC_VOL_ACTION_ENA", BITS(fetch_word(asic, stream, 0), 13, 14), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "TC_WB_ACTION_ENA", BITS(fetch_word(asic, stream, 0), 15, 16), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "TCL1_ACTION_ENA", BITS(fetch_word(asic, stream, 0), 16, 17), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "TC_ACTION_ENA", BITS(fetch_word(asic, stream, 0), 17, 18), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "TC_NC_ACTION_ENA", BITS(fetch_word(asic, stream, 0), 19, 20), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "TC_WC_ACTION_ENA", BITS(fetch_word(asic, stream, 0), 20, 21), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "TC_MD_ACTION_ENA", BITS(fetch_word(asic, stream, 0), 21, 22), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "CACHE_POLICY", BITS(fetch_word(asic, stream, 0), 25, 27), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "EXECUTE", BITS(fetch_word(asic, stream, 0), 28, 29), NULL, 10, 32);

			ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_SEL", BITS(fetch_word(asic, stream, 1), 16, 18), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "INT_SEL", BITS(fetch_word(asic, stream, 1), 24, 27), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "DATA_SEL", BITS(fetch_word(asic, stream, 1), 29, 32), NULL, 10, 32);

			ui->add_field(ui, ib_addr + 12, ib_vmid, "ADDR_LO", fetch_word(asic, stream, 2), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 16, ib_vmid, "ADDR_HI", fetch_word(asic, stream, 3), NULL, 16, 32);

			if (BITS(fetch_word(asic, stream, 1), 24, 27) == 5 ||
			    BITS(fetch_word(asic, stream, 1), 24, 27) == 6) {
				ui->add_field(ui, ib_addr + 20, ib_vmid, "CMP_DATA_LO", fetch_word(asic, stream, 4), NULL, 16, 32);
			} else if (BITS(fetch_word(asic, stream, 1), 29, 32) == 5) {
				ui->add_field(ui, ib_addr + 20, ib_vmid, "DW_OFFSET", BITS(fetch_word(asic, stream, 4), 0, 16), NULL, 16, 32);
				ui->add_field(ui, ib_addr + 20, ib_vmid, "NUM_WORDS", BITS(fetch_word(asic, stream, 4), 16, 32), NULL, 10, 32);
			} else {
				ui->add_field(ui, ib_addr + 20, ib_vmid, "DATA_LO", fetch_word(asic, stream, 4), NULL, 16, 32);
			}

			if (BITS(fetch_word(asic, stream, 1), 24, 27) == 5 ||
			    BITS(fetch_word(asic, stream, 1), 24, 27) == 6)
				ui->add_field(ui, ib_addr + 24, ib_vmid, "CMP_DATA_HI", fetch_word(asic, stream, 5), NULL, 16, 32);
			else
				ui->add_field(ui, ib_addr + 24, ib_vmid, "DATA_HI", fetch_word(asic, stream, 5), NULL, 16, 32);

			if (asic->family >= FAMILY_AI)
				ui->add_field(ui, ib_addr + 28, ib_vmid, "INT_CTXID", fetch_word(asic, stream, 6), NULL, 16, 32);
			break;
		case 0x4A: // PREAMBLE_CNTL
			ui->add_field(ui, ib_addr + 4, ib_vmid, "COMMAND", BITS(fetch_word(asic, stream, 0), 28, 32), NULL, 16, 32);
			break;
		case 0x4C: // DISPATCH_MESH_INDIRECT_MULTI
			ui->add_field(ui, ib_addr + 4, ib_vmid, "DATA_OFFSET", fetch_word(asic, stream, 0), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "XYZ_DIM_LOC", BITS(fetch_word(asic, stream, 1), 0, 16), umr_reg_name(asic, BITS(fetch_word(asic, stream, 1), 0, 16) + 0x2C00), 16, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "RING_ENTRY_LOC", BITS(fetch_word(asic, stream, 1), 16, 32), umr_reg_name(asic, BITS(fetch_word(asic, stream, 1), 16, 32) + 0x2C00), 16, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "USE_VGPRS", BITS(fetch_word(asic, stream, 2), 28, 29), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "THREAD_TRACE_MARKER_ENABLE", BITS(fetch_word(asic, stream, 2), 29, 30), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "COUNT_INDIRECT_ENABLE", BITS(fetch_word(asic, stream, 2), 30, 31), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "DRAW_INDEX_ENABLE", BITS(fetch_word(asic, stream, 2), 31, 32), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 16, ib_vmid, "COUNT", fetch_word(asic, stream, 3), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 20, ib_vmid, "COUNT_ADDR_LO", BITS(fetch_word(asic, stream, 4), 2, 32) << 2, NULL, 16, 32);
			ui->add_field(ui, ib_addr + 24, ib_vmid, "COUNT_ADDR_HI", fetch_word(asic, stream, 5), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 28, ib_vmid, "STRIDE", fetch_word(asic, stream, 6), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 32, ib_vmid, "DRAW_INITIATOR", fetch_word(asic, stream, 7), NULL, 10, 32);
			break;
		case 0x4D: // DISPATCH_TASKMESH_GFX
			ui->add_field(ui, ib_addr + 4, ib_vmid, "XYZ_DIM_LOC", BITS(fetch_word(asic, stream, 0), 0, 16), umr_reg_name(asic, BITS(fetch_word(asic, stream, 0), 0, 16) + 0x2C00), 16, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "RING_ENTRY_LOC", BITS(fetch_word(asic, stream, 0), 16, 32), umr_reg_name(asic, BITS(fetch_word(asic, stream, 0), 16, 32) + 0x2C00), 16, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "THREAD_TRACE_MARKER_ENABLE", BITS(fetch_word(asic, stream, 1), 31, 32), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "DRAW_INITIATOR", fetch_word(asic, stream, 2), NULL, 16, 32);
			break;
		case 0x58: // ACQUIRE_MEM
			ui->add_field(ui, ib_addr + 4, ib_vmid, "ENGINE", BITS(fetch_word(asic, stream, 0), 31, 32), BITS(fetch_word(asic, stream, 0), 31, 32) ? "ME" : "PFP", 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "COHER_CNTL", BITS(fetch_word(asic, stream, 0), 0, 30), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "CP_COHER_SIZE", fetch_word(asic, stream, 1), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "CP_COHER_SIZE_HI", BITS(fetch_word(asic, stream, 2), 0, 8), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 16, ib_vmid, "CP_COHER_BASE", fetch_word(asic, stream, 3), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 20, ib_vmid, "CP_COHER_BASE_HI", BITS(fetch_word(asic, stream, 4), 0, 8), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 24, ib_vmid, "POLL_INTERVAL", BITS(fetch_word(asic, stream, 5), 0, 16), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 28, ib_vmid, "GCR_CNTL", BITS(fetch_word(asic, stream, 6), 0, 19), NULL, 16, 32);
			break;
		case 0x86: // WAIT_ON_CE_COUNTER
			ui->add_field(ui, ib_addr + 4, ib_vmid, "COND_ACQUIRE_MEM", BITS(fetch_word(asic, stream, 0), 0, 1), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "FORCE_SYNC", BITS(fetch_word(asic, stream, 0), 1, 2), NULL, 10, 32);
			break;
		case 0x8B: // SWITCH_BUFFER
			ui->add_field(ui, ib_addr + 4, ib_vmid, "DUMMY", fetch_word(asic, stream, 0), NULL, 16, 32);
			break;
		case 0xA2: // PKT3_MAP_QUEUES
			ui->add_field(ui, ib_addr + 4, ib_vmid, "QUEUE_SEL", BITS(fetch_word(asic, stream, 0), 4, 6), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "GANG_SHED_MODE", BITS(fetch_word(asic, stream, 0), 7, 8), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "VMID", BITS(fetch_word(asic, stream, 0), 8, 12), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "GWS_ENABLED", BITS(fetch_word(asic, stream, 0), 12, 13), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "QUEUE", BITS(fetch_word(asic, stream, 0), 13, 21), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "QUEUE_TYPE", BITS(fetch_word(asic, stream, 0), 21, 24), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "STATIC_QUEUE_GROUP", BITS(fetch_word(asic, stream, 0), 24, 26), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "ENGINE_SEL", BITS(fetch_word(asic, stream, 0), 26, 29), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "NUM_QUEUES", BITS(fetch_word(asic, stream, 0), 29, 32), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "CHECK_DISABLE", BITS(fetch_word(asic, stream, 1), 1, 2), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "DOORBELL_OFFSET", BITS(fetch_word(asic, stream, 1), 2, 28), NULL, 16, 32);
			{
				uint32_t n;
				for (n = 2; n + 4 <= stream->n_words; n += 4) {
					ui->add_field(ui, ib_addr + 12 + 16 * ((n - 2) / 4), ib_vmid, "MQD_ADDR_LO", fetch_word(asic, stream, n), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 16 + 16 * ((n - 2) / 4), ib_vmid, "MQD_ADDR_HI", fetch_word(asic, stream, n + 1), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 20 + 16 * ((n - 2) / 4), ib_vmid, "WPTR_ADDR_LO", fetch_word(asic, stream, n + 2), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 24 + 16 * ((n - 2) / 4), ib_vmid, "WPTR_ADDR_HI", fetch_word(asic, stream, n + 3), NULL, 16, 32);
					if (ui->add_data)
						ui->add_data(ui, asic,
									 ib_addr + 12 + 16 * ((n - 2) / 4), ib_vmid,
									 ((uint64_t)fetch_word(asic, stream, n)) | (((uint64_t)fetch_word(asic, stream, n + 1)) << 32), BITS(fetch_word(asic, stream, 0), 8, 12),
									 UMR_DATABLOCK_MQD_NV, BITS(fetch_word(asic, stream, 0), 26, 29));
				}
			}
			break;
		case 0xA3: // PKT3_UNMAP_QUEUES
			{
				uint32_t action, queue_sel, num_queues, engine_sel;

				queue_sel = BITS(fetch_word(asic, stream, 0), 4, 6);
				engine_sel = BITS(fetch_word(asic, stream, 0), 26, 29);
				num_queues = BITS(fetch_word(asic, stream, 0), 29, 32);
				action = BITS(fetch_word(asic, stream, 0), 0, 2);
				ui->add_field(ui, ib_addr + 4, ib_vmid, "ACTION", BITS(fetch_word(asic, stream, 0), 0, 2), NULL, 10, 32);
				ui->add_field(ui, ib_addr + 4, ib_vmid, "QUEUE_SEL", queue_sel, NULL, 10, 32);
				ui->add_field(ui, ib_addr + 4, ib_vmid, "ENGINE_SEL", engine_sel, NULL, 10, 32);
				ui->add_field(ui, ib_addr + 4, ib_vmid, "NUM_QUEUES", num_queues, NULL, 10, 32);
				if (queue_sel == 1)
					ui->add_field(ui, ib_addr + 8, ib_vmid, "PASID", BITS(fetch_word(asic, stream, 1), 0, 16), NULL, 10, 32);
				else
					ui->add_field(ui, ib_addr + 8, ib_vmid, "DOORBELL_OFFSET0", BITS(fetch_word(asic, stream, 1), 2, 28), NULL, 16, 32);
				if (action == 3)
					ui->add_field(ui, ib_addr + 12, ib_vmid, "TF_ADDR_LO32", BITS(fetch_word(asic, stream, 2), 2, 32), NULL, 16, 32);
				else
					ui->add_field(ui, ib_addr + 12, ib_vmid, "DOORBELL_OFFSET1", BITS(fetch_word(asic, stream, 2), 2, 28), NULL, 16, 32);
				if (action == 3)
					ui->add_field(ui, ib_addr + 16, ib_vmid, "TF_ADDR_HI32", BITS(fetch_word(asic, stream, 3), 0, 32), NULL, 16, 32);
				else
					ui->add_field(ui, ib_addr + 16, ib_vmid, "DOORBELL_OFFSET2", BITS(fetch_word(asic, stream, 3), 2, 28), NULL, 16, 32);
				if (action == 3)
					ui->add_field(ui, ib_addr + 20, ib_vmid, "TF_DATA", BITS(fetch_word(asic, stream, 4), 0, 32), NULL, 16, 32);
				else
					ui->add_field(ui, ib_addr + 20, ib_vmid, "DOORBELL_OFFSET3", BITS(fetch_word(asic, stream, 4), 2, 28), NULL, 16, 32);
			}
			break;
		case 0xA9: 	// PKT3_DISPATCH_TASK_STATE_INIT
			ui->add_field(ui, ib_addr + 4, ib_vmid, "CONTROL_BUF_ADDR_LO", BITS(fetch_word(asic, stream, 0), 8, 32) << 8, NULL, 16, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "CONTROL_BUF_ADDR_HI", fetch_word(asic, stream, 1), NULL, 16, 32);
			break;
		case 0xAA:	// DISPATCH_TASKMESH_DIRECT_ACE
			ui->add_field(ui, ib_addr + 4, ib_vmid, "DIM_X", fetch_word(asic, stream, 0), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "DIM_Y", fetch_word(asic, stream, 1), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "DIM_Z", fetch_word(asic, stream, 2), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 16, ib_vmid, "DISPATCH_INITIATOR", fetch_word(asic, stream, 3), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 20, ib_vmid, "RING_ENTRY_LOC", BITS(fetch_word(asic, stream, 4), 0, 16), umr_reg_name(asic, BITS(fetch_word(asic, stream, 4), 0, 16) + 0x2C00), 16, 32);
			break;
		case 0xAD: // DISPATCH_TASKMESH_INDIRECT_MULTI_ACE
			ui->add_field(ui, ib_addr + 4, ib_vmid, "DATA_ADDR_LO", BITS(fetch_word(asic, stream, 0), 2, 32) << 2, NULL, 16, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "DATA_ADDR_HI", fetch_word(asic, stream, 1), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "RING_ENTRY_LOC", BITS(fetch_word(asic, stream, 2), 0, 16), umr_reg_name(asic, BITS(fetch_word(asic, stream, 2), 0, 16) + 0x2C00), 16, 32);
			ui->add_field(ui, ib_addr + 16, ib_vmid, "THREAD_TRACE_MARKER_ENABLE", BITS(fetch_word(asic, stream, 3), 0, 1), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 16, ib_vmid, "COUNT_INDIRECT_ENABLE", BITS(fetch_word(asic, stream, 3), 1, 2), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 16, ib_vmid, "DISPATCH_INDEX_ENABLE", BITS(fetch_word(asic, stream, 3), 2, 3), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 16, ib_vmid, "COMPUTE_XYZ_DIM_ENABLE", BITS(fetch_word(asic, stream, 3), 3, 4), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 16, ib_vmid, "DISPATCH_INDEX_LOC", BITS(fetch_word(asic, stream, 3), 16, 32), umr_reg_name(asic, BITS(fetch_word(asic, stream, 3), 16, 32) + 0x2C00), 16, 32);
			ui->add_field(ui, ib_addr + 20, ib_vmid, "COMPUTE_XYZ_DIM_LOC", BITS(fetch_word(asic, stream, 4), 0, 16), umr_reg_name(asic, BITS(fetch_word(asic, stream, 4), 0, 16) + 0x2C00), 16, 32);
			ui->add_field(ui, ib_addr + 24, ib_vmid, "COUNT", fetch_word(asic, stream, 5), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 28, ib_vmid, "COUNT_ADDR_LO", BITS(fetch_word(asic, stream, 6), 2, 32) << 2, NULL, 16, 32);
			ui->add_field(ui, ib_addr + 32, ib_vmid, "COUNT_ADDR_HI", fetch_word(asic, stream, 7), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 36, ib_vmid, "STRIDE", fetch_word(asic, stream, 8), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 40, ib_vmid, "DISPATCH_INITIATOR", fetch_word(asic, stream, 9), NULL, 16, 32);
			break;
		default:
			decode_pkt3_gfx9(asic, ui, stream, ib_addr, ib_vmid);
			break;
	}
}

static void decode_pkt3_gfx11(struct umr_asic *asic, struct umr_stream_decode_ui *ui, struct umr_pm4_stream *stream, uint64_t ib_addr, uint32_t ib_vmid)
{
	switch (stream->opcode) {
		case 0x11: // SET_BASE
			ui->add_field(ui, ib_addr + 4, ib_vmid, "BASE_INDEX", BITS(fetch_word(asic, stream, 0), 0, 4), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "ADDRESS_LO", BITS(fetch_word(asic, stream, 1), 3, 32) << 3, NULL, 16, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "ADDRESS_HI", fetch_word(asic, stream, 2), NULL, 16, 32);
			break;
		case 0x1d: // ATOMIC_GDS
			// TODO: fill in
			break;
		case 0x1e: // ATOMIC_MEM
			// TODO: fill in
			break;
		case 0x22: // COND_EXEC
			ui->add_field(ui, ib_addr + 4, ib_vmid, "GPU_ADDR_LO32", BITS(fetch_word(asic, stream, 0), 2, 32) << 2, NULL, 16, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "GPU_ADDR_HI32", fetch_word(asic, stream, 1), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "CACHE_POLICY", BITS(fetch_word(asic, stream, 2), 25, 27), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 16, ib_vmid, "EXEC_COUNT", BITS(fetch_word(asic, stream, 3), 0, 14), NULL, 16, 32);
			break;
		case 0x37: // WRITE_DATA
			ui->add_field(ui, ib_addr + 4, ib_vmid, "ENGINE", BITS(fetch_word(asic, stream, 0), 30, 32), op_37_engines[BITS(fetch_word(asic, stream, 0), 30, 32)], 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "CACHE_POLICY", BITS(fetch_word(asic, stream, 0), 25, 27), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "WR_CONFIRM", BITS(fetch_word(asic, stream, 0), 20, 21), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "WR_ONE_ADDR", BITS(fetch_word(asic, stream, 0), 16, 17), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "DST_SEL", BITS(fetch_word(asic, stream, 0), 8, 12), op_37_dst_sel[BITS(fetch_word(asic, stream, 0), 8, 12)],  10, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_ADDR_LO", BITS(fetch_word(asic, stream, 1), 2, 32) << 2, NULL, 16, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "DST_ADDR_HI", fetch_word(asic, stream, 2), NULL, 16, 32);
			if (BITS(fetch_word(asic, stream, 0), 8, 12) == 0) { // mem-mapped reg
				uint32_t n;
				uint64_t reg_addr = ((uint64_t)fetch_word(asic, stream, 2) << 32) | fetch_word(asic, stream, 1);
				for (n = 3; n < stream->n_words; n++) {
					ui->add_field(ui, ib_addr + 16 + (n - 3) * 4, ib_vmid, umr_reg_name(asic, reg_addr), fetch_word(asic, stream, n), NULL, 16, 32);
					reg_addr += 1;
				}
			}
			break;
		case 0x38: // DRAW_INDEX_INDIRECT_MULTI
			ui->add_field(ui, ib_addr + 4, ib_vmid, "DATA_OFFSET", fetch_word(asic, stream, 0), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "BASE_VTX_LOC", BITS(fetch_word(asic, stream, 1), 0, 16), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "START_INDX_LOC", BITS(fetch_word(asic, stream, 1), 16, 32), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "START_INST_LOC", BITS(fetch_word(asic, stream, 2), 0, 16), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 16, ib_vmid, "DRAW_INDEX_LOC", BITS(fetch_word(asic, stream, 3), 0, 16), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 16, ib_vmid, "TASK_SHADER_MODE", BITS(fetch_word(asic, stream, 3), 25, 26), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 16, ib_vmid, "USE_VGPRS", BITS(fetch_word(asic, stream, 3), 27, 28), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 16, ib_vmid, "START_INDX_ENABLE", BITS(fetch_word(asic, stream, 3), 28, 29), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 16, ib_vmid, "THREAD_TRACE_MARKER_ENABLE", BITS(fetch_word(asic, stream, 3), 29, 30), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 16, ib_vmid, "COUNT_INDIRECT_ENABLE", BITS(fetch_word(asic, stream, 3), 30, 31), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 16, ib_vmid, "DRAW_INDEX_ENABLE", BITS(fetch_word(asic, stream, 3), 31, 32), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 20, ib_vmid, "COUNT", fetch_word(asic, stream, 4), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 24, ib_vmid, "COUNT_ADDR_LO", BITS(fetch_word(asic, stream, 5), 2, 32) << 2, NULL, 16, 32);
			ui->add_field(ui, ib_addr + 28, ib_vmid, "COUNT_ADDR_HI", fetch_word(asic, stream, 6), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 32, ib_vmid, "STRIDE", fetch_word(asic, stream, 7), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 36, ib_vmid, "DRAW_INITIATOR", fetch_word(asic, stream, 8), NULL, 16, 32);
			break;
		case 0x3C: // WAIT_REG_MEM
			ui->add_field(ui, ib_addr + 4, ib_vmid, "ENGINE", BITS(fetch_word(asic, stream, 0), 8, 9), BITS(fetch_word(asic, stream, 0), 8, 9) ? "PFP" : "ME", 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "MEMSPACE", BITS(fetch_word(asic, stream, 0), 4, 5), BITS(fetch_word(asic, stream, 0), 4, 5) ? "MEM" : "REG", 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "OPERATION", BITS(fetch_word(asic, stream, 0), 6, 8), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "FUNCTION", BITS(fetch_word(asic, stream, 0), 0, 4), op_3c_functions[BITS(fetch_word(asic, stream, 0), 0, 4)], 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "MES_INTR_PIPE", BITS(fetch_word(asic, stream, 0), 22, 24), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "MES_ACTION", BITS(fetch_word(asic, stream, 0), 24, 25), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "CACHE_POLICY", BITS(fetch_word(asic, stream, 0), 25, 27), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "POLL_ADDRESS_LO", BITS(fetch_word(asic, stream, 1), 2, 32) << 2, NULL, 16, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "SWAP", BITS(fetch_word(asic, stream, 1), 0, 2), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "POLL_ADDRESS_HI", fetch_word(asic, stream, 2), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 16, ib_vmid, "REFERENCE", fetch_word(asic, stream, 3), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 20, ib_vmid, "MASK", fetch_word(asic, stream, 4), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 24, ib_vmid, "POLL INTERVAL", fetch_word(asic, stream, 5), NULL, 16, 32);
			break;
		case 0x40: // PKT3_COPY_DATA
			ui->add_field(ui, ib_addr + 4, ib_vmid, "SRC_SEL", BITS(fetch_word(asic, stream, 0), 0, 4), op_40_mem_sel[BITS(fetch_word(asic, stream, 0), 0, 4)], 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "DST_SEL", BITS(fetch_word(asic, stream, 0), 8, 12), op_40_mem_sel[BITS(fetch_word(asic, stream, 0), 8, 12)], 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "SRC_CACHE_POLICY", BITS(fetch_word(asic, stream, 0), 13, 15), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "COUNT_SEL", BITS(fetch_word(asic, stream, 0), 16, 17), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "WR_CONFIRM", BITS(fetch_word(asic, stream, 0), 20, 21), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "DST_CACHE_POLICY", BITS(fetch_word(asic, stream, 0), 25, 27), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "PQ_EXE_STATUS", BITS(fetch_word(asic, stream, 0), 29, 30), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "ENGINE_SEL", BITS(fetch_word(asic, stream, 0), 30, 32), NULL, 10, 32);

			switch (BITS(fetch_word(asic, stream, 0), 0, 4)) {
				case 0: ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_REG_OFFSET", BITS(fetch_word(asic, stream, 1), 0, 18), umr_reg_name(asic, BITS(fetch_word(asic, stream, 1), 0, 18)), 16, 32); break;
				case 5: ui->add_field(ui, ib_addr + 8, ib_vmid, "IMM_DATA", fetch_word(asic, stream, 1), NULL, 16, 32); break;
				default: ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_ADDR_LO", BITS(fetch_word(asic, stream, 1), 2, 32) << 2, NULL, 16, 32); break;
			}

			if (BITS(fetch_word(asic, stream, 0), 0, 4) == 5 && BITS(fetch_word(asic, stream, 0), 16, 17) == 1)
				ui->add_field(ui, ib_addr + 12, ib_vmid, "IMM_DATA_HI", fetch_word(asic, stream, 2), NULL, 16, 32);
			else
				ui->add_field(ui, ib_addr + 12, ib_vmid, "SRC_DATA_HI", fetch_word(asic, stream, 2), NULL, 16, 32);

			switch (BITS(fetch_word(asic, stream, 0), 0, 4)) {
				case 0: ui->add_field(ui, ib_addr + 16, ib_vmid, "DST_REG_OFFSET", BITS(fetch_word(asic, stream, 3), 0, 18), umr_reg_name(asic, BITS(fetch_word(asic, stream, 3), 0, 18)), 16, 32); break;
				default: ui->add_field(ui, ib_addr + 16, ib_vmid, "DST_ADDR_LO", fetch_word(asic, stream, 3), NULL, 16, 32); break;
			}
			ui->add_field(ui, ib_addr + 20, ib_vmid, "DST_ADDR_HI", fetch_word(asic, stream, 4), NULL, 16, 32);
			break;
		case 0x49: // RELEASE_MEM
			ui->add_field(ui, ib_addr + 4, ib_vmid, "EVENT_TYPE", BITS(fetch_word(asic, stream, 0), 0, 6), vgt_event_decode(BITS(fetch_word(asic, stream, 0), 0, 6)), 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "EVENT_INDEX", BITS(fetch_word(asic, stream, 0), 8, 12), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "GCR_CNTL", BITS(fetch_word(asic, stream, 0), 12, 24), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "CACHE_POLICY", BITS(fetch_word(asic, stream, 0), 25, 27), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "EXECUTE", BITS(fetch_word(asic, stream, 0), 28, 29), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "PWS_ENABLE", BITS(fetch_word(asic, stream, 0), 31, 32), NULL, 10, 32);

			ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_SEL", BITS(fetch_word(asic, stream, 1), 16, 18), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "MES_INTR_PIPE", BITS(fetch_word(asic, stream, 1), 20, 22), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "MES_ACTION_ID", BITS(fetch_word(asic, stream, 1), 22, 24), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "INT_SEL", BITS(fetch_word(asic, stream, 1), 24, 27), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "DATA_SEL", BITS(fetch_word(asic, stream, 1), 29, 32), NULL, 10, 32);

			ui->add_field(ui, ib_addr + 12, ib_vmid, "ADDR_LO", fetch_word(asic, stream, 2), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 16, ib_vmid, "ADDR_HI", fetch_word(asic, stream, 3), NULL, 16, 32);

			if (BITS(fetch_word(asic, stream, 1), 24, 27) == 5 ||
			    BITS(fetch_word(asic, stream, 1), 24, 27) == 6) {
				ui->add_field(ui, ib_addr + 20, ib_vmid, "CMP_DATA_LO", fetch_word(asic, stream, 4), NULL, 16, 32);
			} else if (BITS(fetch_word(asic, stream, 1), 29, 32) == 5) {
				ui->add_field(ui, ib_addr + 20, ib_vmid, "DW_OFFSET", BITS(fetch_word(asic, stream, 4), 0, 16), NULL, 16, 32);
				ui->add_field(ui, ib_addr + 20, ib_vmid, "NUM_WORDS", BITS(fetch_word(asic, stream, 4), 16, 32), NULL, 10, 32);
			} else {
				ui->add_field(ui, ib_addr + 20, ib_vmid, "DATA_LO", fetch_word(asic, stream, 4), NULL, 16, 32);
			}

			if (BITS(fetch_word(asic, stream, 1), 24, 27) == 5 ||
			    BITS(fetch_word(asic, stream, 1), 24, 27) == 6)
				ui->add_field(ui, ib_addr + 24, ib_vmid, "CMP_DATA_HI", fetch_word(asic, stream, 5), NULL, 16, 32);
			else
				ui->add_field(ui, ib_addr + 24, ib_vmid, "DATA_HI", fetch_word(asic, stream, 5), NULL, 16, 32);

			ui->add_field(ui, ib_addr + 28, ib_vmid, "INT_CTXID", fetch_word(asic, stream, 6), NULL, 16, 32);
			break;
		case 0x4C: // DISPATCH_MESH_INDIRECT_MULTI
			ui->add_field(ui, ib_addr + 4, ib_vmid, "DATA_OFFSET", fetch_word(asic, stream, 0), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "XYZ_DIM_LOC", BITS(fetch_word(asic, stream, 1), 0, 16), umr_reg_name(asic, BITS(fetch_word(asic, stream, 1), 0, 16) + 0x2C00), 16, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "RING_ENTRY_LOC", BITS(fetch_word(asic, stream, 1), 16, 32), umr_reg_name(asic, BITS(fetch_word(asic, stream, 1), 16, 32) + 0x2C00), 16, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "XYZ_DIM_ENABLE", BITS(fetch_word(asic, stream, 2), 28, 29), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "THREAD_TRACE_MARKER_ENABLE", BITS(fetch_word(asic, stream, 2), 29, 30), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "COUNT_INDIRECT_ENABLE", BITS(fetch_word(asic, stream, 2), 30, 31), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "DRAW_INDEX_ENABLE", BITS(fetch_word(asic, stream, 2), 31, 32), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 16, ib_vmid, "COUNT", fetch_word(asic, stream, 3), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 20, ib_vmid, "COUNT_ADDR_LO", BITS(fetch_word(asic, stream, 4), 2, 32) << 2, NULL, 16, 32);
			ui->add_field(ui, ib_addr + 24, ib_vmid, "COUNT_ADDR_HI", fetch_word(asic, stream, 5), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 28, ib_vmid, "STRIDE", fetch_word(asic, stream, 6), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 32, ib_vmid, "DRAW_INITIATOR", fetch_word(asic, stream, 7), NULL, 10, 32);
			break;
		case 0x4D: // DISPATCH_TASKMESH_GFX
			ui->add_field(ui, ib_addr + 4, ib_vmid, "XYZ_DIM_LOC", BITS(fetch_word(asic, stream, 0), 0, 16), umr_reg_name(asic, BITS(fetch_word(asic, stream, 0), 0, 16) + 0x2C00), 16, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "RING_ENTRY_LOC", BITS(fetch_word(asic, stream, 0), 16, 32), umr_reg_name(asic, BITS(fetch_word(asic, stream, 0), 16, 32) + 0x2C00), 16, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "THREAD_TRACE_MARKER_ENABLE", BITS(fetch_word(asic, stream, 1), 31, 32), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "XYZ_DIM_ENABLE", BITS(fetch_word(asic, stream, 1), 30, 31), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "DRAW_INITIATOR", fetch_word(asic, stream, 2), NULL, 16, 32);
			break;
		case 0x58: // ACQUIRE_MEM
			{
				uint32_t pws_ena;

				pws_ena = BITS(fetch_word(asic, stream, 5), 31, 32);
				if (pws_ena) {
					ui->add_field(ui, ib_addr + 4, ib_vmid, "PWS_STAGE_SEL", BITS(fetch_word(asic, stream, 0), 11, 14), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "PWS_COUNTER_SEL", BITS(fetch_word(asic, stream, 0), 14, 16), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "PWS_ENA2", BITS(fetch_word(asic, stream, 0), 17, 18), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "PWS_COUNT", BITS(fetch_word(asic, stream, 0), 18, 24), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "GCR_SIZE", fetch_word(asic, stream, 1), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "GCR_SIZE_HI", BITS(fetch_word(asic, stream, 2), 0, 25), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 16, ib_vmid, "GCR_BASE_LO", fetch_word(asic, stream, 3), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "GCR_BASE_HI", BITS(fetch_word(asic, stream, 4), 0, 25), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "PWS_ENA", BITS(fetch_word(asic, stream, 5), 31, 32), NULL, 16, 32);
				} else {
					ui->add_field(ui, ib_addr + 4, ib_vmid, "ENGINE", BITS(fetch_word(asic, stream, 0), 31, 32), BITS(fetch_word(asic, stream, 0), 31, 32) ? "ME" : "PFP", 10, 32);
					ui->add_field(ui, ib_addr + 4, ib_vmid, "COHER_CNTL", BITS(fetch_word(asic, stream, 0), 0, 30), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 8, ib_vmid, "CP_COHER_SIZE", fetch_word(asic, stream, 1), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "CP_COHER_SIZE_HI", BITS(fetch_word(asic, stream, 2), 0, 24), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 16, ib_vmid, "CP_COHER_BASE_LO", fetch_word(asic, stream, 3), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 20, ib_vmid, "CP_COHER_BASE_HI", BITS(fetch_word(asic, stream, 4), 0, 24), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 24, ib_vmid, "POLL_INTERVAL", BITS(fetch_word(asic, stream, 5), 0, 16), NULL, 10, 32);
				}
				ui->add_field(ui, ib_addr + 28, ib_vmid, "GCR_CNTL", BITS(fetch_word(asic, stream, 6), 0, 19), NULL, 16, 32);
			}
			break;
		case 0xA2: // PKT3_MAP_QUEUES
			{ uint32_t ext_sel = BITS(fetch_word(asic, stream, 0), 2, 4),
					   engine_sel = BITS(fetch_word(asic, stream, 0), 26, 29);
				ui->add_field(ui, ib_addr + 4, ib_vmid, "EXTENDED_ENGINE_SEL", BITS(fetch_word(asic, stream, 0), 2, 4), NULL, 10, 32);
				ui->add_field(ui, ib_addr + 4, ib_vmid, "QUEUE_SEL", BITS(fetch_word(asic, stream, 0), 4, 6), NULL, 10, 32);
				ui->add_field(ui, ib_addr + 4, ib_vmid, "VMID", BITS(fetch_word(asic, stream, 0), 8, 12), NULL, 10, 32);
				ui->add_field(ui, ib_addr + 4, ib_vmid, "QUEUE", BITS(fetch_word(asic, stream, 0), 13, 21), NULL, 10, 32);
				ui->add_field(ui, ib_addr + 4, ib_vmid, "QUEUE_TYPE", BITS(fetch_word(asic, stream, 0), 21, 24), NULL, 10, 32);
				ui->add_field(ui, ib_addr + 4, ib_vmid, "STATIC_QUEUE_GROUP", BITS(fetch_word(asic, stream, 0), 24, 26), NULL, 10, 32);
				ui->add_field(ui, ib_addr + 4, ib_vmid, "ENGINE_SEL", BITS(fetch_word(asic, stream, 0), 26, 29), NULL, 10, 32);
				ui->add_field(ui, ib_addr + 4, ib_vmid, "NUM_QUEUES", BITS(fetch_word(asic, stream, 0), 29, 32), NULL, 10, 32);
				ui->add_field(ui, ib_addr + 8, ib_vmid, "CHECK_DISABLE", BITS(fetch_word(asic, stream, 1), 1, 2), NULL, 16, 32);
				ui->add_field(ui, ib_addr + 8, ib_vmid, "DOORBELL_OFFSET", BITS(fetch_word(asic, stream, 1), 2, 28), NULL, 16, 32);
				{
					uint32_t n;
					for (n = 2; n + 4 <= stream->n_words; n += 4) {
						ui->add_field(ui, ib_addr + 12 + 16 * ((n - 2) / 4), ib_vmid, "MQD_ADDR_LO", fetch_word(asic, stream, n), NULL, 16, 32);
						ui->add_field(ui, ib_addr + 16 + 16 * ((n - 2) / 4), ib_vmid, "MQD_ADDR_HI", fetch_word(asic, stream, n + 1), NULL, 16, 32);
						ui->add_field(ui, ib_addr + 20 + 16 * ((n - 2) / 4), ib_vmid, "WPTR_ADDR_LO", fetch_word(asic, stream, n + 2), NULL, 16, 32);
						ui->add_field(ui, ib_addr + 24 + 16 * ((n - 2) / 4), ib_vmid, "WPTR_ADDR_HI", fetch_word(asic, stream, n + 3), NULL, 16, 32);
						if (ui->add_data)
							ui->add_data(ui, asic,
										 ib_addr + 12 + 16 * ((n - 2) / 4), ib_vmid,
										 ((uint64_t)fetch_word(asic, stream, n)) | (((uint64_t)fetch_word(asic, stream, n + 1)) << 32), BITS(fetch_word(asic, stream, 0), 8, 12),
										 UMR_DATABLOCK_MQD_NV, ext_sel == 0 ? engine_sel : 2);
										 // send SDMA0 if extended_engine_sel is enabled for now
					}
				}
			}
			break;
		case 0xA3: // PKT3_UNMAP_QUEUES
			{
				uint32_t action, queue_sel, num_queues, engine_sel;

				queue_sel = BITS(fetch_word(asic, stream, 0), 4, 6);
				engine_sel = BITS(fetch_word(asic, stream, 0), 26, 29);
				num_queues = BITS(fetch_word(asic, stream, 0), 29, 32);
				action = BITS(fetch_word(asic, stream, 0), 0, 2);
				ui->add_field(ui, ib_addr + 4, ib_vmid, "ACTION", BITS(fetch_word(asic, stream, 0), 0, 2), NULL, 10, 32);
				ui->add_field(ui, ib_addr + 4, ib_vmid, "EXTENDED_ENGINE_SEL", BITS(fetch_word(asic, stream, 0), 2, 4), NULL, 10, 32);
				ui->add_field(ui, ib_addr + 4, ib_vmid, "QUEUE_SEL", queue_sel, NULL, 10, 32);
				ui->add_field(ui, ib_addr + 4, ib_vmid, "ENGINE_SEL", engine_sel, NULL, 10, 32);
				ui->add_field(ui, ib_addr + 4, ib_vmid, "NUM_QUEUES", num_queues, NULL, 10, 32);
				if (queue_sel == 1)
					ui->add_field(ui, ib_addr + 8, ib_vmid, "PASID", BITS(fetch_word(asic, stream, 1), 0, 16), NULL, 10, 32);
				else
					ui->add_field(ui, ib_addr + 8, ib_vmid, "DOORBELL_OFFSET0", BITS(fetch_word(asic, stream, 1), 2, 28), NULL, 16, 32);
				if (action == 3)
					ui->add_field(ui, ib_addr + 12, ib_vmid, "TF_ADDR_LO32", BITS(fetch_word(asic, stream, 2), 2, 32), NULL, 16, 32);
				else
					ui->add_field(ui, ib_addr + 12, ib_vmid, "DOORBELL_OFFSET1", BITS(fetch_word(asic, stream, 2), 2, 28), NULL, 16, 32);
				if (action == 3)
					ui->add_field(ui, ib_addr + 16, ib_vmid, "TF_ADDR_HI32", BITS(fetch_word(asic, stream, 3), 0, 32), NULL, 16, 32);
				else
					ui->add_field(ui, ib_addr + 16, ib_vmid, "DOORBELL_OFFSET2", BITS(fetch_word(asic, stream, 3), 2, 28), NULL, 16, 32);
				if (action == 3)
					ui->add_field(ui, ib_addr + 20, ib_vmid, "TF_DATA", BITS(fetch_word(asic, stream, 4), 0, 32), NULL, 16, 32);
				else
					ui->add_field(ui, ib_addr + 20, ib_vmid, "DOORBELL_OFFSET3", BITS(fetch_word(asic, stream, 4), 2, 28), NULL, 16, 32);
			}
			break;
		case 0xB9: // SET_CONTEXT_REG_PAIRS_PACKED
		case 0xBB:
		case 0xBC:
		case 0xBD: // SET_SH_REG_PAIRS_PACKED(_N)
			{
				uint32_t offset, n, m;

				switch (stream->opcode) {
					case 0xB9: offset = 0xA000; break;
					case 0xBB:
					case 0xBC:
					case 0xBD: offset = 0x2C00; break;
				}

				ui->add_field(ui, ib_addr + 4, ib_vmid, "REG_WRITES_COUNT", BITS(fetch_word(asic, stream, 0), 0, 16), NULL, 10, 32);

				for (m = 0, n = 1; n < stream->n_words; n += 3, ++m) {
					ui->add_field(ui, ib_addr + 8 + 12 * m, ib_vmid, "REG_OFFSET0", BITS(fetch_word(asic, stream, n+0), 0, 16), umr_reg_name(asic, offset + BITS(fetch_word(asic, stream, n+0), 0, 16)), 16, 32);
					ui->add_field(ui, ib_addr + 8 + 12 * m, ib_vmid, "REG_OFFSET1", BITS(fetch_word(asic, stream, n+0), 16, 32), umr_reg_name(asic, offset + BITS(fetch_word(asic, stream, n+0), 16, 32)), 16, 32);
					ui->add_field(ui, ib_addr + 12 + 12 * m, ib_vmid, "REG_DATA0", fetch_word(asic, stream, n+1), NULL, 16, 32);
					ui->add_field(ui, ib_addr + 16 + 12 * m, ib_vmid, "REG_DATA1", fetch_word(asic, stream, n+2), NULL, 16, 32);
				}
			}
			break;
		default:
			decode_pkt3_gfx10(asic, ui, stream, ib_addr, ib_vmid);
			break;
	}
}

static void decode_pkt3_gfx12(struct umr_asic *asic, struct umr_stream_decode_ui *ui, struct umr_pm4_stream *stream, uint64_t ib_addr, uint32_t ib_vmid)
{
	switch (stream->opcode) {
		case 0x1d: // ATOMIC_GDS
			// TODO: fill in
			break;
		case 0x1e: // ATOMIC_MEM
			// TODO: fill in
			break;
		case 0x25: // DRAW_INDEX_INDIRECT
			// note: the field in bit 26 was seemingly reverted for GFX12 so this is effectively the gfx9 decoding now
			decode_pkt3_gfx9(asic, ui, stream, ib_addr, ib_vmid);
			break;
		case 0x33: // INDIRECT_BUFFER_CONST
		case 0x3F: // INDIRECT_BUFFER_CIK
			if (stream->opcode == 0x3F && stream->n_words == 13) {
				// COND packet
				ui->add_field(ui, ib_addr + 4, ib_vmid, "MODE", BITS(fetch_word(asic, stream, 0), 0, 2), NULL, 10, 32);
				ui->add_field(ui, ib_addr + 4, ib_vmid, "FUNCTION", BITS(fetch_word(asic, stream, 0), 8, 11), NULL, 10, 32);
				ui->add_field(ui, ib_addr + 8, ib_vmid, "COMPARE_ADDR_LO", BITS(fetch_word(asic, stream, 1), 3, 32), NULL, 16, 32);
				ui->add_field(ui, ib_addr + 12, ib_vmid, "COMPARE_ADDR_HI", fetch_word(asic, stream, 2), NULL, 16, 32);
				ui->add_field(ui, ib_addr + 16, ib_vmid, "MASK_LO", fetch_word(asic, stream, 3), NULL, 16, 32);
				ui->add_field(ui, ib_addr + 20, ib_vmid, "MASK_HI", fetch_word(asic, stream, 4), NULL, 16, 32);
				ui->add_field(ui, ib_addr + 24, ib_vmid, "REFERENCE_LO", fetch_word(asic, stream, 5), NULL, 16, 32);
				ui->add_field(ui, ib_addr + 28, ib_vmid, "REFERENCE_HI", fetch_word(asic, stream, 6), NULL, 16, 32);
				ui->add_field(ui, ib_addr + 32, ib_vmid, "IB_BASE1_LO", BITS(fetch_word(asic, stream, 7), 2, 32), NULL, 16, 32);
				ui->add_field(ui, ib_addr + 36, ib_vmid, "IB_BASE1_HI", fetch_word(asic, stream, 8), NULL, 16, 32);
				ui->add_field(ui, ib_addr + 40, ib_vmid, "IB_SIZE1", BITS(fetch_word(asic, stream, 9), 0, 20), NULL, 16, 32);
				ui->add_field(ui, ib_addr + 40, ib_vmid, "TEMPORAL1", BITS(fetch_word(asic, stream, 9), 28, 30), NULL, 10, 32);
				ui->add_field(ui, ib_addr + 44, ib_vmid, "IB_BASE2_LO", BITS(fetch_word(asic, stream, 10), 2, 32), NULL, 16, 32);
				ui->add_field(ui, ib_addr + 48, ib_vmid, "IB_BASE2_HI", fetch_word(asic, stream, 11), NULL, 16, 32);
				ui->add_field(ui, ib_addr + 52, ib_vmid, "IB_SIZE2", BITS(fetch_word(asic, stream, 12), 0, 20), NULL, 16, 32);
				ui->add_field(ui, ib_addr + 52, ib_vmid, "TEMPORAL2", BITS(fetch_word(asic, stream, 12), 28, 30), NULL, 10, 32);
			} else {
				// not COND packet
				ui->add_field(ui, ib_addr + 4, ib_vmid, "IB_BASE_LO", BITS(fetch_word(asic, stream, 0), 2, 32) << 2, NULL, 16, 32);
				ui->add_field(ui, ib_addr + 4, ib_vmid, "SWAP", BITS(fetch_word(asic, stream, 0), 0, 2), NULL, 10, 32);
				ui->add_field(ui, ib_addr + 8, ib_vmid, "IB_BASE_HI", BITS(fetch_word(asic, stream, 1), 0, 16), NULL, 16, 32);
				ui->add_field(ui, ib_addr + 12, ib_vmid, "IB_SIZE", BITS(fetch_word(asic, stream, 2), 0, 20), NULL, 10, 32);
				ui->add_field(ui, ib_addr + 12, ib_vmid, "IB_VMID", BITS(fetch_word(asic, stream, 2), 24, 28), NULL, 10, 32);
				if (asic->family >= FAMILY_AI) {
					ui->add_field(ui, ib_addr + 12, ib_vmid, "CHAIN", BITS(fetch_word(asic, stream, 2), 20, 21), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "PRE_ENA", BITS(fetch_word(asic, stream, 2), 21, 22), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "TEMPORAL", BITS(fetch_word(asic, stream, 2), 28, 30), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "PRE_RESUME", BITS(fetch_word(asic, stream, 2), 30, 31), NULL, 10, 32);
					ui->add_field(ui, ib_addr + 12, ib_vmid, "PRIV", BITS(fetch_word(asic, stream, 2), 31, 32), NULL, 10, 32);
				}
			}
			break;
		case 0x37: // WRITE_DATA
			ui->add_field(ui, ib_addr + 4, ib_vmid, "ENGINE", BITS(fetch_word(asic, stream, 0), 30, 32), op_37_engines[BITS(fetch_word(asic, stream, 0), 30, 32)], 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "TEMPORAL", BITS(fetch_word(asic, stream, 0), 25, 27), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "AID_ID", BITS(fetch_word(asic, stream, 0), 23, 25), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "WR_CONFIRM", BITS(fetch_word(asic, stream, 0), 20, 21), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "WR_ONE_ADDR", BITS(fetch_word(asic, stream, 0), 16, 17), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "DST_SEL", BITS(fetch_word(asic, stream, 0), 8, 12), op_37_dst_sel[BITS(fetch_word(asic, stream, 0), 8, 12)],  10, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_ADDR_LO", BITS(fetch_word(asic, stream, 1), 2, 32) << 2, NULL, 16, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "DST_ADDR_HI", fetch_word(asic, stream, 2), NULL, 16, 32);
			if (BITS(fetch_word(asic, stream, 0), 8, 12) == 0) { // mem-mapped reg
				uint32_t n;
				uint64_t reg_addr = ((uint64_t)fetch_word(asic, stream, 2) << 32) | fetch_word(asic, stream, 1);
				for (n = 3; n < stream->n_words; n++) {
					ui->add_field(ui, ib_addr + 16 + (n - 3) * 4, ib_vmid, umr_reg_name(asic, reg_addr), fetch_word(asic, stream, n), NULL, 16, 32);
					reg_addr += 1;
				}
			}
			break;
		case 0x3C: // WAIT_REG_MEM
			ui->add_field(ui, ib_addr + 4, ib_vmid, "ENGINE", BITS(fetch_word(asic, stream, 0), 8, 9), BITS(fetch_word(asic, stream, 0), 8, 9) ? "PFP" : "ME", 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "MEMSPACE", BITS(fetch_word(asic, stream, 0), 4, 5), BITS(fetch_word(asic, stream, 0), 4, 5) ? "MEM" : "REG", 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "OPERATION", BITS(fetch_word(asic, stream, 0), 6, 8), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "FUNCTION", BITS(fetch_word(asic, stream, 0), 0, 4), op_3c_functions[BITS(fetch_word(asic, stream, 0), 0, 4)], 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "MES_INTR_PIPE", BITS(fetch_word(asic, stream, 0), 22, 24), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "MES_ACTION", BITS(fetch_word(asic, stream, 0), 24, 25), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "TEMPORAL", BITS(fetch_word(asic, stream, 0), 25, 27), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "POLL_ADDRESS_LO", BITS(fetch_word(asic, stream, 1), 2, 32) << 2, NULL, 16, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "SWAP", BITS(fetch_word(asic, stream, 1), 0, 2), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "POLL_ADDRESS_HI", fetch_word(asic, stream, 2), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 16, ib_vmid, "REFERENCE", fetch_word(asic, stream, 3), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 20, ib_vmid, "MASK", fetch_word(asic, stream, 4), NULL, 16, 32);
// TODO: should this be 15:0 for gen12
			ui->add_field(ui, ib_addr + 24, ib_vmid, "POLL INTERVAL", fetch_word(asic, stream, 5), NULL, 16, 32);
			break;
		case 0x40: // PKT3_COPY_DATA
			ui->add_field(ui, ib_addr + 4, ib_vmid, "SRC_SEL", BITS(fetch_word(asic, stream, 0), 0, 4), op_40_mem_sel[BITS(fetch_word(asic, stream, 0), 0, 4)], 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "DST_SEL", BITS(fetch_word(asic, stream, 0), 8, 12), op_40_mem_sel[BITS(fetch_word(asic, stream, 0), 8, 12)], 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "SRC_TEMPORAL", BITS(fetch_word(asic, stream, 0), 13, 15), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "COUNT_SEL", BITS(fetch_word(asic, stream, 0), 16, 17), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "WR_CONFIRM", BITS(fetch_word(asic, stream, 0), 20, 21), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "DST_TEMPORAL", BITS(fetch_word(asic, stream, 0), 25, 27), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "PQ_EXE_STATUS", BITS(fetch_word(asic, stream, 0), 29, 30), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "ENGINE_SEL", BITS(fetch_word(asic, stream, 0), 30, 32), NULL, 10, 32);

			switch (BITS(fetch_word(asic, stream, 0), 0, 4)) {
				case 0: ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_REG_OFFSET", BITS(fetch_word(asic, stream, 1), 0, 18), umr_reg_name(asic, BITS(fetch_word(asic, stream, 1), 0, 18)), 16, 32); break;
				case 5: ui->add_field(ui, ib_addr + 8, ib_vmid, "IMM_DATA", fetch_word(asic, stream, 1), NULL, 16, 32); break;
				default: ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_ADDR_LO", BITS(fetch_word(asic, stream, 1), 2, 32) << 2, NULL, 16, 32); break;
			}

			if (BITS(fetch_word(asic, stream, 0), 0, 4) == 5 && BITS(fetch_word(asic, stream, 0), 16, 17) == 1)
				ui->add_field(ui, ib_addr + 12, ib_vmid, "IMM_DATA_HI", fetch_word(asic, stream, 2), NULL, 16, 32);
			else
				ui->add_field(ui, ib_addr + 12, ib_vmid, "SRC_DATA_HI", fetch_word(asic, stream, 2), NULL, 16, 32);

			switch (BITS(fetch_word(asic, stream, 0), 0, 4)) {
				case 0: ui->add_field(ui, ib_addr + 16, ib_vmid, "DST_REG_OFFSET", BITS(fetch_word(asic, stream, 3), 0, 18), umr_reg_name(asic, BITS(fetch_word(asic, stream, 3), 0, 18)), 16, 32); break;
				default: ui->add_field(ui, ib_addr + 16, ib_vmid, "DST_ADDR_LO", fetch_word(asic, stream, 3), NULL, 16, 32); break;
			}
			ui->add_field(ui, ib_addr + 20, ib_vmid, "DST_ADDR_HI", fetch_word(asic, stream, 4), NULL, 16, 32);
			break;
		case 0x46: // EVENT_WRITE
			ui->add_field(ui, ib_addr + 4, ib_vmid, "EVENT_TYPE", BITS(fetch_word(asic, stream, 0), 0, 6), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "EVENT_INDEX", BITS(fetch_word(asic, stream, 0), 8, 12), NULL, 10, 32);
			if (stream->n_words > 2) {
				ui->add_field(ui, ib_addr + 8, ib_vmid, "ADDRESS_LO", BITS(fetch_word(asic, stream, 1), 3, 32) << 3, NULL, 16, 32);
				ui->add_field(ui, ib_addr + 12, ib_vmid, "ADDRESS_HI", fetch_word(asic, stream, 2), NULL, 16, 32);
			}
			break;
		case 0x47: // EVENT_WRITE_EOP
			ui->add_field(ui, ib_addr + 4, ib_vmid, "EVENT_TYPE", BITS(fetch_word(asic, stream, 0), 0, 6), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "EVENT_INDEX", BITS(fetch_word(asic, stream, 0), 8, 12), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "INV_L2", BITS(fetch_word(asic, stream, 0), 20, 21), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "ADDRESS_LO", BITS(fetch_word(asic, stream, 1), 2, 32) << 2, NULL, 16, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "ADDRESS_HI", BITS(fetch_word(asic, stream, 2), 0, 16), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "DATA_SEL", BITS(fetch_word(asic, stream, 2), 29, 32), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "INT_SEL", BITS(fetch_word(asic, stream, 2), 24, 26), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 16, ib_vmid, "DATA_LO", fetch_word(asic, stream, 3), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 20, ib_vmid, "DATA_HI", fetch_word(asic, stream, 4), NULL, 16, 32);
			break;
		case 0x49: // RELEASE_MEM
			ui->add_field(ui, ib_addr + 4, ib_vmid, "EVENT_TYPE", BITS(fetch_word(asic, stream, 0), 0, 6), vgt_event_decode(BITS(fetch_word(asic, stream, 0), 0, 6)), 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "EVENT_INDEX", BITS(fetch_word(asic, stream, 0), 8, 12), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "GCR_CNTL", BITS(fetch_word(asic, stream, 0), 12, 24), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "TEMPORAL", BITS(fetch_word(asic, stream, 0), 25, 27), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "EXECUTE", BITS(fetch_word(asic, stream, 0), 28, 29), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "GLK_INV", BITS(fetch_word(asic, stream, 0), 30, 31), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "PWS_ENABLE", BITS(fetch_word(asic, stream, 0), 31, 32), NULL, 10, 32);

			ui->add_field(ui, ib_addr + 8, ib_vmid, "DST_SEL", BITS(fetch_word(asic, stream, 1), 16, 18), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "MES_INTR_PIPE", BITS(fetch_word(asic, stream, 1), 20, 22), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "MES_ACTION_ID", BITS(fetch_word(asic, stream, 1), 22, 24), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "INT_SEL", BITS(fetch_word(asic, stream, 1), 24, 27), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "DATA_SEL", BITS(fetch_word(asic, stream, 1), 29, 32), NULL, 10, 32);

			ui->add_field(ui, ib_addr + 12, ib_vmid, "ADDR_LO", fetch_word(asic, stream, 2), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 16, ib_vmid, "ADDR_HI", fetch_word(asic, stream, 3), NULL, 16, 32);

			if (BITS(fetch_word(asic, stream, 1), 24, 27) == 5 ||
			    BITS(fetch_word(asic, stream, 1), 24, 27) == 6) {
				ui->add_field(ui, ib_addr + 20, ib_vmid, "CMP_DATA_LO", fetch_word(asic, stream, 4), NULL, 16, 32);
			} else if (BITS(fetch_word(asic, stream, 1), 29, 32) == 5) {
				ui->add_field(ui, ib_addr + 20, ib_vmid, "DW_OFFSET", BITS(fetch_word(asic, stream, 4), 0, 16), NULL, 16, 32);
				ui->add_field(ui, ib_addr + 20, ib_vmid, "NUM_WORDS", BITS(fetch_word(asic, stream, 4), 16, 32), NULL, 10, 32);
			} else {
				ui->add_field(ui, ib_addr + 20, ib_vmid, "DATA_LO", fetch_word(asic, stream, 4), NULL, 16, 32);
			}

			if (BITS(fetch_word(asic, stream, 1), 24, 27) == 5 ||
			    BITS(fetch_word(asic, stream, 1), 24, 27) == 6)
				ui->add_field(ui, ib_addr + 24, ib_vmid, "CMP_DATA_HI", fetch_word(asic, stream, 5), NULL, 16, 32);
			else
				ui->add_field(ui, ib_addr + 24, ib_vmid, "DATA_HI", fetch_word(asic, stream, 5), NULL, 16, 32);

			ui->add_field(ui, ib_addr + 28, ib_vmid, "INT_CTXID", fetch_word(asic, stream, 6), NULL, 16, 32);
			break;
		case 0x4D: // DISPATCH_TASKMESH_GFX
			ui->add_field(ui, ib_addr + 4, ib_vmid, "XYZ_DIM_LOC", BITS(fetch_word(asic, stream, 0), 0, 16), umr_reg_name(asic, BITS(fetch_word(asic, stream, 0), 0, 16) + 0x2C00), 16, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "RING_ENTRY_LOC", BITS(fetch_word(asic, stream, 0), 16, 32), umr_reg_name(asic, BITS(fetch_word(asic, stream, 0), 16, 32) + 0x2C00), 16, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "THREAD_TRACE_MARKER_ENABLE", BITS(fetch_word(asic, stream, 1), 31, 32), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "XYZ_DIM_ENABLE", BITS(fetch_word(asic, stream, 1), 30, 31), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "MODE1_ENABLE", BITS(fetch_word(asic, stream, 1), 29, 30), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "LINEAR_DISPATCH_ENABLE", BITS(fetch_word(asic, stream, 1), 28, 29), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "DRAW_INITIATOR", fetch_word(asic, stream, 2), NULL, 16, 32);
			break;
		case 0x50: // DMA_DATA
			ui->add_field(ui, ib_addr + 4, ib_vmid, "ENGINE_SEL", BITS(fetch_word(asic, stream, 0), 0, 1), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "SRC_INDIRECT", BITS(fetch_word(asic, stream, 0), 1, 2), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "DST_INDIRECT", BITS(fetch_word(asic, stream, 0), 2, 3), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "SRC_TEMPORAL", BITS(fetch_word(asic, stream, 0), 13, 15), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "DST_SEL", BITS(fetch_word(asic, stream, 0), 20, 22), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "DST_TEMPORAL", BITS(fetch_word(asic, stream, 0), 25, 27), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "SRC_SEL", BITS(fetch_word(asic, stream, 0), 29, 31), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "CP_SYNC", BITS(fetch_word(asic, stream, 0), 31, 32), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "SRC_ADDR_LO_OR_DATA", fetch_word(asic, stream, 1), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "SRC_ADDR_HI", fetch_word(asic, stream, 2), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 16, ib_vmid, "DST_ADDR_LO", fetch_word(asic, stream, 3), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 20, ib_vmid, "DST_ADDR_HI", fetch_word(asic, stream, 4), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 24, ib_vmid, "BYTE_COUNT", BITS(fetch_word(asic, stream, 5), 0, 26), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 24, ib_vmid, "SAS", BITS(fetch_word(asic, stream, 5), 26, 27), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 24, ib_vmid, "DAS", BITS(fetch_word(asic, stream, 5), 27, 28), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 24, ib_vmid, "SAIC", BITS(fetch_word(asic, stream, 5), 28, 29), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 24, ib_vmid, "DAIC", BITS(fetch_word(asic, stream, 5), 29, 30), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 24, ib_vmid, "RAW_WAIT", BITS(fetch_word(asic, stream, 5), 30, 31), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 24, ib_vmid, "DIS_WC", BITS(fetch_word(asic, stream, 5), 31, 32), NULL, 10, 32);
			break;
		case 0x9A: // DMA_DATA_FILL_MULTI
			ui->add_field(ui, ib_addr + 4, ib_vmid, "ENGINE_SEL", BITS(fetch_word(asic, stream, 0), 0, 1), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "MEMLOG_CLEAR", BITS(fetch_word(asic, stream, 0), 10, 11), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "DST_SEL", BITS(fetch_word(asic, stream, 0), 20, 22), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "DST_TEMPORAL", BITS(fetch_word(asic, stream, 0), 25, 27), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "SRC_SEL", BITS(fetch_word(asic, stream, 0), 29, 31), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 4, ib_vmid, "CP_SYNC", BITS(fetch_word(asic, stream, 0), 31, 32), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "BYTE_STRIDE", fetch_word(asic, stream, 1), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "DMA_COUNT", fetch_word(asic, stream, 2), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 16, ib_vmid, "DST_ADDR_LO", fetch_word(asic, stream, 3), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 20, ib_vmid, "DST_ADDR_HI", fetch_word(asic, stream, 4), NULL, 16, 32);
			ui->add_field(ui, ib_addr + 24, ib_vmid, "BYTE_COUNT", BITS(fetch_word(asic, stream, 5), 0, 26), NULL, 10, 32);
			break;
		case 0xA2: // PKT3_MAP_QUEUES
			{ uint32_t ext_sel = BITS(fetch_word(asic, stream, 0), 2, 4),
					   engine_sel = BITS(fetch_word(asic, stream, 0), 26, 29);
				ui->add_field(ui, ib_addr + 4, ib_vmid, "EXT_ENGINE_SEL", BITS(fetch_word(asic, stream, 0), 2, 4), NULL, 10, 32);
				ui->add_field(ui, ib_addr + 4, ib_vmid, "QUEUE_SEL", BITS(fetch_word(asic, stream, 0), 4, 6), NULL, 10, 32);
				ui->add_field(ui, ib_addr + 4, ib_vmid, "VMID", BITS(fetch_word(asic, stream, 0), 8, 12), NULL, 10, 32);
				ui->add_field(ui, ib_addr + 4, ib_vmid, "GWS_ENABLED", BITS(fetch_word(asic, stream, 0), 12, 13), NULL, 10, 32);
				ui->add_field(ui, ib_addr + 4, ib_vmid, "QUEUE_ID", BITS(fetch_word(asic, stream, 0), 13, 16), NULL, 10, 32);
				ui->add_field(ui, ib_addr + 4, ib_vmid, "PIPE_ID", BITS(fetch_word(asic, stream, 0), 16, 18), NULL, 10, 32);
				ui->add_field(ui, ib_addr + 4, ib_vmid, "ME_ID", BITS(fetch_word(asic, stream, 0), 18, 20), NULL, 10, 32);
				ui->add_field(ui, ib_addr + 4, ib_vmid, "QUEUE_TYPE", BITS(fetch_word(asic, stream, 0), 21, 24), NULL, 10, 32);
				ui->add_field(ui, ib_addr + 4, ib_vmid, "STATIC_QUEUE_GROUP", BITS(fetch_word(asic, stream, 0), 24, 26), NULL, 10, 32);
				ui->add_field(ui, ib_addr + 4, ib_vmid, "ENGINE_SEL", BITS(fetch_word(asic, stream, 0), 26, 29), NULL, 10, 32);
				ui->add_field(ui, ib_addr + 4, ib_vmid, "NUM_QUEUES", BITS(fetch_word(asic, stream, 0), 29, 32), NULL, 10, 32);

				ui->add_field(ui, ib_addr + 8, ib_vmid, "CHECK_DISABLE", BITS(fetch_word(asic, stream, 1), 1, 2), NULL, 16, 32);
				ui->add_field(ui, ib_addr + 8, ib_vmid, "DOORBELL_OFFSET", BITS(fetch_word(asic, stream, 1), 2, 28), NULL, 16, 32);
				{
					uint32_t n;
					for (n = 2; n + 4 <= stream->n_words; n += 4) {
						ui->add_field(ui, ib_addr + 12 + 16 * ((n - 2) / 4), ib_vmid, "MQD_ADDR_LO", fetch_word(asic, stream, n), NULL, 16, 32);
						ui->add_field(ui, ib_addr + 16 + 16 * ((n - 2) / 4), ib_vmid, "MQD_ADDR_HI", fetch_word(asic, stream, n + 1), NULL, 16, 32);
						ui->add_field(ui, ib_addr + 20 + 16 * ((n - 2) / 4), ib_vmid, "WPTR_ADDR_LO", fetch_word(asic, stream, n + 2), NULL, 16, 32);
						ui->add_field(ui, ib_addr + 24 + 16 * ((n - 2) / 4), ib_vmid, "WPTR_ADDR_HI", fetch_word(asic, stream, n + 3), NULL, 16, 32);
						if (ui->add_data)
							ui->add_data(ui, asic,
										 ib_addr + 12 + 16 * ((n - 2) / 4), ib_vmid,
										 ((uint64_t)fetch_word(asic, stream, n)) | (((uint64_t)fetch_word(asic, stream, n + 1)) << 32), BITS(fetch_word(asic, stream, 0), 8, 12),
										 UMR_DATABLOCK_MQD_NV, ext_sel == 0 ? engine_sel : 2);
					}
				}
			}
			break;
		case 0xA3: // PKT3_UNMAP_QUEUES
			{
				uint32_t action, queue_sel, num_queues, engine_sel;

				queue_sel = BITS(fetch_word(asic, stream, 0), 4, 6);
				engine_sel = BITS(fetch_word(asic, stream, 0), 26, 29);
				num_queues = BITS(fetch_word(asic, stream, 0), 29, 32);
				action = BITS(fetch_word(asic, stream, 0), 0, 2);
				ui->add_field(ui, ib_addr + 4, ib_vmid, "ACTION", BITS(fetch_word(asic, stream, 0), 0, 2), NULL, 10, 32);
				ui->add_field(ui, ib_addr + 4, ib_vmid, "QUEUE_SEL", queue_sel, NULL, 10, 32);
				ui->add_field(ui, ib_addr + 4, ib_vmid, "ENGINE_SEL", engine_sel, NULL, 10, 32);
				ui->add_field(ui, ib_addr + 4, ib_vmid, "EXT_ENGINE_SEL", BITS(fetch_word(asic, stream, 0), 2, 4), NULL, 10, 32);
				ui->add_field(ui, ib_addr + 4, ib_vmid, "NUM_QUEUES", num_queues, NULL, 10, 32);
				if (queue_sel == 1)
					ui->add_field(ui, ib_addr + 8, ib_vmid, "PASID", BITS(fetch_word(asic, stream, 1), 0, 16), NULL, 10, 32);
				else
					ui->add_field(ui, ib_addr + 8, ib_vmid, "DOORBELL_OFFSET0", BITS(fetch_word(asic, stream, 1), 2, 28), NULL, 16, 32);
				if (action == 3)
					ui->add_field(ui, ib_addr + 12, ib_vmid, "TF_ADDR_LO32", BITS(fetch_word(asic, stream, 2), 2, 32), NULL, 16, 32);
				else
					ui->add_field(ui, ib_addr + 12, ib_vmid, "DOORBELL_OFFSET1", BITS(fetch_word(asic, stream, 2), 2, 28), NULL, 16, 32);
				if (action == 3)
					ui->add_field(ui, ib_addr + 16, ib_vmid, "TF_ADDR_HI32", BITS(fetch_word(asic, stream, 3), 0, 32), NULL, 16, 32);
				else
					ui->add_field(ui, ib_addr + 16, ib_vmid, "DOORBELL_OFFSET2", BITS(fetch_word(asic, stream, 3), 2, 28), NULL, 16, 32);
				if (action == 3)
					ui->add_field(ui, ib_addr + 20, ib_vmid, "TF_DATA", BITS(fetch_word(asic, stream, 4), 0, 32), NULL, 16, 32);
				else
					ui->add_field(ui, ib_addr + 20, ib_vmid, "DOORBELL_OFFSET3", BITS(fetch_word(asic, stream, 4), 2, 28), NULL, 16, 32);
			}
			break;
		case 0xA7: // DISPATCH_DIRECT_INTERLEAVED
			ui->add_field(ui, ib_addr + 4, ib_vmid, "DIM_X", fetch_word(asic, stream, 0), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 8, ib_vmid, "DIM_Y", BITS(fetch_word(asic, stream, 1), 0, 16), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 12, ib_vmid, "DIM_Z", BITS(fetch_word(asic, stream, 2), 0, 16), NULL, 10, 32);
			ui->add_field(ui, ib_addr + 16, ib_vmid, "DISPATCH_INITIATOR", fetch_word(asic, stream, 3), NULL, 16, 32);
			break;
		case 0xB8: // SET_CONTEXT_REG_PAIRS
		case 0xBA: // SET_SH_REG_PAIRS
		case 0xBE: // SET_UCONFIG_REG_PAIRS
			{
				uint32_t offset, n, m;

				switch (stream->opcode) {
					case 0xB8: offset = 0xA000; break;
					case 0xBA: offset = 0x2C00; break;
					case 0xBE: offset = 0xC000; break;
				}

				for (m = n = 0; n < stream->n_words; n += 2, ++m) {
					ui->add_field(ui, ib_addr + 4 + 8 * m, ib_vmid, "REG_OFFSET", BITS(fetch_word(asic, stream, n+0), 0, 16), umr_reg_name(asic, offset + BITS(fetch_word(asic, stream, n+0), 0, 16)), 16, 32);
					ui->add_field(ui, ib_addr + 8 + 8 * m, ib_vmid, "REG_DATA", fetch_word(asic, stream, n+1), NULL, 16, 32);
				}
			}
			break;
		case 0xEF: // UPDATE_DB_SUMMARIZER_TIMEOUTS
			ui->add_field(ui, ib_addr + 4, ib_vmid, "REG_VALUE", fetch_word(asic, stream, 0), NULL, 16, 32);
			break;
		default:
			decode_pkt3_gfx11(asic, ui, stream, ib_addr, ib_vmid);
			break;
	}
}

static void decode_pkt3(struct umr_asic *asic, struct umr_stream_decode_ui *ui, struct umr_pm4_stream *stream, uint64_t ib_addr, uint32_t ib_vmid)
{
	int maj, min;

	umr_gfx_get_ip_ver(asic, &maj, &min);

	switch (maj) {
		case 6:
		case 7:
		case 8: decode_pkt3_gfx8(asic, ui, stream, ib_addr, ib_vmid); break;
		case 9: decode_pkt3_gfx9(asic, ui, stream, ib_addr, ib_vmid); break;
		case 10: decode_pkt3_gfx10(asic, ui, stream, ib_addr, ib_vmid); break;
		case 11: decode_pkt3_gfx11(asic, ui, stream, ib_addr, ib_vmid); break;
		case 12: decode_pkt3_gfx12(asic, ui, stream, ib_addr, ib_vmid); break;
		default:
			asic->err_msg("[BUG]: GFX maj (%d) not supported in decode_pkt3()\n", maj);
			return;
	}
}

/** umr_pm4_decode_stream_opcodes -- Decode sequence of PM4 opcodes from a stream
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
struct umr_pm4_stream *umr_pm4_decode_stream_opcodes(struct umr_asic *asic, struct umr_stream_decode_ui *ui, struct umr_pm4_stream *stream, uint64_t ib_addr, uint32_t ib_vmid, uint64_t from_addr, uint64_t from_vmid, unsigned long opcodes, int follow)
{
	uint32_t nwords, ncodes;
	struct umr_pm4_stream *s;
	const char *opcode_name;
	int maj, min;

	s = stream;
	nwords = 0;
	ncodes = opcodes;
	while (s && ncodes--) {
		nwords += 1 + s->n_words;
		s = s->next;
	}

	umr_gfx_get_ip_ver(asic, &maj, &min);

	ui->start_ib(ui, ib_addr, ib_vmid, from_addr, from_vmid, nwords, 4);
	ncodes = opcodes;
	while (stream && ncodes--) {
		if (stream->pkttype == 0) {
			opcode_name = "PKT0";
		} else if (stream->pkttype == 3) {
			if (maj < pm4_pkt_names[stream->opcode].maj || min < pm4_pkt_names[stream->opcode].min) {
				opcode_name = "UNK";
			} else {
				switch (stream->opcode) {
					case 0x33: // INDIRECT_BUFFER_CONST and COND_INDIRECT_BUFFER_CONST
						opcode_name = (stream->n_words == 3) ? "PKT3_INDIRECT_BUFFER_CONST" : "PKT3_COND_INDIRECT_BUFFER_CONST";
						break;
					case 0x3F: // INDIRECT_BUFFER and COND_INDIRECT_BUFFER
						opcode_name = (stream->n_words == 3) ? "PKT3_INDIRECT_BUFFER" : "PKT3_COND_INDIRECT_BUFFER";
						break;
					default:
						opcode_name = pm4_pkt_names[stream->opcode].name;
				}
			}
		} else if (stream->pkttype == 0xff) {
			// invalid header dword
			if (ui->unhandled_dword)
				ui->unhandled_dword(ui, ib_addr, ib_vmid, stream->header);
		}

		if (!strcmp(opcode_name, "UNK")) {
			if (ui->unhandled)
				ui->unhandled(ui, asic, ib_addr, ib_vmid, stream, UMR_RING_PM4);
		} else {
			if (stream->pkttype == 0 || stream->pkttype == 3)
				ui->start_opcode(ui, ib_addr, ib_vmid, stream->pkttype, stream->opcode, 0, stream->n_words, opcode_name, stream->header, stream->words);

			if (stream->pkttype == 3)
				decode_pkt3(asic, ui, stream, ib_addr, ib_vmid);
			else if (stream->pkttype == 0)
				decode_pkt0(asic, ui, stream, ib_addr, ib_vmid);

			if (stream->invalid)
				break;

			if (stream->shader)
				ui->add_shader(ui, asic, ib_addr, ib_vmid, stream->shader);

			if (follow && stream->ib)
				umr_pm4_decode_stream_opcodes(asic, ui, stream->ib, stream->ib_source.addr, stream->ib_source.vmid, ib_addr, ib_vmid, ~0UL, follow);
		}

		ib_addr += 4 + stream->n_words * 4;
		stream = stream->next;
	}
	ui->done(ui);
	return stream;
}

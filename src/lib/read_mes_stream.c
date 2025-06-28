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

static const char *mes_v10_opcodes[] = {
/* 00 */	"MES_SCH_API_SET_HW_RSRC",
/* 01 */	"MES_SCH_API_SET_SCHEDULING_CONFIG",
/* 02 */	"MES_SCH_API_ADD_QUEUE",
/* 03 */	"MES_SCH_API_REMOVE_QUEUE",
/* 04 */	"MES_SCH_API_PERFORM_YIELD",
/* 05 */	"MES_SCH_API_SET_GANG_PRIORITY_LEVEL",
/* 06 */	"MES_SCH_API_SUSPEND",
/* 07 */	"MES_SCH_API_RESUME",
/* 08 */	"MES_SCH_API_RESET",
/* 09 */	"MES_SCH_API_SET_LOG_BUFFER",
/* 0A */	"MES_SCH_API_CHANGE_GANG_PRORITY",
/* 0B */	"MES_SCH_API_QUERY_SCHEDULER_STATUS",
/* 0C */	"MES_SCH_API_PROGRAM_GDS",
/* 0D */	"MES_SCH_API_SET_DEBUG_VMID",
/* 0E */	"MES_SCH_API_MISC",
/* 0F */	"MES_SCH_API_UPDATE_ROOT_PAGE_TABLE",
/* 10 */	"MES_SCH_API_AMD_LOG",
/* 11 */	"MES_SCH_API_SET_SE_MODE",
/* 12 */	"MES_SCH_API_SET_GANG_SUBMIT",
/* 13 */	"MES_SCH_API_SET_HW_RSRC_1",
};

static char *mes_v10_misc_api_opcodes[] = {
	"MODIFY_REG",
	"INV_GART",
	"QUERY_STATUS",
};

static char *mes_v11_misc_api_opcodes[] = {
	"WRITE_REG",
	"INV_GART",
	"QUERY_STATUS",
	"READ_REG",
	"WAIT_REG_MEM",
	"SET_SHADER_DEBUGGER",
	"NOTIFY_WORK_ON_UNMAPPED_QUEUE",
	"NOTIFY_TO_UNMAP_PROCESSES",
	"CHANGE_CONFIG",
	"LAUNCH_CLEANER_SHADER",
};

static char *mes_v12_misc_api_opcodes[] = {
	"WRITE_REG",
	"INV_GART",
	"QUERY_STATUS",
	"READ_REG",
	"WAIT_REG_MEM",
	"SET_SHADER_DEBUGGER",
	"NOTIFY_WORK_ON_UNMAPPED_QUEUE",
	"NOTIFY_TO_UNMAP_PROCESSES",
	"QUERY_HUNG_ENGINE_ID",
	"CHANGE_CONFIG",
	"LAUNCH_CLEANER_SHADER",
};

static char *mes_v11_wrm_operation[] = {
	"WAIT_REG_MEM",
	"WR_WAIT_WR_REG",
};

static char *mes_v11_change_option[] = {
	"LIMIT_SINGLE_PROCESS",
	"ENABLE_HWS_LOGGING_BUFFER",
	"CHANGE_TDR_CONFIG",
};

static char *mes_v12_change_option[] = {
	"LIMIT_SINGLE_PROCESS",
	"ENABLE_HWS_LOGGING_BUFFER",
	"CHANGE_TDR_CONFIG",
};

static char *mes_v11_set_debug_opcodes[] = {
	"PROGRAM",
	"ALLOCATE",
	"RELEASE",
	"RSVD"
};

static char *mes_v10_add_queue_type[] = {
	"GFX",
	"COMPUTE",
	"SDMA",
	"RSVD"
};

static char *mes_v10_add_queue_priority_level[] = {
	"LOW",
	"NORMAL",
	"MEDIUM",
	"HIGH",
	"REALTIME",
};

static char *mes_v12_set_se_mode[] = {
	"INVALID",
	"SINGLE_SE",
	"DUAL_SE",
	"LOWER_POWER",
};

static char *mes_v12_query_mes_subopcode[] = {
	"GET_CTX_ARRAY_SIZE",
	"CHECK_HEALTHY",
};

#define STR_LOOKUP(str_lut, idx, default) \
	((idx) < sizeof(str_lut) / sizeof(str_lut[0]) ? str_lut[(idx)] : (default))

#define BITS(x, a, b) (unsigned long)((x >> (a)) & ((1ULL << ((b)-(a)))-1))

/**
 * umr_mes_decode_stream - Decode an array of 32-bit words into a MES stream
 *
 * @asic: The ASIC the MES stream is bound to
 * @stream: The pointer to the array of 32-bit words
 * @nwords: The number of 32-bit words
 *
 * Returns a pointer to a umr_mes_stream structure or NULL on error.
 */
struct umr_mes_stream *umr_mes_decode_stream(struct umr_asic *asic, uint32_t *stream, uint32_t nwords)
{
	struct umr_mes_stream *ms, *oms, *prev_ms = NULL;
	uint32_t n;
	struct umr_ip_block *ip;
	int mes_ver_maj = 0;

	ip = umr_find_ip_block(asic, "gfx", asic->options.vm_partition);
	if (!ip) {
		asic->err_msg("[BUG]: Cannot find a 'gfx' IP block in this ASIC\n");
		return NULL;
	}

    if (ip->discoverable.maj == 10 && ip->discoverable.min >= 1) {
		mes_ver_maj = 10;
	} else if (ip->discoverable.maj == 11) {
		mes_ver_maj = 11;
	} else if (ip->discoverable.maj == 12) {
		mes_ver_maj = 12;
	}

	if (!mes_ver_maj) {
		asic->err_msg("[ERROR]: MES decoding not supported on this ASIC\n");
		return NULL;
	}

	oms = ms = calloc(1, sizeof *ms);
	if (!ms)
		goto error;

	while (nwords) {
		ms->nwords = (*stream >> 12) & 0xFF;
		ms->opcode = (*stream >> 4) & 0xFF;
		ms->type   = *stream & 0xF;

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

		ms->header = *stream++;
		ms->words = calloc(ms->nwords - 1, sizeof *(ms->words)); // don't need copy of header
		if (!ms->words)
			goto error;
		for (n = 0; n < ms->nwords - 1; n++) {
			ms->words[n] = *stream++;
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

static uint32_t fetch_word(struct umr_asic *asic, struct umr_mes_stream *stream, uint32_t off)
{
	if (off >= stream->nwords) {
		if (!(stream->invalid))
			asic->err_msg("[ERROR]: MES decoding of opcode (%"PRIx32") went out of bounds.\n", stream->opcode);
		stream->invalid = 1;
		return 0;
	} else {
		return stream->words[off];
	}
}

/**
 * umr_mes_decode_stream_opcodes - decode a stream of MES packets
 *
 * @asic: The ASIC the MES packets are bound for
 * @ui: The user interface callback that will present the decoded packets to the user
 * @stream: The pre-processed stream of MES packets
 * @ib_addr: The base VM address where the packets came from
 * @ib_vmid: The VMID the IB is mapped into
 * @from_addr: The address of the ring/IB that pointed to this MES IB
 * @from_vmid: The VMID of the ring/IB that pointed to this MES IB
 * @opcodes: The number of opcodes to decode
 * @follow: Follow any chained IBs
 *
 * Returns the address of the first packet that hasn't been decoded.
 */
struct umr_mes_stream *umr_mes_decode_stream_opcodes(struct umr_asic *asic, struct umr_stream_decode_ui *ui, struct umr_mes_stream *stream, uint64_t ib_addr, uint32_t ib_vmid, unsigned long opcodes)
{
	char tmpfieldname[64];
	uint32_t i, j;
	struct {
		uint32_t MAX_COMPUTE_PIPES,
			MAX_GFX_PIPES,
			MAX_SDMA_PIPES,
			PRIORITY_NUM_LEVELS,
			MAX_HWIP_SEGMENT,
			MISC_DATA_MAX_SIZE_IN_DWORDS;
	} params;
        struct umr_ip_block *ip;
	int mes_ver_maj, pack8 = 0;
	const char* opcode_name;

	ip = umr_find_ip_block(asic, "gfx", asic->options.vm_partition);
	if (!ip) {
		asic->err_msg("[BUG]: Cannot find a 'gfx' IP block in this ASIC\n");
		return NULL;
	}

    if (ip->discoverable.maj == 10 && ip->discoverable.min >= 1) {
		mes_ver_maj = 10;
	} else if (ip->discoverable.maj == 11) {
		mes_ver_maj = 11;
	} else if (ip->discoverable.maj == 12) {
		mes_ver_maj = 12;
		pack8 = 1; // MESv12 uses 8-byte packed structs instead of 4-byte packed
	} else {
		asic->err_msg("[ERROR]: MES decoding not supported on this ASIC\n");
		return NULL;
	}

	params.MAX_COMPUTE_PIPES = 8;
	params.MAX_GFX_PIPES = 2;
	params.MAX_SDMA_PIPES = 2;
	params.PRIORITY_NUM_LEVELS = 5;
	params.MISC_DATA_MAX_SIZE_IN_DWORDS = 20;
	if (mes_ver_maj == 10) {
		params.MAX_HWIP_SEGMENT = 6;
	} else {
		params.MAX_HWIP_SEGMENT = 8;
	}

// todo: from_* and size
	ui->start_ib(ui, ib_addr, ib_vmid, 0, 0, 0, 0);
	while (stream && opcodes-- && stream->nwords) {
		opcode_name = STR_LOOKUP(mes_v10_opcodes, stream->opcode, "MES_UNK");
		ui->start_opcode(ui, ib_addr, ib_vmid, stream->type, stream->opcode, 0, stream->nwords, opcode_name, stream->header, stream->words);

		i = 0;
		ib_addr += 4; // skip over header
		switch (stream->opcode) {
			case 0: // MESAPI_SET_HW_RESOURCES
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "vmid_mask_mmhub", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "vmid_mask_gfxhub", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "gds_size", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "paging_vmid", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
				for (j = 0; j < params.MAX_COMPUTE_PIPES; j++ ) {
					sprintf(tmpfieldname, "compute_hqd_mask[%" PRIu32 "]", j);
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, tmpfieldname, fetch_word(asic, stream, i), NULL, 16, 32); ++i;
				}
				for (j = 0; j < params.MAX_GFX_PIPES; j++ ) {
					sprintf(tmpfieldname, "gfx_hqd_mask[%" PRIu32 "]", j);
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, tmpfieldname, fetch_word(asic, stream, i), NULL, 16, 32); ++i;
				}
				for (j = 0; j < params.MAX_SDMA_PIPES; j++ ) {
					sprintf(tmpfieldname, "sdma_hqd_mask[%" PRIu32 "]", j);
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, tmpfieldname, fetch_word(asic, stream, i), NULL, 16, 32); ++i;
				}
				for (j = 0; j < params.PRIORITY_NUM_LEVELS; j++ ) {
					sprintf(tmpfieldname, "aggregated_doorbells[%" PRIu32 "]", j);
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, tmpfieldname, fetch_word(asic, stream, i), NULL, 16, 32); ++i;
				}
				if (pack8 && !(i&1)) ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "g_sch_ctx_gpu_mc_ptr", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "query_status_fence_gpu_mc_ptr", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				for (j = 0; j < params.MAX_HWIP_SEGMENT; j++ ) {
					sprintf(tmpfieldname, "gc_base[%" PRIu32 "]", j);
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, tmpfieldname, fetch_word(asic, stream, i), NULL, 16, 32); ++i;
				}
				for (j = 0; j < params.MAX_HWIP_SEGMENT; j++ ) {
					sprintf(tmpfieldname, "mmhub_base[%" PRIu32 "]", j);
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, tmpfieldname, fetch_word(asic, stream, i), NULL, 16, 32); ++i;
				}
				for (j = 0; j < params.MAX_HWIP_SEGMENT; j++ ) {
					sprintf(tmpfieldname, "osssys_base[%" PRIu32 "]", j);
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, tmpfieldname, fetch_word(asic, stream, i), NULL, 16, 32); ++i;
				}
				if (pack8 && !(i&1)) ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "api_completion_fence_addr", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "api_completion_fence_value", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;

				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "disable_reset", (fetch_word(asic, stream, i) >> 0) & 1, NULL, 10, 32);
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "use_different_vmid_compute", (fetch_word(asic, stream, i) >> 1) & 1, NULL, 10, 32);
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "disable_mes_log", (fetch_word(asic, stream, i) >> 2) & 1, NULL, 10, 32);
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "apply_mmhub_pgvm_invalidate_ack_loss_wa", (fetch_word(asic, stream, i) >> 3) & 1, NULL, 10, 32);
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "apply_grbm_remote_register_dummy_read_wa", (fetch_word(asic, stream, i) >> 4) & 1, NULL, 10, 32);
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "second_gfx_pipe_enabled", (fetch_word(asic, stream, i) >> 5) & 1, NULL, 10, 32);
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "enable_level_process_quantum_check", (fetch_word(asic, stream, i) >> 6) & 1, NULL, 10, 32);

				if (mes_ver_maj == 10) {
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "apply_cwsr_program_all_vmid_sq_shader_tba_registers_wa", (fetch_word(asic, stream, i) >> 7) & 1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "enable_mqd_active_poll", (fetch_word(asic, stream, i) >> 8) & 1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "disable_timer_int", (fetch_word(asic, stream, i) >> 9) & 1, NULL, 10, 32);
				} else if (mes_ver_maj >= 11) {
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "legacy_sch_mode", (fetch_word(asic, stream, i) >> 7) & 1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "disable_add_queue_wptr_mc_addr", (fetch_word(asic, stream, i) >> 8) & 1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "enable_mes_event_int_logging", (fetch_word(asic, stream, i) >> 9) & 1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "enable_reg_active_poll", (fetch_word(asic, stream, i) >> 10) & 1, NULL, 10, 32);
					if (mes_ver_maj == 11) {
						ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "use_disable_queue_in_legacy_uq_preemption", (fetch_word(asic, stream, i) >> 11) & 1, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "send_write_data", (fetch_word(asic, stream, i) >> 12) & 1, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "os_tdr_timeout_override", (fetch_word(asic, stream, i) >> 13) & 1, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "use_rs64mem_for_proc_gang_ctx", (fetch_word(asic, stream, i) >> 14) & 1, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "use_add_queue_unmap_flag_addr", (fetch_word(asic, stream, i) >> 15) & 1, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "enable_mes_sch_stb_log", (fetch_word(asic, stream, i) >> 16) & 1, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "limit_single_process", (fetch_word(asic, stream, i) >> 17) & 1, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "is_strix_tmz_wa_enabled", (fetch_word(asic, stream, i) >> 18) & 1, NULL, 10, 32);
					} else 	if (mes_ver_maj == 12) {
						ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "use_disable_queue_in_legacy_uq_preemption", (fetch_word(asic, stream, i) >> 11) & 1, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "send_write_data", (fetch_word(asic, stream, i) >> 12) & 1, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "os_tdr_timeout_override", (fetch_word(asic, stream, i) >> 13) & 1, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "use_rs64mem_for_proc_gang_ctx", (fetch_word(asic, stream, i) >> 14) & 1, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "halt_on_misaligned_access", (fetch_word(asic, stream, i) >> 15) & 1, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "use_add_queue_unmap_flag_addr", (fetch_word(asic, stream, i) >> 16) & 1, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "enable_mes_sch_stb_log", (fetch_word(asic, stream, i) >> 17) & 1, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "limit_single_process", (fetch_word(asic, stream, i) >> 18) & 1, NULL, 10, 32);
						ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "unmapped_doorbell_handling", (fetch_word(asic, stream, i) >> 19) & 3, NULL, 10, 32);
					}
				}
				++i;
				if (mes_ver_maj >= 11) {
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "overscubscription_timer", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
					if (pack8 && !(i&1)) ++i;
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "doorbell_info", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "event_intr_history_gpu_mc_ptr", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				}
				if (mes_ver_maj >= 11) {
					if (pack8 && !(i&1)) ++i;
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "timestamp", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "os_tdr_timeout_in_sec", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
				}
				break;

			case 1: // MES_SCH_API_SET_SCHEDULING_CONFIG
				if (pack8 && !(i&1)) ++i;
				for (j = 0; j < params.PRIORITY_NUM_LEVELS; j++ ) {
					sprintf(tmpfieldname, "grace_period_other_levels[%" PRIu32 "]", j);
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, tmpfieldname, (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				}
				for (j = 0; j < params.PRIORITY_NUM_LEVELS; j++ ) {
					sprintf(tmpfieldname, "process_quantum_for_level[%" PRIu32 "]", j);
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, tmpfieldname, (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				}
				for (j = 0; j < params.PRIORITY_NUM_LEVELS; j++ ) {
					sprintf(tmpfieldname, "process_grace_period_same_level[%" PRIu32 "]", j);
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, tmpfieldname, (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				}
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "normal_yield_percent", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
				if (pack8 && !(i&1)) ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "api_completion_fence_addr", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "api_completion_fence_value", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				if (mes_ver_maj == 12) {
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "timestamp", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				}
				break;

			case 2: // MESAPI__ADD_QUEUE
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "process_id", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
				if (pack8 && !(i&1)) ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "page_table_base_addr", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "process_va_start", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "process_va_end", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "process_quantum", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "process_context_addr", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "gang_quantum", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "gang_context_addr", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "inprocess_gang_priority", fetch_word(asic, stream, i), NULL, 16, 32); ++i;

// todo: guessing what size an enum is...
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "gang_global_priority_level", fetch_word(asic, stream, i), STR_LOOKUP(mes_v10_add_queue_priority_level, fetch_word(asic, stream, i), "UNKNOWN"), 16, 32); ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "doorbell_offset", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
				if (pack8 && !(i&1)) ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "mqd_addr", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "wptr_addr", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "h_context", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "h_queue", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;

// todo: guessing what size an enum is...
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "queue_type", fetch_word(asic, stream, i), STR_LOOKUP(mes_v10_add_queue_type, fetch_word(asic, stream, i), "UNKNOWN"), 16, 32); ++i;

				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "gds_base", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "gds_size", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "gws_base", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "gws_size", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "oa_mask", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
				if (pack8 && !(i&1)) ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "trap_handler_addr", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "vm_context_cntl", fetch_word(asic, stream, i), NULL, 16, 32); ++i;

				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "paging", (fetch_word(asic, stream, i) >> 0) & 1, NULL, 10, 32);
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "debug_vmid", (fetch_word(asic, stream, i) >> 1) & 0xF, NULL, 10, 32);
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "program_gds", (fetch_word(asic, stream, i) >> 5) & 1, NULL, 10, 32);
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "is_gang_suspended", (fetch_word(asic, stream, i) >> 6) & 1, NULL, 10, 32);
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "is_tmz_queue", (fetch_word(asic, stream, i) >> 7) & 1, NULL, 10, 32);
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "map_kiq_utility_queue", (fetch_word(asic, stream, i) >> 8) & 1, NULL, 10, 32);
				if (mes_ver_maj >= 11) {
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "is_kfd_process", (fetch_word(asic, stream, i) >> 9) & 1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "trap_en", (fetch_word(asic, stream, i) >> 10) & 1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "is_aql_queue", (fetch_word(asic, stream, i) >> 11) & 1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "skip_process_ctx_clear", (fetch_word(asic, stream, i) >> 12) & 1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "map_legacy_kq", (fetch_word(asic, stream, i) >> 13) & 1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "exclusively_scheduled", (fetch_word(asic, stream, i) >> 14) & 1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "is_long_running", (fetch_word(asic, stream, i) >> 15) & 1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "is_dwm_queue", (fetch_word(asic, stream, i) >> 16) & 1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "is_video_blit_queue", (fetch_word(asic, stream, i) >> 17) & 1, NULL, 10, 32);
				}
				++i;
				if (pack8 && !(i&1)) ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "api_completion_fence_addr", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "api_completion_fence_value", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				if (mes_ver_maj >= 11) {
					if (pack8 && !(i&1)) ++i;
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "tma_addr", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "sch_id", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
					if (pack8 && !(i&1)) ++i;
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "timestamp", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "process_context_array_index", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "gang_context_array_index", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "pipe_id", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "queue_id", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "alignment_mode_setting", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
					if (pack8 && !(i&1)) ++i;
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "unmap_flag_addr", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				}
				break;

			case 3: // MESAPI__REMOVE_QUEUE
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "doorbell_offset", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
				if (pack8 && !(i&1)) ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "gang_context_addr", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				if (mes_ver_maj < 12) {
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "unmap_legacy_gfx_queue", (fetch_word(asic, stream, i) >> 0) & 1, NULL, 10, 32);
				}
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "unmap_kiq_utility_queue", (fetch_word(asic, stream, i) >> 1) & 1, NULL, 10, 32);
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "preempt_legacy_gfx_queue", (fetch_word(asic, stream, i) >> 2) & 1, NULL, 10, 32);
				if (mes_ver_maj >= 11) {
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "unmap_legacy_queue", (fetch_word(asic, stream, i) >> 3) & 1, NULL, 10, 32);
				}
				++i;
				if (pack8 && !(i&1)) ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "api_completion_fence_addr", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "api_completion_fence_value", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;

				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "pipe_id", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "queue_id", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
				if (pack8 && !(i&1)) ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "tf_addr", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "tf_data", fetch_word(asic, stream, i), NULL, 16, 32); ++i;

// todo: guessing what size an enum is...
				if (mes_ver_maj >= 11) {
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "queue_type", fetch_word(asic, stream, i), STR_LOOKUP(mes_v10_add_queue_type, fetch_word(asic, stream, i), "UNKNOWN"), 16, 32); ++i;
				}
				if (mes_ver_maj == 12) {
					if (pack8 && !(i&1)) ++i;
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "timestamp", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "gang_context_array_index", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
				}
				break;

			case 4: //MESAPI__PERFORM_YIELD
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "dummy", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
				if (pack8 && !(i&1)) ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "api_completion_fence_addr", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "api_completion_fence_value", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				if (mes_ver_maj == 12) {
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "timestamp", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				}
				break;

			case 5: // SET_GANG_PRIORITY_LEVEL
				// What structure is this?
				break;

			case 6: // MESAPI__SUSPEND
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "suspend_all_gangs", (fetch_word(asic, stream, i) >> 0) & 1, NULL, 10, 32); ++i;
				if (pack8 && !(i&1)) ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "gang_context_addr", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "suspend_fence_addr", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "suspend_fence_value", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
				if (pack8 && !(i&1)) ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "api_completion_fence_addr", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "api_completion_fence_value", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				if (mes_ver_maj == 12) {
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "sch_id", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "doorbell_offset", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
					if (pack8 && !(i&1)) ++i;
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "timestamp", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "legacy_uq_type", fetch_word(asic, stream, i), STR_LOOKUP(mes_v10_add_queue_type, fetch_word(asic, stream, i), "UNKNOWN"), 16, 32); ++i;
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "legacy_uq_priority_level", fetch_word(asic, stream, i), STR_LOOKUP(mes_v10_add_queue_priority_level, fetch_word(asic, stream, i), "UNKNOWN"), 16, 32); ++i;
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "gang_context_array_index", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
				}
				break;

			case 7: // MESAPI__RESUME
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "resume_all_gangs", (fetch_word(asic, stream, i) >> 0) & 1, NULL, 10, 32); ++i;
				if (pack8 && !(i&1)) ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "gang_context_addr", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "api_completion_fence_addr", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "api_completion_fence_value", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				if (mes_ver_maj == 12) {
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "doorbell_offset", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
					if (pack8 && !(i&1)) ++i;
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "timestamp", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "gang_context_array_index", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
				}
				break;

			case 8: // MESAPI__RESET
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "reset_queue_only", (fetch_word(asic, stream, i) >> 0) & 1, NULL, 10, 32);
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "hang_detect_then_reset", (fetch_word(asic, stream, i) >> 1) & 1, NULL, 10, 32);
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "hang_detect_only", (fetch_word(asic, stream, i) >> 2) & 1, NULL, 10, 32);
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "reset_legacy_gfx", (fetch_word(asic, stream, i) >> 3) & 1, NULL, 10, 32);
				if (mes_ver_maj == 12) {
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "use_connected_queue_index", (fetch_word(asic, stream, i) >> 4) & 1, NULL, 10, 32);
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "use_connected_queue_index_p1", (fetch_word(asic, stream, i) >> 5) & 1, NULL, 10, 32);
				}
				++i;
				if (pack8 && !(i&1)) ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "gang_context_addr", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "doorbell_offset", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
				if (pack8 && !(i&1)) ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "doorbell_offset_addr", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
// todo: guessing what size an enum is
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "queue_type", fetch_word(asic, stream, i), STR_LOOKUP(mes_v10_add_queue_type, fetch_word(asic, stream, i), "UNKNOWN"), 16, 32); ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "pipe_id_lp", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "queue_id_lp", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "vmid_ip_lp", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
				if (pack8 && !(i&1)) ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "mqd_mc_addr_lp", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "doorbell_offset_lp", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
				if (pack8 && !(i&1)) ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "wptr_addr_lp", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;

				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "pipe_id_hp", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "queue_id_hp", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "vmid_ip_hp", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
				if (pack8 && !(i&1)) ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "mqd_mc_addr_hp", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "doorbell_offset_hp", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
				if (pack8 && !(i&1)) ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "wptr_addr_hp", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "api_completion_fence_addr", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "api_completion_fence_value", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				if (mes_ver_maj == 12) {
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "active_vmids", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
					if (pack8 && !(i&1)) ++i;
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "timestamp", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "gang_context_array_index", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "connected_queue_index", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "connected_queue_index_p1", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
				}
				break;

			case 9: //MESAPI__SET_LOGGING_BUFFER
// todo: guessing what size an enum is
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "queue_type", fetch_word(asic, stream, i), STR_LOOKUP(mes_v10_add_queue_type, fetch_word(asic, stream, i), "UNKNOWN"), 16, 32); ++i;

				if (pack8 && !(i&1)) ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "logging_buffer_addr", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "number_of_entries", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "interrupt_entry", fetch_word(asic, stream, i), NULL, 16, 32); ++i;

				if (pack8 && !(i&1)) ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "api_completion_fence_addr", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "api_completion_fence_value", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				if (mes_ver_maj == 12) {
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "timestamp", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "vmid", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
				}
				break;

			case 10: //MESAPI__CHANGE_GANG_PRIORITY_LEVEL
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "inprocess_gang_priority", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
// todo: guessing what size an enum is...
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "gang_global_priority_level", fetch_word(asic, stream, i), STR_LOOKUP(mes_v10_add_queue_priority_level, fetch_word(asic, stream, i), "UNKNOWN"), 16, 32); ++i;
				if (pack8 && !(i&1)) ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "gang_quantum", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "gang_context_addr", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "api_completion_fence_addr", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "api_completion_fence_value", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				if (mes_ver_maj == 12) {
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "doorbell_offset", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
					if (pack8 && !(i&1)) ++i;
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "timestamp", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "gang_context_array_index", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "queue_quantum_scale", (fetch_word(asic, stream, i)) & 3, NULL, 16, 32);
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "queue_quantum_duration", (fetch_word(asic, stream, i) >> 2) & 0xFF, NULL, 16, 32);
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "apply_quantum_all_processes", (fetch_word(asic, stream, i) >> 10) & 1, NULL, 16, 32);
					++i;
				}
				break;

			case 11: // MESAPI__QUERY_MES_STATUS
			{
				uint32_t subopcode;
				if (mes_ver_maj < 12) {
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "mes_healthy", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
				} else {
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "subopcode", subopcode = fetch_word(asic, stream, i), STR_LOOKUP(mes_v12_query_mes_subopcode, fetch_word(asic, stream, i), "UNKNOWN"), 16, 32); ++i;
				}
				if (pack8 && !(i&1)) ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "api_completion_fence_addr", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "api_completion_fence_value", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				if (mes_ver_maj == 12) {
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "timestamp", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
					switch (subopcode) {
						case 0: // QUERY_MES__GET_CTX_ARRAY_SIZE
							if (pack8 && !(i&1)) ++i;
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "proc_ctx_array_size_addr", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "gang_ctx_array_size_addr", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
							break;
						case 1: // QUERY_MES__CHECK_HEALTHY
							if (pack8 && !(i&1)) ++i;
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "healthy_addr", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
							break;
					}
				}
				break;
			}

			case 12: // MESAPI__PROGRAM_GDS
				if (pack8 && !(i&1)) ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "process_context_addr", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "gds_base", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "gds_size", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "gws_base", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "gws_size", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "oa_mask", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
				if (pack8 && !(i&1)) ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "api_completion_fence_addr", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "api_completion_fence_value", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				break;

			case 13: // MESAPI__SET_DEBUG_VMID
				if (pack8 && !(i&1)) ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "api_completion_fence_addr", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "api_completion_fence_value", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "use_gds", (fetch_word(asic, stream, i) >> 0) & 1, NULL, 10, 32);
				if (mes_ver_maj >= 11) {
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "operation", (fetch_word(asic, stream, i) >> 1) & 3, STR_LOOKUP(mes_v11_set_debug_opcodes, (fetch_word(asic, stream, i) >> 1) & 3, "UNKNOWN"), 10, 32);
				}
				++i;

				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "reserved", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "debug_vmid", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
				if (pack8 && !(i&1)) ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "process_context_addr", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "page_table_base_addr", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "process_va_start", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "process_va_end", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "gds_base", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "gds_size", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "gws_base", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "gws_size", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "oa_mask", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
				if (mes_ver_maj >= 11) {
					if (pack8 && !(i&1)) ++i;
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "output_addr", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				}
				if (mes_ver_maj == 12) {
					if (pack8 && !(i&1)) ++i;
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "timestamp", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "process_vm_cntl", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "queue_type", fetch_word(asic, stream, i), STR_LOOKUP(mes_v10_add_queue_type, fetch_word(asic, stream, i), "UNKNOWN"), 16, 32); ++i;
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "process_context_array_index", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "alignment_mode_setting", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
				}
				break;

			case 14: // MESAPI__MISC (note this changes in v11)
				if (mes_ver_maj == 10) {
					uint32_t misc_opcode;
					misc_opcode = fetch_word(asic, stream, i);
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "opcode", fetch_word(asic, stream, i), STR_LOOKUP(mes_v10_misc_api_opcodes, misc_opcode, "UNKNOWN"), 16, 32); ++i;
					if (pack8 && !(i&1)) ++i;
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "api_completion_fence_addr", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "api_completion_fence_value", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
					switch (misc_opcode) {
						case 0: // MODIFY_REG
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "subcode", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "reg_offset", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "reg_value", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
							break;
						case 1: // INV_GART
							if (pack8 && !(i&1)) ++i;
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "inv_range_va_start", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "inv_range_size", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
							break;
						case 2: // QUERY_STATUS
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "context_id", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
							break;
					}
					break;
				} else if (mes_ver_maj == 11) {
					uint32_t misc_opcode;
					misc_opcode = fetch_word(asic, stream, i);
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "opcode", fetch_word(asic, stream, i), STR_LOOKUP(mes_v11_misc_api_opcodes, misc_opcode, "UNKNOWN"), 16, 32); ++i;
					if (pack8 && !(i&1)) ++i;
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "api_completion_fence_addr", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "api_completion_fence_value", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
					switch (misc_opcode) {
						case 0: // MESAPI_MISC__WRITE_REG
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "reg_offset", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "reg_value", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
							break;
						case 1: // MESAPI_MISC__INV_GART
							if (pack8 && !(i&1)) ++i;
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "inv_range_va_start", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "inv_range_size", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
							break;
						case 2: // MESAPI_MISC__QUERY_STATUS
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "context_id", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
							break;
						case 3: // MESAPI_MISC__READ_REG
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "reg_offset", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
							if (pack8 && !(i&1)) ++i;
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "buffer_addr", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
							break;
						case 4: // MESAPI_MISC__WAIT_REG_MEM
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "op", fetch_word(asic, stream, i), STR_LOOKUP(mes_v11_wrm_operation, fetch_word(asic, stream, i), "UNKNOWN"), 16, 32); ++i;
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "reference", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "mask", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "reg_offset1", fetch_word(asic, stream, i), umr_reg_name(asic, fetch_word(asic, stream, i)), 16, 32); ++i;
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "reg_offset2", fetch_word(asic, stream, i), umr_reg_name(asic, fetch_word(asic, stream, i)), 16, 32); ++i;
							break;
						case 5: // MESAPI_MISC__SET_SHADER_DEBUGGER
							if (pack8 && !(i&1)) ++i;
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "process_context_addr", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "single_memop", fetch_word(asic, stream, i) & 1, NULL, 16, 32);
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "single_alu_op", (fetch_word(asic, stream, i) & 2) >> 1, NULL, 16, 32);
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "process_ctx_flush", (fetch_word(asic, stream, i) >> 31) & 1, NULL, 16, 32); ++i;
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "spi_gdbg_per_vmid_cntl", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "tcp_watch_cntl[0]", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "tcp_watch_cntl[1]", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "tcp_watch_cntl[2]", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "tcp_watch_cntl[3]", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "trap_en", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
							break;
						case 6: // MESAPI_MISC__NOTIFY_WORK_ON_UNMAPPED_QUEUE
							break;
						case 7: // MESAPI_MISC__NOTIFY_WORK_TO_UNMAP_PROCESSES
							break;
						case 8: // MESAPI_MISC__CHANGE_CONFIG
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "opcode", fetch_word(asic, stream, i), STR_LOOKUP(mes_v11_change_option, fetch_word(asic, stream, i), "UNKNOWN"), 16, 32); ++i;
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "limit_single_process", fetch_word(asic, stream, i) & 1, NULL, 16, 32);
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "enable_hws_logging_buffer", (fetch_word(asic, stream, i) & 2) >> 1, NULL, 16, 32); ++i;
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "tdr_level", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "tdr_delay", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
							break;
						case 9: // MESAPI_MISC__LAUNCH_CLEANER_SHADER
							break;
					}
				} else if (mes_ver_maj == 12) {
					uint32_t misc_opcode, j;
					misc_opcode = fetch_word(asic, stream, i);
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "opcode", fetch_word(asic, stream, i), STR_LOOKUP(mes_v12_misc_api_opcodes, misc_opcode, "UNKNOWN"), 16, 32); ++i;
					if (pack8 && !(i&1)) ++i;
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "api_completion_fence_addr", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "api_completion_fence_value", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
					j = i;
					switch (misc_opcode) {
						case 0: // MESAPI_MISC__WRITE_REG
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "reg_offset", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "reg_value", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
							break;
						case 1: // MESAPI_MISC__INV_GART
							if (pack8 && !(i&1)) ++i;
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "inv_range_va_start", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "inv_range_size", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
							break;
						case 2: // MESAPI_MISC__QUERY_STATUS
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "context_id", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
							break;
						case 3: // MESAPI_MISC__READ_REG
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "reg_offset", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
							if (pack8 && !(i&1)) ++i;
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "buffer_addr", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "read64Bits", (fetch_word(asic, stream, i)) & 1, NULL, 16, 32);
							++i;
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "all", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
							break;
						case 4: // MESAPI_MISC__WAIT_REG_MEM
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "op", fetch_word(asic, stream, i), STR_LOOKUP(mes_v11_wrm_operation, fetch_word(asic, stream, i), "UNKNOWN"), 16, 32); ++i;
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "reference", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "mask", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "reg_offset1", fetch_word(asic, stream, i), umr_reg_name(asic, fetch_word(asic, stream, i)), 16, 32); ++i;
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "reg_offset2", fetch_word(asic, stream, i), umr_reg_name(asic, fetch_word(asic, stream, i)), 16, 32); ++i;
							break;
						case 5: // MESAPI_MISC__SET_SHADER_DEBUGGER
							if (pack8 && !(i&1)) ++i;
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "process_context_addr", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "single_memop", fetch_word(asic, stream, i) & 1, NULL, 16, 32);
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "single_alu_op", (fetch_word(asic, stream, i) & 2) >> 1, NULL, 16, 32);
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "process_ctx_flush", (fetch_word(asic, stream, i) >> 31) & 1, NULL, 16, 32); ++i;
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "spi_gdbg_per_vmid_cntl", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "tcp_watch_cntl[0]", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "tcp_watch_cntl[1]", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "tcp_watch_cntl[2]", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "tcp_watch_cntl[3]", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "trap_en", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
							break;
						case 6: // MESAPI_MISC__NOTIFY_WORK_ON_UNMAPPED_QUEUE
							break;
						case 7: // MESAPI_MISC__NOTIFY_WORK_TO_UNMAP_PROCESSES
							break;
						case 8: // MESAPI_MISC__QUERY_HUNG_ENGIGNE_ID
							break;
						case 9: // MESAPI_MISC__CHANGE_CONFIG
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "opcode", fetch_word(asic, stream, i), STR_LOOKUP(mes_v12_change_option, fetch_word(asic, stream, i), "UNKNOWN"), 16, 32); ++i;
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "limit_single_process", fetch_word(asic, stream, i) & 1, NULL, 16, 32);
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "enable_hws_logging_buffer", (fetch_word(asic, stream, i) & 2) >> 1, NULL, 16, 32); ++i;
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "tdr_level", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "tdr_delay", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
							break;
						case 10: // MESAPI_MISC__LAUNCH_CLEANER_SHADER
							break;
						case 11: // MESAPI_MISC__SETUP_MES_DBGEXT
							break;
					}
					i = j + params.MISC_DATA_MAX_SIZE_IN_DWORDS;
					if (pack8 && !(i&1)) ++i;
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "timestamp", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "doorbell_offset", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "os_fence", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
				}
				break;
			case 15: // MESAPI__UPDATE_ROOT_PAGE_TABLE
				if (pack8 && !(i&1)) ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "page_table_base_addr", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "process_context_addr", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "api_completion_fence_addr", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "api_completion_fence_value", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				if (mes_ver_maj == 12) {
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "timestamp", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "process_context_array_index", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
				}
				break;

			case 16: // MESAPI_AMD_LOG
				if (pack8 && !(i&1)) ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "p_buffer_memory", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "p_buffer_size_used", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "api_completion_fence_addr", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "api_completion_fence_value", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				if (mes_ver_maj == 12) {
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "timestamp", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				}
				break;

			case 17: // MESAPI__SET_SE_MODE
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "new_se_mode", fetch_word(asic, stream, i), STR_LOOKUP(mes_v12_set_se_mode, fetch_word(asic, stream, i), "UNKNOWN"), 16, 32); ++i;
				if (pack8 && !(i&1)) ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "cpg_ctxt_sync_fence_addr", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "cpg_ctxt_sync_fence_value", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "log_seq_time", (fetch_word(asic, stream, i)) & 1, NULL, 16, 32);
				++i;
				if (pack8 && !(i&1)) ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "api_completion_fence_addr", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "api_completion_fence_value", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				break;

			case 18: // MESAPI__SET_GANG_SUBMIT
				if (pack8 && !(i&1)) ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "api_completion_fence_addr", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "api_completion_fence_value", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "gang_context_addr", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "slave_gang_context_addr", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "gang_context_array_index", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "slave_gang_context_array_index", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
				break;

			case 19: // MES_SCH_API_SET_HW_RSRC_1
				if (pack8 && !(i&1)) ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "api_completion_fence_addr", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "api_completion_fence_value", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "timestamp", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "enable_mes_info_ctx", BITS(fetch_word(asic, stream, i), 0, 1), NULL, 16, 32);
				++i;
				if (pack8 && !(i&1)) ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "mes_info_ctx_mc_addr", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "mes_info_ctx_size", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
				if (mes_ver_maj == 12) {
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "mes_kiq_unmap_timeout", fetch_word(asic, stream, i), NULL, 16, 32); ++i;
				}
				if (pack8 && !(i&1)) ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "reserved1", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "cleaner_shader_fence_mc_addr", (uint64_t)fetch_word(asic, stream, i) | ((uint64_t)fetch_word(asic, stream, i+1) << 32), NULL, 16, 64); i += 2;
				break;
		}

		if (stream->invalid)
			break;

		ib_addr += 4 * (stream->nwords - 1);
		stream = stream->next;
	}
	ui->done(ui);
	(void)i; // silence "dead increment warnings"
	return stream;
}

/**
 * umr_free_mes_stream - Free a mes stream object
 */
void umr_free_mes_stream(struct umr_mes_stream *stream)
{
	while (stream) {
		struct umr_mes_stream *n;
		n = stream->next;
		free(stream);
		stream = n;
	}
}

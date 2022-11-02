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
#include <umr.h>

static const char *mes_v10_opcodes[] = {
	"MES_SCH_API_SET_HW_RSRC",
	"MES_SCH_API_SET_SCHEDULING_CONFIG",
	"MES_SCH_API_ADD_QUEUE",
	"MES_SCH_API_REMOVE_QUEUE",
	"MES_SCH_API_PERFORM_YIELD",
	"MES_SCH_API_SET_GANG_PRIORITY_LEVEL",
	"MES_SCH_API_SUSPEND",
	"MES_SCH_API_RESUME",
	"MES_SCH_API_RESET",
	"MES_SCH_API_SET_LOG_BUFFER",
	"MES_SCH_API_CHANGE_GANG_PRORITY",
	"MES_SCH_API_QUERY_SCHEDULER_STATUS",
	"MES_SCH_API_PROGRAM_GDS",
	"MES_SCH_API_SET_DEBUG_VMID",
	"MES_SCH_API_MISC",
	"MES_SCH_API_UPDATE_ROOT_PAGE_TABLE",
	"MES_SCH_API_AMD_LOG",
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
};

static char *mes_v11_wrm_operation[] = {
	"WAIT_REG_MEM",
	"WR_WAIT_WR_REG",
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
	"HIGH"
	"REALTIME",
};

struct umr_mes_stream *umr_mes_decode_stream(struct umr_asic *asic, uint32_t *stream, uint32_t nwords)
{
	struct umr_mes_stream *ms, *oms;
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
		if (!ms->nwords)
			break;
		ms->header = *stream++;
		ms->words = calloc(ms->nwords - 1, sizeof *(ms->words)); // don't need copy of header
		if (!ms->words)
			goto error;
		for (n = 0; n < ms->nwords - 1; n++) {
			ms->words[n] = *stream++;
		}
		nwords -= ms->nwords;
		ms->next = calloc(1, sizeof *(ms->next));
		if (!ms->next)
			goto error;
		ms = ms->next;
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

struct umr_mes_stream *umr_mes_decode_stream_opcodes(struct umr_asic *asic, struct umr_mes_stream_decode_ui *ui, struct umr_mes_stream *stream, uint64_t ib_addr, uint32_t ib_vmid, unsigned long opcodes)
{
	char tmpfieldname[64];
	uint32_t i, j;
	struct {
		uint32_t MAX_COMPUTE_PIPES,
			MAX_GFX_PIPES,
			MAX_SDMA_PIPES,
			PRIORITY_NUM_LEVELS,
			MAX_HWIP_SEGMENT;
	} params;
        struct umr_ip_block *ip;
	int mes_ver_maj;

	ip = umr_find_ip_block(asic, "gfx", asic->options.vm_partition);
	if (!ip) {
		asic->err_msg("[BUG]: Cannot find a 'gfx' IP block in this ASIC\n");
		return NULL;
	}

        if (ip->discoverable.maj == 10 && ip->discoverable.min >= 1) {
		mes_ver_maj = 10;
	} else if (ip->discoverable.maj == 11) {
		mes_ver_maj = 11;
	} else {
		asic->err_msg("[ERROR]: MES decoding not supported on this ASIC\n");
		return NULL;
	}

	params.MAX_COMPUTE_PIPES = 8;
	params.MAX_GFX_PIPES = 2;
	params.MAX_SDMA_PIPES = 2;
	params.PRIORITY_NUM_LEVELS = 5;
	if (mes_ver_maj == 10) {
		params.MAX_HWIP_SEGMENT = 6;
	} else {
		params.MAX_HWIP_SEGMENT = 8;
	}

// todo: from_* and size
	ui->start_ib(ui, ib_addr, ib_vmid, 0, 0, 0, 0);
	while (stream && opcodes-- && stream->nwords) {
		ui->start_opcode(ui, ib_addr, ib_vmid, stream->opcode, stream->nwords, mes_v10_opcodes[stream->opcode], stream->header, stream->words);
		i = 0;
		ib_addr += 4; // skip over header
		switch (stream->opcode) {
			case 0: // MESAPI_SET_HW_RESOURCES
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "vmid_mask_mmhub", stream->words[i], NULL, 16); ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "vmid_mask_gfxhub", stream->words[i], NULL, 16); ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "gds_size", stream->words[i], NULL, 16); ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "paging_vmid", stream->words[i], NULL, 16); ++i;
				for (j = 0; j < params.MAX_COMPUTE_PIPES; j++ ) {
					sprintf(tmpfieldname, "compute_hqd_mask[%" PRIu32 "]", j);
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, tmpfieldname, stream->words[i], NULL, 16); ++i;
				}
				for (j = 0; j < params.MAX_GFX_PIPES; j++ ) {
					sprintf(tmpfieldname, "gfx_hqd_mask[%" PRIu32 "]", j);
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, tmpfieldname, stream->words[i], NULL, 16); ++i;
				}
				for (j = 0; j < params.MAX_SDMA_PIPES; j++ ) {
					sprintf(tmpfieldname, "sdma_hqd_mask[%" PRIu32 "]", j);
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, tmpfieldname, stream->words[i], NULL, 16); ++i;
				}
				for (j = 0; j < params.PRIORITY_NUM_LEVELS; j++ ) {
					sprintf(tmpfieldname, "aggregated_doorbells[%" PRIu32 "]", j);
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, tmpfieldname, stream->words[i], NULL, 16); ++i;
				}
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "g_sch_ctx_gpu_mc_ptr", (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "query_status_fence_gpu_mc_ptr", (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;
				for (j = 0; j < params.MAX_HWIP_SEGMENT; j++ ) {
					sprintf(tmpfieldname, "gc_base[%" PRIu32 "]", j);
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, tmpfieldname, stream->words[i], NULL, 16); ++i;
				}
				for (j = 0; j < params.MAX_HWIP_SEGMENT; j++ ) {
					sprintf(tmpfieldname, "mmhub_base[%" PRIu32 "]", j);
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, tmpfieldname, stream->words[i], NULL, 16); ++i;
				}
				for (j = 0; j < params.MAX_HWIP_SEGMENT; j++ ) {
					sprintf(tmpfieldname, "osssys_base[%" PRIu32 "]", j);
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, tmpfieldname, stream->words[i], NULL, 16); ++i;
				}
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "api_completion_fence_addr", (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "api_completion_fence_value", (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;

				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "disable_reset", (stream->words[i] >> 0) & 1, NULL, 10);
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "use_different_vmid_compute", (stream->words[i] >> 1) & 1, NULL, 10);
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "disable_mes_log", (stream->words[i] >> 2) & 1, NULL, 10);
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "apply_mmhub_pgvm_invalidate_ack_loss_wa", (stream->words[i] >> 3) & 1, NULL, 10);
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "apply_grbm_remote_register_dummy_read_wa", (stream->words[i] >> 4) & 1, NULL, 10);
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "second_gfx_pipe_enabled", (stream->words[i] >> 5) & 1, NULL, 10);
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "enable_level_process_quantum_check", (stream->words[i] >> 6) & 1, NULL, 10);
				if (mes_ver_maj == 10) {
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "apply_cwsr_program_all_vmid_sq_shader_tba_registers_wa", (stream->words[i] >> 7) & 1, NULL, 10);
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "enable_mqd_active_poll", (stream->words[i] >> 8) & 1, NULL, 10);
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "disable_timer_int", (stream->words[i] >> 9) & 1, NULL, 10);
				}
				++i;
				if (mes_ver_maj == 11) {
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "overscubscription_timer", stream->words[i], NULL, 16); ++i;
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "doorbell_info", (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;
				}
				break;

			case 1: // MES_SCH_API_SET_SCHEDULING_CONFIG
				for (j = 0; j < params.PRIORITY_NUM_LEVELS; j++ ) {
					sprintf(tmpfieldname, "grace_period_other_levels[%" PRIu32 "]", j);
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, tmpfieldname, (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;
				}
				for (j = 0; j < params.PRIORITY_NUM_LEVELS; j++ ) {
					sprintf(tmpfieldname, "process_quantum_for_level[%" PRIu32 "]", j);
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, tmpfieldname, (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;
				}
				for (j = 0; j < params.PRIORITY_NUM_LEVELS; j++ ) {
					sprintf(tmpfieldname, "process_grace_period_same_level[%" PRIu32 "]", j);
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, tmpfieldname, (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;
				}
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "normal_yield_percent", stream->words[i], NULL, 16); ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "api_completion_fence_addr", (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "api_completion_fence_value", (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;
				break;

			case 2: // MESAPI__ADD_QUEUE
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "process_id", stream->words[i], NULL, 16); ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "page_table_base_addr", (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "process_va_start", (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "process_va_end", (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "process_quantum", (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "process_context_addr", (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "gang_quantum", (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "gang_context_addr", (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "inprocess_gang_priority", stream->words[i], NULL, 16); ++i;

// todo: guessing what size an enum is...
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "gang_global_priority_level", stream->words[i], mes_v10_add_queue_priority_level[stream->words[i]], 16); ++i;

				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "doorbell_offset", stream->words[i], NULL, 16); ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "mqd_addr", (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "wptr_addr", (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "h_context", (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "h_queue", (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;

// todo: guessing what size an enum is...
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "queue_type", stream->words[i], mes_v10_add_queue_type[stream->words[i]], 16); ++i;

				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "gds_base", stream->words[i], NULL, 16); ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "gds_size", stream->words[i], NULL, 16); ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "gws_base", stream->words[i], NULL, 16); ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "gws_size", stream->words[i], NULL, 16); ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "oa_mask", stream->words[i], NULL, 16); ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "trap_handler_addr", (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "vm_context_cntl", stream->words[i], NULL, 16); ++i;

				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "paging", (stream->words[i] >> 0) & 1, NULL, 10);
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "debug_vmid", (stream->words[i] >> 1) & 0xF, NULL, 10);
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "program_gds", (stream->words[i] >> 5) & 1, NULL, 10);
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "is_gang_suspended", (stream->words[i] >> 6) & 1, NULL, 10);
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "is_tmz_queue", (stream->words[i] >> 7) & 1, NULL, 10);
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "map_kiq_utility_queue", (stream->words[i] >> 8) & 1, NULL, 10);
				if (mes_ver_maj == 11) {
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "is_kfd_process", (stream->words[i] >> 9) & 1, NULL, 10);
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "trap_en", (stream->words[i] >> 10) & 1, NULL, 10);
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "is_aql_queue", (stream->words[i] >> 11) & 1, NULL, 10);
				}
				++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "api_completion_fence_addr", (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "api_completion_fence_value", (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;
				if (mes_ver_maj == 11) {
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "tma_addr", (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;
				}
				break;

			case 3: // MESAPI__REMOVE_QUEUE
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "doorbell_offset", stream->words[i], NULL, 16); ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "gang_context_addr", (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "unmap_legacy_gfx_queue", (stream->words[i] >> 0) & 1, NULL, 10);
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "unmap_kiq_utility_queue", (stream->words[i] >> 1) & 1, NULL, 10);
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "preempt_legacy_gfx_queue", (stream->words[i] >> 2) & 1, NULL, 10);
				if (mes_ver_maj == 11) {
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "unmap_legacy_queue", (stream->words[i] >> 3) & 1, NULL, 10);
				}
				++i;

				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "api_completion_fence_addr", (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "api_completion_fence_value", (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;

				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "pipe_id", stream->words[i], NULL, 16); ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "queue_id", stream->words[i], NULL, 16); ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "tf_addr", (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "tf_data", stream->words[i], NULL, 16); ++i;

// todo: guessing what size an enum is...
				if (mes_ver_maj == 11) {
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "queue_type", stream->words[i], mes_v10_add_queue_type[stream->words[i]], 16); ++i;
				}
				break;

			case 4: //MESAPI__PERFORM_YIELD
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "dummy", stream->words[i], NULL, 16); ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "api_completion_fence_addr", (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "api_completion_fence_value", (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;
				break;

			case 5: // SET_GANG_PRIOR
// TODO: I don't see a struct for this in the header
				break;

			case 6: // MESAPI__SUSPEND
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "suspend_all_gangs", (stream->words[i] >> 0) & 1, NULL, 10); ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "gang_context_addr", (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "suspend_fence_addr", (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "suspend_fence_value", stream->words[i], NULL, 16); ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "api_completion_fence_addr", (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "api_completion_fence_value", (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;
				break;

			case 7: // MESAPI__RESUME
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "suspend_all_gangs", (stream->words[i] >> 0) & 1, NULL, 10); ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "gang_context_addr", (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "api_completion_fence_addr", (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "api_completion_fence_value", (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;
				break;

			case 8: // MESAPI__RESET
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "reset_queue_only", (stream->words[i] >> 0) & 1, NULL, 10);
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "hang_detect_then_reset", (stream->words[i] >> 1) & 1, NULL, 10);
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "hang_detect_only", (stream->words[i] >> 2) & 1, NULL, 10);
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "reset_legacy_gfx", (stream->words[i] >> 3) & 1, NULL, 10); ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "gang_context_addr", (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "doorbell_offset", stream->words[i], NULL, 16); ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "doorbell_offset_addr", (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;
// todo: guessing what size an enum is
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "queue_type", stream->words[i], mes_v10_add_queue_type[stream->words[i]], 16); ++i;

				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "pipe_id_lp", stream->words[i], NULL, 16); ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "queue_id_lp", stream->words[i], NULL, 16); ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "vmid_ip_lp", stream->words[i], NULL, 16); ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "mqd_mc_addr_lp", (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "doorbell_offset_lp", stream->words[i], NULL, 16); ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "wptr_addr_lp", (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;

				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "pipe_id_hp", stream->words[i], NULL, 16); ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "queue_id_hp", stream->words[i], NULL, 16); ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "vmid_ip_hp", stream->words[i], NULL, 16); ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "mqd_mc_addr_hp", (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "doorbell_offset_hp", stream->words[i], NULL, 16); ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "wptr_addr_hp", (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "api_completion_fence_addr", (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "api_completion_fence_value", (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;
				break;

			case 9: //MESAPI__SET_LOGGING_BUFFER
// todo: guessing what size an enum is
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "queue_type", stream->words[i], mes_v10_add_queue_type[stream->words[i]], 16); ++i;

				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "logging_buffer_addr", (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "number_of_entries", stream->words[i], NULL, 16); ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "interrupt_entry", stream->words[i], NULL, 16); ++i;

				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "api_completion_fence_addr", (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "api_completion_fence_value", (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;
				break;

			case 10: //MESAPI__CHANGE_GANG_PRIORITY_LEVEL
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "inprocess_gang_priority", stream->words[i], NULL, 16); ++i;
// todo: guessing what size an enum is...
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "gang_global_priority_level", stream->words[i], mes_v10_add_queue_priority_level[stream->words[i]], 16); ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "gang_quantum", (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "gang_context_addr", (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "api_completion_fence_addr", (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "api_completion_fence_value", (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;
				break;

			case 11: // MESAPI__QUERY_MES_STATUS
// todo: guessing what size a bool is...
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "mes_healthy", stream->words[i], NULL, 16); ++i;

				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "api_completion_fence_addr", (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "api_completion_fence_value", (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;
				break;

			case 12: // MESAPI__PROGRAM_GDS
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "process_context_addr", (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "gds_base", stream->words[i], NULL, 16); ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "gds_size", stream->words[i], NULL, 16); ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "gws_base", stream->words[i], NULL, 16); ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "gws_size", stream->words[i], NULL, 16); ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "oa_mask", stream->words[i], NULL, 16); ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "api_completion_fence_addr", (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "api_completion_fence_value", (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;
				break;

			case 13: // MESAPI__SET_DEBUG_VMID
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "api_completion_fence_addr", (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "api_completion_fence_value", (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "use_gds", (stream->words[i] >> 0) & 1, NULL, 10);
				if (mes_ver_maj == 11) {
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "operation", (stream->words[i] >> 1) & 3, mes_v11_set_debug_opcodes[(stream->words[i] >> 1) & 3], 10);
				}
				++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "reserved", stream->words[i], NULL, 16); ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "debug_vmid", stream->words[i], NULL, 16); ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "process_context_addr", (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "page_table_base_addr", (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "process_va_start", (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "process_va_end", (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "gds_base", stream->words[i], NULL, 16); ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "gds_size", stream->words[i], NULL, 16); ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "gws_base", stream->words[i], NULL, 16); ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "gws_size", stream->words[i], NULL, 16); ++i;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "oa_mask", stream->words[i], NULL, 16); ++i;
				if (mes_ver_maj == 11) {
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "output_addr", (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;
				}
				break;

			case 14: // MESAPI__MISC (note this changes in v11)
				if (mes_ver_maj == 10) {
					uint32_t misc_opcode;
// todo: enum size...
					misc_opcode = stream->words[i];
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "opcode", stream->words[i], mes_v10_misc_api_opcodes[misc_opcode], 16); ++i;
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "api_completion_fence_addr", (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "api_completion_fence_value", (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;
					switch (misc_opcode) {
						case 0: // MODIFY_REG
// todo: enum size...
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "subcode", misc_opcode = stream->words[i], NULL, 16); ++i;
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "reg_offset", misc_opcode = stream->words[i], NULL, 16); ++i;
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "reg_value", misc_opcode = stream->words[i], NULL, 16); ++i;
							break;
						case 1: // INV_GART
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "inv_range_va_start", (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "inv_range_size", (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;
							break;
						case 2: // QUERY_STATUS
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "context_id", misc_opcode = stream->words[i], NULL, 16); ++i;
							break;
					}
					break;
				} else if (mes_ver_maj == 11) {
					uint32_t misc_opcode;
// todo: enum size...
					misc_opcode = stream->words[i];
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "opcode", stream->words[i], mes_v11_misc_api_opcodes[misc_opcode], 16); ++i;
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "api_completion_fence_addr", (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;
					ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "api_completion_fence_value", (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;
					switch (misc_opcode) {
						case 0: // WRITE_REG
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "reg_offset", stream->words[i], NULL, 16); ++i;
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "reg_value", stream->words[i], NULL, 16); ++i;
							break;
						case 1: // INV_GART
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "inv_range_va_start", (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "inv_range_size", (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;
							break;
						case 2: // QUERY_STATUS
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "context_id", stream->words[i], NULL, 16); ++i;
							break;
						case 3: // READ_REG
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "reg_offset", stream->words[i], NULL, 16); ++i;
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "buffer_addr", (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;
							break;
						case 4: // WAIT_REG_MEM
// todo: enum size...
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "op", stream->words[i], mes_v11_wrm_operation[stream->words[i]], 16); ++i;
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "reference", stream->words[i], NULL, 16); ++i;
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "mask", stream->words[i], NULL, 16); ++i;
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "reg_offset1", stream->words[i], umr_reg_name(asic, stream->words[i]), 16); ++i;
							ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "reg_offset2", stream->words[i], umr_reg_name(asic, stream->words[i]), 16); ++i;
							break;
					}
				}
				break;
			case 15: // MESAPI__UPDATE_ROOT_PAGE_TABLE
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "page_table_base_addr", (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "process_context_addr", (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "api_completion_fence_addr", (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "api_completion_fence_value", (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;
				break;

			case 16: // MESAPI_AMD_LOG
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "p_buffer_memory", (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "p_buffer_size_used", (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "api_completion_fence_addr", (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;
				ui->add_field(ui, ib_addr + 4 * i, ib_vmid, "api_completion_fence_value", (uint64_t)stream->words[i] | ((uint64_t)stream->words[i+1] << 32), NULL, 26); i += 2;
				break;
		}
		ib_addr += 4 * (stream->nwords - 1);
		stream = stream->next;
	}
	ui->done(ui);
	return stream;
}

/**
 * umr_mes_decode_ring - Read a GPU ring and decode into a mes stream
 *
 * @ringname - Common name of the ring, e.g., 'gfx' or 'comp_1.0.0'
 * @no_halt - Set to 0 to issue an SQ_CMD halt command
 * @start, @stop - Where in the ring to start/stop or leave -1 to use rptr/wptr
 *
 * Return a mes stream if successful.
 */
struct umr_mes_stream *umr_mes_decode_ring(struct umr_asic *asic, char *ringname, int no_halt, int start, int stop)
{
	void *ps = NULL;
	uint32_t *ringdata, ringsize;
	int only_active = 1;

	if (!no_halt && asic->options.halt_waves)
		umr_sq_cmd_halt_waves(asic, UMR_SQ_CMD_HALT);

	// read ring data and reduce indeices modulo ring size
	// since the kernel returned values might be unwrapped.
	ringdata = umr_read_ring_data(asic, ringname, &ringsize);

	if ((stop != -1) && (uint32_t)(stop * 4) >= ringsize)
		stop = (ringsize / 4) - 1;

	if (ringdata) {
		ringsize /= 4;
		ringdata[0] %= ringsize;
		ringdata[1] %= ringsize;

		if (start == -1)
			start = ringdata[0]; // use rptr
		else
			only_active = 0;
		if (stop == -1)
			stop = ringdata[1]; // use wptr
		else
			only_active = 0;

		// only proceed if there is data to read
		// and then linearize it so that the stream
		// decoder can do it's thing
		if (!only_active || start != stop) { // rptr != wptr
			uint32_t *lineardata, linearsize;

			// copy ring data into linear array
			lineardata = calloc(ringsize, sizeof(*lineardata));
			linearsize = 0;
			while (start != stop) {
				lineardata[linearsize++] = ringdata[3 + start];  // first 3 words are rptr/wptr/dwptr
				start = (start + 1) % ringsize;
			}

			ps = umr_mes_decode_stream(asic, lineardata, linearsize);
			free(lineardata);
		}
	}
	free(ringdata);

	if (!no_halt && asic->options.halt_waves)
		umr_sq_cmd_halt_waves(asic, UMR_SQ_CMD_RESUME);

	return ps;
}

struct umr_mes_stream *umr_mes_decode_stream_vm(struct umr_asic *asic, int vm_partition, uint32_t vmid, uint64_t addr, uint32_t nwords)
{
	uint32_t *words;
	struct umr_mes_stream *str;

	words = calloc(sizeof *words, nwords);
	if (!words) {
		asic->err_msg("[ERROR]: Out of memory\n");
		return NULL;
	}
	if (umr_read_vram(asic, vm_partition, vmid, addr, nwords * 4, words)) {
		asic->err_msg("[ERROR]: Could not read vram %" PRIx32 "@0x%"PRIx64"\n", vmid, addr);
		free(words);
		return NULL;
	}
	str = umr_mes_decode_stream(asic, words, nwords);
	free(words);
	return str;
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

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

// TODO: this isn't a library function, move to src/app later

/**
 * umr_dump_ib - Decode an IB and print the contents to stdout
 *
 * @asic:  The ASIC device the IB belongs to
 * @decoder:  Contains informationa about the state of the decoding
 */
void umr_dump_ib(struct umr_asic *asic, struct umr_ring_decoder *decoder)
{
	uint32_t *data = NULL, x;
	static const char *hubs[] = { "gfxhub", "mmhub" };
	const char *hub;

	if ((decoder->next_ib_info.vmid >> 8) < 2)
		hub = hubs[decoder->next_ib_info.vmid >> 8];
	else
		hub = "";

	printf("Dumping IB at (%s%s%s) VMID:%u 0x%" PRIx64 " of %u words from ",
		GREEN, hub, RST,
		(unsigned)decoder->next_ib_info.vmid & 0xFF,
		decoder->next_ib_info.ib_addr,
		(unsigned)decoder->next_ib_info.size/4);

	if (decoder->src.ib_addr == 0)
		printf("ring[%s%u%s]", BLUE, (unsigned)decoder->src.addr, RST);
	else
		printf("IB[%s%u%s@%s0x%" PRIx64 "%s + %s0x%x%s]",
			BLUE, (int)decoder->src.vmid & 0xFF, RST,
			YELLOW, decoder->src.ib_addr, RST,
			YELLOW, (unsigned)decoder->src.addr * 4, RST);

	printf("\n");

	// read IB
	data = calloc(sizeof(*data), decoder->next_ib_info.size/sizeof(*data));
	if (data && !umr_read_vram(asic, asic->options.vm_partition, decoder->next_ib_info.vmid, decoder->next_ib_info.ib_addr, decoder->next_ib_info.size, data)) {
	// dump IB
		decoder->pm4.cur_opcode = 0xFFFFFFFF;
		decoder->sdma.cur_opcode = 0xFFFFFFFF;
		for (x = 0; x < decoder->next_ib_info.size/4; x++) {
			decoder->next_ib_info.addr = x;
			printf("IB[%s%u%s@%s0x%" PRIx64 "%s + %s0x%-4x%s] = %s0x%08lx%s ... ",
				BLUE, decoder->next_ib_info.vmid & 0xFF, RST,
				YELLOW, decoder->next_ib_info.ib_addr, RST,
				YELLOW, (unsigned)x * 4, RST,
				GREEN, (unsigned long)data[x], RST);
			umr_print_decode(asic, decoder, data[x], NULL);
			printf("\n");
		}
	}
	free(data);
	printf("End of IB\n\n");
}

void umr_dump_shaders(struct umr_asic *asic, struct umr_ring_decoder *decoder, struct umr_wave_data *wd)
{
	struct umr_shaders_pgm *pshader, *shader;

	shader = decoder->shader;
	while (shader) {
		printf("Disassembly of shader %" PRIu32 "@0x%" PRIx64 " of length %" PRIu32 " bytes from IB[%s%u%s@%s0x%" PRIx64 "%s + %s0x%x%s]\n",
				shader->vmid, shader->addr,
				shader->size,
				BLUE, (unsigned)shader->vmid, RST,
				YELLOW, shader->src.ib_base, RST,
				YELLOW, (unsigned)shader->src.ib_offset * 4, RST);
		umr_vm_disasm(asic, asic->options.vm_partition, shader->vmid, shader->addr, 0, shader->size, 0, wd);
		printf("\n");
		pshader = shader->next;
		free(shader);
		shader = pshader;
	}
}

static void dump_mqd_compute_nv(struct umr_asic *asic, struct umr_pm4_data_block *p)
{
	static const struct {
		uint32_t offset;
		char *label;
	} mqdl[] = {
		{ 0, "header" },
		{ 1, "compute_dispatch_initiator" },
		{ 2, "compute_dim_x" },
		{ 3, "compute_dim_y" },
		{ 4, "compute_dim_z" },
		{ 5, "compute_start_x" },
		{ 6, "compute_start_y" },
		{ 7, "compute_start_z" },
		{ 8, "compute_num_thread_x" },
		{ 9, "compute_num_thread_y" },
		{ 10, "compute_num_thread_z" },
		{ 11, "compute_pipelinestat_enable" },
		{ 12, "compute_perfcount_enable" },
		{ 13, "compute_pgm_lo" },
		{ 14, "compute_pgm_hi" },
		{ 15, "compute_tba_lo" },
		{ 16, "compute_tba_hi" },
		{ 17, "compute_tma_lo" },
		{ 18, "compute_tma_hi" },
		{ 19, "compute_pgm_rsrc1" },
		{ 20, "compute_pgm_rsrc2" },
		{ 21, "compute_vmid" },
		{ 22, "compute_resource_limits" },
		{ 23, "compute_static_thread_mgmt_se0" },
		{ 24, "compute_static_thread_mgmt_se1" },
		{ 25, "compute_tmpring_size" },
		{ 26, "compute_static_thread_mgmt_se2" },
		{ 27, "compute_static_thread_mgmt_se3" },
		{ 28, "compute_restart_x" },
		{ 29, "compute_restart_y" },
		{ 30, "compute_restart_z" },
		{ 31, "compute_thread_trace_enable" },
		{ 33, "compute_dispatch_id" },
		{ 34, "compute_threadgroup_id" },
		{ 35, "compute_relaunch" },
		{ 36, "compute_wave_restore_addr_lo" },
		{ 37, "compute_wave_restore_addr_hi" },
		{ 38, "compute_wave_restore_control" },
		{ 65, "compute_user_data_0" },
		{ 66, "compute_user_data_1" },
		{ 67, "compute_user_data_2" },
		{ 68, "compute_user_data_3" },
		{ 69, "compute_user_data_4" },
		{ 70, "compute_user_data_5" },
		{ 71, "compute_user_data_6" },
		{ 72, "compute_user_data_7" },
		{ 73, "compute_user_data_8" },
		{ 74, "compute_user_data_9" },
		{ 75, "compute_user_data_10" },
		{ 76, "compute_user_data_11" },
		{ 77, "compute_user_data_12" },
		{ 78, "compute_user_data_13" },
		{ 79, "compute_user_data_14" },
		{ 80, "compute_user_data_15" },
		{ 81, "cp_compute_csinvoc_count_lo" },
		{ 82, "cp_compute_csinvoc_count_hi" },
		{ 86, "cp_mqd_query_time_lo" },
		{ 87, "cp_mqd_query_time_hi" },
		{ 88, "cp_mqd_connect_start_time_lo" },
		{ 89, "cp_mqd_connect_start_time_hi" },
		{ 90, "cp_mqd_connect_end_time_lo" },
		{ 91, "cp_mqd_connect_end_time_hi" },
		{ 92, "cp_mqd_connect_end_wf_count" },
		{ 93, "cp_mqd_connect_end_pq_rptr" },
		{ 94, "cp_mqd_connect_end_pq_wptr" },
		{ 95, "cp_mqd_connect_end_ib_rptr" },
		{ 96, "cp_mqd_readindex_lo" },
		{ 97, "cp_mqd_readindex_hi" },
		{ 98, "cp_mqd_save_start_time_lo" },
		{ 99, "cp_mqd_save_start_time_hi" },
		{ 100, "cp_mqd_save_end_time_lo" },
		{ 101, "cp_mqd_save_end_time_hi" },
		{ 102, "cp_mqd_restore_start_time_lo" },
		{ 103, "cp_mqd_restore_start_time_hi" },
		{ 104, "cp_mqd_restore_end_time_lo" },
		{ 105, "cp_mqd_restore_end_time_hi" },
		{ 106, "disable_queue" },
		{ 108, "gds_cs_ctxsw_cnt0" },
		{ 109, "gds_cs_ctxsw_cnt1" },
		{ 110, "gds_cs_ctxsw_cnt2" },
		{ 111, "gds_cs_ctxsw_cnt3" },
		{ 114, "cp_pq_exe_status_lo" },
		{ 115, "cp_pq_exe_status_hi" },
		{ 116, "cp_packet_id_lo" },
		{ 117, "cp_packet_id_hi" },
		{ 118, "cp_packet_exe_status_lo" },
		{ 119, "cp_packet_exe_status_hi" },
		{ 120, "gds_save_base_addr_lo" },
		{ 121, "gds_save_base_addr_hi" },
		{ 122, "gds_save_mask_lo" },
		{ 123, "gds_save_mask_hi" },
		{ 124, "ctx_save_base_addr_lo" },
		{ 125, "ctx_save_base_addr_hi" },
		{ 128, "cp_mqd_base_addr_lo" },
		{ 129, "cp_mqd_base_addr_hi" },
		{ 130, "cp_hqd_active" },
		{ 131, "cp_hqd_vmid" },
		{ 132, "cp_hqd_persistent_state" },
		{ 133, "cp_hqd_pipe_priority" },
		{ 134, "cp_hqd_queue_priority" },
		{ 135, "cp_hqd_quantum" },
		{ 136, "cp_hqd_pq_base_lo" },
		{ 137, "cp_hqd_pq_base_hi" },
		{ 138, "cp_hqd_pq_rptr" },
		{ 139, "cp_hqd_pq_rptr_report_addr_lo" },
		{ 140, "cp_hqd_pq_rptr_report_addr_hi" },
		{ 141, "cp_hqd_pq_wptr_poll_addr_lo" },
		{ 142, "cp_hqd_pq_wptr_poll_addr_hi" },
		{ 143, "cp_hqd_pq_doorbell_control" },
		{ 145, "cp_hqd_pq_control" },
		{ 146, "cp_hqd_ib_base_addr_lo" },
		{ 147, "cp_hqd_ib_base_addr_hi" },
		{ 148, "cp_hqd_ib_rptr" },
		{ 149, "cp_hqd_ib_control" },
		{ 150, "cp_hqd_iq_timer" },
		{ 151, "cp_hqd_iq_rptr" },
		{ 152, "cp_hqd_dequeue_request" },
		{ 153, "cp_hqd_dma_offload" },
		{ 154, "cp_hqd_sema_cmd" },
		{ 155, "cp_hqd_msg_type" },
		{ 156, "cp_hqd_atomic0_preop_lo" },
		{ 157, "cp_hqd_atomic0_preop_hi" },
		{ 158, "cp_hqd_atomic1_preop_lo" },
		{ 159, "cp_hqd_atomic1_preop_hi" },
		{ 160, "cp_hqd_hq_scheduler0" },
		{ 161, "cp_hqd_hq_scheduler1" },
		{ 162, "cp_mqd_control" },
		{ 163, "cp_hqd_hq_status1" },
		{ 164, "cp_hqd_hq_control1" },
		{ 165, "cp_hqd_eop_base_addr_lo" },
		{ 166, "cp_hqd_eop_base_addr_hi" },
		{ 167, "cp_hqd_eop_control" },
		{ 168, "cp_hqd_eop_rptr" },
		{ 169, "cp_hqd_eop_wptr" },
		{ 170, "cp_hqd_eop_done_events" },
		{ 171, "cp_hqd_ctx_save_base_addr_lo" },
		{ 172, "cp_hqd_ctx_save_base_addr_hi" },
		{ 173, "cp_hqd_ctx_save_control" },
		{ 174, "cp_hqd_cntl_stack_offset" },
		{ 175, "cp_hqd_cntl_stack_size" },
		{ 176, "cp_hqd_wg_state_offset" },
		{ 177, "cp_hqd_ctx_save_size" },
		{ 178, "cp_hqd_gds_resource_state" },
		{ 179, "cp_hqd_error" },
		{ 180, "cp_hqd_eop_wptr_mem" },
		{ 181, "cp_hqd_aql_control" },
		{ 182, "cp_hqd_pq_wptr_lo" },
		{ 183, "cp_hqd_pq_wptr_hi" },
		{ 184, "cp_hqd_suspend_cntl_stack_offset" },
		{ 185, "cp_hqd_suspend_cntl_stack_dw_cnt" },
		{ 186, "cp_hqd_suspend_wg_state_offset" },
		{ 192, "iqtimer_pkt_header" },
		{ 193, "iqtimer_pkt_dw0" },
		{ 194, "iqtimer_pkt_dw1" },
		{ 195, "iqtimer_pkt_dw2" },
		{ 196, "iqtimer_pkt_dw3" },
		{ 197, "iqtimer_pkt_dw4" },
		{ 198, "iqtimer_pkt_dw5" },
		{ 199, "iqtimer_pkt_dw6" },
		{ 200, "iqtimer_pkt_dw7" },
		{ 201, "iqtimer_pkt_dw8" },
		{ 202, "iqtimer_pkt_dw9" },
		{ 203, "iqtimer_pkt_dw10" },
		{ 204, "iqtimer_pkt_dw11" },
		{ 205, "iqtimer_pkt_dw12" },
		{ 206, "iqtimer_pkt_dw13" },
		{ 207, "iqtimer_pkt_dw14" },
		{ 208, "iqtimer_pkt_dw15" },
		{ 209, "iqtimer_pkt_dw16" },
		{ 210, "iqtimer_pkt_dw17" },
		{ 211, "iqtimer_pkt_dw18" },
		{ 212, "iqtimer_pkt_dw19" },
		{ 213, "iqtimer_pkt_dw20" },
		{ 214, "iqtimer_pkt_dw21" },
		{ 215, "iqtimer_pkt_dw22" },
		{ 216, "iqtimer_pkt_dw23" },
		{ 217, "iqtimer_pkt_dw24" },
		{ 218, "iqtimer_pkt_dw25" },
		{ 219, "iqtimer_pkt_dw26" },
		{ 220, "iqtimer_pkt_dw27" },
		{ 221, "iqtimer_pkt_dw28" },
		{ 222, "iqtimer_pkt_dw29" },
		{ 223, "iqtimer_pkt_dw30" },
		{ 224, "iqtimer_pkt_dw31" },
		{ 228, "set_resources_header" },
		{ 229, "set_resources_dw1" },
		{ 230, "set_resources_dw2" },
		{ 231, "set_resources_dw3" },
		{ 232, "set_resources_dw4" },
		{ 233, "set_resources_dw5" },
		{ 234, "set_resources_dw6" },
		{ 235, "set_resources_dw7" },
		{ 240, "queue_doorbell_id0" },
		{ 241, "queue_doorbell_id1" },
		{ 242, "queue_doorbell_id2" },
		{ 243, "queue_doorbell_id3" },
		{ 244, "queue_doorbell_id4" },
		{ 245, "queue_doorbell_id5" },
		{ 246, "queue_doorbell_id6" },
		{ 247, "queue_doorbell_id7" },
		{ 248, "queue_doorbell_id8" },
		{ 249, "queue_doorbell_id9" },
		{ 250, "queue_doorbell_id10" },
		{ 251, "queue_doorbell_id11" },
		{ 252, "queue_doorbell_id12" },
		{ 253, "queue_doorbell_id13" },
		{ 254, "queue_doorbell_id14" },
		{ 255, "queue_doorbell_id15" },
		{ 0, NULL },
	};
	uint32_t mqd[512], x, y;

	if (!umr_read_vram(asic, asic->options.vm_partition, p->vmid, p->addr, 512 * 4, &mqd[0])) {
		for (x = 0; x < 512; x++) {
			for (y = 0; mqdl[y].label; y++) {
				if (mqdl[y].offset == x) {
					printf("\tMQD[0x%03" PRIx32 "] == 0x%08" PRIx32 " (%s)\n", x, mqd[x], mqdl[y].label);
					break;
				}
			}
			if (!mqdl[y].label && asic->options.verbose)
				printf("\tMQD[0x%03" PRIx32 "] == 0x%08" PRIx32 " (reserved)\n", x, mqd[x]);
		}
	}
}

static void dump_mqd_graphics_nv(struct umr_asic *asic, struct umr_pm4_data_block *p)
{
	static const struct {
		uint32_t offset;
		char *label;
	} mqdl[] = {
		{ 106, "disable_queue" },
		{ 128, "cp_mqd_base_addr" },
		{ 129, "cp_mqd_base_addr_hi" },
		{ 130, "cp_gfx_hqd_active" },
		{ 131, "cp_gfx_hqd_vmid" },
		{ 134, "cp_gfx_hqd_queue_priority" },
		{ 135, "cp_gfx_hqd_quantum" },
		{ 136, "cp_gfx_hqd_base" },
		{ 137, "cp_gfx_hqd_base_hi" },
		{ 138, "cp_gfx_hqd_rptr" },
		{ 139, "cp_gfx_hqd_rptr_addr" },
		{ 140, "cp_gfx_hqd_rptr_addr_hi" },
		{ 141, "cp_rb_wptr_poll_addr_lo" },
		{ 142, "cp_rb_wptr_poll_addr_hi" },
		{ 143, "cp_rb_doorbell_control" },
		{ 144, "cp_gfx_hqd_offset" },
		{ 145, "cp_gfx_hqd_cntl" },
		{ 148, "cp_gfx_hqd_csmd_rptr" },
		{ 149, "cp_gfx_hqd_wptr" },
		{ 150, "cp_gfx_hqd_wptr_hi" },
		{ 156, "cp_gfx_hqd_mapped" },
		{ 157, "cp_gfx_hqd_que_mgr_control" },
		{ 160, "cp_gfx_hqd_hq_status0" },
		{ 161, "cp_gfx_hqd_hq_control0" },
		{ 162, "cp_gfx_mqd_control" },
		{ 170, "cp_num_prim_needed_count0_lo" },
		{ 171, "cp_num_prim_needed_count0_hi" },
		{ 172, "cp_num_prim_needed_count1_lo" },
		{ 173, "cp_num_prim_needed_count1_hi" },
		{ 174, "cp_num_prim_needed_count2_lo" },
		{ 175, "cp_num_prim_needed_count2_hi" },
		{ 176, "cp_num_prim_needed_count3_lo" },
		{ 177, "cp_num_prim_needed_count3_hi" },
		{ 178, "cp_num_prim_written_count0_lo" },
		{ 179, "cp_num_prim_written_count0_hi" },
		{ 180, "cp_num_prim_written_count1_lo" },
		{ 181, "cp_num_prim_written_count1_hi" },
		{ 182, "cp_num_prim_written_count2_lo" },
		{ 183, "cp_num_prim_written_count2_hi" },
		{ 184, "cp_num_prim_written_count3_lo" },
		{ 185, "cp_num_prim_written_count3_hi" },
		{ 190, "mp1_smn_fps_cnt" },
		{ 191, "sq_thread_trace_buf0_base" },
		{ 192, "sq_thread_trace_buf0_size" },
		{ 193, "sq_thread_trace_buf1_base" },
		{ 194, "sq_thread_trace_buf1_size" },
		{ 195, "sq_thread_trace_wptr" },
		{ 196, "sq_thread_trace_mask" },
		{ 197, "sq_thread_trace_token_mask" },
		{ 198, "sq_thread_trace_ctrl" },
		{ 199, "sq_thread_trace_status" },
		{ 200, "sq_thread_trace_dropped_cntr" },
		{ 201, "sq_thread_trace_finish_done_debug" },
		{ 202, "sq_thread_trace_gfx_draw_cntr" },
		{ 203, "sq_thread_trace_gfx_marker_cntr" },
		{ 204, "sq_thread_trace_hp3d_draw_cntr" },
		{ 205, "sq_thread_trace_hp3d_marker_cntr" },
		{ 208, "cp_sc_psinvoc_count0_lo" },
		{ 209, "cp_sc_psinvoc_count0_hi" },
		{ 210, "cp_pa_cprim_count_lo" },
		{ 211, "cp_pa_cprim_count_hi" },
		{ 212, "cp_pa_cinvoc_count_lo" },
		{ 213, "cp_pa_cinvoc_count_hi" },
		{ 214, "cp_vgt_vsinvoc_count_lo" },
		{ 215, "cp_vgt_vsinvoc_count_hi" },
		{ 216, "cp_vgt_gsinvoc_count_lo" },
		{ 217, "cp_vgt_gsinvoc_count_hi" },
		{ 218, "cp_vgt_gsprim_count_lo" },
		{ 219, "cp_vgt_gsprim_count_hi" },
		{ 220, "cp_vgt_iaprim_count_lo" },
		{ 221, "cp_vgt_iaprim_count_hi" },
		{ 222, "cp_vgt_iavert_count_lo" },
		{ 223, "cp_vgt_iavert_count_hi" },
		{ 224, "cp_vgt_hsinvoc_count_lo" },
		{ 225, "cp_vgt_hsinvoc_count_hi" },
		{ 226, "cp_vgt_dsinvoc_count_lo" },
		{ 227, "cp_vgt_dsinvoc_count_hi" },
		{ 228, "cp_vgt_csinvoc_count_lo" },
		{ 229, "cp_vgt_csinvoc_count_hi" },
		{ 268, "vgt_strmout_buffer_filled_size_0" },
		{ 269, "vgt_strmout_buffer_filled_size_1" },
		{ 270, "vgt_strmout_buffer_filled_size_2" },
		{ 271, "vgt_strmout_buffer_filled_size_3" },
		{ 276, "vgt_dma_max_size" },
		{ 277, "vgt_dma_num_instances" },
		{ 288, "it_set_base_ib_addr_lo" },
		{ 289, "it_set_base_ib_addr_hi" },
		{ 356, "spi_shader_pgm_rsrc3_ps" },
		{ 357, "spi_shader_pgm_rsrc3_vs" },
		{ 358, "spi_shader_pgm_rsrc3_gs" },
		{ 359, "spi_shader_pgm_rsrc3_hs" },
		{ 360, "spi_shader_pgm_rsrc4_ps" },
		{ 361, "spi_shader_pgm_rsrc4_vs" },
		{ 362, "spi_shader_pgm_rsrc4_gs" },
		{ 363, "spi_shader_pgm_rsrc4_hs" },
		{ 364, "db_occlusion_count0_low_00" },
		{ 365, "db_occlusion_count0_hi_00" },
		{ 366, "db_occlusion_count1_low_00" },
		{ 367, "db_occlusion_count1_hi_00" },
		{ 368, "db_occlusion_count2_low_00" },
		{ 369, "db_occlusion_count2_hi_00" },
		{ 370, "db_occlusion_count3_low_00" },
		{ 371, "db_occlusion_count3_hi_00" },
		{ 372, "db_occlusion_count0_low_01" },
		{ 373, "db_occlusion_count0_hi_01" },
		{ 374, "db_occlusion_count1_low_01" },
		{ 375, "db_occlusion_count1_hi_01" },
		{ 376, "db_occlusion_count2_low_01" },
		{ 377, "db_occlusion_count2_hi_01" },
		{ 378, "db_occlusion_count3_low_01" },
		{ 379, "db_occlusion_count3_hi_01" },
		{ 380, "db_occlusion_count0_low_02" },
		{ 381, "db_occlusion_count0_hi_02" },
		{ 382, "db_occlusion_count1_low_02" },
		{ 383, "db_occlusion_count1_hi_02" },
		{ 384, "db_occlusion_count2_low_02" },
		{ 385, "db_occlusion_count2_hi_02" },
		{ 386, "db_occlusion_count3_low_02" },
		{ 387, "db_occlusion_count3_hi_02" },
		{ 388, "db_occlusion_count0_low_03" },
		{ 389, "db_occlusion_count0_hi_03" },
		{ 390, "db_occlusion_count1_low_03" },
		{ 391, "db_occlusion_count1_hi_03" },
		{ 392, "db_occlusion_count2_low_03" },
		{ 393, "db_occlusion_count2_hi_03" },
		{ 394, "db_occlusion_count3_low_03" },
		{ 395, "db_occlusion_count3_hi_03" },
		{ 396, "db_occlusion_count0_low_04" },
		{ 397, "db_occlusion_count0_hi_04" },
		{ 398, "db_occlusion_count1_low_04" },
		{ 399, "db_occlusion_count1_hi_04" },
		{ 400, "db_occlusion_count2_low_04" },
		{ 401, "db_occlusion_count2_hi_04" },
		{ 402, "db_occlusion_count3_low_04" },
		{ 403, "db_occlusion_count3_hi_04" },
		{ 404, "db_occlusion_count0_low_05" },
		{ 405, "db_occlusion_count0_hi_05" },
		{ 406, "db_occlusion_count1_low_05" },
		{ 407, "db_occlusion_count1_hi_05" },
		{ 408, "db_occlusion_count2_low_05" },
		{ 409, "db_occlusion_count2_hi_05" },
		{ 410, "db_occlusion_count3_low_05" },
		{ 411, "db_occlusion_count3_hi_05" },
		{ 412, "db_occlusion_count0_low_06" },
		{ 413, "db_occlusion_count0_hi_06" },
		{ 414, "db_occlusion_count1_low_06" },
		{ 415, "db_occlusion_count1_hi_06" },
		{ 416, "db_occlusion_count2_low_06" },
		{ 417, "db_occlusion_count2_hi_06" },
		{ 418, "db_occlusion_count3_low_06" },
		{ 419, "db_occlusion_count3_hi_06" },
		{ 420, "db_occlusion_count0_low_07" },
		{ 421, "db_occlusion_count0_hi_07" },
		{ 422, "db_occlusion_count1_low_07" },
		{ 423, "db_occlusion_count1_hi_07" },
		{ 424, "db_occlusion_count2_low_07" },
		{ 425, "db_occlusion_count2_hi_07" },
		{ 426, "db_occlusion_count3_low_07" },
		{ 427, "db_occlusion_count3_hi_07" },
		{ 428, "db_occlusion_count0_low_10" },
		{ 429, "db_occlusion_count0_hi_10" },
		{ 430, "db_occlusion_count1_low_10" },
		{ 431, "db_occlusion_count1_hi_10" },
		{ 432, "db_occlusion_count2_low_10" },
		{ 433, "db_occlusion_count2_hi_10" },
		{ 434, "db_occlusion_count3_low_10" },
		{ 435, "db_occlusion_count3_hi_10" },
		{ 436, "db_occlusion_count0_low_11" },
		{ 437, "db_occlusion_count0_hi_11" },
		{ 438, "db_occlusion_count1_low_11" },
		{ 439, "db_occlusion_count1_hi_11" },
		{ 440, "db_occlusion_count2_low_11" },
		{ 441, "db_occlusion_count2_hi_11" },
		{ 442, "db_occlusion_count3_low_11" },
		{ 443, "db_occlusion_count3_hi_11" },
		{ 444, "db_occlusion_count0_low_12" },
		{ 445, "db_occlusion_count0_hi_12" },
		{ 446, "db_occlusion_count1_low_12" },
		{ 447, "db_occlusion_count1_hi_12" },
		{ 448, "db_occlusion_count2_low_12" },
		{ 449, "db_occlusion_count2_hi_12" },
		{ 450, "db_occlusion_count3_low_12" },
		{ 451, "db_occlusion_count3_hi_12" },
		{ 452, "db_occlusion_count0_low_13" },
		{ 453, "db_occlusion_count0_hi_13" },
		{ 454, "db_occlusion_count1_low_13" },
		{ 455, "db_occlusion_count1_hi_13" },
		{ 456, "db_occlusion_count2_low_13" },
		{ 457, "db_occlusion_count2_hi_13" },
		{ 458, "db_occlusion_count3_low_13" },
		{ 459, "db_occlusion_count3_hi_13" },
		{ 460, "db_occlusion_count0_low_14" },
		{ 461, "db_occlusion_count0_hi_14" },
		{ 462, "db_occlusion_count1_low_14" },
		{ 463, "db_occlusion_count1_hi_14" },
		{ 464, "db_occlusion_count2_low_14" },
		{ 465, "db_occlusion_count2_hi_14" },
		{ 466, "db_occlusion_count3_low_14" },
		{ 467, "db_occlusion_count3_hi_14" },
		{ 468, "db_occlusion_count0_low_15" },
		{ 469, "db_occlusion_count0_hi_15" },
		{ 470, "db_occlusion_count1_low_15" },
		{ 471, "db_occlusion_count1_hi_15" },
		{ 472, "db_occlusion_count2_low_15" },
		{ 473, "db_occlusion_count2_hi_15" },
		{ 474, "db_occlusion_count3_low_15" },
		{ 475, "db_occlusion_count3_hi_15" },
		{ 476, "db_occlusion_count0_low_16" },
		{ 477, "db_occlusion_count0_hi_16" },
		{ 478, "db_occlusion_count1_low_16" },
		{ 479, "db_occlusion_count1_hi_16" },
		{ 480, "db_occlusion_count2_low_16" },
		{ 481, "db_occlusion_count2_hi_16" },
		{ 482, "db_occlusion_count3_low_16" },
		{ 483, "db_occlusion_count3_hi_16" },
		{ 484, "db_occlusion_count0_low_17" },
		{ 485, "db_occlusion_count0_hi_17" },
		{ 486, "db_occlusion_count1_low_17" },
		{ 487, "db_occlusion_count1_hi_17" },
		{ 488, "db_occlusion_count2_low_17" },
		{ 489, "db_occlusion_count2_hi_17" },
		{ 490, "db_occlusion_count3_low_17" },
		{ 491, "db_occlusion_count3_hi_17" },
		{ 0, NULL },
	};
	uint32_t mqd[512], x, y;

	if (!umr_read_vram(asic, asic->options.vm_partition, p->vmid, p->addr, 512 * 4, &mqd[0])) {
		for (x = 0; x < 512; x++) {
			for (y = 0; mqdl[y].label; y++) {
				if (mqdl[y].offset == x) {
					printf("\tMQD[0x%03" PRIx32 "] == 0x%08" PRIx32 " (%s)\n", x, mqd[x], mqdl[y].label);
					break;
				}
			}
			if (!mqdl[y].label && asic->options.verbose)
				printf("\tMQD[0x%03" PRIx32 "] == 0x%08" PRIx32 " (reserved)\n", x, mqd[x]);
		}
	}
}

static void dump_mqd_sdma_nv(struct umr_asic *asic, struct umr_pm4_data_block *p)
{
	static const struct {
		uint32_t offset;
		char *label;
	} mqdl[] = {
		{ 0, "sdmax_rlcx_rb_cntl" },
		{ 1, "sdmax_rlcx_rb_base" },
		{ 2, "sdmax_rlcx_rb_base_hi" },
		{ 3, "sdmax_rlcx_rb_rptr" },
		{ 4, "sdmax_rlcx_rb_rptr_hi" },
		{ 5, "sdmax_rlcx_rb_wptr" },
		{ 6, "sdmax_rlcx_rb_wptr_hi" },
		{ 7, "sdmax_rlcx_rb_wptr_poll_cntl" },
		{ 8, "sdmax_rlcx_rb_rptr_addr_hi" },
		{ 9, "sdmax_rlcx_rb_rptr_addr_lo" },
		{ 10, "sdmax_rlcx_ib_cntl" },
		{ 11, "sdmax_rlcx_ib_rptr" },
		{ 12, "sdmax_rlcx_ib_offset" },
		{ 13, "sdmax_rlcx_ib_base_lo" },
		{ 14, "sdmax_rlcx_ib_base_hi" },
		{ 15, "sdmax_rlcx_ib_size" },
		{ 16, "sdmax_rlcx_skip_cntl" },
		{ 17, "sdmax_rlcx_context_status" },
		{ 18, "sdmax_rlcx_doorbell" },
		{ 19, "sdmax_rlcx_status" },
		{ 20, "sdmax_rlcx_doorbell_log" },
		{ 21, "sdmax_rlcx_watermark" },
		{ 22, "sdmax_rlcx_doorbell_offset" },
		{ 23, "sdmax_rlcx_csa_addr_lo" },
		{ 24, "sdmax_rlcx_csa_addr_hi" },
		{ 25, "sdmax_rlcx_ib_sub_remain" },
		{ 26, "sdmax_rlcx_preempt" },
		{ 27, "sdmax_rlcx_dummy_reg" },
		{ 28, "sdmax_rlcx_rb_wptr_poll_addr_hi" },
		{ 29, "sdmax_rlcx_rb_wptr_poll_addr_lo" },
		{ 30, "sdmax_rlcx_rb_aql_cntl" },
		{ 31, "sdmax_rlcx_minor_ptr_update" },
		{ 32, "sdmax_rlcx_midcmd_data0" },
		{ 33, "sdmax_rlcx_midcmd_data1" },
		{ 34, "sdmax_rlcx_midcmd_data2" },
		{ 35, "sdmax_rlcx_midcmd_data3" },
		{ 36, "sdmax_rlcx_midcmd_data4" },
		{ 37, "sdmax_rlcx_midcmd_data5" },
		{ 38, "sdmax_rlcx_midcmd_data6" },
		{ 39, "sdmax_rlcx_midcmd_data7" },
		{ 40, "sdmax_rlcx_midcmd_data8" },
		{ 41, "sdmax_rlcx_midcmd_cntl" },
		{ 128, "sdma_engine_id" },
		{ 129, "sdma_queue_id" },
		{ 0, NULL },
	};
	uint32_t mqd[128], x, y;

	if (!umr_read_vram(asic, asic->options.vm_partition, p->vmid, p->addr, 128 * 4, &mqd[0])) {
		for (x = 0; x < 128; x++) {
			for (y = 0; mqdl[y].label; y++) {
				if (mqdl[y].offset == x) {
					printf("\tMQD[0x%03" PRIx32 "] == 0x%08" PRIx32 " (%s)\n", x, mqd[x], mqdl[y].label);
					break;
				}
			}
			if (!mqdl[y].label && asic->options.verbose)
				printf("\tMQD[0x%03" PRIx32 "] == 0x%08" PRIx32 " (reserved)\n", x, mqd[x]);
		}
	}
}

static void dump_mqd_sdma_vi(struct umr_asic *asic, struct umr_pm4_data_block *p)
{
	static const struct {
		uint32_t offset;
		char *label;
	} mqdl[] = {
		{ 0, "sdmax_rlcx_rb_cntl" },
		{ 1, "sdmax_rlcx_rb_base" },
		{ 2, "sdmax_rlcx_rb_base_hi" },
		{ 3, "sdmax_rlcx_rb_rptr" },
		{ 4, "sdmax_rlcx_rb_rptr_hi" },
		{ 5, "sdmax_rlcx_rb_wptr" },
		{ 6, "sdmax_rlcx_rb_wptr_hi" },
		{ 7, "sdmax_rlcx_rb_wptr_poll_cntl" },
		{ 8, "sdmax_rlcx_rb_rptr_addr_hi" },
		{ 9, "sdmax_rlcx_rb_rptr_addr_lo" },
		{ 10, "sdmax_rlcx_ib_cntl" },
		{ 11, "sdmax_rlcx_ib_rptr" },
		{ 12, "sdmax_rlcx_ib_offset" },
		{ 13, "sdmax_rlcx_ib_base_lo" },
		{ 14, "sdmax_rlcx_ib_base_hi" },
		{ 15, "sdmax_rlcx_ib_size" },
		{ 16, "sdmax_rlcx_skip_cntl" },
		{ 17, "sdmax_rlcx_context_status" },
		{ 18, "sdmax_rlcx_doorbell" },
		{ 19, "sdmax_rlcx_status" },
		{ 20, "sdmax_rlcx_doorbell_log" },
		{ 21, "sdmax_rlcx_watermark" },
		{ 22, "sdmax_rlcx_doorbell_offset" },
		{ 23, "sdmax_rlcx_csa_addr_lo" },
		{ 24, "sdmax_rlcx_csa_addr_hi" },
		{ 25, "sdmax_rlcx_ib_sub_remain" },
		{ 26, "sdmax_rlcx_preempt" },
		{ 27, "sdmax_rlcx_dummy_reg" },
		{ 28, "sdmax_rlcx_rb_wptr_poll_addr_hi" },
		{ 29, "sdmax_rlcx_rb_wptr_poll_addr_lo" },
		{ 30, "sdmax_rlcx_rb_aql_cntl" },
		{ 31, "sdmax_rlcx_minor_ptr_update" },
		{ 32, "sdmax_rlcx_midcmd_data0" },
		{ 33, "sdmax_rlcx_midcmd_data1" },
		{ 34, "sdmax_rlcx_midcmd_data2" },
		{ 35, "sdmax_rlcx_midcmd_data3" },
		{ 36, "sdmax_rlcx_midcmd_data4" },
		{ 37, "sdmax_rlcx_midcmd_data5" },
		{ 38, "sdmax_rlcx_midcmd_data6" },
		{ 39, "sdmax_rlcx_midcmd_data7" },
		{ 40, "sdmax_rlcx_midcmd_data8" },
		{ 41, "sdmax_rlcx_midcmd_cntl" },
		{ 126, "sdma_engine_id" },
		{ 127, "sdma_queue_id" },
		{ 0, NULL },
	};
	uint32_t mqd[128], x, y;

	if (!umr_read_vram(asic, asic->options.vm_partition, p->vmid, p->addr, 128 * 4, &mqd[0])) {
		for (x = 0; x < 128; x++) {
			for (y = 0; mqdl[y].label; y++) {
				if (mqdl[y].offset == x) {
					printf("\tMQD[0x%03" PRIx32 "] == 0x%08" PRIx32 " (%s)\n", x, mqd[x], mqdl[y].label);
					break;
				}
			}
			if (!mqdl[y].label && asic->options.verbose)
				printf("\tMQD[0x%03" PRIx32 "] == 0x%08" PRIx32 " (reserved)\n", x, mqd[x]);
		}
	}
}

static void dump_mqd_compute_vi(struct umr_asic *asic, struct umr_pm4_data_block *p)
{
	static const struct {
		uint32_t offset;
		char *label;
	} mqdl[] = {
		{ 0, "header" },
		{ 1, "compute_dispatch_initiator" },
		{ 2, "compute_dim_x" },
		{ 3, "compute_dim_y" },
		{ 4, "compute_dim_z" },
		{ 5, "compute_start_x" },
		{ 6, "compute_start_y" },
		{ 7, "compute_start_z" },
		{ 8, "compute_num_thread_x" },
		{ 9, "compute_num_thread_y" },
		{ 10, "compute_num_thread_z" },
		{ 11, "compute_pipelinestat_enable" },
		{ 12, "compute_perfcount_enable" },
		{ 13, "compute_pgm_lo" },
		{ 14, "compute_pgm_hi" },
		{ 15, "compute_tba_lo" },
		{ 16, "compute_tba_hi" },
		{ 17, "compute_tma_lo" },
		{ 18, "compute_tma_hi" },
		{ 19, "compute_pgm_rsrc1" },
		{ 20, "compute_pgm_rsrc2" },
		{ 21, "compute_vmid" },
		{ 22, "compute_resource_limits" },
		{ 23, "compute_static_thread_mgmt_se0" },
		{ 24, "compute_static_thread_mgmt_se1" },
		{ 25, "compute_tmpring_size" },
		{ 26, "compute_static_thread_mgmt_se2" },
		{ 27, "compute_static_thread_mgmt_se3" },
		{ 28, "compute_restart_x" },
		{ 29, "compute_restart_y" },
		{ 30, "compute_restart_z" },
		{ 31, "compute_thread_trace_enable" },
		{ 33, "compute_dispatch_id" },
		{ 34, "compute_threadgroup_id" },
		{ 35, "compute_relaunch" },
		{ 36, "compute_wave_restore_addr_lo" },
		{ 37, "compute_wave_restore_addr_hi" },
		{ 38, "compute_wave_restore_control" },
		{ 39, "compute_static_thread_mgmt_se4" },
		{ 40, "compute_static_thread_mgmt_se5" },
		{ 41, "compute_static_thread_mgmt_se6" },
		{ 42, "compute_static_thread_mgmt_se7" },
		{ 65, "compute_user_data_0" },
		{ 66, "compute_user_data_1" },
		{ 67, "compute_user_data_2" },
		{ 68, "compute_user_data_3" },
		{ 69, "compute_user_data_4" },
		{ 70, "compute_user_data_5" },
		{ 71, "compute_user_data_6" },
		{ 72, "compute_user_data_7" },
		{ 73, "compute_user_data_8" },
		{ 74, "compute_user_data_9" },
		{ 75, "compute_user_data_10" },
		{ 76, "compute_user_data_11" },
		{ 77, "compute_user_data_12" },
		{ 78, "compute_user_data_13" },
		{ 79, "compute_user_data_14" },
		{ 80, "compute_user_data_15" },
		{ 81, "cp_compute_csinvoc_count_lo" },
		{ 82, "cp_compute_csinvoc_count_hi" },
		{ 86, "cp_mqd_query_time_lo" },
		{ 87, "cp_mqd_query_time_hi" },
		{ 88, "cp_mqd_connect_start_time_lo" },
		{ 89, "cp_mqd_connect_start_time_hi" },
		{ 90, "cp_mqd_connect_end_time_lo" },
		{ 91, "cp_mqd_connect_end_time_hi" },
		{ 92, "cp_mqd_connect_end_wf_count" },
		{ 93, "cp_mqd_connect_end_pq_rptr" },
		{ 94, "cp_mqd_connect_end_pq_wptr" },
		{ 95, "cp_mqd_connect_end_ib_rptr" },
		{ 96, "cp_mqd_readindex_lo" },
		{ 97, "cp_mqd_readindex_hi" },
		{ 98, "cp_mqd_save_start_time_lo" },
		{ 99, "cp_mqd_save_start_time_hi" },
		{ 100, "cp_mqd_save_end_time_lo" },
		{ 101, "cp_mqd_save_end_time_hi" },
		{ 102, "cp_mqd_restore_start_time_lo" },
		{ 103, "cp_mqd_restore_start_time_hi" },
		{ 104, "cp_mqd_restore_end_time_lo" },
		{ 105, "cp_mqd_restore_end_time_hi" },
		{ 106, "disable_queue" },
		{ 108, "gds_cs_ctxsw_cnt0" },
		{ 109, "gds_cs_ctxsw_cnt1" },
		{ 110, "gds_cs_ctxsw_cnt2" },
		{ 111, "gds_cs_ctxsw_cnt3" },
		{ 114, "cp_pq_exe_status_lo" },
		{ 115, "cp_pq_exe_status_hi" },
		{ 116, "cp_packet_id_lo" },
		{ 117, "cp_packet_id_hi" },
		{ 118, "cp_packet_exe_status_lo" },
		{ 119, "cp_packet_exe_status_hi" },
		{ 120, "gds_save_base_addr_lo" },
		{ 121, "gds_save_base_addr_hi" },
		{ 122, "gds_save_mask_lo" },
		{ 123, "gds_save_mask_hi" },
		{ 124, "ctx_save_base_addr_lo" },
		{ 125, "ctx_save_base_addr_hi" },
		{ 126, "dynamic_cu_mask_addr_lo" },
		{ 127, "dynamic_cu_mask_addr_hi" },
		{ 128, "cp_mqd_base_addr_lo" },
		{ 129, "cp_mqd_base_addr_hi" },
		{ 130, "cp_hqd_active" },
		{ 131, "cp_hqd_vmid" },
		{ 132, "cp_hqd_persistent_state" },
		{ 133, "cp_hqd_pipe_priority" },
		{ 134, "cp_hqd_queue_priority" },
		{ 135, "cp_hqd_quantum" },
		{ 136, "cp_hqd_pq_base_lo" },
		{ 137, "cp_hqd_pq_base_hi" },
		{ 138, "cp_hqd_pq_rptr" },
		{ 139, "cp_hqd_pq_rptr_report_addr_lo" },
		{ 140, "cp_hqd_pq_rptr_report_addr_hi" },
		{ 141, "cp_hqd_pq_wptr_poll_addr_lo" },
		{ 142, "cp_hqd_pq_wptr_poll_addr_hi" },
		{ 143, "cp_hqd_pq_doorbell_control" },
		{ 145, "cp_hqd_pq_control" },
		{ 146, "cp_hqd_ib_base_addr_lo" },
		{ 147, "cp_hqd_ib_base_addr_hi" },
		{ 148, "cp_hqd_ib_rptr" },
		{ 149, "cp_hqd_ib_control" },
		{ 150, "cp_hqd_iq_timer" },
		{ 151, "cp_hqd_iq_rptr" },
		{ 152, "cp_hqd_dequeue_request" },
		{ 153, "cp_hqd_dma_offload" },
		{ 154, "cp_hqd_sema_cmd" },
		{ 155, "cp_hqd_msg_type" },
		{ 156, "cp_hqd_atomic0_preop_lo" },
		{ 157, "cp_hqd_atomic0_preop_hi" },
		{ 158, "cp_hqd_atomic1_preop_lo" },
		{ 159, "cp_hqd_atomic1_preop_hi" },
		{ 160, "cp_hqd_hq_status0" },
		{ 161, "cp_hqd_hq_control0" },
		{ 162, "cp_mqd_control" },
		{ 163, "cp_hqd_hq_status1" },
		{ 164, "cp_hqd_hq_control1" },
		{ 165, "cp_hqd_eop_base_addr_lo" },
		{ 166, "cp_hqd_eop_base_addr_hi" },
		{ 167, "cp_hqd_eop_control" },
		{ 168, "cp_hqd_eop_rptr" },
		{ 169, "cp_hqd_eop_wptr" },
		{ 170, "cp_hqd_eop_done_events" },
		{ 171, "cp_hqd_ctx_save_base_addr_lo" },
		{ 172, "cp_hqd_ctx_save_base_addr_hi" },
		{ 173, "cp_hqd_ctx_save_control" },
		{ 174, "cp_hqd_cntl_stack_offset" },
		{ 175, "cp_hqd_cntl_stack_size" },
		{ 176, "cp_hqd_wg_state_offset" },
		{ 177, "cp_hqd_ctx_save_size" },
		{ 178, "cp_hqd_gds_resource_state" },
		{ 179, "cp_hqd_error" },
		{ 180, "cp_hqd_eop_wptr_mem" },
		{ 181, "cp_hqd_aql_control" },
		{ 182, "cp_hqd_pq_wptr_lo" },
		{ 183, "cp_hqd_pq_wptr_hi" },
		{ 192, "iqtimer_pkt_header" },
		{ 193, "iqtimer_pkt_dw0" },
		{ 194, "iqtimer_pkt_dw1" },
		{ 195, "iqtimer_pkt_dw2" },
		{ 196, "iqtimer_pkt_dw3" },
		{ 197, "iqtimer_pkt_dw4" },
		{ 198, "iqtimer_pkt_dw5" },
		{ 199, "iqtimer_pkt_dw6" },
		{ 200, "iqtimer_pkt_dw7" },
		{ 201, "iqtimer_pkt_dw8" },
		{ 202, "iqtimer_pkt_dw9" },
		{ 203, "iqtimer_pkt_dw10" },
		{ 204, "iqtimer_pkt_dw11" },
		{ 205, "iqtimer_pkt_dw12" },
		{ 206, "iqtimer_pkt_dw13" },
		{ 207, "iqtimer_pkt_dw14" },
		{ 208, "iqtimer_pkt_dw15" },
		{ 209, "iqtimer_pkt_dw16" },
		{ 210, "iqtimer_pkt_dw17" },
		{ 211, "iqtimer_pkt_dw18" },
		{ 212, "iqtimer_pkt_dw19" },
		{ 213, "iqtimer_pkt_dw20" },
		{ 214, "iqtimer_pkt_dw21" },
		{ 215, "iqtimer_pkt_dw22" },
		{ 216, "iqtimer_pkt_dw23" },
		{ 217, "iqtimer_pkt_dw24" },
		{ 218, "iqtimer_pkt_dw25" },
		{ 219, "iqtimer_pkt_dw26" },
		{ 220, "iqtimer_pkt_dw27" },
		{ 221, "iqtimer_pkt_dw28" },
		{ 222, "iqtimer_pkt_dw29" },
		{ 223, "iqtimer_pkt_dw30" },
		{ 224, "iqtimer_pkt_dw31" },
		{ 228, "set_resources_header" },
		{ 229, "set_resources_dw1" },
		{ 230, "set_resources_dw2" },
		{ 231, "set_resources_dw3" },
		{ 232, "set_resources_dw4" },
		{ 233, "set_resources_dw5" },
		{ 234, "set_resources_dw6" },
		{ 235, "set_resources_dw7" },
		{ 240, "queue_doorbell_id0" },
		{ 241, "queue_doorbell_id1" },
		{ 242, "queue_doorbell_id2" },
		{ 243, "queue_doorbell_id3" },
		{ 244, "queue_doorbell_id4" },
		{ 245, "queue_doorbell_id5" },
		{ 246, "queue_doorbell_id6" },
		{ 247, "queue_doorbell_id7" },
		{ 248, "queue_doorbell_id8" },
		{ 249, "queue_doorbell_id9" },
		{ 250, "queue_doorbell_id10" },
		{ 251, "queue_doorbell_id11" },
		{ 252, "queue_doorbell_id12" },
		{ 253, "queue_doorbell_id13" },
		{ 254, "queue_doorbell_id14" },
		{ 255, "queue_doorbell_id15" },
		{ 0, NULL },
	};
	uint32_t mqd[512], x, y;

	if (!umr_read_vram(asic, asic->options.vm_partition, p->vmid, p->addr, 512 * 4, &mqd[0])) {
		for (x = 0; x < 512; x++) {
			for (y = 0; mqdl[y].label; y++) {
				if (mqdl[y].offset == x) {
					printf("\tMQD[0x%03" PRIx32 "] == 0x%08" PRIx32 " (%s)\n", x, mqd[x], mqdl[y].label);
					break;
				}
			}
			if (!mqdl[y].label && asic->options.verbose)
				printf("\tMQD[0x%03" PRIx32 "] == 0x%08" PRIx32 " (reserved)\n", x, mqd[x]);
		}
	}
}


void umr_dump_data(struct umr_asic *asic, struct umr_ring_decoder *decoder)
{
	struct umr_pm4_data_block *p = decoder->datablock, *op;
	static const char *datatypenames[] = { "VIMQD", "NVMQD" };
	while (p) {
		printf("\n\nDumping datablock %" PRIu32 "@0x%" PRIx64 " of type %d[%s] ",
			   p->vmid,
			   p->addr,
			   p->type, datatypenames[p->type]);
		switch (p->type) {
			case UMR_DATABLOCK_MQD_VI: {
				static const char *selnames[] = { "compute", "reserved", "sdma0", "sdma1", "gfx" };
				printf("sub-type %d[%s]\n", p->extra, selnames[p->extra]);
				switch (p->extra) { // ENGINE_SEL from MAP_QUEUES packet
					case 0: dump_mqd_compute_vi(asic, p); break; // compute
					case 2:
					case 3: dump_mqd_sdma_vi(asic, p); break; // SDMA
					case 4: break;
				}
				break;
				}
			case UMR_DATABLOCK_MQD_NV: {
				static const char *selnames[] = { "compute", "reserved", "sdma0", "sdma1", "gfx" };
				printf("sub-type %d[%s]\n", p->extra, selnames[p->extra]);
				switch (p->extra) { // ENGINE_SEL from MAP_QUEUES packet
					case 0: dump_mqd_compute_nv(asic, p); break; // compute
					case 2:
					case 3: dump_mqd_sdma_nv(asic, p); break; // SDMA
					case 4: dump_mqd_graphics_nv(asic, p); break;
				}
				break;
				}
		}
		op = p;
		p = p->next;
		free(op);
	}
	decoder->datablock = NULL;
}

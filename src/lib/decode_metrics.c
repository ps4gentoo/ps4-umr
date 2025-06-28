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
#include <stddef.h>

#define METRICS_HEADER_LIST(__FIELD)	\
	__FIELD(structure_size),	\
	__FIELD(format_revision),	\
	__FIELD(content_revision),

#define METRICS_INFO_V1_0_LIST(__FIELD) \
	__FIELD(system_clock_counter),	\
	__FIELD(temperature_edge),	\
	__FIELD(temperature_hotspot),	\
	__FIELD(temperature_mem),	\
	__FIELD(temperature_vrgfx),	\
	__FIELD(temperature_vrsoc),	\
	__FIELD(temperature_vrmem),	\
	__FIELD(average_gfx_activity),	\
	__FIELD(average_umc_activity),	\
	__FIELD(average_mm_activity),	\
	__FIELD(average_socket_power),	\
	__FIELD(energy_accumulator),	\
	__FIELD(average_gfxclk_frequency),	\
	__FIELD(average_socclk_frequency),	\
	__FIELD(average_uclk_frequency),	\
	__FIELD(average_vclk0_frequency),	\
	__FIELD(average_dclk0_frequency),	\
	__FIELD(average_vclk1_frequency),	\
	__FIELD(average_dclk1_frequency),	\
	__FIELD(current_gfxclk),	\
	__FIELD(current_socclk),	\
	__FIELD(current_uclk),	\
	__FIELD(current_vclk0),	\
	__FIELD(current_dclk0),	\
	__FIELD(current_vclk1),	\
	__FIELD(current_dclk1),	\
	__FIELD(throttle_status),	\
	__FIELD(current_fan_speed),	\
	__FIELD(pcie_link_width),	\
	__FIELD(pcie_link_speed),

#define METRICS_INFO_V1_1_LIST(__FIELD) \
	__FIELD(temperature_edge),	\
	__FIELD(temperature_hotspot),	\
	__FIELD(temperature_mem),	\
	__FIELD(temperature_vrgfx),	\
	__FIELD(temperature_vrsoc),	\
	__FIELD(temperature_vrmem),	\
	__FIELD(average_gfx_activity),	\
	__FIELD(average_umc_activity),	\
	__FIELD(average_mm_activity),	\
	__FIELD(average_socket_power),	\
	__FIELD(energy_accumulator),	\
	__FIELD(system_clock_counter),	\
	__FIELD(average_gfxclk_frequency),	\
	__FIELD(average_socclk_frequency),	\
	__FIELD(average_uclk_frequency),	\
	__FIELD(average_vclk0_frequency),	\
	__FIELD(average_dclk0_frequency),	\
	__FIELD(average_vclk1_frequency),	\
	__FIELD(average_dclk1_frequency),	\
	__FIELD(current_gfxclk),	\
	__FIELD(current_socclk),	\
	__FIELD(current_uclk),	\
	__FIELD(current_vclk0),	\
	__FIELD(current_dclk0),	\
	__FIELD(current_vclk1),	\
	__FIELD(current_dclk1),	\
	__FIELD(throttle_status),	\
	__FIELD(current_fan_speed),	\
	__FIELD(pcie_link_width),	\
	__FIELD(pcie_link_speed),

#define METRICS_INFO_V1_2_LIST(__FIELD) \
	__FIELD(temperature_edge),	\
	__FIELD(temperature_hotspot),	\
	__FIELD(temperature_mem),	\
	__FIELD(temperature_vrgfx),	\
	__FIELD(temperature_vrsoc),	\
	__FIELD(temperature_vrmem),	\
	__FIELD(average_gfx_activity),	\
	__FIELD(average_umc_activity),	\
	__FIELD(average_mm_activity),	\
	__FIELD(average_socket_power),	\
	__FIELD(energy_accumulator),	\
	__FIELD(system_clock_counter),	\
	__FIELD(average_gfxclk_frequency),	\
	__FIELD(average_socclk_frequency),	\
	__FIELD(average_uclk_frequency),	\
	__FIELD(average_vclk0_frequency),	\
	__FIELD(average_dclk0_frequency),	\
	__FIELD(average_vclk1_frequency),	\
	__FIELD(average_dclk1_frequency),	\
	__FIELD(current_gfxclk),	\
	__FIELD(current_socclk),	\
	__FIELD(current_uclk),	\
	__FIELD(current_vclk0),	\
	__FIELD(current_dclk0),	\
	__FIELD(current_vclk1),	\
	__FIELD(current_dclk1),	\
	__FIELD(throttle_status),	\
	__FIELD(current_fan_speed),	\
	__FIELD(pcie_link_width),	\
	__FIELD(pcie_link_speed),       \
	__FIELD(padding), \
	__FIELD(gfx_activity_acc), \
	__FIELD(mem_activity_acc), \
	__FIELD(temperature_hbm[0]), \
	__FIELD(temperature_hbm[1]), \
	__FIELD(temperature_hbm[2]), \
	__FIELD(temperature_hbm[3]), \
	__FIELD(firmware_timestamp),


#define METRICS_INFO_V1_3_LIST(__FIELD) \
	__FIELD(temperature_edge),	\
	__FIELD(temperature_hotspot),	\
	__FIELD(temperature_mem),	\
	__FIELD(temperature_vrgfx),	\
	__FIELD(temperature_vrsoc),	\
	__FIELD(temperature_vrmem),	\
	__FIELD(average_gfx_activity),	\
	__FIELD(average_umc_activity),	\
	__FIELD(average_mm_activity),	\
	__FIELD(average_socket_power),	\
	__FIELD(energy_accumulator),	\
	__FIELD(system_clock_counter),	\
	__FIELD(average_gfxclk_frequency),	\
	__FIELD(average_socclk_frequency),	\
	__FIELD(average_uclk_frequency),	\
	__FIELD(average_vclk0_frequency),	\
	__FIELD(average_dclk0_frequency),	\
	__FIELD(average_vclk1_frequency),	\
	__FIELD(average_dclk1_frequency),	\
	__FIELD(current_gfxclk),	\
	__FIELD(current_socclk),	\
	__FIELD(current_uclk),	\
	__FIELD(current_vclk0),	\
	__FIELD(current_dclk0),	\
	__FIELD(current_vclk1),	\
	__FIELD(current_dclk1),	\
	__FIELD(throttle_status),	\
	__FIELD(current_fan_speed),	\
	__FIELD(pcie_link_width),	\
	__FIELD(pcie_link_speed),       \
	__FIELD(padding), \
	__FIELD(gfx_activity_acc), \
	__FIELD(mem_activity_acc), \
	__FIELD(temperature_hbm[0]), \
	__FIELD(temperature_hbm[1]), \
	__FIELD(temperature_hbm[2]), \
	__FIELD(temperature_hbm[3]), \
	__FIELD(firmware_timestamp), \
	__FIELD(voltage_soc), \
	__FIELD(voltage_gfx), \
	__FIELD(voltage_mem), \
	__FIELD(padding1), \
	__FIELD(indep_throttle_status),

#define METRICS_INFO_V1_4_LIST(__FIELD) \
	__FIELD(temperature_hotspot),	\
	__FIELD(temperature_mem),	\
	__FIELD(temperature_vrsoc),	\
	__FIELD(curr_socket_power),	\
	__FIELD(average_gfx_activity),	\
	__FIELD(average_umc_activity),	\
	__FIELD(vcn_activity[0]),	\
	__FIELD(vcn_activity[1]),	\
	__FIELD(vcn_activity[2]),	\
	__FIELD(vcn_activity[3]),	\
	__FIELD(energy_accumulator),	\
	__FIELD(system_clock_counter),	\
	__FIELD(throttle_status),	\
	__FIELD(gfxclk_lock_status),	\
	__FIELD(pcie_link_width),	\
	__FIELD(pcie_link_speed),       \
	__FIELD(xgmi_link_width),	\
	__FIELD(xgmi_link_speed),	\
	__FIELD(gfx_activity_acc), \
	__FIELD(mem_activity_acc), \
	__FIELD(pcie_bandwidth_acc),	\
	__FIELD(pcie_bandwidth_inst),	\
	__FIELD(pcie_l0_to_recov_count_acc),	\
	__FIELD(pcie_replay_count_acc),	\
	__FIELD(pcie_replay_rover_count_acc),	\
	__FIELD(xgmi_read_data_acc[0]),	\
	__FIELD(xgmi_read_data_acc[1]),	\
	__FIELD(xgmi_read_data_acc[2]),	\
	__FIELD(xgmi_read_data_acc[3]),	\
	__FIELD(xgmi_read_data_acc[4]),	\
	__FIELD(xgmi_read_data_acc[5]),	\
	__FIELD(xgmi_read_data_acc[6]),	\
	__FIELD(xgmi_read_data_acc[7]),	\
	__FIELD(xgmi_write_data_acc[0]),	\
	__FIELD(xgmi_write_data_acc[1]),	\
	__FIELD(xgmi_write_data_acc[2]),	\
	__FIELD(xgmi_write_data_acc[3]),	\
	__FIELD(xgmi_write_data_acc[4]),	\
	__FIELD(xgmi_write_data_acc[5]),	\
	__FIELD(xgmi_write_data_acc[6]),	\
	__FIELD(xgmi_write_data_acc[7]),	\
	__FIELD(firmware_timestamp),	\
	__FIELD(current_gfxclk[0]),	\
	__FIELD(current_gfxclk[1]),	\
	__FIELD(current_gfxclk[2]),	\
	__FIELD(current_gfxclk[3]),	\
	__FIELD(current_gfxclk[4]),	\
	__FIELD(current_gfxclk[5]),	\
	__FIELD(current_gfxclk[6]),	\
	__FIELD(current_gfxclk[7]),	\
	__FIELD(current_socclk[0]),	\
	__FIELD(current_socclk[1]),	\
	__FIELD(current_socclk[2]),	\
	__FIELD(current_socclk[3]),	\
	__FIELD(current_vclk0[0]),	\
	__FIELD(current_vclk0[1]),	\
	__FIELD(current_vclk0[2]),	\
	__FIELD(current_vclk0[3]),	\
	__FIELD(current_dclk0[0]),	\
	__FIELD(current_dclk0[1]),	\
	__FIELD(current_dclk0[2]),	\
	__FIELD(current_dclk0[3]),	\
	__FIELD(current_uclk),	\
	__FIELD(padding),


#define METRICS_INFO_V1_5_LIST(__FIELD) \
	__FIELD(temperature_hotspot),	\
	__FIELD(temperature_mem),	\
	__FIELD(temperature_vrsoc),	\
	__FIELD(curr_socket_power),	\
	__FIELD(average_gfx_activity),	\
	__FIELD(average_umc_activity),	\
	__FIELD(vcn_activity[0]),	\
	__FIELD(vcn_activity[1]),	\
	__FIELD(vcn_activity[2]),	\
	__FIELD(vcn_activity[3]),	\
	__FIELD(jpeg_activity[ 0]), \
	__FIELD(jpeg_activity[ 1]), \
	__FIELD(jpeg_activity[ 2]), \
	__FIELD(jpeg_activity[ 3]), \
	__FIELD(jpeg_activity[ 4]), \
	__FIELD(jpeg_activity[ 5]), \
	__FIELD(jpeg_activity[ 6]), \
	__FIELD(jpeg_activity[ 7]), \
	__FIELD(jpeg_activity[ 8]), \
	__FIELD(jpeg_activity[ 9]), \
	__FIELD(jpeg_activity[10]), \
	__FIELD(jpeg_activity[11]), \
	__FIELD(jpeg_activity[12]), \
	__FIELD(jpeg_activity[13]), \
	__FIELD(jpeg_activity[14]), \
	__FIELD(jpeg_activity[15]), \
	__FIELD(jpeg_activity[16]), \
	__FIELD(jpeg_activity[17]), \
	__FIELD(jpeg_activity[18]), \
	__FIELD(jpeg_activity[19]), \
	__FIELD(jpeg_activity[20]), \
	__FIELD(jpeg_activity[21]), \
	__FIELD(jpeg_activity[22]), \
	__FIELD(jpeg_activity[23]), \
	__FIELD(jpeg_activity[24]), \
	__FIELD(jpeg_activity[25]), \
	__FIELD(jpeg_activity[26]), \
	__FIELD(jpeg_activity[27]), \
	__FIELD(jpeg_activity[28]), \
	__FIELD(jpeg_activity[29]), \
	__FIELD(jpeg_activity[30]), \
	__FIELD(jpeg_activity[31]), \
	__FIELD(energy_accumulator),	\
	__FIELD(system_clock_counter),	\
	__FIELD(throttle_status),	\
	__FIELD(gfxclk_lock_status),	\
	__FIELD(pcie_link_width),	\
	__FIELD(pcie_link_speed),       \
	__FIELD(xgmi_link_width),	\
	__FIELD(xgmi_link_speed),	\
	__FIELD(gfx_activity_acc), \
	__FIELD(mem_activity_acc), \
	__FIELD(pcie_bandwidth_acc),	\
	__FIELD(pcie_bandwidth_inst),	\
	__FIELD(pcie_l0_to_recov_count_acc),	\
	__FIELD(pcie_replay_count_acc),	\
	__FIELD(pcie_replay_rover_count_acc),	\
	__FIELD(pcie_nak_sent_count_acc),	\
	__FIELD(pcie_nak_rcvd_count_acc),	\
	__FIELD(xgmi_read_data_acc[0]),	\
	__FIELD(xgmi_read_data_acc[1]),	\
	__FIELD(xgmi_read_data_acc[2]),	\
	__FIELD(xgmi_read_data_acc[3]),	\
	__FIELD(xgmi_read_data_acc[4]),	\
	__FIELD(xgmi_read_data_acc[5]),	\
	__FIELD(xgmi_read_data_acc[6]),	\
	__FIELD(xgmi_read_data_acc[7]),	\
	__FIELD(xgmi_write_data_acc[0]),	\
	__FIELD(xgmi_write_data_acc[1]),	\
	__FIELD(xgmi_write_data_acc[2]),	\
	__FIELD(xgmi_write_data_acc[3]),	\
	__FIELD(xgmi_write_data_acc[4]),	\
	__FIELD(xgmi_write_data_acc[5]),	\
	__FIELD(xgmi_write_data_acc[6]),	\
	__FIELD(xgmi_write_data_acc[7]),	\
	__FIELD(firmware_timestamp),	\
	__FIELD(current_gfxclk[0]),	\
	__FIELD(current_gfxclk[1]),	\
	__FIELD(current_gfxclk[2]),	\
	__FIELD(current_gfxclk[3]),	\
	__FIELD(current_gfxclk[4]),	\
	__FIELD(current_gfxclk[5]),	\
	__FIELD(current_gfxclk[6]),	\
	__FIELD(current_gfxclk[7]),	\
	__FIELD(current_socclk[0]),	\
	__FIELD(current_socclk[1]),	\
	__FIELD(current_socclk[2]),	\
	__FIELD(current_socclk[3]),	\
	__FIELD(current_vclk0[0]),	\
	__FIELD(current_vclk0[1]),	\
	__FIELD(current_vclk0[2]),	\
	__FIELD(current_vclk0[3]),	\
	__FIELD(current_dclk0[0]),	\
	__FIELD(current_dclk0[1]),	\
	__FIELD(current_dclk0[2]),	\
	__FIELD(current_dclk0[3]),	\
	__FIELD(current_uclk),	\
	__FIELD(padding),

#define METRICS_INFO_V1_6_LIST(__FIELD) \
	__FIELD(temperature_hotspot),	\
	__FIELD(temperature_mem),	\
	__FIELD(temperature_vrsoc),	\
	__FIELD(curr_socket_power),	\
	__FIELD(average_gfx_activity),	\
	__FIELD(average_umc_activity),	\
	__FIELD(energy_accumulator),	\
	__FIELD(system_clock_counter),	\
	__FIELD(accumulation_counter),	\
	__FIELD(prochot_residency_acc),	\
	__FIELD(ppt_residency_acc),	\
	__FIELD(socket_thm_residency_acc),	\
	__FIELD(vr_thm_residency_acc),	\
	__FIELD(hbm_thm_residency_acc),	\
	__FIELD(gfxclk_lock_status),	\
	__FIELD(pcie_link_width),	\
	__FIELD(pcie_link_speed),       \
	__FIELD(xgmi_link_width),	\
	__FIELD(xgmi_link_speed),	\
	__FIELD(gfx_activity_acc), \
	__FIELD(mem_activity_acc), \
	__FIELD(pcie_bandwidth_acc),	\
	__FIELD(pcie_bandwidth_inst),	\
	__FIELD(pcie_l0_to_recov_count_acc),	\
	__FIELD(pcie_replay_count_acc),	\
	__FIELD(pcie_replay_rover_count_acc),	\
	__FIELD(pcie_nak_sent_count_acc),	\
	__FIELD(pcie_nak_rcvd_count_acc),	\
	__FIELD(xgmi_read_data_acc[0]),	\
	__FIELD(xgmi_read_data_acc[1]),	\
	__FIELD(xgmi_read_data_acc[2]),	\
	__FIELD(xgmi_read_data_acc[3]),	\
	__FIELD(xgmi_read_data_acc[4]),	\
	__FIELD(xgmi_read_data_acc[5]),	\
	__FIELD(xgmi_read_data_acc[6]),	\
	__FIELD(xgmi_read_data_acc[7]),	\
	__FIELD(xgmi_write_data_acc[0]),	\
	__FIELD(xgmi_write_data_acc[1]),	\
	__FIELD(xgmi_write_data_acc[2]),	\
	__FIELD(xgmi_write_data_acc[3]),	\
	__FIELD(xgmi_write_data_acc[4]),	\
	__FIELD(xgmi_write_data_acc[5]),	\
	__FIELD(xgmi_write_data_acc[6]),	\
	__FIELD(xgmi_write_data_acc[7]),	\
	__FIELD(firmware_timestamp),	\
	__FIELD(current_gfxclk[0]),	\
	__FIELD(current_gfxclk[1]),	\
	__FIELD(current_gfxclk[2]),	\
	__FIELD(current_gfxclk[3]),	\
	__FIELD(current_gfxclk[4]),	\
	__FIELD(current_gfxclk[5]),	\
	__FIELD(current_gfxclk[6]),	\
	__FIELD(current_gfxclk[7]),	\
	__FIELD(current_socclk[0]),	\
	__FIELD(current_socclk[1]),	\
	__FIELD(current_socclk[2]),	\
	__FIELD(current_socclk[3]),	\
	__FIELD(current_vclk0[0]),	\
	__FIELD(current_vclk0[1]),	\
	__FIELD(current_vclk0[2]),	\
	__FIELD(current_vclk0[3]),	\
	__FIELD(current_dclk0[0]),	\
	__FIELD(current_dclk0[1]),	\
	__FIELD(current_dclk0[2]),	\
	__FIELD(current_dclk0[3]),	\
	__FIELD(current_uclk),	\
	__FIELD(num_partition),	\
	__FIELD(xcp_stats[0].gfx_busy_inst[0]), \
	__FIELD(xcp_stats[0].gfx_busy_inst[1]), \
	__FIELD(xcp_stats[0].gfx_busy_inst[2]), \
	__FIELD(xcp_stats[0].gfx_busy_inst[3]), \
	__FIELD(xcp_stats[0].gfx_busy_inst[4]), \
	__FIELD(xcp_stats[0].gfx_busy_inst[5]), \
	__FIELD(xcp_stats[0].gfx_busy_inst[6]), \
	__FIELD(xcp_stats[0].gfx_busy_inst[7]), \
	__FIELD(xcp_stats[0].jpeg_busy[0]), \
	__FIELD(xcp_stats[0].jpeg_busy[1]), \
	__FIELD(xcp_stats[0].jpeg_busy[2]), \
	__FIELD(xcp_stats[0].jpeg_busy[3]), \
	__FIELD(xcp_stats[0].jpeg_busy[4]), \
	__FIELD(xcp_stats[0].jpeg_busy[5]), \
	__FIELD(xcp_stats[0].jpeg_busy[6]), \
	__FIELD(xcp_stats[0].jpeg_busy[7]), \
	__FIELD(xcp_stats[0].jpeg_busy[8]), \
	__FIELD(xcp_stats[0].jpeg_busy[9]), \
	__FIELD(xcp_stats[0].jpeg_busy[10]), \
	__FIELD(xcp_stats[0].jpeg_busy[11]), \
	__FIELD(xcp_stats[0].jpeg_busy[12]), \
	__FIELD(xcp_stats[0].jpeg_busy[13]), \
	__FIELD(xcp_stats[0].jpeg_busy[14]), \
	__FIELD(xcp_stats[0].jpeg_busy[15]), \
	__FIELD(xcp_stats[0].jpeg_busy[16]), \
	__FIELD(xcp_stats[0].jpeg_busy[17]), \
	__FIELD(xcp_stats[0].jpeg_busy[18]), \
	__FIELD(xcp_stats[0].jpeg_busy[19]), \
	__FIELD(xcp_stats[0].jpeg_busy[20]), \
	__FIELD(xcp_stats[0].jpeg_busy[21]), \
	__FIELD(xcp_stats[0].jpeg_busy[22]), \
	__FIELD(xcp_stats[0].jpeg_busy[23]), \
	__FIELD(xcp_stats[0].jpeg_busy[24]), \
	__FIELD(xcp_stats[0].jpeg_busy[25]), \
	__FIELD(xcp_stats[0].jpeg_busy[26]), \
	__FIELD(xcp_stats[0].jpeg_busy[27]), \
	__FIELD(xcp_stats[0].jpeg_busy[28]), \
	__FIELD(xcp_stats[0].jpeg_busy[29]), \
	__FIELD(xcp_stats[0].jpeg_busy[30]), \
	__FIELD(xcp_stats[0].jpeg_busy[31]), \
	__FIELD(xcp_stats[0].vcn_busy[0]), \
	__FIELD(xcp_stats[0].vcn_busy[1]), \
	__FIELD(xcp_stats[0].vcn_busy[2]), \
	__FIELD(xcp_stats[0].vcn_busy[3]), \
	__FIELD(xcp_stats[0].gfx_busy_acc[0]), \
	__FIELD(xcp_stats[0].gfx_busy_acc[1]), \
	__FIELD(xcp_stats[0].gfx_busy_acc[2]), \
	__FIELD(xcp_stats[0].gfx_busy_acc[3]), \
	__FIELD(xcp_stats[0].gfx_busy_acc[4]), \
	__FIELD(xcp_stats[0].gfx_busy_acc[5]), \
	__FIELD(xcp_stats[0].gfx_busy_acc[6]), \
	__FIELD(xcp_stats[0].gfx_busy_acc[7]), \
	__FIELD(xcp_stats[1].gfx_busy_inst[0]), \
	__FIELD(xcp_stats[1].gfx_busy_inst[1]), \
	__FIELD(xcp_stats[1].gfx_busy_inst[2]), \
	__FIELD(xcp_stats[1].gfx_busy_inst[3]), \
	__FIELD(xcp_stats[1].gfx_busy_inst[4]), \
	__FIELD(xcp_stats[1].gfx_busy_inst[5]), \
	__FIELD(xcp_stats[1].gfx_busy_inst[6]), \
	__FIELD(xcp_stats[1].gfx_busy_inst[7]), \
	__FIELD(xcp_stats[1].jpeg_busy[0]), \
	__FIELD(xcp_stats[1].jpeg_busy[1]), \
	__FIELD(xcp_stats[1].jpeg_busy[2]), \
	__FIELD(xcp_stats[1].jpeg_busy[3]), \
	__FIELD(xcp_stats[1].jpeg_busy[4]), \
	__FIELD(xcp_stats[1].jpeg_busy[5]), \
	__FIELD(xcp_stats[1].jpeg_busy[6]), \
	__FIELD(xcp_stats[1].jpeg_busy[7]), \
	__FIELD(xcp_stats[1].jpeg_busy[8]), \
	__FIELD(xcp_stats[1].jpeg_busy[9]), \
	__FIELD(xcp_stats[1].jpeg_busy[10]), \
	__FIELD(xcp_stats[1].jpeg_busy[11]), \
	__FIELD(xcp_stats[1].jpeg_busy[12]), \
	__FIELD(xcp_stats[1].jpeg_busy[13]), \
	__FIELD(xcp_stats[1].jpeg_busy[14]), \
	__FIELD(xcp_stats[1].jpeg_busy[15]), \
	__FIELD(xcp_stats[1].jpeg_busy[16]), \
	__FIELD(xcp_stats[1].jpeg_busy[17]), \
	__FIELD(xcp_stats[1].jpeg_busy[18]), \
	__FIELD(xcp_stats[1].jpeg_busy[19]), \
	__FIELD(xcp_stats[1].jpeg_busy[20]), \
	__FIELD(xcp_stats[1].jpeg_busy[21]), \
	__FIELD(xcp_stats[1].jpeg_busy[22]), \
	__FIELD(xcp_stats[1].jpeg_busy[23]), \
	__FIELD(xcp_stats[1].jpeg_busy[24]), \
	__FIELD(xcp_stats[1].jpeg_busy[25]), \
	__FIELD(xcp_stats[1].jpeg_busy[26]), \
	__FIELD(xcp_stats[1].jpeg_busy[27]), \
	__FIELD(xcp_stats[1].jpeg_busy[28]), \
	__FIELD(xcp_stats[1].jpeg_busy[29]), \
	__FIELD(xcp_stats[1].jpeg_busy[30]), \
	__FIELD(xcp_stats[1].jpeg_busy[31]), \
	__FIELD(xcp_stats[1].vcn_busy[0]), \
	__FIELD(xcp_stats[1].vcn_busy[1]), \
	__FIELD(xcp_stats[1].vcn_busy[2]), \
	__FIELD(xcp_stats[1].vcn_busy[3]), \
	__FIELD(xcp_stats[1].gfx_busy_acc[0]), \
	__FIELD(xcp_stats[1].gfx_busy_acc[1]), \
	__FIELD(xcp_stats[1].gfx_busy_acc[2]), \
	__FIELD(xcp_stats[1].gfx_busy_acc[3]), \
	__FIELD(xcp_stats[1].gfx_busy_acc[4]), \
	__FIELD(xcp_stats[1].gfx_busy_acc[5]), \
	__FIELD(xcp_stats[1].gfx_busy_acc[6]), \
	__FIELD(xcp_stats[1].gfx_busy_acc[7]), \
	__FIELD(xcp_stats[2].gfx_busy_inst[0]), \
	__FIELD(xcp_stats[2].gfx_busy_inst[1]), \
	__FIELD(xcp_stats[2].gfx_busy_inst[2]), \
	__FIELD(xcp_stats[2].gfx_busy_inst[3]), \
	__FIELD(xcp_stats[2].gfx_busy_inst[4]), \
	__FIELD(xcp_stats[2].gfx_busy_inst[5]), \
	__FIELD(xcp_stats[2].gfx_busy_inst[6]), \
	__FIELD(xcp_stats[2].gfx_busy_inst[7]), \
	__FIELD(xcp_stats[2].jpeg_busy[0]), \
	__FIELD(xcp_stats[2].jpeg_busy[1]), \
	__FIELD(xcp_stats[2].jpeg_busy[2]), \
	__FIELD(xcp_stats[2].jpeg_busy[3]), \
	__FIELD(xcp_stats[2].jpeg_busy[4]), \
	__FIELD(xcp_stats[2].jpeg_busy[5]), \
	__FIELD(xcp_stats[2].jpeg_busy[6]), \
	__FIELD(xcp_stats[2].jpeg_busy[7]), \
	__FIELD(xcp_stats[2].jpeg_busy[8]), \
	__FIELD(xcp_stats[2].jpeg_busy[9]), \
	__FIELD(xcp_stats[2].jpeg_busy[10]), \
	__FIELD(xcp_stats[2].jpeg_busy[11]), \
	__FIELD(xcp_stats[2].jpeg_busy[12]), \
	__FIELD(xcp_stats[2].jpeg_busy[13]), \
	__FIELD(xcp_stats[2].jpeg_busy[14]), \
	__FIELD(xcp_stats[2].jpeg_busy[15]), \
	__FIELD(xcp_stats[2].jpeg_busy[16]), \
	__FIELD(xcp_stats[2].jpeg_busy[17]), \
	__FIELD(xcp_stats[2].jpeg_busy[18]), \
	__FIELD(xcp_stats[2].jpeg_busy[19]), \
	__FIELD(xcp_stats[2].jpeg_busy[20]), \
	__FIELD(xcp_stats[2].jpeg_busy[21]), \
	__FIELD(xcp_stats[2].jpeg_busy[22]), \
	__FIELD(xcp_stats[2].jpeg_busy[23]), \
	__FIELD(xcp_stats[2].jpeg_busy[24]), \
	__FIELD(xcp_stats[2].jpeg_busy[25]), \
	__FIELD(xcp_stats[2].jpeg_busy[26]), \
	__FIELD(xcp_stats[2].jpeg_busy[27]), \
	__FIELD(xcp_stats[2].jpeg_busy[28]), \
	__FIELD(xcp_stats[2].jpeg_busy[29]), \
	__FIELD(xcp_stats[2].jpeg_busy[30]), \
	__FIELD(xcp_stats[2].jpeg_busy[31]), \
	__FIELD(xcp_stats[2].vcn_busy[0]), \
	__FIELD(xcp_stats[2].vcn_busy[1]), \
	__FIELD(xcp_stats[2].vcn_busy[2]), \
	__FIELD(xcp_stats[2].vcn_busy[3]), \
	__FIELD(xcp_stats[2].gfx_busy_acc[0]), \
	__FIELD(xcp_stats[2].gfx_busy_acc[1]), \
	__FIELD(xcp_stats[2].gfx_busy_acc[2]), \
	__FIELD(xcp_stats[2].gfx_busy_acc[3]), \
	__FIELD(xcp_stats[2].gfx_busy_acc[4]), \
	__FIELD(xcp_stats[2].gfx_busy_acc[5]), \
	__FIELD(xcp_stats[2].gfx_busy_acc[6]), \
	__FIELD(xcp_stats[2].gfx_busy_acc[7]), \
	__FIELD(xcp_stats[3].gfx_busy_inst[0]), \
	__FIELD(xcp_stats[3].gfx_busy_inst[1]), \
	__FIELD(xcp_stats[3].gfx_busy_inst[2]), \
	__FIELD(xcp_stats[3].gfx_busy_inst[3]), \
	__FIELD(xcp_stats[3].gfx_busy_inst[4]), \
	__FIELD(xcp_stats[3].gfx_busy_inst[5]), \
	__FIELD(xcp_stats[3].gfx_busy_inst[6]), \
	__FIELD(xcp_stats[3].gfx_busy_inst[7]), \
	__FIELD(xcp_stats[3].jpeg_busy[0]), \
	__FIELD(xcp_stats[3].jpeg_busy[1]), \
	__FIELD(xcp_stats[3].jpeg_busy[2]), \
	__FIELD(xcp_stats[3].jpeg_busy[3]), \
	__FIELD(xcp_stats[3].jpeg_busy[4]), \
	__FIELD(xcp_stats[3].jpeg_busy[5]), \
	__FIELD(xcp_stats[3].jpeg_busy[6]), \
	__FIELD(xcp_stats[3].jpeg_busy[7]), \
	__FIELD(xcp_stats[3].jpeg_busy[8]), \
	__FIELD(xcp_stats[3].jpeg_busy[9]), \
	__FIELD(xcp_stats[3].jpeg_busy[10]), \
	__FIELD(xcp_stats[3].jpeg_busy[11]), \
	__FIELD(xcp_stats[3].jpeg_busy[12]), \
	__FIELD(xcp_stats[3].jpeg_busy[13]), \
	__FIELD(xcp_stats[3].jpeg_busy[14]), \
	__FIELD(xcp_stats[3].jpeg_busy[15]), \
	__FIELD(xcp_stats[3].jpeg_busy[16]), \
	__FIELD(xcp_stats[3].jpeg_busy[17]), \
	__FIELD(xcp_stats[3].jpeg_busy[18]), \
	__FIELD(xcp_stats[3].jpeg_busy[19]), \
	__FIELD(xcp_stats[3].jpeg_busy[20]), \
	__FIELD(xcp_stats[3].jpeg_busy[21]), \
	__FIELD(xcp_stats[3].jpeg_busy[22]), \
	__FIELD(xcp_stats[3].jpeg_busy[23]), \
	__FIELD(xcp_stats[3].jpeg_busy[24]), \
	__FIELD(xcp_stats[3].jpeg_busy[25]), \
	__FIELD(xcp_stats[3].jpeg_busy[26]), \
	__FIELD(xcp_stats[3].jpeg_busy[27]), \
	__FIELD(xcp_stats[3].jpeg_busy[28]), \
	__FIELD(xcp_stats[3].jpeg_busy[29]), \
	__FIELD(xcp_stats[3].jpeg_busy[30]), \
	__FIELD(xcp_stats[3].jpeg_busy[31]), \
	__FIELD(xcp_stats[3].vcn_busy[0]), \
	__FIELD(xcp_stats[3].vcn_busy[1]), \
	__FIELD(xcp_stats[3].vcn_busy[2]), \
	__FIELD(xcp_stats[3].vcn_busy[3]), \
	__FIELD(xcp_stats[3].gfx_busy_acc[0]), \
	__FIELD(xcp_stats[3].gfx_busy_acc[1]), \
	__FIELD(xcp_stats[3].gfx_busy_acc[2]), \
	__FIELD(xcp_stats[3].gfx_busy_acc[3]), \
	__FIELD(xcp_stats[3].gfx_busy_acc[4]), \
	__FIELD(xcp_stats[3].gfx_busy_acc[5]), \
	__FIELD(xcp_stats[3].gfx_busy_acc[6]), \
	__FIELD(xcp_stats[3].gfx_busy_acc[7]), \
	__FIELD(xcp_stats[4].gfx_busy_inst[0]), \
	__FIELD(xcp_stats[4].gfx_busy_inst[1]), \
	__FIELD(xcp_stats[4].gfx_busy_inst[2]), \
	__FIELD(xcp_stats[4].gfx_busy_inst[3]), \
	__FIELD(xcp_stats[4].gfx_busy_inst[4]), \
	__FIELD(xcp_stats[4].gfx_busy_inst[5]), \
	__FIELD(xcp_stats[4].gfx_busy_inst[6]), \
	__FIELD(xcp_stats[4].gfx_busy_inst[7]), \
	__FIELD(xcp_stats[4].jpeg_busy[0]), \
	__FIELD(xcp_stats[4].jpeg_busy[1]), \
	__FIELD(xcp_stats[4].jpeg_busy[2]), \
	__FIELD(xcp_stats[4].jpeg_busy[3]), \
	__FIELD(xcp_stats[4].jpeg_busy[4]), \
	__FIELD(xcp_stats[4].jpeg_busy[5]), \
	__FIELD(xcp_stats[4].jpeg_busy[6]), \
	__FIELD(xcp_stats[4].jpeg_busy[7]), \
	__FIELD(xcp_stats[4].jpeg_busy[8]), \
	__FIELD(xcp_stats[4].jpeg_busy[9]), \
	__FIELD(xcp_stats[4].jpeg_busy[10]), \
	__FIELD(xcp_stats[4].jpeg_busy[11]), \
	__FIELD(xcp_stats[4].jpeg_busy[12]), \
	__FIELD(xcp_stats[4].jpeg_busy[13]), \
	__FIELD(xcp_stats[4].jpeg_busy[14]), \
	__FIELD(xcp_stats[4].jpeg_busy[15]), \
	__FIELD(xcp_stats[4].jpeg_busy[16]), \
	__FIELD(xcp_stats[4].jpeg_busy[17]), \
	__FIELD(xcp_stats[4].jpeg_busy[18]), \
	__FIELD(xcp_stats[4].jpeg_busy[19]), \
	__FIELD(xcp_stats[4].jpeg_busy[20]), \
	__FIELD(xcp_stats[4].jpeg_busy[21]), \
	__FIELD(xcp_stats[4].jpeg_busy[22]), \
	__FIELD(xcp_stats[4].jpeg_busy[23]), \
	__FIELD(xcp_stats[4].jpeg_busy[24]), \
	__FIELD(xcp_stats[4].jpeg_busy[25]), \
	__FIELD(xcp_stats[4].jpeg_busy[26]), \
	__FIELD(xcp_stats[4].jpeg_busy[27]), \
	__FIELD(xcp_stats[4].jpeg_busy[28]), \
	__FIELD(xcp_stats[4].jpeg_busy[29]), \
	__FIELD(xcp_stats[4].jpeg_busy[30]), \
	__FIELD(xcp_stats[4].jpeg_busy[31]), \
	__FIELD(xcp_stats[4].vcn_busy[0]), \
	__FIELD(xcp_stats[4].vcn_busy[1]), \
	__FIELD(xcp_stats[4].vcn_busy[2]), \
	__FIELD(xcp_stats[4].vcn_busy[3]), \
	__FIELD(xcp_stats[4].gfx_busy_acc[0]), \
	__FIELD(xcp_stats[4].gfx_busy_acc[1]), \
	__FIELD(xcp_stats[4].gfx_busy_acc[2]), \
	__FIELD(xcp_stats[4].gfx_busy_acc[3]), \
	__FIELD(xcp_stats[4].gfx_busy_acc[4]), \
	__FIELD(xcp_stats[4].gfx_busy_acc[5]), \
	__FIELD(xcp_stats[4].gfx_busy_acc[6]), \
	__FIELD(xcp_stats[4].gfx_busy_acc[7]), \
	__FIELD(xcp_stats[5].gfx_busy_inst[0]), \
	__FIELD(xcp_stats[5].gfx_busy_inst[1]), \
	__FIELD(xcp_stats[5].gfx_busy_inst[2]), \
	__FIELD(xcp_stats[5].gfx_busy_inst[3]), \
	__FIELD(xcp_stats[5].gfx_busy_inst[4]), \
	__FIELD(xcp_stats[5].gfx_busy_inst[5]), \
	__FIELD(xcp_stats[5].gfx_busy_inst[6]), \
	__FIELD(xcp_stats[5].gfx_busy_inst[7]), \
	__FIELD(xcp_stats[5].jpeg_busy[0]), \
	__FIELD(xcp_stats[5].jpeg_busy[1]), \
	__FIELD(xcp_stats[5].jpeg_busy[2]), \
	__FIELD(xcp_stats[5].jpeg_busy[3]), \
	__FIELD(xcp_stats[5].jpeg_busy[4]), \
	__FIELD(xcp_stats[5].jpeg_busy[5]), \
	__FIELD(xcp_stats[5].jpeg_busy[6]), \
	__FIELD(xcp_stats[5].jpeg_busy[7]), \
	__FIELD(xcp_stats[5].jpeg_busy[8]), \
	__FIELD(xcp_stats[5].jpeg_busy[9]), \
	__FIELD(xcp_stats[5].jpeg_busy[10]), \
	__FIELD(xcp_stats[5].jpeg_busy[11]), \
	__FIELD(xcp_stats[5].jpeg_busy[12]), \
	__FIELD(xcp_stats[5].jpeg_busy[13]), \
	__FIELD(xcp_stats[5].jpeg_busy[14]), \
	__FIELD(xcp_stats[5].jpeg_busy[15]), \
	__FIELD(xcp_stats[5].jpeg_busy[16]), \
	__FIELD(xcp_stats[5].jpeg_busy[17]), \
	__FIELD(xcp_stats[5].jpeg_busy[18]), \
	__FIELD(xcp_stats[5].jpeg_busy[19]), \
	__FIELD(xcp_stats[5].jpeg_busy[20]), \
	__FIELD(xcp_stats[5].jpeg_busy[21]), \
	__FIELD(xcp_stats[5].jpeg_busy[22]), \
	__FIELD(xcp_stats[5].jpeg_busy[23]), \
	__FIELD(xcp_stats[5].jpeg_busy[24]), \
	__FIELD(xcp_stats[5].jpeg_busy[25]), \
	__FIELD(xcp_stats[5].jpeg_busy[26]), \
	__FIELD(xcp_stats[5].jpeg_busy[27]), \
	__FIELD(xcp_stats[5].jpeg_busy[28]), \
	__FIELD(xcp_stats[5].jpeg_busy[29]), \
	__FIELD(xcp_stats[5].jpeg_busy[30]), \
	__FIELD(xcp_stats[5].jpeg_busy[31]), \
	__FIELD(xcp_stats[5].vcn_busy[0]), \
	__FIELD(xcp_stats[5].vcn_busy[1]), \
	__FIELD(xcp_stats[5].vcn_busy[2]), \
	__FIELD(xcp_stats[5].vcn_busy[3]), \
	__FIELD(xcp_stats[5].gfx_busy_acc[0]), \
	__FIELD(xcp_stats[5].gfx_busy_acc[1]), \
	__FIELD(xcp_stats[5].gfx_busy_acc[2]), \
	__FIELD(xcp_stats[5].gfx_busy_acc[3]), \
	__FIELD(xcp_stats[5].gfx_busy_acc[4]), \
	__FIELD(xcp_stats[5].gfx_busy_acc[5]), \
	__FIELD(xcp_stats[5].gfx_busy_acc[6]), \
	__FIELD(xcp_stats[5].gfx_busy_acc[7]), \
	__FIELD(xcp_stats[6].gfx_busy_inst[0]), \
	__FIELD(xcp_stats[6].gfx_busy_inst[1]), \
	__FIELD(xcp_stats[6].gfx_busy_inst[2]), \
	__FIELD(xcp_stats[6].gfx_busy_inst[3]), \
	__FIELD(xcp_stats[6].gfx_busy_inst[4]), \
	__FIELD(xcp_stats[6].gfx_busy_inst[5]), \
	__FIELD(xcp_stats[6].gfx_busy_inst[6]), \
	__FIELD(xcp_stats[6].gfx_busy_inst[7]), \
	__FIELD(xcp_stats[6].jpeg_busy[0]), \
	__FIELD(xcp_stats[6].jpeg_busy[1]), \
	__FIELD(xcp_stats[6].jpeg_busy[2]), \
	__FIELD(xcp_stats[6].jpeg_busy[3]), \
	__FIELD(xcp_stats[6].jpeg_busy[4]), \
	__FIELD(xcp_stats[6].jpeg_busy[5]), \
	__FIELD(xcp_stats[6].jpeg_busy[6]), \
	__FIELD(xcp_stats[6].jpeg_busy[7]), \
	__FIELD(xcp_stats[6].jpeg_busy[8]), \
	__FIELD(xcp_stats[6].jpeg_busy[9]), \
	__FIELD(xcp_stats[6].jpeg_busy[10]), \
	__FIELD(xcp_stats[6].jpeg_busy[11]), \
	__FIELD(xcp_stats[6].jpeg_busy[12]), \
	__FIELD(xcp_stats[6].jpeg_busy[13]), \
	__FIELD(xcp_stats[6].jpeg_busy[14]), \
	__FIELD(xcp_stats[6].jpeg_busy[15]), \
	__FIELD(xcp_stats[6].jpeg_busy[16]), \
	__FIELD(xcp_stats[6].jpeg_busy[17]), \
	__FIELD(xcp_stats[6].jpeg_busy[18]), \
	__FIELD(xcp_stats[6].jpeg_busy[19]), \
	__FIELD(xcp_stats[6].jpeg_busy[20]), \
	__FIELD(xcp_stats[6].jpeg_busy[21]), \
	__FIELD(xcp_stats[6].jpeg_busy[22]), \
	__FIELD(xcp_stats[6].jpeg_busy[23]), \
	__FIELD(xcp_stats[6].jpeg_busy[24]), \
	__FIELD(xcp_stats[6].jpeg_busy[25]), \
	__FIELD(xcp_stats[6].jpeg_busy[26]), \
	__FIELD(xcp_stats[6].jpeg_busy[27]), \
	__FIELD(xcp_stats[6].jpeg_busy[28]), \
	__FIELD(xcp_stats[6].jpeg_busy[29]), \
	__FIELD(xcp_stats[6].jpeg_busy[30]), \
	__FIELD(xcp_stats[6].jpeg_busy[31]), \
	__FIELD(xcp_stats[6].vcn_busy[0]), \
	__FIELD(xcp_stats[6].vcn_busy[1]), \
	__FIELD(xcp_stats[6].vcn_busy[2]), \
	__FIELD(xcp_stats[6].vcn_busy[3]), \
	__FIELD(xcp_stats[6].gfx_busy_acc[0]), \
	__FIELD(xcp_stats[6].gfx_busy_acc[1]), \
	__FIELD(xcp_stats[6].gfx_busy_acc[2]), \
	__FIELD(xcp_stats[6].gfx_busy_acc[3]), \
	__FIELD(xcp_stats[6].gfx_busy_acc[4]), \
	__FIELD(xcp_stats[6].gfx_busy_acc[5]), \
	__FIELD(xcp_stats[6].gfx_busy_acc[6]), \
	__FIELD(xcp_stats[6].gfx_busy_acc[7]), \
	__FIELD(xcp_stats[7].gfx_busy_inst[0]), \
	__FIELD(xcp_stats[7].gfx_busy_inst[1]), \
	__FIELD(xcp_stats[7].gfx_busy_inst[2]), \
	__FIELD(xcp_stats[7].gfx_busy_inst[3]), \
	__FIELD(xcp_stats[7].gfx_busy_inst[4]), \
	__FIELD(xcp_stats[7].gfx_busy_inst[5]), \
	__FIELD(xcp_stats[7].gfx_busy_inst[6]), \
	__FIELD(xcp_stats[7].gfx_busy_inst[7]), \
	__FIELD(xcp_stats[7].jpeg_busy[0]), \
	__FIELD(xcp_stats[7].jpeg_busy[1]), \
	__FIELD(xcp_stats[7].jpeg_busy[2]), \
	__FIELD(xcp_stats[7].jpeg_busy[3]), \
	__FIELD(xcp_stats[7].jpeg_busy[4]), \
	__FIELD(xcp_stats[7].jpeg_busy[5]), \
	__FIELD(xcp_stats[7].jpeg_busy[6]), \
	__FIELD(xcp_stats[7].jpeg_busy[7]), \
	__FIELD(xcp_stats[7].jpeg_busy[8]), \
	__FIELD(xcp_stats[7].jpeg_busy[9]), \
	__FIELD(xcp_stats[7].jpeg_busy[10]), \
	__FIELD(xcp_stats[7].jpeg_busy[11]), \
	__FIELD(xcp_stats[7].jpeg_busy[12]), \
	__FIELD(xcp_stats[7].jpeg_busy[13]), \
	__FIELD(xcp_stats[7].jpeg_busy[14]), \
	__FIELD(xcp_stats[7].jpeg_busy[15]), \
	__FIELD(xcp_stats[7].jpeg_busy[16]), \
	__FIELD(xcp_stats[7].jpeg_busy[17]), \
	__FIELD(xcp_stats[7].jpeg_busy[18]), \
	__FIELD(xcp_stats[7].jpeg_busy[19]), \
	__FIELD(xcp_stats[7].jpeg_busy[20]), \
	__FIELD(xcp_stats[7].jpeg_busy[21]), \
	__FIELD(xcp_stats[7].jpeg_busy[22]), \
	__FIELD(xcp_stats[7].jpeg_busy[23]), \
	__FIELD(xcp_stats[7].jpeg_busy[24]), \
	__FIELD(xcp_stats[7].jpeg_busy[25]), \
	__FIELD(xcp_stats[7].jpeg_busy[26]), \
	__FIELD(xcp_stats[7].jpeg_busy[27]), \
	__FIELD(xcp_stats[7].jpeg_busy[28]), \
	__FIELD(xcp_stats[7].jpeg_busy[29]), \
	__FIELD(xcp_stats[7].jpeg_busy[30]), \
	__FIELD(xcp_stats[7].jpeg_busy[31]), \
	__FIELD(xcp_stats[7].vcn_busy[0]), \
	__FIELD(xcp_stats[7].vcn_busy[1]), \
	__FIELD(xcp_stats[7].vcn_busy[2]), \
	__FIELD(xcp_stats[7].vcn_busy[3]), \
	__FIELD(xcp_stats[7].gfx_busy_acc[0]), \
	__FIELD(xcp_stats[7].gfx_busy_acc[1]), \
	__FIELD(xcp_stats[7].gfx_busy_acc[2]), \
	__FIELD(xcp_stats[7].gfx_busy_acc[3]), \
	__FIELD(xcp_stats[7].gfx_busy_acc[4]), \
	__FIELD(xcp_stats[7].gfx_busy_acc[5]), \
	__FIELD(xcp_stats[7].gfx_busy_acc[6]), \
	__FIELD(xcp_stats[7].gfx_busy_acc[7]), \
	__FIELD(pcie_lc_perf_other_end_recovery),

#define METRICS_INFO_V1_7_LIST(__FIELD) \
	__FIELD(temperature_hotspot),	\
	__FIELD(temperature_mem),	\
	__FIELD(temperature_vrsoc),	\
	__FIELD(curr_socket_power),	\
	__FIELD(average_gfx_activity),	\
	__FIELD(average_umc_activity),	\
	__FIELD(mem_max_bandwidth), \
	__FIELD(energy_accumulator),	\
	__FIELD(system_clock_counter),	\
	__FIELD(accumulation_counter),	\
	__FIELD(prochot_residency_acc),	\
	__FIELD(ppt_residency_acc),	\
	__FIELD(socket_thm_residency_acc),	\
	__FIELD(vr_thm_residency_acc),	\
	__FIELD(hbm_thm_residency_acc),	\
	__FIELD(gfxclk_lock_status),	\
	__FIELD(pcie_link_width),	\
	__FIELD(pcie_link_speed),       \
	__FIELD(xgmi_link_width),	\
	__FIELD(xgmi_link_speed),	\
	__FIELD(gfx_activity_acc), \
	__FIELD(mem_activity_acc), \
	__FIELD(pcie_bandwidth_acc),	\
	__FIELD(pcie_bandwidth_inst),	\
	__FIELD(pcie_l0_to_recov_count_acc),	\
	__FIELD(pcie_replay_count_acc),	\
	__FIELD(pcie_replay_rover_count_acc),	\
	__FIELD(pcie_nak_sent_count_acc),	\
	__FIELD(pcie_nak_rcvd_count_acc),	\
	__FIELD(xgmi_read_data_acc[0]),	\
	__FIELD(xgmi_read_data_acc[1]),	\
	__FIELD(xgmi_read_data_acc[2]),	\
	__FIELD(xgmi_read_data_acc[3]),	\
	__FIELD(xgmi_read_data_acc[4]),	\
	__FIELD(xgmi_read_data_acc[5]),	\
	__FIELD(xgmi_read_data_acc[6]),	\
	__FIELD(xgmi_read_data_acc[7]),	\
	__FIELD(xgmi_write_data_acc[0]),	\
	__FIELD(xgmi_write_data_acc[1]),	\
	__FIELD(xgmi_write_data_acc[2]),	\
	__FIELD(xgmi_write_data_acc[3]),	\
	__FIELD(xgmi_write_data_acc[4]),	\
	__FIELD(xgmi_write_data_acc[5]),	\
	__FIELD(xgmi_write_data_acc[6]),	\
	__FIELD(xgmi_write_data_acc[7]),	\
	__FIELD(xgmi_link_status[0]), \
	__FIELD(xgmi_link_status[1]), \
	__FIELD(xgmi_link_status[2]), \
	__FIELD(xgmi_link_status[3]), \
	__FIELD(xgmi_link_status[4]), \
	__FIELD(xgmi_link_status[5]), \
	__FIELD(xgmi_link_status[6]), \
	__FIELD(xgmi_link_status[7]), \
	__FIELD(padding),	\
	__FIELD(firmware_timestamp),	\
	__FIELD(current_gfxclk[0]),	\
	__FIELD(current_gfxclk[1]),	\
	__FIELD(current_gfxclk[2]),	\
	__FIELD(current_gfxclk[3]),	\
	__FIELD(current_gfxclk[4]),	\
	__FIELD(current_gfxclk[5]),	\
	__FIELD(current_gfxclk[6]),	\
	__FIELD(current_gfxclk[7]),	\
	__FIELD(current_socclk[0]),	\
	__FIELD(current_socclk[1]),	\
	__FIELD(current_socclk[2]),	\
	__FIELD(current_socclk[3]),	\
	__FIELD(current_vclk0[0]),	\
	__FIELD(current_vclk0[1]),	\
	__FIELD(current_vclk0[2]),	\
	__FIELD(current_vclk0[3]),	\
	__FIELD(current_dclk0[0]),	\
	__FIELD(current_dclk0[1]),	\
	__FIELD(current_dclk0[2]),	\
	__FIELD(current_dclk0[3]),	\
	__FIELD(current_uclk),	\
	__FIELD(num_partition),	\
	__FIELD(xcp_stats[0].gfx_busy_inst[0]), \
	__FIELD(xcp_stats[0].gfx_busy_inst[1]), \
	__FIELD(xcp_stats[0].gfx_busy_inst[2]), \
	__FIELD(xcp_stats[0].gfx_busy_inst[3]), \
	__FIELD(xcp_stats[0].gfx_busy_inst[4]), \
	__FIELD(xcp_stats[0].gfx_busy_inst[5]), \
	__FIELD(xcp_stats[0].gfx_busy_inst[6]), \
	__FIELD(xcp_stats[0].gfx_busy_inst[7]), \
	__FIELD(xcp_stats[0].jpeg_busy[0]), \
	__FIELD(xcp_stats[0].jpeg_busy[1]), \
	__FIELD(xcp_stats[0].jpeg_busy[2]), \
	__FIELD(xcp_stats[0].jpeg_busy[3]), \
	__FIELD(xcp_stats[0].jpeg_busy[4]), \
	__FIELD(xcp_stats[0].jpeg_busy[5]), \
	__FIELD(xcp_stats[0].jpeg_busy[6]), \
	__FIELD(xcp_stats[0].jpeg_busy[7]), \
	__FIELD(xcp_stats[0].jpeg_busy[8]), \
	__FIELD(xcp_stats[0].jpeg_busy[9]), \
	__FIELD(xcp_stats[0].jpeg_busy[10]), \
	__FIELD(xcp_stats[0].jpeg_busy[11]), \
	__FIELD(xcp_stats[0].jpeg_busy[12]), \
	__FIELD(xcp_stats[0].jpeg_busy[13]), \
	__FIELD(xcp_stats[0].jpeg_busy[14]), \
	__FIELD(xcp_stats[0].jpeg_busy[15]), \
	__FIELD(xcp_stats[0].jpeg_busy[16]), \
	__FIELD(xcp_stats[0].jpeg_busy[17]), \
	__FIELD(xcp_stats[0].jpeg_busy[18]), \
	__FIELD(xcp_stats[0].jpeg_busy[19]), \
	__FIELD(xcp_stats[0].jpeg_busy[20]), \
	__FIELD(xcp_stats[0].jpeg_busy[21]), \
	__FIELD(xcp_stats[0].jpeg_busy[22]), \
	__FIELD(xcp_stats[0].jpeg_busy[23]), \
	__FIELD(xcp_stats[0].jpeg_busy[24]), \
	__FIELD(xcp_stats[0].jpeg_busy[25]), \
	__FIELD(xcp_stats[0].jpeg_busy[26]), \
	__FIELD(xcp_stats[0].jpeg_busy[27]), \
	__FIELD(xcp_stats[0].jpeg_busy[28]), \
	__FIELD(xcp_stats[0].jpeg_busy[29]), \
	__FIELD(xcp_stats[0].jpeg_busy[30]), \
	__FIELD(xcp_stats[0].jpeg_busy[31]), \
	__FIELD(xcp_stats[0].vcn_busy[0]), \
	__FIELD(xcp_stats[0].vcn_busy[1]), \
	__FIELD(xcp_stats[0].vcn_busy[2]), \
	__FIELD(xcp_stats[0].vcn_busy[3]), \
	__FIELD(xcp_stats[0].gfx_busy_acc[0]), \
	__FIELD(xcp_stats[0].gfx_busy_acc[1]), \
	__FIELD(xcp_stats[0].gfx_busy_acc[2]), \
	__FIELD(xcp_stats[0].gfx_busy_acc[3]), \
	__FIELD(xcp_stats[0].gfx_busy_acc[4]), \
	__FIELD(xcp_stats[0].gfx_busy_acc[5]), \
	__FIELD(xcp_stats[0].gfx_busy_acc[6]), \
	__FIELD(xcp_stats[0].gfx_busy_acc[7]), \
	__FIELD(xcp_stats[0].gfx_below_host_limit_acc[0]), \
	__FIELD(xcp_stats[0].gfx_below_host_limit_acc[1]), \
	__FIELD(xcp_stats[0].gfx_below_host_limit_acc[2]), \
	__FIELD(xcp_stats[0].gfx_below_host_limit_acc[3]), \
	__FIELD(xcp_stats[0].gfx_below_host_limit_acc[4]), \
	__FIELD(xcp_stats[0].gfx_below_host_limit_acc[5]), \
	__FIELD(xcp_stats[0].gfx_below_host_limit_acc[6]), \
	__FIELD(xcp_stats[0].gfx_below_host_limit_acc[7]), \
	__FIELD(xcp_stats[1].gfx_busy_inst[0]), \
	__FIELD(xcp_stats[1].gfx_busy_inst[1]), \
	__FIELD(xcp_stats[1].gfx_busy_inst[2]), \
	__FIELD(xcp_stats[1].gfx_busy_inst[3]), \
	__FIELD(xcp_stats[1].gfx_busy_inst[4]), \
	__FIELD(xcp_stats[1].gfx_busy_inst[5]), \
	__FIELD(xcp_stats[1].gfx_busy_inst[6]), \
	__FIELD(xcp_stats[1].gfx_busy_inst[7]), \
	__FIELD(xcp_stats[1].jpeg_busy[0]), \
	__FIELD(xcp_stats[1].jpeg_busy[1]), \
	__FIELD(xcp_stats[1].jpeg_busy[2]), \
	__FIELD(xcp_stats[1].jpeg_busy[3]), \
	__FIELD(xcp_stats[1].jpeg_busy[4]), \
	__FIELD(xcp_stats[1].jpeg_busy[5]), \
	__FIELD(xcp_stats[1].jpeg_busy[6]), \
	__FIELD(xcp_stats[1].jpeg_busy[7]), \
	__FIELD(xcp_stats[1].jpeg_busy[8]), \
	__FIELD(xcp_stats[1].jpeg_busy[9]), \
	__FIELD(xcp_stats[1].jpeg_busy[10]), \
	__FIELD(xcp_stats[1].jpeg_busy[11]), \
	__FIELD(xcp_stats[1].jpeg_busy[12]), \
	__FIELD(xcp_stats[1].jpeg_busy[13]), \
	__FIELD(xcp_stats[1].jpeg_busy[14]), \
	__FIELD(xcp_stats[1].jpeg_busy[15]), \
	__FIELD(xcp_stats[1].jpeg_busy[16]), \
	__FIELD(xcp_stats[1].jpeg_busy[17]), \
	__FIELD(xcp_stats[1].jpeg_busy[18]), \
	__FIELD(xcp_stats[1].jpeg_busy[19]), \
	__FIELD(xcp_stats[1].jpeg_busy[20]), \
	__FIELD(xcp_stats[1].jpeg_busy[21]), \
	__FIELD(xcp_stats[1].jpeg_busy[22]), \
	__FIELD(xcp_stats[1].jpeg_busy[23]), \
	__FIELD(xcp_stats[1].jpeg_busy[24]), \
	__FIELD(xcp_stats[1].jpeg_busy[25]), \
	__FIELD(xcp_stats[1].jpeg_busy[26]), \
	__FIELD(xcp_stats[1].jpeg_busy[27]), \
	__FIELD(xcp_stats[1].jpeg_busy[28]), \
	__FIELD(xcp_stats[1].jpeg_busy[29]), \
	__FIELD(xcp_stats[1].jpeg_busy[30]), \
	__FIELD(xcp_stats[1].jpeg_busy[31]), \
	__FIELD(xcp_stats[1].vcn_busy[0]), \
	__FIELD(xcp_stats[1].vcn_busy[1]), \
	__FIELD(xcp_stats[1].vcn_busy[2]), \
	__FIELD(xcp_stats[1].vcn_busy[3]), \
	__FIELD(xcp_stats[1].gfx_busy_acc[0]), \
	__FIELD(xcp_stats[1].gfx_busy_acc[1]), \
	__FIELD(xcp_stats[1].gfx_busy_acc[2]), \
	__FIELD(xcp_stats[1].gfx_busy_acc[3]), \
	__FIELD(xcp_stats[1].gfx_busy_acc[4]), \
	__FIELD(xcp_stats[1].gfx_busy_acc[5]), \
	__FIELD(xcp_stats[1].gfx_busy_acc[6]), \
	__FIELD(xcp_stats[1].gfx_busy_acc[7]), \
	__FIELD(xcp_stats[1].gfx_below_host_limit_acc[0]), \
	__FIELD(xcp_stats[1].gfx_below_host_limit_acc[1]), \
	__FIELD(xcp_stats[1].gfx_below_host_limit_acc[2]), \
	__FIELD(xcp_stats[1].gfx_below_host_limit_acc[3]), \
	__FIELD(xcp_stats[1].gfx_below_host_limit_acc[4]), \
	__FIELD(xcp_stats[1].gfx_below_host_limit_acc[5]), \
	__FIELD(xcp_stats[1].gfx_below_host_limit_acc[6]), \
	__FIELD(xcp_stats[1].gfx_below_host_limit_acc[7]), \
	__FIELD(xcp_stats[2].gfx_busy_inst[0]), \
	__FIELD(xcp_stats[2].gfx_busy_inst[1]), \
	__FIELD(xcp_stats[2].gfx_busy_inst[2]), \
	__FIELD(xcp_stats[2].gfx_busy_inst[3]), \
	__FIELD(xcp_stats[2].gfx_busy_inst[4]), \
	__FIELD(xcp_stats[2].gfx_busy_inst[5]), \
	__FIELD(xcp_stats[2].gfx_busy_inst[6]), \
	__FIELD(xcp_stats[2].gfx_busy_inst[7]), \
	__FIELD(xcp_stats[2].jpeg_busy[0]), \
	__FIELD(xcp_stats[2].jpeg_busy[1]), \
	__FIELD(xcp_stats[2].jpeg_busy[2]), \
	__FIELD(xcp_stats[2].jpeg_busy[3]), \
	__FIELD(xcp_stats[2].jpeg_busy[4]), \
	__FIELD(xcp_stats[2].jpeg_busy[5]), \
	__FIELD(xcp_stats[2].jpeg_busy[6]), \
	__FIELD(xcp_stats[2].jpeg_busy[7]), \
	__FIELD(xcp_stats[2].jpeg_busy[8]), \
	__FIELD(xcp_stats[2].jpeg_busy[9]), \
	__FIELD(xcp_stats[2].jpeg_busy[10]), \
	__FIELD(xcp_stats[2].jpeg_busy[11]), \
	__FIELD(xcp_stats[2].jpeg_busy[12]), \
	__FIELD(xcp_stats[2].jpeg_busy[13]), \
	__FIELD(xcp_stats[2].jpeg_busy[14]), \
	__FIELD(xcp_stats[2].jpeg_busy[15]), \
	__FIELD(xcp_stats[2].jpeg_busy[16]), \
	__FIELD(xcp_stats[2].jpeg_busy[17]), \
	__FIELD(xcp_stats[2].jpeg_busy[18]), \
	__FIELD(xcp_stats[2].jpeg_busy[19]), \
	__FIELD(xcp_stats[2].jpeg_busy[20]), \
	__FIELD(xcp_stats[2].jpeg_busy[21]), \
	__FIELD(xcp_stats[2].jpeg_busy[22]), \
	__FIELD(xcp_stats[2].jpeg_busy[23]), \
	__FIELD(xcp_stats[2].jpeg_busy[24]), \
	__FIELD(xcp_stats[2].jpeg_busy[25]), \
	__FIELD(xcp_stats[2].jpeg_busy[26]), \
	__FIELD(xcp_stats[2].jpeg_busy[27]), \
	__FIELD(xcp_stats[2].jpeg_busy[28]), \
	__FIELD(xcp_stats[2].jpeg_busy[29]), \
	__FIELD(xcp_stats[2].jpeg_busy[30]), \
	__FIELD(xcp_stats[2].jpeg_busy[31]), \
	__FIELD(xcp_stats[2].vcn_busy[0]), \
	__FIELD(xcp_stats[2].vcn_busy[1]), \
	__FIELD(xcp_stats[2].vcn_busy[2]), \
	__FIELD(xcp_stats[2].vcn_busy[3]), \
	__FIELD(xcp_stats[2].gfx_busy_acc[0]), \
	__FIELD(xcp_stats[2].gfx_busy_acc[1]), \
	__FIELD(xcp_stats[2].gfx_busy_acc[2]), \
	__FIELD(xcp_stats[2].gfx_busy_acc[3]), \
	__FIELD(xcp_stats[2].gfx_busy_acc[4]), \
	__FIELD(xcp_stats[2].gfx_busy_acc[5]), \
	__FIELD(xcp_stats[2].gfx_busy_acc[6]), \
	__FIELD(xcp_stats[2].gfx_busy_acc[7]), \
	__FIELD(xcp_stats[2].gfx_below_host_limit_acc[0]), \
	__FIELD(xcp_stats[2].gfx_below_host_limit_acc[1]), \
	__FIELD(xcp_stats[2].gfx_below_host_limit_acc[2]), \
	__FIELD(xcp_stats[2].gfx_below_host_limit_acc[3]), \
	__FIELD(xcp_stats[2].gfx_below_host_limit_acc[4]), \
	__FIELD(xcp_stats[2].gfx_below_host_limit_acc[5]), \
	__FIELD(xcp_stats[2].gfx_below_host_limit_acc[6]), \
	__FIELD(xcp_stats[2].gfx_below_host_limit_acc[7]), \
	__FIELD(xcp_stats[3].gfx_busy_inst[0]), \
	__FIELD(xcp_stats[3].gfx_busy_inst[1]), \
	__FIELD(xcp_stats[3].gfx_busy_inst[2]), \
	__FIELD(xcp_stats[3].gfx_busy_inst[3]), \
	__FIELD(xcp_stats[3].gfx_busy_inst[4]), \
	__FIELD(xcp_stats[3].gfx_busy_inst[5]), \
	__FIELD(xcp_stats[3].gfx_busy_inst[6]), \
	__FIELD(xcp_stats[3].gfx_busy_inst[7]), \
	__FIELD(xcp_stats[3].jpeg_busy[0]), \
	__FIELD(xcp_stats[3].jpeg_busy[1]), \
	__FIELD(xcp_stats[3].jpeg_busy[2]), \
	__FIELD(xcp_stats[3].jpeg_busy[3]), \
	__FIELD(xcp_stats[3].jpeg_busy[4]), \
	__FIELD(xcp_stats[3].jpeg_busy[5]), \
	__FIELD(xcp_stats[3].jpeg_busy[6]), \
	__FIELD(xcp_stats[3].jpeg_busy[7]), \
	__FIELD(xcp_stats[3].jpeg_busy[8]), \
	__FIELD(xcp_stats[3].jpeg_busy[9]), \
	__FIELD(xcp_stats[3].jpeg_busy[10]), \
	__FIELD(xcp_stats[3].jpeg_busy[11]), \
	__FIELD(xcp_stats[3].jpeg_busy[12]), \
	__FIELD(xcp_stats[3].jpeg_busy[13]), \
	__FIELD(xcp_stats[3].jpeg_busy[14]), \
	__FIELD(xcp_stats[3].jpeg_busy[15]), \
	__FIELD(xcp_stats[3].jpeg_busy[16]), \
	__FIELD(xcp_stats[3].jpeg_busy[17]), \
	__FIELD(xcp_stats[3].jpeg_busy[18]), \
	__FIELD(xcp_stats[3].jpeg_busy[19]), \
	__FIELD(xcp_stats[3].jpeg_busy[20]), \
	__FIELD(xcp_stats[3].jpeg_busy[21]), \
	__FIELD(xcp_stats[3].jpeg_busy[22]), \
	__FIELD(xcp_stats[3].jpeg_busy[23]), \
	__FIELD(xcp_stats[3].jpeg_busy[24]), \
	__FIELD(xcp_stats[3].jpeg_busy[25]), \
	__FIELD(xcp_stats[3].jpeg_busy[26]), \
	__FIELD(xcp_stats[3].jpeg_busy[27]), \
	__FIELD(xcp_stats[3].jpeg_busy[28]), \
	__FIELD(xcp_stats[3].jpeg_busy[29]), \
	__FIELD(xcp_stats[3].jpeg_busy[30]), \
	__FIELD(xcp_stats[3].jpeg_busy[31]), \
	__FIELD(xcp_stats[3].vcn_busy[0]), \
	__FIELD(xcp_stats[3].vcn_busy[1]), \
	__FIELD(xcp_stats[3].vcn_busy[2]), \
	__FIELD(xcp_stats[3].vcn_busy[3]), \
	__FIELD(xcp_stats[3].gfx_busy_acc[0]), \
	__FIELD(xcp_stats[3].gfx_busy_acc[1]), \
	__FIELD(xcp_stats[3].gfx_busy_acc[2]), \
	__FIELD(xcp_stats[3].gfx_busy_acc[3]), \
	__FIELD(xcp_stats[3].gfx_busy_acc[4]), \
	__FIELD(xcp_stats[3].gfx_busy_acc[5]), \
	__FIELD(xcp_stats[3].gfx_busy_acc[6]), \
	__FIELD(xcp_stats[3].gfx_busy_acc[7]), \
	__FIELD(xcp_stats[3].gfx_below_host_limit_acc[0]), \
	__FIELD(xcp_stats[3].gfx_below_host_limit_acc[1]), \
	__FIELD(xcp_stats[3].gfx_below_host_limit_acc[2]), \
	__FIELD(xcp_stats[3].gfx_below_host_limit_acc[3]), \
	__FIELD(xcp_stats[3].gfx_below_host_limit_acc[4]), \
	__FIELD(xcp_stats[3].gfx_below_host_limit_acc[5]), \
	__FIELD(xcp_stats[3].gfx_below_host_limit_acc[6]), \
	__FIELD(xcp_stats[3].gfx_below_host_limit_acc[7]), \
	__FIELD(xcp_stats[4].gfx_busy_inst[0]), \
	__FIELD(xcp_stats[4].gfx_busy_inst[1]), \
	__FIELD(xcp_stats[4].gfx_busy_inst[2]), \
	__FIELD(xcp_stats[4].gfx_busy_inst[3]), \
	__FIELD(xcp_stats[4].gfx_busy_inst[4]), \
	__FIELD(xcp_stats[4].gfx_busy_inst[5]), \
	__FIELD(xcp_stats[4].gfx_busy_inst[6]), \
	__FIELD(xcp_stats[4].gfx_busy_inst[7]), \
	__FIELD(xcp_stats[4].jpeg_busy[0]), \
	__FIELD(xcp_stats[4].jpeg_busy[1]), \
	__FIELD(xcp_stats[4].jpeg_busy[2]), \
	__FIELD(xcp_stats[4].jpeg_busy[3]), \
	__FIELD(xcp_stats[4].jpeg_busy[4]), \
	__FIELD(xcp_stats[4].jpeg_busy[5]), \
	__FIELD(xcp_stats[4].jpeg_busy[6]), \
	__FIELD(xcp_stats[4].jpeg_busy[7]), \
	__FIELD(xcp_stats[4].jpeg_busy[8]), \
	__FIELD(xcp_stats[4].jpeg_busy[9]), \
	__FIELD(xcp_stats[4].jpeg_busy[10]), \
	__FIELD(xcp_stats[4].jpeg_busy[11]), \
	__FIELD(xcp_stats[4].jpeg_busy[12]), \
	__FIELD(xcp_stats[4].jpeg_busy[13]), \
	__FIELD(xcp_stats[4].jpeg_busy[14]), \
	__FIELD(xcp_stats[4].jpeg_busy[15]), \
	__FIELD(xcp_stats[4].jpeg_busy[16]), \
	__FIELD(xcp_stats[4].jpeg_busy[17]), \
	__FIELD(xcp_stats[4].jpeg_busy[18]), \
	__FIELD(xcp_stats[4].jpeg_busy[19]), \
	__FIELD(xcp_stats[4].jpeg_busy[20]), \
	__FIELD(xcp_stats[4].jpeg_busy[21]), \
	__FIELD(xcp_stats[4].jpeg_busy[22]), \
	__FIELD(xcp_stats[4].jpeg_busy[23]), \
	__FIELD(xcp_stats[4].jpeg_busy[24]), \
	__FIELD(xcp_stats[4].jpeg_busy[25]), \
	__FIELD(xcp_stats[4].jpeg_busy[26]), \
	__FIELD(xcp_stats[4].jpeg_busy[27]), \
	__FIELD(xcp_stats[4].jpeg_busy[28]), \
	__FIELD(xcp_stats[4].jpeg_busy[29]), \
	__FIELD(xcp_stats[4].jpeg_busy[30]), \
	__FIELD(xcp_stats[4].jpeg_busy[31]), \
	__FIELD(xcp_stats[4].vcn_busy[0]), \
	__FIELD(xcp_stats[4].vcn_busy[1]), \
	__FIELD(xcp_stats[4].vcn_busy[2]), \
	__FIELD(xcp_stats[4].vcn_busy[3]), \
	__FIELD(xcp_stats[4].gfx_busy_acc[0]), \
	__FIELD(xcp_stats[4].gfx_busy_acc[1]), \
	__FIELD(xcp_stats[4].gfx_busy_acc[2]), \
	__FIELD(xcp_stats[4].gfx_busy_acc[3]), \
	__FIELD(xcp_stats[4].gfx_busy_acc[4]), \
	__FIELD(xcp_stats[4].gfx_busy_acc[5]), \
	__FIELD(xcp_stats[4].gfx_busy_acc[6]), \
	__FIELD(xcp_stats[4].gfx_busy_acc[7]), \
	__FIELD(xcp_stats[4].gfx_below_host_limit_acc[0]), \
	__FIELD(xcp_stats[4].gfx_below_host_limit_acc[1]), \
	__FIELD(xcp_stats[4].gfx_below_host_limit_acc[2]), \
	__FIELD(xcp_stats[4].gfx_below_host_limit_acc[3]), \
	__FIELD(xcp_stats[4].gfx_below_host_limit_acc[4]), \
	__FIELD(xcp_stats[4].gfx_below_host_limit_acc[5]), \
	__FIELD(xcp_stats[4].gfx_below_host_limit_acc[6]), \
	__FIELD(xcp_stats[4].gfx_below_host_limit_acc[7]), \
	__FIELD(xcp_stats[5].gfx_busy_inst[0]), \
	__FIELD(xcp_stats[5].gfx_busy_inst[1]), \
	__FIELD(xcp_stats[5].gfx_busy_inst[2]), \
	__FIELD(xcp_stats[5].gfx_busy_inst[3]), \
	__FIELD(xcp_stats[5].gfx_busy_inst[4]), \
	__FIELD(xcp_stats[5].gfx_busy_inst[5]), \
	__FIELD(xcp_stats[5].gfx_busy_inst[6]), \
	__FIELD(xcp_stats[5].gfx_busy_inst[7]), \
	__FIELD(xcp_stats[5].jpeg_busy[0]), \
	__FIELD(xcp_stats[5].jpeg_busy[1]), \
	__FIELD(xcp_stats[5].jpeg_busy[2]), \
	__FIELD(xcp_stats[5].jpeg_busy[3]), \
	__FIELD(xcp_stats[5].jpeg_busy[4]), \
	__FIELD(xcp_stats[5].jpeg_busy[5]), \
	__FIELD(xcp_stats[5].jpeg_busy[6]), \
	__FIELD(xcp_stats[5].jpeg_busy[7]), \
	__FIELD(xcp_stats[5].jpeg_busy[8]), \
	__FIELD(xcp_stats[5].jpeg_busy[9]), \
	__FIELD(xcp_stats[5].jpeg_busy[10]), \
	__FIELD(xcp_stats[5].jpeg_busy[11]), \
	__FIELD(xcp_stats[5].jpeg_busy[12]), \
	__FIELD(xcp_stats[5].jpeg_busy[13]), \
	__FIELD(xcp_stats[5].jpeg_busy[14]), \
	__FIELD(xcp_stats[5].jpeg_busy[15]), \
	__FIELD(xcp_stats[5].jpeg_busy[16]), \
	__FIELD(xcp_stats[5].jpeg_busy[17]), \
	__FIELD(xcp_stats[5].jpeg_busy[18]), \
	__FIELD(xcp_stats[5].jpeg_busy[19]), \
	__FIELD(xcp_stats[5].jpeg_busy[20]), \
	__FIELD(xcp_stats[5].jpeg_busy[21]), \
	__FIELD(xcp_stats[5].jpeg_busy[22]), \
	__FIELD(xcp_stats[5].jpeg_busy[23]), \
	__FIELD(xcp_stats[5].jpeg_busy[24]), \
	__FIELD(xcp_stats[5].jpeg_busy[25]), \
	__FIELD(xcp_stats[5].jpeg_busy[26]), \
	__FIELD(xcp_stats[5].jpeg_busy[27]), \
	__FIELD(xcp_stats[5].jpeg_busy[28]), \
	__FIELD(xcp_stats[5].jpeg_busy[29]), \
	__FIELD(xcp_stats[5].jpeg_busy[30]), \
	__FIELD(xcp_stats[5].jpeg_busy[31]), \
	__FIELD(xcp_stats[5].vcn_busy[0]), \
	__FIELD(xcp_stats[5].vcn_busy[1]), \
	__FIELD(xcp_stats[5].vcn_busy[2]), \
	__FIELD(xcp_stats[5].vcn_busy[3]), \
	__FIELD(xcp_stats[5].gfx_busy_acc[0]), \
	__FIELD(xcp_stats[5].gfx_busy_acc[1]), \
	__FIELD(xcp_stats[5].gfx_busy_acc[2]), \
	__FIELD(xcp_stats[5].gfx_busy_acc[3]), \
	__FIELD(xcp_stats[5].gfx_busy_acc[4]), \
	__FIELD(xcp_stats[5].gfx_busy_acc[5]), \
	__FIELD(xcp_stats[5].gfx_busy_acc[6]), \
	__FIELD(xcp_stats[5].gfx_busy_acc[7]), \
	__FIELD(xcp_stats[5].gfx_below_host_limit_acc[0]), \
	__FIELD(xcp_stats[5].gfx_below_host_limit_acc[1]), \
	__FIELD(xcp_stats[5].gfx_below_host_limit_acc[2]), \
	__FIELD(xcp_stats[5].gfx_below_host_limit_acc[3]), \
	__FIELD(xcp_stats[5].gfx_below_host_limit_acc[4]), \
	__FIELD(xcp_stats[5].gfx_below_host_limit_acc[5]), \
	__FIELD(xcp_stats[5].gfx_below_host_limit_acc[6]), \
	__FIELD(xcp_stats[5].gfx_below_host_limit_acc[7]), \
	__FIELD(xcp_stats[6].gfx_busy_inst[0]), \
	__FIELD(xcp_stats[6].gfx_busy_inst[1]), \
	__FIELD(xcp_stats[6].gfx_busy_inst[2]), \
	__FIELD(xcp_stats[6].gfx_busy_inst[3]), \
	__FIELD(xcp_stats[6].gfx_busy_inst[4]), \
	__FIELD(xcp_stats[6].gfx_busy_inst[5]), \
	__FIELD(xcp_stats[6].gfx_busy_inst[6]), \
	__FIELD(xcp_stats[6].gfx_busy_inst[7]), \
	__FIELD(xcp_stats[6].jpeg_busy[0]), \
	__FIELD(xcp_stats[6].jpeg_busy[1]), \
	__FIELD(xcp_stats[6].jpeg_busy[2]), \
	__FIELD(xcp_stats[6].jpeg_busy[3]), \
	__FIELD(xcp_stats[6].jpeg_busy[4]), \
	__FIELD(xcp_stats[6].jpeg_busy[5]), \
	__FIELD(xcp_stats[6].jpeg_busy[6]), \
	__FIELD(xcp_stats[6].jpeg_busy[7]), \
	__FIELD(xcp_stats[6].jpeg_busy[8]), \
	__FIELD(xcp_stats[6].jpeg_busy[9]), \
	__FIELD(xcp_stats[6].jpeg_busy[10]), \
	__FIELD(xcp_stats[6].jpeg_busy[11]), \
	__FIELD(xcp_stats[6].jpeg_busy[12]), \
	__FIELD(xcp_stats[6].jpeg_busy[13]), \
	__FIELD(xcp_stats[6].jpeg_busy[14]), \
	__FIELD(xcp_stats[6].jpeg_busy[15]), \
	__FIELD(xcp_stats[6].jpeg_busy[16]), \
	__FIELD(xcp_stats[6].jpeg_busy[17]), \
	__FIELD(xcp_stats[6].jpeg_busy[18]), \
	__FIELD(xcp_stats[6].jpeg_busy[19]), \
	__FIELD(xcp_stats[6].jpeg_busy[20]), \
	__FIELD(xcp_stats[6].jpeg_busy[21]), \
	__FIELD(xcp_stats[6].jpeg_busy[22]), \
	__FIELD(xcp_stats[6].jpeg_busy[23]), \
	__FIELD(xcp_stats[6].jpeg_busy[24]), \
	__FIELD(xcp_stats[6].jpeg_busy[25]), \
	__FIELD(xcp_stats[6].jpeg_busy[26]), \
	__FIELD(xcp_stats[6].jpeg_busy[27]), \
	__FIELD(xcp_stats[6].jpeg_busy[28]), \
	__FIELD(xcp_stats[6].jpeg_busy[29]), \
	__FIELD(xcp_stats[6].jpeg_busy[30]), \
	__FIELD(xcp_stats[6].jpeg_busy[31]), \
	__FIELD(xcp_stats[6].vcn_busy[0]), \
	__FIELD(xcp_stats[6].vcn_busy[1]), \
	__FIELD(xcp_stats[6].vcn_busy[2]), \
	__FIELD(xcp_stats[6].vcn_busy[3]), \
	__FIELD(xcp_stats[6].gfx_busy_acc[0]), \
	__FIELD(xcp_stats[6].gfx_busy_acc[1]), \
	__FIELD(xcp_stats[6].gfx_busy_acc[2]), \
	__FIELD(xcp_stats[6].gfx_busy_acc[3]), \
	__FIELD(xcp_stats[6].gfx_busy_acc[4]), \
	__FIELD(xcp_stats[6].gfx_busy_acc[5]), \
	__FIELD(xcp_stats[6].gfx_busy_acc[6]), \
	__FIELD(xcp_stats[6].gfx_busy_acc[7]), \
	__FIELD(xcp_stats[6].gfx_below_host_limit_acc[0]), \
	__FIELD(xcp_stats[6].gfx_below_host_limit_acc[1]), \
	__FIELD(xcp_stats[6].gfx_below_host_limit_acc[2]), \
	__FIELD(xcp_stats[6].gfx_below_host_limit_acc[3]), \
	__FIELD(xcp_stats[6].gfx_below_host_limit_acc[4]), \
	__FIELD(xcp_stats[6].gfx_below_host_limit_acc[5]), \
	__FIELD(xcp_stats[6].gfx_below_host_limit_acc[6]), \
	__FIELD(xcp_stats[6].gfx_below_host_limit_acc[7]), \
	__FIELD(xcp_stats[7].gfx_busy_inst[0]), \
	__FIELD(xcp_stats[7].gfx_busy_inst[1]), \
	__FIELD(xcp_stats[7].gfx_busy_inst[2]), \
	__FIELD(xcp_stats[7].gfx_busy_inst[3]), \
	__FIELD(xcp_stats[7].gfx_busy_inst[4]), \
	__FIELD(xcp_stats[7].gfx_busy_inst[5]), \
	__FIELD(xcp_stats[7].gfx_busy_inst[6]), \
	__FIELD(xcp_stats[7].gfx_busy_inst[7]), \
	__FIELD(xcp_stats[7].jpeg_busy[0]), \
	__FIELD(xcp_stats[7].jpeg_busy[1]), \
	__FIELD(xcp_stats[7].jpeg_busy[2]), \
	__FIELD(xcp_stats[7].jpeg_busy[3]), \
	__FIELD(xcp_stats[7].jpeg_busy[4]), \
	__FIELD(xcp_stats[7].jpeg_busy[5]), \
	__FIELD(xcp_stats[7].jpeg_busy[6]), \
	__FIELD(xcp_stats[7].jpeg_busy[7]), \
	__FIELD(xcp_stats[7].jpeg_busy[8]), \
	__FIELD(xcp_stats[7].jpeg_busy[9]), \
	__FIELD(xcp_stats[7].jpeg_busy[10]), \
	__FIELD(xcp_stats[7].jpeg_busy[11]), \
	__FIELD(xcp_stats[7].jpeg_busy[12]), \
	__FIELD(xcp_stats[7].jpeg_busy[13]), \
	__FIELD(xcp_stats[7].jpeg_busy[14]), \
	__FIELD(xcp_stats[7].jpeg_busy[15]), \
	__FIELD(xcp_stats[7].jpeg_busy[16]), \
	__FIELD(xcp_stats[7].jpeg_busy[17]), \
	__FIELD(xcp_stats[7].jpeg_busy[18]), \
	__FIELD(xcp_stats[7].jpeg_busy[19]), \
	__FIELD(xcp_stats[7].jpeg_busy[20]), \
	__FIELD(xcp_stats[7].jpeg_busy[21]), \
	__FIELD(xcp_stats[7].jpeg_busy[22]), \
	__FIELD(xcp_stats[7].jpeg_busy[23]), \
	__FIELD(xcp_stats[7].jpeg_busy[24]), \
	__FIELD(xcp_stats[7].jpeg_busy[25]), \
	__FIELD(xcp_stats[7].jpeg_busy[26]), \
	__FIELD(xcp_stats[7].jpeg_busy[27]), \
	__FIELD(xcp_stats[7].jpeg_busy[28]), \
	__FIELD(xcp_stats[7].jpeg_busy[29]), \
	__FIELD(xcp_stats[7].jpeg_busy[30]), \
	__FIELD(xcp_stats[7].jpeg_busy[31]), \
	__FIELD(xcp_stats[7].vcn_busy[0]), \
	__FIELD(xcp_stats[7].vcn_busy[1]), \
	__FIELD(xcp_stats[7].vcn_busy[2]), \
	__FIELD(xcp_stats[7].vcn_busy[3]), \
	__FIELD(xcp_stats[7].gfx_busy_acc[0]), \
	__FIELD(xcp_stats[7].gfx_busy_acc[1]), \
	__FIELD(xcp_stats[7].gfx_busy_acc[2]), \
	__FIELD(xcp_stats[7].gfx_busy_acc[3]), \
	__FIELD(xcp_stats[7].gfx_busy_acc[4]), \
	__FIELD(xcp_stats[7].gfx_busy_acc[5]), \
	__FIELD(xcp_stats[7].gfx_busy_acc[6]), \
	__FIELD(xcp_stats[7].gfx_busy_acc[7]), \
	__FIELD(xcp_stats[7].gfx_below_host_limit_acc[0]), \
	__FIELD(xcp_stats[7].gfx_below_host_limit_acc[1]), \
	__FIELD(xcp_stats[7].gfx_below_host_limit_acc[2]), \
	__FIELD(xcp_stats[7].gfx_below_host_limit_acc[3]), \
	__FIELD(xcp_stats[7].gfx_below_host_limit_acc[4]), \
	__FIELD(xcp_stats[7].gfx_below_host_limit_acc[5]), \
	__FIELD(xcp_stats[7].gfx_below_host_limit_acc[6]), \
	__FIELD(xcp_stats[7].gfx_below_host_limit_acc[7]), \
	__FIELD(pcie_lc_perf_other_end_recovery),

#define METRICS_INFO_V2_0_LIST(__FIELD) \
	__FIELD(system_clock_counter),	\
	__FIELD(temperature_gfx),	\
	__FIELD(temperature_soc),	\
	__FIELD(temperature_core[0]),	\
	__FIELD(temperature_core[1]),	\
	__FIELD(temperature_core[2]),	\
	__FIELD(temperature_core[3]),	\
	__FIELD(temperature_core[4]),	\
	__FIELD(temperature_core[5]),	\
	__FIELD(temperature_core[6]),	\
	__FIELD(temperature_core[7]),	\
	__FIELD(temperature_l3[0]),	\
	__FIELD(temperature_l3[1]),	\
	__FIELD(average_gfx_activity),	\
	__FIELD(average_mm_activity),	\
	__FIELD(average_socket_power),	\
	__FIELD(average_cpu_power),	\
	__FIELD(average_soc_power),	\
	__FIELD(average_gfx_power),	\
	__FIELD(average_core_power[0]),	\
	__FIELD(average_core_power[1]),	\
	__FIELD(average_core_power[2]),	\
	__FIELD(average_core_power[3]),	\
	__FIELD(average_core_power[4]),	\
	__FIELD(average_core_power[5]),	\
	__FIELD(average_core_power[6]),	\
	__FIELD(average_core_power[7]),	\
	__FIELD(average_gfxclk_frequency),	\
	__FIELD(average_socclk_frequency),	\
	__FIELD(average_uclk_frequency),	\
	__FIELD(average_fclk_frequency),	\
	__FIELD(average_vclk_frequency),	\
	__FIELD(average_dclk_frequency),	\
	__FIELD(current_gfxclk),	\
	__FIELD(current_socclk),	\
	__FIELD(current_uclk),	\
	__FIELD(current_fclk),	\
	__FIELD(current_vclk),	\
	__FIELD(current_dclk),	\
	__FIELD(current_coreclk[0]),	\
	__FIELD(current_coreclk[1]),	\
	__FIELD(current_coreclk[2]),	\
	__FIELD(current_coreclk[3]),	\
	__FIELD(current_coreclk[4]),	\
	__FIELD(current_coreclk[5]),	\
	__FIELD(current_coreclk[6]),	\
	__FIELD(current_coreclk[7]),	\
	__FIELD(current_l3clk[0]),	\
	__FIELD(current_l3clk[1]),	\
	__FIELD(throttle_status),	\
	__FIELD(fan_pwm),

#define METRICS_INFO_V2_1_LIST(__FIELD) \
	__FIELD(temperature_gfx),	\
	__FIELD(temperature_soc),	\
	__FIELD(temperature_core[0]),	\
	__FIELD(temperature_core[1]),	\
	__FIELD(temperature_core[2]),	\
	__FIELD(temperature_core[3]),	\
	__FIELD(temperature_core[4]),	\
	__FIELD(temperature_core[5]),	\
	__FIELD(temperature_core[6]),	\
	__FIELD(temperature_core[7]),	\
	__FIELD(temperature_l3[0]),	\
	__FIELD(temperature_l3[1]),	\
	__FIELD(average_gfx_activity),	\
	__FIELD(average_mm_activity),	\
	__FIELD(system_clock_counter),	\
	__FIELD(average_socket_power),	\
	__FIELD(average_cpu_power),	\
	__FIELD(average_soc_power),	\
	__FIELD(average_gfx_power),	\
	__FIELD(average_core_power[0]),	\
	__FIELD(average_core_power[1]),	\
	__FIELD(average_core_power[2]),	\
	__FIELD(average_core_power[3]),	\
	__FIELD(average_core_power[4]),	\
	__FIELD(average_core_power[5]),	\
	__FIELD(average_core_power[6]),	\
	__FIELD(average_core_power[7]),	\
	__FIELD(average_gfxclk_frequency),	\
	__FIELD(average_socclk_frequency),	\
	__FIELD(average_uclk_frequency),	\
	__FIELD(average_fclk_frequency),	\
	__FIELD(average_vclk_frequency),	\
	__FIELD(average_dclk_frequency),	\
	__FIELD(current_gfxclk),	\
	__FIELD(current_socclk),	\
	__FIELD(current_uclk),	\
	__FIELD(current_fclk),	\
	__FIELD(current_vclk),	\
	__FIELD(current_dclk),	\
	__FIELD(current_coreclk[0]),	\
	__FIELD(current_coreclk[1]),	\
	__FIELD(current_coreclk[2]),	\
	__FIELD(current_coreclk[3]),	\
	__FIELD(current_coreclk[4]),	\
	__FIELD(current_coreclk[5]),	\
	__FIELD(current_coreclk[6]),	\
	__FIELD(current_coreclk[7]),	\
	__FIELD(current_l3clk[0]),	\
	__FIELD(current_l3clk[1]),	\
	__FIELD(throttle_status),	\
	__FIELD(fan_pwm),

#define METRICS_INFO_V2_2_LIST(__FIELD) \
	__FIELD(temperature_gfx),	\
	__FIELD(temperature_soc),	\
	__FIELD(temperature_core[0]),	\
	__FIELD(temperature_core[1]),	\
	__FIELD(temperature_core[2]),	\
	__FIELD(temperature_core[3]),	\
	__FIELD(temperature_core[4]),	\
	__FIELD(temperature_core[5]),	\
	__FIELD(temperature_core[6]),	\
	__FIELD(temperature_core[7]),	\
	__FIELD(temperature_l3[0]),	\
	__FIELD(temperature_l3[1]),	\
	__FIELD(average_gfx_activity),	\
	__FIELD(average_mm_activity),	\
	__FIELD(system_clock_counter),	\
	__FIELD(average_socket_power),	\
	__FIELD(average_cpu_power),	\
	__FIELD(average_soc_power),	\
	__FIELD(average_gfx_power),	\
	__FIELD(average_core_power[0]),	\
	__FIELD(average_core_power[1]),	\
	__FIELD(average_core_power[2]),	\
	__FIELD(average_core_power[3]),	\
	__FIELD(average_core_power[4]),	\
	__FIELD(average_core_power[5]),	\
	__FIELD(average_core_power[6]),	\
	__FIELD(average_core_power[7]),	\
	__FIELD(average_gfxclk_frequency),	\
	__FIELD(average_socclk_frequency),	\
	__FIELD(average_uclk_frequency),	\
	__FIELD(average_fclk_frequency),	\
	__FIELD(average_vclk_frequency),	\
	__FIELD(average_dclk_frequency),	\
	__FIELD(current_gfxclk),	\
	__FIELD(current_socclk),	\
	__FIELD(current_uclk),	\
	__FIELD(current_fclk),	\
	__FIELD(current_vclk),	\
	__FIELD(current_dclk),	\
	__FIELD(current_coreclk[0]),	\
	__FIELD(current_coreclk[1]),	\
	__FIELD(current_coreclk[2]),	\
	__FIELD(current_coreclk[3]),	\
	__FIELD(current_coreclk[4]),	\
	__FIELD(current_coreclk[5]),	\
	__FIELD(current_coreclk[6]),	\
	__FIELD(current_coreclk[7]),	\
	__FIELD(current_l3clk[0]),	\
	__FIELD(current_l3clk[1]),	\
	__FIELD(throttle_status),	\
	__FIELD(fan_pwm), \
	__FIELD(padding[0]), \
	__FIELD(padding[1]), \
	__FIELD(padding[2]), \
	__FIELD(indep_throttle_status),

#define METRICS_INFO_V2_3_LIST(__FIELD) \
	__FIELD(temperature_gfx),	\
	__FIELD(temperature_soc),	\
	__FIELD(temperature_core[0]),	\
	__FIELD(temperature_core[1]),	\
	__FIELD(temperature_core[2]),	\
	__FIELD(temperature_core[3]),	\
	__FIELD(temperature_core[4]),	\
	__FIELD(temperature_core[5]),	\
	__FIELD(temperature_core[6]),	\
	__FIELD(temperature_core[7]),	\
	__FIELD(temperature_l3[0]),	\
	__FIELD(temperature_l3[1]),	\
	__FIELD(average_gfx_activity),	\
	__FIELD(average_mm_activity),	\
	__FIELD(system_clock_counter),	\
	__FIELD(average_socket_power),	\
	__FIELD(average_cpu_power),	\
	__FIELD(average_soc_power),	\
	__FIELD(average_gfx_power),	\
	__FIELD(average_core_power[0]),	\
	__FIELD(average_core_power[1]),	\
	__FIELD(average_core_power[2]),	\
	__FIELD(average_core_power[3]),	\
	__FIELD(average_core_power[4]),	\
	__FIELD(average_core_power[5]),	\
	__FIELD(average_core_power[6]),	\
	__FIELD(average_core_power[7]),	\
	__FIELD(average_gfxclk_frequency),	\
	__FIELD(average_socclk_frequency),	\
	__FIELD(average_uclk_frequency),	\
	__FIELD(average_fclk_frequency),	\
	__FIELD(average_vclk_frequency),	\
	__FIELD(average_dclk_frequency),	\
	__FIELD(current_gfxclk),	\
	__FIELD(current_socclk),	\
	__FIELD(current_uclk),	\
	__FIELD(current_fclk),	\
	__FIELD(current_vclk),	\
	__FIELD(current_dclk),	\
	__FIELD(current_coreclk[0]), \
	__FIELD(current_coreclk[1]), \
	__FIELD(current_coreclk[2]), \
	__FIELD(current_coreclk[3]), \
	__FIELD(current_coreclk[4]), \
	__FIELD(current_coreclk[5]), \
	__FIELD(current_coreclk[6]), \
	__FIELD(current_coreclk[7]), \
	__FIELD(current_l3clk[0]), \
	__FIELD(current_l3clk[1]), \
	__FIELD(throttle_status),	\
	__FIELD(fan_pwm), \
	__FIELD(padding[0]), \
	__FIELD(padding[1]), \
	__FIELD(padding[2]), \
	__FIELD(indep_throttle_status), \
	__FIELD(average_temperature_gfx), \
	__FIELD(average_temperature_soc), \
	__FIELD(average_temperature_core[0]), \
	__FIELD(average_temperature_core[1]), \
	__FIELD(average_temperature_core[2]), \
	__FIELD(average_temperature_core[3]), \
	__FIELD(average_temperature_core[4]), \
	__FIELD(average_temperature_core[5]), \
	__FIELD(average_temperature_core[6]), \
	__FIELD(average_temperature_core[7]), \
	__FIELD(average_temperature_l3[0]), \
	__FIELD(average_temperature_l3[1])

#define METRICS_INFO_V2_4_LIST(__FIELD) \
	__FIELD(temperature_gfx),	\
	__FIELD(temperature_soc),	\
	__FIELD(temperature_core[0]),	\
	__FIELD(temperature_core[1]),	\
	__FIELD(temperature_core[2]),	\
	__FIELD(temperature_core[3]),	\
	__FIELD(temperature_core[4]),	\
	__FIELD(temperature_core[5]),	\
	__FIELD(temperature_core[6]),	\
	__FIELD(temperature_core[7]),	\
	__FIELD(temperature_l3[0]),	\
	__FIELD(temperature_l3[1]),	\
	__FIELD(average_gfx_activity),	\
	__FIELD(average_mm_activity),	\
	__FIELD(system_clock_counter),	\
	__FIELD(average_socket_power),	\
	__FIELD(average_cpu_power),	\
	__FIELD(average_soc_power),	\
	__FIELD(average_gfx_power),	\
	__FIELD(average_core_power[0]),	\
	__FIELD(average_core_power[1]),	\
	__FIELD(average_core_power[2]),	\
	__FIELD(average_core_power[3]),	\
	__FIELD(average_core_power[4]),	\
	__FIELD(average_core_power[5]),	\
	__FIELD(average_core_power[6]),	\
	__FIELD(average_core_power[7]),	\
	__FIELD(average_gfxclk_frequency),	\
	__FIELD(average_socclk_frequency),	\
	__FIELD(average_uclk_frequency),	\
	__FIELD(average_fclk_frequency),	\
	__FIELD(average_vclk_frequency),	\
	__FIELD(average_dclk_frequency),	\
	__FIELD(current_gfxclk),	\
	__FIELD(current_socclk),	\
	__FIELD(current_uclk),	\
	__FIELD(current_fclk),	\
	__FIELD(current_vclk),	\
	__FIELD(current_dclk),	\
	__FIELD(current_coreclk[0]), \
	__FIELD(current_coreclk[1]), \
	__FIELD(current_coreclk[2]), \
	__FIELD(current_coreclk[3]), \
	__FIELD(current_coreclk[4]), \
	__FIELD(current_coreclk[5]), \
	__FIELD(current_coreclk[6]), \
	__FIELD(current_coreclk[7]), \
	__FIELD(current_l3clk[0]), \
	__FIELD(current_l3clk[1]), \
	__FIELD(throttle_status),	\
	__FIELD(fan_pwm), \
	__FIELD(padding[0]), \
	__FIELD(padding[1]), \
	__FIELD(padding[2]), \
	__FIELD(indep_throttle_status), \
	__FIELD(average_temperature_gfx), \
	__FIELD(average_temperature_soc), \
	__FIELD(average_temperature_core[0]), \
	__FIELD(average_temperature_core[1]), \
	__FIELD(average_temperature_core[2]), \
	__FIELD(average_temperature_core[3]), \
	__FIELD(average_temperature_core[4]), \
	__FIELD(average_temperature_core[5]), \
	__FIELD(average_temperature_core[6]), \
	__FIELD(average_temperature_core[7]), \
	__FIELD(average_temperature_l3[0]), \
	__FIELD(average_temperature_l3[1]), \
	__FIELD(average_cpu_voltage), \
	__FIELD(average_soc_voltage), \
	__FIELD(average_gfx_voltage), \
	__FIELD(average_cpu_current), \
	__FIELD(average_soc_current), \
	__FIELD(average_gfx_current)

#define METRICS_INFO_V3_0_LIST(__FIELD) \
	__FIELD(temperature_gfx),	\
	__FIELD(temperature_soc),	\
	__FIELD(temperature_core[0]),	\
	__FIELD(temperature_core[1]),	\
	__FIELD(temperature_core[2]),	\
	__FIELD(temperature_core[3]),	\
	__FIELD(temperature_core[4]),	\
	__FIELD(temperature_core[5]),	\
	__FIELD(temperature_core[6]),	\
	__FIELD(temperature_core[7]),	\
	__FIELD(temperature_core[8]),	\
	__FIELD(temperature_core[9]),	\
	__FIELD(temperature_core[10]),	\
	__FIELD(temperature_core[11]),	\
	__FIELD(temperature_core[12]),	\
	__FIELD(temperature_core[13]),	\
	__FIELD(temperature_core[14]),	\
	__FIELD(temperature_core[15]),	\
	__FIELD(temperature_skin),	\
	__FIELD(average_gfx_activity),	\
	__FIELD(average_vcn_activity),	\
	__FIELD(average_ipu_activity[0]),	\
	__FIELD(average_ipu_activity[1]),	\
	__FIELD(average_ipu_activity[2]),	\
	__FIELD(average_ipu_activity[3]),	\
	__FIELD(average_ipu_activity[4]),	\
	__FIELD(average_ipu_activity[5]),	\
	__FIELD(average_ipu_activity[6]),	\
	__FIELD(average_ipu_activity[7]),	\
	__FIELD(average_core_c0_activity[0]),	\
	__FIELD(average_core_c0_activity[1]),	\
	__FIELD(average_core_c0_activity[2]),	\
	__FIELD(average_core_c0_activity[3]),	\
	__FIELD(average_core_c0_activity[4]),	\
	__FIELD(average_core_c0_activity[5]),	\
	__FIELD(average_core_c0_activity[6]),	\
	__FIELD(average_core_c0_activity[7]),	\
	__FIELD(average_core_c0_activity[8]),	\
	__FIELD(average_core_c0_activity[9]),	\
	__FIELD(average_core_c0_activity[10]),	\
	__FIELD(average_core_c0_activity[11]),	\
	__FIELD(average_core_c0_activity[12]),	\
	__FIELD(average_core_c0_activity[13]),	\
	__FIELD(average_core_c0_activity[14]),	\
	__FIELD(average_core_c0_activity[15]),	\
	__FIELD(average_dram_reads),	\
	__FIELD(average_dram_writes),	\
	__FIELD(average_ipu_reads),	\
	__FIELD(average_ipu_writes),	\
	__FIELD(system_clock_counter),	\
	__FIELD(average_socket_power),	\
	__FIELD(average_ipu_power),	\
	__FIELD(average_apu_power),	\
	__FIELD(average_gfx_power),	\
	__FIELD(average_dgpu_power),	\
	__FIELD(average_all_core_power),	\
	__FIELD(average_core_power[0]),	\
	__FIELD(average_core_power[1]),	\
	__FIELD(average_core_power[2]),	\
	__FIELD(average_core_power[3]),	\
	__FIELD(average_core_power[4]),	\
	__FIELD(average_core_power[5]),	\
	__FIELD(average_core_power[6]),	\
	__FIELD(average_core_power[7]),	\
	__FIELD(average_core_power[8]),	\
	__FIELD(average_core_power[9]),	\
	__FIELD(average_core_power[10]),	\
	__FIELD(average_core_power[11]),	\
	__FIELD(average_core_power[12]),	\
	__FIELD(average_core_power[13]),	\
	__FIELD(average_core_power[14]),	\
	__FIELD(average_core_power[15]),	\
	__FIELD(average_sys_power),	\
	__FIELD(stapm_power_limit),	\
	__FIELD(current_stapm_power_limit),	\
	__FIELD(average_gfxclk_frequency),	\
	__FIELD(average_socclk_frequency),	\
	__FIELD(average_vpeclk_frequency),	\
	__FIELD(average_ipuclk_frequency),	\
	__FIELD(average_fclk_frequency),	\
	__FIELD(average_vclk_frequency),	\
	__FIELD(average_uclk_frequency),	\
	__FIELD(average_mpipu_frequency),	\
	__FIELD(current_coreclk[0]),	\
	__FIELD(current_coreclk[1]),	\
	__FIELD(current_coreclk[2]),	\
	__FIELD(current_coreclk[3]),	\
	__FIELD(current_coreclk[4]),	\
	__FIELD(current_coreclk[5]),	\
	__FIELD(current_coreclk[6]),	\
	__FIELD(current_coreclk[7]),	\
	__FIELD(current_coreclk[8]),	\
	__FIELD(current_coreclk[9]),	\
	__FIELD(current_coreclk[10]),	\
	__FIELD(current_coreclk[11]),	\
	__FIELD(current_coreclk[12]),	\
	__FIELD(current_coreclk[13]),	\
	__FIELD(current_coreclk[14]),	\
	__FIELD(current_coreclk[15]),	\
	__FIELD(current_core_maxfreq),	\
	__FIELD(current_gfx_maxfreq),	\
	__FIELD(throttle_residency_prochot),	\
	__FIELD(throttle_residency_spl),	\
	__FIELD(throttle_residency_fppt),	\
	__FIELD(throttle_residency_sppt),	\
	__FIELD(throttle_residency_thm_core),	\
	__FIELD(throttle_residency_thm_gfx),	\
	__FIELD(throttle_residency_thm_soc),	\
	__FIELD(time_filter_alphavalue)

static struct umr_metrics_field_info metrics_header[] = {
#define METRICS_HEADER_INFO(MEMBER)	FIELD_INFO(struct umr_metrics_table_header, MEMBER)
	METRICS_HEADER_LIST(METRICS_HEADER_INFO)
};

static struct umr_metrics_field_info metrics_v1_0[] = {
#define METRICS_V1_0_INFO(MEMBER)	FIELD_INFO(struct umr_gpu_metrics_v1_0, MEMBER)
	METRICS_INFO_V1_0_LIST(METRICS_V1_0_INFO)
};

static struct umr_metrics_field_info metrics_v1_1[] = {
#define METRICS_V1_1_INFO(MEMBER)	FIELD_INFO(struct umr_gpu_metrics_v1_1, MEMBER)
	METRICS_INFO_V1_1_LIST(METRICS_V1_1_INFO)
};

static struct umr_metrics_field_info metrics_v1_2[] = {
#define METRICS_V1_2_INFO(MEMBER)	FIELD_INFO(struct umr_gpu_metrics_v1_2, MEMBER)
	METRICS_INFO_V1_2_LIST(METRICS_V1_2_INFO)
};

static struct umr_metrics_field_info metrics_v1_3[] = {
#define METRICS_V1_3_INFO(MEMBER)	FIELD_INFO(struct umr_gpu_metrics_v1_3, MEMBER)
	METRICS_INFO_V1_3_LIST(METRICS_V1_3_INFO)
};

static struct umr_metrics_field_info metrics_v1_4[] = {
#define METRICS_V1_4_INFO(MEMBER)	FIELD_INFO(struct umr_gpu_metrics_v1_4, MEMBER)
	METRICS_INFO_V1_4_LIST(METRICS_V1_4_INFO)
};

static struct umr_metrics_field_info metrics_v1_5[] = {
#define METRICS_V1_5_INFO(MEMBER)	FIELD_INFO(struct umr_gpu_metrics_v1_5, MEMBER)
	METRICS_INFO_V1_5_LIST(METRICS_V1_5_INFO)
};

static struct umr_metrics_field_info metrics_v1_6[] = {
#define METRICS_V1_6_INFO(MEMBER)	FIELD_INFO(struct umr_gpu_metrics_v1_6, MEMBER)
	METRICS_INFO_V1_6_LIST(METRICS_V1_6_INFO)
};

static struct umr_metrics_field_info metrics_v1_7[] = {
#define METRICS_V1_7_INFO(MEMBER)	FIELD_INFO(struct umr_gpu_metrics_v1_7, MEMBER)
	METRICS_INFO_V1_7_LIST(METRICS_V1_7_INFO)
};

static struct umr_metrics_field_info metrics_v2_0[] = {
#define METRICS_V2_0_INFO(MEMBER)	FIELD_INFO(struct umr_gpu_metrics_v2_0, MEMBER)
	METRICS_INFO_V2_0_LIST(METRICS_V2_0_INFO)
};

static struct umr_metrics_field_info metrics_v2_1[] = {
#define METRICS_V2_1_INFO(MEMBER)	FIELD_INFO(struct umr_gpu_metrics_v2_1, MEMBER)
	METRICS_INFO_V2_1_LIST(METRICS_V2_1_INFO)
};

static struct umr_metrics_field_info metrics_v2_2[] = {
#define METRICS_V2_2_INFO(MEMBER)	FIELD_INFO(struct umr_gpu_metrics_v2_2, MEMBER)
	METRICS_INFO_V2_2_LIST(METRICS_V2_2_INFO)
};

static struct umr_metrics_field_info metrics_v2_3[] = {
#define METRICS_V2_3_INFO(MEMBER)	FIELD_INFO(struct umr_gpu_metrics_v2_3, MEMBER)
	METRICS_INFO_V2_3_LIST(METRICS_V2_3_INFO)
};

static struct umr_metrics_field_info metrics_v2_4[] = {
#define METRICS_V2_4_INFO(MEMBER)	FIELD_INFO(struct umr_gpu_metrics_v2_4, MEMBER)
	METRICS_INFO_V2_4_LIST(METRICS_V2_4_INFO)
};

static struct umr_metrics_field_info metrics_v3_0[] = {
#define METRICS_V3_0_INFO(MEMBER)	FIELD_INFO(struct umr_gpu_metrics_v3_0, MEMBER)
	METRICS_INFO_V3_0_LIST(METRICS_V3_0_INFO)
};

static void umr_dump_field_info(struct umr_asic *asic, const struct umr_metrics_field_info *info,
				const uint32_t count, const char *prefix, const uint8_t *ref, struct umr_key_value *kv)
{
	uint32_t i;
	const struct umr_metrics_field_info *tmp;

	if (!prefix)
		prefix = "";

	for (i = 0, tmp = &info[i]; i < count; i++, tmp = &info[i]) {
		if (kv->used == UMR_MAX_KEYS) {
			asic->err_msg("[WARNING]: Too many keys for gpu metrics data...\n");
			return;
		}
		sprintf(kv->keys[kv->used].name, "%s%s", prefix, tmp->name);
		switch (tmp->size) {
		case 1:
			sprintf(kv->keys[kv->used].value, "%"PRIu8, *(uint8_t *)(ref + tmp->offset));
			break;
		case 2:
			sprintf(kv->keys[kv->used].value, "%"PRIu16, *(uint16_t *)(ref + tmp->offset));
			break;
		case 4:
			sprintf(kv->keys[kv->used].value, "%"PRIu32, *(uint32_t *)(ref + tmp->offset));
			break;
		case 8:
			sprintf(kv->keys[kv->used].value, "%"PRIu64, *(uint64_t *)(ref + tmp->offset));
			break;
		default:
			break;
		}
		++(kv->used);
	}
}

/**
 * umr_dump_metrics - Dump GPU metrics to a KV array
 *
 * @asic: The ASIC the table comes from
 * @table: The contents of the GPU metrics file
 * @size: The size of the table
 *
 * Returns a pointer to umr_key_value structure if successful.
 */
struct umr_key_value *umr_dump_metrics(struct umr_asic *asic, const void *table, uint32_t size)
{
	struct umr_metrics_table_header *header =
		(struct umr_metrics_table_header *)table;

	struct umr_key_value *kv;

	if (!table || !size)
		return NULL;

	kv = calloc(1, sizeof *kv);

	umr_dump_field_info(asic, metrics_header, ARRAY_SIZE(metrics_header), " hdr.", table, kv);

#define METRICS_VERSION(a, b)	((a << 16) | b )

	switch (METRICS_VERSION(header->format_revision, header->content_revision)) {
	case METRICS_VERSION(1, 0):
		umr_dump_field_info(asic, metrics_v1_0, ARRAY_SIZE(metrics_v1_0), "v1_0.", table, kv);
		break;
	case METRICS_VERSION(1, 1):
		umr_dump_field_info(asic, metrics_v1_1, ARRAY_SIZE(metrics_v1_1), "v1_1.", table, kv);
		break;
	case METRICS_VERSION(1, 2):
		umr_dump_field_info(asic, metrics_v1_2, ARRAY_SIZE(metrics_v1_2), "v1_2.", table, kv);
		break;
	case METRICS_VERSION(1, 3):
		umr_dump_field_info(asic, metrics_v1_3, ARRAY_SIZE(metrics_v1_3), "v1_3.", table, kv);
		break;
	case METRICS_VERSION(1, 4):
		umr_dump_field_info(asic, metrics_v1_4, ARRAY_SIZE(metrics_v1_4), "v1_4.", table, kv);
		break;
	case METRICS_VERSION(1, 5):
		umr_dump_field_info(asic, metrics_v1_5, ARRAY_SIZE(metrics_v1_5), "v1_5.", table, kv);
		break;
	case METRICS_VERSION(1, 6):
		umr_dump_field_info(asic, metrics_v1_6, ARRAY_SIZE(metrics_v1_6), "v1_6.", table, kv);
		break;
	case METRICS_VERSION(1, 7):
		umr_dump_field_info(asic, metrics_v1_7, ARRAY_SIZE(metrics_v1_7), "v1_7.", table, kv);
		break;
	case METRICS_VERSION(2, 0):
		umr_dump_field_info(asic, metrics_v2_0, ARRAY_SIZE(metrics_v2_0), "v2_0.", table, kv);
		break;
	case METRICS_VERSION(2, 1):
		umr_dump_field_info(asic, metrics_v2_1, ARRAY_SIZE(metrics_v2_1), "v2_1.", table, kv);
		break;
	case METRICS_VERSION(2, 2):
		umr_dump_field_info(asic, metrics_v2_2, ARRAY_SIZE(metrics_v2_2), "v2_2.", table, kv);
		break;
	case METRICS_VERSION(2, 3):
		umr_dump_field_info(asic, metrics_v2_3, ARRAY_SIZE(metrics_v2_3), "v2_3.", table, kv);
		break;
	case METRICS_VERSION(2, 4):
		umr_dump_field_info(asic, metrics_v2_4, ARRAY_SIZE(metrics_v2_4), "v2_4.", table, kv);
		break;
	case METRICS_VERSION(3, 0):
		umr_dump_field_info(asic, metrics_v3_0, ARRAY_SIZE(metrics_v3_0), "v3_0.", table, kv);
		break;
	default:
		asic->err_msg("[ERROR]: Unknown Metrics table format: 0x%"PRIx8"\n", header->format_revision);
		free(kv);
		return NULL;
	}

	return kv;
}


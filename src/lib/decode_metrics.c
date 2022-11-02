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

static struct field_info metrics_header[] = {
#define METRICS_HEADER_INFO(MEMBER)	FIELD_INFO(struct umr_metrics_table_header, MEMBER)
	METRICS_HEADER_LIST(METRICS_HEADER_INFO)
};

static struct field_info metrics_v1_0[] = {
#define METRICS_V1_0_INFO(MEMBER)	FIELD_INFO(struct umr_gpu_metrics_v1_0, MEMBER)
	METRICS_INFO_V1_0_LIST(METRICS_V1_0_INFO)
};

static struct field_info metrics_v1_1[] = {
#define METRICS_V1_1_INFO(MEMBER)	FIELD_INFO(struct umr_gpu_metrics_v1_1, MEMBER)
	METRICS_INFO_V1_1_LIST(METRICS_V1_1_INFO)
};

static struct field_info metrics_v1_2[] = {
#define METRICS_V1_2_INFO(MEMBER)	FIELD_INFO(struct umr_gpu_metrics_v1_2, MEMBER)
	METRICS_INFO_V1_2_LIST(METRICS_V1_2_INFO)
};

static struct field_info metrics_v1_3[] = {
#define METRICS_V1_3_INFO(MEMBER)	FIELD_INFO(struct umr_gpu_metrics_v1_3, MEMBER)
	METRICS_INFO_V1_3_LIST(METRICS_V1_3_INFO)
};

static struct field_info metrics_v2_0[] = {
#define METRICS_V2_0_INFO(MEMBER)	FIELD_INFO(struct umr_gpu_metrics_v2_0, MEMBER)
	METRICS_INFO_V2_0_LIST(METRICS_V2_0_INFO)
};

static struct field_info metrics_v2_1[] = {
#define METRICS_V2_1_INFO(MEMBER)	FIELD_INFO(struct umr_gpu_metrics_v2_1, MEMBER)
	METRICS_INFO_V2_1_LIST(METRICS_V2_1_INFO)
};

static struct field_info metrics_v2_2[] = {
#define METRICS_V2_2_INFO(MEMBER)	FIELD_INFO(struct umr_gpu_metrics_v2_2, MEMBER)
	METRICS_INFO_V2_2_LIST(METRICS_V2_2_INFO)
};

static void umr_dump_field_info(struct umr_asic *asic, const struct field_info *info,
				const uint32_t count, const char *prefix, const uint8_t *ref)
{
	uint32_t i;
	const struct field_info *tmp;
	const char *fmt = "%s%-30s = %lld\n";

	if (!prefix)
		prefix = "";

	for (i = 0, tmp = &info[i]; i < count; i++, tmp = &info[i]) {
		switch (tmp->size) {
		case 1:
			asic->std_msg(fmt, prefix, tmp->name, *(uint8_t *)(ref + tmp->offset));
			break;
		case 2:
			asic->std_msg(fmt, prefix, tmp->name, *(uint16_t *)(ref + tmp->offset));
			break;
		case 4:
			asic->std_msg(fmt, prefix, tmp->name, *(uint32_t *)(ref + tmp->offset));
			break;
		case 8:
			asic->std_msg(fmt, prefix, tmp->name, *(uint64_t *)(ref + tmp->offset));
			break;
		default:
			break;
		}
	}
}

int umr_dump_metrics(struct umr_asic *asic, const void *table, uint32_t size)
{
	struct umr_metrics_table_header *header =
		(struct umr_metrics_table_header *)table;

	if (!table || !size)
		return -1;

	umr_dump_field_info(asic, metrics_header, ARRAY_SIZE(metrics_header), " hdr.", table);

#define METRICS_VERSION(a, b)	((a << 16) | b )

	switch (METRICS_VERSION(header->format_revision, header->content_revision)) {
	case METRICS_VERSION(1, 0):
		umr_dump_field_info(asic, metrics_v1_0, ARRAY_SIZE(metrics_v1_0), "v1_0.", table);
		break;
	case METRICS_VERSION(1, 1):
		umr_dump_field_info(asic, metrics_v1_1, ARRAY_SIZE(metrics_v1_1), "v1_1.", table);
		break;
	case METRICS_VERSION(1, 2):
		umr_dump_field_info(asic, metrics_v1_2, ARRAY_SIZE(metrics_v1_2), "v1_2.", table);
		break;
	case METRICS_VERSION(1, 3):
		umr_dump_field_info(asic, metrics_v1_3, ARRAY_SIZE(metrics_v1_3), "v1_3.", table);
		break;
	case METRICS_VERSION(2, 0):
		umr_dump_field_info(asic, metrics_v2_0, ARRAY_SIZE(metrics_v2_0), "v2_0.", table);
		break;
	case METRICS_VERSION(2, 1):
		umr_dump_field_info(asic, metrics_v2_1, ARRAY_SIZE(metrics_v2_1), "v2_1.", table);
		break;
	case METRICS_VERSION(2, 2):
		umr_dump_field_info(asic, metrics_v2_2, ARRAY_SIZE(metrics_v2_2), "v2_2.", table);
		break;
	default:
		asic->err_msg("[ERROR]: Unknown Metrics table format: 0x%"PRIx8"\n", header->format_revision);
		return -1;
	}

	return 0;
}


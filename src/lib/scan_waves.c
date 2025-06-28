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

#include <assert.h>
#include <stdbool.h>

#define MANY_TO_INSTANCE(wgp, simd) (((simd) & 3) | ((wgp) << 2))

static void wave_read_regs_via_mmio(struct umr_asic *asic, uint32_t simd,
			   uint32_t wave, uint32_t thread,
			   uint32_t regno, uint32_t num, uint32_t *out)
{
	struct umr_reg *ind_index, *ind_data;
	uint32_t data;

	ind_index = umr_find_reg_data_by_ip_by_instance(asic, "gfx", asic->options.vm_partition, "mmSQ_IND_INDEX");
	ind_data  = umr_find_reg_data_by_ip_by_instance(asic, "gfx", asic->options.vm_partition, "mmSQ_IND_DATA");

	if (ind_index && ind_data) {
		data = umr_bitslice_compose_value(asic, ind_index, "WAVE_ID", wave);
		data |= umr_bitslice_compose_value(asic, ind_index, "INDEX", regno);
		if (asic->family < FAMILY_NV) {
			data |= umr_bitslice_compose_value(asic, ind_index, "THREAD_ID", thread);
			data |= umr_bitslice_compose_value(asic, ind_index, "FORCE_READ", 1);
			data |= umr_bitslice_compose_value(asic, ind_index, "SIMD_ID", simd);
		} else {
			data |= umr_bitslice_compose_value(asic, ind_index, "WORKITEM_ID", thread);
		}
		data |= umr_bitslice_compose_value(asic, ind_index, "AUTO_INCR", 1);
		asic->reg_funcs.write_reg(asic, ind_index->addr * 4, data, REG_MMIO);
		while (num--)
			*(out++) = asic->reg_funcs.read_reg(asic, ind_data->addr * 4, REG_MMIO);
	} else {
		asic->err_msg("[BUG]: The required SQ_IND_{INDEX,DATA} registers are not found on the asic <%s>\n", asic->asicname);
		return;
	}
}

static int read_gpr_mmio_raw(struct umr_asic *asic, int v_or_s,
				   uint32_t thread, uint32_t se, uint32_t sh, uint32_t cu, uint32_t wave, uint32_t simd,
				   uint32_t offset, uint32_t size, uint32_t *dst)
{
	umr_grbm_select_index(asic, se, sh, cu);
	wave_read_regs_via_mmio(asic, simd, wave, thread, offset + (v_or_s ? 0x400 : 0x200), size, dst);
	umr_grbm_select_index(asic, 0xFFFFFFFFUL, 0xFFFFFFFFUL, 0xFFFFFFFFUL);
	return 0;
}

static int read_gpr_mmio(struct umr_asic *asic, int v_or_s, uint32_t thread, struct umr_wave_data *wd, uint32_t *dst)
{
	uint32_t se, sh, cu, wave, simd, size;
	int r = 0;
	uint64_t addr = 0;

	if (asic->family < FAMILY_NV) {
		se = umr_wave_data_get_bits(asic, wd, "ixSQ_WAVE_HW_ID", "SE_ID");
		sh = umr_wave_data_get_bits(asic, wd, "ixSQ_WAVE_HW_ID", "SH_ID");
		cu = umr_wave_data_get_bits(asic, wd, "ixSQ_WAVE_HW_ID", "CU_ID");
		wave = umr_wave_data_get_bits(asic, wd, "ixSQ_WAVE_HW_ID", "WAVE_ID");
		simd = umr_wave_data_get_bits(asic, wd, "ixSQ_WAVE_HW_ID", "SIMD_ID");

		if (v_or_s == 0) {
			uint32_t shift;
			if (asic->family <= FAMILY_CIK)
				shift = 3;  // on SI..CIK allocations were done in 8-dword blocks
			else
				shift = 4;  // on VI allocations are in 16-dword blocks
			size = 4 * ((umr_wave_data_get_bits(asic, wd, "ixSQ_WAVE_GPR_ALLOC", "SGPR_SIZE") + 1) << shift);
		} else {
			size = 4 * ((umr_wave_data_get_bits(asic, wd, "ixSQ_WAVE_GPR_ALLOC", "VGPR_SIZE") + 1) << asic->parameters.vgpr_granularity);
		}
	} else {
		se = umr_wave_data_get_bits(asic, wd, "ixSQ_WAVE_HW_ID1", "SE_ID");
		sh = umr_wave_data_get_bits(asic, wd, "ixSQ_WAVE_HW_ID1", "SA_ID");
		cu = ((umr_wave_data_get_bits(asic, wd, "ixSQ_WAVE_HW_ID1", "WGP_ID") << 2) | umr_wave_data_get_bits(asic, wd, "ixSQ_WAVE_HW_ID1", "SIMD_ID"));
		wave = umr_wave_data_get_bits(asic, wd, "ixSQ_WAVE_HW_ID1", "WAVE_ID");
		simd = 0;
		if (v_or_s == 0) {
			size = 4 * 124; // regular SGPRs, VCC, and TTMPs
		} else {
			size = 4 * ((umr_wave_data_get_bits(asic, wd, "ixSQ_WAVE_GPR_ALLOC", "VGPR_SIZE") + 1) << asic->parameters.vgpr_granularity);
		}
	}

	r = read_gpr_mmio_raw(asic, v_or_s, thread, se, sh, cu, wave, simd, 0, size, dst);
	if (r < 0)
		return r;

	// if we are reading SGPRS then optionally dump them
	// and then read TRAP registers if necessary
	if (asic->options.test_log && asic->options.test_log_fd) {
		int x;

		// we use addr for test logging
		addr =
			((v_or_s ? 0ULL : 1ULL) << 60) | // reading SGPRs
			((uint64_t)0)                  | // starting address to read from
			((uint64_t)se << 12)        |
			((uint64_t)sh << 20)        |
			((uint64_t)cu << 28)        |
			((uint64_t)wave << 36)      |
			((uint64_t)simd << 44)      |
			((uint64_t)thread << 52ULL); // thread_id

		fprintf(asic->options.test_log_fd, "%cGPR@0x%"PRIx64" = { ", "SV"[v_or_s], addr);
		for (x = 0; x < r; x += 4) {
			fprintf(asic->options.test_log_fd, "0x%"PRIx32, dst[x/4]);
			if (x < (r - 4))
				fprintf(asic->options.test_log_fd, ", ");
		}
		fprintf(asic->options.test_log_fd, "}\n");
	}

	if (v_or_s == 0 && size < (4*0x6C)) {
		// read trap if any
		if (umr_wave_data_get_flag_trap_en(asic, wd) || umr_wave_data_get_flag_priv(asic, wd)) {
			r = read_gpr_mmio_raw(asic, v_or_s, thread, se, sh, cu, wave, simd, 4 * 0x6C, size, &dst[0x6C]);
			if (r > 0) {
				if (asic->options.test_log && asic->options.test_log_fd) {
					int x;
					fprintf(asic->options.test_log_fd, "SGPR@0x%"PRIx64" = { ", addr + 0x6C * 4);
					for (x = 0; x < r; x += 4) {
						fprintf(asic->options.test_log_fd, "0x%"PRIx32, dst[0x6C + x/4]);
						if (x < (r - 4))
							fprintf(asic->options.test_log_fd, ", ");
					}
					fprintf(asic->options.test_log_fd, "}\n");
				}
			}
		}
	}

	return r;
}

/**
 * umr_read_sgprs - Read SGPR registers for a specific wave via MMIO
 *
 * @asic: The ASIC to read the SGPRS from
 * @wd: The wave data describing which wave to read the SGPRs from
 * @dst: Where to store the SGPRS
 *
 * Returns the number of words read (or <0 on error).
 */
int umr_read_sgprs_via_mmio(struct umr_asic *asic, struct umr_wave_data *wd, uint32_t *dst)
{
	return read_gpr_mmio(asic, 0, 0, wd, dst);
}

/**
 * umr_read_vgprs - Read VGPR registers for a specific wave via MMIO
 *
 * @asic: The ASIC to read the VGPRS from
 * @wd: The wave data describing which wave to read the VGPRs from
 * @thread: Which thread of the wave to read the VGPRs from
 * @dst: Where to store the VGPRs
 *
 * Returns the number of words read (or <0 on error).
 */
int umr_read_vgprs_via_mmio(struct umr_asic *asic, struct umr_wave_data *wd, uint32_t thread, uint32_t *dst)
{
	// reading VGPR is not supported on pre GFX9 devices
	if (asic->family < FAMILY_AI)
		return -1;

	return read_gpr_mmio(asic, 1, thread, wd, dst);
}

/**
 * umr_get_wave_sq_info_vi - Read some basic SQ information for VI and below ASICs
 *
 * @asic: The ASIC to read from
 * @se: The SE to query
 * @sh: The SH to query
 * @cu: The CU to query
 * @ws: Where to store the SQ information
 *
 * Returns 0 on success, -1 on error.
 */
int umr_get_wave_sq_info_vi(struct umr_asic *asic, unsigned se, unsigned sh, unsigned cu, struct umr_wave_status *ws)
{
	uint32_t value;
	uint64_t index, data;
	struct {
		uint32_t se, sh, instance, use_grbm;
	} grbm;
	struct umr_reg *ind_index, *ind_data;

	ind_index = umr_find_reg_data_by_ip_by_instance(asic, "gfx", asic->options.vm_partition, "mmSQ_IND_INDEX");
	ind_data  = umr_find_reg_data_by_ip_by_instance(asic, "gfx", asic->options.vm_partition, "mmSQ_IND_DATA");

	if (!(ind_index && ind_data)) {
		return -1;
	} else {
		index = ind_index->addr * 4;
		data = ind_data->addr * 4;

		/* copy grbm options to restore later */
		grbm.use_grbm = asic->options.use_bank;
		grbm.se       = asic->options.bank.grbm.se;
		grbm.sh       = asic->options.bank.grbm.sh;
		grbm.instance = asic->options.bank.grbm.instance;

		/* set GRBM banking options */
		asic->options.use_bank           = 1;
		asic->options.bank.grbm.se       = se;
		asic->options.bank.grbm.sh       = sh;
		asic->options.bank.grbm.instance = cu;

		if (!index || !data) {
			asic->err_msg("[BUG]: Cannot find SQ indirect registers on this asic!\n");
			return -1;
		}

		asic->reg_funcs.write_reg(asic, index, 8 << 16, REG_MMIO);
		value = asic->reg_funcs.read_reg(asic, data, REG_MMIO);

		/* restore whatever the user had picked */
		asic->options.use_bank           = grbm.use_grbm;
		asic->options.bank.grbm.se       = grbm.se;
		asic->options.bank.grbm.sh       = grbm.sh;
		asic->options.bank.grbm.instance = grbm.instance;

		/* Did we try to query a non-existing SQ instance? */
		if (value == 0xbebebeef)
			value = 0;

		ws->sq_info.busy = value & 1;
		ws->sq_info.wave_level = (value >> 4) & 0x3F;
		return 0;
	}
}

static uint32_t wave_read_ind(struct umr_asic *asic, uint32_t simd, uint32_t wave, uint32_t address)
{
	struct umr_reg *ind_index, *ind_data;
	uint32_t data;

	ind_index = umr_find_reg_data_by_ip_by_instance(asic, "gfx", asic->options.vm_partition, "mmSQ_IND_INDEX");
	ind_data  = umr_find_reg_data_by_ip_by_instance(asic, "gfx", asic->options.vm_partition, "mmSQ_IND_DATA");

	if (ind_index && ind_data) {
		data = umr_bitslice_compose_value(asic, ind_index, "WAVE_ID", wave);
		data |= umr_bitslice_compose_value(asic, ind_index, "SIMD_ID", simd);
		data |= umr_bitslice_compose_value(asic, ind_index, "INDEX", address);
		data |= umr_bitslice_compose_value(asic, ind_index, "FORCE_READ", 1);
		asic->reg_funcs.write_reg(asic, ind_index->addr * 4, data, REG_MMIO);
		return asic->reg_funcs.read_reg(asic, ind_data->addr * 4, REG_MMIO);
	} else {
		asic->err_msg("[BUG]: The required SQ_IND_{INDEX,DATA} registers are not found on the asic <%s>\n", asic->asicname);
		return -1;
	}
}

static uint32_t wave_read_ind_gfx_10_12(struct umr_asic *asic, uint32_t wave, uint32_t address)
{
	struct umr_reg *ind_index, *ind_data;
	uint32_t data;

	ind_index = umr_find_reg_data_by_ip_by_instance(asic, "gfx", asic->options.vm_partition, "mmSQ_IND_INDEX");
	ind_data  = umr_find_reg_data_by_ip_by_instance(asic, "gfx", asic->options.vm_partition, "mmSQ_IND_DATA");

	if (ind_index && ind_data) {
		data = umr_bitslice_compose_value(asic, ind_index, "WAVE_ID", wave);
		data |= umr_bitslice_compose_value(asic, ind_index, "INDEX", address);
		asic->reg_funcs.write_reg(asic, ind_index->addr * 4, data, REG_MMIO);
		return asic->reg_funcs.read_reg(asic, ind_data->addr * 4, REG_MMIO);
	} else {
		asic->err_msg("[BUG]: The required SQ_IND_{INDEX,DATA} registers are not found on the asic <%s>\n", asic->asicname);
		return -1;
	}
}

int umr_read_wave_status_via_mmio_gfx8_9(struct umr_asic *asic, uint32_t simd, uint32_t wave, uint32_t *dst, int *no_fields)
{
	/* type 0/1 wave data */
	*no_fields = 0;
	dst[(*no_fields)++] = (asic->family <= FAMILY_VI) ? 0 : 1;
	dst[(*no_fields)++] = wave_read_ind(asic, simd, wave, umr_find_reg_by_name(asic, "ixSQ_WAVE_STATUS", NULL)->addr);
	dst[(*no_fields)++] = wave_read_ind(asic, simd, wave, umr_find_reg_by_name(asic, "ixSQ_WAVE_PC_LO", NULL)->addr);
	dst[(*no_fields)++] = wave_read_ind(asic, simd, wave, umr_find_reg_by_name(asic, "ixSQ_WAVE_PC_HI", NULL)->addr);
	dst[(*no_fields)++] = wave_read_ind(asic, simd, wave, umr_find_reg_by_name(asic, "ixSQ_WAVE_EXEC_LO", NULL)->addr);
	dst[(*no_fields)++] = wave_read_ind(asic, simd, wave, umr_find_reg_by_name(asic, "ixSQ_WAVE_EXEC_HI", NULL)->addr);
	dst[(*no_fields)++] = wave_read_ind(asic, simd, wave, umr_find_reg_by_name(asic, "ixSQ_WAVE_HW_ID", NULL)->addr);
	dst[(*no_fields)++] = wave_read_ind(asic, simd, wave, umr_find_reg_by_name(asic, "ixSQ_WAVE_INST_DW0", NULL)->addr);
	dst[(*no_fields)++] = wave_read_ind(asic, simd, wave, umr_find_reg_by_name(asic, "ixSQ_WAVE_INST_DW1", NULL)->addr);
	dst[(*no_fields)++] = wave_read_ind(asic, simd, wave, umr_find_reg_by_name(asic, "ixSQ_WAVE_GPR_ALLOC", NULL)->addr);
	dst[(*no_fields)++] = wave_read_ind(asic, simd, wave, umr_find_reg_by_name(asic, "ixSQ_WAVE_LDS_ALLOC", NULL)->addr);
	dst[(*no_fields)++] = wave_read_ind(asic, simd, wave, umr_find_reg_by_name(asic, "ixSQ_WAVE_TRAPSTS", NULL)->addr);
	dst[(*no_fields)++] = wave_read_ind(asic, simd, wave, umr_find_reg_by_name(asic, "ixSQ_WAVE_IB_STS", NULL)->addr);
	if (asic->family <= FAMILY_VI) {
		dst[(*no_fields)++] = wave_read_ind(asic, simd, wave, umr_find_reg_by_name(asic, "ixSQ_WAVE_TBA_LO", NULL)->addr);
		dst[(*no_fields)++] = wave_read_ind(asic, simd, wave, umr_find_reg_by_name(asic, "ixSQ_WAVE_TBA_HI", NULL)->addr);
		dst[(*no_fields)++] = wave_read_ind(asic, simd, wave, umr_find_reg_by_name(asic, "ixSQ_WAVE_TMA_LO", NULL)->addr);
		dst[(*no_fields)++] = wave_read_ind(asic, simd, wave, umr_find_reg_by_name(asic, "ixSQ_WAVE_TMA_HI", NULL)->addr);
	}
	dst[(*no_fields)++] = wave_read_ind(asic, simd, wave, umr_find_reg_by_name(asic, "ixSQ_WAVE_IB_DBG0", NULL)->addr);
	dst[(*no_fields)++] = wave_read_ind(asic, simd, wave, umr_find_reg_by_name(asic, "ixSQ_WAVE_M0", NULL)->addr);
	dst[(*no_fields)++] = wave_read_ind(asic, simd, wave, umr_find_reg_by_name(asic, "ixSQ_WAVE_MODE", NULL)->addr);

	return 0;
}

int umr_read_wave_status_via_mmio_gfx_10_11(struct umr_asic *asic, uint32_t wave, uint32_t *dst, int *no_fields)
{
	/* type 2 wave data */
	*no_fields = 0;
	dst[(*no_fields)++] = (asic->family == FAMILY_GFX11) ? 3 : 2;
	dst[(*no_fields)++] = wave_read_ind_gfx_10_12(asic, wave, umr_find_reg_by_name(asic, "ixSQ_WAVE_STATUS", NULL)->addr);
	dst[(*no_fields)++] = wave_read_ind_gfx_10_12(asic, wave, umr_find_reg_by_name(asic, "ixSQ_WAVE_PC_LO", NULL)->addr);
	dst[(*no_fields)++] = wave_read_ind_gfx_10_12(asic, wave, umr_find_reg_by_name(asic, "ixSQ_WAVE_PC_HI", NULL)->addr);
	dst[(*no_fields)++] = wave_read_ind_gfx_10_12(asic, wave, umr_find_reg_by_name(asic, "ixSQ_WAVE_EXEC_LO", NULL)->addr);
	dst[(*no_fields)++] = wave_read_ind_gfx_10_12(asic, wave, umr_find_reg_by_name(asic, "ixSQ_WAVE_EXEC_HI", NULL)->addr);
	dst[(*no_fields)++] = wave_read_ind_gfx_10_12(asic, wave, umr_find_reg_by_name(asic, "ixSQ_WAVE_HW_ID1", NULL)->addr);
	dst[(*no_fields)++] = wave_read_ind_gfx_10_12(asic, wave, umr_find_reg_by_name(asic, "ixSQ_WAVE_HW_ID2", NULL)->addr);
	if (asic->family < FAMILY_GFX11)
		dst[(*no_fields)++] = wave_read_ind_gfx_10_12(asic, wave, umr_find_reg_by_name(asic, "ixSQ_WAVE_INST_DW0", NULL)->addr);
	dst[(*no_fields)++] = wave_read_ind_gfx_10_12(asic, wave, umr_find_reg_by_name(asic, "ixSQ_WAVE_GPR_ALLOC", NULL)->addr);
	dst[(*no_fields)++] = wave_read_ind_gfx_10_12(asic, wave, umr_find_reg_by_name(asic, "ixSQ_WAVE_LDS_ALLOC", NULL)->addr);
	dst[(*no_fields)++] = wave_read_ind_gfx_10_12(asic, wave, umr_find_reg_by_name(asic, "ixSQ_WAVE_TRAPSTS", NULL)->addr);
	dst[(*no_fields)++] = wave_read_ind_gfx_10_12(asic, wave, umr_find_reg_by_name(asic, "ixSQ_WAVE_IB_STS", NULL)->addr);
	dst[(*no_fields)++] = wave_read_ind_gfx_10_12(asic, wave, umr_find_reg_by_name(asic, "ixSQ_WAVE_IB_STS2", NULL)->addr);
	dst[(*no_fields)++] = wave_read_ind_gfx_10_12(asic, wave, umr_find_reg_by_name(asic, "ixSQ_WAVE_IB_DBG1", NULL)->addr);
	dst[(*no_fields)++] = wave_read_ind_gfx_10_12(asic, wave, umr_find_reg_by_name(asic, "ixSQ_WAVE_M0", NULL)->addr);
	dst[(*no_fields)++] = wave_read_ind_gfx_10_12(asic, wave, umr_find_reg_by_name(asic, "ixSQ_WAVE_MODE", NULL)->addr);

	return 0;
}


int umr_read_wave_status_via_mmio_gfx_12(struct umr_asic *asic, uint32_t wave, uint32_t *dst, int *no_fields)
{
	dst[(*no_fields)++] = 4;
	dst[(*no_fields)++] = wave_read_ind_gfx_10_12(asic, wave, umr_find_reg_by_name(asic, "ixSQ_WAVE_STATUS", NULL)->addr);
	dst[(*no_fields)++] = wave_read_ind_gfx_10_12(asic, wave, umr_find_reg_by_name(asic, "ixSQ_WAVE_PC_LO", NULL)->addr);
	dst[(*no_fields)++] = wave_read_ind_gfx_10_12(asic, wave, umr_find_reg_by_name(asic, "ixSQ_WAVE_PC_HI", NULL)->addr);
	dst[(*no_fields)++] = wave_read_ind_gfx_10_12(asic, wave, umr_find_reg_by_name(asic, "ixSQ_WAVE_EXEC_LO", NULL)->addr);
	dst[(*no_fields)++] = wave_read_ind_gfx_10_12(asic, wave, umr_find_reg_by_name(asic, "ixSQ_WAVE_EXEC_HI", NULL)->addr);
	dst[(*no_fields)++] = wave_read_ind_gfx_10_12(asic, wave, umr_find_reg_by_name(asic, "ixSQ_WAVE_HW_ID1", NULL)->addr);
	dst[(*no_fields)++] = wave_read_ind_gfx_10_12(asic, wave, umr_find_reg_by_name(asic, "ixSQ_WAVE_HW_ID2", NULL)->addr);
	dst[(*no_fields)++] = wave_read_ind_gfx_10_12(asic, wave, umr_find_reg_by_name(asic, "ixSQ_WAVE_GPR_ALLOC", NULL)->addr);
	dst[(*no_fields)++] = wave_read_ind_gfx_10_12(asic, wave, umr_find_reg_by_name(asic, "ixSQ_WAVE_LDS_ALLOC", NULL)->addr);
	dst[(*no_fields)++] = wave_read_ind_gfx_10_12(asic, wave, umr_find_reg_by_name(asic, "ixSQ_WAVE_IB_STS", NULL)->addr);
	dst[(*no_fields)++] = wave_read_ind_gfx_10_12(asic, wave, umr_find_reg_by_name(asic, "ixSQ_WAVE_IB_STS2", NULL)->addr);
	dst[(*no_fields)++] = wave_read_ind_gfx_10_12(asic, wave, umr_find_reg_by_name(asic, "ixSQ_WAVE_IB_DBG1", NULL)->addr);
	dst[(*no_fields)++] = wave_read_ind_gfx_10_12(asic, wave, umr_find_reg_by_name(asic, "ixSQ_WAVE_M0", NULL)->addr);
	dst[(*no_fields)++] = wave_read_ind_gfx_10_12(asic, wave, umr_find_reg_by_name(asic, "ixSQ_WAVE_MODE", NULL)->addr);
	dst[(*no_fields)++] = wave_read_ind_gfx_10_12(asic, wave, umr_find_reg_by_name(asic, "ixSQ_WAVE_STATE_PRIV", NULL)->addr);
	dst[(*no_fields)++] = wave_read_ind_gfx_10_12(asic, wave, umr_find_reg_by_name(asic, "ixSQ_WAVE_EXCP_FLAG_PRIV", NULL)->addr);
	dst[(*no_fields)++] = wave_read_ind_gfx_10_12(asic, wave, umr_find_reg_by_name(asic, "ixSQ_WAVE_EXCP_FLAG_USER", NULL)->addr);
	dst[(*no_fields)++] = wave_read_ind_gfx_10_12(asic, wave, umr_find_reg_by_name(asic, "ixSQ_WAVE_TRAP_CTRL", NULL)->addr);
	dst[(*no_fields)++] = wave_read_ind_gfx_10_12(asic, wave, umr_find_reg_by_name(asic, "ixSQ_WAVE_ACTIVE", NULL)->addr);
	dst[(*no_fields)++] = wave_read_ind_gfx_10_12(asic, wave, umr_find_reg_by_name(asic, "ixSQ_WAVE_VALID_AND_IDLE", NULL)->addr);
	dst[(*no_fields)++] = wave_read_ind_gfx_10_12(asic, wave, umr_find_reg_by_name(asic, "ixSQ_WAVE_DVGPR_ALLOC_LO", NULL)->addr);
	dst[(*no_fields)++] = wave_read_ind_gfx_10_12(asic, wave, umr_find_reg_by_name(asic, "ixSQ_WAVE_DVGPR_ALLOC_HI", NULL)->addr);
	dst[(*no_fields)++] = wave_read_ind_gfx_10_12(asic, wave, umr_find_reg_by_name(asic, "ixSQ_WAVE_SCHED_MODE", NULL)->addr);

	return 0;
}

/**
 * umr_get_wave_status_via_mmio - Read WAVE STATUS registers via direct MMIO access
 *
 * @asic: The ASIC to query
 * @se: The SE to query
 * @sh: The SH to query
 * @cu: The CU to query
 * @simd: The SIMD to query
 * @wave: The WAVE to query
 * @ws: Where to store the WAVE STATUS register values
 *
 * Returns -1 on error.
 */
int umr_get_wave_status_via_mmio(struct umr_asic *asic, unsigned se, unsigned sh, unsigned cu, unsigned simd, unsigned wave, struct umr_wave_status *ws)
{
	int no_fields, min, maj;
	uint32_t buf[64];

	memset(buf, 0, sizeof buf);

	umr_gfx_get_ip_ver(asic, &maj, &min);
	no_fields = 0;

	umr_grbm_select_index(asic, se, sh, cu);
	switch (maj) {
		case 8:
		case 9: umr_read_wave_status_via_mmio_gfx8_9(asic, simd, wave, buf, &no_fields); break;
		case 10:
		case 11: umr_read_wave_status_via_mmio_gfx_10_11(asic, wave, buf, &no_fields); break;
		case 12:
			umr_read_wave_status_via_mmio_gfx_12(asic, wave, buf, &no_fields); break;
	};
	umr_grbm_select_index(asic, 0xFFFFFFFFUL, 0xFFFFFFFFUL, 0xFFFFFFFFUL);

	if (no_fields > 0)
		return umr_parse_wave_data_gfx(asic, ws, buf, no_fields);
	else
		return -1;
}

/**
 * umr_parse_wave_data_gfx - Parse wave data as returned by the kernel per GFX IP version
 *
 * @asic: The ASIC that was queried
 * @ws: The destination of the wave status data
 * @buf: The raw words from the kernel
 * @nwords: The number of words
 *
 * Returns 0 on success.
 */
int umr_parse_wave_data_gfx(struct umr_asic *asic, struct umr_wave_status *ws, const uint32_t *buf, uint32_t nwords)
{
	int maj, min;
	uint32_t x;

	umr_gfx_get_ip_ver(asic, &maj, &min);
	switch (maj) {
		case 8: if (buf[0] != 0) { asic->err_msg("[ERROR]: Incorrect wave_data for GFX8\n"); return -1; }; break;
		case 9: if (buf[0] != 1) { asic->err_msg("[ERROR]: Incorrect wave_data for GFX9\n"); return -1; }; break;
		case 10: if (buf[0] != 2) { asic->err_msg("[ERROR]: Incorrect wave_data for GFX10\n"); return -1; }; break;
		case 11: if (buf[0] != 3) { asic->err_msg("[ERROR]: Incorrect wave_data for GFX11\n"); return -1; }; break;
		case 12:
		if (buf[0] != 4) { asic->err_msg("[ERROR]: Incorrect wave_data for GFX12\n"); return -1; }; break;
	};
	for (x = 1; x < nwords; x++) {
		ws->reg_values[x - 1] = buf[x];
	}
	return 0;
}

/**
 * umr_scan_wave_slot - Scan a wave slot for register data
 *
 * @asic: The ASIC to query
 * @se: The SE to query
 * @sh: The SH to query
 * @cu: The CU to query
 * @simd: The SIMD to query
 * @wave: The WAVE to query
 * pwd: Where to put the wave data
 *
 * Returns -1 on error, 0 if success but no wave data, 1 if success with wave data.
 */
int umr_scan_wave_slot(struct umr_asic *asic, uint32_t se, uint32_t sh, uint32_t cu,
			       uint32_t simd, uint32_t wave, struct umr_wave_data *pwd)
{
	unsigned thread, num_threads;
	int r;

	if (asic->family <= FAMILY_AI)
		r = asic->wave_funcs.get_wave_status(asic, se, sh, cu, simd, wave, &pwd->ws);
	else
		r = asic->wave_funcs.get_wave_status(asic, se, sh, MANY_TO_INSTANCE(cu, simd), 0, wave, &pwd->ws);

	if (r)
		return -1;

	if (!umr_wave_data_get_flag_valid(asic, pwd) &&
	    (!umr_wave_data_get_flag_halt(asic, pwd) || umr_wave_data_get_value(asic, pwd, "ixSQ_WAVE_STATUS") == 0xbebebeef))
		return 0;

	pwd->se = se;
	pwd->sh = sh;
	pwd->cu = cu;
	pwd->simd = simd;
	pwd->wave = wave;

	if (!asic->options.skip_gprs) {
		asic->gpr_read_funcs.read_sgprs(asic, pwd, &pwd->sgprs[0]);

		if (asic->family <= FAMILY_AI)
			num_threads = 64;
		else
			num_threads = umr_wave_data_get_flag_wave64(asic, pwd) ? 64 : 32;

		pwd->have_vgprs = 1;
		pwd->num_threads = num_threads;
		for (thread = 0; thread < num_threads; ++thread) {
			if (asic->gpr_read_funcs.read_vgprs(asic, pwd, thread,
					   &pwd->vgprs[256 * thread]) < 0) {
				pwd->have_vgprs = 0;
				break;
			}
		}
	} else {
		pwd->have_vgprs = 0;
	}

	return 1;
}

/**
 * umr_scan_wave_simd - Scan for waves within a single SIMD.
 *
 * @asic: The ASIC to query
 * @se: The SE to query
 * @sh: The SH to query
 * @cu: The CU to query
 * @simd: The SIMD to query
 * @cu: the CU instance on <=gfx9, the WGP index on >=gfx10
 * @simd: the SIMD within the CU / WGP
 * @pppwd: points to the pointer-to-pointer-to the last element of a linked
 *              list of wave data structures, with the last element yet to be filled in.
 *              The pointer-to-pointer-to is updated by this function.
 *
 * Returns -1 on error, 0 on success.
 */
static int umr_scan_wave_simd(struct umr_asic *asic, uint32_t se, uint32_t sh, uint32_t cu, uint32_t simd,
			       struct umr_wave_data ***pppwd)
{
	struct umr_ip_block *gfxip = umr_find_ip_block(asic, "gfx", asic->options.vm_partition);
	uint32_t wave, wave_limit;
	int r;

	if (gfxip->discoverable.maj <= 9)
		wave_limit = 10;
	else if (gfxip->discoverable.maj == 10 && gfxip->discoverable.min != 3)
		wave_limit = 20; // Navi1x
	else
		wave_limit = 16; // Navi2+

	for (wave = 0; wave < wave_limit; wave++) {
		struct umr_wave_data *pwd = **pppwd;
		if ((r = umr_scan_wave_slot(asic, se, sh, cu, simd, wave, pwd)) == 1) {
			pwd->next = calloc(1, sizeof(*pwd));
			if (!pwd->next) {
				asic->err_msg("[ERROR]: Out of memory\n");
				return -1;
			}
			pwd->next->reg_names = pwd->reg_names;
			*pppwd = &pwd->next;
		}
		if (r == -1)
			return -1;
	}
	return 0;
}

static const char *gfx8_regs[] = {
	"ixSQ_WAVE_STATUS",
	"ixSQ_WAVE_PC_LO",
	"ixSQ_WAVE_PC_HI",
	"ixSQ_WAVE_EXEC_LO",
	"ixSQ_WAVE_EXEC_HI",
	"ixSQ_WAVE_HW_ID",
	"ixSQ_WAVE_INST_DW0",
	"ixSQ_WAVE_INST_DW1",
	"ixSQ_WAVE_GPR_ALLOC",
	"ixSQ_WAVE_LDS_ALLOC",
	"ixSQ_WAVE_TRAPSTS",
	"ixSQ_WAVE_IB_STS",
	"ixSQ_WAVE_TBA_LO",
	"ixSQ_WAVE_TBA_HI",
	"ixSQ_WAVE_TMA_LO",
	"ixSQ_WAVE_TMA_HI",
	"ixSQ_WAVE_IB_DBG0",
	"ixSQ_WAVE_M0",
	"ixSQ_WAVE_MODE",
	NULL
};

static const char *gfx9_regs[] = {
	"ixSQ_WAVE_STATUS",
	"ixSQ_WAVE_PC_LO",
	"ixSQ_WAVE_PC_HI",
	"ixSQ_WAVE_EXEC_LO",
	"ixSQ_WAVE_EXEC_HI",
	"ixSQ_WAVE_HW_ID",
	"ixSQ_WAVE_INST_DW0",
	"ixSQ_WAVE_INST_DW1",
	"ixSQ_WAVE_GPR_ALLOC",
	"ixSQ_WAVE_LDS_ALLOC",
	"ixSQ_WAVE_TRAPSTS",
	"ixSQ_WAVE_IB_STS",
	"ixSQ_WAVE_IB_DBG0",
	"ixSQ_WAVE_M0",
	"ixSQ_WAVE_MODE",
	NULL
};

static const char *gfx10_regs[] = {
	"ixSQ_WAVE_STATUS",
	"ixSQ_WAVE_PC_LO",
	"ixSQ_WAVE_PC_HI",
	"ixSQ_WAVE_EXEC_LO",
	"ixSQ_WAVE_EXEC_HI",
	"ixSQ_WAVE_HW_ID1",
	"ixSQ_WAVE_HW_ID2",
	"ixSQ_WAVE_INST_DW0",
	"ixSQ_WAVE_GPR_ALLOC",
	"ixSQ_WAVE_LDS_ALLOC",
	"ixSQ_WAVE_TRAPSTS",
	"ixSQ_WAVE_IB_STS",
	"ixSQ_WAVE_IB_STS2",
	"ixSQ_WAVE_IB_DBG1",
	"ixSQ_WAVE_M0",
	"ixSQ_WAVE_MODE",
	NULL
};

static const char *gfx11_regs[] = {
	"ixSQ_WAVE_STATUS",
	"ixSQ_WAVE_PC_LO",
	"ixSQ_WAVE_PC_HI",
	"ixSQ_WAVE_EXEC_LO",
	"ixSQ_WAVE_EXEC_HI",
	"ixSQ_WAVE_HW_ID1",
	"ixSQ_WAVE_HW_ID2",
	"ixSQ_WAVE_GPR_ALLOC",
	"ixSQ_WAVE_LDS_ALLOC",
	"ixSQ_WAVE_TRAPSTS",
	"ixSQ_WAVE_IB_STS",
	"ixSQ_WAVE_IB_STS2",
	"ixSQ_WAVE_IB_DBG1",
	"ixSQ_WAVE_M0",
	"ixSQ_WAVE_MODE",
	NULL
};

static const char *gfx12_regs[] = {
	"ixSQ_WAVE_STATUS",
	"ixSQ_WAVE_PC_LO",
	"ixSQ_WAVE_PC_HI",
	"ixSQ_WAVE_EXEC_LO",
	"ixSQ_WAVE_EXEC_HI",
	"ixSQ_WAVE_HW_ID1",
	"ixSQ_WAVE_HW_ID2",
	"ixSQ_WAVE_GPR_ALLOC",
	"ixSQ_WAVE_LDS_ALLOC",
	"ixSQ_WAVE_IB_STS",
	"ixSQ_WAVE_IB_STS2",
	"ixSQ_WAVE_IB_DBG1",
	"ixSQ_WAVE_M0",
	"ixSQ_WAVE_MODE",
	"ixSQ_WAVE_STATE_PRIV",
	"ixSQ_WAVE_EXCP_FLAG_PRIV",
	"ixSQ_WAVE_EXCP_FLAG_USER",
	"ixSQ_WAVE_TRAP_CTRL",
	"ixSQ_WAVE_ACTIVE",
	"ixSQ_WAVE_VALID_AND_IDLE",
	"ixSQ_WAVE_DVGPR_ALLOC_LO",
	"ixSQ_WAVE_DVGPR_ALLOC_HI",
	"ixSQ_WAVE_SCHED_MODE",
	NULL
};

/**
 * umr_wave_data_init - Initialize a umr_wave_data structure per GFX IP version
 *
 * @asic: The ASIC this wave data will be used for
 * @wd: The initialized structure.
 *
 * Returns -1 on error, 0 on success.
 */
int umr_wave_data_init(struct umr_asic *asic, struct umr_wave_data *wd)
{
	int maj, min;

	memset(wd, 0, sizeof(*wd));
	umr_gfx_get_ip_ver(asic, &maj, &min);
	switch (maj) {
		case 8:
			wd->reg_names = gfx8_regs;
			break;
		case 9:
			wd->reg_names = gfx9_regs;
			break;
		case 10:
			wd->reg_names = gfx10_regs;
			break;
		case 11:
			wd->reg_names = gfx11_regs;
			break;
		case 12:
			wd->reg_names = gfx12_regs;
			break;
		default:
			return -1;
	}
	return 0;
}

/**
 * umr_scan_wave_data - Scan for any halted valid waves
 *
 * Returns NULL on error (or no waves found).
 */
struct umr_wave_data *umr_scan_wave_data(struct umr_asic *asic)
{
	uint32_t se, sh, simd;
	struct umr_wave_data *ohead, *head, **ptail;
	int r;

	ohead = head = malloc(sizeof *head);
	if (!head) {
		asic->err_msg("[ERROR]: Out of memory\n");
		return NULL;
	}
	ptail = &head;

	if (umr_wave_data_init(asic, ohead) < 0) {
		asic->err_msg("[BUG]: Unsupported ASIC IP version in umr_scan_wave_data()\n");
		free(ohead);
		return NULL;
	}

	for (se = 0; se < asic->config.gfx.max_shader_engines; se++)
	for (sh = 0; sh < asic->config.gfx.max_sh_per_se; sh++) {
		if (asic->family <= FAMILY_AI) {
			for (uint32_t cu = 0; cu < asic->config.gfx.max_cu_per_sh; cu++) {
				asic->wave_funcs.get_wave_sq_info(asic, se, sh, cu, &(*ptail)->ws);
				if ((*ptail)->ws.sq_info.busy) {
					for (simd = 0; simd < 4; simd++) {
						r = umr_scan_wave_simd(asic, se, sh, cu, simd, &ptail);
						if (r < 0)
							goto error;
					}
				}
			}
		} else {
			for (uint32_t wgp = 0; wgp < asic->config.gfx.max_cu_per_sh / 2; wgp++)
			for (simd = 0; simd < 4; simd++) {
				asic->wave_funcs.get_wave_sq_info(asic, se, sh, MANY_TO_INSTANCE(wgp, simd), &(*ptail)->ws);
				if ((*ptail)->ws.sq_info.busy) {
					r = umr_scan_wave_simd(asic, se, sh, wgp, simd, &ptail);
					if (r < 0)
						goto error;
				}
			}
		}
	}

	// drop the pre-allocated tail node
	free(*ptail);
	*ptail = NULL;
	return head;
error:
	while (ohead) {
		head = ohead->next;
		free(ohead);
		ohead = head;
	}
	return NULL;
}

/**
 * umr_wave_data_get_value - return one of the WAVE STATUS registers
 *
 * @asic: The ASIC these registers are from
 * @wd: The WAVE STATUS data that has been captured
 * @regname: Which register to read.
 *
 * Returns 0xDEADBEEF if the register is not found, otherwise the value.
 */
uint32_t umr_wave_data_get_value(struct umr_asic *asic, struct umr_wave_data *wd, const char *regname)
{
	int x;
	for (x = 0; wd->reg_names[x]; x++) {
		if (!strcmp(wd->reg_names[x], regname)) {
			return wd->ws.reg_values[x];
		}
	}
	asic->err_msg("[BUG]: Register (%s) not found in umr_wave_data list for this ASIC\n", regname);
	return 0xDEADBEEF;
}

/**
 * umr_wave_data_get_bits - return a bit slice of one ofthe WAVE STATUS registers
 *
 * @asic: The ASIC these registers are from
 * @wd: The WAVE STATUS data that has been captured
 * @regname: Which register to read.
 * @bitname: Which bitslice to return of the register.
 *
 * Returns 0xDEADBEEF if the register is not found, otherwise the value.
 */
uint32_t umr_wave_data_get_bits(struct umr_asic *asic, struct umr_wave_data *wd, const char *regname, const char *bitname)
{
	uint32_t value;

	value = umr_wave_data_get_value(asic, wd, regname);
	if (value == 0xDEADBEEF) {
		return 0xDEADBEEF;
	}
	return umr_bitslice_reg_by_name_by_ip_by_instance(asic, "gfx", asic->options.vm_partition, (char*)regname, (char*)bitname, value);
}

/**
 * umr_wave_data_get_bit_info - Retrieve bitfield information for a WAVE STATUS registers
 *
 * @asic: The ASIC these registers are from
 * @wd: The WAVE STATUS data that has been captured
 * @regname: Which register to inspect
 * @no_bits: The number of bitfields in the register are stored here
 * @bits: A pointer to the umr_bitfield struct array is stored here
 *
 * Returns 0 on success, -1 on error
 */
int umr_wave_data_get_bit_info(struct umr_asic *asic, struct umr_wave_data *wd, const char *regname, int *no_bits, struct umr_bitfield **bits)
{
	struct umr_reg *reg;
	int x;

	for (x = 0; wd->reg_names[x]; x++) {
		if (!strcmp(wd->reg_names[x], regname)) {
			break;
		}
	}

	if (wd->reg_names[x] == NULL) {
		asic->err_msg("[BUG]: Register [%s] not found in gfx IP\n", regname);
		*no_bits = 0;
		*bits = NULL;
		return -2;
	}
	reg = umr_find_reg_data_by_ip_by_instance(asic, "gfx", asic->options.vm_partition, wd->reg_names[x]);
	if (reg) {
		*no_bits = reg->no_bits;
		*bits = reg->bits;
		return 0;
	} else {
		asic->err_msg("[BUG]: Register [%s] not found in gfx IP\n", regname);
		*no_bits = 0;
		*bits = NULL;
		return -1;
	}
}

/**
 * umr_wave_data_get_flag_valid - return the VALID bit for a given wave
 *
 * @asic: The ASIC the wave is from
 * @wd: The wave data to get the valid flag from
 *
 * Returns 0/1 based on the VALID bits.  -1 on error.
 */
int umr_wave_data_get_flag_valid(struct umr_asic *asic, struct umr_wave_data *wd)
{
	int maj, min;
	umr_gfx_get_ip_ver(asic, &maj, &min);
	switch (maj) {
		case 6:
		case 7:
		case 8:
		case 9:
		case 10:
		case 11:
		case 12:
			return umr_wave_data_get_bits(asic, wd, "ixSQ_WAVE_STATUS", "VALID");
	}
	return -1;
}

/**
 * umr_wave_data_get_flag_trap_en - return the TRAP_EN bit for a given wave
 *
 * @asic: The ASIC the wave is from
 * @wd: The wave data to get the trap enabled flag from
 *
 * Returns 0/1 based on the TRAP_EN bits.  -1 on error.
 */
int umr_wave_data_get_flag_trap_en(struct umr_asic *asic, struct umr_wave_data *wd)
{
	int maj, min;
	umr_gfx_get_ip_ver(asic, &maj, &min);
	switch (maj) {
		case 6:
		case 7:
		case 8:
		case 9:
		case 10:
		case 11:
		case 12:
			return umr_wave_data_get_bits(asic, wd, "ixSQ_WAVE_STATUS", "TRAP_EN");
	}
	return -1;
}

/**
 * umr_wave_data_get_flag_halt - return the HALT bit for a given wave
 *
 * @asic: The ASIC the wave is from
 * @wd: The wave data to get the halt flag from
 *
 * Returns 0/1 based on the HALT bits.  -1 on error.
 */
int umr_wave_data_get_flag_halt(struct umr_asic *asic, struct umr_wave_data *wd)
{
	int maj, min;
	umr_gfx_get_ip_ver(asic, &maj, &min);
	switch (maj) {
		case 6:
		case 7:
		case 8:
		case 9:
		case 10:
		case 11:
			return umr_wave_data_get_bits(asic, wd, "ixSQ_WAVE_STATUS", "HALT");
		case 12:
			return umr_wave_data_get_bits(asic, wd, "ixSQ_WAVE_STATE_PRIV", "HALT");
	}
	return -1;
}

/**
 * umr_wave_data_get_flag_fatal_halt - return the FATAL_HALT bit for a given wave
 *
 * @asic: The ASIC the wave is from
 * @wd: The wave data to get the fatal halt flag from
 *
 * Returns 0/1 based on the FATAL_HALT bits.  -1 on error.
 */
int umr_wave_data_get_flag_fatal_halt(struct umr_asic *asic, struct umr_wave_data *wd)
{
	int maj, min;
	umr_gfx_get_ip_ver(asic, &maj, &min);
	switch (maj) {
		case 6:
		case 7:
		case 8:
		case 9:
		case 10:
		case 11:
		case 12:
			return umr_wave_data_get_bits(asic, wd, "ixSQ_WAVE_STATUS", "FATAL_HALT");
	}
	return -1;
}

/**
 * umr_wave_data_get_flag_priv - return the PRIV bit for a given wave
 *
 * @asic: The ASIC the wave is from
 * @wd: The wave data to get the priv flag from
 *
 * Returns 0/1 based on the PRIV bits.  -1 on error.
 */
int umr_wave_data_get_flag_priv(struct umr_asic *asic, struct umr_wave_data *wd)
{
	int maj, min;
	umr_gfx_get_ip_ver(asic, &maj, &min);
	switch (maj) {
		case 6:
		case 7:
		case 8:
		case 9:
		case 10:
		case 11:
		case 12:
			return umr_wave_data_get_bits(asic, wd, "ixSQ_WAVE_STATUS", "PRIV");
	}
	return -1;
}

/**
 * umr_wave_data_get_flag_wave64 - return the WAVE64 bit for a given wave
 *
 * @asic: The ASIC the wave is from
 * @wd: The wave data to get the wave64 flag from
 *
 * Returns 0/1 based on the WAVE64 bits.  -1 on error.
 */
int umr_wave_data_get_flag_wave64(struct umr_asic *asic, struct umr_wave_data *wd)
{
	int maj, min;
	umr_gfx_get_ip_ver(asic, &maj, &min);
	switch (maj) {
		case 6:
		case 7:
		case 8:
		case 9:
		case 10:
		case 11:
			return umr_wave_data_get_bits(asic, wd, "ixSQ_WAVE_IB_STS2", "WAVE64");
		case 12:
			return umr_wave_data_get_bits(asic, wd, "ixSQ_WAVE_STATUS", "WAVE64");
	}
	return -1;
}

/**
 * umr_wave_data_get_shader_pc_vmid - return the PC and VMID values for a given wave's shader
 *
 * @asic: The ASIC the wave is from
 * @wd: The wave data to get the valid flag from
 * @vmid: The VMID of the shader is stored here
 * @addr: The PC value of the shader is stored here.
 *
 * Returns 0 on success, -1 on error.
 */
int umr_wave_data_get_shader_pc_vmid(struct umr_asic *asic, struct umr_wave_data *wd, uint32_t *vmid, uint64_t *addr)
{
	int maj, min;
	umr_gfx_get_ip_ver(asic, &maj, &min);
	switch (maj) {
		case 6:
		case 7:
		case 8:
		case 9:
			*addr = umr_wave_data_get_value(asic, wd, "ixSQ_WAVE_PC_LO") | ((uint64_t)umr_wave_data_get_value(asic, wd, "ixSQ_WAVE_PC_HI") << 32ULL);
			*vmid = umr_wave_data_get_bits(asic, wd, "ixSQ_WAVE_HW_ID", "VM_ID");
			return 0;
		case 10:
		case 11:
		case 12:
			*addr = umr_wave_data_get_value(asic, wd, "ixSQ_WAVE_PC_LO") | ((uint64_t)umr_wave_data_get_value(asic, wd, "ixSQ_WAVE_PC_HI") << 32ULL);
			*vmid = umr_wave_data_get_bits(asic, wd, "ixSQ_WAVE_HW_ID2", "VM_ID");
			return 0;
	}
	return -1;
}

/**
 * umr_wave_data_get_flag_simd_id - return the SIMD_ID bit for a given wave
 *
 * @asic: The ASIC the wave is from
 * @wd: The wave data to get the simd id flag from
 *
 * Returns 0/1 based on the SIMD_ID bits.  -1 on error.
 */
int umr_wave_data_get_flag_simd_id(struct umr_asic *asic, struct umr_wave_data *wd)
{
	int maj, min;
	umr_gfx_get_ip_ver(asic, &maj, &min);
	switch (maj) {
		case 6:
		case 7:
		case 8:
		case 9:
			return umr_wave_data_get_bits(asic, wd, "ixSQ_WAVE_HW_ID", "SIMD_ID");
		case 10:
		case 11:
		case 12:
			return umr_wave_data_get_bits(asic, wd, "ixSQ_WAVE_HW_ID1", "SIMD_ID");
	}
	return -1;
}

/**
 * umr_wave_data_get_flag_wave_id - return the WAVE_ID bit for a given wave
 *
 * @asic: The ASIC the wave is from
 * @wd: The wave data to get the wave id flag from
 *
 * Returns 0/1 based on the WAVE_ID bits.  -1 on error.
 */
int umr_wave_data_get_flag_wave_id(struct umr_asic *asic, struct umr_wave_data *wd)
{
	int maj, min;
	umr_gfx_get_ip_ver(asic, &maj, &min);
	switch (maj) {
		case 6:
		case 7:
		case 8:
		case 9:
			return umr_wave_data_get_bits(asic, wd, "ixSQ_WAVE_HW_ID", "WAVE_ID");
		case 10:
		case 11:
		case 12:
			return umr_wave_data_get_bits(asic, wd, "ixSQ_WAVE_HW_ID1", "WAVE_ID");
	}
	return -1;
}

/**
 * umr_wave_data_num_of_sgprs - return the SIMD_ID bit for a given wave
 *
 * @asic: The ASIC the wave is from
 * @wd: The wave data to get the simd id flag from
 *
 * Returns 0/1 based on the SIMD_ID bits.  -1 on error.
 */
uint32_t umr_wave_data_num_of_sgprs(struct umr_asic *asic, struct umr_wave_data *wd)
{
	int maj, min;
	umr_gfx_get_ip_ver(asic, &maj, &min);
	switch (maj) {
		case 6:
		case 7: return (umr_wave_data_get_bits(asic, wd, "ixSQ_WAVE_GPR_ALLOC", "SGPR_SIZE")) << 3;
		case 8:
		case 9: return (umr_wave_data_get_bits(asic, wd, "ixSQ_WAVE_GPR_ALLOC", "SGPR_SIZE")) << 4;
		case 10:
		case 11:
		case 12: // TODO: confirm
			return 124;
	}
	return 0;
}

/** umr_wave_data_describe_wavefront - Produce a formatted string that describes this specific wave
 *
 * @asic: The ASIC this wave is from
 * @wd: The WAVE data that needs a description
 *
 * Returns a formatted string that describes to the user where this wave came from.
 * The string can be freed from the heap with free().
 */
char *umr_wave_data_describe_wavefront(struct umr_asic *asic, struct umr_wave_data *wd)
{
	int maj, min;
	char str[256];
	umr_gfx_get_ip_ver(asic, &maj, &min);
	memset(str, 0, sizeof str);
	switch (maj) {
		case 6:
		case 7:
		case 8:
		case 9:
			snprintf(str, sizeof(str)-1, "se%" PRIu32 ".sh%" PRIu32 ".cu%" PRIu32 ".simd%" PRIu32 ".wave%" PRIu32,
				wd->se, wd->sh, wd->cu,
				umr_wave_data_get_bits(asic, wd, "ixSQ_WAVE_HW_ID", "SIMD_ID"),
				umr_wave_data_get_bits(asic, wd, "ixSQ_WAVE_HW_ID", "WAVE_ID"));
			break;
		case 10:
		case 11:
		case 12:
			snprintf(str, sizeof(str)-1, "se%" PRIu32 ".sa%" PRIu32 ".wgp%" PRIu32 ".simd%" PRIu32 ".wave%" PRIu32,
				wd->se, wd->sh, wd->cu,
				umr_wave_data_get_bits(asic, wd, "ixSQ_WAVE_HW_ID1", "SIMD_ID"),
				umr_wave_data_get_bits(asic, wd, "ixSQ_WAVE_HW_ID1", "WAVE_ID"));
			break;
	}
	return strdup(str);
}

/**
 * umr_singlestep_wave - Try to issue a single step to a halted wave
 *
 * @asic: The ASIC where the wave is halted
 * @wd: The wave data associated with the wave in question
 *
 * Returns -1 on error.
 */

int umr_singlestep_wave(struct umr_asic *asic, struct umr_wave_data *wd)
{
	int r = 1;
	int skip_gprs = asic->options.skip_gprs;
	int verbose = asic->options.verbose;

	asic->options.skip_gprs = 0;
	asic->options.verbose = 0;

	uint64_t pc, new_pc;
	uint32_t vmid;

	if (umr_wave_data_get_shader_pc_vmid(asic, wd, &vmid, &pc)) {
		return -1;
	}

	// Send the single-step command in a limited retry loop because a small number of
	// single-step commands are required before an instruction is actually issued after
	// a branch.
	int retry = 0;
	for (; r == 1 && retry < 5; ++retry) {
		umr_sq_cmd_singlestep(asic, wd->se, wd->sh, wd->cu, wd->simd, wd->wave);

		struct umr_wave_data new_wd;
		umr_wave_data_init(asic, &new_wd);

		r = umr_scan_wave_slot(asic, wd->se, wd->sh, wd->cu, wd->simd, wd->wave, &new_wd);
		if (r < 0) {
			r = -2;
			goto out;
		}

		if (umr_wave_data_get_shader_pc_vmid(asic, &new_wd, &vmid, &new_pc)) {
			return -1;
		}
		bool moved = pc != new_pc;
		memcpy(wd, &new_wd, sizeof(new_wd));
		if (moved)
			break;
	}
	if (retry == 5)
		r = -1;

	out:
	asic->options.skip_gprs = skip_gprs;
	asic->options.verbose = verbose;
	return r;
}

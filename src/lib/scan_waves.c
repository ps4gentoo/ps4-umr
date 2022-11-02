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

#include <stdbool.h>

#define MANY_TO_INSTANCE(wgp, simd) (((simd) & 3) | ((wgp) << 2))

int umr_get_wave_sq_info_vi(struct umr_asic *asic, unsigned se, unsigned sh, unsigned cu, struct umr_wave_status *ws)
{
	uint32_t value;
	uint64_t index, data;
	struct {
		uint32_t se, sh, instance, use_grbm;
	} grbm;

	index = umr_find_reg(asic, "mmSQ_IND_INDEX") * 4;
	data = umr_find_reg(asic, "mmSQ_IND_DATA") * 4;

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

static uint32_t wave_read_ind(struct umr_asic *asic, uint32_t simd, uint32_t wave, uint32_t address)
{
	struct umr_reg *ind_index, *ind_data;
	uint32_t data;

	ind_index = umr_find_reg_data(asic, "mmSQ_IND_INDEX");
	ind_data  = umr_find_reg_data(asic, "mmSQ_IND_DATA");

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

static uint32_t wave_read_ind_nv(struct umr_asic *asic, uint32_t wave, uint32_t address)
{
	struct umr_reg *ind_index, *ind_data;
	uint32_t data;

	ind_index = umr_find_reg_data(asic, "mmSQ_IND_INDEX");
	ind_data  = umr_find_reg_data(asic, "mmSQ_IND_DATA");

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
	dst[(*no_fields)++] = wave_read_ind(asic, simd, wave, umr_find_reg_data(asic, "ixSQ_WAVE_STATUS")->addr);
	dst[(*no_fields)++] = wave_read_ind(asic, simd, wave, umr_find_reg_data(asic, "ixSQ_WAVE_PC_LO")->addr);
	dst[(*no_fields)++] = wave_read_ind(asic, simd, wave, umr_find_reg_data(asic, "ixSQ_WAVE_PC_HI")->addr);
	dst[(*no_fields)++] = wave_read_ind(asic, simd, wave, umr_find_reg_data(asic, "ixSQ_WAVE_EXEC_LO")->addr);
	dst[(*no_fields)++] = wave_read_ind(asic, simd, wave, umr_find_reg_data(asic, "ixSQ_WAVE_EXEC_HI")->addr);
	dst[(*no_fields)++] = wave_read_ind(asic, simd, wave, umr_find_reg_data(asic, "ixSQ_WAVE_HW_ID")->addr);
	dst[(*no_fields)++] = wave_read_ind(asic, simd, wave, umr_find_reg_data(asic, "ixSQ_WAVE_INST_DW0")->addr);
	dst[(*no_fields)++] = wave_read_ind(asic, simd, wave, umr_find_reg_data(asic, "ixSQ_WAVE_INST_DW1")->addr);
	dst[(*no_fields)++] = wave_read_ind(asic, simd, wave, umr_find_reg_data(asic, "ixSQ_WAVE_GPR_ALLOC")->addr);
	dst[(*no_fields)++] = wave_read_ind(asic, simd, wave, umr_find_reg_data(asic, "ixSQ_WAVE_LDS_ALLOC")->addr);
	dst[(*no_fields)++] = wave_read_ind(asic, simd, wave, umr_find_reg_data(asic, "ixSQ_WAVE_TRAPSTS")->addr);
	dst[(*no_fields)++] = wave_read_ind(asic, simd, wave, umr_find_reg_data(asic, "ixSQ_WAVE_IB_STS")->addr);
	if (asic->family <= FAMILY_VI) {
		dst[(*no_fields)++] = wave_read_ind(asic, simd, wave, umr_find_reg_data(asic, "ixSQ_WAVE_TBA_LO")->addr);
		dst[(*no_fields)++] = wave_read_ind(asic, simd, wave, umr_find_reg_data(asic, "ixSQ_WAVE_TBA_HI")->addr);
		dst[(*no_fields)++] = wave_read_ind(asic, simd, wave, umr_find_reg_data(asic, "ixSQ_WAVE_TMA_LO")->addr);
		dst[(*no_fields)++] = wave_read_ind(asic, simd, wave, umr_find_reg_data(asic, "ixSQ_WAVE_TMA_HI")->addr);
	}
	dst[(*no_fields)++] = wave_read_ind(asic, simd, wave, umr_find_reg_data(asic, "ixSQ_WAVE_IB_DBG0")->addr);
	dst[(*no_fields)++] = wave_read_ind(asic, simd, wave, umr_find_reg_data(asic, "ixSQ_WAVE_M0")->addr);
	dst[(*no_fields)++] = wave_read_ind(asic, simd, wave, umr_find_reg_data(asic, "ixSQ_WAVE_MODE")->addr);

	return 0;
}

int umr_read_wave_status_via_mmio_gfx10(struct umr_asic *asic, uint32_t wave, uint32_t *dst, int *no_fields)
{
	/* type 2 wave data */
	*no_fields = 0;
	dst[(*no_fields)++] = 2;
	dst[(*no_fields)++] = wave_read_ind_nv(asic, wave, umr_find_reg_data(asic, "ixSQ_WAVE_STATUS")->addr);
	dst[(*no_fields)++] = wave_read_ind_nv(asic, wave, umr_find_reg_data(asic, "ixSQ_WAVE_PC_LO")->addr);
	dst[(*no_fields)++] = wave_read_ind_nv(asic, wave, umr_find_reg_data(asic, "ixSQ_WAVE_PC_HI")->addr);
	dst[(*no_fields)++] = wave_read_ind_nv(asic, wave, umr_find_reg_data(asic, "ixSQ_WAVE_EXEC_LO")->addr);
	dst[(*no_fields)++] = wave_read_ind_nv(asic, wave, umr_find_reg_data(asic, "ixSQ_WAVE_EXEC_HI")->addr);
	dst[(*no_fields)++] = wave_read_ind_nv(asic, wave, umr_find_reg_data(asic, "ixSQ_WAVE_HW_ID1")->addr);
	dst[(*no_fields)++] = wave_read_ind_nv(asic, wave, umr_find_reg_data(asic, "ixSQ_WAVE_HW_ID2")->addr);
	dst[(*no_fields)++] = wave_read_ind_nv(asic, wave, umr_find_reg_data(asic, "ixSQ_WAVE_INST_DW0")->addr);
	dst[(*no_fields)++] = wave_read_ind_nv(asic, wave, umr_find_reg_data(asic, "ixSQ_WAVE_GPR_ALLOC")->addr);
	dst[(*no_fields)++] = wave_read_ind_nv(asic, wave, umr_find_reg_data(asic, "ixSQ_WAVE_LDS_ALLOC")->addr);
	dst[(*no_fields)++] = wave_read_ind_nv(asic, wave, umr_find_reg_data(asic, "ixSQ_WAVE_TRAPSTS")->addr);
	dst[(*no_fields)++] = wave_read_ind_nv(asic, wave, umr_find_reg_data(asic, "ixSQ_WAVE_IB_STS")->addr);
	dst[(*no_fields)++] = wave_read_ind_nv(asic, wave, umr_find_reg_data(asic, "ixSQ_WAVE_IB_STS2")->addr);
	dst[(*no_fields)++] = wave_read_ind_nv(asic, wave, umr_find_reg_data(asic, "ixSQ_WAVE_IB_DBG1")->addr);
	dst[(*no_fields)++] = wave_read_ind_nv(asic, wave, umr_find_reg_data(asic, "ixSQ_WAVE_M0")->addr);
	dst[(*no_fields)++] = wave_read_ind_nv(asic, wave, umr_find_reg_data(asic, "ixSQ_WAVE_MODE")->addr);

	return 0;
}

static int umr_parse_wave_data_gfx_8(struct umr_asic *asic, struct umr_wave_status *ws, const uint32_t *buf)
{
	struct umr_reg *reg;
	uint32_t value;
	int x;

	if (buf[0] != 0) {
		asic->err_msg("[ERROR]: Was expecting type 0 wave data on a CZ/VI part!\n");
		return -1;
	}

	x = 1;
	ws->wave_status.value = value = buf[x++];
		reg = umr_find_reg_data(asic, "ixSQ_WAVE_STATUS");
		ws->wave_status.scc = umr_bitslice_reg(asic, reg, "SCC", value);
		ws->wave_status.priv = umr_bitslice_reg(asic, reg, "PRIV", value);
		ws->wave_status.execz = umr_bitslice_reg(asic, reg, "EXECZ", value);
		ws->wave_status.vccz  = umr_bitslice_reg(asic, reg, "VCCZ", value);
		ws->wave_status.in_tg = umr_bitslice_reg(asic, reg, "IN_TG", value);
		ws->wave_status.halt = umr_bitslice_reg(asic, reg, "HALT", value);
		ws->wave_status.valid = umr_bitslice_reg(asic, reg, "VALID", value);
		ws->wave_status.spi_prio = umr_bitslice_reg(asic, reg, "SPI_PRIO", value);
		ws->wave_status.wave_prio = umr_bitslice_reg(asic, reg, "USER_PRIO", value);
		ws->wave_status.trap_en = umr_bitslice_reg(asic, reg, "TRAP_EN", value);
		ws->wave_status.ttrace_en = umr_bitslice_reg(asic, reg, "TTRACE_EN", value);
		ws->wave_status.export_rdy = umr_bitslice_reg(asic, reg, "EXPORT_RDY", value);
		ws->wave_status.in_barrier = umr_bitslice_reg(asic, reg, "IN_BARRIER", value);
		ws->wave_status.trap = umr_bitslice_reg(asic, reg, "TRAP", value);
		ws->wave_status.ecc_err = umr_bitslice_reg(asic, reg, "ECC_ERR", value);
		ws->wave_status.skip_export = umr_bitslice_reg(asic, reg, "SKIP_EXPORT", value);
		ws->wave_status.perf_en = umr_bitslice_reg(asic, reg, "PERF_EN", value);
		ws->wave_status.cond_dbg_user = (value >> 0x14) & 1;
		ws->wave_status.cond_dbg_sys = (value >> 0x15) & 1;
		ws->wave_status.data_atc = (value >> 0x16) & 1;
		ws->wave_status.inst_atc = (value >> 0x17) & 1;
		ws->wave_status.dispatch_cache_ctrl = (value >> 0x18) & 3;
		ws->wave_status.must_export = umr_bitslice_reg(asic, reg, "MUST_EXPORT", value);

	ws->pc_lo = buf[x++];
	ws->pc_hi = buf[x++];
	ws->exec_lo = buf[x++];
	ws->exec_hi = buf[x++];

	ws->hw_id.value = value = buf[x++];
		reg = umr_find_reg_data(asic, "ixSQ_WAVE_HW_ID");
		ws->hw_id.wave_id = umr_bitslice_reg(asic, reg, "WAVE_ID", value);
		ws->hw_id.simd_id = umr_bitslice_reg(asic, reg, "SIMD_ID", value);
		ws->hw_id.pipe_id = umr_bitslice_reg(asic, reg, "PIPE_ID", value);
		ws->hw_id.cu_id   = umr_bitslice_reg(asic, reg, "CU_ID", value);
		ws->hw_id.sh_id   = umr_bitslice_reg(asic, reg, "SH_ID", value);
		ws->hw_id.se_id   = umr_bitslice_reg(asic, reg, "SE_ID", value);
		ws->hw_id.tg_id   = umr_bitslice_reg(asic, reg, "TG_ID", value);
		ws->hw_id.vm_id   = umr_bitslice_reg(asic, reg, "VM_ID", value);
		ws->hw_id.queue_id = umr_bitslice_reg(asic, reg, "QUEUE_ID", value);
		ws->hw_id.state_id = umr_bitslice_reg(asic, reg, "STATE_ID", value);
		ws->hw_id.me_id    = umr_bitslice_reg(asic, reg, "ME_ID", value);

	ws->wave_inst_dw0 = buf[x++];
	ws->wave_inst_dw1 = buf[x++];

	ws->gpr_alloc.value = value = buf[x++];
		reg = umr_find_reg_data(asic, "ixSQ_WAVE_GPR_ALLOC");
		ws->gpr_alloc.vgpr_base = umr_bitslice_reg(asic, reg, "VGPR_BASE", value);
		ws->gpr_alloc.vgpr_size = umr_bitslice_reg(asic, reg, "VGPR_SIZE", value);
		ws->gpr_alloc.sgpr_base = umr_bitslice_reg(asic, reg, "SGPR_BASE", value);
		ws->gpr_alloc.sgpr_size = umr_bitslice_reg(asic, reg, "SGPR_SIZE", value);

	ws->lds_alloc.value = value = buf[x++];
		reg = umr_find_reg_data(asic, "ixSQ_WAVE_LDS_ALLOC");
		ws->lds_alloc.lds_base = umr_bitslice_reg(asic, reg, "LDS_BASE", value);
		ws->lds_alloc.lds_size = umr_bitslice_reg(asic, reg, "LDS_SIZE", value);

	ws->trapsts.value = value = buf[x++];
		reg = umr_find_reg_data(asic, "ixSQ_WAVE_TRAPSTS");
		ws->trapsts.excp = umr_bitslice_reg(asic, reg, "EXCP", value);
		ws->trapsts.excp_cycle = umr_bitslice_reg(asic, reg, "EXCP_CYCLE", value);
		ws->trapsts.dp_rate = umr_bitslice_reg(asic, reg, "DP_RATE", value);

	ws->ib_sts.value = value = buf[x++];
		reg = umr_find_reg_data(asic, "ixSQ_WAVE_IB_STS");
		ws->ib_sts.vm_cnt = umr_bitslice_reg(asic, reg, "VM_CNT", value);
		ws->ib_sts.exp_cnt = umr_bitslice_reg(asic, reg, "EXP_CNT", value);
		ws->ib_sts.lgkm_cnt = umr_bitslice_reg(asic, reg, "LGKM_CNT", value);
		ws->ib_sts.valu_cnt = umr_bitslice_reg(asic, reg, "VALU_CNT", value);

	ws->tba_lo = buf[x++];
	ws->tba_hi = buf[x++];
	ws->tma_lo = buf[x++];
	ws->tma_hi = buf[x++];
	ws->ib_dbg0 = buf[x++];
	ws->m0 = buf[x++];

	ws->mode.value = value = buf[x++];
		reg = umr_find_reg_data(asic, "ixSQ_WAVE_MODE");
		ws->mode.fp_round = umr_bitslice_reg(asic, reg, "FP_ROUND", value);
		ws->mode.fp_denorm = umr_bitslice_reg(asic, reg, "FP_DENORM", value);
		ws->mode.dx10_clamp = umr_bitslice_reg(asic, reg, "DX10_CLAMP", value);
		ws->mode.ieee = umr_bitslice_reg(asic, reg, "IEEE", value);
		ws->mode.lod_clamped = umr_bitslice_reg(asic, reg, "LOD_CLAMPED", value);
		ws->mode.debug_en = umr_bitslice_reg(asic, reg, "DEBUG_EN", value);
		ws->mode.excp_en = umr_bitslice_reg(asic, reg, "EXCP_EN", value);
		ws->mode.gpr_idx_en = umr_bitslice_reg(asic, reg, "GPR_IDX_EN", value);
		ws->mode.vskip = umr_bitslice_reg(asic, reg, "VSKIP", value);
		ws->mode.csp = umr_bitslice_reg(asic, reg, "CSP", value);
	return 0;
}

static int umr_parse_wave_data_gfx_9(struct umr_asic *asic, struct umr_wave_status *ws, const uint32_t *buf)
{
	struct umr_reg *reg;
	uint32_t value;
	int x;

	if (buf[0] != 1) {
		asic->err_msg("[ERROR]: Was expecting type 1 wave data on a FAMILY_AI part!\n");
		return -1;
	}

	x = 1;
	ws->wave_status.value = value = buf[x++];
		reg = umr_find_reg_data(asic, "ixSQ_WAVE_STATUS");
		ws->wave_status.scc = umr_bitslice_reg(asic, reg, "SCC", value);
		ws->wave_status.priv = umr_bitslice_reg(asic, reg, "PRIV", value);
		ws->wave_status.execz = umr_bitslice_reg(asic, reg, "EXECZ", value);
		ws->wave_status.vccz  = umr_bitslice_reg(asic, reg, "VCCZ", value);
		ws->wave_status.in_tg = umr_bitslice_reg(asic, reg, "IN_TG", value);
		ws->wave_status.halt = umr_bitslice_reg(asic, reg, "HALT", value);
		ws->wave_status.valid = umr_bitslice_reg(asic, reg, "VALID", value);
		ws->wave_status.spi_prio = umr_bitslice_reg(asic, reg, "SPI_PRIO", value);
		ws->wave_status.wave_prio = umr_bitslice_reg(asic, reg, "USER_PRIO", value);
		ws->wave_status.trap_en = umr_bitslice_reg(asic, reg, "TRAP_EN", value);
		ws->wave_status.ttrace_en = umr_bitslice_reg(asic, reg, "TTRACE_EN", value);
		ws->wave_status.export_rdy = umr_bitslice_reg(asic, reg, "EXPORT_RDY", value);
		ws->wave_status.in_barrier = umr_bitslice_reg(asic, reg, "IN_BARRIER", value);
		ws->wave_status.trap = umr_bitslice_reg(asic, reg, "TRAP", value);
		ws->wave_status.ecc_err = umr_bitslice_reg(asic, reg, "ECC_ERR", value);
		ws->wave_status.skip_export = umr_bitslice_reg(asic, reg, "SKIP_EXPORT", value);
		ws->wave_status.perf_en = umr_bitslice_reg(asic, reg, "PERF_EN", value);
		ws->wave_status.cond_dbg_user = (value >> 0x14) & 1;
		ws->wave_status.cond_dbg_sys = (value >> 0x15) & 1;
		ws->wave_status.dispatch_cache_ctrl = (value >> 0x18) & 3;
		ws->wave_status.allow_replay = umr_bitslice_reg(asic, reg, "ALLOW_REPLAY", value);
		ws->wave_status.fatal_halt = umr_bitslice_reg(asic, reg, "FATAL_HALT", value);
		ws->wave_status.must_export = umr_bitslice_reg(asic, reg, "MUST_EXPORT", value);

	ws->pc_lo = buf[x++];
	ws->pc_hi = buf[x++];
	ws->exec_lo = buf[x++];
	ws->exec_hi = buf[x++];

	ws->hw_id.value = value = buf[x++];
		reg = umr_find_reg_data(asic, "ixSQ_WAVE_HW_ID");
		ws->hw_id.wave_id = umr_bitslice_reg(asic, reg, "WAVE_ID", value);
		ws->hw_id.simd_id = umr_bitslice_reg(asic, reg, "SIMD_ID", value);
		ws->hw_id.pipe_id = umr_bitslice_reg(asic, reg, "PIPE_ID", value);
		ws->hw_id.cu_id   = umr_bitslice_reg(asic, reg, "CU_ID", value);
		ws->hw_id.sh_id   = umr_bitslice_reg(asic, reg, "SH_ID", value);
		ws->hw_id.se_id   = umr_bitslice_reg(asic, reg, "SE_ID", value);
		ws->hw_id.tg_id   = umr_bitslice_reg(asic, reg, "TG_ID", value);
		ws->hw_id.vm_id   = umr_bitslice_reg(asic, reg, "VM_ID", value);
		ws->hw_id.queue_id = umr_bitslice_reg(asic, reg, "QUEUE_ID", value);
		ws->hw_id.state_id = umr_bitslice_reg(asic, reg, "STATE_ID", value);
		ws->hw_id.me_id    = umr_bitslice_reg(asic, reg, "ME_ID", value);

	ws->wave_inst_dw0 = buf[x++];
	ws->wave_inst_dw1 = buf[x++];

	ws->gpr_alloc.value = value = buf[x++];
		reg = umr_find_reg_data(asic, "ixSQ_WAVE_GPR_ALLOC");
		ws->gpr_alloc.vgpr_base = umr_bitslice_reg(asic, reg, "VGPR_BASE", value);
		ws->gpr_alloc.vgpr_size = umr_bitslice_reg(asic, reg, "VGPR_SIZE", value);
		ws->gpr_alloc.sgpr_base = umr_bitslice_reg(asic, reg, "SGPR_BASE", value);
		ws->gpr_alloc.sgpr_size = umr_bitslice_reg(asic, reg, "SGPR_SIZE", value);

	ws->lds_alloc.value = value = buf[x++];
		reg = umr_find_reg_data(asic, "ixSQ_WAVE_LDS_ALLOC");
		ws->lds_alloc.lds_base = umr_bitslice_reg(asic, reg, "LDS_BASE", value);
		ws->lds_alloc.lds_size = umr_bitslice_reg(asic, reg, "LDS_SIZE", value);

	ws->trapsts.value = value = buf[x++];
		reg = umr_find_reg_data(asic, "ixSQ_WAVE_TRAPSTS");
		ws->trapsts.excp = umr_bitslice_reg(asic, reg, "EXCP", value);
		ws->trapsts.excp_cycle = umr_bitslice_reg(asic, reg, "EXCP_CYCLE", value);
		ws->trapsts.dp_rate = umr_bitslice_reg(asic, reg, "DP_RATE", value);

	ws->ib_sts.value = value = buf[x++];
		reg = umr_find_reg_data(asic, "ixSQ_WAVE_IB_STS");
		ws->ib_sts.vm_cnt = umr_bitslice_reg(asic, reg, "VM_CNT", value);
		ws->ib_sts.exp_cnt = umr_bitslice_reg(asic, reg, "EXP_CNT", value);
		ws->ib_sts.lgkm_cnt = umr_bitslice_reg(asic, reg, "LGKM_CNT", value);
		ws->ib_sts.valu_cnt = umr_bitslice_reg(asic, reg, "VALU_CNT", value);

	ws->ib_dbg0 = buf[x++];
	ws->m0 = buf[x++];

	ws->mode.value = value = buf[x++];
		reg = umr_find_reg_data(asic, "ixSQ_WAVE_MODE");
		ws->mode.fp_round = umr_bitslice_reg(asic, reg, "FP_ROUND", value);
		ws->mode.fp_denorm = umr_bitslice_reg(asic, reg, "FP_DENORM", value);
		ws->mode.dx10_clamp = umr_bitslice_reg(asic, reg, "DX10_CLAMP", value);
		ws->mode.ieee = umr_bitslice_reg(asic, reg, "IEEE", value);
		ws->mode.lod_clamped = umr_bitslice_reg(asic, reg, "LOD_CLAMPED", value);
		ws->mode.debug_en = umr_bitslice_reg(asic, reg, "DEBUG_EN", value);
		ws->mode.excp_en = umr_bitslice_reg(asic, reg, "EXCP_EN", value);
		ws->mode.fp16_ovfl = umr_bitslice_reg(asic, reg, "FP16_OVFL", value);
		ws->mode.pops_packer0 = umr_bitslice_reg(asic, reg, "POPS_PACKER0", value);
		ws->mode.pops_packer1 = umr_bitslice_reg(asic, reg, "POPS_PACKER1", value);
		ws->mode.disable_perf = umr_bitslice_reg(asic, reg, "DISABLE_PERF", value);
		ws->mode.gpr_idx_en = umr_bitslice_reg(asic, reg, "GPR_IDX_EN", value);
		ws->mode.vskip = umr_bitslice_reg(asic, reg, "VSKIP", value);
		ws->mode.csp = umr_bitslice_reg(asic, reg, "CSP", value);
	return 0;
}

static int umr_parse_wave_data_gfx_10(struct umr_asic *asic, struct umr_wave_status *ws, const uint32_t *buf)
{
	struct umr_reg *reg;
	uint32_t value;
	int x;

	if (buf[0] != 2) {
		asic->err_msg("[ERROR]: Was expecting type 2 wave data on a FAMILY_NV part!\n");
		return -1;
	}

	memset(ws, 0, sizeof *ws);

	x = 1;
	ws->wave_status.value = value = buf[x++];
		reg = umr_find_reg_data(asic, "ixSQ_WAVE_STATUS");
		ws->wave_status.scc = umr_bitslice_reg(asic, reg, "SCC", value);
		ws->wave_status.priv = umr_bitslice_reg(asic, reg, "PRIV", value);
		ws->wave_status.execz = umr_bitslice_reg(asic, reg, "EXECZ", value);
		ws->wave_status.vccz  = umr_bitslice_reg(asic, reg, "VCCZ", value);
		ws->wave_status.in_tg = umr_bitslice_reg(asic, reg, "IN_TG", value);
		ws->wave_status.halt = umr_bitslice_reg(asic, reg, "HALT", value);
		ws->wave_status.valid = umr_bitslice_reg(asic, reg, "VALID", value);
		ws->wave_status.spi_prio = umr_bitslice_reg(asic, reg, "SPI_PRIO", value);
		ws->wave_status.wave_prio = umr_bitslice_reg(asic, reg, "USER_PRIO", value);
		ws->wave_status.trap_en = umr_bitslice_reg(asic, reg, "TRAP_EN", value);
		ws->wave_status.ttrace_en = umr_bitslice_reg(asic, reg, "TTRACE_EN", value);
		ws->wave_status.export_rdy = umr_bitslice_reg(asic, reg, "EXPORT_RDY", value);
		ws->wave_status.in_barrier = umr_bitslice_reg(asic, reg, "IN_BARRIER", value);
		ws->wave_status.trap = umr_bitslice_reg(asic, reg, "TRAP", value);
		ws->wave_status.ecc_err = umr_bitslice_reg(asic, reg, "ECC_ERR", value);
		ws->wave_status.skip_export = umr_bitslice_reg(asic, reg, "SKIP_EXPORT", value);
		ws->wave_status.perf_en = umr_bitslice_reg(asic, reg, "PERF_EN", value);
		ws->wave_status.cond_dbg_user = (value >> 0x14) & 1;
		ws->wave_status.cond_dbg_sys = (value >> 0x15) & 1;
		ws->wave_status.dispatch_cache_ctrl = (value >> 0x18) & 3;
		ws->wave_status.fatal_halt = umr_bitslice_reg(asic, reg, "FATAL_HALT", value);
		ws->wave_status.must_export = umr_bitslice_reg(asic, reg, "MUST_EXPORT", value);
		ws->wave_status.ttrace_simd_en = umr_bitslice_reg(asic, reg, "TTRACE_SIMD_EN", value);

	ws->pc_lo = buf[x++];
	ws->pc_hi = buf[x++];
	ws->exec_lo = buf[x++];
	ws->exec_hi = buf[x++];

	ws->hw_id1.value = value = buf[x++];
		reg = umr_find_reg_data(asic, "ixSQ_WAVE_HW_ID1");
		ws->hw_id1.wave_id = umr_bitslice_reg(asic, reg, "WAVE_ID", value);
		ws->hw_id1.simd_id = umr_bitslice_reg(asic, reg, "SIMD_ID", value);
		ws->hw_id1.wgp_id  = umr_bitslice_reg(asic, reg, "WGP_ID", value);
		ws->hw_id1.sa_id   = umr_bitslice_reg(asic, reg, "SA_ID", value);
		ws->hw_id1.se_id   = umr_bitslice_reg(asic, reg, "SE_ID", value);

	ws->hw_id2.value = value = buf[x++];
		reg = umr_find_reg_data(asic, "ixSQ_WAVE_HW_ID2");
		ws->hw_id2.queue_id     = umr_bitslice_reg(asic, reg, "QUEUE_ID", value);
		ws->hw_id2.pipe_id      = umr_bitslice_reg(asic, reg, "PIPE_ID", value);
		ws->hw_id2.me_id        = umr_bitslice_reg(asic, reg, "ME_ID", value);
		ws->hw_id2.state_id     = umr_bitslice_reg(asic, reg, "STATE_ID", value);
		ws->hw_id2.wg_id        = umr_bitslice_reg(asic, reg, "WG_ID", value);
		ws->hw_id2.vm_id        = umr_bitslice_reg(asic, reg, "VM_ID", value);
		ws->hw_id2.compat_level = umr_bitslice_reg_quiet(asic, reg, "COMPAT_LEVEL", value); // not on 10.3

	ws->wave_inst_dw0 = buf[x++];

	ws->gpr_alloc.value = value = buf[x++];
		reg = umr_find_reg_data(asic, "ixSQ_WAVE_GPR_ALLOC");
		ws->gpr_alloc.vgpr_base = umr_bitslice_reg(asic, reg, "VGPR_BASE", value);
		ws->gpr_alloc.vgpr_size = umr_bitslice_reg(asic, reg, "VGPR_SIZE", value);
		ws->gpr_alloc.sgpr_base = umr_bitslice_reg(asic, reg, "SGPR_BASE", value);
		ws->gpr_alloc.sgpr_size = umr_bitslice_reg(asic, reg, "SGPR_SIZE", value);

	ws->lds_alloc.value = value = buf[x++];
		reg = umr_find_reg_data(asic, "ixSQ_WAVE_LDS_ALLOC");
		ws->lds_alloc.lds_base = umr_bitslice_reg(asic, reg, "LDS_BASE", value);
		ws->lds_alloc.lds_size = umr_bitslice_reg(asic, reg, "LDS_SIZE", value);
		ws->lds_alloc.vgpr_shared_size = umr_bitslice_reg(asic, reg, "VGPR_SHARED_SIZE", value);

	ws->trapsts.value = value = buf[x++];
		reg = umr_find_reg_data(asic, "ixSQ_WAVE_TRAPSTS");
		ws->trapsts.excp          = umr_bitslice_reg(asic, reg, "EXCP", value) |
								    (umr_bitslice_reg(asic, reg, "EXCP_HI", value) << 9);
		ws->trapsts.savectx       = umr_bitslice_reg(asic, reg, "SAVECTX", value);
		ws->trapsts.illegal_inst  = umr_bitslice_reg(asic, reg, "ILLEGAL_INST", value);
		ws->trapsts.excp_hi       = umr_bitslice_reg(asic, reg, "EXCP_HI", value);
		ws->trapsts.buffer_oob    = umr_bitslice_reg(asic, reg, "BUFFER_OOB", value);
		ws->trapsts.excp_cycle    = umr_bitslice_reg(asic, reg, "EXCP_CYCLE", value);
		ws->trapsts.excp_group_mask = umr_bitslice_reg_quiet(asic, reg, "EXCP_GROUP_MASK", value);
		ws->trapsts.excp_wave64hi = umr_bitslice_reg(asic, reg, "EXCP_WAVE64HI", value);
		ws->trapsts.xnack_error   = umr_bitslice_reg_quiet(asic, reg, "XNACK_ERROR", value);
		ws->trapsts.utc_error     = umr_bitslice_reg_quiet(asic, reg, "UTC_ERROR", value);
		ws->trapsts.dp_rate       = umr_bitslice_reg(asic, reg, "DP_RATE", value);

	ws->ib_sts.value = value = buf[x++];
		reg = umr_find_reg_data(asic, "ixSQ_WAVE_IB_STS");
		ws->ib_sts.vm_cnt   = umr_bitslice_reg(asic, reg, "VM_CNT", value) |
							  (umr_bitslice_reg(asic, reg, "VM_CNT_HI", value) << 4);
		ws->ib_sts.exp_cnt  = umr_bitslice_reg(asic, reg, "EXP_CNT", value);
		ws->ib_sts.lgkm_cnt = umr_bitslice_reg(asic, reg, "LGKM_CNT", value) |
							  (umr_bitslice_reg(asic, reg, "LGKM_CNT_BIT4", value) << 4) |
							  (umr_bitslice_reg(asic, reg, "LGKM_CNT_BIT5", value) << 5);
		ws->ib_sts.valu_cnt = umr_bitslice_reg(asic, reg, "VALU_CNT", value);
		ws->ib_sts.replay_w64h = umr_bitslice_reg_quiet(asic, reg, "REPLAY_W64H", value);
		ws->ib_sts.vs_cnt   = umr_bitslice_reg(asic, reg, "VS_CNT", value);

	ws->ib_sts2.value = value = buf[x++];
		reg = umr_find_reg_data(asic, "ixSQ_WAVE_IB_STS2");
		ws->ib_sts2.inst_prefetch     = umr_bitslice_reg(asic, reg, "INST_PREFETCH", value);
		ws->ib_sts2.resource_override = umr_bitslice_reg(asic, reg, "RESOURCE_OVERRIDE", value);
		ws->ib_sts2.mem_order         = umr_bitslice_reg(asic, reg, "MEM_ORDER", value);
		ws->ib_sts2.fwd_progress      = umr_bitslice_reg(asic, reg, "FWD_PROGRESS", value);
		ws->ib_sts2.wave64            = umr_bitslice_reg(asic, reg, "WAVE64", value);
		ws->ib_sts2.wave64hi          = umr_bitslice_reg_quiet(asic, reg, "WAVE64HI", value);
		ws->ib_sts2.subv_loop         = umr_bitslice_reg_quiet(asic, reg, "SUBV_LOOP", value);

	ws->ib_dbg1 = buf[x++];
	ws->m0 = buf[x++];

	ws->mode.value = value = buf[x++];
		reg = umr_find_reg_data(asic, "ixSQ_WAVE_MODE");
		ws->mode.fp_round = umr_bitslice_reg(asic, reg, "FP_ROUND", value);
		ws->mode.fp_denorm = umr_bitslice_reg(asic, reg, "FP_DENORM", value);
		ws->mode.dx10_clamp = umr_bitslice_reg(asic, reg, "DX10_CLAMP", value);
		ws->mode.ieee = umr_bitslice_reg(asic, reg, "IEEE", value);
		ws->mode.lod_clamped = umr_bitslice_reg(asic, reg, "LOD_CLAMPED", value);
		ws->mode.debug_en = umr_bitslice_reg(asic, reg, "DEBUG_EN", value);
		ws->mode.excp_en = umr_bitslice_reg(asic, reg, "EXCP_EN", value);
		ws->mode.fp16_ovfl = umr_bitslice_reg(asic, reg, "FP16_OVFL", value);
		ws->mode.disable_perf = umr_bitslice_reg(asic, reg, "DISABLE_PERF", value);
	return 0;
}

int umr_parse_wave_data_gfx(struct umr_asic *asic, struct umr_wave_status *ws, const uint32_t *buf)
{
	if (asic->family < FAMILY_AI)
		return umr_parse_wave_data_gfx_8(asic, ws, buf);
	else if (asic->family < FAMILY_NV)
		return umr_parse_wave_data_gfx_9(asic, ws, buf);
	else
		return umr_parse_wave_data_gfx_10(asic, ws, buf);
}

/**
 * Scan the given wave slot. Return true and fill in \p pwd if a wave is present.
 * Otherwise, return false.
 *
 * \param cu the CU on <=gfx9, the WGP on >=gfx10
 */
static int umr_scan_wave_slot(struct umr_asic *asic, uint32_t se, uint32_t sh, uint32_t cu,
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

	if (!pwd->ws.wave_status.valid &&
	    (!pwd->ws.wave_status.halt || pwd->ws.wave_status.value == 0xbebebeef))
		return 0;

	pwd->se = se;
	pwd->sh = sh;
	pwd->cu = cu;
	pwd->simd = simd;
	pwd->wave = wave;

	if (!asic->options.skip_gprs) {
		asic->gpr_read_funcs.read_sgprs(asic, &pwd->ws, &pwd->sgprs[0]);

		if (asic->family <= FAMILY_AI)
			num_threads = 64;
		else
			num_threads = pwd->ws.ib_sts2.wave64 ? 64 : 32;

		pwd->have_vgprs = 1;
		pwd->num_threads = num_threads;
		for (thread = 0; thread < num_threads; ++thread) {
			if (asic->gpr_read_funcs.read_vgprs(asic, &pwd->ws, thread,
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
 * Scan for waves within a single SIMD.
 *
 * \param cu the CU instance on <=gfx9, the WGP index on >=gfx10
 * \param simd the SIMD within the CU / WGP
 * \param pppwd points to the pointer-to-pointer-to the last element of a linked
 *              list of wave data structures, with the last element yet to be filled in.
 *              The pointer-to-pointer-to is updated by this function.
 */
static int umr_scan_wave_simd(struct umr_asic *asic, uint32_t se, uint32_t sh, uint32_t cu, uint32_t simd,
			       struct umr_wave_data ***pppwd)
{
	uint32_t wave, wave_limit;
	int r;

	wave_limit = asic->family <= FAMILY_AI ? 10 : 20;

	for (wave = 0; wave < wave_limit; wave++) {
		struct umr_wave_data *pwd = **pppwd;
		if ((r = umr_scan_wave_slot(asic, se, sh, cu, simd, wave, pwd)) == 1) {
			pwd->next = calloc(1, sizeof(*pwd));
			if (!pwd->next) {
				asic->err_msg("[ERROR]: Out of memory\n");
				return -1;
			}
			*pppwd = &pwd->next;
		}
		if (r == -1)
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
	uint32_t se, sh, cu, simd;
	struct umr_wave_data *ohead, *head, **ptail;
	int r;

	ohead = head = calloc(1, sizeof *head);
	if (!head) {
		asic->err_msg("[ERROR]: Out of memory\n");
		return NULL;
	}
	ptail = &head;

	for (se = 0; se < asic->config.gfx.max_shader_engines; se++)
	for (sh = 0; sh < asic->config.gfx.max_sh_per_se; sh++)
	for (cu = 0; cu < asic->config.gfx.max_cu_per_sh; cu++) {
		if (asic->family <= FAMILY_AI) {
			asic->wave_funcs.get_wave_sq_info(asic, se, sh, cu, &(*ptail)->ws);
			if ((*ptail)->ws.sq_info.busy) {
				for (simd = 0; simd < 4; simd++) {
					r = umr_scan_wave_simd(asic, se, sh, cu, simd, &ptail);
					if (r < 0)
						goto error;
				}
			}
		} else {
			for (simd = 0; simd < 4; simd++) {
				asic->wave_funcs.get_wave_sq_info(asic, se, sh, MANY_TO_INSTANCE(cu, simd), &(*ptail)->ws);
				if ((*ptail)->ws.sq_info.busy) {
					r = umr_scan_wave_simd(asic, se, sh, cu, simd, &ptail);
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

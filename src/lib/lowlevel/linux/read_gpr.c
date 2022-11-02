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

static void wave_read_regs_via_mmio(struct umr_asic *asic, uint32_t simd,
			   uint32_t wave, uint32_t thread,
			   uint32_t regno, uint32_t num, uint32_t *out)
{
	struct umr_reg *ind_index, *ind_data;
	uint32_t data;

	ind_index = umr_find_reg_data(asic, "mmSQ_IND_INDEX");
	ind_data  = umr_find_reg_data(asic, "mmSQ_IND_DATA");

	if (ind_index && ind_data) {
		data = umr_bitslice_compose_value(asic, ind_index, "WAVE_ID", wave);
		data |= umr_bitslice_compose_value(asic, ind_index, "SIMD_ID", simd);
		data |= umr_bitslice_compose_value(asic, ind_index, "INDEX", regno);
		data |= umr_bitslice_compose_value(asic, ind_index, "THREAD_ID", thread);
		data |= umr_bitslice_compose_value(asic, ind_index, "FORCE_READ", 1);
		data |= umr_bitslice_compose_value(asic, ind_index, "AUTO_INCR", 1);
		umr_write_reg(asic, ind_index->addr * 4, data, REG_MMIO);
		while (num--)
			*(out++) = umr_read_reg(asic, ind_data->addr * 4, REG_MMIO);
	} else {
		asic->err_msg("[BUG]: The required SQ_IND_{INDEX,DATA} registers are not found on the asic <%s>\n", asic->asicname);
		return;
	}
}

/**
 * umr_read_sgprs - Read SGPR registers for a specific wave
 */
static int umr_read_sgprs_si_ai(struct umr_asic *asic, struct umr_wave_status *ws, uint32_t *dst)
{
	uint64_t addr, shift;
	int r;

	if (asic->family <= FAMILY_CIK)
		shift = 3;  // on SI..CIK allocations were done in 8-dword blocks
	else
		shift = 4;  // on VI allocations are in 16-dword blocks

	if (!asic->options.no_kernel) {
		addr =
			(1ULL << 60)                             | // reading SGPRs
			((uint64_t)0)                            | // starting address to read from
			((uint64_t)ws->hw_id.se_id << 12)        |
			((uint64_t)ws->hw_id.sh_id << 20)        |
			((uint64_t)ws->hw_id.cu_id << 28)        |
			((uint64_t)ws->hw_id.wave_id << 36)      |
			((uint64_t)ws->hw_id.simd_id << 44)      |
			(0ULL << 52); // thread_id

		lseek(asic->fd.gpr, addr, SEEK_SET);
		r = read(asic->fd.gpr, dst, 4 * ((ws->gpr_alloc.sgpr_size + 1) << shift));
		if (r < 0)
			return r;

		if (asic->options.test_log && asic->options.test_log_fd) {
			int x;
			fprintf(asic->options.test_log_fd, "SGPR@0x%"PRIx64" = { ", addr);
			for (x = 0; x < r; x += 4) {
				fprintf(asic->options.test_log_fd, "0x%"PRIx32, dst[x/4]);
				if (x < (r - 4))
					fprintf(asic->options.test_log_fd, ", ");
			}
			fprintf(asic->options.test_log_fd, "}\n");
		}

		// read trap if any
		if (ws->wave_status.trap_en || ws->wave_status.priv) {
			addr += 4 * 0x6C; // address in bytes, kernel adds 0x200 to request
			lseek(asic->fd.gpr, addr, SEEK_SET);
			r = read(asic->fd.gpr, &dst[0x6C], 4 * 16);
			if (r > 0) {
				if (asic->options.test_log && asic->options.test_log_fd) {
					int x;
					fprintf(asic->options.test_log_fd, "SGPR@0x%"PRIx64" = { ", addr);
					for (x = 0; x < r; x += 4) {
						fprintf(asic->options.test_log_fd, "0x%"PRIx32, dst[0x6C + x/4]);
						if (x < (r - 4))
							fprintf(asic->options.test_log_fd, ", ");
					}
					fprintf(asic->options.test_log_fd, "}\n");
				}
			}
		}
		return r;
	} else {
		umr_grbm_select_index(asic, ws->hw_id.se_id, ws->hw_id.sh_id, ws->hw_id.cu_id);
		wave_read_regs_via_mmio(asic, ws->hw_id.simd_id, ws->hw_id.wave_id, 0, 0x200,
					(ws->gpr_alloc.sgpr_size + 1) << shift, dst);
		if (ws->wave_status.trap_en || ws->wave_status.priv)
			wave_read_regs_via_mmio(asic, ws->hw_id.simd_id, ws->hw_id.wave_id, 0, 0x26C,
						16, &dst[0x6C]);
		umr_grbm_select_index(asic, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF);
		return 0;
	}
}

static void wave_read_regs_via_mmio_nv(struct umr_asic *asic,
			   uint32_t wave, uint32_t thread,
			   uint32_t regno, uint32_t num, uint32_t *out)
{
	struct umr_reg *ind_index, *ind_data;
	uint32_t data;

	ind_index = umr_find_reg_data(asic, "mmSQ_IND_INDEX");
	ind_data  = umr_find_reg_data(asic, "mmSQ_IND_DATA");

	if (ind_index && ind_data) {
		data = umr_bitslice_compose_value(asic, ind_index, "WAVE_ID", wave);
		data |= umr_bitslice_compose_value(asic, ind_index, "INDEX", regno);
		data |= umr_bitslice_compose_value(asic, ind_index, "WORKITEM_ID", thread);
		data |= umr_bitslice_compose_value(asic, ind_index, "AUTO_INCR", 1);
		umr_write_reg(asic, ind_index->addr * 4, data, REG_MMIO);
		while (num--)
			*(out++) = umr_read_reg(asic, ind_data->addr * 4, REG_MMIO);
	} else {
		asic->err_msg("[BUG]: The required SQ_IND_{INDEX,DATA} registers are not found on the asic <%s>\n", asic->asicname);
		return;
	}
}

static int umr_read_sgprs_nv(struct umr_asic *asic, struct umr_wave_status *ws, uint32_t *dst)
{
	uint64_t addr;
	int r;

	if (ws->gpr_alloc.value == 0xbebebeef) {
		asic->err_msg("[WARNING]: Trying to read SGPRs from wave with GPR_ALLOC==0xbebebeef\n");
		return 0;
	}

	if (!asic->options.no_kernel) {
		addr =
			(1ULL << 60)                             | // reading SGPRs
			((uint64_t)0)                            | // starting address to read from
			((uint64_t)ws->hw_id1.se_id << 12)       |
			((uint64_t)ws->hw_id1.sa_id << 20)       |
			((uint64_t)((ws->hw_id1.wgp_id << 2) | ws->hw_id1.simd_id) << 28)  |
			((uint64_t)ws->hw_id1.wave_id << 36)     |
			(0ULL << 52); // thread_id

		lseek(asic->fd.gpr, addr, SEEK_SET);
		r = read(asic->fd.gpr, dst, 4 * 112);
		if (r < 0)
			return r;

		if (asic->options.test_log && asic->options.test_log_fd) {
			int x;
			fprintf(asic->options.test_log_fd, "SGPR@0x%"PRIx64" = { ", addr);
			for (x = 0; x < r; x += 4) {
				fprintf(asic->options.test_log_fd, "0x%"PRIx32, dst[x/4]);
				if (x < (r - 4))
					fprintf(asic->options.test_log_fd, ", ");
			}
			fprintf(asic->options.test_log_fd, "}\n");
		}

		// read trap if any
		if (ws->wave_status.trap_en || ws->wave_status.priv) {
			addr += 4 * 0x6C;  // byte offset, kernel adds 0x200 to address
			lseek(asic->fd.gpr, addr, SEEK_SET);
			r = read(asic->fd.gpr, &dst[0x6C], 4 * 16);
			if (r > 0) {
				if (asic->options.test_log && asic->options.test_log_fd) {
					int x;
					fprintf(asic->options.test_log_fd, "SGPR@0x%"PRIx64" = { ", addr);
					for (x = 0; x < r; x += 4) {
						fprintf(asic->options.test_log_fd, "0x%"PRIx32, dst[0x6C + x/4]);
						if (x < (r - 4))
							fprintf(asic->options.test_log_fd, ", ");
					}
					fprintf(asic->options.test_log_fd, "}\n");
				}
			}
		}
		return r;
	} else {
		umr_grbm_select_index(asic, ws->hw_id1.se_id, ws->hw_id1.sa_id, ((ws->hw_id1.wgp_id << 2) | ws->hw_id1.simd_id));
		wave_read_regs_via_mmio_nv(asic, ws->hw_id1.wave_id, 0, 0x200, 112, dst);
		umr_grbm_select_index(asic, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF);
		return 0;
	}
}

int umr_read_sgprs(struct umr_asic *asic, struct umr_wave_status *ws, uint32_t *dst)
{
	if (asic->family == FAMILY_NV)
		return umr_read_sgprs_nv(asic, ws, dst);
	else
		return umr_read_sgprs_si_ai(asic, ws, dst);
}


static int umr_read_vgprs_si_ai(struct umr_asic *asic, struct umr_wave_status *ws, uint32_t thread, uint32_t *dst)
{
	uint64_t addr;
	unsigned granularity = asic->parameters.vgpr_granularity; // default is blocks of 4 registers
	int r;

	// reading VGPR is not supported on pre GFX9 devices
	if (asic->family < FAMILY_AI)
		return -1;

	if (!asic->options.no_kernel) {
		addr =
			(0ULL << 60)                             | // reading VGPRs
			((uint64_t)0)                            | // starting address to read from
			((uint64_t)ws->hw_id.se_id << 12)        |
			((uint64_t)ws->hw_id.sh_id << 20)        |
			((uint64_t)ws->hw_id.cu_id << 28)        |
			((uint64_t)ws->hw_id.wave_id << 36)      |
			((uint64_t)ws->hw_id.simd_id << 44)      |
			((uint64_t)thread << 52);

		lseek(asic->fd.gpr, addr, SEEK_SET);
		r = read(asic->fd.gpr, dst, 4 * ((ws->gpr_alloc.vgpr_size + 1) << granularity));
		if (r > 0) {
			if (asic->options.test_log && asic->options.test_log_fd) {
				int x;
				fprintf(asic->options.test_log_fd, "VGPR@0x%"PRIx64" = { ", addr);
				for (x = 0; x < r; x += 4) {
					fprintf(asic->options.test_log_fd, "0x%"PRIx32, dst[x/4]);
					if (x < (r - 4))
						fprintf(asic->options.test_log_fd, ", ");
				}
				fprintf(asic->options.test_log_fd, "}\n");
			}
		}
		return r;
	} else {
		umr_grbm_select_index(asic, ws->hw_id.se_id, ws->hw_id.sh_id, ws->hw_id.cu_id);
		wave_read_regs_via_mmio(asic, ws->hw_id.simd_id, ws->hw_id.wave_id, thread, 0x400,
					(ws->gpr_alloc.vgpr_size + 1) << granularity, dst);
		umr_grbm_select_index(asic, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF);
		return 0;
	}
}

static int umr_read_vgprs_nv(struct umr_asic *asic, struct umr_wave_status *ws, uint32_t thread, uint32_t *dst)
{
	uint64_t addr;
	unsigned granularity = asic->parameters.vgpr_granularity;
	int r;

	if (ws->gpr_alloc.value == 0xbebebeef) {
		asic->err_msg("[WARNING]: Trying to read VGPRs from wave with GPR_ALLOC==0xbebebeef\n");
		return 0;
	}

	if (!asic->options.no_kernel) {
		addr =
			(0ULL << 60)                             | // reading VGPRs
			((uint64_t)0)                            | // starting address to read from
			((uint64_t)ws->hw_id1.se_id << 12)        |
			((uint64_t)ws->hw_id1.sa_id << 20)        |
			((uint64_t)((ws->hw_id1.wgp_id << 2) | ws->hw_id1.simd_id) << 28)  |
			((uint64_t)ws->hw_id1.wave_id << 36)      |
			((uint64_t)thread << 52);

		lseek(asic->fd.gpr, addr, SEEK_SET);
		r = read(asic->fd.gpr, dst, 4 * ((ws->gpr_alloc.vgpr_size + 1) << granularity));
		if (r > 0) {
			if (asic->options.test_log && asic->options.test_log_fd) {
				int x;
				fprintf(asic->options.test_log_fd, "VGPR@0x%"PRIx64" = { ", addr);
				for (x = 0; x < r; x += 4) {
					fprintf(asic->options.test_log_fd, "0x%"PRIx32, dst[x/4]);
					if (x < (r - 4))
						fprintf(asic->options.test_log_fd, ", ");
				}
				fprintf(asic->options.test_log_fd, "}\n");
			}
		}
		return r;
	} else {
		umr_grbm_select_index(asic, ws->hw_id1.se_id, ws->hw_id1.sa_id, ((ws->hw_id1.wgp_id << 2) | ws->hw_id1.simd_id));
		wave_read_regs_via_mmio_nv(asic, ws->hw_id1.wave_id, thread, 0x400,
					(ws->gpr_alloc.vgpr_size + 1) << granularity, dst);
		umr_grbm_select_index(asic, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF);
		return 0;
	}
}

/**
 * umr_read_vgprs - Read VGPR registers for a specific wave and thread
 */
int umr_read_vgprs(struct umr_asic *asic, struct umr_wave_status *ws, uint32_t thread, uint32_t *dst)
{
	if (asic->family == FAMILY_NV)
		return umr_read_vgprs_nv(asic, ws, thread, dst);
	else
		return umr_read_vgprs_si_ai(asic, ws, thread, dst);
}

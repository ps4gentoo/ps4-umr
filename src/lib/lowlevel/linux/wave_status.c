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

static int umr_get_wave_status_vi_ai(struct umr_asic *asic, unsigned se, unsigned sh, unsigned cu, unsigned simd, unsigned wave, struct umr_wave_status *ws)
{
	uint32_t buf[32];
	int r;
	uint64_t addr;

	memset(buf, 0, sizeof buf);

	if (!asic->options.no_kernel) {
		addr = 0 |
			((uint64_t)se << 7) |
			((uint64_t)sh << 15) |
			((uint64_t)cu << 23) |
			((uint64_t)wave << 31) |
			((uint64_t)simd << 37);
		lseek(asic->fd.wave, addr, SEEK_SET);
		r = read(asic->fd.wave, &buf, 32*4);
		if (r <= 0)
			return -1;
		if (asic->options.test_log && asic->options.test_log_fd) {
			int x;
			fprintf(asic->options.test_log_fd, "WAVESTATUS@0x%"PRIx64" = { ", addr);
			for (x = 0; x < r; x += 4) {
				fprintf(asic->options.test_log_fd, "0x%"PRIx32, buf[x/4]);
				if (x < (r - 4))
					fprintf(asic->options.test_log_fd, ", ");
			}
			fprintf(asic->options.test_log_fd, "}\n");
		}
	} else {
		int n = 0;
		umr_grbm_select_index(asic, se, sh, cu);
		umr_read_wave_status_via_mmio_gfx8_9(asic, simd, wave, &buf[0], &n);
		umr_grbm_select_index(asic, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF);
	}

	return umr_parse_wave_data_gfx(asic, ws, buf);
}

static int umr_get_wave_status_nv(struct umr_asic *asic, unsigned se, unsigned sh, unsigned cu, unsigned simd, unsigned wave, struct umr_wave_status *ws)
{
	uint32_t buf[32], reg;
	uint64_t addr;
	int r;

	if (simd)
		asic->err_msg("[BUG]: simd should be zero in umr_get_wave_status_nv()\n");

	// do a trial read of VMID 0's BASE ADDR to see if it's all F's
	reg = umr_read_reg_by_name_by_ip(asic, "gfx", "mmGCVM_CONTEXT0_PAGE_TABLE_BASE_ADDR_LO32");
	if (reg == 0xFFFFFFFF) {
		asic->err_msg("[ERROR]: The GFXOFF feature may be enabled leading to incorrect behaviour.\nPlease disable with '--gfxoff 0' before reading waves.\n");
		return -1;
	}

	memset(buf, 0, sizeof buf);

	if (!asic->options.no_kernel) {
		addr = 0 |
			((uint64_t)se << 7) |
			((uint64_t)sh << 15) |
			((uint64_t)cu << 23) |
			((uint64_t)wave << 31) |
			((uint64_t)simd << 37);
		lseek(asic->fd.wave, addr, SEEK_SET);
		if ((r = read(asic->fd.wave, &buf, 32*4)) < 0)
			return -1;
		if (asic->options.test_log && asic->options.test_log_fd) {
			int x;
			fprintf(asic->options.test_log_fd, "WAVESTATUS@0x%"PRIx64" = { ", addr);
			for (x = 0; x < r; x += 4) {
				fprintf(asic->options.test_log_fd, "0x%"PRIx32, buf[x/4]);
				if (x < (r - 4))
					fprintf(asic->options.test_log_fd, ", ");
			}
			fprintf(asic->options.test_log_fd, "}\n");
		}
	} else {
		int n = 0;
		umr_grbm_select_index(asic, se, sh, cu);
		umr_read_wave_status_via_mmio_gfx10(asic, wave, &buf[0], &n);
		umr_grbm_select_index(asic, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF);
	}

	return umr_parse_wave_data_gfx(asic, ws, buf);
}

/**
 * umr_get_wave_status - Read the SQ wave status (et al.) data
 *
 * @se, @sh, @cu, @simd, @wave - Identification of the specific wave to read
 *
 * Stores the wave data in @ws if successful.
 */
int umr_get_wave_status(struct umr_asic *asic, unsigned se, unsigned sh, unsigned cu, unsigned simd, unsigned wave, struct umr_wave_status *ws)
{
	if (asic->family == FAMILY_NV)
		return umr_get_wave_status_nv(asic, se, sh, cu, simd, wave, ws);
	else if (asic->family <= FAMILY_AI)
		return umr_get_wave_status_vi_ai(asic, se, sh, cu, simd, wave, ws);
	return -1;
}

int umr_get_wave_sq_info(struct umr_asic *asic, unsigned se, unsigned sh, unsigned cu, struct umr_wave_status *ws)
{
	if (asic->family <= FAMILY_NV)
		return umr_get_wave_sq_info_vi(asic, se, sh, cu, ws);
	return -1;
}

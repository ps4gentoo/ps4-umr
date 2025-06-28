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
#include <linux/ioctl.h>
#include <linux/types.h>
#include <asm/ioctl.h>
#include <sys/ioctl.h>


struct amdgpu_debugfs_gprwave_iocdata {
	__u32 gpr_or_wave, se, sh, cu, wave, simd, xcc_id;
	struct {
		__u32 thread, vpgr_or_sgpr;
	} gpr;
};


enum AMDGPU_DEBUGFS_GPRWAVE_CMDS {
	AMDGPU_DEBUGFS_GPRWAVE_CMD_SET_STATE=0,
};
#define AMDGPU_DEBUGFS_GPRWAVE_IOC_SET_STATE _IOWR(0x20, AMDGPU_DEBUGFS_GPRWAVE_CMD_SET_STATE, struct amdgpu_debugfs_gprwave_iocdata)

/**
 * @brief Reads GPR or wave data from a specified GPU resource.
 *
 * This function reads General Purpose Registers (GPR) or wave data from a specified GPU resource
 * identified by the provided parameters. It uses an ioctl call to set up the necessary state and then
 * performs a read operation on the file descriptor associated with the GPR/wave debugfs entry.
 *
 * @param asic Pointer to the UMR ASIC structure representing the GPU.
 * @param v_or_s Indicates whether to read VGPRs (1) or SGPRs (0).
 * @param thread The thread ID within the wavefront.
 * @param se Shader Engine ID.
 * @param sh Shader Array ID.
 * @param cu Compute Unit ID.
 * @param wave Wave ID.
 * @param simd SIMD ID.
 * @param offset Offset in bytes from which to start reading data.
 * @param size Number of bytes to read.
 * @param dst Pointer to the buffer where the read data will be stored.
 *
 * @return Returns 0 on success, or a negative error code on failure.
 */
int umr_linux_read_gpr_gprwave_raw(struct umr_asic *asic, int v_or_s,
				   uint32_t thread, uint32_t se, uint32_t sh, uint32_t cu, uint32_t wave, uint32_t simd,
				   uint32_t offset, uint32_t size, uint32_t *dst)
{
	struct amdgpu_debugfs_gprwave_iocdata id;
	int r = 0;

	memset(&id, 0, sizeof id);
	id.gpr_or_wave = 1;
	if (asic->family < FAMILY_NV) {
		id.se = se;
		id.sh = sh;
		id.cu = cu;
		id.wave = wave;
		id.simd = simd;

		if (v_or_s == 0) {
			id.gpr.thread = 0;
		} else {
			id.gpr.thread = thread;
		}
	} else {
		id.se = se;
		id.sh = sh;
		id.cu = cu;
		id.wave = wave;
		id.simd = 0;
		if (v_or_s == 0) {
			id.gpr.thread = 0;
		} else {
			id.gpr.thread = thread;
		}
	}
	id.gpr.vpgr_or_sgpr = v_or_s;
	id.xcc_id = asic->options.vm_partition == -1 ? 0 : asic->options.vm_partition;

	r = ioctl(asic->fd.gprwave, AMDGPU_DEBUGFS_GPRWAVE_IOC_SET_STATE, &id);
	if (r)
		return r;

	lseek(asic->fd.gprwave, offset, SEEK_SET);
	return read(asic->fd.gprwave, dst, size);
}

// TODO: hoist id/lseek/read calls into raw function out of this function
static int read_gpr_gprwave(struct umr_asic *asic, int v_or_s, uint32_t thread, struct umr_wave_data *wd, uint32_t *dst)
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
#if 0
		se = umr_wave_data_get_bits(asic, wd, "ixSQ_WAVE_HW_ID1", "SE_ID");
		sh = umr_wave_data_get_bits(asic, wd, "ixSQ_WAVE_HW_ID1", "SA_ID");
		cu = ((umr_wave_data_get_bits(asic, wd, "ixSQ_WAVE_HW_ID1", "WGP_ID") << 2) | umr_wave_data_get_bits(asic, wd, "ixSQ_WAVE_HW_ID1", "SIMD_ID"));
		wave = umr_wave_data_get_bits(asic, wd, "ixSQ_WAVE_HW_ID1", "WAVE_ID");
		simd = 0;
#else
		se = wd->se;
		sh = wd->sh;
		cu = (wd->cu << 2) | wd->simd;
		simd = 0;
		wave = wd->wave;
#endif

		if (v_or_s == 0) {
			size = 4 * 124; // regular SGPRs, VCC, and TTMPs
		} else {
			size = 4 * ((umr_wave_data_get_bits(asic, wd, "ixSQ_WAVE_GPR_ALLOC", "VGPR_SIZE") + 1) << asic->parameters.vgpr_granularity);
		}
	}

	r = umr_linux_read_gpr_gprwave_raw(asic, v_or_s, thread, se, sh, cu, wave, simd, 0, size, dst);
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

	// TODO: hoist trap to _raw
	if (v_or_s == 0 && size < (4*0x6C)) {
		// read trap if any
		if (umr_wave_data_get_flag_trap_en(asic, wd) || umr_wave_data_get_flag_priv(asic, wd)) {
			r = umr_linux_read_gpr_gprwave_raw(asic, v_or_s, thread, se, sh, cu, wave, simd, 4 * 0x6C, size, &dst[0x6C]);
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
 * @brief Reads SGPR registers for a specific wave.
 *
 * This function reads the SGPRs for a specified wave on the GPU.
 * It utilizes the GPR/wave debugfs interface to perform the read operation.
 *
 * @param asic Pointer to the UMR ASIC structure representing the GPU.
 * @param wd Pointer to the UMR wave data structure containing information about the wavefront.
 * @param dst Pointer to the buffer where the read SGPR data will be stored.
 *
 * @return Returns 0 on success, or a negative error code if the kernel is too old or an error occurs during the read operation.
 */int umr_read_sgprs(struct umr_asic *asic, struct umr_wave_data *wd, uint32_t *dst)
{
	if (asic->fd.gprwave >= 0) {
		return read_gpr_gprwave(asic, 0, 0, wd, dst);
	} else {
		asic->err_msg("[ERROR]:  Your kernel is too old the amdgpu_gprwave file is now required.\n");
		return -1;
	}
}

/**
 * @brief Reads VGPR registers for a specific wave and thread.
 *
 * This function reads the Vector General Purpose Registers (VGPRs) for a specified wave and thread on the GPU.
 * It utilizes the GPR/wave debugfs interface to perform the read operation. Reading VGPRs is not supported
 * on pre-GFX9 devices.
 *
 * @param asic Pointer to the UMR ASIC structure representing the GPU.
 * @param wd Pointer to the UMR wave data structure containing information about the wavefront.
 * @param thread The thread ID within the wavefront for which VGPRs are to be read.
 * @param dst Pointer to the buffer where the read VGPR data will be stored.
 *
 * @return Returns 0 on success, -1 if reading VGPRs is not supported on the device, or a negative error code
 *         if the kernel is too old or an error occurs during the read operation.
 */
int umr_read_vgprs(struct umr_asic *asic, struct umr_wave_data *wd, uint32_t thread, uint32_t *dst)
{
	// reading VGPR is not supported on pre GFX9 devices
	if (asic->family < FAMILY_AI)
		return -1;

	if (asic->fd.gprwave >= 0) {
		return read_gpr_gprwave(asic, 1, thread, wd, dst);
	} else {
		asic->err_msg("[ERROR]:  Your kernel is too old the amdgpu_gprwave file is now required.\n");
		return -1;
	}
}

/**
 * @brief Reads raw wave status data from a specified GPU resource.
 *
 * This function reads the raw wave status data from a specified GPU resource identified by the provided parameters.
 * It uses an ioctl call to set up the necessary state and then performs a read operation on the file descriptor
 * associated with the GPR/wave debugfs entry.
 *
 * @param asic Pointer to the UMR ASIC structure representing the GPU.
 * @param se Shader Engine ID.
 * @param sh Shader Array ID.
 * @param cu Compute Unit ID.
 * @param simd SIMD ID.
 * @param wave Wave ID.
 * @param buf Pointer to the buffer where the read wave status data will be stored.
 *
 * @return Returns the number of bytes read on success, or a negative error code if an error occurs during the operation.
 */
int umr_get_wave_status_raw(struct umr_asic *asic, unsigned se, unsigned sh, unsigned cu, unsigned simd, unsigned wave, uint32_t *buf)
{
	int r = 0;
	uint64_t addr = 0;
	struct amdgpu_debugfs_gprwave_iocdata id;

	if (asic->fd.gprwave >= 0) {
		memset(&id, 0, sizeof id);
		id.gpr_or_wave = 0;
		id.se = se;
		id.sh = sh;
		id.cu = cu;
		id.wave = wave;
		id.simd = simd;
		id.xcc_id = asic->options.vm_partition == -1 ? 0 : asic->options.vm_partition;

		r = ioctl(asic->fd.gprwave, AMDGPU_DEBUGFS_GPRWAVE_IOC_SET_STATE, &id);
		if (r)
			return r;

		lseek(asic->fd.gprwave, 0, SEEK_SET);
		r = read(asic->fd.gprwave, buf, 64*4);
		if (r < 0)
			return r;
	} else {
		asic->err_msg("[ERROR]:  Your kernel is too old the amdgpu_gprwave file is now required.\n");
		return -1;
	}

	if (asic->options.test_log && asic->options.test_log_fd) {
		int x;
		addr = ((uint64_t)id.se << 7) |
			   ((uint64_t)id.sh << 15) |
			   ((uint64_t)id.cu << 23) |
			   ((uint64_t)id.wave << 31) |
			   ((uint64_t)id.simd << 37);
		fprintf(asic->options.test_log_fd, "WAVESTATUS@0x%"PRIx64" = { ", addr);
		for (x = 0; x < r; x += 4) {
			fprintf(asic->options.test_log_fd, "0x%"PRIx32, buf[x/4]);
			if (x < (r - 4))
				fprintf(asic->options.test_log_fd, ", ");
		}
		fprintf(asic->options.test_log_fd, "}\n");
	}

	return r;
}

/**
 * @brief Reads and parses wave status data from a specified GPU resource.
 *
 * This function reads the raw wave status data from a specified GPU resource using `umr_get_wave_status_raw()`
 * and then parses it into a structured format using `umr_parse_wave_data_gfx()`.
 *
 * @param asic Pointer to the UMR ASIC structure representing the GPU.
 * @param se Shader Engine ID.
 * @param sh Shader Array ID.
 * @param cu Compute Unit ID.
 * @param simd SIMD ID.
 * @param wave Wave ID.
 * @param ws Pointer to the UMR wave status structure where the parsed wave data will be stored.
 *
 * @return Returns 0 on success, or a negative error code if an error occurs during the read or parsing operation.
 */
int umr_get_wave_status(struct umr_asic *asic, unsigned se, unsigned sh, unsigned cu, unsigned simd, unsigned wave, struct umr_wave_status *ws)
{
	int r;
	uint32_t buf[64];

	memset(buf, 0, sizeof buf);
	r = umr_get_wave_status_raw(asic, se, sh, cu, simd, wave, buf);
	if (r > 0)
		return umr_parse_wave_data_gfx(asic, ws, buf, r>>2);
	else
		return -1;
}

int umr_get_wave_sq_info(struct umr_asic *asic, unsigned se, unsigned sh, unsigned cu, struct umr_wave_status *ws)
{
	return umr_get_wave_sq_info_vi(asic, se, sh, cu, ws);
}

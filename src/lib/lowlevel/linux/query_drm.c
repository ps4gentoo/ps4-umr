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
#include <asm/ioctl.h>
#include <sys/ioctl.h>

#ifndef UMR_NO_DRM
#include <drm.h>
#include <amdgpu_drm.h>

#define DRM_IOC(dir, group, nr, size) _IOC(dir, group, nr, size)
#define DRM_IOC_WRITE           _IOC_WRITE
#define DRM_IOCTL_BASE                  'd'
#define DRM_COMMAND_BASE                0x40

/**
 * umr_query_drm - Perform a DRM IOCTL for AMDGPU_INFO data
 *
 * @field - The field to retrieve via IOCTL
 */
int umr_query_drm(struct umr_asic *asic, int field, void *ret, int size)
{
	struct drm_amdgpu_info inf;

	memset(&inf, 0, sizeof inf);
	inf.return_pointer = (uintptr_t)ret;
	inf.return_size = size;
	inf.query = field;
	return ioctl(asic->fd.drm, DRM_IOC(DRM_IOC_WRITE, DRM_IOCTL_BASE, DRM_COMMAND_BASE + DRM_AMDGPU_INFO, sizeof(inf)), &inf);
}

int umr_query_drm_vbios(struct umr_asic *asic, int field, int type, void *ret, int size)
{
	struct drm_amdgpu_info inf;

	memset(&inf, 0, sizeof inf);
	inf.return_pointer = (uintptr_t)ret;
	inf.return_size = size;
	inf.query = field;
	inf.vbios_info.type = type;
	return ioctl(asic->fd.drm, DRM_IOC(DRM_IOC_WRITE, DRM_IOCTL_BASE, DRM_COMMAND_BASE + DRM_AMDGPU_INFO, sizeof(inf)), &inf);
}

#else

int umr_query_drm(struct umr_asic *asic, int field, void *ret, int size)
{
	return 0;
}

int umr_query_drm_vbios(struct umr_asic *asic, int field, int type, void *ret, int size)
{
	return 0;
}

#endif

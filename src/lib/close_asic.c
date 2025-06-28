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

#define cond_close(x) do { if ((x) >= 0) close((x)); } while(0);

/**
 * umr_close_asic - Close an asic device and free memory
 */
void umr_close_asic(struct umr_asic *asic)
{
	if (asic) {
		cond_close(asic->fd.mmio2);
		cond_close(asic->fd.mmio);
		cond_close(asic->fd.didt);
		cond_close(asic->fd.pcie);
		cond_close(asic->fd.smc);
		cond_close(asic->fd.sensors);
		cond_close(asic->fd.vram);
		cond_close(asic->fd.gprwave);
		cond_close(asic->fd.gpr);
		cond_close(asic->fd.wave);
		cond_close(asic->fd.drm);
		cond_close(asic->fd.iova);
		cond_close(asic->fd.iomem);
		cond_close(asic->fd.gfxoff);
		umr_free_asic(asic);
	}
}

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

/**
 * umr_ring_is_halted - Try to determine if a ring is actually halted
 *
 * @asic: The ASIC the ring is attached to.
 * @ringname: The name of the ring we want to check if it's halted.
 *
 * Returns 1 if it's halted, 0 or otherwise.
 */
int umr_ring_is_halted(struct umr_asic *asic, char *ringname)
{
	uint32_t *ringdata, ringsize;
	int n;

	if (!strcmp(ringname, "none"))
		return 1;

	// read ring data and reduce indeices modulo ring size
	// since the kernel returned values might be unwrapped.
	for (n = 0; n < 100; n++) {
		ringdata = asic->ring_func.read_ring_data(asic, ringname, &ringsize);
		if (!ringdata) {
			return 0;
		}
		ringsize /= 4;
		ringdata[0] %= ringsize;
		ringdata[1] %= ringsize;
		if (ringdata[0] == ringdata[1]) {
			free(ringdata);
			return 0;
		}
		free(ringdata);
		usleep(5);
	}

	return 1;
}

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

/**
 * umr_apply_bank_selection_address - Add bank selection offset
 *
 * Common function used to add on GRBM or SBRM bank selection
 * to an MMIO access via debugfs.
 */
uint64_t umr_apply_bank_selection_address(struct umr_asic *asic)
{
	// only apply bank selection if we're using the old debugfs
	if (asic->fd.mmio2 < 0) {
		if (asic->options.use_bank == 1) {
			return 	(1ULL << 62) |
					(((uint64_t)asic->options.bank.grbm.se & 0x3FFULL) << 24) |
					(((uint64_t)asic->options.bank.grbm.sh & 0x3FFULL) << 34) |
					(((uint64_t)asic->options.bank.grbm.instance & 0x3FFULL) << 44);
		} else if (asic->options.use_bank == 2) {
			return 	(1ULL << 61) |
					(((uint64_t)asic->options.bank.srbm.me & 0x3FFULL) << 24) |
					(((uint64_t)asic->options.bank.srbm.pipe & 0x3FFULL) << 34) |
					(((uint64_t)asic->options.bank.srbm.queue & 0x3FFULL) << 44) |
					(((uint64_t)asic->options.bank.srbm.vmid & 0x3FFULL) << 54);
		} else {
			return 0;
		}
	}
	return 0;
}

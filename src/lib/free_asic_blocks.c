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
 * umr_free_asic - Free memory associated with an asic device
 */
void umr_free_asic_blocks(struct umr_asic *asic)
{
	int x, y, z;
	for (x = 0; x < asic->no_blocks; x++) {
		if (asic->blocks[x]) {
			for (y = 0; y < asic->blocks[x]->no_regs; y++) {
				free(asic->blocks[x]->regs[y].regname);
				for (z = 0; z < asic->blocks[x]->regs[y].no_bits; z++) {
					free(asic->blocks[x]->regs[y].bits[z].regname);
				}
				free(asic->blocks[x]->regs[y].bits);
			}
			free(asic->blocks[x]->ipname);
			free(asic->blocks[x]->regs);
		}
		free(asic->blocks[x]);
	}
	free(asic->blocks);
	free(asic->mmio_accel);
	free(asic->asicname);
	free(asic);
}

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

static int sort_addr(const void *A, const void *B)
{
	const struct umr_mmio_accel_data *a = A, *b = B;
	if (a->mmio_addr > b->mmio_addr)
		return 1;
	if (a->mmio_addr < b->mmio_addr)
		return -1;
	if (a->ord > b->ord)
		return 1;
	if (a->ord < b->ord)
		return -1;
	return 0;
}

/**
 * umr_create_mmio_accel - Create MMIO accelerator table
 *
 * @asic:  Device to create accelerator for
 *
 * This function creates lookup tables that quickly convert
 * an offsetted MMIO address into a pointer to register and ip
 * block structures.
 */
int umr_create_mmio_accel(struct umr_asic *asic)
{
	int i, j;
	uint32_t no_regs, x;

	for (no_regs = i = 0; i < asic->no_blocks; i++) {
		for (j = 0; j < asic->blocks[i]->no_regs; j++) {
			if (asic->blocks[i]->regs[j].type == REG_MMIO) {
				++no_regs;
			}
		}
	}

	asic->mmio_accel = calloc(sizeof asic->mmio_accel[0], no_regs);
	asic->mmio_accel_size = no_regs;
	if (!asic->mmio_accel) {
		asic->err_msg("[ERROR]: Out of memory\n");
		return -1;
	}


	for (x = i = 0; i < asic->no_blocks; i++) {
		for (j = 0; j < asic->blocks[i]->no_regs; j++) {
			if (asic->blocks[i]->regs[j].type == REG_MMIO) {
				asic->mmio_accel[x].mmio_addr = asic->blocks[i]->regs[j].addr;
				asic->mmio_accel[x].ip = asic->blocks[i];
				asic->mmio_accel[x].reg = &asic->blocks[i]->regs[j];
				// ord is used to stabilize the sort; regs with same offset
				// appear in mmio_accel in the order they appeared in the
				// register database (i.e. alphabetically ascending)
				asic->mmio_accel[x].ord = x;
				++x;
			}
		}
	}

	qsort(asic->mmio_accel, no_regs, sizeof asic->mmio_accel[0], sort_addr);

	return 0;
}

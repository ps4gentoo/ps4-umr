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

static int hive_cmp(const void *a, const void *b)
{
	const struct umr_xgmi_hive_info *A = a, *B = b;
	return (A->hive_position > B->hive_position);
}

/**
 * umr_apply_callbacks - Apply the reg/mem callbacks to each ASIC node in an XGMI hive
 *
 * @asic: The ASIC the user connected to
 * @mems: The memory callbacks
 * @regs: The register callbacks
 */
void umr_apply_callbacks(struct umr_asic *asic,
			 struct umr_memory_access_funcs *mems,
			 struct umr_register_access_funcs *regs)
{
	int n;
	struct umr_reg *reg;

	n = 0;
	while (asic->config.xgmi.nodes[n].asic) {
		reg = umr_find_reg_by_name(asic->config.xgmi.nodes[n].asic, "mmMC_VM_XGMI_LFB_CNTL", NULL);
		if (!reg) {
			asic->err_msg("[BUG]: Cannot find register mmMC_VM_XGMI_LFB_CNTL on ASIC\n");
			return;
		}

		asic->config.xgmi.nodes[n].asic->mem_funcs = *mems;
		asic->config.xgmi.nodes[n].asic->reg_funcs = *regs;
		asic->config.xgmi.nodes[n].hive_position = umr_bitslice_reg(asic->config.xgmi.nodes[n].asic, reg, "PF_LFB_REGION", asic->reg_funcs.read_reg(asic->config.xgmi.nodes[n].asic, reg->addr * 4, REG_MMIO));
		++n;
	}

	// sort nodes based on hive position
	qsort(&asic->config.xgmi.nodes[0], n, sizeof(asic->config.xgmi.nodes[0]), hive_cmp);
	asic->config.xgmi.callbacks_applied = 1;
}

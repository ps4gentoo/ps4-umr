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
#ifndef UMR_VM_H_
#define UMR_VM_H_

// memory access
struct umr_vm_pagewalk {
	int levels,
		sys_or_vram;
	uint32_t vmid;
	uint64_t va, phys;
	uint64_t pde[8], pte;
	pde_fields_t pde_fields[8];
	pte_fields_t pte_fields;
	struct {
		uint64_t page_table_base_addr;
	} registers;
};

int umr_access_vram_via_mmio(struct umr_asic *asic, uint64_t address, uint32_t size, void *dst, int write_en);
uint64_t umr_vm_dma_to_phys(struct umr_asic *asic, uint64_t dma_addr);
int umr_access_sram(struct umr_asic *asic, uint64_t address, uint32_t size, void *dst, int write_en);
int umr_access_vram(struct umr_asic *asic, int partition, uint32_t vmid, uint64_t address, uint32_t size, void *data, int write_en, struct umr_vm_pagewalk *vmdata);
int umr_access_linear_vram(struct umr_asic *asic, uint64_t address, uint32_t size, void *data, int write_en);
#define umr_read_vram(asic, partition, vmid, address, size, dst) umr_access_vram(asic, partition, vmid, address, size, dst, 0, NULL)
#define umr_write_vram(asic, partition, vmid, address, size, src) umr_access_vram(asic, partition, vmid, address, size, src, 1, NULL)

#endif

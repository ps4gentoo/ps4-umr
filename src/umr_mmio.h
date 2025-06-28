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
#ifndef UMR_MMIO_H_
#define UMR_MMIO_H_

/* ==== MMIO Functions ====
 * These functions deal with looking up, reading, and writing registers and bitslices.
 */
// init the mmio lookup table
int umr_create_mmio_accel(struct umr_asic *asic);

// find ip block with optional instance
struct umr_ip_block *umr_find_ip_block(const struct umr_asic *asic, const char *ipname, int instance);

// find the word address of a register
uint32_t umr_find_reg(struct umr_asic *asic, const char *regname);

// wildcard searches
struct umr_find_reg_iter *umr_find_reg_wild_first(struct umr_asic *asic, const char *ip, const char *reg);
struct umr_find_reg_iter_result umr_find_reg_wild_next(struct umr_find_reg_iter **iterp);

// find a register and return a printable name (used for human readable output)
char *umr_reg_name(struct umr_asic *asic, uint64_t addr);

// find the register data for a register
struct umr_reg* umr_find_reg_data_by_ip_by_instance_with_ip(struct umr_asic* asic, const char* ip, int inst, const char* regname, struct umr_ip_block **ipp);
struct umr_reg* umr_find_reg_data_by_ip_by_instance(struct umr_asic* asic, const char* ip, int inst, const char* regname);
struct umr_reg *umr_find_reg_data_by_ip(struct umr_asic *asic, const char *ip, const char *regname);
struct umr_reg *umr_find_reg_data_by_ip_quiet(struct umr_asic *asic, const char *ip, const char *regname);
struct umr_reg *umr_find_reg_by_name(struct umr_asic *asic, const char *regname, struct umr_ip_block **ip);
struct umr_reg *umr_find_reg_by_addr(struct umr_asic *asic, uint64_t addr, struct umr_ip_block **ip);

// read/write a 32-bit register given a BYTE address
uint32_t umr_read_reg(struct umr_asic *asic, uint64_t addr, enum regclass type);
int umr_write_reg(struct umr_asic *asic, uint64_t addr, uint32_t value, enum regclass type);

// read/write a register given a name
uint64_t umr_read_reg_by_name(struct umr_asic *asic, char *name);
int umr_write_reg_by_name(struct umr_asic *asic, char *name, uint64_t value);

// read/write a register by ip/inst/name
int umr_write_reg_by_name_by_ip_by_instance(struct umr_asic *asic, char *ip, int inst, char *name, uint64_t value);
uint64_t umr_read_reg_by_name_by_ip_by_instance(struct umr_asic *asic, char *ip, int inst, char *name);

// read/write a register by ip/name
uint64_t umr_read_reg_by_name_by_ip(struct umr_asic *asic, char *ip, char *name);
int umr_write_reg_by_name_by_ip(struct umr_asic *asic, char *ip, char *name, uint64_t value);

// slice a full register into bits (shifted into LSB)
uint64_t umr_bitslice_reg(struct umr_asic *asic, struct umr_reg *reg, char *bitname, uint64_t regvalue);
uint64_t umr_bitslice_reg_quiet(struct umr_asic *asic, struct umr_reg *reg, char *bitname, uint64_t regvalue);
uint64_t umr_bitslice_reg_by_name(struct umr_asic *asic, char *regname, char *bitname, uint64_t regvalue);
uint64_t umr_bitslice_reg_by_name_by_ip(struct umr_asic *asic, char *ip, char *regname, char *bitname, uint64_t regvalue);
uint64_t umr_bitslice_reg_by_name_by_ip_by_instance(struct umr_asic *asic, char *ip, int instance, char *regname, char *bitname, uint64_t regvalue);

// compose a 64-bit register with a value and a bitfield
uint64_t umr_bitslice_compose_value(struct umr_asic *asic, struct umr_reg *reg, char *bitname, uint64_t regvalue);
uint64_t umr_bitslice_compose_value_by_name(struct umr_asic *asic, char *reg, char *bitname, uint64_t regvalue);
uint64_t umr_bitslice_compose_value_by_name_by_ip(struct umr_asic *asic, char *ip, char *regname, char *bitname, uint64_t regvalue);
uint64_t umr_bitslice_compose_value_by_name_by_ip_by_instance(struct umr_asic *asic, char *ip, int instance, char *regname, char *bitname, uint64_t regvalue);

// bank switching
uint64_t umr_apply_bank_selection_address(struct umr_asic *asic);

// select a GRBM_GFX_IDX
int umr_grbm_select_index(struct umr_asic *asic, uint32_t se, uint32_t sh, uint32_t instance);
int umr_srbm_select_index(struct umr_asic *asic, uint32_t me, uint32_t pipe, uint32_t queue, uint32_t vmid);

#endif

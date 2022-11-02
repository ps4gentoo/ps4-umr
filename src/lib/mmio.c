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
 * umr_read_reg_by_name - Read a register by name
 *
 * Reads the value of a register with a specified @name from the first
 * IP block found to contain the register.  To specify an IP block
 * use: umr_read_reg_by_name_by_ip().
 */
uint64_t umr_read_reg_by_name(struct umr_asic *asic, char *name)
{
	return umr_read_reg_by_name_by_ip(asic, NULL, name);
}

/**
 * umr_read_reg_by_name_by_ip - Read a register by name and IP block
 *
 * Reads the value of a register with a specified @name in a specified
 * @ip block.  The IP block can be NULL to find the first instance
 * of the register in the ASIC.
 */
uint64_t umr_read_reg_by_name_by_ip(struct umr_asic *asic, char *ip, char *name)
{
	return umr_read_reg_by_name_by_ip_by_instance(asic, ip, -1, name);
}

/**
 * umr_read_reg_by_name_by_ip_by_instance - Read a register by
 * name and IP block and instance.
 *
 * Reads the value of a register with a specified @name in the specified
 * @inst instance of the @ip block.  The IP block can be NULL to
 * find the first instance of the register in the ASIC.
 */
uint64_t umr_read_reg_by_name_by_ip_by_instance(struct umr_asic *asic, char *ip, int inst, char *name)
{
	struct umr_reg *reg;
	reg = umr_find_reg_data_by_ip_by_instance(asic, ip, inst, name);
	if (reg) {
		if (reg->bit64)
			return ((uint64_t)asic->reg_funcs.read_reg(asic, reg->addr * (reg->type == REG_MMIO ? 4 : 1), reg->type)) |
			((uint64_t)asic->reg_funcs.read_reg(asic, (reg->addr + 1) * (reg->type == REG_MMIO ? 4 : 1), reg->type) << 32);
		else
			return asic->reg_funcs.read_reg(asic, reg->addr * (reg->type == REG_MMIO ? 4 : 1), reg->type);
	} else {
		return 0;
	}
}

/**
 * umr_write_reg_by_name_by_ip - Write to a register by name and IP block
 *
 * Writes the @value specified to the regisrer with a specified @name in
 * a specified @ip block.  The IP block can be NULL to find the
 * first instance of the register in the ASIC.
 */
int umr_write_reg_by_name_by_ip(struct umr_asic *asic, char *ip, char *name, uint64_t value)
{
	return umr_write_reg_by_name_by_ip_by_instance(asic, ip, -1, name, value);
}


/**
 * umr_write_reg_by_name_by_ip_by_instance - Write to a register by
 * name and IP block and instance.
 *
 * Writes the @value specified to the regisrer with a specified @name in
 * the specified @inst instance of an @ip block.  The IP block can be
 * NULL to find the first instance of the register in the ASIC.
 */
int umr_write_reg_by_name_by_ip_by_instance(struct umr_asic *asic, char *ip, int inst, char *name, uint64_t value)
{
	struct umr_reg *reg;
	int r;

	reg = umr_find_reg_data_by_ip_by_instance(asic, ip, inst, name);
	if (reg) {
		if (reg->bit64) {
			r = asic->reg_funcs.write_reg(asic, reg->addr * (reg->type == REG_MMIO ? 4 : 1), value & 0xFFFFFFFFUL, reg->type);
			if (!r)
				return asic->reg_funcs.write_reg(asic, (reg->addr + 1) * (reg->type == REG_MMIO ? 4 : 1), value >> 32, reg->type);
			return r;
		} else {
			return asic->reg_funcs.write_reg(asic, reg->addr * (reg->type == REG_MMIO ? 4 : 1), value, reg->type);
		}
	} else {
		return -1;
	}
}

/**
 * umr_write_reg_by_name - Write to a register by name
 *
 * Writes the @value specified to the register with a specified @name
 * from the first IP block found to contain the register.  To specify
 * an IP block use: umr_write_reg_by_name_by_ip().
 */
int umr_write_reg_by_name(struct umr_asic *asic, char *name, uint64_t value)
{
	return umr_write_reg_by_name_by_ip(asic, NULL, name, value);
}


/**
 * umr_bitslice_reg_quiet - Slice a register value by a bitfield
 *
 * Returns the value of a bitfield of a register specified by
 * @reg by the bitfield parameters specified by the name @bitname.
 * The entire register value must be specified by @regvalue.
 *
 * This function will return 2^32 - 1 if the register is not found
 * so it cannot be used for bitfields that are a full 32-bits wide
 */
uint64_t umr_bitslice_reg_quiet(struct umr_asic *asic, struct umr_reg *reg, char *bitname, uint64_t regvalue)
{
	int i;
	(void)asic;
	for (i = 0; i < reg->no_bits; i++) {
		if (!strcmp(bitname, reg->bits[i].regname)) {
			regvalue >>= reg->bits[i].start;
			regvalue &= (1ULL << (reg->bits[i].stop - reg->bits[i].start + 1)) - 1;
			return regvalue;
		}
	}
	return 0xFFFFFFFFULL;
}

/**
 * umr_bitslice_reg - Slice a register value by a bitfield
 *
 * Returns the value of a bitfield of a register specified by
 * @reg by the bitfield parameters specified by the name @bitname.
 * The entire register value must be specified by @regvalue.
 */
uint64_t umr_bitslice_reg(struct umr_asic *asic, struct umr_reg *reg, char *bitname, uint64_t regvalue)
{
	int i;
	for (i = 0; i < reg->no_bits; i++) {
		if (!strcmp(bitname, reg->bits[i].regname)) {
			regvalue >>= reg->bits[i].start;
			regvalue &= (1ULL << (reg->bits[i].stop - reg->bits[i].start + 1)) - 1;
			return regvalue;
		}
	}
	asic->err_msg("[BUG]: Bitfield [%s] not found in reg [%s] on asic [%s]\n", bitname, reg->regname, asic->asicname);
	return 0;
}

/**
 * umr_bitslice_compose_value - Shift a value into position for a bitfield
 *
 * Masks and shifts a value @regvalue specified for the bitfield
 * indicated by @bitname for the register @reg.  The returned
 * value can be OR'ed with other composed values to complete an
 * entire register word.
 */
uint64_t umr_bitslice_compose_value(struct umr_asic *asic, struct umr_reg *reg, char *bitname, uint64_t regvalue)
{
	int i;
	for (i = 0; i < reg->no_bits; i++) {
		if (!strcmp(bitname, reg->bits[i].regname)) {
			regvalue &= (1ULL << (reg->bits[i].stop - reg->bits[i].start + 1)) - 1;
			regvalue <<= reg->bits[i].start;
			return regvalue;
		}
	}
	asic->err_msg("[BUG]: Bitfield [%s] not found in reg [%s] on asic [%s]\n", bitname, reg->regname, asic->asicname);
	return 0;
}

/**
 * umr_bitslice_reg_by_name_by_ip - Slice out a bitfield by IP and register name.
 */
uint64_t umr_bitslice_reg_by_name_by_ip(struct umr_asic *asic, char *ip, char *regname, char *bitname, uint64_t regvalue)
{
	struct umr_reg *reg;
	reg = umr_find_reg_data_by_ip(asic, ip, regname);
	if (reg)
		return umr_bitslice_reg(asic, reg, bitname, regvalue);
	else
		return 0;
}

/**
 * umr_bitslice_reg_by_name - Slice out a bitfield by register name.
 */
uint64_t umr_bitslice_reg_by_name(struct umr_asic *asic, char *regname, char *bitname, uint64_t regvalue)
{
	return umr_bitslice_reg_by_name_by_ip(asic, NULL, regname, bitname, regvalue);
}

/**
 * umr_bitslice_compose_value_by_name_by_ip - Compose a bitfield
 *
 * Compose a bitfield with the specified @regvalue value for the
 * bitfield named @bitname in the register @regname in the IP block
 * @ip.  The @ip name can be NULL to search for the first matching
 * register in the ASIC.
 *
 * Returns the masked and shifted bitfield value that can be OR'ed
 * with other composed bitfields to form a register value.
 */
uint64_t umr_bitslice_compose_value_by_name_by_ip(struct umr_asic *asic, char *ip, char *regname, char *bitname, uint64_t regvalue)
{
	struct umr_reg *reg;
	reg = umr_find_reg_data_by_ip(asic, ip, regname);
	if (reg)
		return umr_bitslice_compose_value(asic, reg, bitname, regvalue);
	else
		return 0;
}

/**
 * umr_bitslice_compose_value_by_name - Compose a bitfield
 *
 * Compose a bitfield with the specified @regvalue value for the
 * bitfield named @bitname in the register @regname.
 *
 * Returns the masked and shifted bitfield value that can be OR'ed
 * with other composed bitfields to form a register value.
 */
uint64_t umr_bitslice_compose_value_by_name(struct umr_asic *asic, char *regname, char *bitname, uint64_t regvalue)
{
	return umr_bitslice_compose_value_by_name_by_ip(asic, NULL, regname, bitname, regvalue);
}

/**
 * umr_grbm_select_index - Select a GRBM instance
 *
 * Selects via MMIO writes a specific GRBM instance by passing the
 * kernel's control.
 */
int umr_grbm_select_index(struct umr_asic *asic, uint32_t se, uint32_t sh, uint32_t instance)
{
	struct umr_reg *grbm_idx;
	uint32_t data = 0;
	int bank, r;

	grbm_idx = umr_find_reg_data(asic, "mmGRBM_GFX_INDEX");
	if (grbm_idx) {
		if (instance == 0xFFFFFFFFUL) {
			data |= umr_bitslice_compose_value(asic, grbm_idx, "INSTANCE_BROADCAST_WRITES", 1);
		} else {
			data |= umr_bitslice_compose_value(asic, grbm_idx, "INSTANCE_INDEX", instance);
		}
		if (se == 0xFFFFFFFFUL) {
			data |= umr_bitslice_compose_value(asic, grbm_idx, "SE_BROADCAST_WRITES", 1);
		} else {
			data |= umr_bitslice_compose_value(asic, grbm_idx, "SE_INDEX", se);
		}
		if (asic->family < FAMILY_NV) {
			if (sh == 0xFFFFFFFFUL) {
				data |= umr_bitslice_compose_value(asic, grbm_idx, "SH_BROADCAST_WRITES", 1);
			} else {
				data |= umr_bitslice_compose_value(asic, grbm_idx, "SH_INDEX", sh);
			}
		} else {
			if (sh == 0xFFFFFFFFUL) {
				data |= umr_bitslice_compose_value(asic, grbm_idx, "SA_BROADCAST_WRITES", 1);
			} else {
				data |= umr_bitslice_compose_value(asic, grbm_idx, "SA_INDEX", sh);
			}
		}
		bank = asic->options.use_bank;
		asic->options.use_bank = 0;
		r = umr_write_reg(asic, grbm_idx->addr * 4, data, REG_MMIO);
		asic->options.use_bank = bank;
		return r;
	} else {
		return -1;
	}
}

int umr_srbm_select_index(struct umr_asic *asic, uint32_t me, uint32_t pipe, uint32_t queue, uint32_t vmid)
{
	struct umr_reg *srbm_idx;
	uint32_t data = 0;
	int bank, r;

	if (asic->family >= FAMILY_AI)
		srbm_idx = umr_find_reg_data(asic, "mmGRBM_GFX_CNTL");
	else
		srbm_idx = umr_find_reg_data(asic, "mmSRBM_GFX_CNTL");

	if (srbm_idx) {
		data |= umr_bitslice_compose_value(asic, srbm_idx, "PIPEID", pipe);
		data |= umr_bitslice_compose_value(asic, srbm_idx, "MEID", me);
		data |= umr_bitslice_compose_value(asic, srbm_idx, "VMID", vmid);
		data |= umr_bitslice_compose_value(asic, srbm_idx, "QUEUEID", queue);
		bank = asic->options.use_bank;
		asic->options.use_bank = 0;
		r = umr_write_reg(asic, srbm_idx->addr * 4, data, REG_MMIO);
		asic->options.use_bank = bank;
		return r;
	} else {
		return -1;
	}
}

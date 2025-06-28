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

static struct umr_reg *find_sq_cmd(struct umr_asic *asic)
{
	// SQ_CMD is not present on SI
	if (asic->family == FAMILY_SI)
		return 0;

	struct umr_reg *reg = umr_find_reg_data_by_ip_by_instance(asic, "gfx", asic->options.vm_partition,
						  asic->family >= FAMILY_GFX11 ? "regSQ_CMD" : "mmSQ_CMD");
	if (!reg)
		asic->err_msg("[BUG]: Cannot find SQ_CMD register in umr_sq_cmd_halt_waves()\n");
	return reg;
}

/**
 * umr_sq_cmd_halt_waves - Attempt to halt or resume waves
 *
 * @mode:	Use UMR_SQ_CMD_HALT to halt waves and
 * 			UMR_SQ_CMD_RESUME to resume waves.
 * @max_retries:	If > 0 halting the waves will be retried if it failed
 */
int umr_sq_cmd_halt_waves(struct umr_asic *asic, enum umr_sq_cmd_halt_resume mode, int max_retries)
{
	struct umr_reg *reg;
	uint32_t value;
	uint64_t addr;
	struct {
		uint32_t se, sh, instance, use_grbm;
	} grbm;

	reg = find_sq_cmd(asic);
	if (!reg)
		return -1;

	// compose value
	if (asic->family == FAMILY_CIK) {
		value = umr_bitslice_compose_value(asic, reg, "CMD", mode == UMR_SQ_CMD_HALT ? 1 : 2); // SETHALT
	} else {
		value = umr_bitslice_compose_value(asic, reg, "CMD", 1); // SETHALT
		value |= umr_bitslice_compose_value(asic, reg, "DATA", mode == UMR_SQ_CMD_HALT ? 1 : 0);
	}
	value |= umr_bitslice_compose_value(asic, reg, "MODE", 1); // BROADCAST

	/* copy grbm options to restore later */
	grbm.use_grbm = asic->options.use_bank;
	grbm.se       = asic->options.bank.grbm.se;
	grbm.sh       = asic->options.bank.grbm.sh;
	grbm.instance = asic->options.bank.grbm.instance;

	/* set GRBM banking options */
	asic->options.use_bank           = 1;
	asic->options.bank.grbm.se       = 0xFFFFFFFF;
	asic->options.bank.grbm.sh       = 0xFFFFFFFF;
	asic->options.bank.grbm.instance = 0xFFFFFFFF;

	// compose address
	addr = reg->addr * 4;

send_cmd:
	asic->reg_funcs.write_reg(asic, addr, value, reg->type);

	if (mode == UMR_SQ_CMD_HALT &&
		max_retries > 0 &&
		!umr_ring_is_halted(asic, asic->options.ring_name)) {
		usleep(100);
		max_retries--;
		goto send_cmd;
	}

	/* restore whatever the user had picked */
	asic->options.use_bank           = grbm.use_grbm;
	asic->options.bank.grbm.se       = grbm.se;
	asic->options.bank.grbm.sh       = grbm.sh;
	asic->options.bank.grbm.instance = grbm.instance;

	return mode == UMR_SQ_CMD_HALT && !umr_ring_is_halted(asic, asic->options.ring_name) ?
		-1 : 0;
}

/**
 * umr_sq_cmd_singlestep - Attempt to single-step a single wave
 *
 * @asic: The ASIC to issue the single-step to
 * @se: The SE selection
 * @sh: The SH selection
 * @wgp: The WGP selection
 * @simd: The SIMD selection
 * @wave: The WAVE selection
 *
 * The wave is assumed to be halted.  Returns -1 on error.
 */
int umr_sq_cmd_singlestep(struct umr_asic *asic, uint32_t se, uint32_t sh, uint32_t wgp, uint32_t simd, uint32_t wave)
{
	struct umr_reg *reg;
	uint32_t value;
	uint64_t addr;
	struct {
		uint32_t se, sh, instance, use_grbm;
	} grbm;

	if (asic->family < FAMILY_NV)
		return -1; // Only supported on gfx10+

	reg = find_sq_cmd(asic);
	if (!reg)
		return -1;

	// compose value
	value = umr_bitslice_compose_value(asic, reg, "CMD", 8); // SINGLE_STEP
	value |= umr_bitslice_compose_value(asic, reg, "MODE", 0); // single wave
	value |= umr_bitslice_compose_value(asic, reg, "WAVE_ID", wave);

	/* copy grbm options to restore later */
	grbm.use_grbm = asic->options.use_bank;
	grbm.se       = asic->options.bank.grbm.se;
	grbm.sh       = asic->options.bank.grbm.sh;
	grbm.instance = asic->options.bank.grbm.instance;

	/* set GRBM banking options */
	asic->options.use_bank           = 1;
	asic->options.bank.grbm.se       = se;
	asic->options.bank.grbm.sh       = sh;
	asic->options.bank.grbm.instance = (wgp << 2) | simd;

	// compose address
	addr = reg->addr * 4;
	asic->reg_funcs.write_reg(asic, addr, value, reg->type);

	/* restore whatever the user had picked */
	asic->options.use_bank           = grbm.use_grbm;
	asic->options.bank.grbm.se       = grbm.se;
	asic->options.bank.grbm.sh       = grbm.sh;
	asic->options.bank.grbm.instance = grbm.instance;

	return 0;
}

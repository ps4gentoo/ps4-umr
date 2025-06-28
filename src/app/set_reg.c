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
#include "umrapp.h"
#include <inttypes.h>

/* set a register based on regpath e.g.
 *
 * stoney.uvd6.mmFOO
 */
int umr_set_register(struct umr_asic *asic, char *regpath, char *regvalue)
{
	char asicname[128], ipname[128], regname[128];
	int i, j;
	uint64_t value, scale;
	uint32_t v32;

	if (sscanf(regpath, "%[^.].%[^.].%[^.]", asicname, ipname, regname) != 3) {
		fprintf(stderr, "[ERROR]: Invalid regpath for write\n");
		return -1;
	}

	if (asicname[0] == '*' || !strcmp(asicname, asic->asicname)) {
		// scan all ip blocks for matching entry
		for (i = 0; i < asic->no_blocks; i++) {
			if (ipname[0] == '*' || !strcmp(ipname, asic->blocks[i]->ipname)) {
				for (j = 0; j < asic->blocks[i]->no_regs; j++) {
					if (!strcmp(regname, asic->blocks[i]->regs[j].regname)) {
						sscanf(regvalue, "%"SCNx64, &value);

						// set this register
						switch (asic->blocks[i]->regs[j].type){
							case REG_MMIO: scale = 4; break;
							case REG_DIDT: scale = 1; break;
							case REG_PCIE: scale = 1; break;
							case REG_SMC:  scale = 1; break;
							default: return -1;
						}

						v32 = value & 0xFFFFFFFF;
						asic->reg_funcs.write_reg(asic, asic->blocks[i]->regs[j].addr*scale, v32, asic->blocks[i]->regs[j].type);
						if (asic->blocks[i]->regs[j].bit64) {
							v32 = value >> 32;
							asic->reg_funcs.write_reg(asic, (asic->blocks[i]->regs[j].addr+1)*scale, v32, asic->blocks[i]->regs[j].type);
						}

						return 0;
					}
				}
			}
		}
	}

	if (!memcmp(regname, "reg", 3)) {
		fprintf(stderr, "[ERROR]: Path <%s> not found on this ASIC\n", regpath);
		return -1;
	} else {
		char newregpath[512];
		sprintf(newregpath, "%s.%s.reg%s", asicname, ipname, regname + 2);
		fprintf(stderr, "[WARNING]: Retrying operation with new 'reg' prefix path <%s>.\n", newregpath);
		return umr_set_register(asic, newregpath, regvalue);
	}
}

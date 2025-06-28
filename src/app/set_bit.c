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
 * stoney.uvd6.mmFOO.bit
 */
int umr_set_register_bit(struct umr_asic *asic, char *regpath, char *regvalue)
{
	char asicname[128], ipname[128], regname[128], bitname[128];
	int i, j, k;
	uint32_t value;
	uint64_t scale, copy, mask;

	if (sscanf(regpath, "%[^.].%[^.].%[^.].%[^.]", asicname, ipname, regname, bitname) != 4) {
		fprintf(stderr, "[ERROR]: Invalid regpath for bit write\n");
		return -1;
	}

	if (asicname[0] == '*' || !strcmp(asicname, asic->asicname)) {
		/* scan until we compare with regpath... */
		for (i = 0; i < asic->no_blocks; i++) {
			if (ipname[0] == '*' || !strcmp(ipname, asic->blocks[i]->ipname)) {
				for (j = 0; j < asic->blocks[i]->no_regs; j++) {
					if (!strcmp(regname, asic->blocks[i]->regs[j].regname) && asic->blocks[i]->regs[j].bits) {
						for (k = 0; k < asic->blocks[i]->regs[j].no_bits; k++) {
							if (!strcmp(bitname, asic->blocks[i]->regs[j].bits[k].regname)) {
								sscanf(regvalue, "%"SCNx32, &value);

								mask = (1ULL<<((1+asic->blocks[i]->regs[j].bits[k].stop)-asic->blocks[i]->regs[j].bits[k].start))-1;
								mask <<= asic->blocks[i]->regs[j].bits[k].start;

								// set this register
								switch (asic->blocks[i]->regs[j].type){
									case REG_MMIO: scale = 4; break;
									case REG_DIDT: scale = 1; break;
									case REG_PCIE: scale = 1; break;
									case REG_SMC:  scale = 1; break;
									default: return -1;
								}

								copy = asic->reg_funcs.read_reg(asic, asic->blocks[i]->regs[j].addr*scale, asic->blocks[i]->regs[j].type);
								if (asic->blocks[i]->regs[j].bit64) {
									copy |= (uint64_t)asic->reg_funcs.read_reg(asic, (asic->blocks[i]->regs[j].addr+1)*scale, asic->blocks[i]->regs[j].type) << 32;
								}
								// read-modify-write value back
								copy &= ~mask;
								value = ((uint64_t)value << asic->blocks[i]->regs[j].bits[k].start) & mask;
								copy |= value;
								asic->reg_funcs.write_reg(asic, asic->blocks[i]->regs[j].addr*scale, copy & 0xFFFFFFFFUL, asic->blocks[i]->regs[j].type);
								if (asic->blocks[i]->regs[j].bit64) {
									asic->reg_funcs.write_reg(asic, (asic->blocks[i]->regs[j].addr+1)*scale, copy>>32UL, asic->blocks[i]->regs[j].type);
								}

								if (!asic->options.quiet) printf("%s <= 0x%" PRIx64 "\n", regpath, (unsigned long)copy);
								return 0;
							}
						}
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
		sprintf(newregpath, "%s.%s.reg%s.%s", asicname, ipname, regname + 2, bitname);
		fprintf(stderr, "[WARNING]: Retrying operation with new 'reg' prefix path <%s>.\n", newregpath);
		return umr_set_register_bit(asic, newregpath, regvalue);
	}
}


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

void umr_scan_log(struct umr_asic *asic, int use_new)
{
	char line[256], *chr;
	FILE *f;
	int k, found;
	unsigned long delta, did, regno, value, write;
	struct umr_reg *reg;
	struct umr_ip_block *ip;
	char *prefix, *rreg, *wreg;

	if (use_new) {
		prefix = "amdgpu_device_";
		rreg = "amdgpu_device_rreg: 0x%08lx, 0x%08lx, 0x%08lx";
		wreg = "amdgpu_device_wreg: 0x%08lx, 0x%08lx, 0x%08lx";
	} else {
		prefix = "amdgpu_mm_";
		rreg = "amdgpu_mm_rreg: 0x%08lx, 0x%08lx, 0x%08lx";
		wreg = "amdgpu_mm_wreg: 0x%08lx, 0x%08lx, 0x%08lx";
	}

	f = fopen("/sys/kernel/debug/tracing/trace", "r");
	if (!f) {
		perror("Could not open ftrace log");
		return;
	}

	while (fgets(line, sizeof(line), f)) {
		found = 0;
		delta = 0;
		write = 0;

		chr = strstr(line, prefix);
		if (chr) {
			if (sscanf(chr, rreg,
				   &did, &regno, &value) != 3) {
				write = 1;
				if (sscanf(chr, wreg,
					&did, &regno, &value) != 3)
					continue;
			}

			if (did == asic->did) {
				do {
					reg = umr_find_reg_by_addr(asic, regno, &ip);
					if (reg) {
						// bingo
						if (write)
							printf("%s.%s.%s +0x%04lx <= 0x%08lx\n",
								asic->asicname, ip->ipname, reg->regname,
								(unsigned long)delta,
								(unsigned long)value);
						else
							printf("%s.%s.%s +0x%04lx => 0x%08lx\n",
								asic->asicname, ip->ipname, reg->regname,
								(unsigned long)delta,
								(unsigned long)value);
						if (asic->options.bitfields)
							for (k = 0; k < reg->no_bits; k++) {
								uint32_t v;
								v = (1UL << (reg->bits[k].stop + 1 - reg->bits[k].start)) - 1;
								v &= (value >> reg->bits[k].start);
								reg->bits[k].bitfield_print(asic, asic->asicname, ip->ipname, reg->regname, reg->bits[k].regname, reg->bits[k].start, reg->bits[k].stop, v);
							}
						found = 1;
					}
					regno -= 1;
					delta += 1;
				} while (!found);
			}
		}
	}
	fclose(f);
	if (asic->options.empty_log) {
		f = fopen("/sys/kernel/debug/tracing/trace", "w");
		if (f)
			fclose(f);
	}
}


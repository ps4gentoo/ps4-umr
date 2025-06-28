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

/**
 * umr_bitfield_default - Default helper to print out a bitfield
 *
 * @asic - The device associated with the bitfield
 * @asicname, @ipname, @regname, @bitname - The path to the bitfield
 * @start, @stop - The alignment of the bitfield in the word
 * @value - The shifted and masked value to print
 *
 * This default helper simply prints out the value in decimal and
 * hexadecimal.  The callback is meant to allow for custom printing
 * of bitfields for more complicated registers.
 */
void umr_bitfield_default(struct umr_asic *asic, char *asicname, char *ipname, char *regname, char *bitname, int start, int stop, uint32_t value)
{
	char buf[512], fpath[256];
	struct umr_options options = asic->options;
	if (options.bitfields) {
		snprintf(fpath, sizeof(fpath)-1, "%s.%s.%s%s%s.", asicname, ipname, CYAN, regname, RST);
		snprintf(buf, sizeof(buf)-1, "\t%s%s%s%s[%s%d:%d%s]",
			options.bitfields_full ? fpath : ".",
			RED, bitname, RST,
			BLUE, start, stop, RST);
		printf("%-65s == %s%8lu%s (%s0x%08lx%s)\n", buf,
			YELLOW, (unsigned long)value, RST,
			YELLOW, (unsigned long)value, RST);
	}
}


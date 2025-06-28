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
#ifndef UMR_CLOCK_H_
#define UMR_CLOCK_H_

//clock
struct umr_clock_source {
	char clock_name[32];
	uint32_t clock_Mhz[10];
	int clock_level;
	int current_clock;
};

struct umr_asic_clocks {
	struct umr_asic *asic;
	struct umr_clock_source clocks[UMR_CLOCK_MAX];
};

int umr_read_clock(struct umr_asic *asic, char* clockname, struct umr_clock_source* clock);
int umr_set_clock(struct umr_asic *asic, const char* clock_name, void* value);
void umr_set_clock_performance(struct umr_asic *asic, const char* operation);
int umr_check_clock_performance(struct umr_asic *asic, char* name, uint32_t len);
void umr_gfxoff_read(struct umr_asic *asic);

#endif

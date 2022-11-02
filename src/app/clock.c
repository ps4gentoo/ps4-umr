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
 */
#include "umrapp.h"
#include <ncurses.h>

static struct umr_asic_clocks asic_clocks = {
	NULL,
	{{"sclk", {0}, 0, 0 },
	{ "mclk", {0}, 0, 0 },
	{ "pcie", {0}, 0, 0 },
	{ "fclk", {0}, 0, 0 },
	{ "socclk", {0}, 0, 0 },
	{ "dcefclk", {0}, 0, 0 },},
};

static void print_clock(struct umr_clock_source clock_source, struct umr_asic *asic)
{
	int i = 0;
	if (strlen(clock_source.clock_name) && clock_source.clock_level != 0) {
		printf("%s:\n", clock_source.clock_name);
		for (i = 0; i < clock_source.clock_level; i++){
			if (i == clock_source.current_clock)
				printf("\t%s %d %u MHz * %s\n", YELLOW, i, clock_source.clock_Mhz[i], RST);
			else
				printf("\t %d %u MHz\n", i, clock_source.clock_Mhz[i]);
		}
	}
}

static void print_pcie_clock(struct umr_asic *asic)
{
	FILE* fp = NULL;
	char name[256];
	snprintf(name, sizeof(name)-1, \
		"/sys/class/drm/card%d/device/pp_dpm_pcie", asic->instance);
	fp = fopen(name, "r");
	if (fp) {
		printf("pcie:\n");
		while (fgets(name, sizeof(name)-1, fp)) {
			if (strstr(name, "*"))
				printf("\t%s %s %s", YELLOW, name, RST);
			else
				printf("\t %s", name);
		}
		fclose(fp);
	}
}

void umr_clock_scan(struct umr_asic *asic, const char* clock_name)
{
	int i = 0;
	int input_flag = 0;

	asic_clocks.asic = asic;
	if (clock_name == NULL){
		for (i = 0; i < UMR_CLOCK_MAX && strlen(asic_clocks.clocks[i].clock_name); i++) {
			if (umr_read_clock(asic, asic_clocks.clocks[i].clock_name, &asic_clocks.clocks[i]) == 0) {
				if (strcmp(asic_clocks.clocks[i].clock_name, "pcie"))
					print_clock(asic_clocks.clocks[i], asic);
				else {
					if (asic_clocks.clocks[i].clock_level != 0)
						print_pcie_clock(asic);
				}
			}
		}
		input_flag = 1;
	} else {
		for (i = 0; i < UMR_CLOCK_MAX && strlen(asic_clocks.clocks[i].clock_name); i++) {
			if (!strcmp(asic_clocks.clocks[i].clock_name, clock_name)) {
				if (umr_read_clock(asic, asic_clocks.clocks[i].clock_name, &asic_clocks.clocks[i]) == 0){
					if (strcmp(asic_clocks.clocks[i].clock_name, "pcie"))
						print_clock(asic_clocks.clocks[i], asic);
					else {
						if (asic_clocks.clocks[i].clock_level != 0)
							print_pcie_clock(asic);
					}
					input_flag = 1;
					break;
				}
			}
		}
	}

	if (!input_flag)
		printf("[ERROR]: Invalid input clock name!\n");
}

void umr_clock_manual(struct umr_asic *asic, const char* clock_name, void* value)
{
	int i = 0;

	if (clock_name != NULL && value != NULL){
		for (i = 0; i < UMR_CLOCK_MAX && strlen(asic_clocks.clocks[i].clock_name); i++) {
			if (!strcmp(asic_clocks.clocks[i].clock_name, clock_name)) {
				if (umr_set_clock(asic, asic_clocks.clocks[i].clock_name, value) == 0)
					print_clock(asic_clocks.clocks[i], asic);
				break;
			}
		}

		if(i == UMR_CLOCK_MAX)
			printf("[ERROR]: Maybe wrong clock name or not support so far!\n");
	} else {
		printf("[ERROR]: Invalid input!\n");
	}
}

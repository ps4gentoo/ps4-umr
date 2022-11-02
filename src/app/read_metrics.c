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
#include "umrapp.h"

int umr_print_gpu_metrics(struct umr_asic *asic)
{
	FILE *f;
	uint8_t *pp_data;
	uint32_t size;
	char pp_name[128];
	int r;

	snprintf(pp_name, sizeof(pp_name), "/sys/class/drm/card%d/device/gpu_metrics", asic->instance);
	f = fopen(pp_name, "rb");
	if (!f) {
		fprintf(stderr, "[ERROR]:  Cannot open gpu_metrics file %s\n", pp_name);
		return -1;
	}
	fseek(f, 0, SEEK_END);
	size = ftell(f);
	fseek(f, 0, SEEK_SET);
	pp_data = calloc(1, size);
	if (!pp_data) {
		fprintf(stderr, "[ERROR]: Out of memory\n");
		fclose(f);
		return -1;
	}

	fread(pp_data, 1, size, f);
	fclose(f);

	r = umr_dump_metrics(asic, pp_data, size);
	if (r)
		goto error;

error:
	free(pp_data);
	return r;
}

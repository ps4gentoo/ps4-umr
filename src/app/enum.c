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

void umr_enumerate_devices(umr_err_output errout, const char *database_path)
{
	struct umr_asic **asics;
	int no_asics, x, y;

	if (umr_enumerate_device_list(errout, database_path, NULL, &asics, &no_asics, 1) == 0) {
		errout("[VERBOSE]: Found %d AMDGPU devices\n", no_asics);
		for (x = 0; x < no_asics; x++) {
			errout("\n\nGPU #%d => %s\n", asics[x]->instance, asics[x]->asicname);
			umr_print_config(asics[x]);
			errout("\n\tIP Blocks:\n");
			for (y = 0; y < asics[x]->no_blocks; y++) {
				errout("\t\t%s.%s\n", asics[x]->asicname, asics[x]->blocks[y]->ipname);
			}
		}
		errout("\n\n");
		umr_enumerate_device_list_free(asics);
	}
}

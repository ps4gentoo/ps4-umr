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
 */

#include <errno.h>
#include "umrapp.h"
#include "smu_pptable_navi10.h"

int umr_print_pp_table(struct umr_asic *asic, const char *param)
{
	FILE *fp;
	int res;
	char name[256];

	snprintf(name, sizeof(name)-1, \
		 "/sys/class/drm/card%d/device/pp_table", asic->instance);
	fp = fopen(name, "r");
	if (!fp) {
		asic->err_msg("fopen: %s: %d\n", strerror(errno), errno);
		return -errno;
	}
	if (strcmp(asic->asicname, "navi10") == 0 ||
	    strcmp(asic->asicname, "navi14") == 0) {
		res = umr_navi10_pptable_print(param, fp);
	} else {
		asic->err_msg("The powerplay table feature is currently supported only on Navi10/Navi14.\n");
		res = -1;
	}
	fclose(fp);

	return res;
}

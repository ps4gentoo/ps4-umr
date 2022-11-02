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
 * Authors: Kevin Wang <kevinyang.wang@amd.com>
 */

#include "umrapp.h"

int umr_dump_discovery_table_info(struct umr_asic *asic, FILE *stream)
{
	int ret;
	uint32_t size;
	void *table = NULL;

	if (!umr_discovery_table_is_supported(asic)) {
		asic->err_msg("umr discovery table is not supported\n");
		return 0;
	}

	ret = umr_discovery_read_table(asic, NULL, &size);
	if (ret)
		return ret;

	if (size) {
		table = calloc(1, size);
		if (!table)
			return -1;
	}

	ret = umr_discovery_read_table(asic, table, &size);
	if (ret)
		return ret;

	ret = umr_discovery_verify_table(asic, table);
	if (ret)
		return ret;

	ret = umr_discovery_dump_table(asic, table, stream);
	if (ret)
		return ret;

	if (table)
		free(table);

	return ret;
}

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
 */

#include "umr.h"

struct umr_database_scan_item *umr_database_find_ip(
	struct umr_database_scan_item *db,
	char *ipname, int maj, int min, int rev,
	char *desired_path)
{
	struct umr_database_scan_item *si, *best;

	si = db;
	best = NULL;
	while (si) {
		if (!desired_path ||
		    (desired_path && strstr(si->path, desired_path))) {
			if (!strcmp(ipname, si->ipname)) {
				if (maj == si->maj) {
					if (!best) {
						best = si;
					} else {
						if (abs(min - si->min) < abs(min - best->min)) {
							best = si;
						} else {
							if (abs(rev - si->rev) < abs(rev - best->rev))
								best = si;
						}
					}
				}
			}
		}
		si = si->next;
	}
	if (!best && desired_path)
		return umr_database_find_ip(db, ipname, maj, min, rev, NULL);
	return best;
}

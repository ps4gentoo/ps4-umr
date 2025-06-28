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
 */

#include "umr.h"

/**
 * @brief Finds an IP in the database that matches the specified criteria.
 *
 * This function searches through a linked list of database scan items to find an item that matches
 * the given IP name, major version, minor version, and revision. It also optionally filters by a desired path.
 *
 * @param db Pointer to the head of the database scan item list.
 * @param ipname The name of the IP to search for.
 * @param maj The major version number of the IP.
 * @param min The minor version number of the IP.
 * @param rev The revision number of the IP.
 * @param desired_path Optional path to filter the search. If NULL, no path filtering is applied.
 *
 * @return A pointer to the best matching `umr_database_scan_item` if found, otherwise NULL.
 */
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
					} else if (min >= si->min) {
						if (min - si->min < min - best->min || min < best->min) {
							best = si;
						} else if (min - si->min == min - best->min) {
							if (rev >= si->rev && (rev - si->rev < rev - best->rev || rev < best->rev))
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

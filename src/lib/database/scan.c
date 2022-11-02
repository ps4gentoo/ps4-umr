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
#include <sys/types.h>
#include <dirent.h>

static int umr_do_scan(struct umr_database_scan_item *it, char *path)
{
	DIR *dir;
	struct dirent *di;

	dir = opendir(path);
	if (!dir)
		return 0;

	// point to last entry which should be empty
	while (it->next) {
		it = it->next;
	}

	while ((di = readdir(dir))) {
		if (di->d_type == DT_DIR && strcmp(di->d_name, ".") && strcmp(di->d_name, "..")) {
			int r;
			char p[512];
			sprintf(p, "%s/%s", path, di->d_name);
			r = umr_do_scan(it, p);
			if (r) {
				closedir(dir);
				return r;
			}
		}
		if (strstr(di->d_name, ".reg")) { // we only care about register files
			strcpy(it->path, path);
			strcpy(it->fname, di->d_name);
			if (sscanf(di->d_name, "%[a-z0-9]_%d_%d_%d.reg", it->ipname, &it->maj, &it->min, &it->rev) != 4) {
				fprintf(stderr, "[WARNING]: Invalid reg file name %s\n", di->d_name);
			}
			it->next = calloc(1, sizeof *it);
			if (!it->next) {
				fprintf(stderr, "[ERROR]: Out of memory\n");
				closedir(dir);
				return -1;
			}
			it = it->next;
		}
	}
	closedir(dir);
	return 0;
}

struct umr_database_scan_item *umr_database_scan(char *path)
{
	int r;
	struct umr_database_scan_item *it, *pit;
	char p[512];

	pit = it = calloc(1, sizeof *it);
	if (!it) {
		return NULL;
	}

	if (path && *path) {
		r = umr_do_scan(it, path);
		if (r)
			goto error;
	}

	path = getenv("UMR_DATABASE_PATH");
	if (path) {
		r = umr_do_scan(it, path);
		if (r)
			goto error;
	}

#ifdef UMR_DB_DIR
	r = umr_do_scan(it, UMR_DB_DIR);
	if (r)
		goto error;
#endif

	sprintf(p, "%s/database/", UMR_SOURCE_DIR);
	r = umr_do_scan(it, p);
	if (r)
		goto error;

	return it;
error:
	while (pit) {
		it = pit->next;
		free(pit);
		pit = it;
	}
	return NULL;
}

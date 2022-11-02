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
#include "umr.h"

/**
 * umr_discover_asic_by_name - Discover an ASIC by common name
 *
 * @options:  The options to bind to the ASIC
 * @name: Name of the ASIC to look for
 *
 * The first instance of a device that matches the name
 * specified is found and returned.
 */
struct umr_asic *umr_discover_asic_by_name(struct umr_options *options, char *name, umr_err_output errout)
{
	unsigned x;
	struct umr_asic *asic, *tmp;
	char tmpname[256];

	asic = NULL;
	sprintf(tmpname, "%s.asic", name);

	if (options->force_asic_file) {
		asic = umr_database_read_asic(options, tmpname, errout);
		if (!asic) {
			asic = umr_discover_asic_by_discovery_table(name, options, errout);
		}
	} else {
		asic = umr_discover_asic_by_discovery_table(name, options, errout);
		if (!asic) {
			asic = umr_database_read_asic(options, tmpname, errout);
		}
	}

	if (!asic) {
		errout("[ERROR]: Cannot find asic file [%s] in database see README for more information\n", tmpname);
		return NULL;
	}


	if (asic) {
		asic->did = 0;
		if (options->instance == -1) {
			// try and discover an instance that works
			struct umr_options tmp_opt;
			for (x = 0; x < 16; x++) {
				memset(&tmp_opt, 0, sizeof(tmp_opt));
				tmp_opt.quiet = 1;
				tmp_opt.forcedid = -1;
				tmp_opt.instance = x;
				tmp = umr_discover_asic(&tmp_opt, errout);
				if (tmp) {
					if (!strcmp(tmp->asicname, name)) {
						asic->instance = x;
						umr_close_asic(tmp);
						break;
					}
					umr_close_asic(tmp);
				}
			}
		} else {
			asic->instance = options->instance;
		}
		umr_scan_config(asic, 0);
	} else {
		errout("ERROR: Device %s not found in UMR device table\n", name);
	}

	return asic;
}


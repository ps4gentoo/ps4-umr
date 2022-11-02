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

/* format of asic script

cmnname, soc15fname, FAMILY_%d, numblocks, vgpr_granulariy
ipcmnname, ipsocname, instance, regfile
ipcmnname, ipsocname, instance, regfile
ipcmnname, ipsocname, instance, regfile
...

*/
struct umr_asic *umr_database_read_asic(struct umr_options *options, char *filename, umr_err_output errout)
{
	char linebuf[256], cmnname[256], soc15fname[256], ipcmnname[256], ipsocname[256], regfile[256];
	struct umr_asic *asic;
	struct umr_soc15_database *soc15;
	FILE *f;
	int x;
	struct {
		int family, numblocks, vgpr_granularity, is_apu;
	} asic_fields;

	f = umr_database_open(options->database_path, filename);
	if (!f) {
		return NULL;
	}

	asic = calloc(1, sizeof *asic);
	if (!asic)
		return NULL;

	asic->err_msg = errout;
	fgets(linebuf, sizeof linebuf, f);
	if (sscanf(linebuf, "%s %s %d %d %d %d", cmnname, soc15fname, &asic_fields.family, &asic_fields.numblocks, &asic_fields.vgpr_granularity, &asic_fields.is_apu) != 6) {
		asic->err_msg("[ERROR]: Invalid ASIC header line [%s]\n", linebuf);
		free(asic);
		return NULL;
	}

	if (strcmp(soc15fname, "null")) {
		soc15 = umr_database_read_soc15(options->database_path, soc15fname, errout);
		if (!soc15) {
			fclose(f);
			free(asic);
			return NULL;
		}
	} else {
		soc15 = NULL;
	}

	// init basic info
	asic->asicname  = strdup(cmnname);
	asic->options   = *options;
	asic->no_blocks = asic_fields.numblocks;
	asic->family    = asic_fields.family;
	asic->is_apu    = asic_fields.is_apu;
	asic->parameters.vgpr_granularity = asic_fields.vgpr_granularity;
	asic->blocks    = calloc(asic->no_blocks, sizeof(*(asic->blocks)));

	for (x = 0; x < asic->no_blocks; x++) {
		int instance;
		fgets(linebuf, sizeof linebuf, f);
		if (sscanf(linebuf, "%s %s %d %s", ipcmnname, ipsocname, &instance, regfile) != 4) {
			asic->err_msg("[ERROR]: Invalid IP header line [%s]\n", linebuf);
			goto error;
		}
		asic->blocks[x] = umr_database_read_ipblock(soc15, options->database_path, regfile, ipcmnname, ipsocname, instance, errout);
		if (!asic->blocks[x])
			goto error;
	}

	umr_database_free_soc15(soc15);
	fclose(f);
	return asic;
error:
	umr_database_free_soc15(soc15);
	umr_free_asic_blocks(asic);
	fclose(f);
	return NULL;

}

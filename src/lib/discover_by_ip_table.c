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
#include <sys/types.h>
#include <dirent.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>

static struct umr_ip_block *read_ip_block(struct umr_asic *asic, struct umr_discovery_table_entry *det, struct umr_database_scan_item *nit)
{
	FILE *f;
	char ipcmn[256], linebuf[512];
	uint32_t no_regs, x;
	struct umr_ip_block *ip;

	snprintf(linebuf, (sizeof linebuf) - 1, "%s/%s", nit->path, nit->fname);
	f = fopen(linebuf, "r");
	if (!f) {
		asic->err_msg("Could not open file %s\n", linebuf);
		return NULL;
	}
	ip = calloc(1, sizeof *ip);
	if (!ip) {
		fclose(f);
		return NULL;
	}

	fgets(linebuf, sizeof(linebuf) - 1, f);
	sscanf(linebuf, "%"SCNu32, &no_regs);
	ip->no_regs = no_regs;
	ip->regs = calloc(no_regs, sizeof(*(ip->regs)));
	ip->discoverable.die = det->die;
	ip->discoverable.maj = det->maj;
	ip->discoverable.min = det->min;
	ip->discoverable.rev = det->rev;
	ip->discoverable.instance = det->instance;

	// swap for common names
	if (!strcmp(det->ipname, "gc")) {
		strcpy(ipcmn, "gfx");
	} else if (!strcmp(det->ipname, "uvd")) { // for any IP that has discovery UVD == VCN
		strcpy(ipcmn, "vcn");
	} else if (!strcmp(det->ipname, "dmu")) {
		strcpy(ipcmn, "dcn");
	} else {
		strcpy(ipcmn, det->ipname);
	}

	if (det->instance) {
		char ipname[512];
		snprintf(ipname, sizeof ipname - 1, "%s%d%d%d{%d}", ipcmn, det->maj, det->min, det->rev, det->instance);
		ip->ipname = strdup(ipname);
	} else {
		char ipname[512];
		snprintf(ipname, sizeof ipname - 1, "%s%d%d%d", ipcmn, det->maj, det->min, det->rev);
		ip->ipname = strdup(ipname);
	}

	x = 0;
	while (fgets(linebuf, sizeof linebuf, f)) {
		uint32_t y;
		struct {
			char name[128];
			int type;
			uint64_t addr;
			uint32_t nobits;
			uint32_t is64;
			uint32_t idx;
		} reg_fields;
		struct {
			char name[128];
			int start, stop;
		} bit_fields;

		if (sscanf(linebuf, "%s %d 0x%"PRIx64" %"PRIu32" %"PRIu32" %"PRIu32,
			reg_fields.name, &reg_fields.type, &reg_fields.addr,
			&reg_fields.nobits, &reg_fields.is64, &reg_fields.idx) != 6) {
				asic->err_msg("[ERROR]: Invalid regfile line [%s]\n", linebuf);
		}

		ip->regs[x].regname = strdup(reg_fields.name);
		ip->regs[x].type    = reg_fields.type;
		ip->regs[x].addr    = reg_fields.addr;
		if (ip->regs[x].type == REG_MMIO)
			ip->regs[x].addr += det->segments[reg_fields.idx];
		ip->regs[x].no_bits = reg_fields.nobits;
		ip->regs[x].bit64   = reg_fields.is64;

		if (reg_fields.nobits) {
			ip->regs[x].bits = calloc(reg_fields.nobits, sizeof(*(ip->regs[x].bits)));
			for (y = 0; y < reg_fields.nobits; y++) {
				fgets(linebuf, sizeof linebuf, f);
				sscanf(linebuf, "\t%s %d %d", bit_fields.name, &bit_fields.start, &bit_fields.stop);
				ip->regs[x].bits[y].regname = strdup(bit_fields.name);
				ip->regs[x].bits[y].start = bit_fields.start;
				ip->regs[x].bits[y].stop = bit_fields.stop;
				ip->regs[x].bits[y].bitfield_print = &umr_bitfield_default;
			}
		}
		++x;
	}
	fclose(f);
	return ip;
}

static void dump_discovery_to_log(struct umr_discovery_table_entry *det, struct umr_options *options)
{
	fprintf(options->test_log_fd, "DISCOVERY = { ");
	while (det) {
		int x;
		for (x = 0; x < 128; x++) fprintf(options->test_log_fd, "%02" PRIx8, (unsigned)(det->ipname[x] & 0xFF));
		fprintf(options->test_log_fd, "%02" PRIx8 "%02" PRIx8, (unsigned)(det->die >> 8), (unsigned)(det->die));
		fprintf(options->test_log_fd, "%02" PRIx8 "%02" PRIx8, (unsigned)(det->instance >> 8), (unsigned)(det->instance));
		fprintf(options->test_log_fd, "%02" PRIx8 "%02" PRIx8, (unsigned)(det->maj >> 8), (unsigned)(det->maj));
		fprintf(options->test_log_fd, "%02" PRIx8 "%02" PRIx8, (unsigned)(det->min >> 8), (unsigned)(det->min));
		fprintf(options->test_log_fd, "%02" PRIx8 "%02" PRIx8, (unsigned)(det->rev >> 8), (unsigned)(det->rev));
		for (x = 0; x < 16; x++) fprintf(options->test_log_fd, "%016" PRIx64, det->segments[x]);
		det = det->next;
	}
	fprintf(options->test_log_fd, "}\n");
}

static struct umr_discovery_table_entry *import_det_from_log(struct umr_options *options, int *nblocks)
{
	struct umr_discovery_table_entry *det, *pdet;
	int x, y, z;
	uint8_t *data;

	*nblocks = (options->th->discovery.size) / DET_REC_SIZE;
	data = &options->th->discovery.contents[0];

	det = pdet = calloc(1, sizeof *det);
	if (!det) {
		return NULL;
	}
	for (x = 0; x < *nblocks; x++) {
		memcpy(det->ipname, data, 128); data += 128;
		det->die = ((unsigned)data[0] << 8) | ((unsigned)data[1]);	data += 2;
		det->instance = ((unsigned)data[0] << 8) | ((unsigned)data[1]);	data += 2;
		det->maj = ((unsigned)data[0] << 8) | ((unsigned)data[1]);	data += 2;
		det->min = ((unsigned)data[0] << 8) | ((unsigned)data[1]);	data += 2;
		det->rev = ((unsigned)data[0] << 8) | ((unsigned)data[1]);	data += 2;
		for (y = 0; y < 16; y++) {
			for (z = 0; z < 8; z++)
				det->segments[y] = (det->segments[y] << 8) | ((uint64_t)*data++);
		}
		if (x < (*nblocks - 1)) {
			det->next = calloc(1, sizeof *det);
			if (!det->next) {
				while (pdet) {
					det = pdet->next;
					free(pdet);
					pdet = det;
				}
				return NULL;
			}
			det = det->next;
		}
	}

	// no discovery data in log so return NULL to error out
	if (det == pdet) {
		free(det);
		return NULL;
	} else {
		return pdet;
	}
}

struct umr_asic *umr_discover_asic_by_discovery_table(char *aname, struct umr_options *options, umr_err_output errout)
{
	struct umr_discovery_table_entry *det, *pdet;
	struct umr_database_scan_item *it, *nit;
	int numblocks, used_blocks, x, y;
	struct umr_asic *asic;
	char asicname[128], *dasic;
	struct export_data {
		struct umr_discovery_table_entry *det;
		struct umr_database_scan_item *nit;
		int soc15;
		struct export_data *next;
	} exp_data = { 0 }, *pexp_data = NULL;

	// copy name and remove ".asic" if any
	strcpy(asicname, aname);
	dasic = strstr(asicname, ".asic");
	if (dasic)
		*dasic = 0;

	// create discovery table
	if (options->test_log && !options->test_log_fd) {
		det = import_det_from_log(options, &numblocks);
	} else {
		det = umr_parse_ip_discovery(options->instance, &numblocks, errout);
	}

	if (!det)
		return NULL;

	// dump discovered data to test log if open
	if (options->test_log && options->test_log_fd) {
		dump_discovery_to_log(det, options);
	}

	// create database of IP
	it = umr_database_scan(options->database_path);

	asic = calloc(1, sizeof *asic);
	if (!asic) {
		errout("[ERROR]: Out of memory allocating ASIC model\n");
		goto done;
	}
	asic->asicname = strdup(asicname);
	asic->options = *options;
	asic->no_blocks = numblocks;
	asic->blocks    = calloc(asic->no_blocks, sizeof(*(asic->blocks)));
	asic->family    = FAMILY_CONFIGURE;

	asic->parameters.vgpr_granularity = 2; // TODO: discover this at runtime, note this will break aldebaran support...
	if (!strcmp(asicname, "aldebaran")) // TODO: remove this workaround
		asic->parameters.vgpr_granularity = 3;

	asic->err_msg = errout;

	pdet = det;
	used_blocks = 0;
	while (det) {
		char cmnname[256];

		// some common names have to happen here to match up with ip database
		if (!strcmp(det->ipname, "clka")) {
			strcpy(cmnname, "clk");
		} else if (!strcmp(det->ipname, "clkb")) {
			strcpy(cmnname, "clk");
		} else if (!strcmp(det->ipname, "mp0")) {
			strcpy(cmnname, "mp");
		} else if (!strcmp(det->ipname, "mp1")) {
			strcpy(cmnname, "mp");
		} else if (!strcmp(det->ipname, "sdma1")) {
			strcpy(cmnname, "sdma");
		} else if (!strcmp(det->ipname, "uvd")) {
			strcpy(cmnname, "vcn");
		} else if (!strcmp(det->ipname, "dmu")) {
			strcpy(cmnname, "dcn");
		} else {
			strcpy(cmnname, det->ipname);
		}

		nit = umr_database_find_ip(it, cmnname,
			det->maj, det->min, det->rev, options->desired_path[0] ? options->desired_path : NULL);
		if (nit) {
			if (options->export_model) {
				if (pexp_data == NULL) {
					pexp_data = &exp_data;
				} else {
					pexp_data->next = calloc(1, sizeof(exp_data));
					pexp_data = pexp_data->next;
				}
				pexp_data->nit = nit;
				pexp_data->det = det;
			}
			if (options->verbose)
				errout("[VERBOSE]: Using %s/%s (%d.%d.%d) for %s (%d.%d.%d)\n",
					nit->path, nit->fname, nit->maj, nit->min, nit->rev,
					det->ipname, det->maj, det->min, det->rev);
			asic->blocks[used_blocks++] = read_ip_block(asic, det, nit);
		}
		det = det->next;
	}
	asic->no_blocks = used_blocks - 1;

	// scan blocks for missing {0}
	// to be consistent if there is more than one instance of
	// an IP block we number them ALL including the 0'th
	for (x = 0; x < asic->no_blocks; x++) {
		if (!strstr(asic->blocks[x]->ipname, "{")) {
			for (y = 0; y < asic->no_blocks; y++) {
				if (x != y && strstr(asic->blocks[y]->ipname, asic->blocks[x]->ipname))
					break;
			}
			if (y != asic->no_blocks) {
				char tmpbuf[256];
				memset(tmpbuf, 0, sizeof tmpbuf);
				snprintf(tmpbuf, sizeof tmpbuf - 1, "%s{0}", asic->blocks[x]->ipname);
				free(asic->blocks[x]->ipname);
				asic->blocks[x]->ipname = strdup(tmpbuf);
			}
		}
	}

	if (options->export_model) {
		FILE *fexp;
		struct export_data *ppexp;
		char buf[128];
		int x, z;

		// dump SOC15 contents first
		snprintf(buf, sizeof(buf), "%s.soc15", asic->asicname);
		fexp = fopen(buf, "w");
		pexp_data = &exp_data;
		while (pexp_data && strlen(pexp_data->det->ipname)) {
			if (!pexp_data->soc15) {
				fprintf(fexp, "%s\n", pexp_data->det->ipname);
				ppexp = pexp_data;
				z = 0;
				do {
					if (!ppexp->soc15 && !strcmp(pexp_data->det->ipname, ppexp->det->ipname)) {
						ppexp->soc15 = 1;
						fprintf(fexp, "\t");
						for (x = 0; x < 8; x++) {
							fprintf(fexp, "0x%08" PRIx64 " ", ppexp->det->segments[x]);
						}
						fprintf(fexp, "\n");
						++z;
					}
					ppexp = ppexp->next;
				} while (ppexp);

				// at this point we've output Z of 32 rows, so zero out the rest
				for (; z < 32; z++) {
					fprintf(fexp, "\t0x00000000 0x00000000 0x00000000 0x00000000 0x00000000 0x00000000 0x00000000 0x00000000\n");
				}
			}
			pexp_data = pexp_data->next;
		}
		fclose(fexp);

		// output ASIC data
		snprintf(buf, sizeof(buf), "%s.asic", asic->asicname);
		fexp = fopen(buf, "w");
		pexp_data = &exp_data;

		// NOTE: we default to FAMILY_NV, VGPR=2, APU=0
		fprintf(fexp, "%s %s.soc15 %d %d 2 0\n", asic->asicname, asic->asicname, FAMILY_NV, asic->no_blocks);
		for (x = 0; x < asic->no_blocks; x++) {
			fprintf(fexp, "%s %s %d %s/%s\n",
				asic->blocks[x]->ipname,
				pexp_data->det->ipname,
				asic->blocks[x]->discoverable.instance,
				pexp_data->nit->path, pexp_data->nit->fname);
			pexp_data = pexp_data->next;
		}
		fclose(fexp);

		// free memory
		pexp_data = exp_data.next;
		while (pexp_data) {
			ppexp = pexp_data->next;
			free(pexp_data);
			pexp_data = ppexp;
		}
	}



done:
	det = pdet;
	while (det) {
		pdet = det->next;
		free(det);
		det = pdet;
	}
	umr_database_free_scan_items(it);
	return asic;
}

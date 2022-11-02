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

// We try to parse paths like "my_ip/sdma0_4_0_0.reg" to major, minor, and
// revision version number. We also accept paths like "gc_5_0.reg" to parse into
// just major and minor version numbers.
static void fill_ipver_from_path(char* ip_path, struct umr_ip_block* ip_block) {
	char* (pos[3]) = {NULL};
	int len = strlen(ip_path);
	int i;
	int found = 0;
	int maj, min, rev;

	// find last 3 underscores and store their position in pos
	for (i = len - 1; i >= 0 && found < 3; i--) {
		if (ip_path[i] == '_') {
			pos[found++] = &ip_path[i];
		}
	}

	if (found == 3 && sscanf(pos[2], "_%d_%d_%d.reg", &maj, &min, &rev) == 3) {
		// case *_maj_min_rev.reg
		ip_block->discoverable.die = 0;
		ip_block->discoverable.maj = maj;
		ip_block->discoverable.min = min;
		ip_block->discoverable.rev = rev;
	} else if (found >= 2 && sscanf(pos[1], "_%d_%d.reg", &maj, &min) == 2) {
		// case *_maj_min.reg
		ip_block->discoverable.die = 0;
		ip_block->discoverable.maj = maj;
		ip_block->discoverable.min = min;
		ip_block->discoverable.rev = 0;
	}
}

struct umr_ip_block *umr_database_read_ipblock(struct umr_soc15_database *soc15, char *path, char *filename, char *cmnname, char *soc15name, int inst, umr_err_output errout)
{
	struct umr_ip_block *ip;
	FILE *f;
	uint32_t no_regs;
	int x;
	char linebuf[256];

	if (soc15) {
		// find soc15 entry
		while (soc15) {
			if (!strcmp(soc15->ipname, soc15name))
				break;
			soc15 = soc15->next;
		}
		if (!soc15) {
			errout("[ERROR]: Cannot find IP name [%s] in the SOC15 table\n", soc15name);
			return NULL;
		}
	}

	f = umr_database_open(path, filename);
	if (!f) {
		errout("[ERROR]: IP register file [%s] not found\n", filename);
		errout("[ERROR]: These files are typically found in the source tree under [database/ip/]\n");
		errout("[ERROR]: If you have manually relocated the database tree use the '-dbp' option to tell UMR where they are\n");
		return NULL;
	}

	ip = calloc(1, sizeof *ip);
	if (!ip) {
		fclose(f);
		return NULL;
	}

	fgets(linebuf, sizeof(linebuf), f);
	sscanf(linebuf, "%"SCNu32, &no_regs);
	ip->no_regs = no_regs;
	ip->regs = calloc(no_regs, sizeof(*(ip->regs)));
	ip->ipname = strdup(cmnname);

	// try to parse version out of filename (assume path has no spaces)
	if (sscanf(filename, "%s", linebuf)) {
		fill_ipver_from_path(linebuf, ip);
	}

	x = 0;
	while (x != ip->no_regs && fgets(linebuf, sizeof linebuf, f)) {
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
				errout("[ERROR]: Invalid regfile line [%s]\n", linebuf);
		}

		ip->regs[x].regname = strdup(reg_fields.name);
		ip->regs[x].type    = reg_fields.type;
		ip->regs[x].addr    = reg_fields.addr;
		if (soc15 && ip->regs[x].type == REG_MMIO)
			ip->regs[x].addr += soc15->off[inst][reg_fields.idx];
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

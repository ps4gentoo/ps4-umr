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
#include "umr.h"
#include <ctype.h>

static int istr_cmp(const char* a, const char* b)
{
	unsigned char a_up;
	unsigned char b_up;

	for(;;) {
		a_up = (unsigned char)toupper(*a);
		b_up = (unsigned char)toupper(*b);

		if (a_up != b_up || !a_up) {
			break;
		}
		a++;
		b++;
	}
    return a_up - b_up;
}

static int expression_matches(const char* str, const char* pattern)
{
	const char *cp = NULL, *mp = NULL;

	while ((*str) && (*pattern != '*')) {
		if ((toupper(*pattern) != toupper(*str)) && (*pattern != '?')) {
			return 0;
		}
		str++;
		pattern++;
	}

	while (*str) {
		if (*pattern == '*') {
			if (!*++pattern) {
				return 1;
			}
			mp = pattern;
			cp = str + 1;
		} else if ((toupper(*pattern) == toupper(*str)) || (*pattern == '?')) {
			pattern++;
			str++;
		} else {
			pattern = mp;
			str = cp++;
		}
	}

	while (*pattern == '*') {
		pattern++;
	}
	return !*pattern;
}

/**
 * umr_find_reg_wild_first - Initiate a wildcard iterative search
 *
 * @asic: The ASIC to search through
 * @ip: The optional (NULL=don't care) IP block to limit the search to
 * @reg: The partial register name to match
 *
 * Returns a pointer to a umr_find_reg_iter structure on success.
 */
struct umr_find_reg_iter* umr_find_reg_wild_first(struct umr_asic* asic, const char* ip, const char* reg)
{
	struct umr_find_reg_iter* iter;

	iter = calloc(1, sizeof(*iter));
	if (!iter) {
		asic->err_msg("[ERROR]: Out of memory\n");
		return NULL;
	}
	iter->asic = asic;
	iter->ip = ip ? strdup(ip) : NULL;
	iter->reg = strdup(reg);

	iter->ip_i = -1;
	iter->reg_i = -1;
	return iter;
}

/**
 * umr_find_reg_wild_next - Iterate an existing register search
 *
 * @iterp: A pointer to a pointer to a umr_find_reg_iter structure
 *
 * Returns a copy of a umr_find_get_iter_result structure.
 */
struct umr_find_reg_iter_result umr_find_reg_wild_next(struct umr_find_reg_iter **iterp)
{
	struct umr_find_reg_iter_result res;
	struct umr_find_reg_iter *iter = *iterp;
	for (;;) {
		// if reg_i == -1 find the next IP block
		if (iter->reg_i == -1) {
			++(iter->ip_i);
			while ((iter->ip_i < iter->asic->no_blocks) &&
				   (iter->ip && !expression_matches(iter->asic->blocks[iter->ip_i]->ipname, iter->ip))) {
				++(iter->ip_i);
			}

			// no more blocks
			if (iter->ip_i >= iter->asic->no_blocks) {
				free(iter->ip);
				free(iter->reg);
				free(iter);
				res.ip = NULL;
				res.reg = NULL;
				*iterp = NULL;
				return res;
			}

			// start search inside block from the first register
			iter->reg_i = 0;
		}

		while (iter->reg_i < iter->asic->blocks[iter->ip_i]->no_regs) {
			if (expression_matches(iter->asic->blocks[iter->ip_i]->regs[iter->reg_i].regname, iter->reg)) {
				res.reg = &iter->asic->blocks[iter->ip_i]->regs[iter->reg_i++];
				res.ip = iter->asic->blocks[iter->ip_i];
				return res;
			}
			++(iter->reg_i);
		}

		// no match go to next block
		iter->reg_i = -1;
	}
}

/**
 * umr_find_reg_data_by_ip - Find a register by name for a given IP
 *
 * Returns the umr_reg structure for a register for a given IP block
 * with a specific name @regname.  The IP block @ip is optional (can be NULL) and
 * is only compared as a prefix (e.g., "gfx" will match "gfx90").  The
 * search is performed on the @asic specified.
 */
struct umr_reg* umr_find_reg_data_by_ip(struct umr_asic* asic, const char* ip, const char* regname)
{
	int instance = -1;
	char *p;

	if (ip) {
		p = strstr(ip, "{");
		if (p)
			sscanf(p, "{%d}", &instance);
	}
	return umr_find_reg_data_by_ip_by_instance(asic, ip, instance, regname);
}

/**
 * umr_find_reg_by_name - Find a register by name
 *
 * Returns the umr_reg structure for a register with a specific name
 * in the first IP block that contains it. If @ip is not NULL it will also
 * store the IP block pointer for the register as well.
 */
struct umr_reg* umr_find_reg_by_name(struct umr_asic* asic, const char* regname, struct umr_ip_block** ip)
{
	return umr_find_reg_data_by_ip_by_instance_with_ip(asic, NULL, -2, regname, ip);
}

struct umr_reg* umr_find_reg_data_by_ip_by_instance(struct umr_asic* asic, const char* ip, int inst, const char* regname)
{
	return umr_find_reg_data_by_ip_by_instance_with_ip(asic, ip, inst, regname, NULL);
}

/**
 * @brief Finds register data by IP, instance, and register name.
 *
 * This function searches for a specific register within an IP block of a given ASIC instance.
 * It uses the IP address, instance number, and register name to locate the corresponding register data.
 *
 * @param asic Pointer to the ASIC structure containing the IP blocks.
 * @param ip The IP address (as a string) of the IP block to search within.
 * @param inst The instance number of the IP block.
 * @param regname The name of the register to find.
 * @param ipp A pointer to a struct umr_ip_block pointer, which will be set to point to the found IP block if successful.
 *
 * @return A pointer to the found register data (struct umr_reg*), or NULL if no matching register is found.
 */
struct umr_reg* umr_find_reg_data_by_ip_by_instance_with_ip(struct umr_asic* asic, const char* ip, int inst, const char* regname, struct umr_ip_block **ipp)
{
	int i, k;
	char origname[96], tmpregname[96], instname[16];
	const char *oregname = regname;

	strcpy(origname, regname);

	if (ipp)
		*ipp = NULL;

	// compute INST name for IP block
	if (inst >= 0) {
		sprintf(instname, "{%d}", inst);
	} else {
		instname[0] = 0;
	}

	k = regname[0] == '@';
	if (k)
		++regname;

	oregname = regname;
retry:
	for (i = 0; i < asic->no_blocks; i++) {
		// optionally require the ip block name to partially match (allows for ignoring version numbers)
		if (ip && (strlen(asic->blocks[i]->ipname) >= strlen(ip) && memcmp(asic->blocks[i]->ipname, ip, strlen(ip))))
			continue;

		// if we are looking for an instance require the {inst} as well
		if (inst >= 0 && !strstr(asic->blocks[i]->ipname, instname))
			continue;

		// if we are not looking for an instance skip over IP blocks with an instance
		// this is mostly to catch UMR bugs that don't forward say
		// --vm-partition to a register function on partitioned hosts
		if (inst < 0 && inst != -2 && strstr(asic->blocks[i]->ipname, "{"))
			continue;
		{
			int bot, top, mid, diff;
			bot = 0;
			top = asic->blocks[i]->no_regs;
			while (bot < top) {
				mid = (bot + top) >> 1;
				diff = istr_cmp(asic->blocks[i]->regs[mid].regname, regname);
				if (diff < 0) {
					// needle is above mid
					bot = mid + 1;
				} else {
					// needle is below or equal to mid
					top = mid;
				}
			}
			if (bot < asic->blocks[i]->no_regs && !istr_cmp(asic->blocks[i]->regs[bot].regname, regname)) {
				if (ipp)
					*ipp = asic->blocks[i];
				return &asic->blocks[i]->regs[bot];
			}
		}
	}

	// if regname starts with 'mm' search for variant with 'reg' prefix
	// this avoids having to recode a lot of logic.
	if (!memcmp(regname, "mm", 2)) {
		strncpy(tmpregname, "reg", sizeof(tmpregname));
		strncpy(tmpregname + 3, regname + 2, sizeof(tmpregname) - 3);
		regname = (const char *)tmpregname;
		goto retry;
	}

	if (!k) {
		if (!asic->options.trap_unsorted_db) {
			asic->options.trap_unsorted_db = 1;
			for (i = 0; i < asic->no_blocks; i++) {
				for (k = 0; k < asic->blocks[i]->no_regs; k++) {
					if (!strcmp(asic->blocks[i]->regs[k].regname, regname) ||
					    !strcmp(asic->blocks[i]->regs[k].regname, origname)) {
						    asic->err_msg("[ERROR]: Register <%s> found in an **UNSORTED** database\n", origname);
						    asic->err_msg("[ERROR]: Your UMR database is not sorted, please check /usr/share/umr or /usr/local/share/umr for outdated contents.\n");
						    asic->err_msg("[ERROR]: UMR will not function correctly with outdated databases\n");
						    return NULL;
					}
				}
			}
		}
		asic->err_msg("[BUG]: reg [%s](%d) not found on asic [%s]\n", oregname, inst, asic->asicname);
	}
	return NULL;
}

/**
 * umr_find_reg - Find a register by name
 *
 * Returns the offset of the register if found or 0xFFFFFFFF if not.
 */
uint32_t umr_find_reg(struct umr_asic* asic, const char* regname)
{
	struct umr_reg *reg;
	reg = umr_find_reg_by_name(asic, regname, NULL);
	if (reg)
		return reg->addr;
	else
		return 0xFFFFFFFF;
}

/**
 * umr_find_reg_by_addr - Find a register by addressable offset
 *
 * Returns the umr_reg structure (if found) for a register at a
 * given address.  If @ip is not NULL it will also store the IP block
 * pointer for the register as well.
 */
struct umr_reg* umr_find_reg_by_addr(struct umr_asic* asic, uint64_t addr, struct umr_ip_block** ip)
{
	int i, j;

	if (ip)
		*ip = NULL;

	if (asic->mmio_accel) {
		uint32_t bot, mid, top;
		bot = 0;
		top = asic->mmio_accel_size;

		while (bot < top) {
			mid = (bot + top) >> 1;
			if (asic->mmio_accel[mid].mmio_addr < addr) {
				bot = mid + 1;
			} else {
				top = mid;
			}
		}
		if (bot < asic->mmio_accel_size && asic->mmio_accel[bot].mmio_addr == addr) {
			if (ip)
				*ip = asic->mmio_accel[bot].ip;
			return asic->mmio_accel[bot].reg;
		}
		return NULL;
	}

	for (i = 0; i < asic->no_blocks; i++)
		for (j = 0; j < asic->blocks[i]->no_regs; j++)
			if (asic->blocks[i]->regs[j].type == REG_MMIO && asic->blocks[i]->regs[j].addr == addr) {
				if (ip)
					*ip = asic->blocks[i];
				return &asic->blocks[i]->regs[j];
			}
	return NULL;
}

/**
 * umr_reg_name - Construct a human readable name for a register
 *
 * Returns a human readable name including IP and register name
 * to the caller based on the address specified.
 */
char* umr_reg_name(struct umr_asic* asic, uint64_t addr)
{
	struct umr_reg* reg;
	struct umr_ip_block* ip;
	static char name[512];

	reg = umr_find_reg_by_addr(asic, addr, &ip);
	if (ip && reg) {
		sprintf(name, "%s%s.%s%s", RED, ip->ipname, reg->regname, RST);
		return name;
	} else {
		return "<unknown>";
	}
}

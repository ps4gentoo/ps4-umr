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
#include <sys/types.h>
#include <dirent.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>

static void set_ip_logical_inst(struct umr_discovery_table_entry *first,
                                struct umr_discovery_table_entry *end,
                                uint32_t inst_mask)
{
	struct umr_discovery_table_entry *ent = first;
	uint32_t mask;
	int logical_inst;

	while (ent && ent != end) {
		logical_inst = -1;
		if (!ent->harvest) {
			/* Count the set bits in inst_mask before ent->instance */
			mask = ((1U << ent->instance) - 1) & inst_mask;
			logical_inst = 0;
			while (mask) {
				if (mask & 1)
					++logical_inst;
				mask >>= 1;
			}
		}
		ent->logical_inst = logical_inst;
		ent = ent->next;
	}
}

/* dir struct for add_ip_instances
        ├── 1 <= hw_id use name aliases instead
        │   └── 0 <= instance of this IP block
        │       ├── base_addr <= list of segments in hex
        │       ├── hw_id
        │       ├── major <= dec
        │       ├── minor <= dec
        │       ├── num_base_addresses
        │       ├── num_instance
        │       └── revision <= dec
*/
/**
 * @brief Adds IP instances to the discovery table.
 *
 * This function populates the discovery table with instances of a specific IP (Intellectual Property)
 * identified by `ipname` for a given die number (`die_num`). The function updates the number of blocks
 * (`nblocks`) and constructs the path to the die (`diepath`).
 *
 * @param det       Pointer to an array of discovery table entries where the IP instances will be added.
 * @param die_num   The die number for which the IP instances are being added.
 * @param nblocks   Pointer to an integer that will be updated with the number of blocks.
 * @param diepath   Buffer to store the path to the die. This should be pre-allocated by the caller.
 * @param ipname    Name of the IP (Intellectual Property) to be added to the discovery table.
 */
static void add_ip_instances(struct umr_discovery_table_entry **det, int die_num, int *nblocks, char *diepath, char *ipname)
{
	DIR *ipdir;
	char linebuf[512], fname[1024], databuf[256];
	struct dirent *de;
	int x;
	uint32_t inst_mask = 0;
	struct umr_discovery_table_entry *ip_start;
	FILE *f;

	snprintf(linebuf, (sizeof linebuf) - 1, "%s/%s", diepath, ipname);
	ipdir = opendir(linebuf);
	if (!ipdir)
		return;
	ip_start = *det;
	while ((de = readdir(ipdir))) {
		if (isdigit(de->d_name[0])) {
			snprintf(linebuf, (sizeof linebuf) - 1, "%s/%s/%s", diepath, ipname, de->d_name); // path to instance of ip block on given die
			(*det)->die = die_num;
			// base_addr list
			snprintf(fname, (sizeof fname) - 1, "%s/base_addr", linebuf);
			f = fopen(fname, "r");
			if (f) {
				x = 0;
				while (fgets(databuf, sizeof databuf, f)) {
					if (sscanf(databuf, "%"SCNx64, &(*det)->segments[x]) != 1)
						break;
					++x;
				}
			} else {
				fprintf(stderr, "[ERROR]: Can't open [%s]\n", fname);
				exit(-1);
			}
			fclose(f);
			// major
			snprintf(fname, (sizeof fname) - 1, "%s/major", linebuf);
			f = fopen(fname, "r");
				fgets(databuf, sizeof databuf, f);
				sscanf(databuf, "%d", &(*det)->maj);
			fclose(f);
			// minor
			snprintf(fname, (sizeof fname) - 1, "%s/minor", linebuf);
			f = fopen(fname, "r");
				fgets(databuf, sizeof databuf, f);
				sscanf(databuf, "%d", &(*det)->min);
			fclose(f);
			// revision
			snprintf(fname, (sizeof fname) - 1, "%s/revision", linebuf);
			f = fopen(fname, "r");
				fgets(databuf, sizeof databuf, f);
				sscanf(databuf, "%d", &(*det)->rev);
			fclose(f);
			// instance
			snprintf(fname, (sizeof fname) - 1, "%s/num_instance", linebuf);
			f = fopen(fname, "r");
				fgets(databuf, sizeof databuf, f);
				sscanf(databuf, "%d", &(*det)->instance);
			fclose(f);
			// harvest
			snprintf(fname, (sizeof fname) - 1, "%s/harvest",
					linebuf);
			f = fopen(fname, "r");
			if (f) {
				fgets(databuf, sizeof databuf, f);
				sscanf(databuf, "%" SCNx8,
					&(*det)->harvest);
				if ((*det)->harvest == 0)
						inst_mask |=
							(1 << (*det)->instance);

				fclose(f);
			} else {
				inst_mask |= (1 << (*det)->instance);
			}
			// convert name to lowercase
			strcpy((*det)->ipname, ipname);
			for (x = 0; (*det)->ipname[x]; x++)
				(*det)->ipname[x] = tolower((*det)->ipname[x]);
			// add next
			(*det)->next = calloc(1, sizeof **det);
			if (!(*det)->next) {
				closedir(ipdir);
				return;
			}
			*det = (*det)->next;
			++(*nblocks);
		}
	}
	closedir(ipdir);
	set_ip_logical_inst(ip_start, *det, inst_mask);
}

/* Dir structure
 *
.
└── die
    └── 0
        ├── 1 <= hw_id use name aliases instead
        │   └── 0 <= instance of this IP block
        │       ├── base_addr <= list of segments in hex
        │       ├── hw_id
        │       ├── major <= dec
        │       ├── minor <= dec
        │       ├── num_base_addresses
        │       ├── num_instance
        │       └── revision <= dec
*/
/**
 * @brief Parses IP discovery data and returns a discovery table entry.
 *
 * This function parses the IP discovery data for a given instance and populates
 * the discovery table entries. It also updates the number of blocks found in the
 * discovery data.
 *
 * @param[in]  instance The instance identifier for which to parse the IP discovery data.
 * @param[out] nblocks  A pointer to an integer where the number of blocks will be stored.
 * @param[in]  errout   An error output function used to report any errors during parsing.
 *
 * @return A pointer to the first entry in the parsed discovery table, or NULL if an error occurred.
 */
struct umr_discovery_table_entry *umr_parse_ip_discovery(int instance, int *nblocks, umr_err_output errout)
{
	DIR *top = NULL, *die = NULL;
	char linebuf[512];
	struct umr_discovery_table_entry *pdet = NULL, *det = NULL;
	struct dirent *de;
	int die_num;

	snprintf(linebuf, (sizeof linebuf) - 1, "/sys/class/drm/card%d/device/ip_discovery/die", instance);
	top = opendir(linebuf);
	if (!top) {
		return NULL;
	}

	pdet = det = calloc(1, sizeof *det);
	if (!det) {
		closedir(top);
		return NULL;
	}
	*nblocks = 0;
	// iterate over every die
	while ((de = readdir(top))) {
		if (strcmp(de->d_name, ".") && strcmp(de->d_name, "..")) {
			die_num = atoi(de->d_name);

			// process die directory
			snprintf(linebuf, (sizeof linebuf) - 1, "/sys/class/drm/card%d/device/ip_discovery/die/%s", instance, de->d_name);
			die = opendir(linebuf);
			if (!die) {
				errout("Cannot open IP discovery die directory\n");
				goto error;
			}
			while ((de = readdir(die))) {
				if (isalpha(de->d_name[0])) {
					add_ip_instances(&det, die_num, nblocks, linebuf, de->d_name);
				}
			}
			closedir(die);
		}
	}
	closedir(top);
	return pdet;
error:
	*nblocks = 0;
	if (top)
		closedir(top);
	if (die)
		closedir(die);
	det = pdet;
	while (det) {
		pdet = det->next;
		free(det);
		det = pdet;
	}
	return NULL;
}

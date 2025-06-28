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
#include "umrapp.h"
#include <signal.h>
#include <time.h>
#include <stdarg.h>

int umr_kfd_topo_get_pci_busaddr(int node, char *busaddr)
{
	FILE *topo;
	char linebuf[256];
	uint32_t location, domain, mask=0;

	sprintf(linebuf, "/sys/devices/virtual/kfd/kfd/topology/nodes/%d/properties", node);
	topo = fopen(linebuf, "r");
	if (!topo) {
		fprintf(stderr, "[ERROR]: Could not open KFD topology file for this node\n");
		return -1;
	}
	while (fgets(linebuf, sizeof linebuf, topo)) {
		if (sscanf(linebuf, "location_id %"SCNu32, &location) == 1) {
			mask |= 1;
		}
		if (sscanf(linebuf, "domain %"SCNu32, &domain) == 1) {
			mask |= 2;
		}
		if (mask == 3) {
			sprintf(busaddr, "%04" PRIx32 ":%02" PRIx32 ":%02" PRIx32".0", domain, (location&0xFF00)>>8, (location&0xFF));
			fclose(topo);
			return 0;
		}
	}
	fclose(topo);
	return -1;
}


void umr_dump_runlists(struct umr_asic *asic, int node)
{
	FILE *rls;
	unsigned char *rlsbuf;
	char linebuf[256];
	uint32_t linenums[9], rlssize, rlsofs;
	int nv, x, words;

	rlsofs = 0;
	rlssize = 4096;
	rlsbuf = calloc(1, rlssize);

	if (!rlsbuf) {
		asic->err_msg("[ERROR]: Out of memory\n");
		return;
	}

	rls = fopen("/sys/kernel/debug/kfd/rls", "r");
	if (!rls) {
		asic->err_msg("[ERROR]: Could not open RLS file from KFD debug tree\n");
		free(rlsbuf);
		return;
	}
	while (fgets(linebuf, sizeof linebuf, rls)) {
		if (sscanf(linebuf, "Node %d, gpu_id", &nv) == 1 && nv == node) {
			// parse data in
			while (fgets(linebuf, sizeof linebuf, rls)) {
				if ((words = sscanf(linebuf, "%"SCNx32": %"SCNx32" %"SCNx32" %"SCNx32" %"SCNx32" %"SCNx32" %"SCNx32" %"SCNx32" %"SCNx32"\n",
					&linenums[0], &linenums[1], &linenums[2], &linenums[3], &linenums[4], &linenums[5], &linenums[6], &linenums[7],
					&linenums[8])) >= 2) {
						words--; // skip offset
						if ((words * 4 + rlsofs) > rlssize) {
							void *t = realloc(rlsbuf, rlssize + 4096);
							if (!t) {
								asic->err_msg("[ERROR]: Out of memory\n");
								free(rlsbuf);
								return;
							}
							rlssize += 4096;
							rlsbuf = t;
						}
						for (x = 0; x < words; x++) {
							memcpy(&rlsbuf[rlsofs], &linenums[1 + x], 4);
							rlsofs += 4;
						}
				} else {
					break;
				}
			}

			// now rlsbuf has a PM4 IB of size rlsofs bytes
			if (rlsofs)
				umr_ring_stream_present(asic, NULL, -1, -1, 0, 0, (uint32_t *)rlsbuf, rlsofs>>2, UMR_RING_PM4);

			// we're done
			break;
		}
	}
	free(rlsbuf);
	fclose(rls);
}

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

struct umr_soc15_database *umr_database_read_soc15(char *path, char *filename, umr_err_output errout)
{
	struct umr_soc15_database *s, *os;
	FILE *f;
	char linebuf[256];
	int x;

	f = umr_database_open(path, filename);
	if (!f) {
		errout("[ERROR]: SOC15 offset file [%s] not found\n", filename);
		errout("[ERROR]: These files are typically found in the source tree under [database/]\n");
		errout("[ERROR]: If you have manually relocated the database tree use the '-dbp' option to tell UMR where they are\n");
		return NULL;
	}

	os = s = calloc(1, sizeof *s);
	if (!s) {
		fclose(f);
		return NULL;
	}

	while (fgets(linebuf, sizeof(linebuf), f)) {
retry8:
		linebuf[strlen(linebuf)-1] = 0; // chomp
		strcpy(s->ipname, linebuf);
		for (x = 0; x < 32; x++) {
			fgets(linebuf, sizeof(linebuf), f);
			if (sscanf(linebuf, "\t0x%"PRIx64" 0x%"PRIx64" 0x%"PRIx64" 0x%"PRIx64" 0x%"PRIx64" 0x%"PRIx64" 0x%"PRIx64" 0x%"PRIx64,
					&s->off[x][0], &s->off[x][1], &s->off[x][2], &s->off[x][3],
					&s->off[x][4], &s->off[x][5], &s->off[x][6], &s->off[x][7]) != 8) {
						if (x == 8) {
							// originally there were only 8 instances
							// now we support upto 32, so if we die on the 8'th line
							// it's probably just the next IP block
							s->next = calloc(1, sizeof *s);
							if (!s->next) {
								while (os) {
									s = os->next;
									free(os);
									os = s;
								}
								fclose(f);
								return NULL;
							}
							s = s->next;
							goto retry8;
						} else {
							errout("[ERROR]: Invalid SOC15 offset line [%s]\n", linebuf);
						}
			}
		}
		s->next = calloc(1, sizeof *s);
		if (!s->next) {
			while (os) {
				s = os->next;
				free(os);
				os = s;
			}
			fclose(f);
			return NULL;
		}
		s = s->next;
	}
	fclose(f);
	return os;
}

void umr_database_free_soc15(struct umr_soc15_database *soc15)
{
	struct umr_soc15_database *s;

	while (soc15) {
		s = soc15->next;
		free(soc15);
		soc15 = s;
	}
}

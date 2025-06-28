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
#include <ctype.h>

static int parse_segments(char *linebuf, uint64_t *segs)
{
	int n = 0;

	// skip white space
	while (*linebuf && (*linebuf == '\t' || *linebuf == ' ')) ++linebuf;

	// start reading in 64-bit segment offsets
	while (n < UMR_SOC15_MAX_SEG && *linebuf && (*linebuf != '\n')) {
		if (sscanf(linebuf, "0x%"PRIx64, &segs[n]) != 1) {
			return n;
		}
		++n;

		// skip over number
		while (*linebuf && (*linebuf == 'x' || isxdigit(*linebuf))) ++linebuf;

		// skip white space
		while (*linebuf && (*linebuf == '\t' || *linebuf == ' ')) ++linebuf;
	}
	return n;
}

/**
 * @brief Reads the SOC15 database from a specified file.
 *
 * This function opens and reads the SOC15 offset data from a given file located in a specified path.
 * It parses the file to populate a linked list of `umr_soc15_database` structures, each representing
 * an IP block with its associated offsets.
 *
 * @param path The directory path where the database file is located.
 * @param filename The name of the SOC15 offset file to read.
 * @param errout A function pointer for error output handling.
 * @return A pointer to the head of a linked list of `umr_soc15_database` structures on success,
 *         or NULL if an error occurs (e.g., file not found, memory allocation failure).
 */
struct umr_soc15_database *umr_database_read_soc15(char *path, char *filename, umr_err_output errout)
{
	struct umr_soc15_database *s, *os;
	FILE *f;
	char linebuf[1024];
	int x, segs;

	f = umr_database_open(path, filename, 0);
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
		for (x = 0; x < UMR_SOC15_MAX_INST; x++) {
			fgets(linebuf, sizeof(linebuf), f);
			if ((segs = parse_segments(linebuf, &s->off[x][0])) < 8) {
				if (segs == 0) {
					// originally there were only 8 instances, then 32, now we support upto UMR_SOC15_MAX_SEG
					// if we die on the 8'th or 32'nd line it's probably just the next IP block
					// for future proofing we just assume it's the next IP block if we read zero entries
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

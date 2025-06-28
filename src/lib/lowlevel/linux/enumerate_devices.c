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

/**
 * @brief Enumerates AMD GPU devices and populates a list of ASIC structures.
 *
 * This function scans the PCI bus for AMD GPU devices under the `/sys/bus/pci/drivers/amdgpu` path,
 * creates an ASIC structure for each device found, and stores them in a dynamically allocated array.
 * The number of discovered ASICs is returned via the `no_asics` parameter.
 *
 * @param errout A function pointer to handle error output messages.
 * @param database_path Path to the UMR database used for ASIC discovery.
 * @param global_options Pointer to a structure containing global options that affect the discovery process.
 * @param asics Output parameter: a pointer to an array of pointers to `umr_asic` structures, which will be allocated by this function.
 * @param no_asics Output parameter: a pointer to an integer where the number of discovered ASICs will be stored.
 *
 * @return 0 on success, -1 on failure (e.g., if the directory cannot be opened).
 */
int umr_enumerate_device_list(umr_err_output errout, const char *database_path, struct umr_options *global_options, struct umr_asic ***asics, int *no_asics, int xgmi_scan)
{
	struct umr_options options;
	int x;
	DIR *dir;
	struct dirent *de;

	*asics = NULL;
	*no_asics = 0;

	dir = opendir("/sys/bus/pci/drivers/amdgpu");
	if (!dir) {
		errout("[ERROR]: Cannot open path /sys/bus/pci/drivers/amdgpu to enumerate devices.\n");
		return -1;
	}

	*asics = calloc(256, sizeof *asics); // allocate enough pointers for upto 128 devices

	x = 0;
	while (x < 256 && (de  = readdir(dir))) {
		memset(&options, 0, sizeof options);
		if (global_options)
			options = *global_options;
		options.quiet = 1;
		strncpy(options.database_path, database_path, sizeof(options.database_path));
		if (sscanf(de->d_name, "%04x:%02x:%02x.%01x",
				&options.pci.domain, &options.pci.bus, &options.pci.slot,
				&options.pci.func) == 4) {
			// we found a PCI bus address
			(*asics)[x] = umr_discover_asic(&options, errout);
			if ((*asics)[x]) {
				char devicepath[512];
				FILE *f;

				umr_scan_config((*asics)[x], xgmi_scan);

				// grab the DID
				sprintf(devicepath, "/sys/bus/pci/drivers/amdgpu/%s/device", de->d_name);
				f = fopen(devicepath, "r");
				if (f) {
					fscanf(f, "%x", &((*asics)[x]->did));
					fclose(f);
				} else {
					errout("[ERROR]: Could not open 'device' file for enumeration path=<%s>\n", devicepath);
				}

				++x;

				if (global_options && global_options->test_log && global_options->test_log_fd) {
					fprintf(global_options->test_log_fd, "-----\n");
				}
			}
		}
	}
	closedir(dir);
	*no_asics = x;

	return 0;
}

void umr_enumerate_device_list_free(struct umr_asic **asics)
{
	int x;
	for (x = 0; x < 128; x++) {
		if (asics[x]) {
			umr_close_asic(asics[x]);
		}
	}
	free(asics);
}

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
#include "umr_rumr.h"
#include "umrapp.h"
#include <signal.h>
#include <time.h>
#include <stdarg.h>

static void helptext(umr_err_output errout)
{
    errout("Available umr --script commands:\n\n");
    errout("\tinstances\n\t\tList all AMDGPU instances in space delimited format\n\n");
    errout("\tpci-instances <did>\n\t\tList all AMDGPU instances matching a given PCI DID in space delimited format\n\n");
    errout("\tpci-did <instance>\n\t\tOutput the PCI device ID (did) of the AMDGPU device with a given instance\n\n");
    errout("\tpci-bus <instance>\n\t\tOutput the PCI device bus address of the AMDGPU device with a given instance\n\n");
    errout("\tpci-bus-to-instance <busno>\n\t\tOutput the DRI instance matching a PCI device bus address\n\n");
    errout("\txcds <instance>\n\t\tList all GC partitions for a given device\n\n");
    errout("\tgfxname <instance>\n\t\tOutputs the base name for the GC IP block of a given GPU instance\n\n");
}

void umr_handle_scriptware(umr_err_output errout, char *database_path, char **argv, int argc)
{
    struct umr_asic **devices;
    int x, y, no_asics;

    if (argc == 0) {
        helptext(errout);
        return;
    }

    if (umr_enumerate_device_list(errout, database_path, NULL, &devices, &no_asics, 0)) {
        errout("[ERROR]: Could not enumerate AMDGPU devices on this host.\n");
        return;
    }

    for (x = 0; x < argc; x++) {
        if (!strcmp(argv[x], "instances")) {
            for (y = 0; y < no_asics; y++) {
                errout("%d ", devices[y]->instance);
            }
            errout("\n");
        } else if (!strcmp(argv[x], "pci-instances")) {
            if (x + 1 < argc) {
                uint32_t did;
                sscanf(argv[x+1], "%"SCNx32, &did);
                for (y = 0; y < no_asics; y++) {
                    if (devices[y]->did == did) {
                        errout("%d ", devices[y]->instance);
                    }
                }
                errout("\n");
                ++x;
            } else {
                errout("[ERROR]: 'pci-instances' --script command requires one parameter.\n");
            }
        } else if (!strcmp(argv[x], "pci-did")) {
            if (x + 1 < argc) {
                int inst;
                sscanf(argv[x+1], "%d", &inst);
                for (y = 0; y < no_asics; y++) {
                    if (devices[y]->instance == inst) {
                        errout("0x%x ", devices[y]->did);
                    }
                }
                errout("\n");
                ++x;
            } else {
                errout("[ERROR]: 'pci-did' --script command requires one parameter.\n");
            }
        } else if (!strcmp(argv[x], "pci-bus-to-instance")) {
            if (x + 1 < argc) {
                int y;
                for (y = 0; y < no_asics; y++) {
                    if (!strcmp(devices[y]->options.pci.name, argv[x+1])) {
                        errout("%d ", devices[y]->instance);
                        break;
                    }
                }
                errout("\n");
                ++x;
            } else {
                errout("[ERROR]: 'pci-did' --script command requires one parameter.\n");
            }
        } else if (!strcmp(argv[x], "pci-bus")) {
            if (x + 1 < argc) {
                int inst;
                sscanf(argv[x+1], "%d", &inst);
                for (y = 0; y < no_asics; y++) {
                    if (devices[y]->instance == inst) {
                        errout(devices[y]->options.pci.name);
                    }
                }
                errout("\n");
                ++x;
            } else {
                errout("[ERROR]: 'pci-did' --script command requires one parameter.\n");
            }
        } else if (!strcmp(argv[x], "gfxname")) {
            if (x + 1 < argc) {
                int inst, z;
                sscanf(argv[x+1], "%d", &inst);
                for (y = 0; y < no_asics; y++) {
                    if (devices[y]->instance == inst) {
                        for (z = 0; z < devices[y]->no_blocks; z++) {
                            if (strstr(devices[y]->blocks[z]->ipname, "gfx")) {
                                uint32_t inst;
                                char name[64];
                                if (sscanf(devices[y]->blocks[z]->ipname, "%[0-9a-z]{%"SCNu32"}", name, &inst) == 2) {
                                    errout("%s", name);
                                    break;
                                }
                                // must be only GC instance
                                errout("%s", devices[y]->blocks[z]->ipname);
                                break;
                            }
                        }
                    }
                }
                errout("\n");
                ++x;
            } else {
                errout("[ERROR]: 'gfxname' --script command requires one parameter.\n");
            }
        } else if (!strcmp(argv[x], "xcds")) {
            if (x + 1 < argc) {
                int inst, z;
                sscanf(argv[x+1], "%d", &inst);
                for (y = 0; y < no_asics; y++) {
                    if (devices[y]->instance == inst) {
                        for (z = 0; z < devices[y]->no_blocks; z++) {
                            if (strstr(devices[y]->blocks[z]->ipname, "gfx")) {
                                uint32_t inst;
                                char name[64];
                                if (sscanf(devices[y]->blocks[z]->ipname, "%[0-9a-z]{%"SCNu32"}", name, &inst) == 2) {
                                    errout("%d ", inst);
                                }
                            }
                        }
                    }
                }
                errout("\n");
                ++x;
            } else {
                errout("[ERROR]: 'xcds' --script command requires one parameter.\n");
            }
        }
   }
}
 
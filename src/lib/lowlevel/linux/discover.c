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
#include <dirent.h>
#include <sys/types.h>

#include "umr.h"
#include "umr_rumr.h"

static int is_did_match(struct umr_asic *asic, unsigned did)
{
	struct umr_asic *tmp;
	int r = 0, q;
	int tryipdiscovery = 0;

	q = asic->options.quiet;
	asic->options.quiet = 1;

	tmp = umr_discover_asic_by_did(&asic->options, did, asic->err_msg, &tryipdiscovery);
	if (tmp) {
		if (!strcmp(tmp->asicname, asic->asicname)) {
			asic->did = did;
			r = 1;
		}
		umr_close_asic(tmp);
	}
	asic->options.quiet = q;
	return r;
}

static int find_pci_instance(const char* pci_string)
{
	DIR *dir;
	struct dirent *dir_entry;
	int saved = -1;

	dir = opendir("/sys/kernel/debug/dri");
	if (dir == NULL) {
		perror("Couldn't open DRI under debugfs");
		return -1;
	}

	while ((dir_entry = readdir(dir)) != NULL) {
		char device[512], name[512];
		int parsed_device;
		FILE *f;

		// ignore . and ..
		if (strcmp(dir_entry->d_name, ".") == 0 ||
		    strcmp(dir_entry->d_name, "..") == 0)
			continue;

		// ignore non AMDGPU DRI directoties
		snprintf(name, sizeof(name), "/sys/kernel/debug/dri/%s/amdgpu_regs2",
			dir_entry->d_name);
		f = fopen(name, "r");
		if (!f)
			continue;
		fclose(f);

		// now try to match PCI bus address
		snprintf(name, sizeof(name), "/sys/kernel/debug/dri/%s/name",
			dir_entry->d_name);

		f = fopen(name, "r");
		if (!f)
			continue;

		device[sizeof(device) - 1] = 0;
		parsed_device = fscanf(f, "%*s %255s", device);
		fclose(f);

		if (parsed_device != 1)
			continue;

		// strip off dev= for kernels > 4.7
		if (strstr(device, "dev="))
			memmove(device, device + 4, strlen(device) - 3);
		if (strcmp(pci_string, device) == 0) {
			// save the instance in case this is pre-ipdiscovery hardware
			saved = atoi(dir_entry->d_name);

			// test if IP discovery exists for this
			snprintf(name, sizeof(name), "/sys/class/drm/card%s/device/ip_discovery/die/0/num_ips", dir_entry->d_name);
			f = fopen(name, "r");
			if (!f)
				continue;
			fclose(f);
			break;
		}
	}
	closedir(dir);
	return saved;
}


/**
 * umr_discover_asic - Search for an ASIC in the system
 *
 * @options: The ASIC options that control how an ASIC is found and are bound to the structure once found.
 * @errout:  Error output function pointer to handle error messages.
 *
 * This function searches for an ASIC based on the provided options. It can discover the ASIC by:
 * - Virtual device if @options->dev_name starts with a '.'.
 * - PCI bus address specified in @options->pci.
 * - DRI instance specified in @options->instance.
 * - Device name specified in @options->dev_name.
 *
 * The function handles various scenarios such as checking for the presence of the amdgpu kernel module,
 * reading device IDs, and mapping PCI memory if necessary.
 *
 * @return A pointer to the discovered ASIC structure on success, or NULL on failure.
 */
struct umr_asic *umr_discover_asic(struct umr_options *options, umr_err_output errout)
{
	char driver[512], name[256], fname[256];
	FILE *f;
	unsigned did = 0;
	struct umr_asic *asic = NULL;
	long trydid = options->forcedid;
	int busmatch = 0, parsed_did, need_config_scan = 0;
	int tryipdiscovery = 0;

	// virtual device
	if (options->dev_name[0] == '.') {
		options->is_virtual = 1;
		asic = umr_discover_asic_by_name(options, options->dev_name + 1, errout);
		if (asic)
			asic->options = *options;
		return asic;
	}

	// Try to map to instance if we have a specific pci device
	if (options->pci.domain || options->pci.bus ||
	    options->pci.slot || options->pci.func) {
		int parsed_did, inst = -1;
		unsigned long did;

		snprintf(options->pci.name, sizeof(options->pci.name), "%04x:%02x:%02x.%x",
			options->pci.domain, options->pci.bus, options->pci.slot,
			options->pci.func);

		if (!options->no_kernel)
			inst = find_pci_instance(options->pci.name);

		if (inst >= 0) {
			options->instance = inst;

			snprintf(driver, sizeof(driver), "/sys/bus/pci/devices/%s/device", options->pci.name);
			f = fopen(driver, "r");
			if (!f) {
				if (!options->quiet) perror("Cannot open PCI device name under sysfs (is a display attached?)");
				return NULL;
			}
			parsed_did = fscanf(f, "0x%04lx", &did);
			trydid = did;
			fclose(f);
			if (parsed_did != 1) {
				if (!options->quiet) printf("Could not read device id");
				return NULL;
			}

			if (options->no_kernel) {
				// try loading an .sasic
				struct rumr_buffer *buf;
				char fname[32];

				sprintf(fname, "0x%"PRIx32".sasic", (uint32_t)did);
				buf = rumr_load_serialized_asic(fname, options->database_path);
				if (buf) {
					asic = rumr_parse_serialized_asic(buf);
					rumr_buffer_free(buf);
					if (asic) {
						asic->options = *options;
					}
				}
			}
		}
	}

	// try to scan via debugfs
	if (!asic && options->instance >= 0 && !options->no_kernel) {
		asic = calloc(1, sizeof *asic);
		if (asic) {
			asic->instance = options->instance;
			asic->options  = *options;
			if (!umr_scan_config(asic, 0) && asic->config.pci.device)
				trydid = asic->config.pci.device;
			umr_free_asic(asic);
			asic = NULL;
		}
	}

	if (!options->no_kernel) {
		snprintf(name, sizeof(name)-1, "/sys/kernel/debug/dri/%d/name", options->instance);
		f = fopen(name, "r");
		if (!f && options->instance >= 0 && !options->no_kernel && !options->use_pci) {
			int found = 0;
			if (!options->quiet) {
				f = popen("lsmod | grep ^amdgpu", "r");
				while (fgets(name, sizeof(name)-1, f)) {
					if (strstr(name, "amdgpu"))
						found = 1;
				}
				pclose(f);

				perror("Cannot open DRI name under debugfs");
				if (!found)
					printf("ERROR: amdgpu.ko is not loaded.\n");
				else
					printf("ERROR: amdgpu.ko is loaded but /sys/kernel/debug/dri/%d/name is not found\n", options->instance);
			}
			return NULL;
		} else if (f) {
			int r;

			r = fscanf(f, "%*s %s", name);
			fclose(f);
			if (r == 1) {
				// strip off dev= for kernels > 4.7
				if (strstr(name, "dev="))
					memmove(name, name+4, strlen(name)-3);

				if (!strlen(options->pci.name)) {
					// read the PCI info
					strcpy(options->pci.name, name);
					sscanf(name, "%04x:%02x:%02x.%x",
						&options->pci.domain,
						&options->pci.bus,
						&options->pci.slot,
						&options->pci.func);
					need_config_scan = 1;
				}
			}
		}

		if (trydid < 0) {
			snprintf(driver, sizeof(driver)-1, "/sys/bus/pci/devices/%s/device", name);
			f = fopen(driver, "r");
			if (!f) {
				if (!options->quiet) perror("Cannot open PCI device name under sysfs (is a display attached?)");
				return NULL;
			}

			parsed_did = fscanf(f, "0x%04x", &did);
			fclose(f);
			if (parsed_did != 1) {
				if (!options->quiet) printf("Could not read device id");
				return NULL;
			}
			asic = umr_discover_asic_by_did(options, did, errout, &tryipdiscovery);
		} else {
			if (options->dev_name[0]) {
				asic = umr_discover_asic_by_name(options, options->dev_name, errout);
			} else {
				asic = umr_discover_asic_by_did(options, trydid, errout, &tryipdiscovery);
				if (!asic && tryipdiscovery) {
					char buf[32];
					sprintf(buf, "amd%04" PRIx64, (uint64_t)trydid);
					asic = umr_discover_asic_by_name(options, buf, errout);
					if (asic && !options->quiet)
						errout("[WARNING]: Unknown ASIC [%s] should be added to pci.did to get proper name\n", buf);
				}
			}
		}
	}

	if (asic) {
		asic->err_msg = errout;
		memcpy(&asic->options, options, sizeof(*options));
		if (!asic->options.no_kernel) {
			snprintf(fname, sizeof(fname)-1, "/sys/kernel/debug/dri/%d/amdgpu_regs2", asic->instance);
			asic->fd.mmio2 = open(fname, O_RDWR);
			if (asic->fd.mmio2 >= 0) {
				asic->fd.mmio = -1;
			} else {
				// only open this if regs2 is not found
				snprintf(fname, sizeof(fname)-1, "/sys/kernel/debug/dri/%d/amdgpu_regs", asic->instance);
				asic->fd.mmio = open(fname, O_RDWR);
			}
			snprintf(fname, sizeof(fname)-1, "/sys/kernel/debug/dri/%d/amdgpu_regs_didt", asic->instance);
			asic->fd.didt = open(fname, O_RDWR);
			snprintf(fname, sizeof(fname)-1, "/sys/kernel/debug/dri/%d/amdgpu_regs_pcie", asic->instance);
			asic->fd.pcie = open(fname, O_RDWR);
			snprintf(fname, sizeof(fname)-1, "/sys/kernel/debug/dri/%d/amdgpu_regs_smc", asic->instance);
			asic->fd.smc = open(fname, O_RDWR);
			snprintf(fname, sizeof(fname)-1, "/sys/kernel/debug/dri/%d/amdgpu_sensors", asic->instance);
			asic->fd.sensors = open(fname, O_RDONLY);
			snprintf(fname, sizeof(fname)-1, "/sys/kernel/debug/dri/%d/amdgpu_vram", asic->instance);
			asic->fd.vram = open(fname, O_RDWR);
			snprintf(fname, sizeof(fname)-1, "/sys/kernel/debug/dri/%d/amdgpu_gprwave", asic->instance);
			asic->fd.gprwave = open(fname, O_RDWR);
			if (asic->fd.gprwave < 0) {
				snprintf(fname, sizeof(fname)-1, "/sys/kernel/debug/dri/%d/amdgpu_gpr", asic->instance);
				asic->fd.gpr = open(fname, O_RDONLY);
				snprintf(fname, sizeof(fname)-1, "/sys/kernel/debug/dri/%d/amdgpu_wave", asic->instance);
				asic->fd.wave = open(fname, O_RDONLY);
			} else {
				asic->fd.gpr = -1;
				asic->fd.wave = -1;
			}

			snprintf(fname, sizeof(fname)-1, "/sys/kernel/debug/dri/%d/amdgpu_iova", asic->instance);
			asic->fd.iova = open(fname, O_RDWR);
			snprintf(fname, sizeof(fname)-1, "/sys/kernel/debug/dri/%d/amdgpu_iomem", asic->instance);
			asic->fd.iomem = open(fname, O_RDWR);
			snprintf(fname, sizeof(fname)-1, "/sys/kernel/debug/dri/%d/amdgpu_gfxoff", asic->instance);
			asic->fd.gfxoff = open(fname, O_RDWR);
			asic->fd.drm = -1; // default to closed
			// if appending to the fd list remember to update close_asic() and discover_by_did()...
		} else {
			// no files open!
			asic->fd.mmio2 = -1;
			asic->fd.mmio = -1;
			asic->fd.didt = -1;
			asic->fd.pcie = -1;
			asic->fd.smc = -1;
			asic->fd.sensors = -1;
			asic->fd.vram = -1;
			asic->fd.gprwave = -1;
			asic->fd.gpr = -1;
			asic->fd.wave = -1;
			asic->fd.drm = -1;
			asic->fd.iova = -1;
			asic->fd.iomem = -1;
			asic->fd.gfxoff = -1;
		}

		if (options->use_pci) {
			// init PCI mapping
			int use_region;
			void *pcimem_v;
			struct pci_device_iterator *pci_iter;
			pciaddr_t pci_region_addr;

			pci_system_init();
			pci_iter = pci_id_match_iterator_create(NULL);
			if (!pci_iter) {
				errout("[ERROR]: Cannot create PCI iterator");
				goto err_pci;
			}
			do {
				asic->pci.pdevice = pci_device_next(pci_iter);
				if (options->pci.domain || options->pci.bus || options->pci.slot || options->pci.func) {
					while (asic->pci.pdevice && (
						options->pci.domain != asic->pci.pdevice->domain ||
						options->pci.bus != asic->pci.pdevice->bus ||
						options->pci.slot != asic->pci.pdevice->dev ||
						options->pci.func != asic->pci.pdevice->func)) {
						asic->pci.pdevice = pci_device_next(pci_iter);
					}

					// indicate we found an exact match for the pci bus/slot
					// this is used because NPI use cases won't have names to
					// match against in is_did_match()
					if (asic->pci.pdevice)
						busmatch = 1;
				}
			} while (asic->pci.pdevice && !(busmatch || (asic->pci.pdevice->vendor_id == 0x1002 && is_did_match(asic, asic->pci.pdevice->device_id))));

			if (!asic->pci.pdevice) {
				errout("[ERROR]: Could not find ASIC with DID of %04lx\n", (unsigned long)asic->did);
				goto err_pci;
			}
			pci_iterator_destroy(pci_iter);

			// enable device if kernel module isn't present
			if (asic->options.no_kernel)
				pci_device_enable(asic->pci.pdevice);

			pci_device_probe(asic->pci.pdevice);

			use_region = 6;
			// try to detect based on ASIC family
			if (asic->family <= FAMILY_SI) {
				// try region 2 for SI
				if (asic->pci.pdevice->regions[2].is_64 == 0 &&
					asic->pci.pdevice->regions[2].is_prefetchable == 0 &&
					asic->pci.pdevice->regions[2].is_IO == 0) {
						use_region = 2;
				}
			} else if (asic->family <= FAMILY_VI) {
				// try region 5 for CIK..VI
				if (asic->pci.pdevice->regions[5].is_64 == 0 &&
					asic->pci.pdevice->regions[5].is_prefetchable == 0 &&
					asic->pci.pdevice->regions[5].is_IO == 0) {
						use_region = 5;
				}
			}

			// scan for a region 256K <= X <= 4096K which is 32-bit, non IO, non prefetchable
			// manually scan because a lot of distro ship with a buggy libpciaccess
			if (use_region == 6) {
				// open /sys/bus/pci/devices/${pciaddr}/resource
				uint64_t lowaddr, highaddr, size, flags;
				char linebuf[512];
				FILE *res;
				sprintf(linebuf, "/sys/bus/pci/devices/%04"PRIx32":%02"PRIx32":%02"PRIx32".%d/resource",
					(uint32_t)asic->pci.pdevice->domain,
					(uint32_t)asic->pci.pdevice->bus,
					(uint32_t)asic->pci.pdevice->dev,
					(int)asic->pci.pdevice->func);
				res = fopen(linebuf, "r");
				if (res) {
					use_region = 0;
					while (fgets(linebuf, sizeof linebuf, res)) {
						sscanf(linebuf, "0x%"PRIx64" 0x%"PRIx64" 0x%"PRIx64, &lowaddr, &highaddr, &flags);
						size = highaddr - lowaddr + 1;
						if (size >= (256 * 1024ULL) && size <= (4096 * 1024ULL) && !(flags & (1|4|8)))
							break;
						++use_region;
					}
					fclose(res);
				}
			}

			if (use_region >= 6) {
				errout("[ERROR]: Could not find PCI region (debugfs mode might still work)\n");
				goto err_pci;
			}
			asic->pci.region = use_region;

			pci_region_addr = asic->pci.pdevice->regions[use_region].base_addr;
			if (pci_device_map_range(asic->pci.pdevice, pci_region_addr, asic->pci.pdevice->regions[use_region].size, PCI_DEV_MAP_FLAG_WRITABLE, &pcimem_v)) {
				errout("[ERROR]: Could not map PCI memory\n");
				goto err_pci;
			}
			asic->pci.mem = pcimem_v;
		}
	}

	if (asic && need_config_scan)
		umr_scan_config(asic, 0);

	return asic;
err_pci:
	umr_close_asic(asic);
	return NULL;
}


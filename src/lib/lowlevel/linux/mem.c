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
#include <inttypes.h>


#if 0
#define DEBUG(...) asic->err_msg("DEBUG:" __VA_ARGS__)
#else
#define DEBUG(...)
#endif

/** umr_vm_dma_to_phys -- Convert a GPU bound bus address to CPU physical */
 uint64_t umr_vm_dma_to_phys(struct umr_asic *asic, uint64_t dma_addr)
{
	uint64_t phys;
	if (asic->fd.iova >= 0) {
		// older kernels had a iova debugfs file which would return
		// an address given a seek to a given address this has been
		// removed in newer kernels
		lseek(asic->fd.iova, dma_addr & ~0xFFFULL, SEEK_SET);
		if (read(asic->fd.iova, &phys, 8) != 8) {
			asic->err_msg("[ERROR]: Could not read from debugfs iova file for address %" PRIx64 "\n", dma_addr);
			return 0;
		}
	} else {
		// newer kernels use iomem which requires a GPU bus address
		// to read/write system memory bound to the GPU
		phys = dma_addr;
	}
	return phys;
}

/**
 * umr_access_sram - Access system memory
 */
int umr_access_sram(struct umr_asic *asic, uint64_t address, uint32_t size, void *dst, int write_en)
{
	int fd, need_close=0;
	uint32_t r;

	DEBUG("Reading physical sys addr: 0x" PRIx64 "\n", address);

	// check if we have access to the amdgpu_iomem debugfs entry
	if (asic->fd.iomem >= 0) {
		fd = asic->fd.iomem;
	} else {
retry:
		// if not try to read system memory directly
		need_close = 1;

		// try /dev/fmem first
		fd = open("/dev/fmem", O_RDWR);
		if (fd < 0)
			fd = open("/dev/mem", O_RDWR | O_DSYNC);
	}

	if (fd >= 0) {
		lseek(fd, address, SEEK_SET);
		if (write_en == 0) {
			memset(dst, 0xFF, size);
			if ((r = read(fd, dst, size)) != size) {
				perror("Cannot read from system memory");
				asic->err_msg("[ERROR]: Accessing system memory returned: %d\n", r);
				if (need_close)
					close(fd);
				if (fd == asic->fd.iomem)
					goto retry;
				return -1;
			}
			if (asic->options.test_log && asic->options.test_log_fd) {
				uint8_t *tlp = (uint8_t *)dst;
				unsigned x;
				fprintf(asic->options.test_log_fd, "SYSRAM@0x%"PRIx64" = {", address);
				for (x = 0; x < size; x++) {
					fprintf(asic->options.test_log_fd, "%02"PRIx8, tlp[x]);
				}
				fprintf(asic->options.test_log_fd, "}\n");
			}
		} else {
			if ((r = write(fd, dst, size)) != size) {
				perror("Cannot write to system memory");
				asic->err_msg("[ERROR]: Accessing system memory returned: %d\n", r);
				if (need_close)
					close(fd);
				if (fd == asic->fd.iomem)
					goto retry;
				return -1;
			}
		}
		if (need_close)
			close(fd);
		return 0;
	}
	return -1;
}

/**
 * umr_access_linear_vram -- Access VRAM linearly without VM translation
 */
int umr_access_linear_vram(struct umr_asic *asic, uint64_t address, uint32_t size, void *data, int write_en)
{
	lseek(asic->fd.vram, address, SEEK_SET);
	if (write_en == 0) {
		if (read(asic->fd.vram, data, size) != size) {
			asic->err_msg("[ERROR]: Could not read from VRAM at address 0x%" PRIx64 "\n", address);
			return -1;
		}
		if (asic->options.test_log && asic->options.test_log_fd) {
			uint8_t *tlp = (uint8_t *)data;
			unsigned x;
			fprintf(asic->options.test_log_fd, "VRAM@0x%"PRIx64" = {", address);
			for (x = 0; x < size; x++) {
				fprintf(asic->options.test_log_fd, "%02"PRIx8, tlp[x]);
			}
			fprintf(asic->options.test_log_fd, "}\n");
		}
	} else {
		if (write(asic->fd.vram, data, size) != size) {
			asic->err_msg("[ERROR]: Could not write to VRAM at address 0x%" PRIx64 "\n", address);
			return -1;
		}
	}
	return 0;
}

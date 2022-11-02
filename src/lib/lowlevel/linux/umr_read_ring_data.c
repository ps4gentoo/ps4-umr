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

/**
 * umr_read_ring_data - Read the entire contents of a ring
 *
 * @ringname:  Common name for the ring, e.g., 'gfx' or 'comp_1.0.0'
 * @ringsize:  Receives the size of the ring in bytes (exluding the 12 byte header)
 *
 * Returns a buffer which contains the ring contents.  The first three
 * words of the buffer are the read, write, and devices write pointer
 * respectively.  The next @ringsize - 12 bytes are the words
 * of the ring.  Note that the ring buffer can wrap around
 * so you need to reduce your indecies modulo (@ringsize / 4).
 */
void *umr_read_ring_data(struct umr_asic *asic, char *ringname, uint32_t *ringsize)
{
	int fd;
	uint32_t r;
	void *ring_data;
	char fname[128];

	if (asic->options.test_log && !asic->options.test_log_fd) {
		return umr_test_harness_get_ring_data(asic, ringsize);
	} else {
		snprintf(fname, sizeof(fname)-1, "/sys/kernel/debug/dri/%d/amdgpu_ring_%s", asic->instance, ringname);
		fd = open(fname, O_RDONLY);
		if (fd < 0) {
			asic->err_msg("[ERROR]: Could not open ring debugfs file '%s'\n", fname);
			if (asic->family >= FAMILY_NV && !strcmp(ringname, "gfx"))
				asic->err_msg("[WARNING]: On Navi and later ASICs the gfx ring name has changed, for instance: 'gfx_0.0.0'\n");
			return NULL;
		}

		/* determine file size */
		*ringsize = lseek(fd, 0, SEEK_END) - 12;
		lseek(fd, 0, SEEK_SET);

		ring_data = calloc(1, *ringsize + 12);
		if (!ring_data) {
			close(fd);
			asic->err_msg("[ERROR]: Out of memory\n");
			return NULL;
		}
		r = read(fd, ring_data, *ringsize + 12);
		close(fd);
		if (r != *ringsize + 12) {
			free(ring_data);
			return NULL;
		}

		// store in test vector if open
		if (asic->options.test_log && asic->options.test_log_fd) {
			uint32_t *rd = ring_data, x;
			fprintf(asic->options.test_log_fd, "RINGDATA = { ");
			for (x = 0; x < (*ringsize + 12); x += 4) {
				if (x) {
					fprintf(asic->options.test_log_fd, ", ");
				}
				fprintf(asic->options.test_log_fd, "0x%"PRIx32, rd[x/4]);
			}
			fprintf(asic->options.test_log_fd, "}\n");
		}
	}

	return ring_data;
}

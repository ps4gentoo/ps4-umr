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
#include "umrapp.h"
#include <inttypes.h>

void umr_read_ring(struct umr_asic *asic, char *ringpath)
{
	char ringname[32], from[32], to[32];
	int  enable_decoder, gprs;
	uint32_t wptr, rptr, drv_wptr, ringsize, start, end, value,
		 *ring_data;
	struct umr_ring_decoder decoder, *pdecoder, *ppdecoder;
	struct umr_wave_data *wd;

	memset(ringname, 0, sizeof ringname);
	memset(from, 0, sizeof from);
	memset(to, 0, sizeof to);
	if (sscanf(ringpath, "%[a-z0-9._][%[.0-9]:%[.0-9]]", ringname, from, to) < 1) {
		printf("Invalid ringpath\n");
		return;
	}

	// only decode PM4 packets on certain rings
	memset(&decoder, 0, sizeof decoder);
	if (!memcmp(ringname, "gfx", 3) ||
	    !memcmp(ringname, "uvd", 3) ||
	    !memcmp(ringname, "vcn_dec", 7) ||
	    !memcmp(ringname, "vcn_enc", 7) ||
	    !memcmp(ringname, "kiq", 3) ||
	    !memcmp(ringname, "comp", 4)) {
		enable_decoder = 1;
		decoder.pm = 4;
	} else if (!memcmp(ringname, "sdma", 4) ||
		   !memcmp(ringname, "page", 4)) {
		enable_decoder = 1;
		decoder.pm = 3;
	} else {
		enable_decoder = 0;
	}

	if (asic->options.halt_waves)
		umr_sq_cmd_halt_waves(asic, UMR_SQ_CMD_HALT);

	ring_data = umr_read_ring_data(asic, ringname, &ringsize);
	if (!ring_data)
		goto end;

	/* read pointers */
	rptr = ring_data[0]<<2;
	wptr = ring_data[1]<<2;
	drv_wptr = ring_data[2]<<2;

	/* default to reading entire ring */
	if (!from[0]) {
		start = 0;
		end   = ringsize-4;
	} else {
		if (from[0] == '.') {
			if (to[0] == 0 || to[0] == '.') {
				/* Notation: [.] or [.:.], meaning
				 * [rptr, wptr].
				 */
				start = rptr;
				end = wptr;
			} else {
				/* Notation: [.:k], k >=0, meaning
				 * [rptr, rtpr+k] double-words.
				 */
				start = rptr;
				sscanf(to, "%"SCNu32, &end);
				end *= 4;
				end = (start + end + ringsize) % ringsize;
			}
		} else {
			sscanf(from, "%"SCNu32, &start);
			start *= 4;

			if (to[0] != 0 && to[0] != '.') {
				/* [k:r] ==> absolute [k, r].
				 */
				sscanf(to, "%"SCNu32, &end);
				end *= 4;
				start %= ringsize;
				end %= ringsize;
			} else {
				/* to[0] is 0 or '.',
				 * [k] or [k:.] ==> [wptr - k, wptr]
				 */
				start = (wptr - start + ringsize) % ringsize;
				end = wptr;
			}
		}
	}

	/* dump data */
	printf("\n%s.%s.rptr == %lu\n%s.%s.wptr == %lu\n%s.%s.drv_wptr == %lu\n",
		asic->asicname, ringname, (unsigned long)rptr >> 2,
		asic->asicname, ringname, (unsigned long)wptr >> 2,
		asic->asicname, ringname, (unsigned long)drv_wptr >> 2);

	if (enable_decoder) {
		decoder.pm4.cur_opcode = 0xFFFFFFFF;
		decoder.sdma.cur_opcode = 0xFFFFFFFF;
	}

	do {
		value = ring_data[(start+12)>>2];
		printf("%s.%s.ring[%s%4lu%s] == %s0x%08lx%s   ",
			asic->asicname, ringname,
			BLUE, (unsigned long)start >> 2, RST,
			YELLOW, (unsigned long)value, RST);
		printf(" %c%c%c ",
			(start == rptr) ? 'r' : '.',
			(start == wptr) ? 'w' : '.',
			(start == drv_wptr) ? 'D' : '.');
		decoder.next_ib_info.addr = start / 4;
		if (enable_decoder)
			umr_print_decode(asic, &decoder, value, NULL);
		printf("\n");
		start += 4;
		start %= ringsize;
	} while (start != ((end + 4) % ringsize));
	free(ring_data);
	printf("\n");

	if (!asic->options.no_scan_waves) {
		gprs = asic->options.skip_gprs;
		asic->options.skip_gprs = 1;
		wd = umr_scan_wave_data(asic);
		asic->options.skip_gprs = gprs;
	} else {
		wd = NULL;
	}
	umr_dump_shaders(asic, &decoder, wd);
	umr_dump_data(asic, &decoder);
	pdecoder = decoder.next_ib;
	while (pdecoder) {
		if (!asic->options.no_follow_ib) {
			umr_dump_ib(asic, pdecoder);
			umr_dump_shaders(asic, pdecoder, wd);
			umr_dump_data(asic, &decoder);
		}
		ppdecoder = pdecoder->next_ib;
		free(pdecoder);
		pdecoder = ppdecoder;
	}

	while (wd) {
		struct umr_wave_data *pwd = wd->next;
		free(wd);
		wd = pwd;
	}

end:
	if (asic->options.halt_waves)
		umr_sq_cmd_halt_waves(asic, UMR_SQ_CMD_RESUME);
}

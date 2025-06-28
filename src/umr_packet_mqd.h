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
#ifndef UMR_PACKET_MQD_H_
#define UMR_PACKET_MQD_H_

// mqd decoding
enum umr_mqd_engine_sel {
	UMR_MQD_ENGINE_COMPUTE=0,
	UMR_MQD_ENGINE_RESERVED,
	UMR_MQD_ENGINE_SDMA0,
	UMR_MQD_ENGINE_SDMA1,
	UMR_MQD_ENGINE_GFX,
	UMR_MQD_ENGINE_MES,

	UMR_MQD_ENGINE_INVALID,
};

struct umr_mqd_fields {
	uint32_t offset;
	char *label;
};

uint32_t umr_mqd_decode_size(enum umr_mqd_engine_sel eng, enum chipfamily fam);
uint32_t umr_mqd_decode_rows(enum umr_mqd_engine_sel eng, enum chipfamily fam);
char **umr_mqd_decode_data(enum umr_mqd_engine_sel eng, enum chipfamily fam, uint32_t *data, char *match);

#endif

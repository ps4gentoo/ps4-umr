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
#ifndef UMR_IH_H_
#define UMR_IH_H_

/* IH decoding */
struct umr_ih_decode_ui {
	/** start_vector - Start processing an interrupt vector
	 *
	 * ui: The callback structure used
	 * offset: The offset in dwords of the start of the vector
	 */
	void (*start_vector)(struct umr_ih_decode_ui *ui, uint32_t offset);

	/** add_field - Add a field to the decoding of the interrupt vector
	 *
	 * ui: The callback structure used
	 * offset: The offset in dwords of the field
	 * field_name: Description of the field
	 * value: Value (if any) of the field
	 * str: A string value (if any) for the field
	 * ideal_radix:  The ideal radix to print the value in
	 */
	void (*add_field)(struct umr_ih_decode_ui *ui, uint32_t offset, const char *field_name, uint32_t value, char *str, int ideal_radix);

	/** done: Finish a vector */
	void (*done)(struct umr_ih_decode_ui *ui);

	/** data -- opaque pointer that can be used to track state information */
	void *data;
};

// decode interrupt vectors
int umr_ih_decode_vectors(struct umr_asic *asic, struct umr_ih_decode_ui *ui, uint32_t *ih_data, uint32_t length);

#endif

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

#define BITS(x, a, b) (unsigned long)((x >> (a)) & ((1ULL << ((b)-(a)))-1))

/** umr_ih_decode_vectors - Decode a series of interrupt vectors
 *
 * asic: The device the vectors came from
 * ui: Callback structure to handle the decoded data
 * ih_data: The vector data
 * length: Length of vector data in bytes (must be multiple of vector size)
 *
 * Returns the number of vectors processed.
 */
int umr_ih_decode_vectors(struct umr_asic *asic, struct umr_ih_decode_ui *ui, uint32_t *ih_data, uint32_t length)
{
	uint32_t off = 0;

	switch (asic->family) {
	case FAMILY_VI: // oss30
		while (length) {
			ui->start_vector(ui, off);
			ui->add_field(ui, off + 0, "SourceID",   BITS(ih_data[off + 0], 0, 8), NULL, 10); // TODO: add ID to name translation
			ui->add_field(ui, off + 1, "SourceData", BITS(ih_data[off + 1], 0, 24), NULL, 16);
			ui->add_field(ui, off + 2, "VMID",       BITS(ih_data[off + 2], 8, 16), NULL, 10);
			ui->add_field(ui, off + 2, "PASID",      BITS(ih_data[off + 2], 16, 32), NULL, 10);
			ui->add_field(ui, off + 3, "ContextID0", ih_data[off + 3], NULL, 16);
			ui->done(ui);
			length -= 16;
			off += 4;
		}
		return off / 4;

	case FAMILY_NV: // oss40/50
	case FAMILY_AI:
		while (length) {
			ui->start_vector(ui, off);
			ui->add_field(ui, off + 0, "ClientID", BITS(ih_data[off + 0], 0, 8), NULL, 10); // TODO: add ID to name translation
			ui->add_field(ui, off + 0, "SourceID", BITS(ih_data[off + 0], 8, 16), NULL, 10); // TODO: add ID to name translation
			ui->add_field(ui, off + 0, "RingID",   BITS(ih_data[off + 0], 16, 24), NULL, 10);
			ui->add_field(ui, off + 0, "VMID",     BITS(ih_data[off + 0], 24, 28), NULL, 10);
			ui->add_field(ui, off + 0, "VMID_TYPE", BITS(ih_data[off + 0], 31, 32), NULL, 10);
			ui->add_field(ui, off + 1, "Timestamp", ih_data[off + 1] + ((uint64_t)BITS(ih_data[off+2], 0, 16) << 32), NULL, 10);
			ui->add_field(ui, off + 2, "Timestamp_SRC", BITS(ih_data[off + 2], 31, 32), NULL, 10);
			ui->add_field(ui, off + 3, "PASID", BITS(ih_data[off + 3], 0, 16), NULL, 16);
			ui->add_field(ui, off + 4, "ContextID0", ih_data[off + 4], NULL, 16);
			ui->add_field(ui, off + 5, "ContextID1", ih_data[off + 5], NULL, 16);
			ui->add_field(ui, off + 6, "ContextID2", ih_data[off + 6], NULL, 16);
			ui->add_field(ui, off + 7, "ContextID3", ih_data[off + 7], NULL, 16);
			ui->done(ui);
			length -= 32;
			off += 8;
		}
		return off / 8;

	case FAMILY_SI:
	case FAMILY_CIK:
	case FAMILY_NPI:
	default:
		asic->err_msg("[BUG]: unhandled family case:%d in umr_ih_decode_vectors()\n", asic->family);
		return -1;
	}
	return 0;
}

#if 0
static void start_vector(struct umr_ih_decode_ui *ui, uint32_t offset)
{
	printf("Start: %lu offset\n", (unsigned long)offset);
}

static void add_field(struct umr_ih_decode_ui *ui, uint32_t offset, const char *field_name, uint32_t value, char *str, int ideal_radix)
{
	printf("Field: %lu offset, name==[%s], value==%lx,%lu, str==[%s]\n", offset, field_name, value, value, str ? str : "");
}

static void done(struct umr_ih_decode_ui *ui)
{
	printf("Done.\n");
}

static struct umr_ih_decode_ui myui = { &start_vector, &add_field, &done, NULL };

void ih_self_test(struct umr_asic *asic)
{
	uint32_t kat[] = {
		// SDMA1 #SDMA_TRAP { client_id: 9 source_id: e0 ring_id: 0 vm_id: 0 vmid_type: 1(MM) }
		0x8000E009, 0x65DF5E93, 0x00000649, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
		// DCE #DC_D1_CRTC_V_UPDATE { client_id: 4 source_id: 7 ring_id: 0 vm_id: 0 vmid_type: 1(MM) }
		0x80000704, 0x65FA01F2, 0x00000649, 0x00000000, 0x05FA01DC, 0x00000000, 0x00000000, 0x00000000,
		// GRBM_CP #CP_EOP_INTERRUPT { client_id: 14 source_id: b5 ring_id: 0 vm_id: 0 vmid_type: 0(GFX) }
		0x0000B514, 0x65FA5536, 0x80000649, 0x00000000, 0x056F8100, 0x00000000, 0x00000000, 0x00000000,
	};

	if (umr_ih_decode_vectors(asic, &myui, kat, sizeof(kat)) != (sizeof(kat) / 32))
		asic->err_msg("[ERROR]: Wrong number of vectors returned\n");
}

#endif

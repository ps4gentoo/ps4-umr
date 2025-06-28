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
 * Authors: Kevin Wang <kevinyang.wang@amd.com>
 *
 */
#include "umr.h"
#include "import/discovery.h"
#include "import/soc15_hw_ip.h"
#include <stdio.h>
#include <stddef.h>
#include <stdarg.h>
#include <endian.h>

#define UMR_DISCOVERY_TABLE_SIZE	(4 << 10)
#define UMR_DISCOVERY_TABLE_OFFSET	(64 << 10)
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define SIGNATURE(a, b, c, d) (((d) << 24) | ((c) << 16) | ((b) << 8) | (a))
#define ALIGN(val, align)   (((val) + (align - 1)) & ~(align - 1))

#ifndef UMR_NO_DRM
#include <amdgpu_drm.h>
static uint64_t get_full_vram_size(struct umr_asic *asic)
{
	struct drm_amdgpu_memory_info mem;
	char fname[64];

	if (asic->fd.drm < 0) {
		snprintf(fname, sizeof(fname)-1, "/dev/dri/card%d", asic->instance);
		asic->fd.drm = open(fname, O_RDWR);
	}

	if (umr_query_drm(asic, AMDGPU_INFO_MEMORY, &mem, sizeof(mem)))
	    return 0;

	return mem.vram.total_heap_size;
}
#else
static uint64_t get_full_vram_size(struct umr_asic *asic)
{
	return umr_read_reg_by_name_by_ip(asic, "nbio", "mmRCC_CONFIG_MEMSIZE") << 20;
}
#endif /* UMR_NO_DRM */

int umr_discovery_table_is_supported(struct umr_asic *asic)
{
	uint64_t vram_size, offset;
	struct binary_header header;
	int ret;

	vram_size = get_full_vram_size(asic);
	if (!vram_size)
		return 0;

	offset = vram_size - UMR_DISCOVERY_TABLE_OFFSET;

	memset(&header, 0, sizeof(header));
	ret = umr_access_linear_vram(asic, offset, ALIGN(sizeof(header), 4), &header, 0);
	if (ret)
		return 0;

	return le32toh(header.binary_signature) == BINARY_SIGNATURE ? 1 : 0;
}

int umr_discovery_read_table(struct umr_asic *asic, uint8_t *table, uint32_t *size)
{
	uint64_t vram_size, offset;
	int ret;

	if (!table && size) {
		*size = UMR_DISCOVERY_TABLE_SIZE;
		return 0;
	}

	vram_size = get_full_vram_size(asic);
	if (!vram_size)
		return -1;

	offset = vram_size - UMR_DISCOVERY_TABLE_OFFSET;

	if (table) {
		ret = umr_access_linear_vram(asic, offset, UMR_DISCOVERY_TABLE_SIZE,
					     table, 0);
		if (ret)
			return ret;
	}

	if (size)
		*size = UMR_DISCOVERY_TABLE_SIZE;

	return 0;
}

static uint16_t umr_calculate_discovery_checksum(uint8_t *data, uint16_t size)
{
	uint16_t checksum = 0;
	int i;

	for (i = 0; i < size; i++)
		checksum += data[i];

	return checksum;
}

static int umr_verify_discovery_table_by_id(uint8_t *table, int table_id)
{
	struct binary_header *bhdr = (struct binary_header *)table;
	struct table_info *info = &bhdr->table_list[table_id];
	uint16_t checksum = le16toh(info->checksum);
	uint16_t offset = le16toh(info->offset);
	uint16_t size = le16toh(info->size);

	return umr_calculate_discovery_checksum(table + offset, size) == checksum ? 0 : -1;
}

int umr_discovery_verify_table(struct umr_asic *asic, uint8_t *table)
{
	struct binary_header *bhdr = (struct binary_header *)table;
	uint16_t offset, size, checksum;

	/* 1. verify table header */
	if (le32toh(bhdr->binary_signature) != BINARY_SIGNATURE) {
		asic->err_msg("[ERROR]: invalid discovery table signature: 0x%08x\n",
			le32toh(bhdr->binary_signature));
		return -1;
	}

	/* 2. verify table checksum */
	offset = offsetof(struct binary_header, binary_size);
	size = le16toh(bhdr->binary_size) - offset;
	checksum = le16toh(bhdr->binary_checksum);

	if (umr_calculate_discovery_checksum(table + offset, size) != checksum) {
		asic->err_msg("[ERROR]: invalid discovery table checksum\n");
		return -1;
	}

	/* 3. verify discovery tables */
	if (umr_verify_discovery_table_by_id(table, IP_DISCOVERY)) {
		asic->err_msg("[ERROR]: invalid discovery table: IP_DISCOVERY\n");
		return -1;
	}

	if (umr_verify_discovery_table_by_id(table, GC)) {
		asic->err_msg("[ERROR]: invalid discovery table: GC\n");
		return -1;
	}

	if (umr_verify_discovery_table_by_id(table, HARVEST_INFO)) {
		asic->err_msg("[ERROR]: invalid discovery table: HARVEST_INFO\n");
		return -1;
	}

	return 0;
}

static void print_level_prefix(int level, FILE *stream)
{
	switch (level) {
	case 0:
		/* fprintf(stream, ""); */
		break;
	default:
		for (int i = 0; i < level; i++)
			fprintf(stream, "│   ");
		fprintf(stream, "├── ");
		break;
	}
}

static void lfprintf(int level, FILE *stream, const char *fmt, ...)
{
	va_list ap;
	print_level_prefix(level, stream);
	va_start(ap, fmt);
	vfprintf(stream, fmt, ap);
	va_end(ap);
}

#define HWID_NAME(hwid) \
	[hwid##_HWID] = #hwid

static char* hwid_name[] = {
	HWID_NAME(MP1),
	HWID_NAME(MP2),
	HWID_NAME(THM),
	HWID_NAME(SMUIO),
	HWID_NAME(FUSE),
	HWID_NAME(CLKA),
	HWID_NAME(PWR),
	HWID_NAME(GC),
	HWID_NAME(UVD),
	/* VCN == UVD */
	/* HWID_NAME(VCN), */
	HWID_NAME(AUDIO_AZ),
	HWID_NAME(ACP),
	HWID_NAME(DCI),
	HWID_NAME(DMU),
	HWID_NAME(DCO),
	HWID_NAME(DIO),
	HWID_NAME(XDMA),
	HWID_NAME(DCEAZ),
	HWID_NAME(DAZ),
	HWID_NAME(SDPMUX),
	HWID_NAME(NTB),
	HWID_NAME(IOHC),
	HWID_NAME(L2IMU),
	HWID_NAME(VCE),
	HWID_NAME(MMHUB),
	HWID_NAME(ATHUB),
	HWID_NAME(DBGU_NBIO),
	HWID_NAME(DFX),
	HWID_NAME(DBGU0),
	HWID_NAME(DBGU1),
	HWID_NAME(OSSSYS),
	HWID_NAME(HDP),
	HWID_NAME(SDMA0),
	HWID_NAME(SDMA1),
	HWID_NAME(ISP),
	HWID_NAME(DBGU_IO),
	HWID_NAME(DF),
	HWID_NAME(CLKB),
	HWID_NAME(FCH),
	HWID_NAME(DFX_DAP),
	HWID_NAME(L1IMU_PCIE),
	HWID_NAME(L1IMU_NBIF),
	HWID_NAME(L1IMU_IOAGR),
	HWID_NAME(L1IMU3),
	HWID_NAME(L1IMU4),
	HWID_NAME(L1IMU5),
	HWID_NAME(L1IMU6),
	HWID_NAME(L1IMU7),
	HWID_NAME(L1IMU8),
	HWID_NAME(L1IMU9),
	HWID_NAME(L1IMU10),
	HWID_NAME(L1IMU11),
	HWID_NAME(L1IMU12),
	HWID_NAME(L1IMU13),
	HWID_NAME(L1IMU14),
	HWID_NAME(L1IMU15),
	HWID_NAME(WAFLC),
	HWID_NAME(FCH_USB_PD),
	HWID_NAME(SDMA2),
	HWID_NAME(SDMA3),
	HWID_NAME(PCIE),
	HWID_NAME(PCS),
	HWID_NAME(DDCL),
	HWID_NAME(SST),
	HWID_NAME(IOAGR),
	HWID_NAME(NBIF),
	HWID_NAME(IOAPIC),
	HWID_NAME(SYSTEMHUB),
	HWID_NAME(NTBCCP),
	HWID_NAME(UMC),
	HWID_NAME(SATA),
	HWID_NAME(USB),
	HWID_NAME(CCXSEC),
	HWID_NAME(XGMI),
	HWID_NAME(XGBE),
	HWID_NAME(MP0),
};

static const char *hwid2name(uint32_t id)
{
	return hwid_name[id];
}

static int umr_dump_discovery_table__ip_discovery(struct umr_asic *asic, uint8_t *table, FILE *stream, int level)
{
	struct binary_header *bhdr = (struct binary_header *)table;
	struct table_info *table_info = &bhdr->table_list[IP_DISCOVERY];
	struct ip_discovery_header *ihdr = (struct ip_discovery_header *)(table + le32toh(table_info->offset));
	uint16_t num_dies, ip_offset;
	uint32_t signature = le32toh(ihdr->signature);

	if (signature != SIGNATURE('I', 'P', 'D', 'S')) {
		asic->err_msg("invalid IP_DISCOVERY signature: 0x%08x\n", signature);
		return -1;
	}

	lfprintf(level, stream, "signature: 0x%08x\n", le32toh(ihdr->signature));
	lfprintf(level, stream, "version: 0x%x\n", le16toh(ihdr->version));
	lfprintf(level, stream, "size: %d (0x%x)\n", le16toh(ihdr->size), le16toh(ihdr->size));
	lfprintf(level, stream, "id: 0x%08x\n", le32toh(ihdr->id), le32toh(ihdr->id));
	lfprintf(level, stream, "num_dies: 0x%x\n", le16toh(ihdr->num_dies));
	level++;

	num_dies = MIN(le16toh(ihdr->num_dies), 16);
	for (int i = 0; i < num_dies; i++) {
		struct die_info *die_info = &ihdr->die_info[i];
		uint16_t die_offset = le16toh(die_info->die_offset);
		struct die_header *dhdr = (struct die_header *)(table + die_offset);
		uint16_t num_ips = le16toh(dhdr->num_ips);
		uint16_t die_header_id = le16toh(dhdr->die_id);
		lfprintf(level, stream, "die_id: %d\n", die_header_id);
		lfprintf(level, stream, "num_ips: %d\n",num_ips);
		level++;

		ip_offset = die_offset + sizeof(*dhdr);
		for (int j = 0; j < num_ips; j++) {
			struct ip *ip = (struct ip *)(table + ip_offset);
			lfprintf(level, stream, "[%02d] hw_id:%s(%d)\n", j, hwid2name(le16toh(ip->hw_id)), le16toh(ip->hw_id));

			int ip_level = level + 1;
			lfprintf(ip_level, stream, "num_instances: %d\n", ip->number_instance);
			lfprintf(ip_level, stream, "major.min.rev: %d.%d.%d\n", ip->major, ip->minor, ip->revision);
			lfprintf(ip_level, stream, "harvest: %d\n", ip->harvest);
			lfprintf(ip_level, stream, "num_base_address: %d\n", ip->num_base_address);
			for (int q = 0; q < ip->num_base_address; q++)
				lfprintf(ip_level + 1, stream, "base_address[%d]: 0x%08x\n", q, ip->base_address[q]);


			ip_offset += sizeof(*ip) + 4 * (ip->num_base_address - 1);
		}
	}

	return 0;
}

#define DUMP_GC_INFO_V1_0(field) \
	lfprintf(level, stream, "%-30s:%-8d (0x%08x) \n", #field, le32toh(v1_0->field), le32toh(v1_0->field))
static int umr_dump_discovery_table__gc(struct umr_asic *asic, uint8_t *table, FILE *stream, int level)
{
	struct binary_header *bhdr = (struct binary_header *)table;
	struct table_info *table_info = &bhdr->table_list[GC];
	struct gpu_info_header *ghdr = (struct gpu_info_header *)(table + le32toh(table_info->offset));
	struct gc_info_v1_0 *v1_0;
	uint32_t table_id = le32toh(ghdr->table_id);
	uint32_t version_major, version_minor;

	if (table_id != SIGNATURE('G', 'C', 0, 0)) {
		asic->err_msg("invalid gpu_info_header signature: 0x%08x\n", table_id);
		return -1;
	}

	version_major = le32toh(ghdr->version_major);
	version_minor = le32toh(ghdr->version_minor);

	lfprintf(level, stream, "table_id: 0x%08x\n", le32toh(ghdr->table_id));
	lfprintf(level, stream, "version: %d.%d\n", version_major, version_minor);
	lfprintf(level, stream, "size: %d (0x%x)\n", le32toh(ghdr->size), le32toh(ghdr->size));

	/* only support gc_info_v1_0 */
	if (version_major != 1) {
		lfprintf(level, stream, "Unsupported version: %d.%d\n", version_major, version_minor);
		return 0;
	}

	level++;
	v1_0 = (struct gc_info_v1_0 *)ghdr;
	DUMP_GC_INFO_V1_0(gc_num_se);
	DUMP_GC_INFO_V1_0(gc_num_wgp0_per_sa);
	DUMP_GC_INFO_V1_0(gc_num_wgp0_per_sa);
	DUMP_GC_INFO_V1_0(gc_num_se);
	DUMP_GC_INFO_V1_0(gc_num_wgp0_per_sa);
	DUMP_GC_INFO_V1_0(gc_num_wgp1_per_sa);
	DUMP_GC_INFO_V1_0(gc_num_rb_per_se);
	DUMP_GC_INFO_V1_0(gc_num_gl2c);
	DUMP_GC_INFO_V1_0(gc_num_gprs);
	DUMP_GC_INFO_V1_0(gc_num_max_gs_thds);
	DUMP_GC_INFO_V1_0(gc_gs_table_depth);
	DUMP_GC_INFO_V1_0(gc_gsprim_buff_depth);
	DUMP_GC_INFO_V1_0(gc_parameter_cache_depth);
	DUMP_GC_INFO_V1_0(gc_double_offchip_lds_buffer);
	DUMP_GC_INFO_V1_0(gc_wave_size);
	DUMP_GC_INFO_V1_0(gc_max_waves_per_simd);
	DUMP_GC_INFO_V1_0(gc_max_scratch_slots_per_cu);
	DUMP_GC_INFO_V1_0(gc_lds_size);
	DUMP_GC_INFO_V1_0(gc_num_sc_per_se);
	DUMP_GC_INFO_V1_0(gc_num_sa_per_se);
	DUMP_GC_INFO_V1_0(gc_num_packer_per_sc);
	DUMP_GC_INFO_V1_0(gc_num_gl2a);

	return 0;
}
static int umr_dump_discovery_table__harvest_info(struct umr_asic *asic, uint8_t *table, FILE *stream, int level)
{
	struct binary_header *bhdr = (struct binary_header *)table;
	struct table_info *table_info = &bhdr->table_list[HARVEST_INFO];
	struct harvest_info_header *hhdr = (struct harvest_info_header *)(table + le32toh(table_info->offset));
	struct harvest_table *harvest_table;
	struct harvest_info *hinfo;
	int i;
	uint32_t version = le32toh(hhdr->version);
	uint32_t signature = le32toh(hhdr->signature);

	if (signature != SIGNATURE('H', 'A', 'R', 'V')) {
		asic->err_msg("[ERROR]: invalid harvest_table signature: 0x%08x\n", signature);
		return -1;
	}

	lfprintf(level, stream, "harvest signature: 0x%08x\n", le32toh(hhdr->signature));
	lfprintf(level, stream, "harvest version: 0x%08x\n", le32toh(hhdr->version));

	switch (version) {
	case 0:
	default:
		harvest_table = (struct harvest_table *)hhdr;
		for (i = 0; i < 32; i++) {
			hinfo = &harvest_table->list[i];
			if (!le16toh(hinfo->hw_id))
				break;
			lfprintf(level, stream, "[%02d]: hw_id: 0x%04d\n", i, le16toh(hinfo->hw_id));
			lfprintf(level, stream, "[%02d]: num_instances: 0x%02d\n", i, hinfo->number_instance);
		}
		if (!i)
			lfprintf(level, stream, "no harvest info\n");
		break;
	}

	return 0;
}

int umr_discovery_dump_table(struct umr_asic *asic, uint8_t *table, FILE *stream)
{
	int ret, i;
	struct binary_header *bhdr = (struct binary_header *)table;
	int level = 0;

	if (le32toh(bhdr->binary_signature) != BINARY_SIGNATURE)
		return -1;

	if (!stream)
		stream = stdout;
	lfprintf(level, stream, "AMDGPU DISCOVERY TABLE (%s)\n", asic->asicname);
	level++;
	lfprintf(level, stream, "binary_signature: 0x%08x\n", le32toh(bhdr->binary_signature));
	lfprintf(level, stream, "version: %d.%d\n", le32toh(bhdr->version_major), le32toh(bhdr->version_minor));
	lfprintf(level, stream, "binary_checksum: 0x%08x\n", le32toh(bhdr->binary_checksum));
	lfprintf(level, stream, "binary_size: 0x%08x(%d) bytes\n", le32toh(bhdr->binary_size), le32toh(bhdr->binary_size));

	for (i = 0; i < TOTAL_TABLES; i++) {
		switch (i) {
		case IP_DISCOVERY:
			lfprintf(level, stream, "TABLE: %s (%02d)\n", "IP_DISCOVERY", i);
			ret = umr_dump_discovery_table__ip_discovery(asic, table, stream, level + 1);
			break;
		case GC:
			lfprintf(level, stream, "TABLE: %s (%02d)\n", "GC", i);
			ret = umr_dump_discovery_table__gc(asic, table, stream, level + 1);
			break;
		case HARVEST_INFO:
			lfprintf(level, stream, "TABLE: %s (%02d)\n", "HARVEST_INFO", i);
			ret = umr_dump_discovery_table__harvest_info(asic, table, stream, level + 1);
			break;
		default:
			break;
		}

		if (ret)
			return ret;
	}

	return ret;
}


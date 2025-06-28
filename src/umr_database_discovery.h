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
#ifndef UMR_DATABASE_DISCOVERY_H_
#define UMR_DATABASE_DISCOVERY_H_

// size of serialized umr_discovery_table_entry
#define DET_REC_SIZE (128 + 6 * 2 + 8 * 32)

struct umr_discovery_table_entry {
	char ipname[128];
	int die, instance, maj, min, rev, logical_inst;
	uint64_t segments[32];
	uint8_t harvest;
	struct umr_discovery_table_entry *next;
};

// database
struct umr_database_scan_item {
	char path[256], fname[128], ipname[128];
	int maj, min, rev;
	struct umr_database_scan_item *next;
};

#define UMR_SOC15_MAX_INST 256
#define UMR_SOC15_MAX_SEG 256

struct umr_soc15_database {
	char ipname[64];
	uint64_t off[UMR_SOC15_MAX_INST][UMR_SOC15_MAX_SEG];
	struct umr_soc15_database *next;
};

// vbios
struct umr_vbios_info {
	uint8_t name[64];
	uint8_t vbios_pn[64];
	uint32_t version;
	uint32_t pad;
	uint8_t vbios_ver_str[32];
	uint8_t date[32];
};

struct umr_discovery_table_entry *umr_parse_ip_discovery(int instance, int *nblocks, umr_err_output errout);

FILE *umr_database_open(char *path, char *filename, int binary);
struct umr_database_scan_item *umr_database_scan(char *path);
struct umr_database_scan_item *umr_database_find_ip(
	struct umr_database_scan_item *db,
	char *ipname, int maj, int min, int rev,
	char *desired_path);
void umr_database_free_scan_items(struct umr_database_scan_item *it);

struct umr_soc15_database *umr_database_read_soc15(char *path, char *filename, umr_err_output errout);
struct umr_ip_block *umr_database_read_ipblock(struct umr_soc15_database *soc15, char *path, char *filename, char *cmnname, char *soc15name, int inst, umr_err_output errout);
struct umr_asic *umr_database_read_asic(struct umr_options *options, char *filename, umr_err_output errout);
void umr_database_free_soc15(struct umr_soc15_database *soc15);

int umr_discovery_table_is_supported(struct umr_asic *asic);
int umr_discovery_read_table(struct umr_asic *asic, uint8_t *table, uint32_t *size);
int umr_discovery_verify_table(struct umr_asic *asic, uint8_t *table);
int umr_discovery_dump_table(struct umr_asic *asic, uint8_t *table, FILE *stream);

#endif

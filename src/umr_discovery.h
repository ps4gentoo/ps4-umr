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
#ifndef UMR_DISCOVERY_H_
#define UMR_DISCOVERY_H_

/* ==== ASIC Discovery ====
 * These functions deal with discovering an AMDGPU ASIC in a live/virtual system
 */

// discover based on the options provided (dev name, asicname, instance, etc)
struct umr_asic *umr_discover_asic(struct umr_options *options, umr_err_output errout);
// discover based on the PCI device ID (did)
struct umr_asic *umr_discover_asic_by_did(struct umr_options *options, long did, umr_err_output errout, int *tryipdiscovery);
// discover based on the ASIC name
struct umr_asic *umr_discover_asic_by_name(struct umr_options *options, char *name, umr_err_output errout);
struct umr_asic *umr_discover_asic_by_discovery_table(char *asicname, struct umr_options *options, umr_err_output errout);
void umr_free_asic_blocks(struct umr_asic *asic);
void umr_free_asic(struct umr_asic *asic);
void umr_free_maps(struct umr_asic *asic);
void umr_close_asic(struct umr_asic *asic); // call this to close a fully open asic

int umr_query_drm(struct umr_asic *asic, int field, void *ret, int size);
int umr_query_drm_vbios(struct umr_asic *asic, int field, int type, void *ret, int size);

// query the revision of an IP block based on the constructed name (not from IP discovery specifically)
uint32_t umr_get_ip_revision(struct umr_asic *asic, const char *ipname);

int umr_gfx_get_ip_ver(struct umr_asic *asic, int *maj, int *min);
int umr_sdma_get_ip_ver(struct umr_asic *asic, int *maj, int *min);
int umr_osssys_get_ip_ver(struct umr_asic *asic, int *maj, int *min);

int umr_scan_config(struct umr_asic *asic, int xgmi_scan);
void umr_scan_config_gca_data(struct umr_asic *asic);
void umr_apply_callbacks(struct umr_asic *asic,
			 struct umr_memory_access_funcs *mems,
			 struct umr_register_access_funcs *regs);

#endif

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
#ifndef UMR_WAVES_H_
#define UMR_WAVES_H_

/* ==== WAVE Status ====
 * These functions deal with reading WAVE status data including flags, shaders, etc
 */
int umr_get_wave_status_raw(struct umr_asic *asic, unsigned se, unsigned sh, unsigned cu, unsigned simd, unsigned wave, uint32_t *buf);
int umr_get_wave_status(struct umr_asic *asic, unsigned se, unsigned sh, unsigned cu, unsigned simd, unsigned wave, struct umr_wave_status *ws);
int umr_get_wave_status_via_mmio(struct umr_asic *asic, unsigned se, unsigned sh, unsigned cu, unsigned simd, unsigned wave, struct umr_wave_status *ws);
struct umr_wave_data *umr_scan_wave_data(struct umr_asic *asic);

int umr_wave_data_init(struct umr_asic *asic, struct umr_wave_data *wd);
uint32_t umr_wave_data_get_value(struct umr_asic *asic, struct umr_wave_data *wd, const char *regname);
uint32_t umr_wave_data_get_bits(struct umr_asic *asic, struct umr_wave_data *wd, const char *regname, const char *bitname);
int umr_wave_data_get_bit_info(struct umr_asic *asic, struct umr_wave_data *wd, const char *regname, int *no_bits, struct umr_bitfield **bits);
int umr_wave_data_get_shader_pc_vmid(struct umr_asic *asic, struct umr_wave_data *wd, uint32_t *vmid, uint64_t *addr);
uint32_t umr_wave_data_num_of_sgprs(struct umr_asic *asic, struct umr_wave_data *wd);
char *umr_wave_data_describe_wavefront(struct umr_asic *asic, struct umr_wave_data *wd);
int umr_wave_data_get_flag_valid(struct umr_asic *asic, struct umr_wave_data *wd);
int umr_wave_data_get_flag_trap_en(struct umr_asic *asic, struct umr_wave_data *wd);
int umr_wave_data_get_flag_halt(struct umr_asic *asic, struct umr_wave_data *wd);
int umr_wave_data_get_flag_fatal_halt(struct umr_asic *asic, struct umr_wave_data *wd);
int umr_wave_data_get_flag_priv(struct umr_asic *asic, struct umr_wave_data *wd);
int umr_wave_data_get_flag_wave64(struct umr_asic *asic, struct umr_wave_data *wd);
int umr_wave_data_get_flag_simd_id(struct umr_asic *asic, struct umr_wave_data *wd);
int umr_wave_data_get_flag_wave_id(struct umr_asic *asic, struct umr_wave_data *wd);

int umr_scan_wave_slot(struct umr_asic *asic, uint32_t se, uint32_t sh, uint32_t cu,
		       uint32_t simd, uint32_t wave, struct umr_wave_data *pwd);
int umr_read_wave_status_via_mmio_gfx8_9(struct umr_asic *asic, uint32_t simd, uint32_t wave, uint32_t *dst, int *no_fields);
int umr_read_wave_status_via_mmio_gfx_10_11(struct umr_asic *asic, uint32_t wave, uint32_t *dst, int *no_fields);
int umr_read_wave_status_via_mmio_gfx_12(struct umr_asic *asic, uint32_t wave, uint32_t *dst, int *no_fields);
int umr_parse_wave_data_gfx(struct umr_asic *asic, struct umr_wave_status *ws, const uint32_t *buf, uint32_t nwords);
int umr_get_wave_sq_info_vi(struct umr_asic *asic, unsigned se, unsigned sh, unsigned cu, struct umr_wave_status *ws);
int umr_get_wave_sq_info(struct umr_asic *asic, unsigned se, unsigned sh, unsigned cu, struct umr_wave_status *ws);
int umr_read_sgprs(struct umr_asic *asic, struct umr_wave_data *wd, uint32_t *dst);
int umr_read_vgprs(struct umr_asic *asic, struct umr_wave_data *wd, uint32_t thread, uint32_t *dst);
int umr_read_sgprs_via_mmio(struct umr_asic *asic, struct umr_wave_data *wd, uint32_t *dst);
int umr_read_vgprs_via_mmio(struct umr_asic *asic, struct umr_wave_data *wd, uint32_t thread, uint32_t *dst);
int umr_read_sensor(struct umr_asic *asic, int sensor, void *dst, int *size);

int umr_singlestep_wave(struct umr_asic *asic, struct umr_wave_data *wd);
int umr_linux_read_gpr_gprwave_raw(struct umr_asic *asic, int v_or_s,
								   uint32_t thread, uint32_t se, uint32_t sh, uint32_t cu, uint32_t wave, uint32_t simd,
								   uint32_t offset, uint32_t size, uint32_t *dst);
int umr_get_wave_status_raw(struct umr_asic *asic, unsigned se, unsigned sh, unsigned cu, unsigned simd, unsigned wave, uint32_t *buf);

// halt/resume SQ waves
int umr_sq_cmd_halt_waves(struct umr_asic *asic, enum umr_sq_cmd_halt_resume mode, int max_retries);
int umr_sq_cmd_singlestep(struct umr_asic *asic, uint32_t se, uint32_t sh, uint32_t wgp, uint32_t simd, uint32_t wave);

#endif

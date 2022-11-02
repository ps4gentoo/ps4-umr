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

#define p(x) printf("\t" #x " == %lu\n" , (unsigned long)asic->config. x)
#define px(x) printf("\t" #x " == %08lx\n" , (unsigned long)asic->config. x)
#define plx(x) printf("\t" #x " == %08" PRIx64 "\n" , (uint64_t)asic->config. x)

#define pp(x) printf("\tpci." #x " == %lu\n" , (unsigned long)asic->pci.pdevice-> x)
#define ppx(x) printf("\tpci." #x " == %lx\n" , (unsigned long)asic->pci.pdevice-> x)

#define M(x, y) { ""#x, y },

struct {
	char *name;
	uint64_t mask;
} cg_masks[] = {
M(AMD_CG_SUPPORT_GFX_MGCG, 1UL << 0)
M(AMD_CG_SUPPORT_GFX_MGLS, 1UL << 1)
M(AMD_CG_SUPPORT_GFX_CGCG, 1UL << 2)
M(AMD_CG_SUPPORT_GFX_CGLS, 1UL << 3)
M(AMD_CG_SUPPORT_GFX_CGTS, 1UL << 4)
M(AMD_CG_SUPPORT_GFX_CGTS_LS, 1UL << 5)
M(AMD_CG_SUPPORT_GFX_CP_LS, 1UL << 6)
M(AMD_CG_SUPPORT_GFX_RLC_LS, 1UL << 7)
M(AMD_CG_SUPPORT_MC_LS, 1UL << 8)
M(AMD_CG_SUPPORT_MC_MGCG, 1UL << 9)
M(AMD_CG_SUPPORT_SDMA_LS, 1UL << 10)
M(AMD_CG_SUPPORT_SDMA_MGCG, 1UL << 11)
M(AMD_CG_SUPPORT_BIF_LS, 1UL << 12)
M(AMD_CG_SUPPORT_UVD_MGCG, 1UL << 13)
M(AMD_CG_SUPPORT_VCE_MGCG, 1UL << 14)
M(AMD_CG_SUPPORT_HDP_LS, 1UL << 15)
M(AMD_CG_SUPPORT_HDP_MGCG, 1UL << 16)
M(AMD_CG_SUPPORT_ROM_MGCG, 1UL << 17)
M(AMD_CG_SUPPORT_DRM_LS, 1UL << 18)
M(AMD_CG_SUPPORT_BIF_MGCG, 1UL << 19)
M(AMD_CG_SUPPORT_GFX_3D_CGCG, 1UL << 20)
M(AMD_CG_SUPPORT_GFX_3D_CGLS, 1UL << 21)
M(AMD_CG_SUPPORT_DRM_MGCG, 1UL << 22)
M(AMD_CG_SUPPORT_DF_MGCG, 1UL << 23)
M(AMD_CG_SUPPORT_VCN_MGCG, 1UL << 24)
M(AMD_CG_SUPPORT_HDP_DS, 1UL << 25)
M(AMD_CG_SUPPORT_HDP_SD, 1UL << 26)
M(AMD_CG_SUPPORT_IH_CG, 1UL << 27)
M(AMD_CG_SUPPORT_ATHUB_LS, 1UL << 28)
M(AMD_CG_SUPPORT_ATHUB_MGCG, 1UL << 29)
M(AMD_CG_SUPPORT_JPEG_MGCG, 1UL << 30)
M(AMD_CG_SUPPORT_GFX_FGCG, 1UL << 31)
{ NULL, 0, },
};

struct {
	char *name;
	uint64_t mask;
} pg_masks[] = {
M(AMD_PG_SUPPORT_GFX_PG, 1UL << 0)
M(AMD_PG_SUPPORT_GFX_SMG, 1UL << 1)
M(AMD_PG_SUPPORT_GFX_DMG, 1UL << 2)
M(AMD_PG_SUPPORT_UVD, 1UL << 3)
M(AMD_PG_SUPPORT_VCE, 1UL << 4)
M(AMD_PG_SUPPORT_CP, 1UL << 5)
M(AMD_PG_SUPPORT_GDS, 1UL << 6)
M(AMD_PG_SUPPORT_RLC_SMU_HS, 1UL << 7)
M(AMD_PG_SUPPORT_SDMA, 1UL << 8)
M(AMD_PG_SUPPORT_ACP, 1UL << 9)
M(AMD_PG_SUPPORT_SAMU, 1UL << 10)
M(AMD_PG_SUPPORT_GFX_QUICK_MG, 1UL << 11)
M(AMD_PG_SUPPORT_GFX_PIPELINE, 1UL << 12)
M(AMD_PG_SUPPORT_MMHUB, 1UL << 13)
M(AMD_PG_SUPPORT_VCN, 1UL << 14)
M(AMD_PG_SUPPORT_VCN_DPG, 1UL << 15)
M(AMD_PG_SUPPORT_ATHUB, 1UL << 16)
M(AMD_PG_SUPPORT_JPEG, 1UL << 17)
{ NULL, 0, },
};

static const struct {
	char *name;
	unsigned family_id;
} family[] = {
	{ "Sea Island", 120 },
	{ "Kaveri", 125 },
	{ "Volcanic Islands", 130 },
	{ "Carrizo", 135 },
	{ "Arctic Islands", 141 },
	{ "Raven", 142 },
	{ "Navi", 143 },
	{ NULL, 0 },
};

void umr_print_config(struct umr_asic *asic)
{
	int r, x;

	printf("\tasic.instance == %d\n", asic->instance);
	printf("\tasic.devname == %s\n", asic->options.pci.name);
	printf("\tasic.family == %d\n", (int)asic->family);

	printf("\n\tasic.gtt_size == %llu\n", (unsigned long long)asic->config.gtt_size);
	printf("\tasic.vis_vram_size == %llu\n", (unsigned long long)asic->config.vis_vram_size);
	printf("\tasic.vram_size == %llu\n\n", (unsigned long long)asic->config.vram_size);

	if (asic->options.use_xgmi) {
		printf("\tasic.xgmi_hive_id == %llu\n", (unsigned long long)asic->config.xgmi.hive_id);
		printf("\tasic.xgmi_device_id == %llu\n", (unsigned long long)asic->config.xgmi.device_id);
		for (x = 0; asic->config.xgmi.nodes[x].asic; x++) {
			printf("\tasic.xgmi.node[%d].asicname == %s\n", x, asic->config.xgmi.nodes[x].asic->asicname);
			printf("\tasic.xgmi.node[%d].devname == %s\n", x, asic->config.xgmi.nodes[x].asic->options.pci.name);
			printf("\tasic.xgmi.node[%d].device_id == %llu\n", x, (unsigned long long)asic->config.xgmi.nodes[x].node_id);
			printf("\tasic.xgmi.node[%d].hive_position == %d\n", x, asic->config.xgmi.nodes[x].hive_position);
			printf("\tasic.xgmi.node[%d].vram_mib == %llu\n", x, (unsigned long long)asic->config.xgmi.nodes[x].asic->config.vram_size >> 20ULL);
		}
	}

	printf("\n\tumr.version == %s\n\n", UMR_BUILD_REV);

	printf("\tvbios.version == %s\n\n", asic->config.vbios_version);

	for (r = 0; asic->config.fw[r].name[0]; r++) {
		printf("\tfw.%s == .feature==%lu .firmware==0x%08lx\n",
			asic->config.fw[r].name,
			(unsigned long)asic->config.fw[r].feature_version,
			(unsigned long)asic->config.fw[r].firmware_version);
	}
	printf("\n");

	if (asic->pci.pdevice) {
		ppx(vendor_id);
		ppx(device_id);
		ppx(subvendor_id);
		ppx(subdevice_id);
		ppx(revision);
	}
	printf("\n");

	p(gfx.max_shader_engines);
	p(gfx.max_tile_pipes);
	p(gfx.max_cu_per_sh);
	p(gfx.max_sh_per_se);
	p(gfx.max_backends_per_se);
	p(gfx.max_texture_channel_caches);
	p(gfx.max_gprs);
	p(gfx.max_gs_threads);
	p(gfx.max_hw_contexts);
	p(gfx.sc_prim_fifo_size_frontend);
	p(gfx.sc_prim_fifo_size_backend);
	p(gfx.sc_hiz_tile_fifo_size);
	p(gfx.sc_earlyz_tile_fifo_size);
	p(gfx.num_tile_pipes);
	p(gfx.backend_enable_mask);
	p(gfx.mem_max_burst_length_bytes);
	p(gfx.mem_row_size_in_kb);
	p(gfx.shader_engine_tile_size);
	p(gfx.num_gpus);
	p(gfx.multi_gpu_tile_size);
	p(gfx.mc_arb_ramcfg);
	p(gfx.gb_addr_config);
	p(gfx.num_rbs);

	printf("\tgfx.family = %u", asic->config.gfx.family);
	for (x = 0; family[x].name != NULL; x++)
		if (family[x].family_id == asic->config.gfx.family)
			printf(", %s", family[x].name);
	printf("\n");
	p(is_apu);
	px(gfx.rev_id);
	px(gfx.external_rev_id);
	plx(gfx.cg_flags);
	for (x = 0; cg_masks[x].name; x++)
		if (asic->config.gfx.cg_flags & cg_masks[x].mask)
			printf("\t\t%s\n", cg_masks[x].name);
	plx(gfx.pg_flags);
	for (x = 0; pg_masks[x].name; x++)
		if (asic->config.gfx.pg_flags & pg_masks[x].mask)
			printf("\t\t%s\n", pg_masks[x].name);
	printf("\n");
}

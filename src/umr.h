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
#ifndef UMR_H_
#define UMR_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#if defined(_MSC_VER)
#include <inttypes.h>
#endif
#if defined(__unix__)
	#include <unistd.h>
	#include <sys/types.h>
	#include <sys/stat.h>
	#include <fcntl.h>
	#include <pciaccess.h>
	#include <pthread.h>
#endif

/* SQ_CMD halt/resume */
enum umr_sq_cmd_halt_resume {
	UMR_SQ_CMD_HALT=0,
	UMR_SQ_CMD_RESUME,
};

/* memory space hubs */
enum umr_hub_space {
	UMR_GFX_HUB = 0 << 8,        // default on everything before AI
	UMR_MM_HUB = 1 << 8,         // available on AI and later
	UMR_MM_VC0 = 2 << 8,         // Arcturus VC0
	UMR_MM_VC1 = 3 << 8,         // Arcturus VC1

	UMR_PROCESS_HUB = 0xFD << 8, // process space, allows the use of umr functions on memory buffers inside the process
	UMR_USER_HUB = 0xFE << 8,    // for user supplied HUB names (npi work...)
	UMR_LINEAR_HUB = 0xFF << 8,  // this is for linear access to vram
};

enum umr_shader_type {
	UMR_SHADER_PIXEL = 0,
	UMR_SHADER_VERTEX,
	UMR_SHADER_COMPUTE,
	UMR_SHADER_HS,
	UMR_SHADER_GS,
	UMR_SHADER_ES,
	UMR_SHADER_LS,
	UMR_SHADER_OPAQUE,
};

/* sourced from amd_powerplay.h from the kernel */
enum amd_pp_sensors {
	AMDGPU_PP_SENSOR_GFX_SCLK = 0,
	AMDGPU_PP_SENSOR_CPU_CLK,
	AMDGPU_PP_SENSOR_VDDNB,
	AMDGPU_PP_SENSOR_VDDGFX,
	AMDGPU_PP_SENSOR_UVD_VCLK,
	AMDGPU_PP_SENSOR_UVD_DCLK,
	AMDGPU_PP_SENSOR_VCE_ECCLK,
	AMDGPU_PP_SENSOR_GPU_LOAD,
	AMDGPU_PP_SENSOR_MEM_LOAD,
	AMDGPU_PP_SENSOR_GFX_MCLK,
	AMDGPU_PP_SENSOR_GPU_TEMP,
	AMDGPU_PP_SENSOR_EDGE_TEMP = AMDGPU_PP_SENSOR_GPU_TEMP,
	AMDGPU_PP_SENSOR_HOTSPOT_TEMP,
	AMDGPU_PP_SENSOR_MEM_TEMP,
	AMDGPU_PP_SENSOR_VCE_POWER,
	AMDGPU_PP_SENSOR_UVD_POWER,
	AMDGPU_PP_SENSOR_GPU_POWER,
	AMDGPU_PP_SENSOR_STABLE_PSTATE_SCLK,
	AMDGPU_PP_SENSOR_STABLE_PSTATE_MCLK,
	AMDGPU_PP_SENSOR_ENABLED_SMC_FEATURES_MASK,
	AMDGPU_PP_SENSOR_MIN_FAN_RPM,
	AMDGPU_PP_SENSOR_MAX_FAN_RPM,
	AMDGPU_PP_SENSOR_VCN_POWER_STATE,
};

enum chipfamily {
	FAMILY_SI=0,
	FAMILY_CIK,
	FAMILY_VI,
	FAMILY_AI,
	FAMILY_NV,    // NAVI1X, NAVI2X
	FAMILY_GFX11,
	FAMILY_GFX12,

	FAMILY_NPI, // reserves for new devices that are not public yet
	FAMILY_CONFIGURE,
};

enum regclass {
	REG_MMIO,
	REG_DIDT,
	REG_SMC,
	REG_PCIE,
	REG_SMN,
};

enum UMR_CLOCK_SOURCES {
	UMR_CLOCK_SCLK = 0,
	UMR_CLOCK_MCLK,
	UMR_CLOCK_PCIE,
	UMR_CLOCK_FCLK,
	UMR_CLOCK_SOCCLK,
	UMR_CLOCK_DCEFCLK,
	UMR_CLOCK_MAX,
};

enum UMR_DATABLOCK_ENUM {
	UMR_DATABLOCK_MQD_VI=0,
	UMR_DATABLOCK_MQD_NV
};

struct umr_asic;

struct umr_bitfield {
	/* if regname is NULL the bitfield is considered inactive */
	char *regname;
	/* bit start/stop locations starting from 0 up to 31 */
	unsigned char start, stop;
	/* helper to print bitfield, optional */
	void (*bitfield_print)(struct umr_asic *asic, char *asicname, char *ipname, char *regname, char *bitname, int start, int stop, uint32_t value);
};

struct umr_reg {
	char *regname;
	enum regclass type;
	uint64_t addr;
	struct umr_bitfield *bits;
	int no_bits;
	uint64_t bit64, value;
};

struct umr_reg_soc15 {
	char *regname;
	enum regclass type;
	uint64_t addr, idx;
	struct umr_bitfield *bits;
	int no_bits;
	uint64_t bit64, value;
};

struct umr_find_reg_iter {
	struct umr_asic *asic;
	char *ip, *reg;
	int ip_i, reg_i;
};

struct umr_ip_block {
	char *ipname;
	int no_regs;
	struct umr_reg *regs;
	struct {
          int die, maj, min, rev, instance, logical_inst;
    } discoverable;
};

struct umr_find_reg_iter_result {
	struct umr_ip_block *ip;
	struct umr_reg *reg;
};

struct umr_ip_offsets_soc15 {
	char *name;
	uint32_t offset[5][5];
};

struct umr_gfx_config {
	unsigned max_shader_engines;
	unsigned max_tile_pipes;
	unsigned max_cu_per_sh;
	unsigned max_sh_per_se;
	unsigned max_backends_per_se;
	unsigned max_texture_channel_caches;
	unsigned max_gprs;
	unsigned max_gs_threads;
	unsigned max_hw_contexts;
	unsigned sc_prim_fifo_size_frontend;
	unsigned sc_prim_fifo_size_backend;
	unsigned sc_hiz_tile_fifo_size;
	unsigned sc_earlyz_tile_fifo_size;

	unsigned num_tile_pipes;
	unsigned backend_enable_mask;
	unsigned mem_max_burst_length_bytes;
	unsigned mem_row_size_in_kb;
	unsigned shader_engine_tile_size;
	unsigned num_gpus;
	unsigned multi_gpu_tile_size;
	unsigned mc_arb_ramcfg;
	unsigned gb_addr_config;
	unsigned num_rbs;

	unsigned rev_id;
	uint64_t cg_flags;
	uint64_t pg_flags;

	unsigned family;
	unsigned external_rev_id;
};

struct umr_pci_config {
	unsigned device;
	unsigned revision;
	unsigned subsystem_device;
	unsigned subsystem_vendor;
};

struct umr_fw_config {
	char name[16];
	uint32_t feature_version,
		 firmware_version;
};

#define UMR_MAX_FW 32
#define UMR_MAX_XGMI_DEVICES 128

#define NUM_HBM_INSTANCES 4

// struct for sysram and vram blocks
struct umr_test_harness_ram_blocks {
	uint64_t base_address; // base address in bytes
	uint32_t size;         // size in bytes
	uint8_t *contents;
	struct umr_test_harness_ram_blocks *next;
};

struct umr_test_harness_mmio_blocks {
	uint64_t mmio_address;  // dword address
	uint32_t *values;       // values for this register
	uint32_t no_values;     // number of values slotted in this spot
	uint32_t cur_slot;      // index to current value to return
	struct umr_test_harness_mmio_blocks *next;
};

struct umr_test_harness_sq_blocks {
	uint32_t sq_address;    // value for SQ_IND_INDEX
	uint32_t *values;       // values for this register
	uint32_t no_values;     // number of values slotted in this spot
	uint32_t cur_slot;      // index to current value to return
	struct umr_test_harness_sq_blocks *next;
};

struct umr_test_harness {
	struct umr_asic *asic;

	struct umr_test_harness_ram_blocks vram, sysram, config, discovery;
	struct umr_test_harness_mmio_blocks mmio, ws, vgpr, sgpr, wave, ring;
	struct umr_test_harness_sq_blocks sq;

	uint64_t vram_mm_index; // when these are written they are shadowed here
	uint32_t sq_ind_index;
};

struct umr_options {
	int forced_instance,
		instance,
	    bitfields,
	    bitfields_full,
	    empty_log,
	    use_bank,
	    many,
	    use_pci,
	    use_colour,
	    read_smc,
	    quiet,
	    no_follow_ib,
	    no_follow_shader,
	    no_follow_loadx,
	    verbose,
	    halt_waves,
	    no_kernel,
	    no_disasm,
	    disasm_early_term,
	    use_xgmi,
	    disasm_anyways,
	    skip_gprs,
	    wave64,
	    full_shader,
	    context_reg_bank,
	    no_fold_vm_decode,
	    pg_lock,
	    test_log,
	    vm_partition,
	    is_virtual,
	    force_asic_file,
	    export_model,
	    vgpr_granularity,
	    use_v1_regs_debugfs,
	    trap_unsorted_db;

	// hs/gs shaders can be opaque depending on circumstances on gfx9+ platforms
	struct {
		int
			enable_ps_shader,
			enable_vs_shader,
			enable_gs_shader,
			enable_hs_shader,
			enable_es_shader,
			enable_ls_shader,
			enable_es_ls_swap,
			enable_comp_shader;
	} shader_enable;

	union {
		struct {
			uint32_t
				instance,
				se,
				sh;
		} grbm;
		struct {
			uint32_t
				me,
				queue,
				pipe,
				vmid;
		} srbm;
	} bank;

	long forcedid;
	char
		*scanblock,
		dev_name[64],
		hub_name[32],
		ring_name[32],
		desired_path[256],
		database_path[256];
	struct {
		unsigned domain,
		    bus,
		    slot,
		    func;
		char name[32];
	} pci;

	FILE *test_log_fd;
	struct umr_test_harness *th;
};

// Page Directory Entry for VM page walking
typedef struct {
	uint64_t
		frag_size,
		pte_base_addr,
		valid,
		system,
		coherent,
		pte,
		further,
		tfs_addr,
		llc_noalloc,
		mtype,
		pa_rsvd,
		mall_reuse;
} pde_fields_t;

// Page Table Entry for VM page walking
typedef struct {
	uint64_t
		valid,
		system,
		coherent,
		tmz,
		execute,
		read,
		write,
		fragment,
		page_base_addr,
		prt,
		pde,
		further,
		mtype,
		pte_mask,
		gcr,
		llc_noalloc,
		software,
		pa_rsvd,
		dcc,
		pte;
} pte_fields_t;

struct umr_memory_access_funcs {
	/** access_sram -- Access System RAM
	 * @asic:  The device the memory is bound to
	 * @address: The address relative to the GPUs bus (might not be a physical system memory access)
	 * @size: Number of bytes
	 * @dst: Buffer to read/write
	 * @write_en: true for write, false for read
	 */
	int (*access_sram)(struct umr_asic *asic, uint64_t address, uint32_t size, void *dst, int write_en);

	/** access_linear_vram -- Access Video RAM
	 * @asic:  The device the memory is bound to
	 * @address: The address relative to the GPUs start of VRAM (or relative to the start of an XGMI map)
	 * @size: Number of bytes
	 * @dst: Buffer to read/write
	 * @write_en: true for write, false for read
	 */
	int (*access_linear_vram)(struct umr_asic *asic, uint64_t address, uint32_t size, void *data, int write_en);

	/** gpu_bus_to_cpu_address -- convert a GPU bound address for
	 * 							  system memory pages to CPU bound
	 * 							  addresses
	 * @asic: The device the memory is bound to
	 * @dma_addr: The GPU bound address
	 *
	 * Returns: The address the CPU can use to access the memory in
	 * system memory
	 */
	uint64_t (*gpu_bus_to_cpu_address)(struct umr_asic *asic, uint64_t dma_addr);

	/** vm_message -- Display a VM decoding message
	 *
	 * @fmt:  The format string
	 * @...:  Parameters to print
	 */
	int (*vm_message)(const char *fmt, ...);

	void (*va_addr_decode)(pde_fields_t *pdes, int num_pde, pte_fields_t pte);

	/** data -- opaque pointer the callbacks can use for state tracking */
	void *data;
};

struct umr_register_access_funcs {
	/** read_reg -- Read a register
	 * @asic: The device the register is from
	 * @addr:  The byte address of the register to read
	 * @type:  REG_MMIO or REG_SMC
	 */
	uint32_t (*read_reg)(struct umr_asic *asic, uint64_t addr, enum regclass type);

	/** write_reg -- Write a register
	 * @asic: The device the register is from
	 * @addr: The byte address of the register to write
	 * @value: The 32-bit value to write
	 * @type: REG_MMIO or REG_SMC
	 */
	int (*write_reg)(struct umr_asic *asic, uint64_t addr, uint32_t value, enum regclass type);

	/** data -- opaque pointer the callbacks can use for state tracking */
	void *data;
};

struct umr_wave_status;
struct umr_wave_access_funcs {
	/** get_wave_status -- Populate the umr_wave_status structure
	 * @asic: The device the SQ_WAVE data should come from
	 * @se, @sh, @cu, @simd, @wave: The specific wave to read data from
	 * @ws: where to store the SQ_WAVE_* decoded data (see src/lib/lowlevel/linux/wave_status.c for an example)
	 */
	int (*get_wave_status)(struct umr_asic *asic, unsigned se, unsigned sh, unsigned cu, unsigned simd, unsigned wave, struct umr_wave_status *ws);

	/** get_wave_sq_info -- Populate the sq_info sub-structure of the umr_wave_status structure
	 * @asic: The device to get SQ information from
	 * @se, @sh, @cu: Which engine to read
	 * @ws: Where to store the sq information.
	 */
	int (*get_wave_sq_info)(struct umr_asic *asic, unsigned se, unsigned sh, unsigned cu, struct umr_wave_status *ws);

	/** data -- opaque pointer the callbacks can use for state tracking */
	void *data;
};

struct umr_shader_disasm_funcs {
	/** disasm -- Disassemble a block of shader text into human readable text
	 * @asic: The device the shader text is for
	 * @inst: pointer to array of shader text
	 * @inst_bytes: number of bytes (should be multiple of 4)
	 * @PC: The current PC value for the wave
	 * @disasm_text: Pointer to an array of char arrays that will contain the human readable output.
	 *
	 * Each entry of **disasm_text and *disasm_text itself must be freed with free().
	 */
	int (*disasm)(struct umr_asic *asic,
						  uint8_t *inst, unsigned inst_bytes,
						  uint64_t PC,
						  char ***disasm_text);

	void *data;
};

// This captures wave data for a specific engine/wave/etc
struct umr_wave_status {
	struct {
		uint32_t
			busy,
			wave_level;
	} sq_info;

	uint32_t reg_values[64];
};

// This captures *all* active/halted waves
struct umr_wave_data {
	uint32_t vgprs[64 * 256], sgprs[1024], num_threads;
	int se, sh, cu, simd, wave, have_vgprs;
	const char **reg_names;
	struct umr_wave_status ws;
	struct umr_wave_thread *threads;
	struct umr_wave_data *next;
};

struct umr_read_gpr_funcs {
	void *data;

	/** read_vgprs -- Read VGPR data for a given wave and thread */
	int (*read_vgprs)(struct umr_asic *asic, struct umr_wave_data *wd, uint32_t thread, uint32_t *dst);

	/** read_sgprs -- Read VGPR data for a given wave */
	int (*read_sgprs)(struct umr_asic *asic, struct umr_wave_data *wd, uint32_t *dst);
};

struct umr_read_ring_func {
	void *data;

	void *(*read_ring_data)(struct umr_asic *asic, char *ringname, uint32_t *ringsize);
};

// contains info about a node in an XGMI hive
struct umr_xgmi_hive_info {
	uint64_t node_id;
	int instance, hive_position;
	struct umr_asic *asic;
};

struct umr_mmio_accel_data {
	uint64_t mmio_addr;
	uint32_t ord;
	struct umr_ip_block *ip;
	struct umr_reg *reg;
};

struct umr_asic {
	char *asicname;
	int no_blocks;
	int instance;
	enum chipfamily family;
	int is_apu;
	int was_ip_discovered;
	unsigned did;
	struct umr_ip_block **blocks;
	struct {
		unsigned vgpr_granularity;
	} parameters;
	struct {
		struct umr_gfx_config gfx;
		struct umr_fw_config fw[UMR_MAX_FW];
		struct umr_pci_config pci;
		int is_apu;
		char vbios_version[128];
		uint64_t vram_size,
			 vis_vram_size,
			 gtt_size;

		struct {
			uint64_t
				device_id,
				hive_id;
			int callbacks_applied;
			struct umr_xgmi_hive_info nodes[UMR_MAX_XGMI_DEVICES];
		} xgmi;
		uint32_t data[512];
	} config;
	struct {
		int mmio,
		    mmio2,
		    didt,
		    pcie,
		    smc,
		    sensors,
		    drm,
		    vram,
		    gprwave,
		    gpr,
		    wave,
		    iova,
		    iomem,
		    gfxoff;
	} fd;
	struct {
		uint64_t sq_ind_index;
	} test_harness;
	struct {
		struct pci_device *pdevice;
		uint32_t *mem; // virtual address
		int region;
	} pci;
	struct umr_options options;
	struct umr_dma_maps *maps;
	struct umr_memory_access_funcs mem_funcs;
	struct umr_register_access_funcs reg_funcs;
	struct umr_wave_access_funcs wave_funcs;
	struct umr_shader_disasm_funcs shader_disasm_funcs;
	struct umr_read_gpr_funcs gpr_read_funcs;
	struct umr_mmio_accel_data *mmio_accel;
	struct umr_read_ring_func ring_func;
	uint32_t mmio_accel_size;
	int (*err_msg)(const char *fmt, ...);
	int (*std_msg)(const char *fmt, ...);
};

typedef	int (*umr_err_output)(const char *, ...);

// contains information about a compute/gfx shader program
struct umr_shaders_pgm {
	// VMID and length in bytes
	uint32_t
		vmid,
		size,
		rsrc1,
		rsrc2;

	// shader type (0==PS, 1==VS, 2==COMPUTE)
	int
		type;

	// address in VM space for this shader
	uint64_t addr;

	struct umr_shaders_pgm *next;

	struct {
		uint64_t ib_base, ib_offset;
	} src;
};

// incs here
#include <umr_metrics.h>
#include <umr_discovery.h>
#include <umr_waves.h>
#include <umr_mmio.h>
#include <umr_packet.h>
#include <umr_ih.h>
#include <umr_vm.h>
#include <umr_test_harness.h>
#include <umr_clock.h>
#include <umr_database_discovery.h>

#define RED     (asic->options.use_colour ? "\x1b[31;1m" : "")
#define GREEN   (asic->options.use_colour ? "\x1b[32;1m" : "")
#define YELLOW  (asic->options.use_colour ? "\x1b[33;1m" : "")
#define BLUE    (asic->options.use_colour ? "\x1b[34;1m" : "")
#define MAGENTA (asic->options.use_colour ? "\x1b[35;1m" : "")
#define CYAN    (asic->options.use_colour ? "\x1b[36;1m" : "")
#define WHITE   (asic->options.use_colour ? "\x1b[37;1m" : "")
#define BRED     (asic->options.use_colour ? "\x1b[91;1m" : "")
#define BGREEN   (asic->options.use_colour ? "\x1b[92;1m" : "")
#define BYELLOW  (asic->options.use_colour ? "\x1b[93;1m" : "")
#define BBLUE    (asic->options.use_colour ? "\x1b[94;1m" : "")
#define BMAGENTA (asic->options.use_colour ? "\x1b[95;1m" : "")
#define BCYAN    (asic->options.use_colour ? "\x1b[96;1m" : "")
#define BWHITE   (asic->options.use_colour ? "\x1b[97;1m" : "")
#define RST     (asic->options.use_colour ? "\x1b[0m" : "")

void umr_bitfield_default(struct umr_asic *asic, char *asicname, char *ipname, char *regname, char *bitname, int start, int stop, uint32_t value);

#if UMR_SERVER
#include "parson.h"
JSON_Value *umr_process_json_request(JSON_Object *request, void **raw_data, unsigned *raw_data_size);
void umr_run_gui(const char *url);
#endif

int umr_enumerate_device_list(umr_err_output errout, const char *database_path, struct umr_options *global_options, struct umr_asic ***asics, int *no_asics, int xgmi_scan);
void umr_enumerate_device_list_free(struct umr_asic **asics);

#endif

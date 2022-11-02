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
	FAMILY_NV, // NAVI10 and up

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
		int die, maj, min, rev, instance;
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
#define UMR_MAX_XGMI_DEVICES 32

#define NUM_HBM_INSTANCES 4

// struct for sysram and vram blocks
struct umr_ram_blocks {
	uint64_t base_address; // base address in bytes
	uint32_t size;         // size in bytes
	uint8_t *contents;
	struct umr_ram_blocks *next;
};

struct umr_mmio_blocks {
	uint64_t mmio_address;  // dword address
	uint32_t *values;       // values for this register
	uint32_t no_values;     // number of values slotted in this spot
	uint32_t cur_slot;      // index to current value to return
	struct umr_mmio_blocks *next;
};

struct umr_sq_blocks {
	uint32_t sq_address;    // value for SQ_IND_INDEX
	uint32_t *values;       // values for this register
	uint32_t no_values;     // number of values slotted in this spot
	uint32_t cur_slot;      // index to current value to return
	struct umr_sq_blocks *next;
};

struct umr_test_harness {
	struct umr_asic *asic;

	struct umr_ram_blocks vram, sysram, config, discovery;
	struct umr_mmio_blocks mmio, ws, vgpr, sgpr, wave, ring;
	struct umr_sq_blocks sq;

	uint64_t vram_mm_index; // when these are written they are shadowed here
	uint32_t sq_ind_index;
};

struct umr_options {
	int instance,
	    need_scan,
	    print,
	    bitfields,
	    bitfields_full,
	    empty_log,
	    follow,
	    use_bank,
	    many,
	    use_pci,
	    use_colour,
	    read_smc,
	    quiet,
	    no_follow_ib,
	    no_follow_shader,
	    verbose,
	    halt_waves,
	    no_kernel,
	    no_disasm,
	    disasm_early_term,
	    use_xgmi,
	    disasm_anyways,
	    skip_gprs,
	    no_scan_waves,
	    wave64,
	    full_shader,
	    context_reg_bank,
	    no_fold_vm_decode,
	    pg_lock,
	    test_log,
	    vm_partition,
	    is_virtual,
	    force_asic_file,
	    export_model;

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
		mtype;
} pde_fields_t;

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
		software;
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

struct umr_read_gpr_funcs {
	/** read_vgprs -- Read VGPR data for a given wave and thread */
	int (*read_vgprs)(struct umr_asic *asic, struct umr_wave_status *ws, uint32_t thread, uint32_t *dst);

	/** read_sgprs -- Read VGPR data for a given wave */
	int (*read_sgprs)(struct umr_asic *asic, struct umr_wave_status *ws, uint32_t *dst);
};

struct umr_hive_info {
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
			struct umr_hive_info nodes[UMR_MAX_XGMI_DEVICES];
		} xgmi;
	} config;
	struct {
		int mmio,
		    mmio2,
		    didt,
		    pcie,
		    smc,
		    sensors,
		    drm,
		    wave,
		    vram,
		    gpr,
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
	uint32_t mmio_accel_size;
	int (*err_msg)(const char *fmt, ...);
	int (*std_msg)(const char *fmt, ...);
};

typedef	int (*umr_err_output)(const char *, ...);


struct umr_wave_status {
	struct {
		uint32_t
			busy,
			wave_level;
	} sq_info;

	struct {
		uint32_t
			value,
			priv,
			scc,
			execz,
			vccz,
			in_tg,
			halt,
			valid,
			spi_prio,
			wave_prio,
			trap_en,
			ttrace_en,
			export_rdy,
			in_barrier,
			trap,
			ecc_err,
			skip_export,
			perf_en,
			cond_dbg_user,
			cond_dbg_sys,
			allow_replay,
			fatal_halt,
			data_atc,
			inst_atc,
			dispatch_cache_ctrl,
			must_export,
			ttrace_simd_en;
	} wave_status;

	uint32_t
		pc_lo,
		pc_hi,
		exec_lo,
		exec_hi,
		wave_inst_dw0,
		wave_inst_dw1,
		tba_lo,
		tba_hi,
		tma_lo,
		tma_hi,
		ib_dbg0,
		ib_dbg1,
		m0;

	struct {
		uint32_t
			value,
			wave_id,
			simd_id,
			pipe_id,
			cu_id,
			sh_id,
			se_id,
			tg_id,
			vm_id,
			queue_id,
			state_id,
			me_id;
	} hw_id;

	struct {
			uint32_t
					value,
					wave_id,
					simd_id,
					wgp_id,
					sa_id,
					se_id;
	} hw_id1;

	struct {
			uint32_t
					value,
					queue_id,
					pipe_id,
					me_id,
					state_id,
					wg_id,
					vm_id,
					compat_level;
	} hw_id2;

	struct {
		uint32_t
			value,
			vgpr_base,
			vgpr_size,
			sgpr_base,
			sgpr_size;
	} gpr_alloc;

	struct {
		uint32_t
			value,
			lds_base,
			lds_size,
			vgpr_shared_size;
	} lds_alloc;

	struct {
		uint32_t
			value,
			vm_cnt,
			exp_cnt,
			lgkm_cnt,
			valu_cnt,
			vs_cnt,
			replay_w64h;
	} ib_sts;

	struct {
			uint32_t
					value,
					inst_prefetch,
					resource_override,
					mem_order,
					fwd_progress,
					wave64,
					wave64hi,
					subv_loop;
	} ib_sts2;

	struct {
		uint32_t
			value,
			excp,
			excp_cycle,
			dp_rate,
			savectx,
			illegal_inst,
			excp_hi,
			excp_wave64hi,
			xnack_error,
			buffer_oob,
			excp_group_mask,
			utc_error;
	} trapsts;

	struct {
		uint32_t
			value,
			fp_round,
			fp_denorm,
			dx10_clamp,
			ieee,
			lod_clamped,
			debug_en,
			excp_en,
			fp16_ovfl,
			pops_packer0,
			pops_packer1,
			disable_perf,
			gpr_idx_en,
			vskip,
			csp;
	} mode;
};

struct umr_wave_data {
	uint32_t vgprs[64 * 256], sgprs[1024], num_threads;
	int se, sh, cu, simd, wave, have_vgprs;
	struct umr_wave_status ws;
	struct umr_wave_thread *threads;
	struct umr_wave_data *next;
};

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

struct umr_pm4_data_block {
	uint32_t vmid, extra;
	uint64_t addr;
	enum UMR_DATABLOCK_ENUM type;
	struct umr_pm4_data_block *next;
};

struct umr_ring_decoder {
	// type of ring (4==PM4, 3==SDMA)
	int
		pm;

	// source of this IB
	struct {
		uint64_t
			addr,
			vmid,
			ib_addr;
	} src; 

	// working state for the PM4 decoder
	struct {
		uint32_t
			cur_opcode,
			pkt_type,
			n_words,
			cur_word,
			control;

		struct {
			uint32_t ib_addr_lo,
				 ib_addr_hi,
				 ib_size,
				 ib_vmid;
			int tally;
		} next_ib_state;

		struct {
			uint32_t type,
				 addr_lo,
				 addr_hi,
				 value;
		} next_write_mem;

		// structure to keep track of PKT3_NOP comment data
		struct {
			uint32_t
				pktlen,
				pkttype,
				magic;
			char *str;
		} nop;

	} pm4;

	// working state of the SDMA decoder
	struct {
		uint32_t
			cur_opcode,
			cur_sub_opcode,
			n_words,
			cur_word,
			header_dw,
			next_write_mem;

		struct {
			uint32_t
				ib_addr_lo,
				ib_addr_hi,
				csa_addr_lo,
				csa_addr_hi,
				ib_size,
				ib_vmid;
		} next_ib_state;
	} sdma;

	// pointer to next IB to decode
	struct umr_ring_decoder *next_ib;

	// only used by tail end of ring_read ...
	struct {
		uint64_t
			ib_addr,
			vm_base_addr; // not used yet (will be used by IB parser...)
		uint32_t vmid,
			 size,
			 addr;
	} next_ib_info;

	// count shaders in the IB
	struct umr_shaders_pgm *shader;

	// count data blocks in IB
	struct umr_pm4_data_block *datablock;
};

struct umr_metrics_table_header {
	uint16_t			structure_size;
	uint8_t				format_revision;
	uint8_t				content_revision;
};

struct umr_gpu_metrics_v1_0 {
	struct umr_metrics_table_header	common_header;

	/* Driver attached timestamp (in ns) */
	uint64_t			system_clock_counter;

	/* Temperature */
	uint16_t			temperature_edge;
	uint16_t			temperature_hotspot;
	uint16_t			temperature_mem;
	uint16_t			temperature_vrgfx;
	uint16_t			temperature_vrsoc;
	uint16_t			temperature_vrmem;

	/* Utilization */
	uint16_t			average_gfx_activity;
	uint16_t			average_umc_activity; // memory controller
	uint16_t			average_mm_activity; // UVD or VCN

	/* Power/Energy */
	uint16_t			average_socket_power;
	uint32_t			energy_accumulator;

	/* Average clocks */
	uint16_t			average_gfxclk_frequency;
	uint16_t			average_socclk_frequency;
	uint16_t			average_uclk_frequency;
	uint16_t			average_vclk0_frequency;
	uint16_t			average_dclk0_frequency;
	uint16_t			average_vclk1_frequency;
	uint16_t			average_dclk1_frequency;

	/* Current clocks */
	uint16_t			current_gfxclk;
	uint16_t			current_socclk;
	uint16_t			current_uclk;
	uint16_t			current_vclk0;
	uint16_t			current_dclk0;
	uint16_t			current_vclk1;
	uint16_t			current_dclk1;

	/* Throttle status */
	uint32_t			throttle_status;

	/* Fans */
	uint16_t			current_fan_speed;

	/* Link width/speed */
	uint8_t				pcie_link_width;
	uint8_t				pcie_link_speed; // in 0.1 GT/s
};

struct umr_gpu_metrics_v1_1 {
	struct umr_metrics_table_header	common_header;

	/* Temperature */
	uint16_t			temperature_edge;
	uint16_t			temperature_hotspot;
	uint16_t			temperature_mem;
	uint16_t			temperature_vrgfx;
	uint16_t			temperature_vrsoc;
	uint16_t			temperature_vrmem;

	/* Utilization */
	uint16_t			average_gfx_activity;
	uint16_t			average_umc_activity; // memory controller
	uint16_t			average_mm_activity; // UVD or VCN

	/* Power/Energy */
	uint16_t			average_socket_power;
	uint64_t			energy_accumulator;

	/* Driver attached timestamp (in ns) */
	uint64_t			system_clock_counter;

	/* Average clocks */
	uint16_t			average_gfxclk_frequency;
	uint16_t			average_socclk_frequency;
	uint16_t			average_uclk_frequency;
	uint16_t			average_vclk0_frequency;
	uint16_t			average_dclk0_frequency;
	uint16_t			average_vclk1_frequency;
	uint16_t			average_dclk1_frequency;

	/* Current clocks */
	uint16_t			current_gfxclk;
	uint16_t			current_socclk;
	uint16_t			current_uclk;
	uint16_t			current_vclk0;
	uint16_t			current_dclk0;
	uint16_t			current_vclk1;
	uint16_t			current_dclk1;

	/* Throttle status */
	uint32_t			throttle_status;

	/* Fans */
	uint16_t			current_fan_speed;

	/* Link width/speed */
	uint16_t			pcie_link_width;
	uint16_t			pcie_link_speed; // in 0.1 GT/s

	uint16_t			padding;

	uint32_t			gfx_activity_acc;
	uint32_t			mem_activity_acc;

	uint16_t			temperature_hbm[NUM_HBM_INSTANCES];
};

struct umr_gpu_metrics_v1_2 {
	struct umr_metrics_table_header	common_header;

	/* Temperature */
	uint16_t			temperature_edge;
	uint16_t			temperature_hotspot;
	uint16_t			temperature_mem;
	uint16_t			temperature_vrgfx;
	uint16_t			temperature_vrsoc;
	uint16_t			temperature_vrmem;

	/* Utilization */
	uint16_t			average_gfx_activity;
	uint16_t			average_umc_activity; // memory controller
	uint16_t			average_mm_activity; // UVD or VCN

	/* Power/Energy */
	uint16_t			average_socket_power;
	uint64_t			energy_accumulator;

	/* Driver attached timestamp (in ns) */
	uint64_t			system_clock_counter;

	/* Average clocks */
	uint16_t			average_gfxclk_frequency;
	uint16_t			average_socclk_frequency;
	uint16_t			average_uclk_frequency;
	uint16_t			average_vclk0_frequency;
	uint16_t			average_dclk0_frequency;
	uint16_t			average_vclk1_frequency;
	uint16_t			average_dclk1_frequency;

	/* Current clocks */
	uint16_t			current_gfxclk;
	uint16_t			current_socclk;
	uint16_t			current_uclk;
	uint16_t			current_vclk0;
	uint16_t			current_dclk0;
	uint16_t			current_vclk1;
	uint16_t			current_dclk1;

	/* Throttle status (ASIC dependent) */
	uint32_t			throttle_status;

	/* Fans */
	uint16_t			current_fan_speed;

	/* Link width/speed */
	uint16_t			pcie_link_width;
	uint16_t			pcie_link_speed; // in 0.1 GT/s

	uint16_t			padding;

	uint32_t			gfx_activity_acc;
	uint32_t			mem_activity_acc;

	uint16_t			temperature_hbm[NUM_HBM_INSTANCES];

	/* PMFW attached timestamp (10ns resolution) */
	uint64_t			firmware_timestamp;
};

struct umr_gpu_metrics_v1_3 {
	struct umr_metrics_table_header	common_header;

	/* Temperature */
	uint16_t			temperature_edge;
	uint16_t			temperature_hotspot;
	uint16_t			temperature_mem;
	uint16_t			temperature_vrgfx;
	uint16_t			temperature_vrsoc;
	uint16_t			temperature_vrmem;

	/* Utilization */
	uint16_t			average_gfx_activity;
	uint16_t			average_umc_activity; // memory controller
	uint16_t			average_mm_activity; // UVD or VCN

	/* Power/Energy */
	uint16_t			average_socket_power;
	uint64_t			energy_accumulator;

	/* Driver attached timestamp (in ns) */
	uint64_t			system_clock_counter;

	/* Average clocks */
	uint16_t			average_gfxclk_frequency;
	uint16_t			average_socclk_frequency;
	uint16_t			average_uclk_frequency;
	uint16_t			average_vclk0_frequency;
	uint16_t			average_dclk0_frequency;
	uint16_t			average_vclk1_frequency;
	uint16_t			average_dclk1_frequency;

	/* Current clocks */
	uint16_t			current_gfxclk;
	uint16_t			current_socclk;
	uint16_t			current_uclk;
	uint16_t			current_vclk0;
	uint16_t			current_dclk0;
	uint16_t			current_vclk1;
	uint16_t			current_dclk1;

	/* Throttle status */
	uint32_t			throttle_status;

	/* Fans */
	uint16_t			current_fan_speed;

	/* Link width/speed */
	uint16_t			pcie_link_width;
	uint16_t			pcie_link_speed; // in 0.1 GT/s

	uint16_t			padding;

	uint32_t			gfx_activity_acc;
	uint32_t			mem_activity_acc;

	uint16_t			temperature_hbm[NUM_HBM_INSTANCES];

	/* PMFW attached timestamp (10ns resolution) */
	uint64_t			firmware_timestamp;

	/* Voltage (mV) */
	uint16_t			voltage_soc;
	uint16_t			voltage_gfx;
	uint16_t			voltage_mem;

	uint16_t			padding1;

	/* Throttle status (ASIC independent) */
	uint64_t			indep_throttle_status;
};

struct umr_gpu_metrics_v2_0 {
	struct umr_metrics_table_header	common_header;

	/* Driver attached timestamp (in ns) */
	uint64_t			system_clock_counter;

	/* Temperature */
	uint16_t			temperature_gfx; // gfx temperature on APUs
	uint16_t			temperature_soc; // soc temperature on APUs
	uint16_t			temperature_core[8]; // CPU core temperature on APUs
	uint16_t			temperature_l3[2];

	/* Utilization */
	uint16_t			average_gfx_activity;
	uint16_t			average_mm_activity; // UVD or VCN

	/* Power/Energy */
	uint16_t			average_socket_power; // dGPU + APU power on A + A platform
	uint16_t			average_cpu_power;
	uint16_t			average_soc_power;
	uint16_t			average_gfx_power;
	uint16_t			average_core_power[8]; // CPU core power on APUs

	/* Average clocks */
	uint16_t			average_gfxclk_frequency;
	uint16_t			average_socclk_frequency;
	uint16_t			average_uclk_frequency;
	uint16_t			average_fclk_frequency;
	uint16_t			average_vclk_frequency;
	uint16_t			average_dclk_frequency;

	/* Current clocks */
	uint16_t			current_gfxclk;
	uint16_t			current_socclk;
	uint16_t			current_uclk;
	uint16_t			current_fclk;
	uint16_t			current_vclk;
	uint16_t			current_dclk;
	uint16_t			current_coreclk[8]; // CPU core clocks
	uint16_t			current_l3clk[2];

	/* Throttle status */
	uint32_t			throttle_status;

	/* Fans */
	uint16_t			fan_pwm;

	uint16_t			padding;
};

struct umr_gpu_metrics_v2_1 {
	struct umr_metrics_table_header	common_header;

	/* Temperature */
	uint16_t			temperature_gfx; // gfx temperature on APUs
	uint16_t			temperature_soc; // soc temperature on APUs
	uint16_t			temperature_core[8]; // CPU core temperature on APUs
	uint16_t			temperature_l3[2];

	/* Utilization */
	uint16_t			average_gfx_activity;
	uint16_t			average_mm_activity; // UVD or VCN

	/* Driver attached timestamp (in ns) */
	uint64_t			system_clock_counter;

	/* Power/Energy */
	uint16_t			average_socket_power; // dGPU + APU power on A + A platform
	uint16_t			average_cpu_power;
	uint16_t			average_soc_power;
	uint16_t			average_gfx_power;
	uint16_t			average_core_power[8]; // CPU core power on APUs

	/* Average clocks */
	uint16_t			average_gfxclk_frequency;
	uint16_t			average_socclk_frequency;
	uint16_t			average_uclk_frequency;
	uint16_t			average_fclk_frequency;
	uint16_t			average_vclk_frequency;
	uint16_t			average_dclk_frequency;

	/* Current clocks */
	uint16_t			current_gfxclk;
	uint16_t			current_socclk;
	uint16_t			current_uclk;
	uint16_t			current_fclk;
	uint16_t			current_vclk;
	uint16_t			current_dclk;
	uint16_t			current_coreclk[8]; // CPU core clocks
	uint16_t			current_l3clk[2];

	/* Throttle status */
	uint32_t			throttle_status;

	/* Fans */
	uint16_t			fan_pwm;

	uint16_t			padding[3];
};

struct umr_gpu_metrics_v2_2 {
	struct umr_metrics_table_header	common_header;

	/* Temperature */
	uint16_t			temperature_gfx; // gfx temperature on APUs
	uint16_t			temperature_soc; // soc temperature on APUs
	uint16_t			temperature_core[8]; // CPU core temperature on APUs
	uint16_t			temperature_l3[2];

	/* Utilization */
	uint16_t			average_gfx_activity;
	uint16_t			average_mm_activity; // UVD or VCN

	/* Driver attached timestamp (in ns) */
	uint64_t			system_clock_counter;

	/* Power/Energy */
	uint16_t			average_socket_power; // dGPU + APU power on A + A platform
	uint16_t			average_cpu_power;
	uint16_t			average_soc_power;
	uint16_t			average_gfx_power;
	uint16_t			average_core_power[8]; // CPU core power on APUs

	/* Average clocks */
	uint16_t			average_gfxclk_frequency;
	uint16_t			average_socclk_frequency;
	uint16_t			average_uclk_frequency;
	uint16_t			average_fclk_frequency;
	uint16_t			average_vclk_frequency;
	uint16_t			average_dclk_frequency;

	/* Current clocks */
	uint16_t			current_gfxclk;
	uint16_t			current_socclk;
	uint16_t			current_uclk;
	uint16_t			current_fclk;
	uint16_t			current_vclk;
	uint16_t			current_dclk;
	uint16_t			current_coreclk[8]; // CPU core clocks
	uint16_t			current_l3clk[2];

	/* Throttle status (ASIC dependent) */
	uint32_t			throttle_status;

	/* Fans */
	uint16_t			fan_pwm;

	uint16_t			padding[3];

	/* Throttle status (ASIC independent) */
	uint64_t			indep_throttle_status;
};
union umr_gpu_metrics {
	struct umr_metrics_table_header hdr;
	struct umr_gpu_metrics_v1_0 v1;
	struct umr_gpu_metrics_v1_1 v1_1;
	struct umr_gpu_metrics_v1_2 v1_2;
	struct umr_gpu_metrics_v1_3 v1_3;
	struct umr_gpu_metrics_v2_0 v2;
	struct umr_gpu_metrics_v2_1 v2_1;
	struct umr_gpu_metrics_v2_2 v2_2;
};

struct field_info {
	const char *name;
	uint32_t size;
	uint32_t offset;
};

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#define sizeof_field(TYPE, MEMBER) sizeof((((TYPE *)0)->MEMBER))

#define FIELD_INFO(TYPE, MEMBER)	\
{ #MEMBER, sizeof_field(TYPE, MEMBER), offsetof(TYPE, MEMBER) }

int umr_dump_metrics(struct umr_asic *asic, const void *table, uint32_t size);

/* discover */
// size of serialized umr_discovery_table_entry
#define DET_REC_SIZE (128 + 5 * 2 + 8 * 16)

struct umr_discovery_table_entry {
	char ipname[128];
	int die, instance, maj, min, rev;
	uint64_t segments[16];
	struct umr_discovery_table_entry *next;
};
struct umr_asic *umr_discover_asic(struct umr_options *options, umr_err_output errout);
struct umr_asic *umr_discover_asic_by_did(struct umr_options *options, long did, umr_err_output errout, int *tryipdiscovery);
struct umr_asic *umr_discover_asic_by_name(struct umr_options *options, char *name, umr_err_output errout);
struct umr_discovery_table_entry *umr_parse_ip_discovery(int instance, int *nblocks, umr_err_output errout);
struct umr_asic *umr_discover_asic_by_discovery_table(char *asicname, struct umr_options *options, umr_err_output errout);
void umr_free_asic_blocks(struct umr_asic *asic);
void umr_free_asic(struct umr_asic *asic);
void umr_free_maps(struct umr_asic *asic);
void umr_close_asic(struct umr_asic *asic); // call this to close a fully open asic
int umr_query_drm(struct umr_asic *asic, int field, void *ret, int size);
int umr_query_drm_vbios(struct umr_asic *asic, int field, int type, void *ret, int size);
int umr_update(struct umr_asic *asic, char *script);
int umr_update_string(struct umr_asic *asic, char *sdata);

/* lib helpers */
uint32_t umr_get_ip_revision(struct umr_asic *asic, const char *ipname);
int umr_get_wave_status(struct umr_asic *asic, unsigned se, unsigned sh, unsigned cu, unsigned simd, unsigned wave, struct umr_wave_status *ws);
struct umr_wave_data *umr_scan_wave_data(struct umr_asic *asic);
int umr_read_wave_status_via_mmio_gfx8_9(struct umr_asic *asic, uint32_t simd, uint32_t wave, uint32_t *dst, int *no_fields);
int umr_read_wave_status_via_mmio_gfx10(struct umr_asic *asic, uint32_t wave, uint32_t *dst, int *no_fields);
int umr_parse_wave_data_gfx(struct umr_asic *asic, struct umr_wave_status *ws, const uint32_t *buf);
int umr_get_wave_sq_info_vi(struct umr_asic *asic, unsigned se, unsigned sh, unsigned cu, struct umr_wave_status *ws);
int umr_get_wave_sq_info(struct umr_asic *asic, unsigned se, unsigned sh, unsigned cu, struct umr_wave_status *ws);
int umr_read_sgprs(struct umr_asic *asic, struct umr_wave_status *ws, uint32_t *dst);
int umr_read_vgprs(struct umr_asic *asic, struct umr_wave_status *ws, uint32_t thread, uint32_t *dst);
int umr_read_sensor(struct umr_asic *asic, int sensor, void *dst, int *size);

/* mmio helpers */
// init the mmio lookup table
int umr_create_mmio_accel(struct umr_asic *asic);

// find ip block with optional instance
struct umr_ip_block *umr_find_ip_block(const struct umr_asic *asic, const char *ipname, int instance);

// find the word address of a register
uint32_t umr_find_reg(struct umr_asic *asic, const char *regname);

// wildcard searches
struct umr_find_reg_iter *umr_find_reg_wild_first(struct umr_asic *asic, const char *ip, const char *reg);
struct umr_find_reg_iter_result umr_find_reg_wild_next(struct umr_find_reg_iter *iter);


// find a register and return a printable name (used for human readable output)
char *umr_reg_name(struct umr_asic *asic, uint64_t addr);

// find the register data for a register
struct umr_reg* umr_find_reg_data_by_ip_by_instance_with_ip(struct umr_asic* asic, const char* ip, int inst, const char* regname, struct umr_ip_block **ipp);
struct umr_reg* umr_find_reg_data_by_ip_by_instance(struct umr_asic* asic, const char* ip, int inst, const char* regname);
struct umr_reg *umr_find_reg_data_by_ip(struct umr_asic *asic, const char *ip, const char *regname);
struct umr_reg *umr_find_reg_data(struct umr_asic *asic, const char *regname);
struct umr_reg *umr_find_reg_by_name(struct umr_asic *asic, const char *regname, struct umr_ip_block **ip);
struct umr_reg *umr_find_reg_by_addr(struct umr_asic *asic, uint64_t addr, struct umr_ip_block **ip);

// read/write a 32-bit register given a BYTE address
uint32_t umr_read_reg(struct umr_asic *asic, uint64_t addr, enum regclass type);
int umr_write_reg(struct umr_asic *asic, uint64_t addr, uint32_t value, enum regclass type);

// read/write a register given a name
uint64_t umr_read_reg_by_name(struct umr_asic *asic, char *name);
int umr_write_reg_by_name(struct umr_asic *asic, char *name, uint64_t value);

// read/write a register by ip/inst/name
int umr_write_reg_by_name_by_ip_by_instance(struct umr_asic *asic, char *ip, int inst, char *name, uint64_t value);
uint64_t umr_read_reg_by_name_by_ip_by_instance(struct umr_asic *asic, char *ip, int inst, char *name);

// read/write a register by ip/name
uint64_t umr_read_reg_by_name_by_ip(struct umr_asic *asic, char *ip, char *name);
int umr_write_reg_by_name_by_ip(struct umr_asic *asic, char *ip, char *name, uint64_t value);

// slice a full register into bits (shifted into LSB)
uint64_t umr_bitslice_reg(struct umr_asic *asic, struct umr_reg *reg, char *bitname, uint64_t regvalue);
uint64_t umr_bitslice_reg_quiet(struct umr_asic *asic, struct umr_reg *reg, char *bitname, uint64_t regvalue);
uint64_t umr_bitslice_reg_by_name(struct umr_asic *asic, char *regname, char *bitname, uint64_t regvalue);
uint64_t umr_bitslice_reg_by_name_by_ip(struct umr_asic *asic, char *ip, char *regname, char *bitname, uint64_t regvalue);

// compose a 64-bit register with a value and a bitfield
uint64_t umr_bitslice_compose_value(struct umr_asic *asic, struct umr_reg *reg, char *bitname, uint64_t regvalue);
uint64_t umr_bitslice_compose_value_by_name(struct umr_asic *asic, char *reg, char *bitname, uint64_t regvalue);
uint64_t umr_bitslice_compose_value_by_name_by_ip(struct umr_asic *asic, char *ip, char *regname, char *bitname, uint64_t regvalue);

// bank switching
uint64_t umr_apply_bank_selection_address(struct umr_asic *asic);

// select a GRBM_GFX_IDX
int umr_grbm_select_index(struct umr_asic *asic, uint32_t se, uint32_t sh, uint32_t instance);
int umr_srbm_select_index(struct umr_asic *asic, uint32_t me, uint32_t pipe, uint32_t queue, uint32_t vmid);

// halt/resume SQ waves
int umr_sq_cmd_halt_waves(struct umr_asic *asic, enum umr_sq_cmd_halt_resume mode);

/* IB/ring decoding/dumping/etc */
enum umr_ring_type {
	UMR_RING_GFX=0,
	UMR_RING_COMP,
	UMR_RING_VCN,
	UMR_RING_KIQ,
	UMR_RING_SDMA,

	UMR_RING_UNK=0xFF, // if unknown
};

struct umr_pm4_stream {
	uint32_t pkttype,				// packet type (0==simple write, 3 == packet)
			 pkt0off,				// base address for PKT0 writes
			 opcode,
			 header,				// header DWORD of packet
			 n_words,				// number of words ignoring header
			 *words;				// words following header word

	struct umr_pm4_stream *next,	// adjacent PM4 packet if any
			      *ib;				// IB this packet might point to

	struct {
		uint64_t addr;
		uint32_t vmid;
	} ib_source;					// where did an IB if any come from?

	struct umr_shaders_pgm *shader; // shader program if any

	enum umr_ring_type ring_type;
};

void *umr_read_ring_data(struct umr_asic *asic, char *ringname, uint32_t *ringsize);
struct umr_pm4_stream *umr_pm4_decode_ring(struct umr_asic *asic, char *ringname, int no_halt, int start, int stop);
struct umr_pm4_stream *umr_pm4_decode_stream(struct umr_asic *asic, int vm_partition, uint32_t vmid, uint32_t *stream, uint32_t nwords, enum umr_ring_type rt);
struct umr_pm4_stream *umr_pm4_decode_stream_vm(struct umr_asic *asic, int vm_partition, uint32_t vmid, uint64_t addr, uint32_t nwords, enum umr_ring_type rt);
void umr_free_pm4_stream(struct umr_pm4_stream *stream);

struct umr_shaders_pgm *umr_find_shader_in_stream(struct umr_pm4_stream *stream, unsigned vmid, uint64_t addr);
struct umr_shaders_pgm *umr_find_shader_in_ring(struct umr_asic *asic, char *ringname, unsigned vmid, uint64_t addr, int no_halt);
int umr_pm4_decode_ring_is_halted(struct umr_asic *asic, char *ringname);

// PM4 decoding library
struct umr_pm4_stream_decode_ui {

	/** start_ib -- Start a new IB
	 * ib_addr/ib_vmid: Address of the IB
	 * from_addr/from_vmid: Where does this reference come from?
	 * size: size of IB in DWORDs
	 * type: type of IB (which type of packets
	 */
	void (*start_ib)(struct umr_pm4_stream_decode_ui *ui, uint64_t ib_addr, uint32_t ib_vmid, uint64_t from_addr, uint32_t from_vmid, uint32_t size, int type);

	/** start_opcode -- Start a new opcode
	 * ib_addr/ib_vmid: Address of where packet is found
	 * opcode: The numeric value of the ocpode
	 * nwords: Number of DWORDS in this opcode
	 * opcode_name: Printable string name of opcode
	 * header: Raw header DWORD of this packet
	 * raw_data: Pointer to a buffer of length nwords containing the raw data of this packet (does not include header DWORD)
	 */
	void (*start_opcode)(struct umr_pm4_stream_decode_ui *ui, uint64_t ib_addr, uint32_t ib_vmid, int pkttype, uint32_t opcode, uint32_t nwords, const char *opcode_name, uint32_t header, const uint32_t* raw_data);

	/** add_field -- Add a decoded field to a specific DWORD
	 * ib_addr/ib_vmid:  Address of the word from which the field comes
	 * field_name: printable name of the field
	 * value:  Value of the field
	 * ideal_radix: (10 decimal, 16 hex)
	 */
	void (*add_field)(struct umr_pm4_stream_decode_ui *ui, uint64_t ib_addr, uint32_t ib_vmid, const char *field_name, uint32_t value, char *str, int ideal_radix);

	/** add_shader -- Add a reference to a shader found in the IB stream
	 * ib_addr/ib_vmid:  Address of where reference comes from
	 * asic:  The ASIC the IB stream and shader are bound to
	 * shader: The shader reference
	 */
	void (*add_shader)(struct umr_pm4_stream_decode_ui *ui, struct umr_asic *asic, uint64_t ib_addr, uint32_t ib_vmid, struct umr_shaders_pgm *shader);

	/** add_data -- Add a reference to a data buffer found in the IB stream
	 * ib_addr/ib_vmid:  Address of where reference comes from
	 * asic:  The ASIC the IB stream and shader are bound to
	 * data_addr/data_vmid: A GPUVM reference to the object
	 * type: The type of object
	 */
	void (*add_data)(struct umr_pm4_stream_decode_ui *ui, struct umr_asic *asic, uint64_t ib_addr, uint32_t ib_vmid, uint64_t buf_addr, uint32_t buf_vmid, enum UMR_DATABLOCK_ENUM type, uint64_t etype);

	/** unhandled -- Decoder for unhandled (private) opcodes
	 * asic: The ASIC the IB stream is bound to
	 * ib_addr:ib_vmid: The address where the PM4 opcode comes from
	 * stream:  The pointer to the current stream opcode being handled
	 *
	 * Can be NULL to drop support for unhandled opcodes.
	 */
	void (*unhandled)(struct umr_pm4_stream_decode_ui *ui, struct umr_asic *asic, uint64_t ib_addr, uint32_t ib_vmid, struct umr_pm4_stream *stream);

	void (*done)(struct umr_pm4_stream_decode_ui *ui);

	/** data -- opaque pointer that can be used to track state information */
	void *data;
};

struct umr_pm4_stream *umr_pm4_decode_stream_opcodes(struct umr_asic *asic, struct umr_pm4_stream_decode_ui *ui, struct umr_pm4_stream *stream, uint64_t ib_addr, uint32_t ib_vmid, uint64_t from_addr, uint64_t from_vmid, unsigned long opcodes, int follow);
int umr_pm4_decode_opcodes_ib(struct umr_asic *asic, struct umr_pm4_stream_decode_ui *ui, int vm_partition, uint64_t ib_addr, uint32_t ib_vmid, uint32_t nwords, uint64_t from_addr, uint64_t from_ib, unsigned long opcodes, int follow, enum umr_ring_type rt);

/* SDMA decoding */
struct umr_sdma_stream {
	uint32_t
		opcode,
		sub_opcode,
		nwords,
		header_dw,
		*words;

	struct {
		uint32_t vmid, size;
		uint64_t addr;
	} ib;

	struct {
		int vmid;
		uint64_t addr;
	} from;

	struct umr_sdma_stream *next, *next_ib;
};

struct umr_sdma_stream_decode_ui {

	/** start_ib -- Start a new IB
	 * ib_addr/ib_vmid: Address of the IB
	 * from_addr/from_vmid: Where does this reference come from?
	 * size: size of IB in DWORDs
	 */
	void (*start_ib)(struct umr_sdma_stream_decode_ui *ui, uint64_t ib_addr, uint32_t ib_vmid, uint64_t from_addr, uint32_t from_vmid, uint32_t size);

	/** start_opcode -- Start a new opcode
	 * ib_addr/ib_vmid: Address of where packet is found
	 * opcode: The numeric value of the ocpode
	 * nwords: number of DWORDS in this opcode
	 * opcode_name: Printable string name of opcode
	 */
	void (*start_opcode)(struct umr_sdma_stream_decode_ui *ui, uint64_t ib_addr, uint32_t ib_vmid, uint32_t opcode, uint32_t sub_opcode, uint32_t nwords, char *opcode_name, uint32_t header_dw, uint32_t *raw_data);

	/** add_field -- Add a decoded field to a specific DWORD
	 * ib_addr/ib_vmid:  Address of the word from which the field comes
	 * field_name: printable name of the field
	 * value:  Value of the field
	 * ideal_radix: (10 decimal, 16 hex)
	 */
	void (*add_field)(struct umr_sdma_stream_decode_ui *ui, uint64_t ib_addr, uint32_t ib_vmid, const char *field_name, uint32_t value, char *str, int ideal_radix);

	/** unhandled -- Decoder for unhandled (private) opcodes
	 * asic: The ASIC the IB stream is bound to
	 * ib_addr:ib_vmid: The address where the sdma opcode comes from
	 * stream:  The pointer to the current stream opcode being handled
	 *
	 * Can be NULL to drop support for unhandled opcodes.
	 */
	void (*unhandled)(struct umr_sdma_stream_decode_ui *ui, struct umr_asic *asic, uint64_t ib_addr, uint32_t ib_vmid, struct umr_sdma_stream *stream);

	/** unhandled_size -- For returning size of packets for unhandled (private) opcodes.
	 * To use, populate stream->nwords with the size of the current packet (should not include header DWORD) and then
	 * return 0 to signal success. Returning non-zero will signal failure to handle opcode.
	 *
	 * asic: The ASIC the IB stream is bound to
	 * stream:  The pointer to the current stream opcode being handled. Write the size of the packet to stream->nwords.
	 *
	 * return: Return non-zero if size of packet is unknown.
	 *
	 * Can be NULL to drop support for unhandled opcodes.
	 */
	int (*unhandled_size)(struct umr_sdma_stream_decode_ui *ui, struct umr_asic *asic, struct umr_sdma_stream *stream);

	/** unhandled_subop -- Decoder for unhandled (private) sub-opcodes
	 * asic: The ASIC the IB stream is bound to
	 * ib_addr:ib_vmid: The address where the sdma opcode comes from
	 * stream:  The pointer to the current stream opcode being handled
	 *
	 * Can be NULL to drop support for unhandled opcodes.
	 */
	void (*unhandled_subop)(struct umr_sdma_stream_decode_ui *ui, struct umr_asic *asic, uint64_t ib_addr, uint32_t ib_vmid, struct umr_sdma_stream *stream);


	void (*done)(struct umr_sdma_stream_decode_ui *ui);

	/** data -- opaque pointer that can be used to track state information */
	void *data;
};

struct umr_sdma_stream *umr_sdma_decode_ring(struct umr_asic *asic, struct umr_sdma_stream_decode_ui *ui, char *ringname, int start, int stop);
struct umr_sdma_stream *umr_sdma_decode_stream(struct umr_asic *asic, struct umr_sdma_stream_decode_ui *ui, int vm_partition, uint64_t from_addr, uint32_t from_vmid, uint32_t *stream, uint32_t nwords);
struct umr_sdma_stream *umr_sdma_decode_stream_vm(struct umr_asic *asic, struct umr_sdma_stream_decode_ui *ui, int vm_partition, uint32_t vmid, uint64_t addr, uint32_t nwords, enum umr_ring_type rt);
void umr_free_sdma_stream(struct umr_sdma_stream *stream);

struct umr_sdma_stream *umr_sdma_decode_stream_opcodes(struct umr_asic *asic, struct umr_sdma_stream_decode_ui *ui, struct umr_sdma_stream *stream, uint64_t ib_addr, uint32_t ib_vmid, uint64_t from_addr, uint64_t from_vmid, unsigned long opcodes, int follow);

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

// MES library
struct umr_mes_stream {
	uint32_t *words,
		 nwords,
		 header,
		 opcode,
		 type;

	struct umr_mes_stream *next;
};

struct umr_mes_stream_decode_ui {
	/** start_ib -- Start a new IB
	 * ib_addr/ib_vmid: Address of the IB
	 * from_addr/from_vmid: Where does this reference come from?
	 * size: size of IB in DWORDs
	 * type: type of IB (which type of packets
	 */
	void (*start_ib)(struct umr_mes_stream_decode_ui *ui, uint64_t ib_addr, uint32_t ib_vmid, uint64_t from_addr, uint32_t from_vmid, uint32_t size, int type);

	/** start_opcode -- Start a new opcode
	 * ib_addr/ib_vmid: Address of where packet is found
	 * opcode: The numeric value of the ocpode
	 * nwords: Number of DWORDS in this opcode
	 * opcode_name: Printable string name of opcode
	 * header: Raw header DWORD of this packet
	 * raw_data: Pointer to a buffer of length nwords containing the raw data of this packet (does not include header DWORD)
	 */
	void (*start_opcode)(struct umr_mes_stream_decode_ui *ui, uint64_t ib_addr, uint32_t ib_vmid, uint32_t opcode, uint32_t nwords, const char *opcode_name, uint32_t header, const uint32_t* raw_data);

	/** add_field -- Add a decoded field to a specific DWORD
	 * ib_addr/ib_vmid:  Address of the word from which the field comes
	 * field_name: printable name of the field
	 * value:  Value of the field
	 * ideal_radix: (10 decimal, 16 hex)
	 */
	void (*add_field)(struct umr_mes_stream_decode_ui *ui, uint64_t ib_addr, uint32_t ib_vmid, const char *field_name, uint64_t value, char *str, int ideal_radix);

	/** unhandled -- Decoder for unhandled (private) opcodes
	 * asic: The ASIC the IB stream is bound to
	 * ib_addr:ib_vmid: The address where the mes opcode comes from
	 * stream:  The pointer to the current stream opcode being handled
	 *
	 * Can be NULL to drop support for unhandled opcodes.
	 */
	void (*unhandled)(struct umr_mes_stream_decode_ui *ui, struct umr_asic *asic, uint64_t ib_addr, uint32_t ib_vmid, struct umr_mes_stream *stream);

	void (*done)(struct umr_mes_stream_decode_ui *ui);

	/** data -- opaque pointer that can be used to track state information */
	void *data;
};

struct umr_mes_stream *umr_mes_decode_stream(struct umr_asic *asic, uint32_t *stream, uint32_t nwords);
struct umr_mes_stream *umr_mes_decode_stream_opcodes(struct umr_asic *asic, struct umr_mes_stream_decode_ui *ui, struct umr_mes_stream *stream, uint64_t ib_addr, uint32_t ib_vmid, unsigned long opcodes);
struct umr_mes_stream *umr_mes_decode_ring(struct umr_asic *asic, char *ringname, int no_halt, int start, int stop);
struct umr_mes_stream *umr_mes_decode_stream_vm(struct umr_asic *asic, int vm_partition, uint32_t vmid, uint64_t addr, uint32_t nwords);
void umr_free_mes_stream(struct umr_mes_stream *stream);

// various low level functions
const char *umr_pm4_opcode_to_str(uint32_t header);
void umr_print_decode(struct umr_asic *asic, struct umr_ring_decoder *decoder, uint32_t ib, int (*custom_message)(const char *fmt, ...));
void umr_dump_ib(struct umr_asic *asic, struct umr_ring_decoder *decoder);
void umr_dump_shaders(struct umr_asic *asic, struct umr_ring_decoder *decoder, struct umr_wave_data *wd);
void umr_dump_data(struct umr_asic *asic, struct umr_ring_decoder *decoder);

int umr_shader_disasm(struct umr_asic *asic,
		    uint8_t *inst, unsigned inst_bytes,
		    uint64_t PC,
		    char ***disasm_text);
int umr_vm_disasm_to_str(struct umr_asic *asic, int vm_partition, unsigned vmid, uint64_t addr, uint64_t PC, uint32_t size, uint32_t start_offset, char ***out);
int umr_vm_disasm(struct umr_asic *asic, int vm_partition, unsigned vmid, uint64_t addr, uint64_t PC, uint32_t size, uint32_t start_offset, struct umr_wave_data *wd);
uint32_t umr_compute_shader_size(struct umr_asic *asic, int vm_partition, struct umr_shaders_pgm *shader);


// memory access
int umr_access_vram_via_mmio(struct umr_asic *asic, uint64_t address, uint32_t size, void *dst, int write_en);
uint64_t umr_vm_dma_to_phys(struct umr_asic *asic, uint64_t dma_addr);
int umr_access_sram(struct umr_asic *asic, uint64_t address, uint32_t size, void *dst, int write_en);
int umr_access_vram(struct umr_asic *asic, int partition, uint32_t vmid, uint64_t address, uint32_t size, void *data, int write_en);
int umr_access_linear_vram(struct umr_asic *asic, uint64_t address, uint32_t size, void *data, int write_en);
#define umr_read_vram(asic, partition, vmid, address, size, dst) umr_access_vram(asic, partition, vmid, address, size, dst, 0)
#define umr_write_vram(asic, partition, vmid, address, size, src) umr_access_vram(asic, partition, vmid, address, size, src, 1)

// test harness support

struct umr_test_harness *umr_create_test_harness_file(const char *fname);
struct umr_test_harness *umr_create_test_harness(const char *script);
void umr_free_test_harness(struct umr_test_harness *th);
void umr_attach_test_harness(struct umr_test_harness *th, struct umr_asic *asic);
int umr_test_harness_get_config_data(struct umr_asic *asic, uint8_t *dst);
void *umr_test_harness_get_ring_data(struct umr_asic *asic, uint32_t *ringsize);

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
int umr_scan_config(struct umr_asic *asic, int xgmi_scan);
void umr_apply_callbacks(struct umr_asic *asic,
			 struct umr_memory_access_funcs *mems,
			 struct umr_register_access_funcs *regs);
//clock
struct umr_clock_source{
	char clock_name[32];
	uint32_t clock_Mhz[10];
	int clock_level;
	int current_clock;
};

struct umr_asic_clocks{
	struct umr_asic *asic;
	struct umr_clock_source clocks[UMR_CLOCK_MAX];
};

int umr_read_clock(struct umr_asic *asic, char* clockname, struct umr_clock_source* clock);
int umr_set_clock(struct umr_asic *asic, const char* clock_name, void* value);
void umr_set_clock_performance(struct umr_asic *asic, const char* operation);
int umr_check_clock_performance(struct umr_asic *asic, char* name, uint32_t len);
void umr_gfxoff_read(struct umr_asic *asic);

#if UMR_GUI
#include "parson.h"
JSON_Value *umr_process_json_request(JSON_Object *request);
void umr_run_gui(const char *url);
#endif

// database
struct umr_database_scan_item {
	char path[256], fname[128], ipname[128];
	int maj, min, rev;
	struct umr_database_scan_item *next;
};

struct umr_soc15_database {
	char ipname[64];
	uint64_t off[32][8];
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

FILE *umr_database_open(char *path, char *filename);
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

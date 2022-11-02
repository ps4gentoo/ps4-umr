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

// print bitfields but skip ones set to 2^32 - 1 as they are not 
// present on the current asic
#define PP(x, y) if (wd->ws.x.y != 0xFFFFFFFF) { if (col++ == 4) { col = 1; printf("\n\t"); } printf("%s%20s%s: %s%8u%s | ", GREEN, #y, RST, BLUE, (unsigned)wd->ws.x.y, RST); }
#define PX(x, y) if (wd->ws.x.y != 0xFFFFFFFF) { if (col++ == 4) { col = 1; printf("\n\t"); } printf("%s%20s%s: %s%08lx%s | ", GREEN, #y, RST, BLUE, (unsigned long)wd->ws.x.y, RST); }

#define P(x) if (col++ == 4) { col = 1; printf("\n\t"); } printf("%s%20s%s: %s%8u%s | ", GREEN, #x, RST, BLUE, (unsigned)wd->ws.x, RST);
#define X(x) if (col++ == 4) { col = 1; printf("\n\t"); } printf("%s%20s%s: %s%08lx%s | ", GREEN, #x, RST, BLUE, (unsigned long)wd->ws.x, RST);

#define H(x) if (col) { printf("\n"); }; col = 0; printf("\n\n%s:\n\t", x);
#define Hv(x, y) if (col) { printf("\n"); }; col = 0; printf("\n\n%s[%08lx]:\n\t", x, (unsigned long)y);

#define NUM_OPCODE_WORDS 16

static void umr_print_waves_si_ai(struct umr_asic *asic)
{
	uint32_t x, y, shift, thread;
	uint64_t pgm_addr, shader_addr;
	struct umr_wave_data *wd, *owd;
	int first = 1, col = 0, ring_halted = 0, use_ring = 1;
	struct umr_shaders_pgm *shader = NULL;
	struct umr_pm4_stream *stream;
	struct {
		uint32_t vmid, size;
		uint64_t addr;
	} ib_addr;

	if (sscanf(asic->options.ring_name, "%"SCNx32"@%"SCNx64".%"SCNx32, &ib_addr.vmid, &ib_addr.addr, &ib_addr.size) == 3)
		use_ring = 0;

	if (asic->options.halt_waves) {
		umr_sq_cmd_halt_waves(asic, UMR_SQ_CMD_HALT);
		if (use_ring && !umr_pm4_decode_ring_is_halted(asic, asic->options.ring_name[0] ? asic->options.ring_name : "gfx"))
			fprintf(stderr, "[WARNING]: Rings are not halted!  %s\n", asic->options.disasm_anyways ? "" : "Use '-O disasm_anyways' to enable disassembly without halted rings");
		else
			ring_halted = 1;
	}

	// always disasm if disasm_anyways is enabled
	if (asic->options.disasm_anyways)
		ring_halted = 1;

	// don't scan for shader info by reading the ring if no_disasm is
	// requested.  This is useful for when the ring or IBs contain
	// invalid or racy data that cannot be reliably parsed.
	if (!asic->options.no_disasm) {
		// scan a ring but don't trigger the halt/resume
		// since it would have already been done
		if (use_ring) {
			stream = umr_pm4_decode_ring(asic, asic->options.ring_name[0] ? asic->options.ring_name : "gfx", 1, -1, -1);
		} else {
			stream = umr_pm4_decode_stream_vm(asic, asic->options.vm_partition, ib_addr.vmid, ib_addr.addr, ib_addr.size / 4, UMR_RING_UNK);
		}
	} else {
		ring_halted = 0;
		stream = NULL;
	}

	if (asic->family <= FAMILY_CIK)
		shift = 3;  // on SI..CIK allocations were done in 8-dword blocks
	else
		shift = 4;  // on VI allocations are in 16-dword blocks

	owd = wd = umr_scan_wave_data(asic);
	while (wd) {
		if (!asic->options.bitfields && first) {
			static const char* titles[] = {
				"WAVE_STATUS", "PC_HI", "PC_LO", "INST_DW0", "INST_DW1", "EXEC_HI", "EXEC_LO", "HW_ID", "GPRALLOC",
				"LDSALLOC", "TRAPSTS", "IBSTS", "TBA_HI", "TBA_LO", "TMA_HI", "TMA_LO", "IB_DBG0", "M0", "MODE", NULL
			};
			first = 0;
			printf("SE SH CU SIMD WAVE# ");
			for (int x = 0; titles[x]; x++) {
				printf("%8s ", titles[x]);
			}
			printf("\n");
		}
		if (!asic->options.bitfields) {
		printf(
"%2u %2u %2u %4u %5u " // se/sh/cu/simd/wave
"   %08lx %08lx %08lx " // wave_status pc/hi/lo
"%08lx %08lx %08lx %08lx " // inst0/1 exec hi/lo
"%08lx %08lx %08lx %08lx %08lx " // HW_ID GPR/LDSALLOC TRAP/IB STS
"%08lx %08lx %08lx %08lx %08lx %08lx %08lx " // TBA_HI TBA_LO TMA_HI TMA_LO IB_DBG0 M0 MODE\n");
"\n",
(unsigned)wd->se, (unsigned)wd->sh, (unsigned)wd->cu, (unsigned)wd->ws.hw_id.simd_id, (unsigned)wd->ws.hw_id.wave_id,
(unsigned long)wd->ws.wave_status.value, (unsigned long)wd->ws.pc_hi, (unsigned long)wd->ws.pc_lo,
(unsigned long)wd->ws.wave_inst_dw0, (unsigned long)wd->ws.wave_inst_dw1, (unsigned long)wd->ws.exec_hi, (unsigned long)wd->ws.exec_lo,
(unsigned long)wd->ws.hw_id.value, (unsigned long)wd->ws.gpr_alloc.value, (unsigned long)wd->ws.lds_alloc.value, (unsigned long)wd->ws.trapsts.value, (unsigned long)wd->ws.ib_sts.value,
(unsigned long)wd->ws.tba_hi, (unsigned long)wd->ws.tba_lo, (unsigned long)wd->ws.tma_hi, (unsigned long)wd->ws.tma_lo, (unsigned long)wd->ws.ib_dbg0, (unsigned long)wd->ws.m0, (unsigned long)wd->ws.mode.value
);
			if (wd->ws.wave_status.halt || wd->ws.wave_status.fatal_halt) {
				for (x = 0; x < ((wd->ws.gpr_alloc.sgpr_size + 1) << shift); x += 4)
					printf(">SGPRS[%s%u%s..%s%u%s] = { %s%08lx%s, %s%08lx%s, %s%08lx%s, %s%08lx%s }\n",
						YELLOW, (unsigned)(x), RST,
						YELLOW, (unsigned)(x + 3), RST,
						BLUE, (unsigned long)wd->sgprs[x], RST,
						BLUE, (unsigned long)wd->sgprs[x+1], RST,
						BLUE, (unsigned long)wd->sgprs[x+2], RST,
						BLUE, (unsigned long)wd->sgprs[x+3], RST);

				if (wd->ws.wave_status.trap_en || wd->ws.wave_status.priv) {
					for (y = 0, x = 0x6C; x < (16 + 0x6C); x += 4) {
						printf(">%s[%s%u%s..%s%u%s] = { %s%08lx%s, %s%08lx%s, %s%08lx%s, %s%08lx%s }\n",
							(x < (0x6C + 4) && asic->family <= FAMILY_VI) ? "TBA/TMA" : "TTMP",
							YELLOW, (unsigned)(y), RST,
							YELLOW, (unsigned)(y + 3), RST,
							BLUE, (unsigned long)wd->sgprs[x], RST,
							BLUE, (unsigned long)wd->sgprs[x+1], RST,
							BLUE, (unsigned long)wd->sgprs[x+2], RST,
							BLUE, (unsigned long)wd->sgprs[x+3], RST);

						// restart numbering on SI..VI with TTMP0
						y += 4;
						if (x == 0x6C && asic->family <= FAMILY_VI)
							y = 0;
					}
				}
			}
			if (ring_halted && (wd->ws.wave_status.halt || wd->ws.wave_status.fatal_halt)) {
				pgm_addr = (((uint64_t)wd->ws.pc_hi << 32) | wd->ws.pc_lo) - (NUM_OPCODE_WORDS*4)/2;
				umr_vm_disasm(asic, asic->options.vm_partition, wd->ws.hw_id.vm_id, pgm_addr, (((uint64_t)wd->ws.pc_hi << 32) | wd->ws.pc_lo), NUM_OPCODE_WORDS*4, 0, NULL);
			}
		} else {
			first = 0;
			printf("\n------------------------------------------------------\nse%u.sh%u.cu%u.simd%u.wave%u\n",
			(unsigned)wd->se, (unsigned)wd->sh, (unsigned)wd->cu, (unsigned)wd->ws.hw_id.simd_id, (unsigned)wd->ws.hw_id.wave_id);

			H("Main Registers");
			X(pc_hi);
			X(pc_lo);
			X(wave_inst_dw0);
			X(wave_inst_dw1);
			X(exec_hi);
			X(exec_lo);
			X(tba_hi);
			X(tba_lo);
			X(tma_hi);
			X(tma_lo);
			X(m0);
			X(ib_dbg0);

			Hv("Wave_Status", wd->ws.wave_status.value);
			PP(wave_status, scc);
			PP(wave_status, execz);
			PP(wave_status, vccz);
			PP(wave_status, in_tg);
			PP(wave_status, halt);
			PP(wave_status, valid);
			PP(wave_status, spi_prio);
			PP(wave_status, wave_prio);
			PP(wave_status, priv);
			PP(wave_status, trap_en);
			PP(wave_status, trap);
			PP(wave_status, ttrace_en);
			PP(wave_status, export_rdy);
			PP(wave_status, in_barrier);
			PP(wave_status, ecc_err);
			PP(wave_status, skip_export);
			PP(wave_status, perf_en);
			PP(wave_status, cond_dbg_user);
			PP(wave_status, cond_dbg_sys);
			if (asic->family < FAMILY_AI) {
				PP(wave_status, data_atc);
				PP(wave_status, inst_atc);
				PP(wave_status, dispatch_cache_ctrl);
			} else {
				PP(wave_status, allow_replay);
				PP(wave_status, fatal_halt);
			}
			PP(wave_status, must_export);

			Hv("HW_ID", wd->ws.hw_id.value);
			PP(hw_id, wave_id);
			PP(hw_id, simd_id);
			PP(hw_id, pipe_id);
			PP(hw_id, cu_id);
			PP(hw_id, sh_id);
			PP(hw_id, se_id);
			PP(hw_id, tg_id);
			PP(hw_id, vm_id);
			PP(hw_id, queue_id);
			PP(hw_id, state_id);
			PP(hw_id, me_id);

			Hv("GPR_ALLOC", wd->ws.gpr_alloc.value);
			PP(gpr_alloc, vgpr_base);
			PP(gpr_alloc, vgpr_size);
			PP(gpr_alloc, sgpr_base);
			PP(gpr_alloc, sgpr_size);

			if (wd->ws.wave_status.halt || wd->ws.wave_status.fatal_halt) {
				printf("\n\nSGPRS:\n");
				for (x = 0; x < ((wd->ws.gpr_alloc.sgpr_size + 1) << shift); x += 4)
					printf("\t[%s%4u%s..%s%4u%s] = { %s%08lx%s, %s%08lx%s, %s%08lx%s, %s%08lx%s }\n",
						YELLOW, (unsigned)(x), RST,
						YELLOW, (unsigned)(x + 3), RST,
						BLUE, (unsigned long)wd->sgprs[x], RST,
						BLUE, (unsigned long)wd->sgprs[x+1], RST,
						BLUE, (unsigned long)wd->sgprs[x+2], RST,
						BLUE, (unsigned long)wd->sgprs[x+3], RST);

				if (wd->ws.wave_status.trap_en || wd->ws.wave_status.priv) {
					for (y  = 0, x = 0x6C; x < (16 + 0x6C); x += 4) {
						// only print label once each
						if ((asic->family <= FAMILY_VI && x < 0x6C + 8) ||
							(asic->family > FAMILY_VI && x < 0x6C + 4))
							printf("\n%s:\n", (x < 0x6C + 4 && asic->family <= FAMILY_VI) ? "TBA/TMA" : "TTMP");
						printf("\t[%s%4u%s..%s%4u%s] = { %s%08lx%s, %s%08lx%s, %s%08lx%s, %s%08lx%s }\n",
							YELLOW, (unsigned)(y), RST,
							YELLOW, (unsigned)(y + 3), RST,
							BLUE, (unsigned long)wd->sgprs[x], RST,
							BLUE, (unsigned long)wd->sgprs[x+1], RST,
							BLUE, (unsigned long)wd->sgprs[x+2], RST,
							BLUE, (unsigned long)wd->sgprs[x+3], RST);

						// reset count on SI..VI
						y += 4;
						if (x == 0x6C && asic->family <= FAMILY_VI)
							y = 0;
					}
				}
			}

			if (wd->have_vgprs) {
				unsigned granularity = asic->parameters.vgpr_granularity; // default is blocks of 4 VGPRs
				printf("\n");
				for (x = 0; x < ((wd->ws.gpr_alloc.vgpr_size + 1) << granularity); ++x) {
					if (x % 16 == 0) {
						if (x == 0)
							printf("VGPRS:       ");
						else
							printf("             ");
						for (thread = 0; thread < 64; ++thread) {
							unsigned live = thread < 32 ? (wd->ws.exec_lo & (1u << thread))
											: (wd->ws.exec_hi & (1u << (thread - 32)));
							printf(live ? " t%02u     " : " (t%02u)   ", thread);
						}
						printf("\n");
					}

					printf("    [%s%3u%s] = {", YELLOW, x, RST);
					for (thread = 0; thread < 64; ++thread) {
						unsigned live = thread < 32 ? (wd->ws.exec_lo & (1u << thread))
										: (wd->ws.exec_hi & (1u << (thread - 32)));
						printf(" %s%08x%s", live ? BLUE : RST, wd->vgprs[thread * 256 + x], RST);
					}
					printf(" }\n");
				}
			}

			if (ring_halted && (wd->ws.wave_status.halt || wd->ws.wave_status.fatal_halt)) {
				uint32_t shader_size = NUM_OPCODE_WORDS*4;
				printf("\n\nPGM_MEM:");
				pgm_addr = (((uint64_t)wd->ws.pc_hi << 32) | wd->ws.pc_lo);
				if (stream)
					shader = umr_find_shader_in_stream(stream, wd->ws.hw_id.vm_id, pgm_addr);
				if (shader) {
					printf(" (found shader at: %s%u%s@0x%s%llx%s of %s%u%s bytes)\n",
						BLUE, shader->vmid, RST,
						YELLOW, (unsigned long long)shader->addr, RST,
						BLUE, shader->size, RST);

					// start decoding a bit before PC if possible
					if (!(asic->options.full_shader) && (shader->addr + ((NUM_OPCODE_WORDS*4)/2) < pgm_addr))
						pgm_addr -= (NUM_OPCODE_WORDS*4)/2;
					else
						pgm_addr = shader->addr;
					if (asic->options.full_shader)
						shader_size = shader->size;
					shader_addr = shader->addr;
					free(shader);
				} else {
					pgm_addr -= (NUM_OPCODE_WORDS*4)/2;
					shader_addr = pgm_addr;
					printf("\n");
				}
				umr_vm_disasm(asic, asic->options.vm_partition, wd->ws.hw_id.vm_id, shader_addr, (((uint64_t)wd->ws.pc_hi << 32) | wd->ws.pc_lo), shader_size, pgm_addr - shader_addr, NULL);
			}

			Hv("LDS_ALLOC", wd->ws.lds_alloc.value);
			PP(lds_alloc, lds_base);
			PP(lds_alloc, lds_size);

			Hv("IB_STS", wd->ws.ib_sts.value);
			PP(ib_sts, vm_cnt);
			PP(ib_sts, exp_cnt);
			PP(ib_sts, lgkm_cnt);
			PP(ib_sts, valu_cnt);

			Hv("TRAPSTS", wd->ws.trapsts.value);
			PP(trapsts, excp);
			PP(trapsts, excp_cycle);
			PP(trapsts, dp_rate);

			Hv("MODE", wd->ws.mode.value);
			PP(mode, fp_round);
			PP(mode, fp_denorm);
			PP(mode, dx10_clamp);
			PP(mode, ieee);
			PP(mode, lod_clamped);
			PP(mode, debug_en);
			PP(mode, excp_en);
			if (asic->family > FAMILY_VI)
				PP(mode, fp16_ovfl);
			PP(mode, pops_packer0);
			PP(mode, pops_packer1);
			if (asic->family > FAMILY_VI)
				PP(mode, disable_perf);
			PP(mode, gpr_idx_en);
			PP(mode, vskip);
			PP(mode, csp);

			printf("\n"); col = 0;
		}
		wd = wd->next;
	}
	if (first)
		printf("No active waves!\n");

	wd = owd;
	while (wd) {
		owd = wd->next;
		free(wd);
		wd = owd;
	}

	if (stream)
		umr_free_pm4_stream(stream);

	if (asic->options.halt_waves)
		umr_sq_cmd_halt_waves(asic, UMR_SQ_CMD_RESUME);
}

static void umr_print_waves_nv(struct umr_asic *asic)
{
	uint32_t x, y, thread;
	uint64_t pgm_addr, shader_addr;
	struct umr_wave_data *wd, *owd;
	int first = 1, col = 0, ring_halted = 0, use_ring = 1;
	struct umr_shaders_pgm *shader = NULL;
	struct umr_pm4_stream *stream;
	struct {
		uint32_t vmid, size;
		uint64_t addr;
	} ib_addr;

	if (sscanf(asic->options.ring_name, "%"SCNx32"@%"SCNx64".%"SCNx32, &ib_addr.vmid, &ib_addr.addr, &ib_addr.size) == 3)
		use_ring = 0;

	if (asic->options.halt_waves) {
		// warn users if they don't specify a ring on gfx10 hardware
		if (asic->family >= FAMILY_NV && !asic->options.ring_name[0])
			fprintf(stderr, "[WARNING]: On gfx10 the default ring name 'gfx' is not valid.  Please specify one on the command line.\n");

		umr_sq_cmd_halt_waves(asic, UMR_SQ_CMD_HALT);
		if (use_ring && !umr_pm4_decode_ring_is_halted(asic, asic->options.ring_name[0] ? asic->options.ring_name : "gfx"))
			fprintf(stderr, "[WARNING]: Rings are not halted!\n");
		else
			ring_halted = 1;
	}


	// always disasm if disasm_anyways is enabled
	if (asic->options.disasm_anyways)
		ring_halted = 1;

	// don't scan for shader info by reading the ring if no_disasm is
	// requested.  This is useful for when the ring or IBs contain
	// invalid or racy data that cannot be reliably parsed.
	if (!asic->options.no_disasm) {
		// scan a ring but don't trigger the halt/resume
		// since it would have already been done
		if (use_ring) {
			stream = umr_pm4_decode_ring(asic, asic->options.ring_name[0] ? asic->options.ring_name : "gfx", 1, -1, -1);
		} else {
			stream = umr_pm4_decode_stream_vm(asic, asic->options.vm_partition, ib_addr.vmid, ib_addr.addr, ib_addr.size / 4, UMR_RING_UNK);
		}
	} else {
		ring_halted = 0;
		stream = NULL;
	}

	owd = wd = umr_scan_wave_data(asic);

	while (wd) {
		if (!asic->options.bitfields && first) {
			static const char* titles[] = {
				"WAVE_STATUS", "PC_HI", "PC_LO", "INST_DW0", "EXEC_HI", "EXEC_LO", "HW_ID1", "HW_ID2", "GPRALLOC", "LDSALLOC", "TRAPSTS", "IBSTS1", "IBSTS2", "IB_DBG1", "M0", "MODE", NULL
			};
			first = 0;
			printf("SE SA WGP SIMD WAVE# ");
			for (int x = 0; titles[x]; x++) {
				printf("%8s ", titles[x]);
			}
			printf("\n");
		}
		if (!asic->options.bitfields) {
		printf(
"%2u %2u %3u %4u %5u " // se/sa/wgp/simd/wave
"   %08lx %08lx %08lx " // wave_status pc/hi/lo
"%08lx %08lx %08lx " // inst0 exec hi/lo
"%08lx %08lx %08lx %08lx %08lx %08lx %08lx " // HW_ID1 HW_ID2 GPR/LDSALLOC TRAP/IB STS
"%08lx %08lx %08lx " // IB_DBG1 M0 MODE\n");
"\n",
(unsigned)wd->se, (unsigned)wd->sh, (unsigned)wd->cu, (unsigned)wd->ws.hw_id1.simd_id, (unsigned)wd->ws.hw_id1.wave_id, // TODO: wgp printed out won't match geometry for now w.r.t. to SPI
(unsigned long)wd->ws.wave_status.value, (unsigned long)wd->ws.pc_hi, (unsigned long)wd->ws.pc_lo,
(unsigned long)wd->ws.wave_inst_dw0, (unsigned long)wd->ws.exec_hi, (unsigned long)wd->ws.exec_lo,
(unsigned long)wd->ws.hw_id1.value, (unsigned long)wd->ws.hw_id2.value, (unsigned long)wd->ws.gpr_alloc.value, (unsigned long)wd->ws.lds_alloc.value, (unsigned long)wd->ws.trapsts.value,
(unsigned long)wd->ws.ib_sts.value, (unsigned long)wd->ws.ib_sts2.value, (unsigned long)wd->ws.ib_dbg1, (unsigned long)wd->ws.m0, (unsigned long)wd->ws.mode.value);
			if (wd->ws.wave_status.halt || wd->ws.wave_status.fatal_halt) {
				for (x = 0; x < 112; x += 4)
					printf(">SGPRS[%u..%u] = { %08lx, %08lx, %08lx, %08lx }\n",
						(unsigned)(x),
						(unsigned)(x + 3),
						(unsigned long)wd->sgprs[x],
						(unsigned long)wd->sgprs[x+1],
						(unsigned long)wd->sgprs[x+2],
						(unsigned long)wd->sgprs[x+3]);


				if (wd->ws.wave_status.trap_en || wd->ws.wave_status.priv) {
					for (y = 0, x = 0x6C; x < (16 + 0x6C); x += 4) {
						printf(">TTMP[%u..%u] = { %08lx, %08lx, %08lx, %08lx }\n",
							(unsigned)(y),
							(unsigned)(y + 3),
							(unsigned long)wd->sgprs[x],
							(unsigned long)wd->sgprs[x+1],
							(unsigned long)wd->sgprs[x+2],
							(unsigned long)wd->sgprs[x+3]);

						// restart numbering on SI..VI with TTMP0
						y += 4;
						if (x == 0x6C && asic->family <= FAMILY_VI)
							y = 0;
					}
				}
			}
			if (ring_halted && (wd->ws.wave_status.halt || wd->ws.wave_status.fatal_halt)) {
				pgm_addr = (((uint64_t)wd->ws.pc_hi << 32) | wd->ws.pc_lo) - (NUM_OPCODE_WORDS*4)/2;
				umr_vm_disasm(asic, asic->options.vm_partition, wd->ws.hw_id2.vm_id, pgm_addr, (((uint64_t)wd->ws.pc_hi << 32) | wd->ws.pc_lo), NUM_OPCODE_WORDS*4, 0, NULL);
			}
		} else {
			first = 0;
			printf("\n------------------------------------------------------\nse%u.sa%u.wgp%u.simd%u.wave%u\n",
			(unsigned)wd->se, (unsigned)wd->sh, (unsigned)wd->cu, (unsigned)wd->ws.hw_id1.simd_id, (unsigned)wd->ws.hw_id1.wave_id);

			H("Main Registers");
			X(pc_hi);
			X(pc_lo);
			X(wave_inst_dw0);
			X(exec_hi);
			X(exec_lo);
			X(m0);
			X(ib_dbg1);

			Hv("Wave_Status", wd->ws.wave_status.value);
			PP(wave_status, scc);
			PP(wave_status, execz);
			PP(wave_status, vccz);
			PP(wave_status, in_tg);
			PP(wave_status, halt);
			PP(wave_status, valid);
			PP(wave_status, spi_prio);
			PP(wave_status, wave_prio);
			PP(wave_status, trap_en);
			PP(wave_status, ttrace_en);
			PP(wave_status, export_rdy);
			PP(wave_status, in_barrier);
			PP(wave_status, trap);
			PP(wave_status, ecc_err);
			PP(wave_status, skip_export);
			PP(wave_status, perf_en);
			PP(wave_status, cond_dbg_user);
			PP(wave_status, cond_dbg_sys);
			PP(wave_status, data_atc);
			PP(wave_status, inst_atc);
			PP(wave_status, dispatch_cache_ctrl);
			PP(wave_status, must_export);
			PP(wave_status, fatal_halt);
			PP(wave_status, ttrace_simd_en);

			Hv("HW_ID1", wd->ws.hw_id1.value);
			PP(hw_id1, wave_id);
			PP(hw_id1, simd_id);
			PP(hw_id1, wgp_id);
			PP(hw_id1, se_id);
			PP(hw_id1, sa_id);

			Hv("HW_ID2", wd->ws.hw_id2.value);
			PP(hw_id2, queue_id);
			PP(hw_id2, pipe_id);
			PP(hw_id2, me_id);
			PP(hw_id2, state_id);
			PP(hw_id2, wg_id);
			PP(hw_id2, compat_level);
			PP(hw_id2, vm_id);

			Hv("GPR_ALLOC", wd->ws.gpr_alloc.value);
			PP(gpr_alloc, vgpr_base);
			PP(gpr_alloc, vgpr_size);
			PP(gpr_alloc, sgpr_base);
			PP(gpr_alloc, sgpr_size);

			if (wd->ws.wave_status.halt || wd->ws.wave_status.fatal_halt) {
				printf("\n\nSGPRS:\n");
				for (x = 0; x < 112; x += 4)
					printf("\t[%4u..%4u] = { %08lx, %08lx, %08lx, %08lx }\n",
						(unsigned)(x),
						(unsigned)(x + 3),
						(unsigned long)wd->sgprs[x],
						(unsigned long)wd->sgprs[x+1],
						(unsigned long)wd->sgprs[x+2],
						(unsigned long)wd->sgprs[x+3]);

				if (wd->ws.wave_status.trap_en || wd->ws.wave_status.priv) {
					for (y  = 0, x = 0x6C; x < (16 + 0x6C); x += 4, y += 4) {
						// only print label once each
						if (y < 4)
							printf("TTMP: \n");
						printf("\t[%4u..%4u] = { %08lx, %08lx, %08lx, %08lx }\n",
							(unsigned)(y),
							(unsigned)(y + 3),
							(unsigned long)wd->sgprs[x],
							(unsigned long)wd->sgprs[x+1],
							(unsigned long)wd->sgprs[x+2],
							(unsigned long)wd->sgprs[x+3]);
					}
				}
			}

			if (wd->have_vgprs) {
				unsigned granularity = asic->parameters.vgpr_granularity;
				printf("\n");
				for (x = 0; x < ((wd->ws.gpr_alloc.vgpr_size + 1) << granularity); ++x) {
					if (x % 16 == 0) {
						if (x == 0)
							printf("VGPRS:       ");
						else
							printf("             ");
						for (thread = 0; thread < wd->num_threads; ++thread) {
							unsigned live = thread < 32 ? (wd->ws.exec_lo & (1u << thread))
											: (wd->ws.exec_hi & (1u << (thread - 32)));
							printf(live ? " t%02u     " : " (t%02u)   ", thread);
						}
						printf("\n");
					}

					printf("    [%3u] = {", x);
					for (thread = 0; thread < wd->num_threads; ++thread) {
						unsigned live = thread < 32 ? (wd->ws.exec_lo & (1u << thread))
										: (wd->ws.exec_hi & (1u << (thread - 32)));
						printf(" %s%08x%s", live ? BLUE : RST, wd->vgprs[thread * 256 + x], RST);
					}
					printf(" }\n");
				}

				// TODO: print shared VGPRs
			}

			if (ring_halted && (wd->ws.wave_status.halt || wd->ws.wave_status.fatal_halt)) {
				uint32_t shader_size = NUM_OPCODE_WORDS*4;
				printf("\n\nPGM_MEM:\n");
				pgm_addr = (((uint64_t)wd->ws.pc_hi << 32) | wd->ws.pc_lo);
				if (stream)
					shader = umr_find_shader_in_stream(stream, wd->ws.hw_id2.vm_id, pgm_addr);
				if (shader) {
					printf(" (found shader at: %s%u%s@0x%s%llx%s of %s%u%s bytes)\n",
						BLUE, shader->vmid, RST,
						YELLOW, (unsigned long long)shader->addr, RST,
						BLUE, shader->size, RST);

					// start decoding a bit before PC if possible
					if (!(asic->options.full_shader) && (shader->addr + ((NUM_OPCODE_WORDS*4)/2) < pgm_addr))
						pgm_addr -= (NUM_OPCODE_WORDS*4)/2;
					else
						pgm_addr = shader->addr;
					if (asic->options.full_shader)
						shader_size = shader->size;
					shader_addr = shader->addr;
					free(shader);
				} else {
					pgm_addr -= (NUM_OPCODE_WORDS*4)/2;
					shader_addr = pgm_addr;
					printf("\n");
				}
				umr_vm_disasm(asic, asic->options.vm_partition, wd->ws.hw_id2.vm_id, shader_addr, (((uint64_t)wd->ws.pc_hi << 32) | wd->ws.pc_lo), shader_size, pgm_addr - shader_addr, NULL);
			}

			Hv("LDS_ALLOC", wd->ws.lds_alloc.value);
			PP(lds_alloc, lds_base);
			PP(lds_alloc, lds_size);
			PP(lds_alloc, vgpr_shared_size);

			Hv("IB_STS", wd->ws.ib_sts.value);
			PP(ib_sts, vm_cnt);
			PP(ib_sts, exp_cnt);
			PP(ib_sts, lgkm_cnt);
			PP(ib_sts, valu_cnt);
			PP(ib_sts, vs_cnt);
			PP(ib_sts, replay_w64h);

			Hv("IB_STS2", wd->ws.ib_sts2.value);
			PP(ib_sts2, inst_prefetch);
			PP(ib_sts2, resource_override);
			PP(ib_sts2, mem_order);
			PP(ib_sts2, fwd_progress);
			PP(ib_sts2, wave64);
			PP(ib_sts2, wave64hi);
			PP(ib_sts2, subv_loop);

			Hv("TRAPSTS", wd->ws.trapsts.value);
			PP(trapsts, excp);
			PP(trapsts, illegal_inst);
			PP(trapsts, buffer_oob);
			PP(trapsts, excp_cycle);
			PP(trapsts, excp_wave64hi);
			PP(trapsts, xnack_error);
			PP(trapsts, dp_rate);
			PP(trapsts, excp_group_mask);
			PP(trapsts, utc_error);

			Hv("MODE", wd->ws.mode.value);
			PP(mode, fp_round);
			PP(mode, fp_denorm);
			PP(mode, dx10_clamp);
			PP(mode, ieee);
			PP(mode, lod_clamped);
			PP(mode, debug_en);
			PP(mode, excp_en);
			PP(mode, fp16_ovfl);
			PP(mode, disable_perf);

			printf("\n"); col = 0;
		}
		wd = wd->next;
	}

	if (first)
		printf("No active waves! (or GFXOFF was not disabled)\n");

	wd = owd;
	while (wd) {
		owd = wd->next;
		free(wd);
		wd = owd;
	}

	if (stream)
		umr_free_pm4_stream(stream);

	if (asic->options.halt_waves)
		umr_sq_cmd_halt_waves(asic, UMR_SQ_CMD_RESUME);
}

void umr_print_waves(struct umr_asic *asic)
{
	if (asic->family == FAMILY_NV)
		umr_print_waves_nv(asic);
	else if (asic->family <= FAMILY_AI)
		umr_print_waves_si_ai(asic);
}

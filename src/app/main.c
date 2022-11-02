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
#include <signal.h>
#include <time.h>
#include <stdarg.h>

static int quit;

void sigint(int signo)
{
	if (signo == SIGINT)
		quit = 1;
}

struct umr_options options;

static int std_printf(const char *fmt, ...)
{
	va_list ap;
	int r;

	va_start(ap, fmt);
	r = vfprintf(stdout, fmt, ap);
	fflush(stdout);
	va_end(ap);
	return r;
}

static int err_printf(const char *fmt, ...)
{
	va_list ap;
	int r;

	va_start(ap, fmt);
	r = vfprintf(stderr, fmt, ap);
	fflush(stderr);
	va_end(ap);
	return r;
}


static struct umr_asic *get_asic(void)
{
	struct umr_asic *asic;
	asic = umr_discover_asic(&options, std_printf);
	if (!asic) {
		printf("ASIC not found (instance=%d, did=%08lx)\n", options.instance, (unsigned long)options.forcedid);
		exit(EXIT_FAILURE);
	}
	umr_scan_config(asic, 1);

	// assign linux callbacks
	asic->err_msg = err_printf;
	asic->std_msg = std_printf;
	asic->mem_funcs.vm_message = std_printf;
	asic->mem_funcs.gpu_bus_to_cpu_address = umr_vm_dma_to_phys;
	asic->mem_funcs.access_sram = umr_access_sram;

	if (asic->options.use_pci == 0)
		asic->mem_funcs.access_linear_vram = umr_access_linear_vram;
	else
		asic->mem_funcs.access_linear_vram = umr_access_vram_via_mmio;

	asic->reg_funcs.read_reg = umr_read_reg;
	asic->reg_funcs.write_reg = umr_write_reg;

	asic->wave_funcs.get_wave_sq_info = umr_get_wave_sq_info;
	asic->wave_funcs.get_wave_status = umr_get_wave_status;
	asic->shader_disasm_funcs.disasm = umr_shader_disasm;

	asic->gpr_read_funcs.read_sgprs = umr_read_sgprs;
	asic->gpr_read_funcs.read_vgprs = umr_read_vgprs;

	// default shader options
	if (asic->family <= FAMILY_VI) { // on gfx9+ hs/gs are opaque
		asic->options.shader_enable.enable_gs_shader = 1;
		asic->options.shader_enable.enable_hs_shader = 1;
	}
	asic->options.shader_enable.enable_vs_shader   = 1;
	asic->options.shader_enable.enable_ps_shader   = 1;
	asic->options.shader_enable.enable_es_shader   = 1;
	asic->options.shader_enable.enable_ls_shader   = 1;
	asic->options.shader_enable.enable_comp_shader = 1;

	if (asic->family > FAMILY_VI)
		asic->options.shader_enable.enable_es_ls_swap = 1;  // on >FAMILY_VI we swap LS/ES for HS/GS

	umr_create_mmio_accel(asic);

	return asic;
}

// returns blockname supports
// asicname.blockname
// *.blockname
// blockname
static char *get_block_name(struct umr_asic *asic, char *path)
{
	static char asicname[256], block[256], *dot;

	memset(asicname, 0, sizeof asicname);
	memset(block, 0, sizeof block);
	if ((dot = strstr(path, "."))) {
		memcpy(asicname, path, (int)(dot - path));
		strcpy(block, dot + 1);
	} else {
		strcpy(block, path);
	}

	if (asicname[0] && asicname[0] != '*' && strcmp(asic->asicname, asicname)) {
		printf("[ERROR]: Invalid asicname <%s>\n", asicname);
		return NULL;
	}
	return block;
}

static void parse_options(char *str)
{
	char option[64], *p;

	while (*str) {
		p = &option[0];
		while (*str && *str != ',' && p != &option[sizeof(option)-1])
			*p++ = *str++;
		*p = 0;
		if (*str == ',')
			++str;
		if (!strcmp(option, "bits")) {
			options.bitfields = 1;
		} else if (!strcmp(option, "empty_log")) {
			options.empty_log = 1;
		} else if (!strcmp(option, "follow")) {
			options.follow = 1;
		} else if (!strcmp(option, "use_pci")) {
			options.use_pci = 1;
		} else if (!strcmp(option, "use_colour") || !strcmp(option, "use_color")) {
			options.use_colour = 1;
		} else if (!strcmp(option, "bitsfull")) {
			options.bitfields = 1;
			options.bitfields_full = 1;
		} else if (!strcmp(option, "read_smc")) {
			options.read_smc = 1;
		} else if (!strcmp(option, "quiet")) {
			options.quiet = 1;
		} else if (!strcmp(option, "full_shader")) {
			options.full_shader = 1;
		} else if (!strcmp(option, "no_follow_ib")) {
			options.no_follow_ib = 1;
			options.no_follow_shader = 1;
		} else if (!strcmp(option, "verbose")) {
			options.verbose = 1;
		} else if (!strcmp(option, "halt_waves")) {
			options.halt_waves = 1;
		} else if (!strcmp(option, "wave64")) {
			options.wave64 = 1;
		} else if (!strcmp(option, "disasm_early_term")) {
			options.disasm_early_term = 1;
		} else if (!strcmp(option, "no_kernel")) {
			options.no_kernel = 1;
			options.use_pci = 1;
		} else if (!strcmp(option, "no_disasm")) {
			options.no_disasm = 1;
		} else if (!strcmp(option, "disasm_anyways")) {
			options.disasm_anyways = 1;
		} else if (!strcmp(option, "no_fold_vm_decode")) {
			options.no_fold_vm_decode = 1;
		} else if (!strcmp(option, "no_scan_waves")) {
			options.no_scan_waves = 1;
		} else if (!strcmp(option, "force_asic_file")) {
			options.force_asic_file = 1;
		} else if (!strcmp(option, "export_model")) {
			options.export_model = 1;
		} else {
			printf("error: Unknown option [%s]\n", option);
			exit(EXIT_FAILURE);
		}
	}
}

#define MIN(x, y) ((x) < (y) ? (x) : (y))

int main(int argc, char **argv)
{
	int i, j, k, l;
	struct umr_asic *asic;
	char *blockname, *str, *str2, asicname[256], ipname[256], regname[256], clockperformance[256];
	struct timespec req;
	struct umr_test_harness *th = NULL;
#if UMR_GUI
	int running_as_gui = 0;
	char *guiurl = NULL;

	if (strstr(argv[0], "umrgui"))
		running_as_gui = 1;
#endif

	memset(&options, 0, sizeof options);

	/* defaults */
	asic = NULL;
	options.need_scan = 1;
	options.forcedid = -1;
	options.scanblock = "";
	options.vm_partition = -1;

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--database-path") || !strcmp(argv[i], "-dbp")) {
			if (i + 1 < argc) {
				strcpy(options.database_path, argv[i+1]);
				++i;
			} else {
				fprintf(stderr, "[ERROR]: --database-path requires at least one parameter\n");
				return EXIT_FAILURE;
			}
		} else if (!strcmp(argv[i], "--gpu") || !strcmp(argv[i], "-g")) {
			if (i + 1 < argc) {
				char *s;
				s = strstr(argv[i+1], "@");
				if (s) {
					strncpy(options.dev_name, argv[i+1], MIN(sizeof(options.dev_name), (unsigned)(s - argv[i+1])));
					options.instance = atoi(s + 1);
					asic = get_asic();
				} else if ((s = strstr(argv[i+1], "="))) {
					strncpy(options.dev_name, argv[i+1], MIN(sizeof(options.dev_name), (unsigned)(s - argv[i+1])));
					sscanf(s + 1, "%04x:%02x:%02x.%01x", &options.pci.domain, &options.pci.bus, &options.pci.slot, &options.pci.func);
					options.use_pci = 1;
					asic = get_asic();
				} else {
					fprintf(stderr, "[ERROR]: Invalid syntax for option --gpu\n");
					return EXIT_FAILURE;
				}
				++i;
			} else {
				fprintf(stderr, "[ERROR]: --gpu requires a parameter\n");
				return EXIT_FAILURE;
			}
		} else if (!strcmp(argv[i], "--instance") || !strcmp(argv[i], "-i")) {
			if (i + 1 < argc) {
				options.instance = atoi(argv[i+1]);
				++i;
			} else {
				fprintf(stderr, "[ERROR]: --instance requires a number\n");
				return EXIT_FAILURE;
			}
		} else if (!strcmp(argv[i], "--vm_partition") || !strcmp(argv[i], "-vmp")) {
			if (i + 1 < argc) {
				options.vm_partition = atoi(argv[i+1]);
				++i;
			} else {
				fprintf(stderr, "[ERROR]: --vm_partition requires a number\n");
				return EXIT_FAILURE;
			}
		} else if (!strcmp(argv[i], "--bank") || !strcmp(argv[i], "-b")) {
			if (!asic)
				asic = get_asic();
			if (i + 3 < argc) {
				options.bank.grbm.se = argv[i+1][0] == 'x' ? 0xFFFFFFFFUL : (uint32_t)atoi(argv[i+1]);
				options.bank.grbm.sh = argv[i+2][0] == 'x' ? 0xFFFFFFFFUL : (uint32_t)atoi(argv[i+2]);
				options.bank.grbm.instance = argv[i+3][0] == 'x' ? 0xFFFFFFFFUL : (uint32_t)atoi(argv[i+3]);
				if (!options.no_kernel) {
					if ((options.bank.grbm.se != 0xFFFFFFFFUL && options.bank.grbm.se >= asic->config.gfx.max_shader_engines) ||
						(options.bank.grbm.sh != 0xFFFFFFFFUL && options.bank.grbm.sh >= asic->config.gfx.max_sh_per_se)) {
						printf("Invalid bank selection for specific ASIC\n");
						return EXIT_FAILURE;
					}
				}
				options.use_bank = 1;
				i += 3;
				asic->options = options;
			} else {
				fprintf(stderr, "[ERROR]: --bank requires three parameters\n");
				return EXIT_FAILURE;
			}
		} else if (!strcmp(argv[i], "--sbank") || !strcmp(argv[i], "-sb")) {
			if (!asic)
				asic = get_asic();
			if (i + 3 < argc) {
				options.bank.srbm.me = atoi(argv[i+1]);
				options.bank.srbm.pipe = atoi(argv[i+2]);
				options.bank.srbm.queue = atoi(argv[i+3]);
				if (i + 4 < argc && sscanf(argv[i+4], "%u", &options.bank.srbm.vmid) == 1)
					++i;
				else
					options.bank.srbm.vmid = 0;  // default
				options.use_bank = 2;
				i += 3;
				asic->options = options;
			} else {
				fprintf(stderr, "[ERROR]: --sbank requires three parameters\n");
				return EXIT_FAILURE;
			}
		} else if (!strcmp(argv[i], "--cbank") || !strcmp(argv[i], "-cb")) {
			if (!asic)
				asic = get_asic();
			if (i + 1 < argc) {
				options.context_reg_bank = atoi(argv[i+1]);
				++i;
				asic->options = options;
			} else {
				fprintf(stderr, "[ERROR]: --cbank requires one parameters\n");
				return EXIT_FAILURE;
			}
		} else if (!strcmp(argv[i], "--force") || !strcmp(argv[i], "-f")) {
			if (i + 1 < argc) {
				unsigned long did = 0;
				if (sscanf(argv[i+1], "0x%lx", &did) == 0) {
					strncpy(options.dev_name, argv[i+1], sizeof(options.dev_name) - 1);
				}
				options.forcedid = did;
				options.instance = -1;
				++i;
			} else {
				fprintf(stderr, "[ERROR]: --force requires a number/name\n");
				return EXIT_FAILURE;
			}
		} else if (!strcmp(argv[i], "--pci")) {
			if (i + 1 < argc && sscanf(argv[i+1], "%04x:%02x:%02x.%01x",
				&options.pci.domain, &options.pci.bus, &options.pci.slot,
				&options.pci.func ) >= 4) {
				options.use_pci = 1; // implied by the --pci option
				++i;
			} else {
				fprintf(stderr, "[ERROR]: --pci requires domain:bus:slot.function\n");
				return EXIT_FAILURE;
			}
		} else if (!strcmp(argv[i], "--config") || !strcmp(argv[i], "-c")) {
			if (!asic)
				asic = get_asic();
			umr_apply_callbacks(asic, &asic->mem_funcs, &asic->reg_funcs);
			umr_print_config(asic);
		} else if (!strcmp(argv[i], "--list-blocks") || !strcmp(argv[i], "-lb")) {
			if (!asic)
				asic = get_asic();
			for (j = 0; j < asic->no_blocks; j++) {
				printf("\t%s.%s (%d.%d.%d)\n", asic->asicname, asic->blocks[j]->ipname, asic->blocks[j]->discoverable.maj,
				asic->blocks[j]->discoverable.min, asic->blocks[j]->discoverable.rev);
			}
		} else if (!strcmp(argv[i], "--list-regs") || !strcmp(argv[i], "-lr")) {
			if (i + 1 < argc) {
				if (!asic)
					asic = get_asic();
				blockname = get_block_name(asic, argv[i+1]);
				if (!blockname)
					return EXIT_FAILURE;
				for (j = 0; j < asic->no_blocks; j++)
					if (!strcmp(asic->blocks[j]->ipname, blockname))
						for (k = 0; k < asic->blocks[j]->no_regs; k++) {
							printf("\t%s.%s.%s (%d) => 0x%05lx\n", asic->asicname, asic->blocks[j]->ipname, asic->blocks[j]->regs[k].regname, (int)asic->blocks[j]->regs[k].type, (unsigned long)asic->blocks[j]->regs[k].addr);
							if (options.bitfields) {
								for (l = 0; l < asic->blocks[j]->regs[k].no_bits; l++)
									printf("\t\t%s.%s.%s.%s[%d:%d]\n", asic->asicname, asic->blocks[j]->ipname, asic->blocks[j]->regs[k].regname, asic->blocks[j]->regs[k].bits[l].regname, asic->blocks[j]->regs[k].bits[l].start, asic->blocks[j]->regs[k].bits[l].stop);
							}
						}
				++i;
			} else {
				fprintf(stderr, "[ERROR]: --list-regs requires one parameter\n");
				return EXIT_FAILURE;
			}
		} else if (!strcmp(argv[i], "--lookup") || !strcmp(argv[i], "-lu")) {
			if (i + 2 < argc) {
				int tmp = options.bitfields;
				if (!asic)
					asic = get_asic();
				options.bitfields = 1;
				umr_lookup(asic, argv[i+1], argv[i+2]);
				options.bitfields = tmp;
				i += 2;
			}
		} else if (!strcmp(argv[i], "--write") || !strcmp(argv[i], "-w")) {
			if (i + 2 < argc) {
				uint32_t reg, val;

				if (!asic)
					asic = get_asic();
				if (!memcmp(argv[i+1], "0x", 2) && sscanf(argv[i+1], "%"SCNx32, &reg) == 1 && sscanf(argv[i+2], "%"SCNx32, &val) == 1)
					umr_write_reg(asic, reg, val, REG_MMIO);
				else
					umr_set_register(asic, argv[i+1], argv[i+2]);
				i += 2;
				options.need_scan = 0;
			} else {
				fprintf(stderr, "[ERROR]: --write requires two parameters\n");
				return EXIT_FAILURE;
			}
		} else if (!strcmp(argv[i], "--writebit") || !strcmp(argv[i], "-wb")) {
			if (i + 2 < argc) {
				if (!asic)
					asic = get_asic();
				umr_set_register_bit(asic, argv[i+1], argv[i+2]);
				i += 2;
				options.need_scan = 0;
			} else {
				fprintf(stderr, "[ERROR]: --write requires two parameters\n");
				return EXIT_FAILURE;
			}
		} else if (!strcmp(argv[i], "--waves") || !strcmp(argv[i], "-wa")) {
			if (!asic)
				asic = get_asic();
			if (i + 1 < argc) {
				if (argv[i+1][0] != '-') {
					strcpy(asic->options.ring_name, argv[i+1]);
					++i;
				}
			}
			umr_print_waves(asic);
		} else if (!strcmp(argv[i], "--scan") || !strcmp(argv[i], "-s")) {
			if (i + 1 < argc) {
				if (!asic)
					asic = get_asic();
				blockname = get_block_name(asic, argv[i+1]);
				if (!blockname)
					return EXIT_FAILURE;
				if (!umr_scan_asic(asic, "", blockname, ""))
					umr_print_asic(asic, blockname);
				++i;
				options.need_scan = 0;
			} else {
				fprintf(stderr, "[ERROR]: --scan requires one parameter\n");
				return EXIT_FAILURE;
			}
		} else if (!strcmp(argv[i], "--read") || !strcmp(argv[i], "-r")) {
			if (i + 1 < argc) {
				uint32_t reg;

				if (!asic)
					asic = get_asic();

				if (!memcmp(argv[i+1], "0x", 2) && sscanf(argv[i+1], "%"SCNx32, &reg) == 1) {
					printf("0x%08lx\n", (unsigned long)asic->reg_funcs.read_reg(asic, reg, REG_MMIO));
				} else {
					str = strstr(argv[i+1], ".");
					str2 = str ? strstr(str+1, ".") : NULL;
					if (str && str2) {
						memset(asicname, 0, sizeof asicname);
						memset(ipname, 0, sizeof ipname);
						memset(regname, 0, sizeof regname);
						str[0] = 0;
						str2[0] = 0;
						strcpy(asicname, argv[i+1]);
						strcpy(ipname, str+1);
						strcpy(regname, str2+1);
					} else {
						fprintf(stderr, "[ERROR]: Invalid asicname.ipname.regname syntax\n");
						return EXIT_FAILURE;
					}
					umr_scan_asic(asic, asicname, ipname, regname);
					options.need_scan = 0;
				}
				++i;
			} else {
				fprintf(stderr, "[ERROR]: --read requires one parameter\n");
				return EXIT_FAILURE;
			}
		} else if (!strcmp(argv[i], "--ring") || !strcmp(argv[i], "-R")) {
			if (i + 1 < argc) {
				if (!asic)
					asic = get_asic();
				fprintf(stderr, "[WARNING]: The --ring command is deprecated and will be removed in a future release.  Please use the --ring-stream command.\n");
				umr_read_ring(asic, argv[i+1]);
				++i;
			} else {
				fprintf(stderr, "[ERROR]: --ring requires one parameter\n");
				return EXIT_FAILURE;
			}
		} else if (!strcmp(argv[i], "--ring-stream") || !strcmp(argv[i], "-RS")) {
			if (i + 1 < argc) {
				if (!asic)
					asic = get_asic();
				umr_read_ring_stream(asic, argv[i+1]);
				++i;
			} else {
				fprintf(stderr, "[ERROR]: --ring-stream requires one parameter\n");
				return EXIT_FAILURE;
			}
		} else if (!strcmp(argv[i], "--dump-ib") || !strcmp(argv[i], "-di")) {
			if (i + 2 < argc) {
				uint64_t address;
				uint32_t vmid, len;
				int pm;
				char str[128];
				char prefix[] = { ' ', ' ', 'M', 'S', 'P' };

				if (!asic)
					asic = get_asic();

				if (sscanf(argv[i+1], "0x%"SCNx32"@%"SCNx64, &vmid, &address) != 2)
					if (sscanf(argv[i+1], "%"SCNu32"@%"SCNx64, &vmid, &address) != 2) {
						sscanf(argv[i+1], "%"SCNx64, &address);
						vmid = UMR_LINEAR_HUB;
					}

				if (sscanf(argv[i+2], "0x%"SCNx32, &len) != 1)
					sscanf(argv[i+2], "%"SCNu32, &len);

				if ((i + 3 < argc) && sscanf(argv[i+3], "%d", &pm) == 1) {
					i += 3;
				} else {
					pm = 4;
					i += 2;
				}
				sprintf(str, "%c0x%"PRIx32"@0x%"PRIx64".0x%"PRIu32, prefix[pm], vmid, address, len);
				umr_read_ring_stream(asic, str);
			} else {
				fprintf(stderr, "[ERROR]: --dump-ib requires three parameters\n");
				return EXIT_FAILURE;
			}
		} else if (!strcmp(argv[i], "--dump-ib-file") || !strcmp(argv[i], "-df")) {
			if (i + 1 < argc) {
				int pm;
				char *name = argv[i+1];

				if (!asic)
					asic = get_asic();

				if ((i + 2 < argc) && sscanf(argv[i+2], "%d", &pm) == 1) {
					i += 2;
				} else {
					pm = 4;
					i += 1;
				}
				umr_ib_read_file(asic, name, pm);
			} else {
				fprintf(stderr, "[ERROR]: --dump-ib-file requires two parameters\n");
				return EXIT_FAILURE;
			}
		} else if (!strcmp(argv[i], "--logscan") || !strcmp(argv[i], "-ls")) {
			if (!asic)
				asic = get_asic();
			if (options.follow) {
				int r;

				signal(SIGINT, sigint);
				r = system("echo 1 > /sys/kernel/debug/tracing/events/amdgpu/amdgpu_mm_wreg/enable");
				r |= system("echo 1 > /sys/kernel/debug/tracing/events/amdgpu/amdgpu_mm_rreg/enable");
				if (r) {
					fprintf(stderr, "[ERROR]: Could not enable mm tracers\n");
					return EXIT_FAILURE;
				}
				req.tv_sec = 0;
				req.tv_nsec = 1000000000/10; // 100ms
				while (!quit) {
					nanosleep(&req, NULL);
					umr_scan_log(asic);
				}
				r = system("echo 0 > /sys/kernel/debug/tracing/events/amdgpu/amdgpu_mm_wreg/enable");
				r |= system("echo 0 > /sys/kernel/debug/tracing/events/amdgpu/amdgpu_mm_rreg/enable");
				if (r) {
					fprintf(stderr, "[ERROR]: Could not diable mm tracers\n");
					return EXIT_FAILURE;
				}
			} else {
				umr_scan_log(asic);
			}
		} else if (!strcmp(argv[i], "--top") || !strcmp(argv[i], "-t")) {
			uint32_t value;
			if (!asic)
				asic = get_asic();
			value = 0;
			if (asic->fd.gfxoff >= 0)
				write(asic->fd.gfxoff, &value, sizeof(value));
			umr_top(asic);
			value = 1;
			if (asic->fd.gfxoff >= 0)
				write(asic->fd.gfxoff, &value, sizeof(value));
		} else if (!strcmp(argv[i], "--enumerate") || !strcmp(argv[i], "-e")) {
			umr_enumerate_devices(std_printf);
			return 0;
		} else if (!strcmp(argv[i], "-mm")) {
			if (i + 1 < argc) {
				strcpy(options.hub_name, argv[i+1]);
				++i;
			} else {
				fprintf(stderr, "[ERROR]: -mm requires on parameter");
				return EXIT_FAILURE;
			}
		} else if (!strcmp(argv[i], "--vm-decode") || !strcmp(argv[i], "-vm")) {
			if (i + 2 < argc) {
				uint64_t address;
				uint32_t size, n, vmid;
				int overbose;

				if (!asic)
					asic = get_asic();

				overbose = asic->options.verbose;
				asic->options.verbose = 1;

				// allow specifying the vmid in hex as well so
				// people can add the HUB flags more easily
				if ((n = sscanf(argv[i+1], "0x%"SCNx32"@%"SCNx64, &vmid, &address)) != 2)
					if ((n = sscanf(argv[i+1], "%"SCNu32"@%"SCNx64, &vmid, &address)) != 2) {
						fprintf(stderr, "[ERROR]: Must specify a VMID for the --vm-decode command\n");
						exit(EXIT_FAILURE);
					}
				sscanf(argv[i+2], "%"SCNx32, &size);

				// imply user hub if hub name specified
				if (options.hub_name[0])
					vmid |= UMR_USER_HUB;

				umr_read_vram(asic, asic->options.vm_partition, vmid, address, 0x1000UL * size, NULL);
				i += 2;

				asic->options.verbose = overbose;
			} else {
				fprintf(stderr, "[ERROR]: --vm-decode requires two parameters\n");
				return EXIT_FAILURE;
			}
		} else if (!strcmp(argv[i], "-vr") || !strcmp(argv[i], "--vm-read")) {
			if (i + 2 < argc) {
				unsigned char buf[256];
				uint64_t address;
				uint32_t size, n, vmid;

				if (!asic)
					asic = get_asic();

				// allow specifying the vmid in hex as well so
				// people can add the HUB flags more easily
				if ((n = sscanf(argv[i+1], "0x%"SCNx32"@%"SCNx64, &vmid, &address)) != 2)
					if ((n = sscanf(argv[i+1], "%"SCNu32"@%"SCNx64, &vmid, &address)) != 2) {
						sscanf(argv[i+1], "%"SCNx64, &address);
						vmid = UMR_LINEAR_HUB;
					}

				// imply user hub if hub name specified
				if (options.hub_name[0])
					vmid |= UMR_USER_HUB;

				sscanf(argv[i+2], "%"SCNx32, &size);
				do {
					n = size > sizeof(buf) ? sizeof(buf) : size;
					if (umr_read_vram(asic, asic->options.vm_partition, vmid, address, n, buf))
						return EXIT_FAILURE;
					fwrite(buf, 1, n, stdout);
					size -= n;
					address += n;
				} while (size);
				i += 2;
			} else {
				fprintf(stderr, "[ERROR]: --vm-read requires two parameters\n");
				return EXIT_FAILURE;
			}
		} else if (!strcmp(argv[i], "-vw") || !strcmp(argv[i], "--vm-write")) {
			if (i + 2 < argc) {
				unsigned char buf[256];
				uint64_t address;
				uint32_t size, n, vmid;

				if (!asic)
					asic = get_asic();

				// allow specifying the vmid in hex as well so
				// people can add the HUB flags more easily
				if ((n = sscanf(argv[i+1], "0x%"SCNx32"@%"SCNx64, &vmid, &address)) != 2)
					if ((n = sscanf(argv[i+1], "%"SCNu32"@%"SCNx64, &vmid, &address)) != 2) {
						sscanf(argv[i+1], "%"SCNx64, &address);
						vmid = UMR_LINEAR_HUB;
					}

				// imply user hub if hub name specified
				if (options.hub_name[0])
					vmid |= UMR_USER_HUB;

				sscanf(argv[i+2], "%"SCNx32, &size);
				do {
					n = size > sizeof(buf) ? sizeof(buf) : size;
					n = fread(buf, 1, n, stdin);
					if (umr_write_vram(asic, asic->options.vm_partition, vmid, address, n, buf))
						return EXIT_FAILURE;
					size -= n;
					address += n;
				} while (size);
				i += 2;
			} else {
				fprintf(stderr, "[ERROR]: --vm-write requires two parameters\n");
				return EXIT_FAILURE;
			}
		} else if (!strcmp(argv[i], "-vww") || !strcmp(argv[i], "--vm-write-word")) {
			if (i + 2 < argc) {
				uint64_t address;
				uint32_t n, data, vmid;

				if (!asic)
					asic = get_asic();

				// allow specifying the vmid in hex as well so
				// people can add the HUB flags more easily
				if ((n = sscanf(argv[i+1], "0x%"SCNx32"@%"SCNx64, &vmid, &address)) != 2)
					if ((n = sscanf(argv[i+1], "%"SCNu32"@%"SCNx64, &vmid, &address)) != 2) {
						sscanf(argv[i+1], "%"SCNx64, &address);
						vmid = UMR_LINEAR_HUB;
					}

				// imply user hub if hub name specified
				if (options.hub_name[0])
					vmid |= UMR_USER_HUB;

				sscanf(argv[i+2], "%"SCNx32, &data);
				if (umr_write_vram(asic, asic->options.vm_partition, vmid, address, 4, &data))
					return EXIT_FAILURE;
				i += 2;
			} else {
				fprintf(stderr, "[ERROR]: --vm-write-word requires two parameters\n");
				return EXIT_FAILURE;
			}

		} else if (!strcmp(argv[i], "-vdis") || !strcmp(argv[i], "--vm-disasm")) {
			if (i + 2 < argc) {
				uint64_t address;
				uint32_t size, n, vmid;

				if (!asic)
					asic = get_asic();

				// allow specifying the vmid in hex as well so
				// people can add the HUB flags more easily
				if ((n = sscanf(argv[i+1], "0x%"SCNx32"@%"SCNx64, &vmid, &address)) != 2)
					if ((n = sscanf(argv[i+1], "%"SCNu32"@%"SCNx64, &vmid, &address)) != 2) {
						sscanf(argv[i+1], "%"SCNx64, &address);
						vmid = UMR_LINEAR_HUB;
					}

				// imply user hub if hub name specified
				if (options.hub_name[0])
					vmid |= UMR_USER_HUB;

				sscanf(argv[i+2], "%"SCNx32, &size);
				if (!size) {
					struct umr_shaders_pgm shader;
					shader.vmid = vmid;
					shader.addr = address;
					size = umr_compute_shader_size(asic, asic->options.vm_partition, &shader);
				}
				umr_vm_disasm(asic, asic->options.vm_partition, vmid, address, 0, size, 0, NULL);

				i += 2;
			} else {
				fprintf(stderr, "[ERROR]: --vm-disasm requires two parameters\n");
				return EXIT_FAILURE;
			}
		} else if (!strcmp(argv[i], "-prof") || !strcmp(argv[i], "--profiler")) {
			if (i + 1 < argc) {
				int n = 0, samples = -1, type = -1;
				if (!asic)
					asic = get_asic();
				if (i + 2 < argc && argv[i+2][0] != '-') {
					n = 1;
					strcpy(asic->options.ring_name, argv[i+2]);
				}
				if (sscanf(argv[i+1], "pixel=%d", &samples) == 1)
					type = 0;
				else if (sscanf(argv[i+1], "vertex=%d", &samples) == 1)
					type = 1;
				else if (sscanf(argv[i+1], "compute=%d", &samples) == 1)
					type = 2;
				else
					samples = atoi(argv[i+1]);
				umr_profiler(asic, samples, type);
				i += 1 + n;
			} else {
				fprintf(stderr, "[ERROR]: --profiler requires one parameter\n");
				return EXIT_FAILURE;
			}
		} else if (!strcmp(argv[i], "--option") || !strcmp(argv[i], "-O")) {
			if (asic)
				options = asic->options;
			if (i + 1 < argc) {
				parse_options(argv[i+1]);
				++i;
			} else {
				fprintf(stderr, "[ERROR]: --option requires one parameter\n");
				return EXIT_FAILURE;
			}
			if (asic)
				asic->options = options;
		} else if (!strcmp(argv[i], "--header-dump") || !strcmp(argv[i], "-hd")) {
			if (!asic)
				asic = get_asic();
			if (i + 1 < argc) {
				int n;
				for (n = 0; n < 8; n++) {
					uint32_t v = umr_read_reg_by_name(asic, argv[i+1]);
					printf("\t[0x%08" PRIx32"] %s\n", v, umr_pm4_opcode_to_str(v));
				}
				++i;
			} else {
				fprintf(stderr, "[ERROR]: --header-dump requires one parameter\n");
				return EXIT_FAILURE;
			}
		} else if (!strcmp(argv[i], "--gfxoff") || !strcmp(argv[i], "-go")) {
			uint32_t value;
			if (!asic)
				asic = get_asic();
			if (i + 1 < argc) {
				sscanf(argv[i+1], "%"SCNu32, &value);
				if (asic->fd.gfxoff >= 0)
					write(asic->fd.gfxoff, &value, sizeof(value));
				else
					fprintf(stderr, "[ERROR]: amdgpu_gfxoff file not present please update your kernel\n");
				++i;
			} else {
				if (asic->fd.gfxoff >= 0)
					umr_gfxoff_read(asic);
				else
					fprintf(stderr, "[ERROR]: amdgpu_gfxoff file not present please update your kernel\n");
			}
		} else if (!strcmp(argv[i], "--power") || !strcmp(argv[i], "-p")) {
			if (!asic)
				asic = get_asic();
			umr_power(asic);
		} else if (!strcmp(argv[i], "--clock-scan") || !strcmp(argv[i], "-cs")) {
			if (!asic)
				asic = get_asic();
			if (i + 1 < argc) {
				umr_clock_scan(asic, argv[i+1]);
				i++;
			} else {
				umr_clock_scan(asic, NULL);
			}
		} else if (!strcmp(argv[i], "--clock-manual") || !strcmp(argv[i], "-cm")) {
			if (!asic)
				asic = get_asic();
			if (i + 1 < argc) {
				umr_clock_manual(asic, argv[i+1], argv[i+2]);
				i += 2;
			} else {
				umr_set_clock_performance(asic, "manual");
				if (umr_check_clock_performance(asic, clockperformance, sizeof(clockperformance)) != 0)
					printf("power_dpm_force_performance_level: %s", clockperformance);
			}
		} else if (!strcmp(argv[i], "--clock-high") || !strcmp(argv[i], "-ch")) {
			if (!asic)
				asic = get_asic();
			umr_set_clock_performance(asic, "high");
			if (umr_check_clock_performance(asic, clockperformance, sizeof(clockperformance)) != 0)
				printf("power_dpm_force_performance_level: %s", clockperformance);
		} else if (!strcmp(argv[i], "--clock-low") || !strcmp(argv[i], "-cl")) {
			if (!asic)
				asic = get_asic();
			umr_set_clock_performance(asic, "low");
			if (umr_check_clock_performance(asic, clockperformance, sizeof(clockperformance)) != 0)
				printf("power_dpm_force_performance_level: %s", clockperformance);
		} else if (!strcmp(argv[i], "--clock-auto") || !strcmp(argv[i], "-ca")) {
			if (!asic)
				asic = get_asic();
			umr_set_clock_performance(asic, "auto");
			if (umr_check_clock_performance(asic, clockperformance, sizeof(clockperformance)) != 0)
				printf("power_dpm_force_performance_level: %s", clockperformance);
		} else if (!strcmp(argv[i], "--ppt-read") ||
			   !strcmp(argv[i], "--ppt_read") ||
			   !strcmp(argv[i], "-pptr")) {
			if (!asic)
				asic = get_asic();
			if (i + 1 < argc) {
				if (umr_print_pp_table(asic, argv[i+1]) != 0)
					fprintf(stderr, "[ERROR]: can not print pp table info.\n");
				i++;
			} else {
				if (umr_print_pp_table(asic, NULL) != 0)
					fprintf(stderr, "[ERROR]: can not print pp table info.\n");
			}
		} else if (!strcmp(argv[i], "--gpu-metrics") ||
			   !strcmp(argv[i], "--gpu_metrics") ||
			   !strcmp(argv[i], "-gm")) {
			if (!asic)
				asic = get_asic();
			if (umr_print_gpu_metrics(asic) != 0)
				fprintf(stderr, "[ERROR]: Cannot print pp table info.\n");
#if 0
		} else if (!strcmp(argv[i], "--iv")) {
			if (!asic)
				asic = get_asic();
			ih_self_test(asic);
#endif
		} else if (!strcmp(argv[i], "--vbios_info") || !strcmp(argv[i], "-vi")) {
			if (!asic)
				asic = get_asic();
			if (umr_print_vbios_info(asic) != 0)
				fprintf(stderr, "[ERROR]: Cannot print vbios info.\n");
		} else if (!strcmp(argv[i], "--test-log") || !strcmp(argv[i], "-tl")) {
			if (i + 1 < argc) {
				options.test_log_fd = fopen(argv[i + 1], "w");
				options.test_log = 1;
				if (!asic)
					asic = get_asic();
				umr_scan_config(asic, 0);
				++i;
			} else {
				fprintf(stderr, "[ERROR]: --test-log requires one parameter\n");
				return EXIT_FAILURE;
			}
		} else if (!strcmp(argv[i], "--test-harness") || !strcmp(argv[i], "-th")) {
			if (i + 1 < argc) {
				th = umr_create_test_harness_file(argv[i + 1]);
				options.th = th;
				options.test_log = 1;
				options.test_log_fd = NULL;
				if (!asic)
					asic = get_asic();
				umr_attach_test_harness(th, asic);
				umr_scan_config(asic, 0);
				++i;
			} else {
				fprintf(stderr, "[ERROR]: --test-harness requires one parameter\n");
				return EXIT_FAILURE;
			}
		} else if (!strcmp(argv[i], "--dump-discovery-table") || !strcmp(argv[i], "-ddt")) {
			asic = asic ? asic: get_asic();
			umr_dump_discovery_table_info(asic, NULL);
                } else if (!strcmp(argv[i], "--print-cpc") || !strcmp(argv[i], "-cpc")) {
                        if (!asic)
                                asic = get_asic();
                        umr_print_cpc(asic);
                } else if (!strcmp(argv[i], "--print-sdma") || !strcmp(argv[i], "-sdma")) {
                        if (!asic)
                                asic = get_asic();
                        umr_print_sdma(asic);
		} else if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
			printf("User Mode Register debugger v%s for AMDGPU devices (build: %s [%s], date: %s), Copyright (c) 2022, AMD Inc.\n"
"\n*** Device Selection ***\n"
"\n\t--database-path, -dbp <path>"
	"\n\t\tSpecify a database path for register, ip, and asic model data.\n"
"\n\t--option -O <string>[,<string>,...]\n\t\tEnable various flags:"
	"\n\t\t\tbits, bitsfull, empty_log, follow, no_follow_ib,"
	"\n\t\t\tuse_pci, use_colour, read_smc, quiet, no_kernel, verbose, halt_waves,"
	"\n\t\t\tdisasm_early_term, no_disasm, disasm_anyways, wave64, full_shader, no_fold_vm_decode, no_scan_waves, force_asic_file\n"
"\n\t--gpu, -g <asicname>(@<instance> | =<pcidevice>)"
	"\n\t\tSelect a gpu by ASIC name and either the instance number or the PCI bus identifier.\n"
"\n\t--instance, -i <number>\n\t\tSelect a device instance to investigate. (default: 0)"
	"\n\t\tThe instance is the directory name under /sys/kernel/debug/dri/"
	"\n\t\tof the card you want to work with.\n"
"\n\t--force, -f <number>\n\t\tForce a PCIE DID number in hex or asic name.  Useful if amdgpu is"
	"\n\t\tnot loaded or a display is not attached.\n"
"\n\t--pci <device>"
	"\n\t\tForce a specific PCI device using the domain:bus:slot.function format in hex."
	"\n\t\tThis is useful when more than one GPU is available. If the amdgpu driver is"
	"\n\t\tloaded the corresponding instance will be automatically detected.\n"
"\n\t--gfxoff, -go <0 | 1>"
	"\n\t\tEnable GFXOFF with a non-zero value or disable with a 0.  Used to control the GFXOFF feature on"
	"\n\t\tselect hardware. Command without parameter will check GFXOFF status.\n"
"\n\t--vm_partition, -vmp <-1, 0...n>"
	"\n\t\tSelect a VM partition for all GPUVM accesses.  Default is -1 which"
	"\n\t\trefers to the 0'th instance of the VM hub which is not the same as"
	"\n\t\tspecifying '0'.  Values above -1 are for ASICs with multiple IP instances.\n"
"\n*** Bank Selection ***\n"
"\n\t--bank, -b <se> <sh> <instance>\n\t\tSelect a GRBM se/sh/instance bank in decimal. Can use 'x' to denote broadcast.\n"
"\n\t--sbank, -sb <me> <pipe> <queue> [vmid]\n\t\tSelect a SRBM me/pipe/queue bank in decimal.  VMID is optional (default: 0). \n"
"\n\t--cbank, -cb <context_reg_bank>\n\t\tSelect a context register bank (value is multiplied by 0x1000). \n"
"\n*** Device Information ***\n"
"\n\t--config, -c\n\t\tPrint out configuation data read from kernel driver.\n"
"\n\t--enumerate, -e\n\t\tEnumerate all AMDGPU devices detected.\n"
"\n\t--list-blocks, -lb\n\t\tList the IP blocks discovered for this device.\n"
"\n\t--list-regs, -lr <string>\n\t\tList the registers for a given IP block (can use '-O bits' to list bitfields).\n"
"\n\t--dump-discovery-table, -ddt \n\t\tDump device discovery table information.\n"
"\n*** Register Access ***\n"
"\n\t--lookup, -lu <address_or_regname> <value>\n\t\tLook up bit decoding of an MMIO register by address (with 0x prefix) or by register name."
	"\n\t\tThe register name string must include the ipname, e.g., uvd6.mmUVD_CONTEXT_ID.\n"
"\n\t--write, -w <address> <number>\n\t\tWrite a value in hex to a register specified as a register path in the"
	"\n\t\tform <asicname.ipname.regname>.  For instance \"tonga.uvd5.mmUVD_SOFT_RESET\"."
	"\n\t\tCan be used multiple times to set multiple registers.  You can"
	"\n\t\tspecify * for asicname and/or ipname to simplify scripts.\n"
"\n\t--writebit, -wb <string> <number>\n\t\tWrite a value in hex to a register bitfield specified as in --write but"
	"\n\t\tthe addition of the bitfield name.  For instance: \"*.gfx80.mmRLC_PG_CNTL.PG_OVERRIDE\"\n"
"\n\t--read, -r <string>\n\t\tRead a value from a register and print it to stdout.  This command"
	"\n\t\tuses the same path notation as --write.  It also accepts * for regname."
	"\n\t\tA trailing * on a regname will read any register that has a name that contains the"
	"\n\t\tremainder of the name specified.\n"
"\n\t--scan, -s <string>\n\t\tScan and print an ip block by name, e.g. \"uvd6\" or \"carrizo.uvd6\"."
	"\n\t\tCan be used multiple times.\n"
"\n\t--logscan, -ls\n\t\tRead and display contents of the MMIO register log (usually specified with"
	"\n\t\t'-O bits,follow,empty_log' to continually dump the trace log.)\n",
	UMR_BUILD_VER, UMR_BUILD_REV, UMR_BUILD_BRANCH, __DATE__);

printf(
"\n*** Device Utilization ***\n"
"\n\t--top, -t\n\t\tSummarize GPU utilization.  Can select a SE block with --bank.  Can use"
	"\n\t\toptions 'use_colour' to colourize output and 'use_pci' to improve efficiency.\n"
"\n\t--waves, -wa [<ring_name> | <vmid>@<addr>.<size>]\n\t\tPrint out information about any active CU waves.  Can use '-O bits'"
	"\n\t\tto see decoding of various wave fields.  Can use the '-O halt_waves' option"
	"\n\t\tto halt the SQ while reading registers.  An optional ring name can be specified"
	"\n\t\twhich will then search a given ring for pointers to active shaders.  It will"
	"\n\t\tdefault to the 'gfx' ring if nothing is specified.  Alternatively, an IB can be specified"
	"\n\t\tby a vmid, address, and size (in hex bytes) triplet.\n"
"\n\t--profiler, -prof [pixel= | vertex= | compute=]<nsamples> [ring]"
	"\n\t\tCapture 'nsamples' samples of wave data. Optionally specify a ring to search"
	"\n\t\tfor IBs that point to shaders.  Defaults to 'gfx'.  Additionally, the type"
	"\n\t\tof shader can be selected for as well to only profile a given type.\n"
"\n*** Virtual Memory Access ***\n"
"\n\tVMIDs are specified in umr as 16 bit numbers where the lower 8 bits"
"\n\tindicate the hardware VMID and the upper 8 bits indicate the which VM space to use."
"\n\n\t\t0 - GFX hub"
"\n\t\t1 - MM hub"
"\n\t\t2 - VC0 hub"
"\n\t\t3 - VC1 hub"
"\n\n\tFor instance, 0x107 would specify the 7'th VMID on the MM hub.\n"
"\n\t--vm-decode, -vm vmid@<address> <num_of_pages>"
	"\n\t\tDecode page mappings at a specified address (in hex) from the VMID specified."
	"\n\t\tThe VMID can be specified in hexadecimal (with leading '0x') or in decimal."
	"\n\t\tImplies '-O verbose' for the duration of the command so does not require it"
	"\n\t\tto be manually specified.\n"
"\n\t--vm-read, -vr [<vmid>@]<address> <size>"
	"\n\t\tRead 'size' bytes (in hex) from a given address (in hex) to stdout. Optionally"
	"\n\t\tspecify the VMID (in decimal or in hex with a '0x' prefix) treating the address"
	"\n\t\tas a virtual address instead.  Can use 'verbose' option to print out PDE/PTE"
	"\n\t\tdecodings.\n"
"\n\t--vm-write, -vw [<vmid>@]<address> <size>"
	"\n\t\tWrite 'size' bytes (in hex) to a given address (in hex) from stdin.\n"
"\n\t--vm-write-word, -vww [<vmid>@]<address> <word>"
	"\n\t\tWrite a 32-bit word 'data' (in hex) to a given address (in hex) in host machine order.\n"
"\n\t--vm-disasm, -vdis [<vmid>@]<address> <size>"
	"\n\t\tDisassemble 'size' bytes (in hex) from a given address (in hex).  The size can"
	"\n\t\tbe specified as zero to have umr try and compute the shader size.\n"
"\n*** Ring and PM4 decoding ***\n"
"\n\t--ring-stream, -RS <string>([from:to])\n\t\tRead the contents of a ring named by the string without the amdgpu_ring_ prefix. "
	"\n\t\tBy default it will read and display the entire ring.  A starting and ending "
	"\n\t\taddress can be specified in decimal or a '.' can be used to indicate relative "
	"\n\t\tto the current wptr pointer.  For example, \"-R gfx\" would read the entire gfx "
	"\n\t\tring, \"-R gfx[0:16]\" would display the contents from 0 to 16 inclusively, and "
	"\n\t\t\"-R gfx[.]\" or \"-R gfx[.:.]\" would display the last 32 words relative to rptr.\n"
"\n\t--dump-ib, -di [vmid@]address length [pm]"
	"\n\t\tDump an IB packet at an address with an optional VMID.  The length is specified"
	"\n\t\tin bytes.  The type of decoder <pm> is optional and defaults to PM4 packets."
	"\n\t\tCan specify '3' for SDMA packets, and '2' for MES packets\n"
"\n\t--dump-ib-file, -df filename [pm]"
	"\n\t\tDump an IB stored in a file as a series of hexadecimal DWORDS one per line."
	"\n\t\tOptionally supply a PM type, can specify '2' for MES, '3' for SDMA IBs, or '4' for"
	"\n\t\tPM4 IBs.  The default is PM4.\n"
"\n\t--header-dump, -hd [HEADER_DUMP_reg]"
	"\n\t\tDump the contents of the HEADER_DUMP buffer and decode the opcode into a"
	"\n\t\thuman readable string.\n"
"\n\t--print-cpc, -cpc"
	"\n\t\tPrint CPC register data.\n"
"\n\t--print-sdma, -sdma"
	"\n\t\tPrint SDMA register data.\n");

printf(
"\n*** Power and clock ***\n"
"\n\t--power, -p \n\t\tRead the content of clocks, temperature, gpu loading at runtime"
	"\n\t\toptions 'use_colour' to colourize output.\n"
"\n\t--clock-scan, -cs [clock]\n\t\tScan the current hierarchy value of each clock."
	"\n\t\tDefault will list all the hierarchy value of clocks. otherwise will list the corresponding clock, eg. sclk.\n"
"\n\t--clock-manual, -cm [clock] [value]\n\t\tSet the value of the corresponding clock."
	"\n\t\tUse -cs command to check hierarchy values of clock and then use -cm value to set the clock.\n"
"\n\t--clock-high, -ch\n\t\tSet power_dpm_force_performance_level to high.\n"
"\n\t--clock-low, -cl\n\t\tSet power_dpm_force_performance_level to low.\n"
"\n\t--clock-auto, -ca\n\t\tSet power_dpm_force_performance_level to auto.\n"
"\n\t--ppt_read, -pptr [ppt_field_name]\n\t\tRead powerplay table value and print it to stdout."
	"\n\t\tThis command will print all the powerplay table information or the corresponding string in powerplay table.\n"
"\n\t--gpu_metrics, -gm"
	"\n\t\tPrint the GPU metrics table for the device."
"\n\t--power, -p \n\t\tRead the conetent of clocks, temperature, gpu loading at runtime"
	"\n\t\toptions 'use_colour' to colourize output \n"
"\n*** Video BIOS Information ***\n"
	"\n\t--vbios_info, -vi \n\t\tPrint Video BIOS information\n"
"\n*** Test Vector Generation ***\n"
	"\n\t--test-log, -tl <filename>\n\t\tLog all MMIO/memory reads to a file\n"
	"\n\t--test-harness, -th <filename>\n\t\tUse a test harness file instead of reading from hardware\n");

#if UMR_GUI
printf(
"\n*** GUI server ***\n"
#if UMR_GUI_REMOTE
"\n\t--server [url] \n\t\turl can be tcp://127.0.0.1:1234 or tcp://*:8090. Default value is 'tcp://*:1234' see Nanomsg protocol doc for more example\n"
#endif
"\n\t--gui [url] \n\t\tRun umr in GUI mode. An optional url can be supplied to connect to a remote instance (see --server)\n");
#endif
			exit(EXIT_SUCCESS);
#if UMR_GUI
#if UMR_GUI_REMOTE
		} else if (!strcmp(argv[i], "--server")) {
			char *url = (i < argc - 1) ? argv[i + 1] : "tcp://*:1234";
			run_server_loop(url, asic);
#endif
		} else if (!strcmp(argv[i], "--gui")) {
			if (i < argc - 1 && argv[i+1][0] != '-') {
				guiurl = argv[i+1];
				i++;
			}
			if (!running_as_gui) {
				umr_run_gui(guiurl);
				exit(EXIT_SUCCESS);
			}
#endif
		} else {
			fprintf(stderr, "[ERROR]: Unknown option <%s>\n", argv[i]);
		}
	}

#if UMR_GUI
	if (running_as_gui) {
		umr_run_gui(guiurl);
		exit(EXIT_SUCCESS);
	}
#endif


	if (options.need_scan && options.print) {
		asic = get_asic();
		umr_scan_asic(asic, "", "", "");
	}

	if (options.print) {
		asic = get_asic();
		umr_print_asic(asic, "");
	}

	if (!asic) {
		printf("User Mode Register debugger v%s for AMDGPU devices (build: %s [%s], date: %s), Copyright (c) 2022, AMD Inc.\n\n"
			   "Use '--help' for a list of commands and options.\n",
			    UMR_BUILD_VER, UMR_BUILD_REV, UMR_BUILD_BRANCH, __DATE__);
	}

	if (options.use_xgmi) {
		// the parent 'asic' is included in the nodes array
		int n;
		for (n = 0; asic->config.xgmi.nodes[n].asic; n++)
			umr_close_asic(asic->config.xgmi.nodes[n].asic);
	} else {
		umr_close_asic(asic);
	}

	if (th) {
		umr_free_test_harness(th);
	}
	
	if (options.export_model) {
		fprintf(stderr, "[NOTE]: ASIC model exported uses FAMILY_NV family and IS_APU=0 flag, change these as appropriate.\n");
	}
}

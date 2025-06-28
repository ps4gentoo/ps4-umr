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
#include "umr_rumr.h"
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
static struct umr_asic *asic;
static struct umr_test_harness *th = NULL;

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

	if (!options.verbose && strstr(fmt, "[VERBOSE]"))
		return 0;

	va_start(ap, fmt);
	r = vfprintf(stderr, fmt, ap);
	fflush(stderr);
	va_end(ap);
	return r;
}

static struct rumr_comm_funcs *rumr_get_cf(char *arg, char **addr)
{
	struct rumr_comm_funcs *cf;
	*addr = arg;

	if (!memcmp(arg, "tcp://", 6)) {
		cf = calloc(1, sizeof rumr_tcp_funcs);
		*cf = rumr_tcp_funcs;
		cf->log_msg = err_printf;
		*addr = &arg[6];
		return cf;
	}
	return NULL;
}

static struct umr_asic *get_asic(void)
{
	struct umr_options topt;

	if (th && th->discovery.contents) {
		asic = umr_discover_asic_by_discovery_table("emulated", &options, std_printf);
		umr_attach_test_harness(th, asic);
		return asic;
	} else if (th && strlen(options.dev_name)) {
		asic = umr_discover_asic_by_name(&options, options.dev_name, std_printf);
		umr_attach_test_harness(th, asic);
		return asic;
	}

	options.quiet = 1;
	topt = options;
retry:
	options = topt;
	if (options.verbose) {
		fprintf(stderr, "[VERBOSE]: Trying to connect to DRI instance %d...\n", options.instance);
	}
	asic = umr_discover_asic(&options, err_printf);
	if (!asic && !options.forced_instance && options.instance < 256) {
		topt.instance++;
		goto retry;
	}

	if (!asic) {
		fprintf(stderr, "[ERROR]: ASIC not found or compatible (instance=%d, did=%08lx)\n", options.instance, (unsigned long)options.forcedid);
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
	asic->ring_func.read_ring_data = umr_read_ring_data;

	asic->wave_funcs.get_wave_sq_info = umr_get_wave_sq_info;
	if (options.no_kernel) {
		asic->gpr_read_funcs.read_sgprs = umr_read_sgprs_via_mmio;
		asic->gpr_read_funcs.read_vgprs = umr_read_vgprs_via_mmio;
		asic->wave_funcs.get_wave_status = umr_get_wave_status_via_mmio;
	} else {
		asic->gpr_read_funcs.read_sgprs = umr_read_sgprs;
		asic->gpr_read_funcs.read_vgprs = umr_read_vgprs;
		asic->wave_funcs.get_wave_status = umr_get_wave_status;
	}

	asic->shader_disasm_funcs.disasm = umr_shader_disasm;

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

	if (asic->options.vgpr_granularity >= 0)
		asic->parameters.vgpr_granularity = asic->options.vgpr_granularity;

	// sanity check if they didn't specify a partition
	if (asic->options.vm_partition < 0) {
		int n;
		for (n = 0; n < asic->no_blocks; n++) {
			if ((!memcmp(asic->blocks[n]->ipname, "gfx", 3) && strstr(asic->blocks[n]->ipname, "{")) ||
			    (!memcmp(asic->blocks[n]->ipname, "mmhub", 5) && strstr(asic->blocks[n]->ipname, "{"))) {
				asic->err_msg(
					"[WARNING]: No VM partition is selected on hardware with multiple GC and/or MMHUB blocks.\n"
					"[WARNING]: Page walking and wave scanning are unlikely to work.\n"
					"[WARNING]: Please use -vmp or --vm-partition to select a VM partition.\n");
				break;
			}
		}
	}

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
		} else if (!strcmp(option, "skip_gprs")) {
			options.skip_gprs = 1;
		} else if (!strcmp(option, "empty_log")) {
			options.empty_log = 1;
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
			options.no_follow_loadx = 1;
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

enum {
	PASS_OPTIONS=0,
	PASS_TEST_HARNESS,
	PASS_ASIC_MODEL,
	PASS_COMMANDS,

	PASS_MAX
};

static void do_help(void)
{
	printf("User Mode Register debugger v%s for AMDGPU devices (build: %s [%s], date: %s), Copyright (c) 2025, AMD Inc.\n"
	"\n*** Device Selection ***\n"
	"\n\t--database-path, -dbp <path>"
		"\n\t\tSpecify a database path for register, ip, and asic model data.\n"
	"\n\t--option -O <string>[,<string>,...]\n\t\tEnable various flags:"
		"\n\t\t\tbits, bitsfull, empty_log, follow, no_follow_ib,"
		"\n\t\t\tuse_pci, use_colour, read_smc, quiet, no_kernel, verbose, halt_waves,"
		"\n\t\t\tdisasm_early_term, no_disasm, disasm_anyways, wave64, full_shader, skip_gprs, no_fold_vm_decode, force_asic_file\n"
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
	"\n\t--by-pci <device>"
		"\n\t\tLike --pci but still uses the traditional debugfs path to interface with"
		"\n\t\tthe hardware.  This is useful for interacting with APIs that identify hardware"
		"\n\t\tby the PCI bus address.\n"
	"\n\t--gfxoff, -go <0 | 1>"
		"\n\t\tEnable GFXOFF with a non-zero value or disable with a 0.  Used to control the GFXOFF feature on"
		"\n\t\tselect hardware. Command without parameter will check GFXOFF status.\n"
	"\n\t--vm-partition, -vmp <-1, 0...n>"
		"\n\t\tSelect a VM partition for all GPUVM accesses.  Default is -1 which"
		"\n\t\trefers to the 0'th instance of the VM hub which is not the same as"
		"\n\t\tspecifying '0'.  Values above -1 are for ASICs with multiple IP instances.\n"
	"\n\t--vgpr-granularity, -vgpr <-1, 0...n>"
		"\n\t\tSpecify the VGPR size granularity as a power of 2, e.g., '2' means 4 DWORDs per increment.\n"
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
		"\n\t\tremainder of the name specified.\n",
		UMR_BUILD_VER, UMR_BUILD_REV, UMR_BUILD_BRANCH, __DATE__);

	printf(
	"\n\t--logscan, -ls\n\t\tRead and display contents of the MMIO register log (usually specified with"
		"\n\t\t'-O bits,empty_log' to continually dump bitfields and empty the trace after.)\n"
	"\n*** Device Utilization ***\n"
	"\n\t--top, -t\n\t\tSummarize GPU utilization.  Can select a SE block with --bank.  Can use"
		"\n\t\toptions 'use_colour' to colourize output and 'use_pci' to improve efficiency.\n"
	"\n\t--waves, -wa [<ring_name> | <vmid>@<addr>.<size>]\n\t\tPrint out information about any active CU waves.  Can use '-O bits'"
		"\n\t\tto see decoding of various wave fields.  Can use the '-O halt_waves' option"
		"\n\t\tto halt the SQ while reading registers.  An optional ring name can be specified"
		"\n\t\twhich will then search a given ring for pointers to active shaders.  It will"
		"\n\t\tdefault to the 'gfx' ring if nothing is specified.  Alternatively, an IB can be specified"
		"\n\t\tby a vmid, address, and size (in hex bytes) triplet.\n"
	"\n\t--singlestep, -ss <se>,<sh>,<wgp>,<simd>,<wave>\n\t\tSingle-step one wave."
	"\n\t\tTries advancing execution on the specified wave by one instruction."
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
		"\n\t\tto be manually specified.\n");

	printf(
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
		"\n\t\tto the current wptr pointer.  For example, \"-RS gfx\" would read the entire gfx "
		"\n\t\tring, \"-RS gfx[0:16]\" would display the contents from address 0 to 16 inclusively, and "
		"\n\t\t\"-RS gfx[.]\" or \"-RS gfx[.:.]\" would display contents from the ring READ pointer to "
		"\n\t\tthe ring WRITE pointer.\n"
	"\n\t--dump-ib, -di [vmid@]address length [pm]"
		"\n\t\tDump an IB packet at an address with an optional VMID.  The length is specified"
		"\n\t\tin bytes.  The type of decoder <pm> is optional and defaults to PM4 packets."
		"\n\t\tCan specify '3' for SDMA packets, '2' for MES packets, '1' for VPE packets, '5' for UMSCH packets,"
		"\n\t\t'6' for HSA packets, '7' for VCN decode, and '8' for VCN encode.\n"
	"\n\t--dump-ib-file, -df filename [pm]"
		"\n\t\tDump an IB stored in a file as a series of hexadecimal DWORDS one per line.  If the filename"
		"\n\t\tends in .bin the file is treated as binary, if the filename ends in .ring it treats it as a"
		"\n\t\tring copy and skips the first 12 bytes.  Can optionally specify '3' for SDMA packets, '2' for"
		"\n\t\tMES packets, '1' for VPE packets, '5' for UMSCH packets, '6' for HSA packets, '7' for VCN decode, and '8' for"
		"\n\t\tVCN encode.  The default is PM4.\n");

	printf(
	"\n\t--header-dump, -hd [HEADER_DUMP_reg]"
		"\n\t\tDump the contents of the HEADER_DUMP buffer and decode the opcode into a"
		"\n\t\thuman readable string.\n"
	"\n\t--print-cpc, -cpc"
		"\n\t\tPrint CPC register data.\n"
	"\n\t--print-sdma, -sdma"
	"\n\t\tPrint SDMA register data.\n"
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
	"\n\t--ppt-read, -pptr [ppt_field_name]\n\t\tRead powerplay table value and print it to stdout."
		"\n\t\tThis command will print all the powerplay table information or the corresponding string in powerplay table.\n"
	"\n\t--gpu-metrics, -gm [delay]"
		"\n\t\tPrint the GPU metrics table for the device, optionally continuously read every 'delay' milliseconds.\n"
	"\n\t--power, -p \n\t\tRead the conetent of clocks, temperature, gpu loading at runtime"
		"\n\t\toptions 'use_colour' to colourize output \n"
	"\n*** Video BIOS Information ***\n"
		"\n\t--vbios-info, -vi \n\t\tPrint Video BIOS information\n"
	"\n*** Test Vector Generation ***\n"
		"\n\t--test-log, -tl <filename>\n\t\tLog all MMIO/memory reads to a file\n"
		"\n\t--test-harness, -th <filename>\n\t\tUse a test harness file instead of reading from hardware\n"
	"\n*** RUMR Commands ***\n"
		"\n\t--rumr-client <server>\n\t\tRun as a RUMR client connecting to 'server', e.g. tcp://127.0.0.1:9000\n"
		"\n\t--rumr-server <server>\n\t\tRun as a RUMR server binding to 'server', e.g. tcp://127.0.0.1:9000\n"
	"\n*** KFD Support ***\n"
		"\n\t--runlist, -rls <node>\n\t\tDump any runlists for a given KFD node specified\n"
		"\n\t--dump-mqd vmid@virtualaddr engsel\n\t\tDump an MQD from a given VMID and virtual address for a given engine and asic family."
		"\n\t\tEngines are 0=compute, 2=sdma0, 3=sdma1, 4=gfx, 5=mes.\n"
	"\n*** Scriptware Support ***\n"
		"\n\t--script [commands]\n\t\tRun a script helper command.  Run without parameters to see list of commands.\n"
	);
	#if UMR_SERVER
	printf(
	"\n*** GUI server ***\n");
	printf(
		"\n\t--server [url] \n\t\turl can be tcp://127.0.0.1:1234 or tcp://*:8090. Default value is 'tcp://*:1234' see Nanomsg protocol doc for more example\n");
	#elif UMR_GUI
	printf(
	"\n*** GUI ***\n");
	#endif
	#if UMR_GUI
	printf(
		"\n\t--gui [url] \n\t\tRun umr in GUI mode. An optional url can be supplied to connect to a remote instance (see --server)\n");
	#endif
	exit(EXIT_SUCCESS);
}


static void umr_start_rumr_client(struct rumr_client_state *cs, char *server)
{
	struct rumr_comm_funcs *cf;
	char *cfp;
	cf = rumr_get_cf(server, &cfp);
	if (rumr_client_connect(cs, cf, cfp)) {
		free(cf);
		exit(EXIT_FAILURE);
	}
	asic = cs->asic;
	free(cf);
}

int main(int argc, char **argv)
{
	int pass, i, j, k, l;
	char *blockname, *str, *str2, asicname[256], ipname[256], regname[256], clockperformance[256];
	struct timespec req;
	FILE *f;
	struct rumr_client_state client_st;
	char *argflags;
#if UMR_GUI
	int running_as_gui = 0;
	char *guiurl = NULL;

	if (strstr(argv[0], "umrgui")) {
		if (argc >= 2)
			guiurl = argv[1];
		running_as_gui = 1;
	} else if (argc >= 2 && strcmp(argv[1], "--gui") == 0) {
		if (argc >= 3)
			guiurl = argv[2];
		running_as_gui = 1;
	}

	if (running_as_gui) {
		umr_run_gui(guiurl);
		exit(EXIT_SUCCESS);
	}
#endif

	memset(&options, 0, sizeof options);

	/* defaults */
	asic = NULL;
	options.forcedid = -1;
	options.scanblock = "";
	options.vm_partition = -1;
	options.vgpr_granularity = -1;
	options.forced_instance = 0;

	argflags = calloc(1, argc+1);

	str = getenv("RUMR_SERVER_ADDR");
	if (str) {
		umr_start_rumr_client(&client_st, str);
	}

	for (pass = 0; pass < PASS_MAX; pass++) {
		// if we already have an ASIC assign options
		if (pass == PASS_ASIC_MODEL && asic) {
			asic->options = options;
		}

		// if we're the pass right after when an ASIC model is created
		// normally let's make sure we have one
		if ((pass - 1) == PASS_ASIC_MODEL) {
			if (!asic)
				asic = get_asic();
		}

		if ((pass - 1) == PASS_OPTIONS) {
			// sanity check
			f = umr_database_open(options.database_path, "pci.did", 0);
			if (!f) {
				fprintf(stderr, "[ERROR]: Cannot open pci.did which means the database isn't found.\n");
				fprintf(stderr, "[ERROR]: UMR should either be installed via packaging or 'make install', or\n");
				fprintf(stderr, "[ERROR]: you should run UMR from the original build tree it was built in.\n");
				fprintf(stderr, "[ERROR]: Copying a build tree from one host to another may not work if the build tree\n");
				fprintf(stderr, "[ERROR]: is not in the same path location.\n");
				return EXIT_FAILURE;
			} else {
				fclose(f);
			}
		}

		for (i = 1; i < argc; i++) {
			if (pass == PASS_OPTIONS) {
				if (!strcmp(argv[i], "--vgpr-granularity") || !strcmp(argv[i], "-vgpr")) {
					if (i + 1 < argc) {
						argflags[i] = 1;
						argflags[i+1] = 1;
						options.vgpr_granularity = atoi(argv[i+1]);
						++i;
					} else {
						fprintf(stderr, "[ERROR]: --vgpr-granularity requires at least one parameter\n");
						return EXIT_FAILURE;
					}
				} else if (!strcmp(argv[i], "--option") || !strcmp(argv[i], "-O")) {
					if (i + 1 < argc) {
						argflags[i] = 1;
						argflags[i+1] = 1;
						parse_options(argv[i+1]);
						++i;
					} else {
						fprintf(stderr, "[ERROR]: --option requires one parameter\n");
						return EXIT_FAILURE;
					}
				} else if (!strcmp(argv[i], "--database-path") || !strcmp(argv[i], "-dbp")) {
					if (i + 1 < argc) {
						argflags[i] = 1;
						argflags[i+1] = 1;
						strcpy(options.database_path, argv[i+1]);
						++i;
					} else {
						fprintf(stderr, "[ERROR]: --database-path requires at least one parameter\n");
						return EXIT_FAILURE;
					}
				} else if (!strcmp(argv[i], "--vm-partition") ||
					   !strcmp(argv[i], "--vm_partition") ||
					   !strcmp(argv[i], "-vmp")) {
					if (i + 1 < argc) {
						argflags[i] = 1;
						argflags[i+1] = 1;
						options.vm_partition = atoi(argv[i+1]);
						++i;
					} else {
						fprintf(stderr, "[ERROR]: --vm-partition requires a number\n");
						return EXIT_FAILURE;
					}
				} else if (!strcmp(argv[i], "--bank") || !strcmp(argv[i], "-b")) {
					if (i + 3 < argc) {
						argflags[i] = 1;
						argflags[i+1] = 1;
						argflags[i+2] = 1;
						argflags[i+3] = 1;
						options.bank.grbm.se = argv[i+1][0] == 'x' ? 0xFFFFFFFFUL : (uint32_t)atoi(argv[i+1]);
						options.bank.grbm.sh = argv[i+2][0] == 'x' ? 0xFFFFFFFFUL : (uint32_t)atoi(argv[i+2]);
						options.bank.grbm.instance = argv[i+3][0] == 'x' ? 0xFFFFFFFFUL : (uint32_t)atoi(argv[i+3]);
						options.use_bank = 1;
						i += 3;
					} else {
						fprintf(stderr, "[ERROR]: --bank requires three parameters\n");
						return EXIT_FAILURE;
					}
				} else if (!strcmp(argv[i], "--sbank") || !strcmp(argv[i], "-sb")) {
					if (i + 3 < argc) {
						argflags[i] = 1;
						argflags[i+1] = 1;
						argflags[i+2] = 1;
						argflags[i+3] = 1;
						options.bank.srbm.me = atoi(argv[i+1]);
						options.bank.srbm.pipe = atoi(argv[i+2]);
						options.bank.srbm.queue = atoi(argv[i+3]);
						if (i + 4 < argc && sscanf(argv[i+4], "%u", &options.bank.srbm.vmid) == 1) {
							argflags[i+4] = 1;
							++i;
						} else {
							options.bank.srbm.vmid = 0;  // default
						}
						options.use_bank = 2;
						i += 3;
					} else {
						fprintf(stderr, "[ERROR]: --sbank requires three parameters\n");
						return EXIT_FAILURE;
					}
				} else if (!strcmp(argv[i], "--cbank") || !strcmp(argv[i], "-cb")) {
					if (i + 1 < argc) {
						argflags[i] = 1;
						argflags[i+1] = 1;
						options.context_reg_bank = atoi(argv[i+1]);
						++i;
					} else {
						fprintf(stderr, "[ERROR]: --cbank requires one parameters\n");
						return EXIT_FAILURE;
					}
				} else if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
					do_help();
				}
			} else if (pass == PASS_ASIC_MODEL) {
				if (!strcmp(argv[i], "--script")) {
					int argi, argj;
					argflags[i] = 1;

					// grab all arguments 
					argi = ++i;
					argj = argi;
					for (argj = argi; argj < argc; argj++) {
						if (argv[argj][0] == '-') {
							--argj;
							break;
						}
						argflags[argj] = 1;
						++i;
					}
					umr_handle_scriptware(std_printf, options.database_path, &argv[argi], argj - argi);
					goto stopprocessingcommands;
				} else if (!strcmp(argv[i], "--gpu") || !strcmp(argv[i], "-g")) {
					if (i + 1 < argc) {
						char *s;
						argflags[i] = 1;
						argflags[i+1] = 1;
						s = strstr(argv[i+1], "@");
						if (s) {
							strncpy(options.dev_name, argv[i+1], MIN(sizeof(options.dev_name), (unsigned)(s - argv[i+1])));
							options.instance = atoi(s + 1);
							options.forced_instance = 1;
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
						umr_apply_callbacks(asic, &asic->mem_funcs, &asic->reg_funcs);
						++i;
					} else {
						fprintf(stderr, "[ERROR]: --gpu requires a parameter\n");
						return EXIT_FAILURE;
					}
				} else if (!strcmp(argv[i], "--instance") || !strcmp(argv[i], "-i")) {
					if (i + 1 < argc) {
						argflags[i] = 1;
						argflags[i+1] = 1;
						options.instance = atoi(argv[i+1]);
						options.forced_instance = 1;
						asic = get_asic();
						++i;
					} else {
						fprintf(stderr, "[ERROR]: --instance requires a number\n");
						return EXIT_FAILURE;
					}
				} else if (!strcmp(argv[i], "--force") || !strcmp(argv[i], "-f")) {
					if (i + 1 < argc) {
						unsigned long did = 0;
						argflags[i] = 1;
						argflags[i+1] = 1;
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
				} else if (!strcmp(argv[i], "--by-pci")) {
					if (i + 1 < argc && sscanf(argv[i+1], "%04x:%02x:%02x.%01x",
						&options.pci.domain, &options.pci.bus, &options.pci.slot,
						&options.pci.func ) >= 4) {
						argflags[i] = 1;
						argflags[i+1] = 1;
						options.use_pci = 0; // always use debugfs!
						++i;
					} else {
						fprintf(stderr, "[ERROR]: --by-pci requires domain:bus:slot.function\n");
						return EXIT_FAILURE;
					}
				} else if (!strcmp(argv[i], "--pci")) {
					if (i + 1 < argc && sscanf(argv[i+1], "%04x:%02x:%02x.%01x",
						&options.pci.domain, &options.pci.bus, &options.pci.slot,
						&options.pci.func ) >= 4) {
						argflags[i] = 1;
						argflags[i+1] = 1;
						options.use_pci = 1; // implied by the --pci option
						++i;
					} else {
						fprintf(stderr, "[ERROR]: --pci requires domain:bus:slot.function\n");
						return EXIT_FAILURE;
					}
				} else if (!strcmp(argv[i], "--rumr-client")) {
					if (i + 1 < argc) {
						argflags[i] = 1;
						argflags[i+1] = 1;
						umr_start_rumr_client(&client_st, argv[i+1]);
						asic->options = options;
						++i;
					} else {
						fprintf(stderr, "[ERROR]: --rumr-client requires one parameter\n");
						return EXIT_FAILURE;
					}
				}
			} else if (pass == PASS_TEST_HARNESS) {
				if (!strcmp(argv[i], "--test-log") || !strcmp(argv[i], "-tl")) {
					if (i + 1 < argc) {
						argflags[i] = 1;
						argflags[i+1] = 1;
						options.test_log_fd = fopen(argv[i + 1], "w");
						options.test_log = 1;
						++i;
					} else {
						fprintf(stderr, "[ERROR]: --test-log requires one parameter\n");
						return EXIT_FAILURE;
					}
				} else if (!strcmp(argv[i], "--test-harness") || !strcmp(argv[i], "-th")) {
					if (i + 1 < argc) {
						argflags[i] = 1;
						argflags[i+1] = 1;
						th = umr_create_test_harness_file(argv[i + 1]);
						options.th = th;
						options.test_log = 1;
						options.test_log_fd = NULL;
						++i;
					} else {
						fprintf(stderr, "[ERROR]: --test-harness requires one parameter\n");
						return EXIT_FAILURE;
					}
				} else if (!strcmp(argv[i], "--enumerate") || !strcmp(argv[i], "-e")) {
					// not a test harness command but we want to run this before
					// we hit the ASIC_MODEL step
					argflags[i] = 1;
					umr_enumerate_devices(std_printf, options.database_path);
					goto stopprocessingcommands;
				}
			} else if (pass == PASS_COMMANDS) {
				if (!strcmp(argv[i], "--dump-mqd")) {
					uint32_t mqdbuf[512], engsel, vmid;
					uint64_t va;
					if (i + 2 < argc) {
						argflags[i] = 1;
						argflags[i+1] = 1;
						argflags[i+2] = 1;
						sscanf(argv[i+1], "%"PRIx32"@%"PRIx64, &vmid, &va);
						sscanf(argv[i+2], "%"PRIu32, &engsel);
						// read buffer
						if (umr_read_vram(asic, asic->options.vm_partition, vmid, va, 512*4, &mqdbuf) < 0) {
							asic->err_msg("[ERROR]: Could not read MQD buffer\n");
						} else {
							char **mqd_txt;
							mqd_txt = umr_mqd_decode_data(engsel, asic->family, mqdbuf, "*");
							if (mqd_txt) {
								int x;
								for (x = 0; mqd_txt[x]; x++) {
									asic->std_msg("%s\n", mqd_txt[x]);
									free(mqd_txt[x]);
								}
								free(mqd_txt);
							}
						}
						i += 2;
					} else {
						fprintf(stderr, "[ERROR]: --dump-mqd requires two parameters\n");
						return EXIT_FAILURE;
					}
				} else if (!strcmp(argv[i], "--config") || !strcmp(argv[i], "-c")) {
					argflags[i] = 1;
					umr_apply_callbacks(asic, &asic->mem_funcs, &asic->reg_funcs);
					umr_print_config(asic);
				} else if (!strcmp(argv[i], "--list-blocks") || !strcmp(argv[i], "-lb")) {
					argflags[i] = 1;
					for (j = 0; j < asic->no_blocks; j++) {
						printf("\t%s.%s (%d.%d.%d)\n", asic->asicname, asic->blocks[j]->ipname, asic->blocks[j]->discoverable.maj,
						asic->blocks[j]->discoverable.min, asic->blocks[j]->discoverable.rev);
					}
				} else if (!strcmp(argv[i], "--list-regs") || !strcmp(argv[i], "-lr")) {
					if (i + 1 < argc) {
						argflags[i] = 1;
						argflags[i+1] = 1;
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
						int tmp = asic->options.bitfields;
						argflags[i] = 1;
						argflags[i+1] = 1;
						argflags[i+2] = 1;
						asic->options.bitfields = 1;
						umr_lookup(asic, argv[i+1], argv[i+2]);
						asic->options.bitfields = tmp;
						i += 2;
					}
				} else if (!strcmp(argv[i], "--write") || !strcmp(argv[i], "-w")) {
					if (i + 2 < argc) {
						uint32_t reg, val;

						argflags[i] = 1;
						argflags[i+1] = 1;
						argflags[i+2] = 1;

						if (!memcmp(argv[i+1], "0x", 2) && sscanf(argv[i+1], "%"SCNx32, &reg) == 1 && sscanf(argv[i+2], "%"SCNx32, &val) == 1)
							umr_write_reg(asic, reg, val, REG_MMIO);
						else
							umr_set_register(asic, argv[i+1], argv[i+2]);
						i += 2;
					} else {
						fprintf(stderr, "[ERROR]: --write requires two parameters\n");
						return EXIT_FAILURE;
					}
				} else if (!strcmp(argv[i], "--writebit") || !strcmp(argv[i], "-wb")) {
					if (i + 2 < argc) {
						argflags[i] = 1;
						argflags[i+1] = 1;
						argflags[i+2] = 1;
						umr_set_register_bit(asic, argv[i+1], argv[i+2]);
						i += 2;
					} else {
						fprintf(stderr, "[ERROR]: --write requires two parameters\n");
						return EXIT_FAILURE;
					}
				} else if (!strcmp(argv[i], "--waves") || !strcmp(argv[i], "-wa")) {
					if (i + 1 < argc) {
						argflags[i] = 1;
						argflags[i+1] = 1;
						if (argv[i+1][0] != '-') {
							strcpy(asic->options.ring_name, argv[i+1]);
							++i;
						}
					}
					umr_print_waves(asic);
				} else if (!strcmp(argv[i], "--singlestep") || !strcmp(argv[i], "-ss")) {
					if (asic->family < FAMILY_NV) {
						fprintf(stderr, "[ERROR]: --singlestep is only supported on gfx10+!\n");
						return EXIT_FAILURE;
					}
					if (i + 1 < argc) {
						argflags[i] = 1;
						argflags[i+1] = 1;
						if (argv[i + 1][0] == '-') {
							fprintf(stderr, "[ERROR]: --singlestep requires two parameters\n");
							return EXIT_FAILURE;
						}

						char *loc_start = argv[i + 1];
						unsigned wave_loc[5];
						for (j = 0; j < 5; ++j) {
							char *loc_end = strstr(loc_start, ",");
							unsigned loc_size = loc_end ? (unsigned)(loc_end - loc_start) : strlen(loc_start);
							if (loc_size > 2) {
								fprintf(stderr, "[ERROR]: Invalid format for wave id! Format: \"se,sh,wgp,simd,wave\"\n");
								return EXIT_FAILURE;
							}
							char num[3];
							strncpy(num, loc_start, loc_size);
							memset(num + loc_size, 0, 3 - loc_size);
							wave_loc[j] = strtoul(num, NULL, 10);

							if (!loc_end) {
								if (j == 4)
									break;
								fprintf(stderr, "[ERROR]: Not enough elements in wave id! Format: \"se,sh,wgp,simd,wave\"\n");
								return EXIT_FAILURE;
							}
							loc_start = loc_end + 1;
						}

						struct umr_wave_data wd;
						umr_wave_data_init(asic, &wd);

						int r = umr_scan_wave_slot(asic, wave_loc[0], wave_loc[1], wave_loc[2], wave_loc[3], wave_loc[4],
															&wd);
						if (r < 0) {
							fprintf(stderr, "[ERROR]: Failed to scan wave slot\n");
							return EXIT_FAILURE;
						} else if (r == 0) {
							fprintf(stderr, "[ERROR]: Wave is not active!\n");
							return EXIT_FAILURE;
						}

						r = umr_singlestep_wave(asic, &wd);
						if (r < 0) {
							fprintf(stderr, "[ERROR]: Failed to single-step wave!\n");
							return EXIT_FAILURE;
						}
					} else {
						fprintf(stderr, "[ERROR]: --singlestep requires two parameters\n");
						return EXIT_FAILURE;
					}
				} else if (!strcmp(argv[i], "--read") || !strcmp(argv[i], "-r")) {
					if (i + 1 < argc) {
						uint32_t reg;

						argflags[i] = 1;
						argflags[i+1] = 1;

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
						}
						++i;
					} else {
						fprintf(stderr, "[ERROR]: --read requires one parameter\n");
						return EXIT_FAILURE;
					}
				} else if (!strcmp(argv[i], "--ring-stream") || !strcmp(argv[i], "-RS")) {
					if (i + 1 < argc) {
						argflags[i] = 1;
						argflags[i+1] = 1;
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

						argflags[i] = 1;
						argflags[i+1] = 1;
						argflags[i+2] = 1;

						if (sscanf(argv[i+1], "0x%"SCNx32"@%"SCNx64, &vmid, &address) != 2)
							if (sscanf(argv[i+1], "%"SCNu32"@%"SCNx64, &vmid, &address) != 2) {
								sscanf(argv[i+1], "%"SCNx64, &address);
								vmid = UMR_LINEAR_HUB;
							}

						if (sscanf(argv[i+2], "0x%"SCNx32, &len) != 1)
							sscanf(argv[i+2], "%"SCNu32, &len);

						if ((i + 3 < argc) && sscanf(argv[i+3], "%d", &pm) == 1) {
							argflags[i+3] = 1;
							i += 3;
						} else {
							pm = 4;
							i += 2;
						}
						sprintf(str, "%c/0x%"PRIx32"@0x%"PRIx64".0x%"PRIu32, '0' + pm, vmid, address, len);
						umr_read_ring_stream(asic, str);
					} else {
						fprintf(stderr, "[ERROR]: --dump-ib requires three parameters\n");
						return EXIT_FAILURE;
					}
				} else if (!strcmp(argv[i], "--dump-ib-file") || !strcmp(argv[i], "-df")) {
					if (i + 1 < argc) {
						int pm, follow;
						char *name = argv[i+1];
						char str[128];

						argflags[i] = 1;
						argflags[i+1] = 1;

						if ((i + 2 < argc) && sscanf(argv[i+2], "%d", &pm) == 1) {
							argflags[i+2] = 1;
							i += 2;
						} else {
							pm = 4;
							i += 1;
						}

						sprintf(str, "%c/%s", '0' + pm, name);
						follow = asic->options.no_follow_ib;
						asic->options.no_follow_ib = 1;
						asic->options.no_follow_shader = 1;
						asic->options.no_follow_loadx = 1;
						umr_read_ring_stream(asic, str);
						asic->options.no_follow_ib = follow;
						asic->options.no_follow_shader = follow;
						asic->options.no_follow_loadx = follow;
					} else {
						fprintf(stderr, "[ERROR]: --dump-ib-file requires two parameters\n");
						return EXIT_FAILURE;
					}
				} else if (!strcmp(argv[i], "--logscan") || !strcmp(argv[i], "-ls")) {
					int r, new = 0;

					argflags[i] = 1;

					signal(SIGINT, sigint);
					r = system("echo 1 > /sys/kernel/debug/tracing/events/amdgpu/amdgpu_mm_wreg/enable");
					r |= system("echo 1 > /sys/kernel/debug/tracing/events/amdgpu/amdgpu_mm_rreg/enable");
					if (r) {
						r = system("echo 1 > /sys/kernel/debug/tracing/events/amdgpu/amdgpu_device_wreg/enable");
						r |= system("echo 1 > /sys/kernel/debug/tracing/events/amdgpu/amdgpu_device_rreg/enable");
						if (r) {
							fprintf(stderr, "[ERROR]: Could not enable mm tracers\n");
							return EXIT_FAILURE;
						}
						new = 1;
					}
					req.tv_sec = 0;
					req.tv_nsec = 1000000000/10; // 100ms
					while (!quit) {
						nanosleep(&req, NULL);
						umr_scan_log(asic, new);
					}
					if (new) {
						r = system("echo 0 > /sys/kernel/debug/tracing/events/amdgpu/amdgpu_device_wreg/enable");
						r |= system("echo 0 > /sys/kernel/debug/tracing/events/amdgpu/amdgpu_device_rreg/enable");
					} else {
						r = system("echo 0 > /sys/kernel/debug/tracing/events/amdgpu/amdgpu_mm_wreg/enable");
						r |= system("echo 0 > /sys/kernel/debug/tracing/events/amdgpu/amdgpu_mm_rreg/enable");
					}
					if (r) {
						fprintf(stderr, "[ERROR]: Could not diable mm tracers\n");
						return EXIT_FAILURE;
					}
				} else if (!strcmp(argv[i], "--top") || !strcmp(argv[i], "-t")) {
					uint32_t value;
					argflags[i] = 1;
					value = 0;
					if (asic->fd.gfxoff >= 0)
						write(asic->fd.gfxoff, &value, sizeof(value));
					umr_top(asic);
					value = 1;
					if (asic->fd.gfxoff >= 0)
						write(asic->fd.gfxoff, &value, sizeof(value));
				} else if (!strcmp(argv[i], "-mm")) {
					if (i + 1 < argc) {
						argflags[i] = 1;
						argflags[i+1] = 1;
						strcpy(options.hub_name, argv[i+1]);
						++i;
					} else {
						fprintf(stderr, "[ERROR]: -mm requires on parameter");
						return EXIT_FAILURE;
					}
				} else if (!strcmp(argv[i], "--vm-decode") || !strcmp(argv[i], "-vm")) {
					if (i + 2 < argc) {
						uint64_t address;
						uint32_t size, vmid;
						int overbose;

						argflags[i] = 1;
						argflags[i+1] = 1;
						argflags[i+2] = 1;

						overbose = asic->options.verbose;
						asic->options.verbose = 1;

						// allow specifying the vmid in hex as well so
						// people can add the HUB flags more easily
						if ((sscanf(argv[i+1], "0x%"SCNx32"@%"SCNx64, &vmid, &address)) != 2)
							if ((sscanf(argv[i+1], "%"SCNu32"@%"SCNx64, &vmid, &address)) != 2) {
								fprintf(stderr, "[ERROR]: Must specify a VMID for the --vm-decode command\n");
								exit(EXIT_FAILURE);
							}

						// sometimes people forget the 0x when using different hubs...
						if ((vmid & 0xFF) > 15) {
							fprintf(stderr, "[WARNING]: VMID > 15 is likely a typo on the command line (did you forget to add 0x?)\n");
						}

						sscanf(argv[i+2], "%"SCNx32, &size);

						// imply user hub if hub name specified
						if (asic->options.hub_name[0])
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

						argflags[i] = 1;
						argflags[i+1] = 1;
						argflags[i+2] = 1;

						// allow specifying the vmid in hex as well so
						// people can add the HUB flags more easily
						if ((sscanf(argv[i+1], "0x%"SCNx32"@%"SCNx64, &vmid, &address)) != 2)
							if ((sscanf(argv[i+1], "%"SCNu32"@%"SCNx64, &vmid, &address)) != 2) {
								sscanf(argv[i+1], "%"SCNx64, &address);
								vmid = UMR_LINEAR_HUB;
							}

						// imply user hub if hub name specified
						if (asic->options.hub_name[0])
							vmid |= UMR_USER_HUB;

						// sometimes people forget the 0x when using different hubs...
						if ((vmid & 0xFF) > 15) {
							fprintf(stderr, "[WARNING]: VMID > 15 is likely a typo on the command line (did you forget to add 0x?)\n");
						}

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

						argflags[i] = 1;
						argflags[i+1] = 1;
						argflags[i+2] = 1;

						// allow specifying the vmid in hex as well so
						// people can add the HUB flags more easily
						if ((sscanf(argv[i+1], "0x%"SCNx32"@%"SCNx64, &vmid, &address)) != 2)
							if ((sscanf(argv[i+1], "%"SCNu32"@%"SCNx64, &vmid, &address)) != 2) {
								sscanf(argv[i+1], "%"SCNx64, &address);
								vmid = UMR_LINEAR_HUB;
							}

						// imply user hub if hub name specified
						if (asic->options.hub_name[0])
							vmid |= UMR_USER_HUB;

						// sometimes people forget the 0x when using different hubs...
						if ((vmid & 0xFF) > 15) {
							fprintf(stderr, "[WARNING]: VMID > 15 is likely a typo on the command line (did you forget to add 0x?)\n");
						}

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
						uint32_t data, vmid;

						argflags[i] = 1;
						argflags[i+1] = 1;
						argflags[i+2] = 1;

						// allow specifying the vmid in hex as well so
						// people can add the HUB flags more easily
						if ((sscanf(argv[i+1], "0x%"SCNx32"@%"SCNx64, &vmid, &address)) != 2)
							if ((sscanf(argv[i+1], "%"SCNu32"@%"SCNx64, &vmid, &address)) != 2) {
								sscanf(argv[i+1], "%"SCNx64, &address);
								vmid = UMR_LINEAR_HUB;
							}

						// imply user hub if hub name specified
						if (asic->options.hub_name[0])
							vmid |= UMR_USER_HUB;

						// sometimes people forget the 0x when using different hubs...
						if ((vmid & 0xFF) > 15) {
							fprintf(stderr, "[WARNING]: VMID > 15 is likely a typo on the command line (did you forget to add 0x?)\n");
						}

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
						uint32_t size, vmid;

						argflags[i] = 1;
						argflags[i+1] = 1;
						argflags[i+2] = 1;

						// allow specifying the vmid in hex as well so
						// people can add the HUB flags more easily
						if ((sscanf(argv[i+1], "0x%"SCNx32"@%"SCNx64, &vmid, &address)) != 2)
							if ((sscanf(argv[i+1], "%"SCNu32"@%"SCNx64, &vmid, &address)) != 2) {
								sscanf(argv[i+1], "%"SCNx64, &address);
								vmid = UMR_LINEAR_HUB;
							}

						// imply user hub if hub name specified
						if (asic->options.hub_name[0])
							vmid |= UMR_USER_HUB;

						sscanf(argv[i+2], "%"SCNx32, &size);
						if (!size) {
							struct umr_shaders_pgm shader;
							shader.vmid = vmid;
							shader.addr = address;
							size = umr_compute_shader_size(asic, asic->options.vm_partition, &shader);
						}
						umr_vm_disasm(asic, stdout, asic->options.vm_partition, vmid, address, 0, size, 0, NULL);

						i += 2;
					} else {
						fprintf(stderr, "[ERROR]: --vm-disasm requires two parameters\n");
						return EXIT_FAILURE;
					}
				} else if (!strcmp(argv[i], "-prof") || !strcmp(argv[i], "--profiler")) {
					if (i + 1 < argc) {
						int n = 0, samples = -1, type = -1;

						argflags[i] = 1;
						argflags[i+1] = 1;

						if (i + 2 < argc && argv[i+2][0] != '-') {
							argflags[i+2] = 1;
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
				} else if (!strcmp(argv[i], "--header-dump") || !strcmp(argv[i], "-hd")) {
					if (i + 1 < argc) {
						int n;
						argflags[i] = 1;
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
					if (i + 1 < argc) {
						argflags[i] = 1;
						argflags[i+1] = 1;
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
					argflags[i] = 1;
					umr_power(asic);
				} else if (!strcmp(argv[i], "--clock-scan") || !strcmp(argv[i], "-cs")) {
					argflags[i] = 1;
					if (i + 1 < argc) {
						argflags[i+1] = 1;
						umr_clock_scan(asic, argv[i+1]);
						i++;
					} else {
						umr_clock_scan(asic, NULL);
					}
				} else if (!strcmp(argv[i], "--clock-manual") || !strcmp(argv[i], "-cm")) {
					argflags[i] = 1;
					if (i + 1 < argc) {
						argflags[i+1] = 1;
						argflags[i+2] = 1;
						umr_clock_manual(asic, argv[i+1], argv[i+2]);
						i += 2;
					} else {
						umr_set_clock_performance(asic, "manual");
						if (umr_check_clock_performance(asic, clockperformance, sizeof(clockperformance)) != 0)
							printf("power_dpm_force_performance_level: %s", clockperformance);
					}
				} else if (!strcmp(argv[i], "--clock-high") || !strcmp(argv[i], "-ch")) {
					argflags[i] = 1;
					umr_set_clock_performance(asic, "high");
					if (umr_check_clock_performance(asic, clockperformance, sizeof(clockperformance)) != 0)
						printf("power_dpm_force_performance_level: %s", clockperformance);
				} else if (!strcmp(argv[i], "--clock-low") || !strcmp(argv[i], "-cl")) {
					argflags[i] = 1;
					umr_set_clock_performance(asic, "low");
					if (umr_check_clock_performance(asic, clockperformance, sizeof(clockperformance)) != 0)
						printf("power_dpm_force_performance_level: %s", clockperformance);
				} else if (!strcmp(argv[i], "--clock-auto") || !strcmp(argv[i], "-ca")) {
					argflags[i] = 1;
					umr_set_clock_performance(asic, "auto");
					if (umr_check_clock_performance(asic, clockperformance, sizeof(clockperformance)) != 0)
						printf("power_dpm_force_performance_level: %s", clockperformance);
				} else if (!strcmp(argv[i], "--ppt-read") ||
					   !strcmp(argv[i], "--ppt_read") ||
					   !strcmp(argv[i], "-pptr")) {
					if (i + 1 < argc) {
						argflags[i] = 1;
						argflags[i+1] = 1;
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
					int delay = 0;
					argflags[i] = 1;
					if (i + 1 < argc && sscanf(argv[i+1], "%d", &delay) == 1) {
						argflags[i+1] = 1;
						++i;
					}
					if (umr_print_gpu_metrics(asic, delay) != 0)
						fprintf(stderr, "[ERROR]: Cannot print pp table info.\n");
				} else if (!strcmp(argv[i], "--vbios-info") ||
					   !strcmp(argv[i], "--vbios_info") ||
					   !strcmp(argv[i], "-vi")) {
					argflags[i] = 1;
					if (umr_print_vbios_info(asic) != 0)
						fprintf(stderr, "[ERROR]: Cannot print vbios info.\n");
				} else if (!strcmp(argv[i], "--runlist") || !strcmp(argv[i], "-rls")) {
					char busaddr[64];
					if (i + 1 < argc) {
						int node = atoi(argv[i + 1]); ++i;
						argflags[i] = 1;
						argflags[i+1] = 1;
						if (umr_kfd_topo_get_pci_busaddr(node, busaddr)) {
							return EXIT_FAILURE;
						}
						sscanf(busaddr, "%04x:%02x:%02x.%01x", &options.pci.domain, &options.pci.bus, &options.pci.slot, &options.pci.func);
						asic->options.use_pci = 0;
						// TODO: it'd be nice to get VMID from the PASID so we can enable these
						asic->options.no_follow_ib = 1;
						asic->options.no_follow_shader = 1;
						asic->options.no_follow_loadx = 1;
						umr_dump_runlists(asic, node);
					} else {
						fprintf(stderr, "[ERROR]: --runlist requires one parameter\n");
						return EXIT_FAILURE;
					}
				} else if (!strcmp(argv[i], "--dump-discovery-table") || !strcmp(argv[i], "-ddt")) {
					argflags[i] = 1;
					umr_dump_discovery_table_info(asic, NULL);
				} else if (!strcmp(argv[i], "--print-cpg") || !strcmp(argv[i], "-cpg")) {
					argflags[i] = 1;
					umr_print_cpg(asic);
				} else if (!strcmp(argv[i], "--print-cpc") || !strcmp(argv[i], "-cpc")) {
					argflags[i] = 1;
					umr_print_cpc(asic);
				} else if (!strcmp(argv[i], "--print-sdma") || !strcmp(argv[i], "-sdma")) {
					argflags[i] = 1;
					umr_print_sdma(asic);
				} else if (!strcmp(argv[i], "--rumr-export-asic")) {
					struct rumr_buffer *buf;
					argflags[i] = 1;
					if (!asic)
						asic = get_asic();
					buf = rumr_serialize_asic(asic);
					rumr_save_serialized_asic(asic, buf);
					rumr_buffer_free(buf);
				} else if (!strcmp(argv[i], "--rumr-server")) {
					struct rumr_comm_funcs *cf;
					char *cfp;
					struct rumr_server_state st;
					argflags[i] = 1;
					if (!asic)
						asic = get_asic();
					if (i + 1 < argc) {
						cf = rumr_get_cf(argv[i+1], &cfp);
						++i;
						st.asic = asic;
						if (rumr_server_bind(&st, cf, cfp)) {
							return EXIT_FAILURE;
						}
						for (;;) {
							rumr_server_accept(&st);
							while (!rumr_server_loop(&st));
						}
					} else {
						fprintf(stderr, "[ERROR]: --rumr-server requires one parameter\n");
						return EXIT_FAILURE;
					}
		#if UMR_SERVER
				} else if (!strcmp(argv[i], "--server")) {
					char *url = (i < argc - 1) ? argv[i + 1] : "tcp://*:1234";
					run_server_loop(url, NULL);
		#endif
				}
			}
		}
	}

stopprocessingcommands:

	for (i = 1; i < argc; i++) {
		if (!argflags[i]) {
			fprintf(stderr, "[WARNING]: Command line argument #%d [%s] was not understood\n", i, argv[i]);
		}
	}

	free(argflags);

	if (argc == 1) {
		printf("User Mode Register debugger v%s for AMDGPU devices (build: %s [%s], date: %s), Copyright (c) 2025, AMD Inc.\n\n"
			   "Use '--help' for a list of commands and options.\n",
			    UMR_BUILD_VER, UMR_BUILD_REV, UMR_BUILD_BRANCH, __DATE__);
	}

	if (options.use_xgmi) {
		// the parent 'asic' is included in the nodes array
		int n;
		for (n = 0; asic->config.xgmi.nodes[n].asic; n++)
			umr_close_asic(asic->config.xgmi.nodes[n].asic);
	} else if (asic) {
		if (client_st.asic == asic) {
			rumr_client_close(&client_st);
		} else {
			umr_close_asic(asic);
		}
	}

	if (th) {
		umr_free_test_harness(th);
	}

	if (options.export_model) {
		fprintf(stderr, "[NOTE]: ASIC model exported uses FAMILY_NV family and IS_APU=0 flag, change these as appropriate.\n");
	}
}

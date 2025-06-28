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
#include <umr_rumr.h>
#include <stdarg.h>

/* Implementation of "the" client side.  This should be
 * static for all cases as this binds to the umr library
 * for client services
 */

/* helper used to send opcodes to the server */
static struct rumr_buffer *send_opcode(struct rumr_client_state *state, uint32_t opcode, int nparam, ...)
{
	va_list ap;
	struct rumr_buffer *buf;
	int r;

	va_start(ap, nparam);
	buf = rumr_buffer_init();
	rumr_buffer_add_uint32(buf, (opcode << 10) | (RUMR_VERSION << 1)); // header word
	while (nparam--) {
		uint32_t next;
		next = va_arg(ap, uint32_t);
		rumr_buffer_add_uint32(buf, next);
	}
	r = state->comm.tx(&state->comm, buf);
	rumr_buffer_free(buf);
	buf = NULL;
	if (r) {
		state->log_msg("[ERROR]: Could not transmit opcode to server.\n");
		goto error;
	}

	// there is no return packet
	if (opcode == RUMR_OP_GOODBYE)
		goto error;

	// return reply from server
	r = state->comm.rx(&state->comm, &buf);
	if (!r && buf) {
		uint32_t reply;
		// ensure version and server bit is correct
		reply = rumr_buffer_read_uint32(buf);
		if (((reply>>1)&0xFF) != RUMR_VERSION) {
			state->log_msg("[ERROR]: Incorrect server version returned from server\n");
			rumr_buffer_free(buf);
			return NULL;
		}
		if (!(reply&1)) {
			state->log_msg("[ERROR]: Incorrect server flag returned from server\n");
			rumr_buffer_free(buf);
			return NULL;
		}
	} else {
		rumr_buffer_free(buf);
		buf = NULL;
	}
error:
	va_end(ap);
	return buf;
}

static struct rumr_buffer *send_opcode_buf(struct rumr_client_state *state, uint32_t opcode, uint32_t *pkt, uint32_t pktsize)
{
	struct rumr_buffer *buf;
	int r;

	buf = rumr_buffer_init();
		rumr_buffer_add_uint32(buf, (opcode << 10) | (RUMR_VERSION << 1)); // header word
		rumr_buffer_add_data(buf, pkt, pktsize*4);
	r = state->comm.tx(&state->comm, buf);
	rumr_buffer_free(buf);
	buf = NULL;
	if (r) {
		state->log_msg("[ERROR]: Could not transmit opcode to server.\n");
		goto error;
	}

	// return reply from server
	r = state->comm.rx(&state->comm, &buf);
	if (!r && buf) {
		uint32_t reply;
		// ensure version and server bit is correct
		reply = rumr_buffer_read_uint32(buf);
		if (((reply>>1)&0xFF) != RUMR_VERSION) {
			state->log_msg("[ERROR]: Incorrect server version returned from server\n");
			rumr_buffer_free(buf);
			return NULL;
		}
		if (!(reply&1)) {
			state->log_msg("[ERROR]: Incorrect server flag returned from server\n");
			rumr_buffer_free(buf);
			return NULL;
		}
	} else {
		rumr_buffer_free(buf);
		buf = NULL;
	}
error:
	return buf;
}

// handle VRAM/SRAM reads/writes and DMA translations
static int mem_op(struct umr_asic *asic, uint64_t *addr, uint32_t size, void *dst, int write_en, int vram_en)
{
	struct rumr_buffer *buf;
	int subop;
	uint32_t *pkt, n;
	struct rumr_client_state *state = asic->mem_funcs.data;

	pkt = calloc(4 + (write_en ? size >> 2 : 0), sizeof pkt[0]);
	if (!pkt) {
		state->log_msg("[ERROR]: Out of memory\n");
		return -1;
	}

	if (!dst) {
		subop = 2;
	} else {
		subop = (vram_en) ? 0 : 1;
	}

	pkt[0] = (*addr & 0xFFFFFFFFULL);
	pkt[1] = (*addr >> 32ULL);
	pkt[2] = (write_en ? (1<<2) : 0) | (subop);
	pkt[3] = size;
	n = 0;
	if (write_en && dst) {
		uint32_t *pbuf = dst;
		for (n = 0; n < (size >> 2); n++) {
			pkt[4 + n] = pbuf[n];
		}
	}
	n += 4;
	buf = send_opcode_buf(asic->mem_funcs.data, RUMR_OP_MEM_ACCESS, pkt, n);
	free(pkt);

	if (!buf || rumr_buffer_read_uint32(buf) != 1) {
		state->log_msg("[ERROR]: Could not transmit memory opcode.\n");
		rumr_buffer_free(buf);
		return -1;
	}

	*addr  =  rumr_buffer_read_uint32(buf);
	*addr |=  (uint64_t)rumr_buffer_read_uint32(buf) << 32ULL;

	if (!write_en && subop != 2) {
		rumr_buffer_read_data(buf, dst, size);
	}

	rumr_buffer_free(buf);
	return 0;
}


/** access_sram -- Access System RAM
 * @asic:  The device the memory is bound to
 * @address: The address relative to the GPUs bus (might not be a physical system memory access)
 * @size: Number of bytes
 * @dst: Buffer to read/write
 * @write_en: true for write, false for read
 */
static int access_sram(struct umr_asic *asic, uint64_t address, uint32_t size, void *dst, int write_en)
{
	return mem_op(asic, &address, size, dst, write_en, 0);
}

/** access_linear_vram -- Access Video RAM
 * @asic:  The device the memory is bound to
 * @address: The address relative to the GPUs start of VRAM (or relative to the start of an XGMI map)
 * @size: Number of bytes
 * @dst: Buffer to read/write
 * @write_en: true for write, false for read
 */
static int access_linear_vram(struct umr_asic *asic, uint64_t address, uint32_t size, void *data, int write_en)
{
	return mem_op(asic, &address, size, data, write_en, 1);
}

/** gpu_bus_to_cpu_address --	convert a GPU bound address for
 * 				system memory pages to CPU bound
 * 				addresses
 * @asic: The device the memory is bound to
 * @dma_addr: The GPU bound address
 *
 * Returns: The address the CPU can use to access the memory in
 * system memory
 */
static uint64_t gpu_bus_to_cpu_address(struct umr_asic *asic, uint64_t dma_addr)
{
	mem_op(asic, &dma_addr, 0, NULL, 0, 0);
	return dma_addr;
}

static int gprs_op(struct umr_asic *asic, struct umr_wave_data *wd, uint32_t thread, int v_or_s, uint32_t *dst)
{
	struct rumr_buffer *buf;
	uint32_t v, size;
	struct rumr_client_state *state = asic->gpr_read_funcs.data;

	if (v_or_s == 0) {
		uint32_t shift;
		if (asic->family <= FAMILY_CIK)
			shift = 3;  // on SI..CIK allocations were done in 8-dword blocks
		else
			shift = 4;  // on VI allocations are in 16-dword blocks
		size = 4 * ((umr_wave_data_get_bits(asic, wd, "ixSQ_WAVE_GPR_ALLOC", "SGPR_SIZE") + 1) << shift);
	} else {
		size = 4 * ((umr_wave_data_get_bits(asic, wd, "ixSQ_WAVE_GPR_ALLOC", "VGPR_SIZE") + 1) << asic->parameters.vgpr_granularity);
	}

	buf = send_opcode(asic->gpr_read_funcs.data, RUMR_OP_GPR_ACCESS, 9,
				(uint32_t)wd->se,
				(uint32_t)wd->sh,
				(uint32_t)wd->cu,
				(uint32_t)wd->simd,
				(uint32_t)wd->wave,
				(uint32_t)thread,
				(uint32_t)v_or_s,
				(uint32_t)0,
				(uint32_t)size);

	if (!buf || rumr_buffer_read_uint32(buf) != 1) {
		state->log_msg("[ERROR]: Could not transmit GPR opcode.\n");
		return -1;
	}

	v = rumr_buffer_read_uint32(buf) >> 2;
	if (v <= size) {
		rumr_buffer_read_data(buf, dst, size);
	}
	rumr_buffer_free(buf);

	if (v_or_s == 0) {
		// SGPR: read trap if any
		if (umr_wave_data_get_flag_trap_en(asic, wd) || umr_wave_data_get_flag_priv(asic, wd)) {
			// TODO: traps are 16 DWORDS right?
			size = 16 * 4;
			buf = send_opcode(asic->gpr_read_funcs.data, RUMR_OP_GPR_ACCESS, 8,
						(uint32_t)wd->se,
						(uint32_t)wd->sh,
						(uint32_t)wd->cu,
						(uint32_t)wd->simd,
						(uint32_t)thread,
						(uint32_t)v_or_s,
						(uint32_t)(0x6C * 4),
						(uint32_t)size); // TODO: size?

			if (!buf || rumr_buffer_read_uint32(buf) != 1) {
				state->log_msg("[ERROR]: Could not transmit GPR (trap) opcode.\n");
				return -1;
			}

			v = rumr_buffer_read_uint32(buf) >> 2;
			if (v <= size) {
				rumr_buffer_read_data(buf, &dst[0x6C], size);
			}
			rumr_buffer_free(buf);
		}
	}

	return 0;
}

/** read_vgprs -- Read VGPR data for a given wave and thread */
static int read_vgprs(struct umr_asic *asic, struct umr_wave_data *wd, uint32_t thread, uint32_t *dst)
{
	return gprs_op(asic, wd, thread, 1, dst);
}

/** read_sgprs -- Read VGPR data for a given wave */
static int read_sgprs(struct umr_asic *asic, struct umr_wave_data *wd, uint32_t *dst)
{
	return gprs_op(asic, wd, 0, 0, dst);
}

static int get_wave_status(struct umr_asic *asic, unsigned se, unsigned sh, unsigned cu, unsigned simd, unsigned wave, struct umr_wave_status *ws)
{
	uint32_t *ws_buf, wslen;
	struct rumr_buffer *buf;
	int r;
	struct rumr_client_state *state = asic->wave_funcs.data;

	ws_buf = calloc(256, sizeof *ws_buf);
	buf = send_opcode(asic->wave_funcs.data, RUMR_OP_WAVE_ACCESS, 5,
				(uint32_t)se,
				(uint32_t)sh,
				(uint32_t)cu,
				(uint32_t)simd,
				(uint32_t)wave);

	if (!buf || rumr_buffer_read_uint32(buf) != 1) {
		state->log_msg("[ERROR]: Could not transmit wavestatus opcode.\n");
		free(ws_buf);
		rumr_buffer_free(buf);
		return -1;
	}

	wslen = rumr_buffer_read_uint32(buf);
	if (wslen > 256) {
		free(ws_buf);
		rumr_buffer_free(buf);
		return -1;
	}
	rumr_buffer_read_data(buf, ws_buf, wslen);
	rumr_buffer_free(buf);
	r =  umr_parse_wave_data_gfx(asic, ws, ws_buf, wslen>>2);
	free(ws_buf);
	return r;
}

static int mmio_reg_op(struct umr_asic *asic, uint64_t addr, enum regclass type, uint32_t *value, int read_en)
{
	struct rumr_buffer *buf;
	struct rumr_client_state *state = asic->reg_funcs.data;

	if (asic->options.use_bank == 1) {
		// GRBM
		if (read_en) {
			buf = send_opcode(asic->reg_funcs.data, RUMR_OP_REG_ACCESS, 7,
					(uint32_t)(addr & 0xFFFFFFFFULL),		// ADDR_LO
					(uint32_t)(addr >> 32ULL),			// ADDR_HI
					(uint32_t)(1 | (1 << 1) | (type << 3)),		// ACCESS_BANK
					(uint32_t)asic->options.bank.grbm.se,
					(uint32_t)asic->options.bank.grbm.sh,
					(uint32_t)asic->options.bank.grbm.instance,
					(uint32_t)0);
		} else {
			buf = send_opcode(asic->reg_funcs.data, RUMR_OP_REG_ACCESS, 9,
					(uint32_t)(addr & 0xFFFFFFFFULL),		// ADDR_LO
					(uint32_t)(addr >> 32ULL),			// ADDR_HI
					(uint32_t)(0 | (1 << 1) | (type << 3)),		// ACCESS_BANK
					(uint32_t)asic->options.bank.grbm.se,
					(uint32_t)asic->options.bank.grbm.sh,
					(uint32_t)asic->options.bank.grbm.instance,
					(uint32_t)0,
					(uint32_t)(*value),
					(uint32_t)0);
		}
	} else if (asic->options.use_bank == 2) {
		// SRBM
		if (read_en) {
			buf = send_opcode(asic->reg_funcs.data, RUMR_OP_REG_ACCESS, 7,
					(uint32_t)(addr & 0xFFFFFFFFULL),		// ADDR_LO
					(uint32_t)(addr >> 32ULL),			// ADDR_HI
					(uint32_t)(1 | (1 << 2) | (type << 3)),		// ACCESS_BANK
					(uint32_t)asic->options.bank.srbm.me,
					(uint32_t)asic->options.bank.srbm.pipe,
					(uint32_t)asic->options.bank.srbm.queue,
					(uint32_t)asic->options.bank.srbm.vmid);
		} else {
			buf = send_opcode(asic->reg_funcs.data, RUMR_OP_REG_ACCESS, 9,
					(uint32_t)(addr & 0xFFFFFFFFULL),		// ADDR_LO
					(uint32_t)(addr >> 32ULL),			// ADDR_HI
					(uint32_t)(0 | (1 << 2) | (type << 3)),		// ACCESS_BANK
					(uint32_t)asic->options.bank.srbm.me,
					(uint32_t)asic->options.bank.srbm.pipe,
					(uint32_t)asic->options.bank.srbm.queue,
					(uint32_t)asic->options.bank.srbm.vmid,
					(uint32_t)(*value),
					(uint32_t)0);
		}
	} else {
		// No bank switching
		if (read_en) {
			buf = send_opcode(asic->reg_funcs.data, RUMR_OP_REG_ACCESS, 7,
					(uint32_t)(addr & 0xFFFFFFFFULL),		// ADDR_LO
					(uint32_t)(addr >> 32ULL),			// ADDR_HI
					(uint32_t)(1 | (type << 3)),			// ACCESS_BANK
					(uint32_t)0,
					(uint32_t)0,
					(uint32_t)0,
					(uint32_t)0);
		} else {
			buf = send_opcode(asic->reg_funcs.data, RUMR_OP_REG_ACCESS, 9,
					(uint32_t)(addr & 0xFFFFFFFFULL),		// ADDR_LO
					(uint32_t)(addr >> 32ULL),			// ADDR_HI
					(uint32_t)(0 | (type << 3)),			// ACCESS_BANK
					(uint32_t)0,
					(uint32_t)0,
					(uint32_t)0,
					(uint32_t)0,
					(uint32_t)(*value),
					(uint32_t)0);
		}
	}

	if (!buf || rumr_buffer_read_uint32(buf) != 1) {
		state->log_msg("[ERROR]: Could not transmit register opcode.\n");
		rumr_buffer_free(buf);
		return -1;
	}

	if (read_en) {
		*value = rumr_buffer_read_uint32(buf);
	}
	rumr_buffer_free(buf);
	return 0;
}

/** read_reg -- Read a register
 * @asic: The device the register is from
 * @addr:  The byte address of the register to read
 * @type:  REG_MMIO or REG_SMC
 */
static uint32_t read_reg(struct umr_asic *asic, uint64_t addr, enum regclass type)
{
	uint32_t v;
	v = 0xBEBEBEEF;
	(void)mmio_reg_op(asic, addr, type, &v, 1);
	return v;
}

/** write_reg -- Write a register
 * @asic: The device the register is from
 * @addr: The byte address of the register to write
 * @value: The 32-bit value to write
 * @type: REG_MMIO or REG_SMC
 */
static int write_reg(struct umr_asic *asic, uint64_t addr, uint32_t value, enum regclass type)
{
	return mmio_reg_op(asic, addr, type, &value, 0);
}

static void *read_ring_data(struct umr_asic *asic, char *ringname, uint32_t *ringsize)
{
	char linebuf[128];
	uint32_t *pkt = (uint32_t*)&linebuf[0];
	struct rumr_buffer *buf;
	struct rumr_client_state *state = asic->ring_func.data;
	void *ret;

	memset(linebuf, 0, sizeof linebuf);
	strncpy(linebuf, ringname, (sizeof linebuf) - 1);

	buf = send_opcode_buf(state, RUMR_OP_RING_ACCESS, pkt, (sizeof linebuf) / 4);
	if (!buf || rumr_buffer_read_uint32(buf) != 1) {
		return NULL;
	}

	*ringsize = rumr_buffer_read_uint32(buf);
	ret = calloc(1, *ringsize + 12);
	rumr_buffer_read_data(buf, ret, *ringsize + 12); // 12 bytes for RPTR/WPTR/RPTR(cache)
	rumr_buffer_free(buf);
	return ret;
}

/**
 * @brief Discover the ASIC connected to the RUMR client.
 *
 * This function sends a discovery opcode to the server to gather information about the connected ASIC.
 * It initializes the ASIC model in the client state using the data received from the server.
 *
 * @param state Pointer to the RUMR client state structure.
 * @return int Returns 0 on success, -1 on failure.
 */
int rumr_client_discover(struct rumr_client_state *state)
{
	struct rumr_buffer *buf;

	buf = send_opcode(state, RUMR_OP_DISCOVER, 0);
	if (!buf) {
		state->log_msg("[ERROR]: Could not transmit discoever opcode.\n");
		return -1;
	}

	state->asic = rumr_parse_serialized_asic(buf);

	return state->asic ? 0 : -1;
}

/**
 * @brief Establish a connection to the RUMR server and discover the ASIC.
 *
 * This function initializes the communication with the RUMR server using the provided communication functions
 * and address. It establishes a connection, discovers the ASIC connected to the server, and binds various callback
 * functions for memory access, register operations, wave data retrieval, ring data reading, shader disassembly,
 * and GPR read operations.
 *
 * @param state Pointer to the RUMR client state structure.
 * @param cf    Pointer to the communication function structure that defines how to connect and communicate with the server.
 * @param addr  The address of the server to connect to.
 * @return int Returns 0 on success, a negative error code on failure.
 */
int rumr_client_connect(struct rumr_client_state *state, struct rumr_comm_funcs *cf, char *addr)
{
	int r;

	state->comm = *cf;
	state->log_msg = state->comm.log_msg;

	r = state->comm.connect(&state->comm, addr);
	if (r < 0) {
		state->log_msg("[ERROR]: Could not establish connection to <%s>\n", addr);
		return r;
	}

	// init asic model
	r = rumr_client_discover(state);
	if (r < 0) {
		state->log_msg("[ERROR]: Could not discover ASIC\n");
		return r;
	}

	// now bind callbacks
		state->asic->err_msg = state->log_msg;
		state->asic->std_msg = state->log_msg;

	// memfuncs
		state->asic->mem_funcs.vm_message = state->log_msg;
		state->asic->mem_funcs.access_linear_vram = access_linear_vram;
		state->asic->mem_funcs.access_sram = access_sram;
		state->asic->mem_funcs.data = state;
		state->asic->mem_funcs.gpu_bus_to_cpu_address = gpu_bus_to_cpu_address;
	// regfuncs
		state->asic->reg_funcs.data = state;
		state->asic->reg_funcs.read_reg = read_reg;
		state->asic->reg_funcs.write_reg = write_reg;
	// wavefuncs
		state->asic->wave_funcs.data = state;
		state->asic->wave_funcs.get_wave_status = get_wave_status;
		state->asic->wave_funcs.get_wave_sq_info = umr_get_wave_sq_info;
	// ring funcs
		state->asic->ring_func.data = state;
		state->asic->ring_func.read_ring_data = read_ring_data;
	// shader
		state->asic->shader_disasm_funcs.disasm = umr_shader_disasm;
	// GPRs
		state->asic->gpr_read_funcs.read_sgprs = read_sgprs;
		state->asic->gpr_read_funcs.read_vgprs = read_vgprs;
		state->asic->gpr_read_funcs.data = state;

	// default shader options
		if (state->asic->family <= FAMILY_VI) { // on gfx9+ hs/gs are opaque
			state->asic->options.shader_enable.enable_gs_shader = 1;
			state->asic->options.shader_enable.enable_hs_shader = 1;
		}
		state->asic->options.shader_enable.enable_vs_shader   = 1;
		state->asic->options.shader_enable.enable_ps_shader   = 1;
		state->asic->options.shader_enable.enable_es_shader   = 1;
		state->asic->options.shader_enable.enable_ls_shader   = 1;
		state->asic->options.shader_enable.enable_comp_shader = 1;

		if (state->asic->family > FAMILY_VI)
			state->asic->options.shader_enable.enable_es_ls_swap = 1;  // on >FAMILY_VI we swap LS/ES for HS/GS

	// default options
		state->asic->options.vm_partition = -1;

	// create mmio lookup accelerator
		umr_create_mmio_accel(state->asic);

	return 0;
}

/**
 * @brief Close the connection to the RUMR server and free resources.
 *
 * This function sends a goodbye opcode to the server to indicate that the client is disconnecting.
 * It also frees any resources associated with the ASIC model stored in the client state.
 *
 * @param state Pointer to the RUMR client state structure.
 */
void rumr_client_close(struct rumr_client_state *state)
{
	send_opcode(state, RUMR_OP_GOODBYE, 0);
	umr_free_asic(state->asic);
}

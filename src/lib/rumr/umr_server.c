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
#include <stdint.h>

/* An implementation of the Server side using UMR
 * to access the ASIC.  This is an example though useful for
 * breaking out of same-host sandboxes.  In reality down the road
 * servers will be written against other APIs for non-common hardware
 * configurations.
 */

/** rumr_server_bind: Bind a server state to a comms host address
 *
 * state: the server state
 * cf: A copy of the communication functions structure you want to use
 * host: The host string to pass to the comms bind function
 */
int rumr_server_bind(struct rumr_server_state *state, struct rumr_comm_funcs *cf, char *host)
{
	// create serialized asic we can use over and over
	memcpy(&state->comm, cf, sizeof *cf);
	state->log_msg = state->comm.log_msg;
	state->serialized_asic = rumr_serialize_asic(state->asic);
	if (!state->serialized_asic)
		return -1;
	state->log_msg("[VERBOSE]: Serialized ASIC is %"PRIu32" bytes long\n", state->serialized_asic->woffset);

	// bind communication for server
	return state->comm.bind(&state->comm, host);
}

/** rumr_server_accept: Accept a new client connection
 *
 * state: The server state to accept on
 *
 * This is called after bind and before the rumr_server_loop().
 */
int rumr_server_accept(struct rumr_server_state *state)
{
	state->log_msg("[VERBOSE]: Accepting a new client...\n");
	return state->comm.accept(&state->comm);
}

// handle a generic GPR access for both V and S GPRs
static int handle_op_gpr_access(struct rumr_server_state *state, struct rumr_buffer *inbuf, struct rumr_buffer *outbuf)
{
	struct {
		uint32_t
			se,
			sh,
			cu_wgp,
			simd,
			wave,
			thread,
			vgpr_or_sgpr,
			offset,
			size;
	} in;
	int r;
	uint32_t buf[1024];
	struct umr_asic *asic = state->asic;

	in.se = rumr_buffer_read_uint32(inbuf);
	in.sh = rumr_buffer_read_uint32(inbuf);
	in.cu_wgp = rumr_buffer_read_uint32(inbuf);
	in.simd = rumr_buffer_read_uint32(inbuf);
	in.wave = rumr_buffer_read_uint32(inbuf);
	in.thread = rumr_buffer_read_uint32(inbuf);
	in.vgpr_or_sgpr = rumr_buffer_read_uint32(inbuf);
	in.offset = rumr_buffer_read_uint32(inbuf);
	in.size = rumr_buffer_read_uint32(inbuf);

	r = umr_linux_read_gpr_gprwave_raw(asic, in.vgpr_or_sgpr,
			in.thread, in.se, in.sh, in.cu_wgp, in.wave, in.simd, in.offset, in.size, buf);

	rumr_buffer_add_uint32(outbuf, r>0 ? 1 : 0);
	if (r > 0) {
		rumr_buffer_add_uint32(outbuf, r);
		rumr_buffer_add_data(outbuf, buf, r);
	}
	return 0;
}

// handle a generic WAVE data request
// note the format has to be in the same format the kernel would return
// so check the various amdgpu/gfx*.c for *_read_wave_data() functions
// to see which format your particular ASIC should return
static int handle_op_wave_access(struct rumr_server_state *state, struct rumr_buffer *inbuf, struct rumr_buffer *outbuf)
{
	struct {
		uint32_t
			se,
			sh,
			cu_wgp,
			simd,
			wave;
	} in;
	uint32_t buf[64];
	struct umr_asic *asic = state->asic;
	int r;

	in.se = rumr_buffer_read_uint32(inbuf);
	in.sh = rumr_buffer_read_uint32(inbuf);
	in.cu_wgp = rumr_buffer_read_uint32(inbuf);
	in.simd = rumr_buffer_read_uint32(inbuf);
	in.wave = rumr_buffer_read_uint32(inbuf);

	r = umr_get_wave_status_raw(asic, in.se, in.sh, in.cu_wgp, in.simd, in.wave, buf);
	rumr_buffer_add_uint32(outbuf, r>0 ? 1 : 0);
	if (r > 0) {
		rumr_buffer_add_uint32(outbuf, r);
		rumr_buffer_add_data(outbuf, buf, r);
	}
	return 0;
}

// handle a generic memory access (read/write, iommu mapping)
static int handle_op_mem_access(struct rumr_server_state *state, struct rumr_buffer *inbuf, struct rumr_buffer *outbuf)
{
	struct {
		uint32_t
			addr_lo,
			addr_hi,
			options,
			size;

		int
			subcommand,
			rw;
	} in;
	uint64_t addr;
	int r;
	struct umr_asic *asic = state->asic;

	in.addr_lo = rumr_buffer_read_uint32(inbuf);
	in.addr_hi = rumr_buffer_read_uint32(inbuf);
		addr = ((uint64_t)in.addr_lo) | ((uint64_t)in.addr_hi << 32ULL);
	in.options = rumr_buffer_read_uint32(inbuf);
		in.subcommand = in.options & 3;
		in.rw = (in.options >> 2) & 1;
	in.size = rumr_buffer_read_uint32(inbuf);

	if (in.subcommand == 3) {
		state->log_msg("[ERROR]: Invalid mem access subcommand\n");
		return -1;
	}

	if (in.size & 3) {
		state->log_msg("[ERROR]: Invalid size request (must be multiple of 4)\n");
		return -1;
	}

	if (addr & 3) {
		state->log_msg("[ERROR]: Invalid address request (must be multiple of 4)\n");
		return -1;
	}

	if (in.rw && in.subcommand == 2) {
		state->log_msg("[ERROR]: Cannot specify write with IOMMU translate\n");
		return -1;
	}

	if (in.subcommand == 0 || in.subcommand == 1) {
		// we're accessing memory
		if (in.rw == 0) {
			// reading
			void *dst;

			dst = calloc(1, in.size);
			if (in.subcommand == 1) {
				// read from sysmem
				r = umr_access_sram(asic, addr, in.size, dst, 0);
			} else {
				// read from vram
				r = umr_access_vram(asic, asic->options.vm_partition, UMR_LINEAR_HUB, addr, in.size, dst, 0, NULL);
			}
			rumr_buffer_add_uint32(outbuf, r ? 0 : 1);
			rumr_buffer_add_uint32(outbuf, in.addr_lo);
			rumr_buffer_add_uint32(outbuf, in.addr_hi);
			if (!r)
				rumr_buffer_add_data(outbuf, dst, in.size);
			free(dst);
		} else {
			// writing
			void *dst;

			if (in.size != (inbuf->size - inbuf->roffset)) {
				state->log_msg("[ERROR]: Write buffer size does not match remaining packet size\n");
				return -1;
			}

			dst = &inbuf->data[inbuf->roffset];
			if (in.subcommand == 0) {
				// write to sysmem
				r = umr_access_sram(asic, addr, in.size, dst, 1);
			} else {
				// write to vram
				r = umr_access_vram(asic, asic->options.vm_partition, UMR_LINEAR_HUB, addr, in.size, dst, 1, NULL);
			}
			rumr_buffer_add_uint32(outbuf, r ? 0 : 1);
			rumr_buffer_add_uint32(outbuf, in.addr_lo);
			rumr_buffer_add_uint32(outbuf, in.addr_hi);
		}
	} else {
		// we're translating an IOMMU address
		addr = umr_vm_dma_to_phys(asic, addr);
		rumr_buffer_add_uint32(outbuf, 1);
		rumr_buffer_add_uint32(outbuf, (addr & 0xFFFFFFFFULL));
		rumr_buffer_add_uint32(outbuf, (addr >> 32) & 0xFFFFFFFFULL);
	}

	return 0;
}

// handle generic mmio/etc register access
static int handle_op_reg_access(struct rumr_server_state *state, struct rumr_buffer *inbuf, struct rumr_buffer *outbuf)
{
	struct {
		uint32_t
			reg_addr_lo,
			reg_addr_hi,
			access_bank, // access/GRBM/SRBM/TYPE
			se_or_me,
			sh_or_pipe,
			instance_or_queue,
			vmid,
			value_lo,
			value_hi;

		int
			access,
			grbm_index,
			srbm_index,
			type;
	} in;
	uint64_t readval, addr;
	struct umr_asic *asic = state->asic;

	in.reg_addr_lo = rumr_buffer_read_uint32(inbuf);
	in.reg_addr_hi = rumr_buffer_read_uint32(inbuf);
		addr = ((uint64_t)in.reg_addr_lo) | ((uint64_t)in.reg_addr_hi << 32ULL);
	in.access_bank = rumr_buffer_read_uint32(inbuf);
		in.access = in.access_bank & 1;
		in.grbm_index = (in.access_bank >> 1) & 1;
		in.srbm_index = (in.access_bank >> 2) & 1;
		in.type = (in.access_bank >> 3) & 0xFF;
	in.se_or_me = rumr_buffer_read_uint32(inbuf);
	in.sh_or_pipe = rumr_buffer_read_uint32(inbuf);
	in.instance_or_queue = rumr_buffer_read_uint32(inbuf);
	in.vmid = rumr_buffer_read_uint32(inbuf);
	if (in.access == 0) { // 0 == write
		in.value_lo = rumr_buffer_read_uint32(inbuf);
		in.value_hi = rumr_buffer_read_uint32(inbuf);
	}

	if (in.grbm_index && in.srbm_index) {
		state->log_msg("[ERROR]: Cannot set both GRBM and SRBM index\n");
		return -1;
	}

	if (in.grbm_index) {
		asic->options.use_bank           = 1;
		asic->options.bank.grbm.se       = in.se_or_me;
		asic->options.bank.grbm.sh       = in.sh_or_pipe;
		asic->options.bank.grbm.instance = in.instance_or_queue;
	}
	if (in.srbm_index) {
		asic->options.use_bank           = 2;
		asic->options.bank.srbm.me	 = in.se_or_me;
		asic->options.bank.srbm.queue	 = in.instance_or_queue;
		asic->options.bank.srbm.pipe	 = in.sh_or_pipe;
		asic->options.bank.srbm.vmid	 = in.vmid;
	}

	if (in.access == 0) {
		umr_write_reg(asic, addr, in.value_lo | ((uint64_t)in.value_hi << 32ULL), in.type);
	} else {
		readval = umr_read_reg(asic, addr, in.type);
	}

	// turn off bank selection
	asic->options.use_bank = 0;

	rumr_buffer_add_uint32(outbuf, 1); // STATUS==1
	if (in.access == 1) {
		rumr_buffer_add_uint32(outbuf, readval & 0xFFFFFFFF);
		rumr_buffer_add_uint32(outbuf, readval >> 32ULL);
	}
	return 0;
}

// handle reading a debugfs format RING file basically the format
// is the contents of the ring with a 12-byte header consisting of
// RPTR | WPTR | (cached) RPTR
// so if you read the ring directly from memory make sure to add
// those 12 bytes at the start, but they're not counted in the ringsize
// directly (ringsize == size of ring buffer only)
static int handle_op_ring_access(struct rumr_server_state *state, struct rumr_buffer *inbuf, struct rumr_buffer *outbuf)
{
	struct {
		char ringname[128];
	} in;
	void *ret;
	struct umr_asic *asic = state->asic;
	uint32_t ringsize;

	rumr_buffer_read_data(inbuf, in.ringname, sizeof in.ringname);
	in.ringname[sizeof(in.ringname) - 1] = 0;

	ret = umr_read_ring_data(asic, in.ringname, &ringsize);
	rumr_buffer_add_uint32(outbuf, ret ? 1 : 0);
	if (ret) {
		rumr_buffer_add_uint32(outbuf, ringsize);
		rumr_buffer_add_data(outbuf, ret, ringsize + 12);
		free(ret);
	}
	return 0;
}

/** rumr_server_loop: Handles one command from client
 * state: The server state
 *
 * Returns 0 on success, 1 if the client voluntarily disconnected, <0 on error
 */
int rumr_server_loop(struct rumr_server_state *state)
{
	struct rumr_buffer *rbuf, *outbuf;
	uint32_t header;
	int r;

	// receive client packet
	if (state->comm.rx(&state->comm, &rbuf)) {
		state->log_msg("[ERROR]: Could not receive client packet\n");
		return -1;
	}

	// read header
		header = rumr_buffer_read_uint32(rbuf);

	// sanity check
		// only accept packets from clients
		if (header & 0) {
			state->log_msg("[ERROR]: Packet header must not set SERVER bit\n");
			return -1;
		}

		if (((header >> 1) & 0xFF) != RUMR_VERSION) {
			state->log_msg("[ERROR]: Packet header version number does not match what we are expecting\n");
			return -1;
		}

	// handle opcodes
		outbuf = rumr_buffer_init();
		if (!outbuf) {
			rumr_buffer_free(rbuf);
			return -1;
		}
		outbuf->woffset = 4; // skip over packet header

		r = 0;
		switch ((header >> 10) & 0xFF) {
			case RUMR_OP_DISCOVER:
				rumr_buffer_add_buffer(outbuf, state->serialized_asic);
				break;
			case RUMR_OP_REG_ACCESS:
				r = handle_op_reg_access(state, rbuf, outbuf);
				break;
			case RUMR_OP_MEM_ACCESS:
				r = handle_op_mem_access(state, rbuf, outbuf);
				break;
			case RUMR_OP_WAVE_ACCESS:
				r = handle_op_wave_access(state, rbuf, outbuf);
				break;
			case RUMR_OP_GPR_ACCESS:
				r = handle_op_gpr_access(state, rbuf, outbuf);
				break;
			case RUMR_OP_RING_ACCESS:
				r = handle_op_ring_access(state, rbuf, outbuf);
				break;
			case RUMR_OP_GOODBYE:
				state->comm.closeconn(&state->comm);
				rumr_buffer_free(rbuf);
				rumr_buffer_free(outbuf);
				return 1;
			default:
				state->log_msg("[ERROR]: Invalid packet upcode (0x%" PRIx32 ")\n", (header >> 10) & 0xFF);
				r = -1;
				goto error;
		}

		if (r) {
			goto error;
		}

	// check if failed
		if (outbuf->failed) {
			state->log_msg("[ERROR]: Failed to create output buffer\n");
			rumr_buffer_free(rbuf);
			rumr_buffer_free(outbuf);
			return -1;
		}

	// fix packet header
		header |= 1; // set SERVER flag
		memcpy(&outbuf->data[0], &header, 4);

	// transmit
		r = state->comm.tx(&state->comm, outbuf);
		if (r) {
			state->log_msg("[ERROR]: Could not send response buffer to client\n");
		}

	// free
error:
		rumr_buffer_free(rbuf);
		rumr_buffer_free(outbuf);

	return r;
}

/** rumr_server_close: Close down the server
 * state: The server state to close
 */
void rumr_server_close(struct rumr_server_state *state)
{
	rumr_buffer_free(state->serialized_asic);
	state->comm.close(&state->comm);
}

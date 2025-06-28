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
#ifndef RUMR_H_
#define RUMR_H_

#include <stdint.h>

// version of RUMR protocol
#define RUMR_VERSION 0x01

// amount of preheader space used by comms
// layer this allows transmitting "once"
// which so far for TCP speeds things up
// greatly
#define RUMR_BUFFER_PREHEADER 8

enum rumr_opcodes {
	RUMR_OP_DISCOVER=0x00,
	RUMR_OP_REG_ACCESS,
	RUMR_OP_MEM_ACCESS,
	RUMR_OP_WAVE_ACCESS,
	RUMR_OP_GPR_ACCESS,
	RUMR_OP_RING_ACCESS,
	RUMR_OP_GOODBYE,
};

struct rumr_buffer{
	uint8_t *data;
	uint32_t size, roffset, woffset;
	int failed;
};

struct rumr_comm_funcs {
	// opaque data for comm channel
	void *data;

	// connect to server
	int (*connect)(struct rumr_comm_funcs *cf, char *server_address);

	// bind to host address
	int (*bind)(struct rumr_comm_funcs *cf, char *host_address);

	// wait_for_client
	int (*accept)(struct rumr_comm_funcs *cf);

	// transmit buffer
	int (*tx)(struct rumr_comm_funcs *cf, struct rumr_buffer *buf);

	// receive buffer
	int (*rx)(struct rumr_comm_funcs *cf, struct rumr_buffer **buf);

	// close connections (when terminating)
	int (*close)(struct rumr_comm_funcs *cf);

	// close client connect on server side
	int (*closeconn)(struct rumr_comm_funcs *cf);

	// status
	int (*status)(struct rumr_comm_funcs *cf);

	// logging messages
	int (*log_msg)(const char *fmt, ...);
};

struct rumr_server_state {
	struct rumr_buffer *serialized_asic;
	void *asic;
	struct rumr_comm_funcs comm;
	int (*log_msg)(const char *fmt, ...);
};

// RUMR_SERVER_ONLY allows the inclusion
// of the rumr header without bringing in the umr
// headers as well for say backend servers not based on
// the umr libraries
#ifndef RUMR_SERVER_ONLY

#include <umr.h>

struct rumr_client_state {
	struct rumr_comm_funcs comm;
	struct umr_asic *asic;
	int (*log_msg)(const char *fmt, ...);
};

// client functions
int rumr_client_connect(struct rumr_client_state *state, struct rumr_comm_funcs *cf, char *addr);
void rumr_client_close(struct rumr_client_state *state);
int rumr_client_discover(struct rumr_client_state *state);
#endif

// buffer functions
struct rumr_buffer *rumr_buffer_init(void);

void rumr_buffer_add_buffer(struct rumr_buffer *buf, struct rumr_buffer *srcbuf);
void rumr_buffer_add_data(struct rumr_buffer *buf, void *data, uint32_t size);
void rumr_buffer_add_uint32(struct rumr_buffer *buf, uint32_t val);

void rumr_buffer_read_data(struct rumr_buffer *buf, void *data, uint32_t size);
uint32_t rumr_buffer_read_uint32(struct rumr_buffer *buf);

void rumr_buffer_free(struct rumr_buffer *buf);

struct rumr_buffer *rumr_buffer_load_file(const char *fname, char *database_path);

// server functions
int rumr_server_bind(struct rumr_server_state *state, struct rumr_comm_funcs *cf, char *host);
int rumr_server_accept(struct rumr_server_state *state);
int rumr_server_loop(struct rumr_server_state *state);
void rumr_server_close(struct rumr_server_state *state);


// serialized asic functions
struct rumr_buffer *rumr_serialize_asic(struct umr_asic *asic);
struct umr_asic *rumr_parse_serialized_asic(struct rumr_buffer *buf);
int rumr_save_serialized_asic(struct umr_asic *asic, struct rumr_buffer *buf);
struct rumr_buffer *rumr_load_serialized_asic(const char *fname, char *database_path);

// EXTERNS
// comms
extern const struct rumr_comm_funcs rumr_tcp_funcs;

#endif

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
#ifndef UMR_PACKET_H_
#define UMR_PACKET_H_

/* ==== Packet Processor opcode decoding ====
 * These functions deal with decoding the contents of various rings/IB in various packet formats
 */
enum umr_ring_type {
	UMR_RING_PM4,
	UMR_RING_PM4_LITE,
	UMR_RING_SDMA,
	UMR_RING_MES,
	UMR_RING_VPE,
	UMR_RING_UMSCH,
	UMR_RING_HSA,
	UMR_RING_VCN_DEC,
	UMR_RING_VCN_ENC,

	UMR_RING_GUESS,
	UMR_RING_UNK=0xFF, // if unknown
};

/* Multimedia VCN CMD_MSG_BUFFER Messages
 * We are not interested in other messages */
struct umr_vcn_cmd_message {
	// VMID
	uint32_t vmid;

	// size in bytes for ENC IB, not used for DEC IB
	uint32_t size;

	// encode or decode (0 == DEC, 1 == ENC)
	uint32_t type;

	// message buffer address in VM space
	uint64_t addr;

	// addr offset in the IB block
	uint64_t from;

	// command for this message, useful for DEC IB
	uint32_t cmd;

	// message buffer read from addr above, used for ENC
	uint32_t *buf;

	struct umr_vcn_cmd_message *next; // in case there are more
};


struct umr_stream_decode_ui {
	enum umr_ring_type rt;

	/** start_ib -- Start a new IB/buffer object
	 * ib_addr/ib_vmid: Address of the IB
	 * from_addr/from_vmid: Where does this reference come from?
	 * size: size of IB in DWORDs
	 * type: type of IB (which type of packets)
	 */
	void (*start_ib)(struct umr_stream_decode_ui *ui, uint64_t ib_addr, uint32_t ib_vmid, uint64_t from_addr, uint32_t from_vmid, uint32_t size, int type);

	/** unhandled_dword -- Print out a dword that doesn't match a valid packet header
	 * ib_addr/ib_vmid: address of dword
	 * dword: the value that doesn't decode to a valid header
	 */
	void (*unhandled_dword)(struct umr_stream_decode_ui *ui, uint64_t ib_addr, uint32_t ib_vmid, uint32_t dword);

	/** start_opcode -- Start a new opcode
	 * ib_addr/ib_vmid: Address of where packet is found
	 * opcode: The numeric value of the ocpode
	 * subop: any sub-opcode
	 * nwords: Number of DWORDS in this opcode
	 * opcode_name: Printable string name of opcode
	 * header: Raw header DWORD of this packet
	 * raw_data: Pointer to a buffer of length nwords containing the raw data of this packet (does not include header DWORD)
	 */
	void (*start_opcode)(struct umr_stream_decode_ui *ui, uint64_t ib_addr, uint32_t ib_vmid, int pkttype, uint32_t opcode, uint32_t subop, uint32_t nwords, const char *opcode_name, uint32_t header, const uint32_t* raw_data);

	/** add_field -- Add a decoded field to a specific DWORD
	 * ib_addr/ib_vmid:  Address of the word from which the field comes
	 * field_name: printable name of the field
	 * value:  Value of the field
	 * ideal_radix: (10 decimal, 16 hex)
	 * field_size: number of bits in the field
	 */
	void (*add_field)(struct umr_stream_decode_ui *ui, uint64_t ib_addr, uint32_t ib_vmid, const char *field_name, uint64_t value, char *str, int ideal_radix, int field_size);

	/** add_shader -- Add a reference to a shader found in the IB stream
	 * ib_addr/ib_vmid:  Address of where reference comes from
	 * asic:  The ASIC the IB stream and shader are bound to
	 * shader: The shader reference
	 */
	void (*add_shader)(struct umr_stream_decode_ui *ui, struct umr_asic *asic, uint64_t ib_addr, uint32_t ib_vmid, struct umr_shaders_pgm *shader);

	/** add_vcn -- Add a reference to a VCN message buffer in the IB stream                                                                                                   
	 * vcn: The pointer to the current VCN message                                                                                                                            
	 */                                                                                                                                                                       
	void (*add_vcn)(struct umr_stream_decode_ui *ui, struct umr_asic *asic, struct umr_vcn_cmd_message *vcn);

	/** add_data -- Add a reference to a data buffer found in the IB stream
	 * ib_addr/ib_vmid:  Address of where reference comes from
	 * asic:  The ASIC the IB stream and shader are bound to
	 * data_addr/data_vmid: A GPUVM reference to the object
	 * type: The type of object
	 */
	void (*add_data)(struct umr_stream_decode_ui *ui, struct umr_asic *asic, uint64_t ib_addr, uint32_t ib_vmid, uint64_t buf_addr, uint32_t buf_vmid, enum UMR_DATABLOCK_ENUM type, uint64_t etype);

	/** unhandled -- Decoder for unhandled (private) opcodes
	 * asic: The ASIC the IB stream is bound to
	 * ib_addr:ib_vmid: The address where the PM4 opcode comes from
	 * stream:  The pointer to the current stream opcode being handled
	 *
	 * Can be NULL to drop support for unhandled opcodes.
	 */
	void (*unhandled)(struct umr_stream_decode_ui *ui, struct umr_asic *asic, uint64_t ib_addr, uint32_t ib_vmid, void *stream, enum umr_ring_type stream_type);

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
	int (*unhandled_size)(struct umr_stream_decode_ui *ui, struct umr_asic *asic, void *stream, enum umr_ring_type stream_type);

	/** unhandled_subop -- Decoder for unhandled (private) sub-opcodes
	 * asic: The ASIC the IB stream is bound to
	 * ib_addr:ib_vmid: The address where the sdma opcode comes from
	 * stream:  The pointer to the current stream opcode being handled
	 *
	 * Can be NULL to drop support for unhandled opcodes.
	 */
	void (*unhandled_subop)(struct umr_stream_decode_ui *ui, struct umr_asic *asic, uint64_t ib_addr, uint32_t ib_vmid, void *stream, enum umr_ring_type stream_type);

	void (*done)(struct umr_stream_decode_ui *ui);

	/** data -- opaque pointer that can be used to track state information */
	void *data;
};

// all of the supported formats are wrapped up in the "packet" API
// to make development easier.
struct umr_packet_stream {
	struct umr_asic *asic;
	enum umr_ring_type type;

	union {
		struct umr_pm4_stream *pm4;
		struct umr_sdma_stream *sdma;
		struct umr_mes_stream *mes;
		struct umr_vpe_stream *vpe;
		struct umr_umsch_stream *umsch;
		struct umr_hsa_stream *hsa;
		struct umr_vcn_enc_stream *enc;
	} stream;

	void *cont;

	struct umr_stream_decode_ui *ui;
};

// decode an array of dwords into a packet stream
struct umr_packet_stream *umr_packet_decode_buffer(struct umr_asic *asic, struct umr_stream_decode_ui *ui,
						   uint32_t from_vmid, uint64_t from_addr,
						   uint32_t *stream, uint32_t nwords, enum umr_ring_type rt);

// decode a ring file (debugfs) into a packet stream
struct umr_packet_stream *umr_packet_decode_ring(struct umr_asic *asic, struct umr_stream_decode_ui *ui,
						char *ringname, int halt_waves, int *start, int *stop, enum umr_ring_type rt);

// decode a GPU mapped buffer into a packet stream
struct umr_packet_stream *umr_packet_decode_vm_buffer(struct umr_asic *asic, struct umr_stream_decode_ui *ui,
						      uint32_t vmid, uint64_t addr, uint32_t nwords, enum umr_ring_type rt);

// free a (umr) packet stream from memory
void umr_packet_free(struct umr_packet_stream *stream);

// find a compute/gfx shader program in a packet stream
struct umr_shaders_pgm *umr_packet_find_shader(struct umr_packet_stream *stream, unsigned vmid, uint64_t addr);

// disassemble a packet stream
struct umr_packet_stream *umr_packet_disassemble_stream(struct umr_packet_stream *stream, uint64_t ib_addr, uint32_t ib_vmid,
							uint64_t from_addr, uint64_t from_vmid, unsigned long opcodes, int follow, int cont);

// disassemble a GPU mapped VM buffer
int umr_packet_disassemble_opcodes_vm(struct umr_asic *asic, struct umr_stream_decode_ui *ui, uint64_t ib_addr, uint32_t ib_vmid, uint32_t nwords, uint64_t from_addr, uint64_t from_vmid, int follow, enum umr_ring_type rt);

// determine if a ring is halted for at least 500 ms
int umr_ring_is_halted(struct umr_asic *asic, char *ringname);
void *umr_read_ring_data(struct umr_asic *asic, char *ringname, uint32_t *ringsize);

#include <umr_packet_pm4.h>
#include <umr_packet_sdma.h>
#include <umr_packet_mes.h>
#include <umr_packet_vpe.h>
#include <umr_packet_umsch.h>
#include <umr_packet_hsa.h>
#include <umr_packet_mqd.h>
#include <umr_packet_vcn.h>

/* shader disassembly */
int umr_shader_disasm(struct umr_asic *asic,
		    uint8_t *inst, unsigned inst_bytes,
		    uint64_t PC,
		    char ***disasm_text);
int umr_vm_disasm_to_str(struct umr_asic *asic, int vm_partition, unsigned vmid, uint64_t addr, uint64_t PC, uint32_t size, uint32_t start_offset, char ***out);
int umr_vm_disasm(struct umr_asic *asic, FILE *output, int vm_partition, unsigned vmid, uint64_t addr, uint64_t PC, uint32_t size, uint32_t start_offset, struct umr_wave_data *wd);
uint32_t umr_compute_shader_size(struct umr_asic *asic, int vm_partition, struct umr_shaders_pgm *shader);


#endif

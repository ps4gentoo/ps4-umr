Packet API Documentation
========================

The Packet API in libumr is what provides a unified interface
to decoding, disassembling, and otherwise working with streams
of packets in various forms.  Supported formats currently include:
PM4, SDMA, and MES.

These are indicated to libumr via the following enum:

::

	enum umr_ring_type {
		UMR_RING_PM4,
		UMR_RING_PM4_LITE,
		UMR_RING_SDMA,
		UMR_RING_MES,
		UMR_RING_VPE,
		UMR_RING_UMSCH,
		UMR_RING_HSA,

		UMR_RING_GUESS,
		UMR_RING_UNK=0xFF, // if unknown
	};

Where PM4_LITE and GUESS are special types used internally by the
library.

The streams are created by decoding a process mapped buffer, GPU
mapped buffer, or ring contents into a list described as follows:

::

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
		} stream;

		void *cont;

		struct umr_stream_decode_ui *ui;
	};

Decoding and disassembly both require a user interface 'ui' callback
to be provided described as follows:

::

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

--------------------------------
Decoding a process mapped buffer
--------------------------------

To decode a process mapped buffer into a stream the following function can be used:

::

	struct umr_packet_stream *umr_packet_decode_buffer(struct umr_asic *asic, struct umr_stream_decode_ui *ui,
							   uint32_t from_vmid, uint32_t from_addr,
							   uint32_t *stream, uint32_t nwords, enum umr_ring_type rt);

This decodes the array of words in 'stream' of length 'nwords' with a packet type of 'rt'.  The 'from_vmid' and 'from_addr' parameters
indicate (if known) where this was taken from and can be used as part of shader/IB following.

----------------------------
Decoding a GPU mapped buffer
----------------------------

To decode a GPU mapped buffer into a stream the following function can be used:

::

	struct umr_packet_stream *umr_packet_decode_vm_buffer(struct umr_asic *asic, struct umr_stream_decode_ui *ui,
								  uint32_t vmid, uint64_t addr, uint32_t nwords, enum umr_ring_type rt);

						      
This will read 'nwords' 32-bit words from the GPU mapped space indicated by the 'vmid' and 'addr' indicated and then proceed to
decode the buffer via the user interface 'ui' presented.

---------------------------
Decoding a ring file buffer
---------------------------

To decode a kernel ring buffer into a stream the following function can be used:

::

	struct umr_packet_stream *umr_packet_decode_ring(struct umr_asic *asic, struct umr_stream_decode_ui *ui,
		char *ringname, int halt_waves, int *start, int *stop, enum umr_ring_type rt);

This function will open up the ring by prepending amdgpu_ to 'ringname'.  The shader engines can be sent a halt command if the 'halt_waves'
flag is set.  The ring will be read from the 'start'th word to the 'stop'th word.  These can be specified as -1 to use the devices
read and write ring pointers respectively.

---------------------------
Disassemble a packet stream
---------------------------

To render through the user interface callback a stream of packets into human readable format the
following function can be used.

::

	struct umr_packet_stream *umr_packet_disassemble_stream(struct umr_packet_stream *stream, uint64_t ib_addr, uint32_t ib_vmid,
								uint64_t from_addr, uint64_t from_vmid, unsigned long opcodes, int follow, int cont);

This will disassemble a stream pointed to by 'stream'.  If it was taken from GPU mapped memory it can be indicated in the 'ib_addr' and
'ib_vmid' parameters, otherwise they can be zero.  If this stream was fetched from a buffer object or indirect buffer the address of this can be
indicated with the 'from_addr' and 'from_vmid' parameters.  The disassembly can be ordered to stop after 'opcodes' many opcodes (or set to ~0UL 
to decode the entire stream).  If disassembly is done in stages the 'cont' flag can be set on the 2nd and subsequent calls to resume disassembly
from where it stopped before.

To disassemble a GPU mapped buffer in one call the following function can be used:

::

	int umr_packet_disassemble_opcodes_vm(struct umr_asic *asic, struct umr_stream_decode_ui *ui, 
										  uint64_t ib_addr, uint32_t ib_vmid, uint32_t nwords, uint64_t from_addr, uint64_t from_vmid,
										  int follow, enum umr_ring_type rt);

This will fetch 'nwords' 32-bit words from the GPU mapped buffer indicated by 'ib_addr' and 'ib_vmid' and proceed to dissassemble the entire
stream.

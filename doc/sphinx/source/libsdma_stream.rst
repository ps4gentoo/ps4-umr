====================
SDMA Stream Decoding
====================

The UMR library has the ability to read rings into a linked list
of SDMA packets with pointers to indirect buffers (IBs) and shaders.

--------------------
SDMA Decode a Buffer
--------------------

To decode a SDMA stream inside a user buffer the following function
can be used:

::

	struct umr_sdma_stream *umr_sdma_decode_stream(struct umr_asic *asic, struct umr_sdma_stream_decode_ui *ui, int vm_partition,
						       uint64_t from_addr, uint32_t from_vmid, uint32_t *stream, uint32_t nwords)

Which will decode a buffer and return a pointer to
the following structure if successful:

::

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

		int invalid;

		struct umr_sdma_stream *next, *next_ib;
	};

Adjacent SDMA packets are pointed to by 'next' (NULL terminated) and
any IBs that are found are pointed to by 'next_ib'.


----------------------
Freeing an SDMA Stream
----------------------

An SDMA stream can be freed with the following function:

::

	void umr_free_sdma_stream(struct umr_sdma_stream *stream);

---------------
Packet Decoding
---------------

To decode packets the following function is used:

::

	struct umr_sdma_stream *umr_sdma_decode_stream_opcodes(struct umr_asic *asic, struct umr_stream_decode_ui *ui, struct umr_sdma_stream *stream,
							       uint64_t ib_addr, uint32_t ib_vmid, uint64_t from_addr, uint64_t from_vmid,
							       unsigned long opcodes, int follow);

The function takes an already streamed SDMA structure and proceeds to decode the packets and the internal fields.  The ib_addr/ib_vmid reference the address of the packets being
decoded while the from_addr/from_vmid point to any stream that pointed to this data (e.g. the ring offset that points to this IB).  The 'opcodes' parameter
indicates how many opcodes to decode (set to ~0UL for the entire stream).  The 'follow' parameter indicates whether the function should also decode packets from IBs pointed
to by this stream.

It returns the address of the first undecoded packet in the stream.

-------------------------
Packet Decoding Callbacks
-------------------------

These functions use the following callback structure to pass information back and forth from the caller:

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


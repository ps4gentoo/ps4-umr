===================
PM4 Stream Decoding
===================

The UMR library has the ability to read rings into a linked list
of PM4 packets with pointers to indirect buffers (IBs) and shaders.

-------------------
PM4 Decode a Buffer
-------------------

To decode a PM4 stream inside a user buffer the following function
can be used:

::

	struct umr_pm4_stream *umr_pm4_decode_stream(struct umr_asic *asic, int vm_partition, int vmid, uint32_t *stream, uint32_t nwords);

Which will decode the buffer and return a pointer to
the following structure if successful:

::

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

		int invalid;
	};

Adjacent PM4 packets are pointed to by 'next' (NULL terminated) and
any IBs or shaders that are found are pointed to by 'ib' and 'shader'
respectively.  The 'no_halt' parameter controls where the "halt_waves"
option will be ignored or not.  This is used if the waves have already
been halted and you don't wish to resume them with this call.

The 'invalid' flag is set if the decoding of the packet fails due to
out of bounds checking (e.g. not enough words for the packet to decode).

--------------------
Freeing a PM4 Stream
--------------------

A PM4 stream can be freed with the following function:

::

	void umr_free_pm4_stream(struct umr_pm4_stream *stream);

------------------------------
Finding Shaders in PM4 Streams
------------------------------

The WAVE_STATUS registers can indicate active waves and where in
shaders they are but not information about the shaders themselves.
The following functions can find shaders in PM4 streams:

::

	struct umr_shaders_pgm *umr_find_shader_in_stream(struct umr_pm4_stream *stream, int vm_partition, unsigned vmid, uint64_t addr);
	struct umr_shaders_pgm *umr_find_shader_in_ring(struct umr_asic *asic, char *ringname, unsigned vmid, uint64_t addr, int no_halt);

If found they return a pointer to a shader structure which then
indicates the base address, VMID, and size of the shader.  This
function returns a copy of the shader structure from the PM4 stream
structure which must be freed independently.  Calling umr_free_pm4_stream()
will not free these copies.

::

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

---------------
Packet Decoding
---------------

To decode packets the following function is used:

::

		struct umr_pm4_stream *umr_pm4_decode_stream_opcodes(struct umr_asic *asic, struct umr_stream_decode_ui *ui, struct umr_pm4_stream *stream,
									 uint64_t ib_addr, uint32_t ib_vmid, uint64_t from_addr, uint64_t from_vmid,
									 unsigned long opcodes, int follow);

	The function takes an already streamed PM4 structure and proceeds to decode the packets and the internal fields.  The ib_addr/ib_vmid reference the address of the packets being
	decoded while the from_addr/from_vmid point to any stream that pointed to this data (e.g. the ring offset that points to this IB).  The 'opcodes' parameter
	indicates how many opcodes to decode (set to ~0UL for the entire stream).  The 'follow' parameter indicates whether the function should also decode packets from IBs pointed
	to by this stream.

	It returns the address of the first undecoded packet in the stream.

	The function uses the following callback structure to pass information back to the caller:

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



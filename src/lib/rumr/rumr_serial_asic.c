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

/**
 * @brief Serialize an ASIC model into a buffer.
 *
 * This function creates a buffer containing the serialized data of the ASIC model.
 * The order of data serialization is crucial, especially for non-amdgpu.ko platforms,
 * where certain parts of the CONFIG data are required.
 *
 * @param asic Pointer to the ASIC structure to be serialized.
 * @return A pointer to the rumr_buffer containing the serialized ASIC data, or NULL on failure.
 */
struct rumr_buffer *rumr_serialize_asic(struct umr_asic *asic)
{
	struct rumr_buffer *buf;
	int ip, reg, bit;
	char tmpbuf[256];

	buf = rumr_buffer_init();
	if (!buf)
		return NULL;

	#pragma GCC diagnostic ignored "-Wmisleading-indentation"

	// ASICNAME
		memset(tmpbuf, 0, sizeof tmpbuf);
		strcpy(tmpbuf, asic->asicname);
		rumr_buffer_add_data(buf, tmpbuf, 64);
	// DID
		rumr_buffer_add_uint32(buf, asic->did);
	// CHIPFAMILY
		rumr_buffer_add_uint32(buf, asic->family);
	// VGPR_GRANULARITY
		rumr_buffer_add_uint32(buf, asic->parameters.vgpr_granularity);
	// CONFIG (this comes from the gca_config debugfs file)
		rumr_buffer_add_uint32(buf, sizeof(asic->config.data));
		rumr_buffer_add_data(buf, asic->config.data, sizeof(asic->config.data));
		// only certain fields are needed
		// git grep asic-\>config src/lib/*.c src/lib/lowlevel/linux/*.c | grep -v scan_config
	// VRAM
		rumr_buffer_add_uint32(buf, asic->config.vram_size & 0xFFFFFFFFUL);
		rumr_buffer_add_uint32(buf, asic->config.vram_size >> 32);
	// VIS_VRAM
		rumr_buffer_add_uint32(buf, asic->config.vis_vram_size & 0xFFFFFFFFUL);
		rumr_buffer_add_uint32(buf, asic->config.vis_vram_size >> 32);
	// GTT
		rumr_buffer_add_uint32(buf, asic->config.gtt_size & 0xFFFFFFFFUL);
		rumr_buffer_add_uint32(buf, asic->config.gtt_size >> 32);
	// APU
		rumr_buffer_add_uint32(buf, asic->is_apu);
	// NO blocks
		rumr_buffer_add_uint32(buf, asic->no_blocks);

	// per IP block
	for (ip = 0; ip < asic->no_blocks; ip++) {
		// ipname
			memset(tmpbuf, 0, sizeof tmpbuf);
			strcpy(tmpbuf, asic->blocks[ip]->ipname);
			rumr_buffer_add_data(buf, tmpbuf, 64);
		// no_regs
			rumr_buffer_add_uint32(buf, asic->blocks[ip]->no_regs);
		// discoverable
			rumr_buffer_add_uint32(buf, asic->blocks[ip]->discoverable.die);
			rumr_buffer_add_uint32(buf, asic->blocks[ip]->discoverable.maj);
			rumr_buffer_add_uint32(buf, asic->blocks[ip]->discoverable.min);
			rumr_buffer_add_uint32(buf, asic->blocks[ip]->discoverable.rev);
			rumr_buffer_add_uint32(buf, asic->blocks[ip]->discoverable.instance);
			rumr_buffer_add_uint32(buf, asic->blocks[ip]->discoverable.logical_inst);
		// registers
		for (reg = 0; reg < asic->blocks[ip]->no_regs; reg++) {
			// regname
				memset(tmpbuf, 0, sizeof tmpbuf);
				strcpy(tmpbuf, asic->blocks[ip]->regs[reg].regname);
				rumr_buffer_add_data(buf, tmpbuf, 128);
			// type
				rumr_buffer_add_uint32(buf, asic->blocks[ip]->regs[reg].type);
			// ADDR_LO
				rumr_buffer_add_uint32(buf, asic->blocks[ip]->regs[reg].addr & 0xFFFFFFFFULL);
			// ADDR_HI
				rumr_buffer_add_uint32(buf, asic->blocks[ip]->regs[reg].addr >> 32);
			// bit64
				rumr_buffer_add_uint32(buf, asic->blocks[ip]->regs[reg].bit64);
			// nobits
				rumr_buffer_add_uint32(buf, asic->blocks[ip]->regs[reg].no_bits);

			for (bit = 0; bit < asic->blocks[ip]->regs[reg].no_bits; bit++) {
				// regname
					memset(tmpbuf, 0, sizeof tmpbuf);
					strcpy(tmpbuf, asic->blocks[ip]->regs[reg].bits[bit].regname);
					rumr_buffer_add_data(buf, tmpbuf, 128);
				// start
					rumr_buffer_add_uint32(buf, asic->blocks[ip]->regs[reg].bits[bit].start);
				// stop
					rumr_buffer_add_uint32(buf, asic->blocks[ip]->regs[reg].bits[bit].stop);
			}
		}
	}
	if (buf->failed) {
		rumr_buffer_free(buf);
		return NULL;
	}
	return buf;
}

/**
 * @brief Parse a serialized ASIC buffer into an ASIC model.
 *
 * This function reads the serialized data from a buffer and reconstructs the ASIC model.
 *
 * @param buf Pointer to the rumr_buffer containing the serialized ASIC data.
 * @return A pointer to the reconstructed umr_asic structure, or NULL on failure.
 */
struct umr_asic *rumr_parse_serialized_asic(struct rumr_buffer *buf)
{
	struct umr_asic *asic;
	uint32_t v;
	char linebuf[256];
	int ip, reg, bit;

	asic = calloc(1, sizeof *asic);

	// read asicname
		memset(linebuf, 0, sizeof linebuf);
		rumr_buffer_read_data(buf, linebuf, 64);
		asic->asicname = strdup(linebuf);
	// DID
		asic->did = rumr_buffer_read_uint32(buf);
	// CHIPFAMILY
		asic->family = rumr_buffer_read_uint32(buf);
	// VGPR Granularity
		asic->parameters.vgpr_granularity = rumr_buffer_read_uint32(buf);
	// config
		v = rumr_buffer_read_uint32(buf);
		if (v <= sizeof(asic->config.data)) {
			rumr_buffer_read_data(buf, asic->config.data, v);
		} else {
			free(asic->asicname);
			free(asic);
			return NULL;
		}
	// VRAM
		asic->config.vram_size = rumr_buffer_read_uint32(buf);
		asic->config.vram_size |= (uint64_t)rumr_buffer_read_uint32(buf) << 32ULL;
	// VIS_VRAM
		asic->config.vis_vram_size = rumr_buffer_read_uint32(buf);
		asic->config.vis_vram_size |= (uint64_t)rumr_buffer_read_uint32(buf) << 32ULL;
	// GTT
		asic->config.gtt_size = rumr_buffer_read_uint32(buf);
		asic->config.gtt_size |= (uint64_t)rumr_buffer_read_uint32(buf) << 32ULL;
	// APU
		asic->is_apu = rumr_buffer_read_uint32(buf);
	// NO blocks
		asic->no_blocks = rumr_buffer_read_uint32(buf);
		asic->blocks = calloc(asic->no_blocks, sizeof asic->blocks[0]);

	// per IP block
	for (ip = 0; ip < asic->no_blocks; ip++) {
		asic->blocks[ip] = calloc(1, sizeof *(asic->blocks[0]));
		// ipname
			memset(linebuf, 0, sizeof linebuf);
			rumr_buffer_read_data(buf, linebuf, 64);
			asic->blocks[ip]->ipname = strdup(linebuf);
		// no_regs
			asic->blocks[ip]->no_regs = rumr_buffer_read_uint32(buf);
			asic->blocks[ip]->regs = calloc(asic->blocks[ip]->no_regs, sizeof asic->blocks[0]->regs[0]);
		// discoverable
			asic->blocks[ip]->discoverable.die = rumr_buffer_read_uint32(buf);
			asic->blocks[ip]->discoverable.maj = rumr_buffer_read_uint32(buf);
			asic->blocks[ip]->discoverable.min = rumr_buffer_read_uint32(buf);
			asic->blocks[ip]->discoverable.rev = rumr_buffer_read_uint32(buf);
			asic->blocks[ip]->discoverable.instance = rumr_buffer_read_uint32(buf);
			asic->blocks[ip]->discoverable.logical_inst = rumr_buffer_read_uint32(buf);
		// registers
			for (reg = 0; reg < asic->blocks[ip]->no_regs; reg++) {
			// regname
				memset(linebuf, 0, sizeof linebuf);
				rumr_buffer_read_data(buf, linebuf, 128);
				asic->blocks[ip]->regs[reg].regname = strdup(linebuf);
			// type
				asic->blocks[ip]->regs[reg].type = rumr_buffer_read_uint32(buf);
			// ADDR_LO
				asic->blocks[ip]->regs[reg].addr = rumr_buffer_read_uint32(buf);
			// ADDR_HI
				asic->blocks[ip]->regs[reg].addr |= (uint64_t)rumr_buffer_read_uint32(buf) << 32ULL;
			// bit64
				asic->blocks[ip]->regs[reg].bit64 = rumr_buffer_read_uint32(buf);
			// nobits
				asic->blocks[ip]->regs[reg].no_bits = rumr_buffer_read_uint32(buf);
				asic->blocks[ip]->regs[reg].bits = calloc(asic->blocks[ip]->regs[reg].no_bits, sizeof asic->blocks[0]->regs[0].bits[0]);

			// bits
				for (bit = 0; bit < asic->blocks[ip]->regs[reg].no_bits; bit++) {
					// bitname
						memset(linebuf, 0, sizeof linebuf);
						rumr_buffer_read_data(buf, linebuf, 128);
						asic->blocks[ip]->regs[reg].bits[bit].regname = strdup(linebuf);
					// start
						asic->blocks[ip]->regs[reg].bits[bit].start = rumr_buffer_read_uint32(buf);
					// stop
						asic->blocks[ip]->regs[reg].bits[bit].stop = rumr_buffer_read_uint32(buf);
					// bitfield print
						asic->blocks[ip]->regs[reg].bits[bit].bitfield_print = umr_bitfield_default;
				}
			}
	}

	// now we have to process the config data, etc...
		umr_scan_config_gca_data(asic);

	return asic;
}

/**
 * @brief Save a serialized ASIC buffer to a file.
 *
 * This function writes the serialized ASIC data from a buffer to a file named after the ASIC's DID.
 *
 * @param asic Pointer to the ASIC structure whose data is serialized and saved.
 * @param buf Pointer to the rumr_buffer containing the serialized ASIC data.
 * @return 0 on success, non-zero on failure.
 */
int rumr_save_serialized_asic(struct umr_asic *asic, struct rumr_buffer *buf)
{
	char fname[32];
	FILE *f;

	sprintf(fname, "0x%"PRIx32".sasic", (uint32_t)asic->did);
	f = fopen(fname, "wb");
	fwrite(buf->data, 1, buf->woffset, f);
	fclose(f);
	return 0;
}

/**
 * @brief Load a serialized ASIC buffer from a file.
 *
 * This function reads the serialized ASIC data from a specified file and returns it in a buffer.
 *
 * @param fname The name of the file containing the serialized ASIC data.
 * @param database_path Path to the database directory (not used in this implementation).
 * @return A pointer to the rumr_buffer containing the loaded serialized ASIC data, or NULL on failure.
 */
struct rumr_buffer *rumr_load_serialized_asic(const char *fname, char *database_path)
{
	return rumr_buffer_load_file(fname, database_path);
}

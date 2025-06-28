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

#define MAX(x, y) (((x) >= (y)) ? (x) : (y))

/**
 * @brief Initializes a new rumr_buffer structure.
 *
 * This function allocates memory for a new buffer and sets its initial size.
 *
 * @return A pointer to the newly created rumr_buffer, or NULL if allocation fails.
 */
struct rumr_buffer *rumr_buffer_init(void)
{
	struct rumr_buffer *buf;

	buf = calloc(1, sizeof *buf);
	if (!buf)
		return NULL;
	buf->size = 1024;
	buf->data = (uint8_t*)calloc(1, buf->size + RUMR_BUFFER_PREHEADER);
	if (!buf->data) {
		free(buf);
		return NULL;
	}
	buf->data += RUMR_BUFFER_PREHEADER;
	return buf;
}

/**
 * @brief Adds data to the end of the buffer.
 *
 * If there is not enough space in the buffer, it will be resized.
 *
 * @param buf Pointer to the rumr_buffer structure.
 * @param data Pointer to the data to add.
 * @param size Size of the data to add.
 */
void rumr_buffer_add_data(struct rumr_buffer *buf, void *data, uint32_t size)
{
	if (buf->failed)
		return;
	if ((buf->woffset + size) > buf->size) {
		uint32_t new_size = buf->size + MAX(1024, size);
		uint8_t *tmp = (uint8_t*)realloc(buf->data - RUMR_BUFFER_PREHEADER, new_size + RUMR_BUFFER_PREHEADER);
		if (!tmp) {
			buf->failed = 1;
			return;
		}
		buf->data = tmp + RUMR_BUFFER_PREHEADER;
		buf->size = new_size;
	}
	memcpy(buf->data + buf->woffset, data, size);
	buf->woffset += size;
}

/**
 * @brief Adds another buffer's contents to the end of this buffer.
 *
 * @param buf Pointer to the destination rumr_buffer structure.
 * @param srcbuf Pointer to the source rumr_buffer structure whose data will be added.
 */
void rumr_buffer_add_buffer(struct rumr_buffer *buf, struct rumr_buffer *srcbuf)
{
	rumr_buffer_add_data(buf, srcbuf->data, srcbuf->woffset);
}

/**
 * @brief Adds a 32-bit unsigned integer to the end of the buffer.
 *
 * @param buf Pointer to the rumr_buffer structure.
 * @param val The 32-bit unsigned integer to add.
 */
void rumr_buffer_add_uint32(struct rumr_buffer *buf, uint32_t val)
{
	rumr_buffer_add_data(buf, &val, 4);
}

/**
 * @brief Reads data from the buffer starting at the current read offset.
 *
 * If there is not enough data in the buffer, the remaining space will be filled with zeros.
 *
 * @param buf Pointer to the rumr_buffer structure.
 * @param data Pointer to the destination where the data will be copied.
 * @param size Size of the data to read.
 */
void rumr_buffer_read_data(struct rumr_buffer *buf, void *data, uint32_t size)
{
	if ((buf->roffset + size) <= buf->woffset) {
		memcpy(data, &buf->data[buf->roffset], size);
		buf->roffset += size;
	} else {
		// ensure something safe is in the buffer if we hit the end of the buffer
		memset(data, 0, size);
	}
}

/**
 * @brief Reads a 32-bit unsigned integer from the buffer starting at the current read offset.
 *
 * @param buf Pointer to the rumr_buffer structure.
 * @return The 32-bit unsigned integer read from the buffer.
 */
uint32_t rumr_buffer_read_uint32(struct rumr_buffer *buf)
{
	uint32_t tmp;
	rumr_buffer_read_data(buf, &tmp, 4);
	return tmp;
}

/**
 * @brief Frees the memory allocated for a rumr_buffer structure.
 *
 * @param buf Pointer to the rumr_buffer structure to free.
 */
void rumr_buffer_free(struct rumr_buffer *buf)
{
	if (buf) {
		free(buf->data - RUMR_BUFFER_PREHEADER);
		free(buf);
	}
}

/**
 * @brief Loads data from a file into a new rumr_buffer structure.
 *
 * The file is opened using the provided database path and filename.
 *
 * @param fname Name of the file to load.
 * @param database_path Path to the directory containing the file.
 * @return A pointer to the newly created rumr_buffer with the file's data, or NULL if loading fails.
 */
struct rumr_buffer *rumr_buffer_load_file(const char *fname, char *database_path)
{
	struct rumr_buffer *buf;
	uint32_t size;
	FILE *f;

	f = umr_database_open(database_path, (char *) fname, 1);
	if (!f)
		return NULL;
	fseek(f, 0, SEEK_END);
	size = ftell(f);
	fseek(f, 0, SEEK_SET);

	buf = calloc(1, sizeof *buf);
	buf->data = calloc(1, size + RUMR_BUFFER_PREHEADER);
	if (!buf->data) {
		free(buf);
		fclose(f);
		return NULL;
	}
	buf->data += RUMR_BUFFER_PREHEADER;
	buf->size = size;
	fread(buf->data, 1, size, f);
	fclose(f);
	buf->woffset = size;
	return buf;
}

/*
 * Copyright 2022 Advanced Micro Devices, Inc.
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
 */
#include "umrapp.h"
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdarg.h>
#include <errno.h>
#if UMR_GUI_REMOTE
#include <nanomsg/nn.h>
#include <nanomsg/reqrep.h>
#endif
#include "parson.h"

static char * read_file(const char *path) {
	static char *buffer = NULL;
	static unsigned buffer_size = 0;
	FILE *fd = fopen(path, "r");
	if (fd) {
		long total = 0;
		while (1) {
			if (total >= buffer_size) {
				buffer_size = total ? total * 2 : 1024;
				buffer = realloc(buffer, buffer_size);
			}

			int n = fread(&buffer[total], 1, buffer_size - total, fd);
			if (!n) {
				buffer[total] = '\0';
				break;
			}
			total += n;
		}
		fclose(fd);
		return buffer;
	}
	return "";
}

static uint64_t read_sysfs_uint64(const char *path) {
	char *content = read_file(path);
	uint64_t v;
	if (sscanf(content, "%lu", &v) == 1)
		return v;
	return 0;
}

void parse_sysfs_clock_file(char *content, int *min, int *max) {
	*min = 100000;
	*max = 0;

	int i, value;
	char *in = content;
	char *ptr;
	char tmp[1024];
	while((ptr = strchr(in, '\n'))) {
		strncpy(tmp, in, ptr - in);
		tmp[ptr - in] = '\0';
		if (sscanf(tmp, "%d: %dMHz", &i, &value) == 2) {
			if (value < *min) *min = value;
			if (value > *max) *max = value;
		}
		in = ptr + 1;
	}
}

static const char * lookup_field(const char **in, const char *field, char separator) {
	static char value[2048];
	const char *input = *in;
	input = strstr(input, field);
	if (!input)
		return NULL;
	input += strlen(field);
	while (*input != separator)
		input++;
	input++;
	while (*input == ' ' || *input == '\t')
		input++;
	const char *end = input + 1;
	while (*end != '\n')
		end++;
	memcpy(value, input, end - input);
	value[end - input] = '\0';
	*in = end;

	return value;
}

enum kms_field_type {
	KMS_STRING = 0,
	KMS_INT_10,
	KMS_INT_16,
	KMS_SIZE,
	KMS_SIZE_POS
};
static int parse_kms_field(const char **content, const char *field, const char *js_field, enum kms_field_type type, JSON_Object *out) {
	const char *ptr = lookup_field(content, field, '=');
	if (!ptr)
		return 0;

	switch (type) {
		case KMS_STRING:
			json_object_set_string(out, js_field, ptr);
			break;
		case KMS_INT_10: {
			int v;
			if (sscanf(ptr, "%d", &v) == 1)
				json_object_set_number(out, js_field, v);
			break;
		}
		case KMS_INT_16: {
			uint64_t v;
			if (sscanf(ptr, "0x%" PRIx64, &v) == 1)
				json_object_set_number(out, js_field, v);
			break;
		}
		case KMS_SIZE: {
			int w, h;
			if (sscanf(ptr, "%dx%d", &w, &h) == 2) {
				JSON_Value *size = json_value_init_object();
				json_object_set_number(json_object(size), "w", w);
				json_object_set_number(json_object(size), "h", h);
				json_object_set_value(out, js_field, size);
			}
			break;
		}
		case KMS_SIZE_POS: {
			int x, y, w, h;
			if (sscanf(ptr, "%dx%d+%d+%d", &w, &h, &x, &y) == 4) {
				JSON_Value *size = json_value_init_object();
				json_object_set_number(json_object(size), "w", w);
				json_object_set_number(json_object(size), "h", h);
				json_object_set_number(json_object(size), "x", x);
				json_object_set_number(json_object(size), "y", y);
				json_object_set_value(out, js_field, size);
			}
			break;
		}
		default:
			break;
	}
	return 1;
}

JSON_Array *parse_kms_framebuffer_sysfs_file(const char *content) {
	JSON_Array *out = json_array(json_value_init_array());
	const char *next_framebuffer = strstr(content, "framebuffer[");
	while (next_framebuffer) {
		if (!next_framebuffer)
			break;

		JSON_Object *fb = json_object(json_value_init_object());
		content = next_framebuffer + strlen("framebuffer");
		int id;
		if (sscanf(content, "[%d]", &id) == 1)
			json_object_set_number(fb, "id", id);

		parse_kms_field(&content, "allocated by", "allocated by", KMS_STRING, fb);
		parse_kms_field(&content, "format", "format", KMS_STRING, fb);
		parse_kms_field(&content, "modifier", "modifier", KMS_INT_16, fb);
		parse_kms_field(&content, "size", "size", KMS_SIZE, fb);

		JSON_Value *layers = json_value_init_array();
		content = strstr(content, "layers:");
		next_framebuffer = strstr(content, "framebuffer[");
		int layer_id = 0;
		while (content) {
			JSON_Value *layer = json_value_init_object();

			char tmp[256];
			sprintf(tmp, "size[%d]", layer_id);
			if (parse_kms_field(&content, tmp, "size", KMS_SIZE, json_object(layer))) {
				if (content > next_framebuffer && next_framebuffer) {
					json_value_free(layer);
					break;
				}

				sprintf(tmp, "pitch[%d]", layer_id);
				parse_kms_field(&content, tmp, "pitch", KMS_INT_10, json_object(layer));
				json_array_append_value(json_array(layers), layer);
				layer_id++;
			} else {
				json_value_free(layer);
				break;
			}
		}
		json_object_set_value(fb, "layers", layers);

		json_array_append_value(out, json_object_get_wrapping_value(fb));
	}

	return out;
}

JSON_Object *parse_kms_state_sysfs_file(const char *content) {
	char tmp[256];
	JSON_Object *out = json_object(json_value_init_object());

	/* planes */
	JSON_Value *planes = json_value_init_array();
	int plane_id = 0;
	while (1) {
		const char *last_input = content;
		sprintf(tmp, "plane-%d", plane_id);
		const char *plane = strstr(content, tmp);
		if (plane) {
			JSON_Object *p = json_object(json_value_init_object());
			json_object_set_number(p, "id", plane_id);
			const char *ptr = lookup_field(&plane, "crtc", '=');
			if (ptr) {
				int crtc_id;
				if (sscanf(ptr, "crtc-%d", &crtc_id) == 1) {
					parse_kms_field(&plane, "fb", "fb", KMS_INT_10, p);
					JSON_Object *cr = json_object(json_value_init_object());
					json_object_set_number(cr, "id", crtc_id);
					parse_kms_field(&plane, "crtc-pos", "pos", KMS_SIZE_POS, cr);
					json_object_set_value(p, "crtc", json_object_get_wrapping_value(cr));
				} else {
					parse_kms_field(&plane, "fb", "fb", KMS_INT_10, p);
				}
			}
			content = plane;
			json_array_append_value(json_array(planes), json_object_get_wrapping_value(p));
			plane_id++;
		} else {
			content = last_input;
			break;
		}
	}

	/* crtcs */
	JSON_Value *crtcs = json_value_init_array();
	int crtc_id = 0;
	while (1) {
		const char *last_input = content;
		sprintf(tmp, "crtc-%d", crtc_id);
		const char *crtc = strstr(content, tmp);
		if (crtc) {
			JSON_Object *c = json_object(json_value_init_object());
			json_object_set_number(c, "id", crtc_id);
			parse_kms_field(&content, "enable", "enable", KMS_INT_10, c);
			parse_kms_field(&content, "active", "active", KMS_INT_10, c);
			json_array_append_value(json_array(crtcs), json_object_get_wrapping_value(c));
			crtc_id++;
		} else {
			content = last_input;
			break;
		}
	}

	/* connectors */
	JSON_Value *connectors = json_value_init_array();
	while (1) {
		const char *last_input = content;
		const char *ptr = strstr(content, "connector[");
		if (ptr) {
			JSON_Object *c = json_object(json_value_init_object());
			while(*ptr != ' ')
				ptr++;
			ptr++;
			const char *end = ptr + 1;
			while (*end != '\n')
				end++;
			json_object_set_string_with_len(c, "name", ptr, end - ptr);
			const char *cr = lookup_field(&ptr, "crtc", '=');
			if (cr) {
				int cid;
				if (sscanf(cr, "crtc-%d", &cid) == 1) {
					json_object_set_number(c, "crtc", cid);
				}
			}
			json_array_append_value(json_array(connectors), json_object_get_wrapping_value(c));
			content = end;
		} else {
			content = last_input;
			break;
		}
	}

	json_object_set_value(out, "planes", planes);
	json_object_set_value(out, "crtcs", crtcs);
	json_object_set_value(out, "connectors", connectors);

	return out;
}

JSON_Array *get_rings_last_signaled_fences(const char *fence_info, const char *ring_filter) {
	JSON_Array *fences = json_array(json_value_init_array());
	int cursor = 0;
	while (1) {
		char *next_ring = strstr(&fence_info[cursor], "--- ring");
		if (!next_ring)
			break;
		char *next_ring_start = strchr(next_ring, '(');
		if (!next_ring_start)
			break;
		next_ring_start++;
		char *next_ring_end = strchr(next_ring_start, ')');
		int len = next_ring_end - next_ring_start;
		char ring_name[128];
		strncpy(ring_name, next_ring_start, len);
		ring_name[len] = '\0';

		char *next_line = strstr(next_ring_end + 1, "0x");
		if (!next_line)
			break;

		int c = next_line - fence_info;
		unsigned long last_signaled;
		if (sscanf(&fence_info[c], "0x%08lx", &last_signaled) == 1) {
			if (!ring_filter || strcmp(ring_filter, ring_name) == 0) {
				JSON_Value *fence = json_value_init_object();
				json_object_set_string(json_object(fence), "name", ring_name);
				json_object_set_number(json_object(fence), "value", last_signaled);
				json_array_append_value(fences, fence);
			}
		}
		cursor = c + strlen("0x00000000") + 1;
	}
	return fences;
}

JSON_Value *compare_fence_infos(const char *fence_info_before, const char *fence_info_after) {
	JSON_Value *fences = json_value_init_array();
	JSON_Array *before = get_rings_last_signaled_fences(fence_info_before, NULL);
	JSON_Array *after = get_rings_last_signaled_fences(fence_info_after, NULL);
	for (size_t i = 0; i < json_array_get_count(before); i++) {
		JSON_Value *fence = json_value_init_object();
		JSON_Object *b = json_object(json_array_get_value(before, i));
		JSON_Object *a = json_object(json_array_get_value(after, i));

		const char *ring_name = json_object_get_string(b, "name");
		uint32_t v1 = (uint32_t)json_object_get_number(b, "value");
		uint32_t v2 = (uint32_t)json_object_get_number(a, "value");
		json_object_set_string(json_object(fence), "name", ring_name);
		json_object_set_number(json_object(fence), "delta", v2 - v1);
		json_array_append_value(json_array(fences), fence);
	}
	json_value_free(json_array_get_wrapping_value(before));
	json_value_free(json_array_get_wrapping_value(after));

	return fences;
}

JSON_Array *parse_vm_info(const char *content)
{
	JSON_Array *pids = json_array(json_value_init_array());

	const char *ptr = content;
	while (ptr) {
		unsigned pid;
		char *next_pid = strstr(ptr, "pid:");
		if (!next_pid)
			break;
		char *next_space = strchr(next_pid, '\t');
		ptr = next_space + 1;

		if (sscanf(next_pid, "pid:%u", &pid) == 1) {
			JSON_Value *p = json_value_init_object();
			json_array_append_value(pids, p);
			json_object_set_number(json_object(p), "pid", pid);

			ptr = next_space + 1 + strlen("Process:");
			next_space = strchr(ptr, ' ');
			int len = next_space - ptr;

			json_object_set_string_with_len(json_object(p), "name", ptr, len);
			ptr = next_space + 1;
			const char *categories[] = { "Idle", "Evicted", "Relocated", "Moved", "Invalidated", "Done" };
			uint64_t pid_total = 0;
			for (int i = 0; ptr && i < 6; i++) {
				JSON_Array *cat = json_array(json_value_init_array());
				uint64_t cat_total = 0;
				ptr = strstr(ptr, categories[i]);
				/* Consume all chars until next line */
				while (*ptr != '\n')
					ptr++;
				ptr++;

				while (ptr) {
					char *end_of_line = strchr(ptr, '\n');
					char *id = strstr(ptr, "0x");
					if (id && id < end_of_line) {
						id += 11;
						while (*id == ' ')
							id++;
						char *b = strstr(id, "byte");

						/* Parse size */
						uint64_t sz;
						sscanf(id, "%lu byte", &sz);
						ptr = b + 5;

						JSON_Value *bo = json_value_init_object();
						json_array_append_value(cat, bo);
						json_object_set_number(json_object(bo), "size", sz);
						cat_total += sz;
						pid_total += sz;

						/* Parse attributes */
						char attr_in_progress[256];
						int concat_the_next_n = 0;
						JSON_Array *attr = json_array(json_value_init_array());
						while (ptr < end_of_line) {
							next_space = strchr(ptr, ' ');
							if (!next_space || next_space > end_of_line)
								next_space = end_of_line;
							if (next_space) {
								int len = next_space - ptr;
								if (ptr != next_space) {
									if ((len == strlen("exported") && !strncmp(ptr, "exported", len)) ||
										(len == strlen("imported") && !strncmp(ptr, "imported", len)) ||
										(len == strlen("pin") && !strncmp(ptr, "pin", len))) {
										strncpy(attr_in_progress, ptr, len);
										attr_in_progress[len] = '\0';
										concat_the_next_n = 2;
									} else if (concat_the_next_n > 0) {
										sprintf(&attr_in_progress[strlen(attr_in_progress)], " %.*s", len, ptr);
										concat_the_next_n--;
									} else {
										strncpy(attr_in_progress, ptr, len);
										attr_in_progress[len] = '\0';
									}

									if (concat_the_next_n == 0) {
										json_array_append_string(attr, attr_in_progress);
										attr_in_progress[0] = '\0';
									}
								}
								ptr = next_space + 1;
							} else {
								break;
							}
						}
						ptr = end_of_line + 1;
						if (json_array_get_count(attr))
							json_object_set_value(json_object(bo), "attributes",
								json_array_get_wrapping_value(attr));
						else
							json_value_free(json_array_get_wrapping_value(attr));
					} else {
						break;
					}
				}

				if (cat_total > 0) {
					json_object_set_value(json_object(p), categories[i],
						json_array_get_wrapping_value(cat));
				}
			}
			json_object_set_number(json_object(p), "total", pid_total);
		}
	}
	return pids;
}

enum sensor_maps {
	SENSOR_IDENTITY = 0,
	SENSOR_D1000,
	SENSOR_D100,
	SENSOR_WAIT,
};

struct power_bitfield{
	char *regname;
	uint32_t value;
	enum amd_pp_sensors sensor_id;
	enum sensor_maps map;
};


static uint32_t parse_sensor_value(enum sensor_maps map, uint32_t value)
{
	uint32_t result = 0;

	switch(map) {
		case SENSOR_IDENTITY:
			result = value;
			break;
		case SENSOR_D1000:
			result = value / 1000;
			break;
		case SENSOR_D100:
			result = value / 100;
			break;
		case SENSOR_WAIT:
			result = ((value >> 8) * 1000);
			if ((value & 0xFF) < 100)
				result += (value & 0xFF) * 10;
			else
				result += value;
			result /= 1000;
			break;
		default:
			printf("invalid input value!\n");
			break;
	}
	return result;
}

int parse_pp_feature_vega_line(const char *line, const char **name_start, const char **name_end, int *bit, int *enabled)
{
	/* NAME      0x0000000000000000    Y */
	*name_start = line;
	*name_end = strchr(line, ' ');

	if (*name_end == NULL)
		return -1;

	const char *ptr = *name_end;
	while (*ptr == ' ')
		ptr++;

	uint64_t bitmask;
	*bit = -1;
	if (sscanf(ptr, "0x%" PRIx64, &bitmask) == 1) {
		for (int b = 0; b < 64; b++)
			if (bitmask & (1lu << b)) {
				*bit = b;
				break;
			}
	}


	if (*bit >= 0) {
		/* Skip the 16 bytes hex value */
		ptr += 18;
		while (*ptr == ' ') ptr++;
		*enabled = *ptr == 'Y';
		return 0;
	}

	return -1;
}

int parse_pp_feature_line(const char *line, int i, const char **name_start, const char **name_end, int *bit, int *enabled)
{
	const char *ptr = line;
	char label[64];
	sprintf(label, "%02d.", i);
	const char *feature = lookup_field(&ptr, label, ' ');
	if (!feature)
		return -1;
	const char *sp = strchr(feature, ' ');
	if (!sp)
		return -1;
	*name_start = feature;
	*name_end = sp;
	*enabled = strstr(feature, "enabled") != NULL;

	/* Skip spaces */
	while (*sp == ' ')
		sp++;
	/* Parse bit */
	if (sscanf(sp, "(%d)", bit) == 1)
		return 0;

	return -1;
}

JSON_Object *parse_pp_features_sysfs_file(const char *content)
{
	const char *ptr = content;
	uint64_t raw_value = 0;
	JSON_Object *out = json_object(json_value_init_object());
	JSON_Array *features = json_array(json_value_init_array());
	json_object_set_value(out, "features", json_array_get_wrapping_value(features));

	int vega_format = 0;

	/* 2 possible formats: Vega dGPU or newer ones */
	vega_format = strncmp(ptr, "Current ppfeatures", strlen("Current ppfeatures")) == 0;

	/* Strip the first 2 lines */
	ptr = strchr(ptr, '\n') + 1;
	ptr = strchr(ptr, '\n') + 1;

	for (int i = 0; i < 64; i++) {
		const char *name_start, *name_end;
		int bit, enabled;
		int r;
		if (vega_format) {
			r = parse_pp_feature_vega_line(ptr, &name_start, &name_end, &bit, &enabled);
		} else {
			r = parse_pp_feature_line(ptr, i, &name_start, &name_end, &bit, &enabled);
		}

		if (r < 0)
			break;

		JSON_Object *feat = json_object(json_value_init_object());
		json_object_set_string_with_len(feat, "name", name_start, name_end - name_start);
		json_object_set_boolean(feat, "on", enabled);

		int implicit_bit = json_array_get_count(features);
		/* If there's a gap insert dummy values, so array index can be used
		 * as bit index. */
		if (implicit_bit < bit) {
			int delta = bit - implicit_bit;
			for (int j = 0; j < delta; j++)
				json_array_append_value(features, json_value_init_object());
		}

		json_array_append_value(features, json_object_get_wrapping_value(feat));
		if (enabled)
			raw_value |= 1lu << bit;

		ptr = strchr(ptr, '\n') + 1;
	}
	json_object_set_number(out, "raw_value", raw_value);

	return out;
}

struct {
	uint64_t pba;
	uint64_t va_mask;

	int type; /* 0: base, 1: pde, 2: pte */

	int system, tmz, mtype;
	int pte;
} page_table[64];
int num_page_table_entries;

static void my_va_decode(pde_fields_t *pdes, int num_pde, pte_fields_t pte) {
	for (int i = 0; i < num_pde; i++) {
		page_table[num_page_table_entries].pba = pdes[i].pte_base_addr;
		page_table[num_page_table_entries].type = i == 0 ? 0 : 1;
		page_table[num_page_table_entries].system = pdes[i].pte;
		num_page_table_entries++;
	}
	if (pte.valid || 1) {
		page_table[num_page_table_entries].type = 2;
		page_table[num_page_table_entries].pba = pte.page_base_addr;
		page_table[num_page_table_entries].system = pte.system;
		page_table[num_page_table_entries].va_mask = pte.pte_mask;
		page_table[num_page_table_entries].tmz = pte.tmz;
		page_table[num_page_table_entries].mtype = pte.mtype;
		num_page_table_entries++;
	}
}

static int dummy_printf(const char *fmt, ...) {
	(void)fmt;
	return 0;
}

static JSON_Value *shader_pgm_to_json(struct umr_asic *asic, uint32_t vmid, uint64_t addr, uint32_t size) {
	JSON_Object *res = NULL;
	uint32_t *opcodes = calloc(size / 4, sizeof(uint32_t));
	if (umr_read_vram(asic, asic->options.vm_partition, vmid, addr, size, (void*)opcodes) == 0) {
		res = json_object(json_value_init_object());
		JSON_Array *op = json_array(json_value_init_array());
		for (unsigned i = 0; i < size / 4; i++)
			json_array_append_number(op, opcodes[i]);
		json_object_set_value(res, "opcodes", json_array_get_wrapping_value(op));
		json_object_set_number(res, "address", addr);
		json_object_set_number(res, "vmid", vmid);
	}
	free(opcodes);
	return json_object_get_wrapping_value(res);
}

/* Ring stream decoding */
struct ring_decoding_data {
	JSON_Array *shaders;
	JSON_Array *ibs;
	JSON_Value *ring;
	JSON_Array *open_ibs;
};

static void _ring_start_ib(struct ring_decoding_data *data, uint64_t ib_addr, uint32_t ib_vmid) {
	JSON_Object *current_ib = json_object(json_value_init_object());
	json_object_set_number(current_ib, "address", ib_addr);
	json_object_set_number(current_ib, "vmid", ib_vmid);
	json_object_set_value(current_ib, "opcodes", json_value_init_array());
	json_array_append_value(data->open_ibs, json_object_get_wrapping_value(current_ib));
}
static void _ring_start_opcode(struct ring_decoding_data *data, uint32_t nwords, uint32_t header, const uint32_t* raw_data) {
	JSON_Value *current_ib = json_array_get_value(data->open_ibs, json_array_get_count(data->open_ibs) - 1);
	JSON_Array *opcodes = json_object_get_array(json_object(current_ib), "opcodes");
	json_array_append_number(opcodes, header);
	for (unsigned i = 0; i < nwords; i++)
		json_array_append_number(opcodes, raw_data[i]);
}
static void _ring_done(struct ring_decoding_data *data) {
	int idx = json_array_get_count(data->open_ibs) - 1;

	JSON_Value *v = json_value_deep_copy(json_array_get_value(data->open_ibs, idx));
	if (json_object_get_number(json_object(v), "address") == 0)
		data->ring = v;
	else
		json_array_append_value(data->ibs, v);
	json_array_remove(data->open_ibs, idx);
}


#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"


static void ring_start_ib(struct umr_pm4_stream_decode_ui *ui, uint64_t ib_addr, uint32_t ib_vmid, uint64_t from_addr, uint32_t from_vmid, uint32_t size, int type) {
	_ring_start_ib((struct ring_decoding_data*) ui->data, ib_addr, ib_vmid);
}

static void ring_start_opcode(struct umr_pm4_stream_decode_ui *ui, uint64_t ib_addr, uint32_t ib_vmid, int pkttype, uint32_t opcode, uint32_t nwords, const char *opcode_name, uint32_t header, const uint32_t* raw_data) {
	_ring_start_opcode((struct ring_decoding_data*) ui->data, nwords, header, raw_data);
}

static void ring_add_field(struct umr_pm4_stream_decode_ui *ui, uint64_t ib_addr, uint32_t ib_vmid, const char *field_name, uint32_t value, char *str, int ideal_radix) {
	/* Ignore */
}

static void ring_add_shader(struct umr_pm4_stream_decode_ui *ui, struct umr_asic *asic, uint64_t ib_addr, uint32_t ib_vmid, struct umr_shaders_pgm *shader) {
	struct ring_decoding_data *data = (struct ring_decoding_data*) ui->data;

	for (size_t i = 0; i < json_array_get_count(data->shaders); i++) {
		JSON_Object *sh = json_object(json_array_get_value(data->shaders, i));
		uint64_t addr = json_object_get_number(sh, "address");
		uint64_t vmid = json_object_get_number(sh, "vmid");
		if (addr == shader->addr && vmid == shader->vmid) {
			/* Duplicate => skip */
			return;
		}
	}

	JSON_Value *sh = shader_pgm_to_json(asic, shader->vmid, shader->addr, shader->size);
	if (sh)
		json_array_append_value(data->shaders, sh);
}

static void ring_add_data(struct umr_pm4_stream_decode_ui *ui, struct umr_asic *asic, uint64_t ib_addr, uint32_t ib_vmid, uint64_t buf_addr, uint32_t buf_vmid, enum UMR_DATABLOCK_ENUM type, uint64_t etype) {
	/* Ignore */
}

static void ring_unhandled(struct umr_pm4_stream_decode_ui *ui, struct umr_asic *asic, uint64_t ib_addr, uint32_t ib_vmid, struct umr_pm4_stream *stream) {
	/* Ignore */
}

static void ring_done(struct umr_pm4_stream_decode_ui *ui) {
	_ring_done((struct ring_decoding_data*) ui->data);
}

static void sdma_start_ib(struct umr_sdma_stream_decode_ui *ui, uint64_t ib_addr, uint32_t ib_vmid, uint64_t from_addr, uint32_t from_vmid, uint32_t size) {
	_ring_start_ib((struct ring_decoding_data*) ui->data, ib_addr, ib_vmid);
}

static void sdma_start_opcode(struct umr_sdma_stream_decode_ui *ui, uint64_t ib_addr, uint32_t ib_vmid, uint32_t opcode, uint32_t sub_opcode, uint32_t nwords, char *opcode_name, uint32_t header, uint32_t* raw_data) {
	_ring_start_opcode((struct ring_decoding_data*) ui->data, nwords - 1, header, raw_data);
}

static void sdma_add_field(struct umr_sdma_stream_decode_ui *ui, uint64_t ib_addr, uint32_t ib_vmid, const char *field_name, uint32_t value, char *str, int ideal_radix) {
	/* Ignore */
}

static void sdma_unhandled(struct umr_sdma_stream_decode_ui *ui, struct umr_asic *asic, uint64_t ib_addr, uint32_t ib_vmid, struct umr_sdma_stream *stream) {
	/* Ignore */
}

static int sdma_unhandled_size(struct umr_sdma_stream_decode_ui *ui, struct umr_asic *asic, struct umr_sdma_stream *stream) {
	/* Ignore */
	return 1;
}

static void sdma_unhandled_subop(struct umr_sdma_stream_decode_ui *ui, struct umr_asic *asic, uint64_t ib_addr, uint32_t ib_vmid, struct umr_sdma_stream *stream) {
	/* Ignore */
}

static void sdma_done(struct umr_sdma_stream_decode_ui *ui) {
	_ring_done((struct ring_decoding_data*) ui->data);
}
#pragma GCC diagnostic pop

static struct umr_asic *asics[16] = {0};
static char *ip_discovery_dump[16] = {0};

static void init_asics() {
	int i = 0;
	struct umr_options opt = {0};
	opt.need_scan = 1;
	opt.forcedid = -1;
	opt.scanblock = "";
	opt.instance = 0;
	opt.vm_partition = -1;

	/* Allocate a buffer to pass ip discovery info to the client. */
	ip_discovery_dump[0] = calloc(1, 100000);
	opt.test_log_fd = fmemopen(ip_discovery_dump[0], 100000, "w");
	if (!opt.test_log_fd)
		opt.force_asic_file = 1;
	else
		opt.test_log = 1;

	while ((asics[i] = umr_discover_asic(&opt, NULL))) {
		// assign linux callbacks
		asics[i]->mem_funcs.vm_message = dummy_printf;
		asics[i]->mem_funcs.gpu_bus_to_cpu_address = umr_vm_dma_to_phys;
		asics[i]->mem_funcs.access_sram = umr_access_sram;

		asics[i]->shader_disasm_funcs.disasm = umr_shader_disasm;

		if (asics[i]->options.use_pci == 0)
			asics[i]->mem_funcs.access_linear_vram = umr_access_linear_vram;
		else
			asics[i]->mem_funcs.access_linear_vram = umr_access_vram_via_mmio;

		asics[i]->reg_funcs.read_reg = umr_read_reg;
		asics[i]->reg_funcs.write_reg = umr_write_reg;

		asics[i]->wave_funcs.get_wave_sq_info = umr_get_wave_sq_info;
		asics[i]->wave_funcs.get_wave_status = umr_get_wave_status;

		// default shader options
		if (asics[i]->family <= FAMILY_VI) { // on gfx9+ hs/gs are opaque
			asics[i]->options.shader_enable.enable_gs_shader = 1;
			asics[i]->options.shader_enable.enable_hs_shader = 1;
		}
		asics[i]->options.shader_enable.enable_vs_shader   = 1;
		asics[i]->options.shader_enable.enable_ps_shader   = 1;
		asics[i]->options.shader_enable.enable_es_shader   = 1;
		asics[i]->options.shader_enable.enable_ls_shader   = 1;
		asics[i]->options.shader_enable.enable_comp_shader = 1;

		asics[i]->gpr_read_funcs.read_sgprs = umr_read_sgprs;
		asics[i]->gpr_read_funcs.read_vgprs = umr_read_vgprs;

		asics[i]->err_msg = dummy_printf;

		if (asics[i]->family > FAMILY_VI)
			asics[i]->options.shader_enable.enable_es_ls_swap = 1;  // on >FAMILY_VI we swap LS/ES for HS/GS

		umr_scan_config(asics[i], 1);
		if (asics[i]->fd.drm < 0) {
			char fname[64];
			snprintf(fname, sizeof(fname) - 1, "/dev/dri/card%d", asics[i]->instance);
			asics[i]->fd.drm = open(fname, O_RDWR);
		}

		i++;

		if (opt.test_log_fd) {
			fclose(opt.test_log_fd);
			ip_discovery_dump[i] = malloc(100000);
			opt.test_log_fd = fmemopen(ip_discovery_dump[i], 100000, "w");
		}
		memset(&opt, 0, sizeof(opt));
		opt.need_scan = 1;
		opt.forcedid = -1;
		opt.scanblock = "";
		opt.instance = i;
	}

	if (opt.test_log_fd) {
		fclose(opt.test_log_fd);
		free(ip_discovery_dump[i]);
	}
}

static void wave_to_json(struct umr_asic *asic, int is_halted, int include_shaders, JSON_Object *out) {
	struct umr_pm4_stream *stream = umr_pm4_decode_ring(asic, asic->options.ring_name, 1, -1, -1);

	struct umr_wave_data *wd = umr_scan_wave_data(asic);

	JSON_Value *shaders = json_value_init_object();

	JSON_Value *waves = json_value_init_array();
	while (wd) {
		uint64_t pgm_addr = (((uint64_t)wd->ws.pc_hi << 32) | wd->ws.pc_lo);
		unsigned vmid;

		JSON_Value *wave = json_value_init_object();
		json_object_set_number(json_object(wave), "se", wd->se);
		json_object_set_number(json_object(wave), "sh", wd->sh);
		json_object_set_number(json_object(wave), asic->family < FAMILY_NV ? "cu" : "wgp", wd->cu);
		json_object_set_number(json_object(wave), "simd_id", wd->ws.hw_id1.simd_id);
		json_object_set_number(json_object(wave), "wave_id", wd->ws.hw_id1.wave_id);
		json_object_set_number(json_object(wave), "PC", pgm_addr);
		json_object_set_number(json_object(wave), "wave_inst_dw0", wd->ws.wave_inst_dw0);
		if (asic->family < FAMILY_NV)
			json_object_set_number(json_object(wave), "wave_inst_dw1", wd->ws.wave_inst_dw1);

		JSON_Value *status = json_value_init_object();
		json_object_set_number(json_object(status), "value", wd->ws.wave_status.value);
		json_object_set_number(json_object(status), "scc", wd->ws.wave_status.scc);
		json_object_set_number(json_object(status), "execz", wd->ws.wave_status.execz);
		json_object_set_number(json_object(status), "vccz", wd->ws.wave_status.vccz);
		json_object_set_number(json_object(status), "in_tg", wd->ws.wave_status.in_tg);
		json_object_set_number(json_object(status), "halt", wd->ws.wave_status.halt);
		json_object_set_number(json_object(status), "valid", wd->ws.wave_status.valid);
		json_object_set_number(json_object(status), "spi_prio", wd->ws.wave_status.spi_prio);
		json_object_set_number(json_object(status), "wave_prio", wd->ws.wave_status.wave_prio);
		json_object_set_number(json_object(status), "priv", wd->ws.wave_status.priv);
		json_object_set_number(json_object(status), "trap_en", wd->ws.wave_status.trap_en);
		json_object_set_number(json_object(status), "trap", wd->ws.wave_status.trap);
		json_object_set_number(json_object(status), "ttrace_en", wd->ws.wave_status.ttrace_en);
		json_object_set_number(json_object(status), "export_rdy", wd->ws.wave_status.export_rdy);
		json_object_set_number(json_object(status), "in_barrier", wd->ws.wave_status.in_barrier);
		json_object_set_number(json_object(status), "ecc_err", wd->ws.wave_status.ecc_err);
		json_object_set_number(json_object(status), "skip_export", wd->ws.wave_status.skip_export);
		json_object_set_number(json_object(status), "perf_en", wd->ws.wave_status.perf_en);
		json_object_set_number(json_object(status), "cond_dbg_user", wd->ws.wave_status.cond_dbg_user);
		json_object_set_number(json_object(status), "cond_dbg_sys", wd->ws.wave_status.cond_dbg_sys);
		json_object_set_number(json_object(status), "allow_replay", wd->ws.wave_status.allow_replay);
		json_object_set_number(json_object(status), "fatal_halt", asic->family >= FAMILY_AI && wd->ws.wave_status.fatal_halt);
		json_object_set_number(json_object(status), "must_export", wd->ws.wave_status.must_export);

		json_object_set_value(json_object(wave), "status", status);

		JSON_Value *hw_id = json_value_init_object();
		if (asic->family < FAMILY_NV) {
			json_object_set_number(json_object(hw_id), "value", wd->ws.hw_id.value);
			json_object_set_number(json_object(hw_id), "wave_id", wd->ws.hw_id.wave_id);
			json_object_set_number(json_object(hw_id), "simd_id", wd->ws.hw_id.simd_id);
			json_object_set_number(json_object(hw_id), "pipe_id", wd->ws.hw_id.pipe_id);
			json_object_set_number(json_object(hw_id), "cu_id", wd->ws.hw_id.cu_id);
			json_object_set_number(json_object(hw_id), "sh_id", wd->ws.hw_id.sh_id);
			json_object_set_number(json_object(hw_id), "tg_id", wd->ws.hw_id.tg_id);
			json_object_set_number(json_object(hw_id), "state_id", wd->ws.hw_id.state_id);
			json_object_set_number(json_object(hw_id), "vm_id", wd->ws.hw_id.vm_id);
			vmid = wd->ws.hw_id.vm_id;
		} else {
			json_object_set_number(json_object(hw_id), "value", wd->ws.hw_id1.value);
			json_object_set_number(json_object(hw_id), "wave_id", wd->ws.hw_id1.wave_id);
			json_object_set_number(json_object(hw_id), "simd_id", wd->ws.hw_id1.simd_id);
			json_object_set_number(json_object(hw_id), "wgp_id", wd->ws.hw_id1.wgp_id);
			json_object_set_number(json_object(hw_id), "se_id", wd->ws.hw_id1.se_id);
			json_object_set_number(json_object(hw_id), "sa_id", wd->ws.hw_id1.sa_id);
			json_object_set_number(json_object(hw_id), "queue_id", wd->ws.hw_id2.queue_id);
			json_object_set_number(json_object(hw_id), "pipe_id", wd->ws.hw_id2.pipe_id);
			json_object_set_number(json_object(hw_id), "me_id", wd->ws.hw_id2.me_id);
			json_object_set_number(json_object(hw_id), "state_id", wd->ws.hw_id2.state_id);
			json_object_set_number(json_object(hw_id), "wg_id", wd->ws.hw_id2.wg_id);
			json_object_set_number(json_object(hw_id), "compat_level", wd->ws.hw_id2.compat_level);
			json_object_set_number(json_object(hw_id), "vm_id", wd->ws.hw_id2.vm_id);
			vmid = wd->ws.hw_id2.vm_id;
		}
		json_object_set_value(json_object(wave), "hw_id", hw_id);

		JSON_Value *gpr_alloc = json_value_init_object();
		json_object_set_number(json_object(gpr_alloc), "vgpr_base", wd->ws.gpr_alloc.vgpr_base);
		json_object_set_number(json_object(gpr_alloc), "vgpr_size", wd->ws.gpr_alloc.vgpr_size);
		json_object_set_number(json_object(gpr_alloc), "sgpr_base", wd->ws.gpr_alloc.sgpr_base);
		json_object_set_number(json_object(gpr_alloc), "sgpr_size", wd->ws.gpr_alloc.sgpr_size);
		json_object_set_value(json_object(wave), "gpr_alloc", gpr_alloc);

		if (is_halted && wd->ws.gpr_alloc.value != 0xbebebeef) {
			int shift;
			if (asic->family <= FAMILY_CIK || asic->family >= FAMILY_NV)
				shift = 3;
			else
				shift = 4;

			int spgr_count = (wd->ws.gpr_alloc.sgpr_size + 1) << shift;
			JSON_Value *sgpr = json_value_init_array();
			for (int x = 0; x < spgr_count; x++) {
				json_array_append_number(json_array(sgpr), wd->sgprs[x]);
			}
			json_object_set_value(json_object(wave), "sgpr", sgpr);

			JSON_Value *threads = json_value_init_array();
			int num_threads = wd->num_threads;
			for (int thread = 0; thread < num_threads; thread++) {
				unsigned live = thread < 32 ? (wd->ws.exec_lo & (1u << thread))	: (wd->ws.exec_hi & (1u << (thread - 32)));
				json_array_append_boolean(json_array(threads), live ? 1 : 0);
			}
			json_object_set_value(json_object(wave), "threads", threads);


			if (wd->have_vgprs) {
				unsigned granularity = asic->parameters.vgpr_granularity;
				unsigned vpgr_count = (wd->ws.gpr_alloc.vgpr_size + 1) << granularity;
				JSON_Value *vgpr = json_value_init_array();
				for (int x = 0; x < (int) vpgr_count; x++) {
					JSON_Value *v = json_value_init_array();
					for (int thread = 0; thread < num_threads; thread++) {
						json_array_append_number(json_array(v), wd->vgprs[thread * 256 + x]);
					}
					json_array_append_value(json_array(vgpr), v);
				}
				json_object_set_value(json_object(wave), "vgpr", vgpr);
			}

			/* */
			if (include_shaders && (wd->ws.wave_status.halt || wd->ws.wave_status.fatal_halt)) {
				struct umr_shaders_pgm *shader = umr_find_shader_in_stream(stream, vmid, pgm_addr);
				uint32_t shader_size;
				uint64_t shader_addr;
				if (shader) {
					shader_size = shader->size;
					shader_addr = shader->addr;
				} else {
					#define NUM_OPCODE_WORDS 16
					pgm_addr -= (NUM_OPCODE_WORDS*4)/2;
					shader_addr = pgm_addr;
					shader_size = NUM_OPCODE_WORDS * 4;
					#undef NUM_OPCODE_WORDS
				}

				char tmp[128];
				sprintf(tmp, "%lx", shader_addr);
				/* This given shader isn't there, so read it. */
				if (json_object_get_value(json_object(shaders), tmp) == NULL) {
					JSON_Value *shader = shader_pgm_to_json(asic, vmid, shader_addr, shader_size);
					if (shader)
						json_object_set_value(json_object(shaders), tmp, shader);
				}
				json_object_set_string(json_object(wave), "shader", tmp);
			}
		}

		json_array_append_value(json_array(waves), wave);

		struct umr_wave_data *old = wd;
		wd = wd->next;
		free(old);
	}

	json_object_set_value(out, "waves", waves);
	if (include_shaders)
		json_object_set_value(out, "shaders", shaders);
	else
		json_value_free(shaders);

	if (stream)
		umr_free_pm4_stream(stream);
}

JSON_Value *umr_process_json_request(JSON_Object *request)
{
	JSON_Value *answer = NULL;
	const char *last_error;
	const char *command = json_object_get_string(request, "command");

	if (!command) {
		last_error = "missing command";
		goto error;
	}

	if (asics[0] == NULL) {
		init_asics();
	}

	struct umr_asic *asic = NULL;
	JSON_Object *asc = json_object_get_object(request, "asic");
	if (asc) {
		unsigned did = json_object_get_number(asc, "did");
		int instance = json_object_get_number(asc, "instance");
		for (int i = 0; !asic; i++) {
			if (asics[i] && asics[i]->did == did && asics[i]->instance == instance)
				asic = asics[i];
		}
	}

	int is_enumerate = strcmp(command, "enumerate") == 0;

	if (!asic && !is_enumerate) {
		last_error = "asic not found";
		goto error;
	}

	if (is_enumerate) {
		int i = 0, j;
		answer = json_value_init_array();
		while (asics[i]) {
			JSON_Value *as = json_value_init_object ();
			json_object_set_string(json_object(as), "name", asics[i]->asicname);
			json_object_set_number(json_object(as), "index", i);
			json_object_set_number(json_object(as), "instance", asics[i]->instance);
			json_object_set_number(json_object(as), "did", asics[i]->did);
			json_object_set_number(json_object(as), "family", asics[i]->family);
			json_object_set_number(json_object(as), "vram_size", asics[i]->config.vram_size);
			json_object_set_number(json_object(as), "vis_vram_size", asics[i]->config.vis_vram_size);
			json_object_set_string(json_object(as), "vbios_version", asics[i]->config.vbios_version);
			JSON_Value *fws = json_value_init_array();
			j = 0;
			while (asics[i]->config.fw[j].name[0] != '\0') {
				JSON_Value *fw = json_value_init_object();
				json_object_set_string(json_object(fw), "name", asics[i]->config.fw[j].name);
				json_object_set_number(json_object(fw), "feature_version", asics[i]->config.fw[j].feature_version);
				json_object_set_number(json_object(fw), "firmware_version", asics[i]->config.fw[j].firmware_version);
				json_array_append_value(json_array(fws), fw);
				j++;
			}
			json_object_set_value(json_object(as), "firmwares", fws);

			/* Discover the rings */
			{
				JSON_Value *rings = json_value_init_array();
				char fname[256];
				struct dirent *dir;
				sprintf(fname, "/sys/kernel/debug/dri/%d/", asics[i]->instance);
				DIR *d = opendir(fname);
				if (d) {
					while ((dir = readdir(d))) {
						if (strncmp(dir->d_name, "amdgpu_ring_", strlen("amdgpu_ring_")) == 0) {
							json_array_append_string(json_array(rings), dir->d_name);
						}
					}
					closedir(d);
				}
				json_object_set_value(json_object(as), "rings", rings);
			}

			/* PCIe link speed/width */
			{
				char fname[256];
				sprintf(fname, "/sys/class/drm/card%d/device/current_link_speed", asics[i]->instance);
				const char *content = read_file(fname);
				JSON_Value *pcie = json_value_init_object();
				if (content)
					json_object_set_string(json_object(pcie), "speed", content);
				sprintf(fname, "/sys/class/drm/card%d/device/current_link_width", asics[i]->instance);
				uint64_t width = read_sysfs_uint64(fname);
				if (width)
					json_object_set_number(json_object(pcie), "width", width);
				json_object_set_value(json_object(as), "pcie", pcie);
			}

			/* If this asic has been discovered through ip_discovery, send the dump to the client
			 * so it can recreate it.
			 */
			if (asics[i]->was_ip_discovered && ip_discovery_dump[i]) {
				json_object_set_string(json_object(as), "ip_discovery_dump", ip_discovery_dump[i]);
			}

			json_array_append_value(json_array(answer), as);
			i++;
		}
	} else if (strcmp(command, "read") == 0) {
		const char *block = json_object_get_string(request, "block");
		struct umr_reg *r = umr_find_reg_data_by_ip(
			asic, block, json_object_get_string(request, "register"));

		if (r == NULL) {
			last_error = "unknown register";
			goto error;
		}

		answer = json_value_init_object();
		unsigned value = umr_read_reg_by_name_by_ip(asic, (char*) json_object_get_string(request, "block"), r->regname);
		json_object_set_number(json_object(answer), "value", value);
	} else if (strcmp(command, "accumulate") == 0) {
		JSON_Array *regs = json_object_get_array(request, "registers");
		const int num_reg = json_array_get_count(regs);
		char *ipname = (char*) json_object_get_string(request, "block");
		struct umr_reg **reg = malloc(num_reg * sizeof(struct umr_reg*));
		for (int i = 0; i < num_reg; i++) {
			reg[i] = umr_find_reg_data_by_ip(asic, ipname, json_array_get_string(regs, i));
			if (!reg[i]) {
				printf("Inconsistent state detected: server and client disagree on ASIC definition.\n");
				free(reg);
				goto error;
			}
		}

		answer = json_value_init_object();
		int step_ms = json_object_get_number(request, "step_ms");
		int period_ms = json_object_get_number(request, "period");
		unsigned *counters = calloc(32 * num_reg, sizeof(unsigned));

		/* Disable GFXOFF */
		if (asic->fd.gfxoff >= 0) {
			uint32_t value = 0;
			write(asic->fd.gfxoff, &value, sizeof(value));
		}

		char path[256];
		sprintf(path, "/sys/kernel/debug/dri/%d/amdgpu_fence_info", asic->instance);
		const char *content_before = read_file(path);

		struct timespec req, rem;
		int steps = period_ms / step_ms;
		for (int i = 0; i < steps; i++) {
			req.tv_sec = 0;
			req.tv_nsec = step_ms * 1000000;

			for (int j = 0; j < num_reg; j++) {
				uint64_t value = (uint64_t)asic->reg_funcs.read_reg(asic,
																	reg[j]->addr * (reg[j]->type == REG_MMIO ? 4 : 1),
																	reg[j]->type);
				for (int k = 0; k < reg[j]->no_bits; k++) {
					uint64_t v = umr_bitslice_reg_quiet(asic, reg[j], reg[j]->bits[k].regname, value);
					counters[32 * j + k] += (unsigned)v;
				}
			}

			while (nanosleep(&req, &rem) == EINTR) {
				req = rem;
			}
		}

		/* Re-enable GFXOFF */
		if (asic->fd.gfxoff >= 0) {
			uint32_t value = 1;
			write(asic->fd.gfxoff, &value, sizeof(value));
		}

		char *copy = strdup(content_before);
		JSON_Value *fences = compare_fence_infos(copy, read_file(path));
		free(copy);
		json_object_set_value(json_object(answer), "fences", fences);

		JSON_Value *values = json_value_init_array();
		for (int j = 0; j < num_reg; j++) {
			JSON_Value *regvalue = json_value_init_array();
			for (int k = 0; k < reg[j]->no_bits; k++) {
				JSON_Value *v = json_value_init_object();
				json_object_set_string(json_object(v), "name", reg[j]->bits[k].regname);
				json_object_set_number(json_object(v), "counter", counters[num_reg * j + k]);
				json_array_append_value(json_array(regvalue), v);
			}
			json_array_append_value(json_array(values), regvalue);
		}
		json_object_set_value(json_object(answer), "values", values);
		free(counters);
		free(reg);
	} else if (strcmp(command, "write") == 0) {
		struct umr_reg *r = umr_find_reg_data_by_ip(
			asic, json_object_get_string(request, "block"), json_object_get_string(request, "register"));

		if (r == NULL) {
			last_error = "unknown register";
			goto error;
		}

		answer = json_value_init_object();

		char *block = (char*) json_object_get_string(request, "block");
		unsigned value = json_object_get_number(request, "value");
		if (umr_write_reg_by_name_by_ip(asic, block, r->regname, value)) {
			value = umr_read_reg_by_name_by_ip(asic, block, r->regname);
		}
		json_object_set_number(json_object(answer), "value", value);
	} else if (strcmp(command, "vm-read") == 0 || strcmp(command, "vm-decode") == 0) {
		uint64_t address = json_object_get_number(request, "address");
		JSON_Value *vmidv = json_object_get_value(request, "vmid");
		uint32_t vmid = UMR_LINEAR_HUB;
		if (vmidv)
			vmid = json_number(vmidv);

		uint64_t *buf = NULL;
		unsigned size = json_object_get_number(request, "size");
		if (size % 8) {
			size += 8 - size % 8;
		}

		asic->options.verbose = 1;
		asic->mem_funcs.vm_message = dummy_printf;
		asic->mem_funcs.va_addr_decode = my_va_decode;

		memset(page_table, 0, sizeof(page_table));
		num_page_table_entries = 0;

		if (strcmp(command, "vm-read") == 0) {
			buf = malloc((sizeof(uint64_t) * size) / 8);
		} else {
			size = 4;
		}

		int r = umr_read_vram(asic, asic->options.vm_partition, vmid, address, size, buf);
		if (r && buf) {
			memset(buf, 0, size);
			num_page_table_entries = 0;
		}

		asic->mem_funcs.vm_message = NULL;
		asic->options.verbose = 0;

		answer = json_value_init_object();

		if (buf) {
			JSON_Value *value = json_value_init_array();
			for (int i = 0; i < (int) size / 8; i++) {
				json_array_append_number(json_array(value), buf[i]);
			}
			json_object_set_value(json_object(answer), "values", value);
			free(buf);
		}
		JSON_Value *pt = json_value_init_array();
		for (int i = 0; i < num_page_table_entries; i++) {
			JSON_Value *level = json_value_init_object();
			json_object_set_number(json_object(level), "pba", page_table[i].pba);
			if (page_table[i].type == 2)
				json_object_set_number(json_object(level), "va_mask", page_table[i].va_mask);
			json_object_set_number(json_object(level), "type", page_table[i].type);
			json_object_set_number(json_object(level), "system", page_table[i].system);
			json_object_set_number(json_object(level), "tmz", page_table[i].tmz);
			json_object_set_number(json_object(level), "mtype", page_table[i].mtype);
			json_array_append_value(json_array(pt), level);
		}
		json_object_set_value(json_object(answer), "page_table", pt);
	}
	else if (strcmp(command, "waves") == 0) {
		int halt_waves = json_object_get_boolean(request, "halt_waves");
		int resume_waves = json_object_get_boolean(request, "resume_waves");
		int disable_gfxoff = json_object_get_boolean(request, "disable_gfxoff");
		strcpy(asic->options.ring_name, json_object_get_string(request, "ring"));

		if (disable_gfxoff && asic->fd.gfxoff >= 0) {
			uint32_t value = 0;
			write(asic->fd.gfxoff, &value, sizeof(value));
		}

		if (halt_waves) {
			umr_sq_cmd_halt_waves(asic, UMR_SQ_CMD_HALT);
		}

		asic->options.skip_gprs = 0;
		asic->options.halt_waves = halt_waves;
		asic->options.verbose = 0;

		int is_halted = umr_pm4_decode_ring_is_halted(asic, asic->options.ring_name);

		answer = json_value_init_object();

		wave_to_json(asic, is_halted, 1, json_object(answer));

		if (disable_gfxoff && asic->fd.gfxoff >= 0) {
			uint32_t value = 1;
			write(asic->fd.gfxoff, &value, sizeof(value));
		}
		if (resume_waves)
			umr_sq_cmd_halt_waves(asic, UMR_SQ_CMD_RESUME);
	} else if (strcmp(command, "resume-waves") == 0) {
		strcpy(asic->options.ring_name, json_object_get_string(request, "ring"));
		umr_sq_cmd_halt_waves(asic, UMR_SQ_CMD_RESUME);
		answer = json_value_init_object();
	} else if (strcmp(command, "ring") == 0) {
		char *ring_name = (char*)json_object_get_string(request, "ring");
		uint32_t wptr, rptr, drv_wptr, ringsize, value, *ring_data;
		int halt_waves = json_object_get_boolean(request, "halt_waves");
		int limit_ptr = json_object_get_boolean(request, "rptr_wptr");

		/* Disable gfxoff */
		value = 0;
		if (asic->fd.gfxoff >= 0)
			write(asic->fd.gfxoff, &value, sizeof(value));

		struct ring_decoding_data data;
		data.ibs = json_array(json_value_init_array());
		data.open_ibs = json_array(json_value_init_array());
		data.shaders = json_array(json_value_init_array());

		answer = json_value_init_object();

		char path[256];
		sprintf(path, "/sys/kernel/debug/dri/%d/amdgpu_fence_info", asic->instance);
		const char *fence_info = read_file(path);
		JSON_Array *signaled_fences = get_rings_last_signaled_fences(fence_info, ring_name);

		ring_data = umr_read_ring_data(asic, ring_name, &ringsize);
		/* read pointers */
		ringsize /= 4;
		rptr = ring_data[0] % ringsize;
		wptr = ring_data[1] % ringsize;
		drv_wptr = ring_data[2] % ringsize;

		if (!memcmp(ring_name, "sdma", 4) ||
			!memcmp(ring_name, "page", 4)) {
			struct umr_sdma_stream_decode_ui fn = { sdma_start_ib, sdma_start_opcode, sdma_add_field, sdma_unhandled, sdma_unhandled_size, sdma_unhandled_subop, sdma_done, &data };
			struct umr_sdma_stream *ps = umr_sdma_decode_ring(asic, &fn, ring_name, limit_ptr ? -1 : 0, limit_ptr ? -1 : (int)(ringsize - 1));

			if (ps) {
				/* Ring content */
				umr_sdma_decode_stream_opcodes(asic, &fn, ps, 0, 0, 0, 0, ~0UL, 1);
				json_object_set_value(json_object(answer), "ring", data.ring);
				json_object_set_value(json_object(answer), "ibs", json_array_get_wrapping_value(data.ibs));
				json_object_set_number(json_object(answer), "decoder", 3);
				umr_free_sdma_stream(ps);
			}
		} else {
			struct umr_pm4_stream_decode_ui fn;
			fn.data = &data;
			fn.start_ib = ring_start_ib;
			fn.start_opcode = ring_start_opcode;
			fn.add_field = ring_add_field;
			fn.add_shader = ring_add_shader;
			fn.add_data = ring_add_data;
			fn.unhandled = ring_unhandled;
			fn.done = ring_done;

			asic->options.halt_waves = halt_waves;
			struct umr_pm4_stream *str = umr_pm4_decode_ring(asic, ring_name, !halt_waves, limit_ptr ? -1 : 0, limit_ptr ? -1 : (int)(ringsize - 1));

			if (str) {
				umr_pm4_decode_stream_opcodes(asic, &fn, str, 0, 0, 0, 0, ~0UL, 1);
				json_object_set_value(json_object(answer), "shaders", json_array_get_wrapping_value(data.shaders));
				json_object_set_value(json_object(answer), "ring", data.ring);
				json_object_set_value(json_object(answer), "ibs", json_array_get_wrapping_value(data.ibs));
				json_object_set_number(json_object(answer), "decoder", 4);
				umr_free_pm4_stream(str);
			}
		}

		free(ring_data);
		json_object_set_number(json_object(answer), "read_ptr", rptr);
		json_object_set_number(json_object(answer), "write_ptr", wptr);
		json_object_set_number(json_object(answer), "driver_write_ptr", drv_wptr);
		json_object_set_number(json_object(answer), "last_signaled_fence",
			json_object_get_number(json_object(json_array_get_value(signaled_fences, 0)), "value"));

		/* Reenable gfxoff */
		value = 1;
		if (asic->fd.gfxoff >= 0)
			write(asic->fd.gfxoff, &value, sizeof(value));

	} else if (strcmp(command, "power") == 0) {
		const char *profiles[] = {
			"auto",
			"low",
			"high",
			"manual",
			"profile_standard",
			"profile_min_sclk",
			"profile_min_mclk",
			"profile_peak",
			NULL
		};

		answer = json_value_init_object();
		JSON_Value *valid = json_value_init_array();
		for (int i = 0; profiles[i]; i++)
			json_array_append_string(json_array(valid), profiles[i]);
		json_object_set_value(json_object(answer), "profiles", valid);
		const char *write = json_object_get_string(request, "set");
		char path[512];
		sprintf(path, "/sys/class/drm/card%d/device/power_dpm_force_performance_level", asic->instance);
		if (!write) {
			char *content = read_file(path);
			size_t s = strlen(content);

			if (s > 0 && content[s - 1] == '\n')
				content[s - 1] = '\0';

			int current = -1;
			for (int i = 0; profiles[i] && current < 0; i++) {
				if (!strcmp(content, profiles[i]))
					current = i;
			}
			json_object_set_string(json_object(answer), "current", current >= 0 ? profiles[current] : "");
		} else {
			FILE *fd = fopen(path, "w");
			if (fd) {
				fwrite(write, 1, strlen(write), fd);
				fclose(fd);
				json_object_set_string(json_object(answer), "current", write);
			} else {
				json_object_set_string(json_object(answer), "current", "");
			}
		}
	} else if (strcmp(command, "sensors") == 0) {
		static struct power_bitfield p_info[] = {
			{"GFX_SCLK", 0, AMDGPU_PP_SENSOR_GFX_SCLK, SENSOR_D100 },
			{"GFX_MCLK", 0, AMDGPU_PP_SENSOR_GFX_MCLK, SENSOR_D100 },
			{"AVG_GPU",  0, AMDGPU_PP_SENSOR_GPU_POWER, SENSOR_WAIT },
			{"GPU_LOAD", 0, AMDGPU_PP_SENSOR_GPU_LOAD, SENSOR_IDENTITY },
			{"MEM_LOAD", 0, AMDGPU_PP_SENSOR_MEM_LOAD, SENSOR_IDENTITY },
			{NULL, 0, 0, 0},
		};
		char fname[256];
		snprintf(fname, sizeof(fname)-1, "/sys/kernel/debug/dri/%d/amdgpu_sensors", asic->instance);
		asic->fd.sensors = open(fname, O_RDWR);
		answer = json_value_init_object();
		if (asic->fd.sensors) {
			uint32_t gpu_power_data[32];
			JSON_Array *values = json_array(json_value_init_array());
			for (int i = 0; p_info[i].regname; i++){
				int size = 4;
				p_info[i].value = 0;
				gpu_power_data[0] = 0;
				umr_read_sensor(asic, p_info[i].sensor_id, (uint32_t*)&gpu_power_data[0], &size);
				if (gpu_power_data[0] != 0){
					p_info[i].value = gpu_power_data[0];
					p_info[i].value = parse_sensor_value(p_info[i].map, p_info[i].value);
				}
				JSON_Object *v = json_object(json_value_init_object());
				json_object_set_string(v, "name", p_info[i].regname);
				json_object_set_number(v, "value", p_info[i].value);

				/* Determine min/max */
				{
					int min, max;
					if (i == 0) {
						snprintf(fname, sizeof(fname)-1, "/sys/class/drm/card%d/device/pp_dpm_sclk", asic->instance);
						parse_sysfs_clock_file(read_file(fname), &min, &max);
						json_object_set_string(v, "unit", "MHz");
					} else if (i == 1) {
						snprintf(fname, sizeof(fname)-1, "/sys/class/drm/card%d/device/pp_dpm_mclk", asic->instance);
						parse_sysfs_clock_file(read_file(fname), &min, &max);
						json_object_set_string(v, "unit", "MHz");
					} else if (i == 2) {
						min = 0;
						max = 300;
						json_object_set_string(v, "unit", "W");
					} else if (i >= 3 && i <= 4) {
						min = 0;
						max = 100;
						json_object_set_string(v, "unit", "%");
					} else {
						min = 15;
						max = 120;
						json_object_set_string(v, "unit", "°C");
					}
					json_object_set_number(v, "min", min);
					json_object_set_number(v, "max", max);
				}

				json_array_append_value(values, json_object_get_wrapping_value(v));
			}
			close(asic->fd.sensors);
			json_object_set_value(json_object(answer), "values", json_array_get_wrapping_value(values));
		}
	} else if (strcmp(command, "hwmon") == 0) {
		char dname[256], fname[1024];
		int values[4];

		if (json_object_has_value(request, "set")) {
			JSON_Object *set = json_object_get_object(request, "set");
			int fan_idx = json_object_get_number(set, "hwmon");
			int new_mode = json_object_get_number(set, "mode");
			if (new_mode >= 0) {
				snprintf(fname, sizeof(fname)-1, "/sys/class/drm/card%d/device/hwmon/hwmon%d/pwm1_enable", asic->instance, fan_idx);
				FILE *fd = fopen(fname, "w");
				if (fd) {
					fprintf(fd, "%d", new_mode);
					fclose(fd);
				}
			}
			int new_pwm = json_object_get_number(set, "value");
			if (new_pwm >= 0) {
				snprintf(fname, sizeof(fname)-1, "/sys/class/drm/card%d/device/hwmon/hwmon%d/pwm1", asic->instance, fan_idx);
				FILE *fd = fopen(fname, "w");
				if (fd) {
					fprintf(fd, "%d", new_pwm);
					fclose(fd);
				}
			}
		}

		const char * files[] = { "pwm1_enable", "fan1_min", "fan1_max", "pwm1" };
		answer = json_value_init_object();
		JSON_Array *hwmons = json_array(json_value_init_array());

		struct dirent *dir;
		sprintf(dname, "/sys/class/drm/card%d/device/hwmon/", asic->instance);
		DIR *d = opendir(dname);
		if (d) {
			while ((dir = readdir(d))) {
				if (strncmp(dir->d_name, "hwmon", 5) == 0) {
					int hwmon_id = 0;
					if (sscanf(dir->d_name + 5, "%d", &hwmon_id) == 1) {
						int r = 0;

						JSON_Object *hwmon = json_object(json_value_init_object());
						/* Read fan1 data */
						for (int i = 0; i < 4 && r == i; i++) {
							sprintf(fname, "%s/%s/%s", dname, dir->d_name, files[i]);
							r += sscanf(read_file(fname), "%d", &values[i]);
						}
						if (r == 4) {
							JSON_Value *fan = json_value_init_object();
							json_object_set_number(json_object(fan), "mode", values[0]);
							json_object_set_number(json_object(fan), "min", values[1]);
							json_object_set_number(json_object(fan), "max", values[2]);
							json_object_set_number(json_object(fan), "value", values[3]);
							json_object_set_number(hwmon, "id", hwmon_id);
							json_object_set_value(hwmon, "fan", fan);
						} else {
							json_value_free(json_object_get_wrapping_value(hwmon));
							continue;
						}

						json_array_append_value(hwmons, json_object_get_wrapping_value(hwmon));

						JSON_Array *temps = json_array(json_value_init_array());
						/* Read temp data */
						for (int i = 1;; i++) {
							int r = 0;
							sprintf(fname, "%s/%s/temp%d_input", dname, dir->d_name, i);
							r += sscanf(read_file(fname), "%d", &values[0]);
							sprintf(fname, "%s/%s/temp%d_crit", dname, dir->d_name, i);
							r += sscanf(read_file(fname), "%d", &values[1]);
							if (r == 2) {
								sprintf(fname, "%s/%s/temp%d_label", dname, dir->d_name, i);
								const char *label = read_file(fname);

								JSON_Object *temp = json_object(json_value_init_object());
								json_object_set_string_with_len(temp, "label", label, strlen(label) - 1);
								json_object_set_number(temp, "value", values[0]);
								json_object_set_number(temp, "critical", values[1]);
								json_array_append_value(temps, json_object_get_wrapping_value(temp));
							} else {
								break;
							}
						}
						if (json_array_get_count(temps))
							json_object_set_value(hwmon, "temp", json_array_get_wrapping_value(temps));
						else
							json_value_free(json_array_get_wrapping_value(temps));

					}
				}
			}
			closedir(d);
			if (json_array_get_count(hwmons))
				json_object_set_value(json_object(answer), "hwmons", json_array_get_wrapping_value(hwmons));
		}

	} else if (strcmp(command, "pp_features") == 0) {
		char path[512];
		sprintf(path, "/sys/class/drm/card%d/device/pp_features", asic->instance);
		if (!json_object_has_value(request, "set")) {
			char *content = read_file(path);
			if (content && strlen(content)) {
				answer = json_object_get_wrapping_value(parse_pp_features_sysfs_file(content));
			} else {
				last_error = "unsupported";
				goto error;
			}
		} else {
			FILE *fd = fopen(path, "w");
			if (fd) {
				char tmp[64];
				sprintf(tmp, "0x%lx", (uint64_t)json_object_get_number(request, "set"));
				fwrite(tmp, 1, strlen(tmp), fd);
				fclose(fd);
			}

			char *content = read_file(path);
			answer = json_object_get_wrapping_value(parse_pp_features_sysfs_file(content));
		}
	} else if (!strcmp(command, "memory-usage")) {
		const char *names[] = {
			"vram", "vis_vram", "gtt", NULL
		};
		const char *suffixes[] = {
			"total", "used", NULL
		};
		char path[256];

		answer = json_value_init_object();

		for (int i = 0; names[i]; i++) {
			JSON_Value *m = json_value_init_object();
			for (int j = 0; suffixes[j]; j++) {
				sprintf(path, "/sys/class/drm/card%d/device/mem_info_%s_%s", asic->instance, names[i], suffixes[j]);
				uint64_t v = read_sysfs_uint64(path);
				json_object_set_number(json_object(m), suffixes[j], v);
			}
			json_object_set_value(json_object(answer), names[i], m);
		}

		/* per pid reporting */
		sprintf(path, "/sys/kernel/debug/dri/%d/amdgpu_vm_info", asic->instance);
		JSON_Array *pids = parse_vm_info(read_file(path));
		json_object_set_value(json_object(answer), "pids", json_array_get_wrapping_value(pids));
	} else if (!strcmp(command, "drm-counters")) {
		uint64_t values[3] = { 0 };
		umr_query_drm(asic, 0x0f /* AMDGPU_INFO_NUM_BYTES_MOVED */, &values[0], sizeof(values[0]));
		umr_query_drm(asic, 0x18 /* AMDGPU_INFO_NUM_EVICTIONS */, &values[1], sizeof(values[0]));
		umr_query_drm(asic, 0x1E /* AMDGPU_INFO_NUM_VRAM_CPU_PAGE_FAULTS */, &values[2], sizeof(values[0]));
		answer = json_value_init_object();
		json_object_set_number(json_object(answer), "bytes-moved", (double)values[0]);
		json_object_set_number(json_object(answer), "num-evictions", (double)values[1]);
		json_object_set_number(json_object(answer), "cpu-page-faults", (double)values[2]);
	} else if (!strcmp(command, "evict")) {
		char path[256];
		int type = json_object_get_number(request, "type");
		const char *mem = type == 0 ? "vram" : "gtt";
		sprintf(path, "/sys/kernel/debug/dri/%d/amdgpu_evict_%s", asic->instance, mem);
		read_file(path);
	} else if (!strcmp(command, "kms")) {
		char path[256];
		sprintf(path, "/sys/kernel/debug/dri/%d/framebuffer", asic->instance);
		const char *content = read_file(path);
		JSON_Array *framebuffers = parse_kms_framebuffer_sysfs_file(content);

		sprintf(path, "/sys/kernel/debug/dri/%d/state", asic->instance);
		content = read_file(path);
		JSON_Object *state = parse_kms_state_sysfs_file(content);

		answer = json_object_get_wrapping_value(state);
		json_object_set_value(json_object(answer), "framebuffers", json_array_get_wrapping_value(framebuffers));

		/* Parse interesting DC registers */
		JSON_Array *crtcs = json_object_get_array(state, "crtcs");
		for (int i = 0; i < (int)json_array_get_count(crtcs); i++) {
			JSON_Object *crtc = json_object(json_array_get_value(crtcs, i));
			if (json_object_get_boolean(crtc, "active")) {
				sprintf(path, "mmHUBPREQ%d_DCSURF_SURFACE_CONTROL", i);
				struct umr_reg *r = umr_find_reg_data(asic, path);
				if (r) {
					uint64_t value = umr_read_reg_by_name(asic, path);
					int tmz = umr_bitslice_reg(asic, r, "PRIMARY_SURFACE_TMZ", value);
					int dcc = umr_bitslice_reg(asic, r, "PRIMARY_SURFACE_DCC_EN", value);
					json_object_set_number(crtc, "tmz", tmz);
					json_object_set_number(crtc, "dcc", dcc);
				}
			}
		}

		sprintf(path, "/sys/kernel/debug/dri/%d/amdgpu_dm_visual_confirm", asic->instance);
		if (json_object_has_value(request, "dm_visual_confirm")) {
			FILE *fd = fopen(path, "w");
			if (fd) {
				const char *v = json_object_get_boolean(request, "dm_visual_confirm") ? "1" : "0";
				fwrite(v, 1, 1, fd);
				fclose(fd);
			}
		}
		json_object_set_number(json_object(answer), "dm_visual_confirm", read_sysfs_uint64(path));

	} else {
		last_error = "unknown command";
		goto error;
	}

	JSON_Value *out = json_value_init_object();
	json_object_set_value(json_object(out), "answer", answer);
	json_object_set_value(json_object(out), "request", json_object_get_wrapping_value(request));

	return out;

error:
	answer = json_value_init_object();
	json_object_set_string(json_object(answer), "error", last_error);
	json_object_set_value(json_object(answer), "request", json_object_get_wrapping_value(request));
	return answer;
}

#if UMR_GUI_REMOTE
void run_server_loop(const char *url, struct umr_asic * asic)
{
	int sock = nn_socket(AF_SP, NN_REP);
	if (sock < 0) {
		exit(1);
	}

	int rv = nn_bind(sock, url);
	if (rv < 0) {
		exit(1);
	}

	int size = 100000000;
	if (nn_setsockopt(sock, NN_SOL_SOCKET, NN_RCVMAXSIZE, &size, sizeof(size)) < 0) {
		exit(0);
	}

	if (asic) {
		asics[0] = asic;
	} else {
		init_asics();
	}

	/* Everything is ready. Wait for commands */

	printf("Waiting for commands.\n");
	for (;;) {
		char* buf;
		int len = nn_recv(sock, &buf, NN_MSG, 0);
		if (len < 0)
			exit(0);
		else if (len == 0)
			continue;

		buf[len - 1] = '\0';
		JSON_Value *request = json_parse_string(buf);

		if (request == NULL) {
			printf("ERROR\n");
		} else {
			JSON_Value *answer = umr_process_json_request(json_object(request));
			char* s = json_serialize_to_string(answer);
			size_t len = strlen(s) + 1;
			if (nn_send(sock, s, len, 0) < 0)
				exit(0);
			json_value_free(answer);
			json_free_serialized_string(s);
		}
		nn_freemsg(buf);
	}
}
#endif

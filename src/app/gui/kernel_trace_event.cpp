/*
 * Copyright © 2024 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON-INFRINGEMENT. IN NO EVENT SHALL THE COPYRIGHT HOLDERS, AUTHORS
 * AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 */

#include "kernel_trace_event.h"
#include <ctype.h>
#include <cassert>
#include <cstdio>

int Event::parse_raw_event_buffer(void *raw_data, unsigned raw_data_size,
								  JSON_Array *names, EventBase* out) {
	unsigned consumed = 0;
	char *input = static_cast<char*>(raw_data);
	size_t n_names = json_array_get_count(names);

	while (consumed < raw_data_size) {
		int task_name_id, process_name_id;
		memcpy(&task_name_id, &input[consumed], sizeof(int));
		consumed += 4;
		assert(task_name_id < n_names);
		out->task = json_array_get_string(names, task_name_id);

		memcpy(&process_name_id, &input[consumed], sizeof(int));
		consumed += 4;
		assert(process_name_id < n_names);
		out->process = json_array_get_string(names, process_name_id);
		
		memcpy(&out->pid, &input[consumed], 4);
		consumed += 4;

		memcpy(&out->tgid, &input[consumed], 4);
		consumed += 4;

		memcpy(&out->timestamp, &input[consumed], 8);
		consumed += 8;

		EventType::Enum type;
		memcpy(&type, &input[consumed], 4);
		consumed += 4;

		// printf("%16s | %12s | %5d | %5d | %s | %lf | %s\n",
		//  	   out->task, out->process, out->pid, out->tgid, EventType::to_str(type), out->timestamp, &input[consumed]);

		/* Parse event name. */
		if (type == EventType::Unknown) {
			assert(false);
			continue;
		}

		out->type = type;

		return consumed;
	}

	return -1;
}

int Event::parse_event_fields(char *cursor, Event *event) {
	const size_t line_len = strlen(cursor);
	const char *line_end = cursor + line_len;
	bool nameless_fields = false;
	int field_idx = 0;

	/* Parse fields */
	do {
		/* Skip white spaces */
		while ((isspace(*cursor) || *cursor == ',') &&
			   *cursor != '|' &&
			   *cursor != '\0')
			cursor++;

		if (*cursor == '\n' || *cursor == '|')
			break;

		char *field_name_start, *field_name_end;
		char *value_start, *value_end;
		/* Possible formats:
		 *     field1=value1, field2=value2
		 *     field1=value1 field2=    value2
		 *     field1:value1 field 2:value2
		 *     value1, value2, value3
		 */
		field_name_start = cursor;
		while (*cursor != ':' &&
				*cursor != '=' &&
				*cursor != '\0' &&
				*cursor != ',' &&
				*cursor != '|') cursor++;
		if (!nameless_fields && (*cursor == '\0' || *cursor == '|'))
			break;

		if (*cursor == ',' || nameless_fields) {
			nameless_fields = true;
			value_start = field_name_start;
			value_end = cursor;
			field_name_start = NULL;
			field_name_end = NULL;
		} else {
			field_name_end = cursor++;

			if (*cursor) {
				while (isspace(*cursor) && *cursor != '\0') cursor++;
				value_start = cursor;
				if (*cursor == '(') {
					while (*cursor != ')') cursor++;
					cursor++;
				} else if (*cursor == '{') {
					while (*cursor != '}') cursor++;
					cursor++;
				} else {
					while (*cursor != ',' && *cursor != ' ' && *cursor != '\0') cursor++;
				}
				value_end = cursor;
			} else {
				break;
			}
		}

		event->handle_field(field_idx++, field_name_start, field_name_end - field_name_start,
						    value_start, value_end - value_start);
		if (value_end == line_end || *value_end == '\0' || *value_end == '|')
			break;
	} while (true);

	if (*cursor == '|') {
		event->stacktrace = strdup(cursor + 1);
	}

	return 1 + line_len;
}
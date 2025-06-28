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
#pragma once

#include <cstddef>
#include <cstring>
#include <cstdlib>
#include "parson.h"

namespace EventType
{
	enum Enum
	{
		Unknown = 0,
		DrmSchedJob,
		DrmRunJob,
		DrmSchedProcessJob,
		DrmSchedJobWaitDep,
		AmdgpuSchedRunJob,
		AmdgpuDeviceWreg,
	};

	inline const char *to_str(enum Enum t) {
		switch (t) {
			case Enum::DrmSchedJob: return "drm_sched_job";
			case Enum::DrmRunJob: return "drm_run_job";
			case Enum::DrmSchedProcessJob: return "drm_sched_process_job";
			case Enum::DrmSchedJobWaitDep: return "drm_sched_job_wait_dep";
			case Enum::AmdgpuSchedRunJob: return "amdgpu_sched_run_job";
			case Enum::AmdgpuDeviceWreg: return "amdgpu_device_wreg";
			default: return "unknown";
		}
	}
}


inline bool str_is(const char *str, const char *ref, int n = -1) {
	/* Assume ref is 0-terminated. */
	if (str == NULL)
		return false;
	if (n < 0)
		return strcmp(str, ref) == 0;
	if (strlen(ref) != n)
		return false;
	return strncmp(str, ref, n) == 0;
}

#define PARSE_INT(full, n, base)  else if (str_is(name, n, name_len)) full = strtoll(value, NULL, base)

struct EventBase {
	double timestamp;
	EventType::Enum type;
	const char *process; /* not owned */
	const char *task; /* not owned */
	const char *stacktrace; /* owned */
	int tgid;
	int pid;
};

struct Event {
	Event(EventType::Enum t, double ts, int p) : type(t), timestamp(ts), stacktrace(NULL) {
		pid = p;
	}

	Event(const EventBase& b) : type(b.type), timestamp(b.timestamp), stacktrace(NULL) {
		pid = b.pid;
	}

	virtual ~Event() {
		free(stacktrace);
	}

	virtual void _copy(const Event& evt) {
		type = evt.type;
		timestamp = evt.timestamp;
		pid = evt.pid;
		if(evt.stacktrace)
			stacktrace = strdup(evt.stacktrace);
		else
			stacktrace = NULL;
	}
	Event(const Event& evt) { _copy(evt); }

	Event& operator=(const Event& evt) {
		_copy(evt);
		return *this;
	}

	inline bool is(EventType::Enum t) const { return t == type; }

	virtual void handle_field(int field_idx, char *name, int name_len, char *value, int value_len) = 0;

	EventType::Enum type;
	double timestamp;
	char *stacktrace;
	int pid;

	static int parse_raw_event_buffer(void *raw_data, unsigned raw_data_size,
									  JSON_Array *names, EventBase* out);

	static int parse_event_fields(char *cursor, Event *event);
};

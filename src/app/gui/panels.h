/*
 * Copyright © 2021 Advanced Micro Devices, Inc.
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

extern "C" {
	#include "parson.h"
	#include "umr.h"
}
struct ImVec2;

class Panel {
public:
	Panel(struct umr_asic *_asic) : asic(_asic), info(NULL) {}
	virtual ~Panel() {
		if (info)
			json_value_free(json_object_get_wrapping_value(info));
	};
	virtual void process_server_message(JSON_Object *request, JSON_Value *answer) = 0;

	virtual bool display(float dt, const ImVec2& avail, bool can_make_request) = 0;

	void send_request(JSON_Value *req);

	void store_info(JSON_Value *answer) {
		if (info)
			json_value_free(json_object_get_wrapping_value(info));
		info = json_object(json_value_deep_copy(answer));
	}

protected:
	struct umr_asic *asic;
	JSON_Object *info;
};

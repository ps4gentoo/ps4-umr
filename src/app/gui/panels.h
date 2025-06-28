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
	virtual void process_server_message(JSON_Object *response, void *raw_data, unsigned raw_data_size) = 0;

	virtual bool display(float dt, const ImVec2& avail, bool can_make_request) = 0;

	void send_request(JSON_Value *req);

	void store_info(JSON_Value *answer) {
		if (info)
			json_value_free(json_object_get_wrapping_value(info));
		info = json_object(json_value_deep_copy(answer));
	}

	struct umr_asic *asic;

	static const char* format_duration(double dt) {
		static char txt[32];
		if (dt > 1)
			sprintf(txt, "%.3f sec", dt);
		else if (dt > 0.001)
			sprintf(txt, "%.3f ms", dt * 1000);
		else
			sprintf(txt, "%.3f us", dt * 1000000);
		return txt;
	}

protected:
	JSON_Object *info;
};

static inline const char *color_to_hex_str(const ImColor& color) {
	static char tmp[64];
	sprintf(tmp, "%02x%02x%02x", (color >> IM_COL32_R_SHIFT) & 0xff,
								 (color >> IM_COL32_G_SHIFT) & 0xff,
								 (color >> IM_COL32_B_SHIFT) & 0xff);
	return tmp;
}

/* Solarized palette (MIT License), https://github.com/altercation/solarized */
ImColor palette[] = {
	ImColor(181, 137,   0),
	ImColor(203,  75,  22),
	ImColor(220,  50,  47),
	ImColor(211,  54, 130),
	ImColor(108, 113, 196),
	ImColor( 38, 139, 210),
	ImColor( 42, 161, 152),
	ImColor(133, 153,   0),
	ImColor(131, 148, 150),
	ImColor(238, 232, 213),
	ImColor(253, 246, 227),
};

/* Block palette generated with:
 *   glasbey.create_block_palette([6, 6, 6, 6, 6, 6])
 */
ImColor block_palette[] = {
ImColor(89, 0, 0),
 ImColor(138, 0, 12),
 ImColor(186, 32, 28),
 ImColor(227, 93, 45),
 ImColor(247, 146, 69),
 ImColor(251, 182, 85),
 ImColor(0, 0, 158),
 ImColor(16, 40, 219),
 ImColor(36, 97, 251),
 ImColor(73, 146, 247),
 ImColor(117, 178, 243),
 ImColor(174, 223, 247),
 ImColor(0, 45, 0),
 ImColor(0, 81, 0),
 ImColor(20, 117, 0),
 ImColor(69, 150, 12),
 ImColor(113, 186, 28),
 ImColor(170, 231, 45),
 ImColor(93, 8, 97),
 ImColor(150, 32, 154),
 ImColor(194, 53, 198),
 ImColor(243, 89, 247),
 ImColor(255, 162, 255),
 ImColor(255, 206, 247),
 ImColor(40, 36, 36),
 ImColor(73, 69, 73),
 ImColor(109, 105, 109),
 ImColor(150, 142, 134),
 ImColor(194, 182, 154),
 ImColor(239, 219, 162),
 ImColor(4, 65, 57),
 ImColor(8, 113, 101),
 ImColor(8, 146, 130),
 ImColor(20, 186, 162),
 ImColor(69, 243, 194),
 ImColor(146, 255, 166)
};
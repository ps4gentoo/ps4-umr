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
#include "panels.h"
#include <map>
#include <string>

class KmsPanel : public Panel {
public:
	KmsPanel(struct umr_asic *asic) : Panel(asic), last_answer(NULL), read_interval(0.5) {
		delta_since_last_read = 10;
		visual_plane_debug = false;
	}

	~KmsPanel() {
		if (last_answer)
			json_value_free(json_object_get_wrapping_value(last_answer));
	}

	void process_server_message(JSON_Object *response, void *raw_data, unsigned raw_data_size) {
		JSON_Value *error = json_object_get_value(response, "error");
		if (error)
			return;

		JSON_Object *request = json_object(json_object_get_value(response, "request"));
		JSON_Value *answer = json_object_get_value(response, "answer");
		const char *command = json_object_get_string(request, "command");

		if (!strcmp(command, "kms")) {
			if (last_answer)
				json_value_free(json_object_get_wrapping_value(last_answer));
			last_answer = json_object(json_value_deep_copy(answer));
			visual_plane_debug = json_object_get_number(last_answer, "dm_visual_confirm");
		}
	}

	float CenterText(const char *text, float width) {
		float w = ImGui::CalcTextSize(text).x;
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + width / 2 - w / 2);
		return w;
	}

	struct Group {
		float container_width;
		ImVec2 top_left;
		ImVec2 padding;
	};
	std::vector<Group> groups;
	void BeginBorderedGroup(const ImVec2& frame_padding, float container_width = 0) {
		ImVec2 top_left = ImGui::GetCursorScreenPos();
		ImVec2 padded = top_left;
		padded.x += frame_padding.x;
		padded.y += frame_padding.y;
		ImGui::SetCursorScreenPos(padded);

		ImGui::BeginGroup();

		Group gp;
		gp.top_left = top_left;
		if (container_width == 0) {
			assert(!groups.empty());
			Group parent = groups.back();
			container_width = parent.container_width - parent.padding.x * 2;
		}
		gp.container_width = container_width;
		gp.padding = frame_padding;

		groups.push_back(gp);

		ImVec2 br = top_left;
		br.y += 100000;
		br.x += container_width;
		ImGui::PushClipRect(top_left, br, true);
	}

	ImVec4 EndBorderedGroup(ImU32 color) {
		assert(!groups.empty());
		Group gp = groups.back();

		ImVec2 br = ImGui::GetCursorScreenPos();
		br.y += gp.padding.y;
		ImGui::SetCursorScreenPos(br);
		br.x = gp.top_left.x + gp.container_width;
		ImGui::GetWindowDrawList()->AddRect(gp.top_left, br, color);
		ImGui::EndGroup();
		groups.pop_back();

		ImGui::PopClipRect();

		ImVec4 border;
		border.x = gp.top_left.x;
		border.y = gp.top_left.y;
		border.z = br.x;
		border.w = br.y;
		return border;
	}

	void Connect(ImVec2 p1, ImVec2 p4, ImU32 col) {
		ImVec2 p2(p1.x * 0.2 + p4.x * 0.8, p1.y);
		ImVec2 p3(p1.x * 0.8 + p4.x * 0.2, p4.y);
		ImGui::PushClipRect(fullscreen_top_left, fullscreen_bottom_right, false);
		ImGui::GetWindowDrawList()->AddBezierCubic(p1, p2, p3, p4, col, 1);
		ImGui::PopClipRect();
	}

	bool display(float dt, const ImVec2& avail, bool can_send_request) {
		if (!last_answer) {
			if (can_send_request) {
				send_kms_command();
			}
			return false;
		}

		ImGui::BeginDisabled(!can_send_request);

		ImGui::Text("KMS plane visual indicator:");
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip(
				"Toggle amdgpu_dm_visual_confirm debug.\n"
				"Note: this needs a planes reconfiguration to be effective.\n"
				"(see https://www.kernel.org/doc/html/latest/gpu/amdgpu/display/dc-debug.html)");
		}
		ImGui::SameLine();
		ImGui::PushID("visual_plane_debug");
		if (ImGui::Checkbox("", &visual_plane_debug)) {
			send_kms_command(visual_plane_debug ? 1 : 0);
		}
		ImGui::PopID();
		ImGui::SameLine();
		ImGui::Dummy(ImVec2(30.0, 0));
		ImGui::SameLine();
		if (ImGui::Button("Refresh")) {
			send_kms_command();
			delta_since_last_read = 0;
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::Text("Auto-refresh:");
		ImGui::SameLine();
		bool autorefresh_enabled = read_interval >= 0;
		ImGui::PushID("autorefresh");
		if (ImGui::Checkbox("", &autorefresh_enabled)) {
			read_interval = autorefresh_enabled ? 0.0 : -1;
		}
		ImGui::PopID();
		if (autorefresh_enabled) {
			ImGui::SameLine();
			ImGui::Text("Interval:");
			ImGui::SameLine();
			ImGui::DragFloat(" (drag to modify)", &read_interval, 0.1, 0, 10, "%.1f sec");
		}
		ImGui::Separator();

		fullscreen_top_left = ImGui::GetCursorPos();
		fullscreen_bottom_right = fullscreen_top_left;
		fullscreen_bottom_right.y += 100000;
		fullscreen_bottom_right.x += avail.x;

		if (autorefresh_enabled) {
			if (delta_since_last_read > read_interval) {
				if (can_send_request) {
					send_kms_command();
					delta_since_last_read = 0;
				}
			} else {
				delta_since_last_read += dt;
			}
		}

		ImGui::BeginChild("kms");
		ImGui::BeginTable("state", 4, ImGuiTableFlags_Borders);
		ImGui::TableSetupColumn("Framebuffers");
		ImGui::TableSetupColumn("Planes");
		ImGui::TableSetupColumn("CRTCs");
		ImGui::TableSetupColumn("Connectors");
		ImGui::TableHeadersRow();

		ImGui::TableNextRow();

		std::map<int, ImVec2> fb_resolutions;
		std::map<int, ImVec4> fb_frames, crtc_frames;
		std::map<int, ImU32> crtc_colors;
		ImU32 connectors_colors[] = {
			ImColor(12, 232, 56),
			ImColor(232, 45, 45),
			ImColor(120, 115, 200),
			ImColor(80, 210, 156),
		};

		/* Framebuffers */
		ImGui::TableSetColumnIndex(0);
		ImGui::NewLine();
		JSON_Array *fbs = json_object_get_array(last_answer, "framebuffers");
		std::vector<std::string> apps;
		for (int i = 0; i < json_array_get_count(fbs); i++) {
			JSON_Object *fb = json_object(json_array_get_value(fbs, i));
			const char *owner = json_object_get_string(fb, "allocated by");
			if (std::find(apps.begin(), apps.end(), owner) == apps.end()) {
				apps.push_back(owner);
			}
		}

		const ImVec2 frame_padding = ImGui::GetStyle().FramePadding;
		float colwidth = avail.x / 4 - 5 * frame_padding.x;
		ImVec2 colpadding = ImVec2(frame_padding.x * 3, frame_padding.y * 3);
		while (!apps.empty()) {
			const char *owner = apps.back().c_str();

			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + frame_padding.x);
			BeginBorderedGroup(colpadding, colwidth);
			CenterText(owner, colwidth);
			if (strcmp(owner, "[fbcon]") == 0) {
				ImGui::TextUnformatted(owner);
			} else {
				if (ImGui::Button(owner)) {
					goto_tab(SDLK_o);
				}
			}
			std::vector<JSON_Object *> sorted_fb;

			for (int i = 0; i < json_array_get_count(fbs); i++) {
				JSON_Object *fb = json_object(json_array_get_value(fbs, i));

				if (strcmp(owner, json_object_get_string(fb, "allocated by")))
					continue;
				sorted_fb.push_back(fb);
			}

			std::sort(sorted_fb.begin(), sorted_fb.end(), [](JSON_Object *a, JSON_Object *b) {
				return json_object_get_number(a, "id") < json_object_get_number(b, "id");
			});

			for (auto fb: sorted_fb) {
				BeginBorderedGroup(frame_padding);

				int id = (int) json_object_get_number(fb, "id");
				char plane_id[128];
				sprintf(plane_id, "id: #d33682%d", id);
				JSON_Object *size = json_object(json_object_get_value(fb, "size"));
				fb_resolutions[id] = ImVec2(json_object_get_number(size, "w"), json_object_get_number(size, "h"));
				if (ImGui::CollapsingHeader(plane_id)) {
					const char *fmt = json_object_get_string(fb, "format");
					const char *fourcc = strchr(fmt, '(');
					ImGui::Text("format: %.*s", (int)(fourcc - fmt), fmt);
					fourcc++;
					ImGui::Text("fourcc: %.*s", (int)(strlen(fourcc) - 1), fourcc);
					ImGui::Text("modifier: 0x%s", json_object_get_string(fb, "modifier"));
					if (json_object_has_value(fb, "modifier_str") && ImGui::IsItemHovered()) {
						ImGui::BeginTooltip();
						ImGui::Text("Description: ");
						const char *str = json_object_get_string(fb, "modifier_str");
						ImGui::Indent();
						do {
							const char *next = strchr(str + 1, ',');
							ImGui::TextUnformatted(str, next);
							str = next ? next + 1 : NULL;
						} while (str);
						ImGui::Unindent();
						ImGui::EndTooltip();
					}
					ImGui::Text("size: %dx%d",
						(int)json_object_get_number(size, "w"),
						(int)json_object_get_number(size, "h"));

					JSON_Array *layers = json_object_get_array(fb, "layers");
					ImVec2 p = frame_padding;
					p.x += 10; p.y += 5;
					BeginBorderedGroup(p);
					CenterText("Layers", groups.back().container_width);
					ImGui::Text("Layers");
					for (int j = 0; j < json_array_get_count(layers); j++) {
						BeginBorderedGroup(frame_padding);
						JSON_Object *layer = json_object(json_array_get_value(layers, j));
						JSON_Object *size = json_object(json_object_get_value(layer, "size"));
						ImGui::Text("size: #3097a1%dx#3097a1%d",
							(int)json_object_get_number(size, "w"),
							(int)json_object_get_number(size, "h"));
						ImGui::Text("pitch: %d", (int) json_object_get_number(layer, "pitch"));
						EndBorderedGroup(IM_COL32(0x40, 0x40, 0x40, 0x80));
					}
					EndBorderedGroup(IM_COL32(0x40, 0x40, 0x40, 0xff));
				}

				fb_frames[id] = EndBorderedGroup(IM_COL32(0x80, 0x80, 0x80, 0xff));

				ImGui::SetCursorPosY(ImGui::GetCursorPosY() + frame_padding.y);
			}
			apps.pop_back();
			EndBorderedGroup(ImGui::GetColorU32(ImGuiCol_Text));
			ImGui::NewLine();
		}


		/* CRTCs */
		ImGui::TableSetColumnIndex(2);
		ImGui::NewLine();
		JSON_Array *crtcs = json_object_get_array(last_answer, "crtcs");
		for (int i = 0; i < json_array_get_count(crtcs); i++) {
			JSON_Object *crtc = json_object(json_array_get_value(crtcs, i));

			float w = CenterText("Address 0x0123456701234567", colwidth);
			BeginBorderedGroup(colpadding, w + colpadding.x * 2);
			int id = (int) json_object_get_number(crtc, "id");
			ImGui::Text("id: #3097a1%d", id);
			int enable = (int) json_object_get_number(crtc, "enable");
			ImGui::Text("enable: %d", enable);
			int active = (int) json_object_get_number(crtc, "active");
			ImGui::Text("active: %d", active);

			if (active) {
				ImGui::Separator();
				ImGui::Text("dcc: %s", json_object_get_number(crtc, "dcc") ? "#34de51on" : "#586e75off");
				ImGui::Text("tmz: %s", json_object_get_number(crtc, "tmz") ? "#34de51on" : "#586e75off");
			}

			crtc_frames[id] = EndBorderedGroup(IM_COL32(0x80, 0x80, 0x80, 0xff));

			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + frame_padding.y);
		}

		/* Connectors */
		ImGui::TableSetColumnIndex(3);
		ImGui::NewLine();
		JSON_Array *connectors = json_object_get_array(last_answer, "connectors");
		for (int i = 0; i < json_array_get_count(connectors); i++) {
			JSON_Object *conn = json_object(json_array_get_value(connectors, i));

			const char *name = json_object_get_string(conn, "name");
			float w = CenterText(name, colwidth);
			BeginBorderedGroup(colpadding, w + colpadding.x * 2);
			ImU32 color = connectors_colors[i % 4];
			ImGui::Text("#%02x%02x%02x%s",
				(color >> IM_COL32_R_SHIFT) & 0xff,
				(color >> IM_COL32_G_SHIFT) & 0xff,
				(color >> IM_COL32_B_SHIFT) & 0xff,
				name);

			int has_crtc = json_object_has_value(conn, "crtc");
			ImVec4 frame = EndBorderedGroup(has_crtc ?
				color :
				IM_COL32(0x80, 0x80, 0x80, 0xff));

			if (has_crtc) {
				int crtc = json_object_get_number(conn, "crtc");
				ImVec4 b = crtc_frames[crtc];
				ImVec2 o(b.z, (b.y + b.w) * 0.5);
				Connect(o, ImVec2(frame.x, (frame.y + frame.w) * 0.5), color);
				ImGui::PushClipRect(fullscreen_top_left, fullscreen_bottom_right, false);
				ImGui::GetWindowDrawList()->AddRect(ImVec2(b.x, b.y), ImVec2(b.z, b.w),
					color);
				ImGui::PopClipRect();
				crtc_colors[crtc] = color;
			}
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + frame_padding.y);
		}

		/* Planes */
		ImGui::TableSetColumnIndex(1);
		ImGui::NewLine();
		JSON_Array *planes = json_object_get_array(last_answer, "planes");
		std::map<int, std::vector<ImVec4>> crtc_from_planes;
		for (int i = 0; i < json_array_get_count(planes); i++) {
			JSON_Object *plane = json_object(json_array_get_value(planes, i));

			float w = CenterText("fb: XXXX", colwidth);
			BeginBorderedGroup(colpadding, w + colpadding.x * 2);
			int id = (int) json_object_get_number(plane, "id");
			ImGui::Text("id: #b58900%d", id);
			int fb = (int) json_object_get_number(plane, "fb");
			ImU32 col = IM_COL32(0x80, 0x80, 0x80, 0xff);
			if (fb) {
				ImGui::Text("fb: #d33682%d", fb);

				JSON_Object *crtc = json_object(json_object_get_value(plane, "crtc"));
				if (crtc) {
					int c = json_object_get_number(crtc, "id");
					col = crtc_colors[c];
				}

				ImVec4 frame = EndBorderedGroup(col);

				if (crtc) {
					int c = json_object_get_number(crtc, "id");

					/* fb -> plane */
					ImVec2 a(frame.x, (frame.y + frame.w) * 0.5);
					ImVec4 fb_frame = fb_frames[fb];
					ImVec2 b(fb_frame.z, (fb_frame.y + fb_frame.w) * 0.5);
					Connect(a, b, col);

					ImGui::PushClipRect(fullscreen_top_left, fullscreen_bottom_right, false);
					ImGui::GetWindowDrawList()->AddRect(
						ImVec2(fb_frame.x, fb_frame.y),
						ImVec2(fb_frame.z, fb_frame.w), col);
					ImGui::PopClipRect();

					if (crtc_from_planes.find(c) == crtc_from_planes.end())
						crtc_from_planes[c] = std::vector<ImVec4>();

					JSON_Object *pos = json_object(json_object_get_value(crtc, "pos"));
					crtc_from_planes[c].push_back(
						ImVec4(frame.z, (frame.y + frame.w) * 0.5,
						json_object_get_number(pos, "x"), json_object_get_number(pos, "y")));

					/* Resolution */
					char txt_x[32], txt_y[32];
					ImVec2 plane_out(frame.z, frame.y);
					sprintf(txt_x, "w: %d", (int) fb_resolutions[fb].x);
					sprintf(txt_y, "h: %d", (int) fb_resolutions[fb].y);
					plane_out.x += frame_padding.x;

					ImGui::GetWindowDrawList()->AddText(plane_out, ImGui::GetColorU32(ImGuiCol_Text), txt_x);
					plane_out.y = frame.w - ImGui::GetTextLineHeight();
					ImGui::GetWindowDrawList()->AddText(plane_out, ImGui::GetColorU32(ImGuiCol_Text), txt_y);
				}
			} else {
				EndBorderedGroup(col);
			}

			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + frame_padding.y);
		}
		ImGui::EndTable();

		/* Draw the planes -> crtc lines */
		for (auto it: crtc_from_planes) {
			int crtc = it.first;
			ImVec4 frame = crtc_frames[crtc];
			ImU32 col = crtc_colors[crtc];

			std::vector<ImVec4> crtc_pins = it.second;

			float height = (frame.w - frame.y) - 4 * frame_padding.y;
			float base = frame.y + 2 * frame_padding.y;
			float v_steps;
			if (crtc_pins.size() == 1) {
				base += height / 2;
				v_steps = 0;
			} else {
				v_steps = height / (crtc_pins.size() - 1);
			}

			for (int i = 0; i < (int) crtc_pins.size(); i++) {
				ImVec4 v = crtc_pins[i];
				ImVec2 plane_pin = ImVec2(v.x, v.y);
				ImVec2 crtc_pin = ImVec2(frame.x, base + i * v_steps);

				Connect(plane_pin, crtc_pin, col);

				char txt_x[32], txt_y[32];
				sprintf(txt_x, "x: %d", (int)v.z);
				sprintf(txt_y, "y: %d", (int) v.w);
				crtc_pin.y -= 2 * ImGui::GetTextLineHeight();
				crtc_pin.x -= std::max(ImGui::CalcTextSize(txt_x).x, ImGui::CalcTextSize(txt_y).x)
					+ frame_padding.x;

				ImGui::GetWindowDrawList()->AddText(crtc_pin, ImGui::GetColorU32(ImGuiCol_Text), txt_x);
				crtc_pin.y += ImGui::GetTextLineHeight();
				ImGui::GetWindowDrawList()->AddText(crtc_pin, ImGui::GetColorU32(ImGuiCol_Text), txt_y);
			}
		}

		ImGui::EndChild();

		return read_interval > 0;
	}

private:
	void send_kms_command(int enable_visual_debug = -1) {
		JSON_Value *req = json_value_init_object();
		json_object_set_string(json_object(req), "command", "kms");
		if (enable_visual_debug != -1) {
			json_object_set_boolean(json_object(req), "dm_visual_confirm", enable_visual_debug);
		}
		send_request(req);
	}

private:
	JSON_Object *last_answer;
	float delta_since_last_read;
	float read_interval;
	ImVec2 fullscreen_top_left, fullscreen_bottom_right;
	bool visual_plane_debug;
};


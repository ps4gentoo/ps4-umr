/*
 * Copyright © 2023 Advanced Micro Devices, Inc.
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
#include "imgui/imgui.h"
#include "panels.h"

class BufferObjectPanel : public Panel {
public:
	BufferObjectPanel(struct umr_asic *asic) : Panel(asic), last_answer_gem_info(NULL),
											   last_answer_peak_bo(NULL), texture_id(0),
											   raw_data(NULL), zoom_to_fit(true),
											   last_error_peak_bo(NULL), zoom(1),
											   only_display_pinned_bo(false) {

	}
	~BufferObjectPanel() {
		if (last_answer_gem_info)
			json_value_free(json_object_get_wrapping_value(last_answer_gem_info));
	}

	void process_server_message(JSON_Object *response, void *_raw_data, unsigned _raw_data_size) {
		JSON_Value *error = json_object_get_value(response, "error");
		JSON_Object *request = json_object(json_object_get_value(response, "request"));
		JSON_Value *answer = json_object_get_value(response, "answer");
		const char *command = json_object_get_string(request, "command");

		if (!strcmp(command, "gem-info") && !error) {
			if (last_answer_gem_info)
				json_value_free(json_object_get_wrapping_value(last_answer_gem_info));
			last_answer_gem_info = json_object(json_value_deep_copy(answer));
		}
		if (!strcmp(command, "peak-bo")) {
			if (last_answer_peak_bo) {
				json_value_free(json_object_get_wrapping_value(last_answer_peak_bo));
				last_answer_peak_bo = NULL;
			}

			if (error) {
				if (last_error_peak_bo)
					free(last_error_peak_bo);
				last_error_peak_bo = strdup(json_string(error));
				displayed_bo = NULL;
			} else {
				last_answer_peak_bo = json_object(json_value_deep_copy(answer));
				if (this->raw_data)
					free(this->raw_data);
				this->raw_data = malloc(_raw_data_size);
				memcpy(this->raw_data, _raw_data, _raw_data_size);
				this->raw_data_size = _raw_data_size;
			}
		}
	}

	bool display(float dt, const ImVec2& avail, bool can_send_request) {
		if (raw_data) {
			/* The creation has to be created in the display thread because
			 * it requires the GL context.
			 */
			int width = json_object_get_number(last_answer_peak_bo, "width");
			int height = json_object_get_number(last_answer_peak_bo, "height");
			if (texture_id)
				glDeleteTextures(1, &texture_id);
			texture_id = texture_from_qoi_buffer(width, height,
												 this->raw_data, this->raw_data_size);
			free(raw_data);
			this->raw_data_size = 0;
			this->raw_data = NULL;
		}

		if (can_send_request) {
			if (!last_answer_gem_info)
				send_gem_info_command();
		}

		if (ImGui::Button("Refresh List")) {
			displayed_bo = NULL;
			send_gem_info_command();
			last_command = 0;
		}
		ImGui::SameLine();
		ImGui::Checkbox("Only #3097a1pinned buffers", &only_display_pinned_bo);
		ImGui::SameLine();
		ImGui::Checkbox("Zoom to fit", &zoom_to_fit);
		if (!zoom_to_fit) {
			ImGui::SliderFloat("Zoom", &zoom, 0.1, 10, "%.1f");
		}

		ImGui::Separator();

		if (!last_answer_gem_info)
			return 0;

		ImGui::BeginChild("list",
			ImVec2(ImGui::CalcTextSize("    (handle, resolution, format, sw) View >  ").x, 0),
			false, ImGuiWindowFlags_NoTitleBar);
		ImGui::PushStyleColor(ImGuiCol_Text, (ImU32)ImColor(229, 169, 41));
		ImGui::TextUnformatted("Buffer Object from pid");
		ImGui::PopStyleColor();
		bool display_help = true;
		JSON_Array *apps = json_object_get_array(last_answer_gem_info, "pids");
		for (int i = 0; i < json_array_get_count(apps); i++) {
			JSON_Value *_app = json_array_get_value(apps, i);
			if (json_value_get_type(_app) == JSONNull)
				continue;

			JSON_Object *app = json_object(_app);
			JSON_Array *bos = json_object_get_array(app, "bos");

			int bo_count = 0;
			for (int j = 0; j < json_array_get_count(bos); j++) {
				JSON_Value *_bo = json_array_get_value(bos, j);
				if (json_value_get_type(_bo) == JSONNull)
					continue;
				JSON_Object *bo = json_object(_bo);
				if (only_display_pinned_bo && !json_object_get_boolean(bo, "pinned"))
					continue;
				bo_count++;
			}
			if (bo_count == 0)
				continue;

			if (display_help) {
				ImGui::Indent();
				ImGui::Text("(handle, resolution, format, sw)");
				ImGui::Unindent();
			}
			display_help = false;

			ImGui::PushID(i);
			char label[256];
			sprintf(label, "%s (%d)", json_object_get_string(app, "command"), (int)json_object_get_number(app, "pid"));

			if (bo_count <= 5)
				ImGui::SetNextItemOpen(true, ImGuiCond_Always);

			if (ImGui::TreeNodeEx(label)) {
				for (int j = 0; j < json_array_get_count(bos); j++) {
					JSON_Value *_bo = json_array_get_value(bos, j);
					if (json_value_get_type(_bo) == JSONNull)
						continue;
					JSON_Object *bo = json_object(_bo);
					bool pinned = json_object_get_boolean(bo, "pinned");
					if (only_display_pinned_bo && !pinned)
						continue;
					ImGui::PushID(j);
					if (displayed_bo == bo) {
						ImGui::Unindent();
						ImGui::Bullet();
						ImGui::Indent();
						ImGui::SameLine();
					}

					ImGui::Text("%s0x%05x",
						pinned ? "#3097a1" : "#dbde79",
						(int) json_object_get_number(bo, "handle"));
					ImGui::SameLine();

					char img_label[512];
					snprintf(img_label, sizeof(img_label),
						"%dx%d",
						(int) json_object_get_number(bo, "width"),
						(int) json_object_get_number(bo, "height"));
					int l = strlen(img_label);
					ImGui::Text("%*s", (int)sizeof("resolution"), img_label);
					ImGui::SameLine();
					ImGui::Text("%*d", (int)sizeof("format"), (int) json_object_get_number(bo, "format"));
					ImGui::SameLine();
					ImGui::Text("%*d", (int)sizeof("sw"), (int) json_object_get_number(bo, "swizzle"));
					ImGui::SameLine();

					/* Hack the cursor position to get the button text aligned with the label. */
					ImGui::SetCursorPosY(ImGui::GetCursorPosY() - ImGui::GetStyle().FramePadding.y);
					if (ImGui::Button("View >")) {
						if (texture_id) {
							glDeleteTextures(1, &texture_id);
							texture_id = 0;
						}
						send_peak_bo_command(json_object_get_number(app, "pid"),
											 json_object_get_number(bo, "handle"),
											 json_object_get_number(bo, "gpu_fd"));
						displayed_bo = bo;
					}
					ImGui::PopID();
				}
				ImGui::TreePop();
			}
			ImGui::PopID();
		}
		if (display_help) {
			ImGui::Text("Buffer objects can only be shown for\n"
			            "applications using the Mesa driver and\n"
			            "launched using:");
			ImGui::Bullet();
			ImGui::SameLine();
			ImGui::PushStyleColor(ImGuiCol_Text, (ImU32)ImColor(48, 151, 161));
			ImGui::Text("AMD_DEBUG=extra_md");
			ImGui::PopStyleColor();
			ImGui::SameLine();
			ImGui::Text("(OpenGL / radeonsi)");
			ImGui::Bullet();
			ImGui::SameLine();
			ImGui::PushStyleColor(ImGuiCol_Text, (ImU32)ImColor(48, 151, 161));
			ImGui::Text("RADV_DEBUG=extra_md");
			ImGui::PopStyleColor();
			ImGui::SameLine();
			ImGui::Text("(Vulkan / radv)");
		}
		ImGui::NewLine();
		ImGui::Separator();
		ImGui::NewLine();

		ImGui::PushStyleColor(ImGuiCol_Text, (ImU32)ImColor(229, 169, 41));
		ImGui::TextUnformatted("Buffer Object from KMS framebuffers");
		ImGui::PopStyleColor();
		ImGui::Indent();
		ImGui::Text("(appname  id    resolution)");
		JSON_Array *fbs = json_object_get_array(last_answer_gem_info, "framebuffers");
		for (int i = 0; i < json_array_get_count(apps); i++) {
			JSON_Object *fb = json_object(json_array_get_value(fbs, i));
			if (json_object_has_value(fb, "metadata") == 0)
				continue;

			JSON_Object *md = json_object(json_object_get_value(fb, "metadata"));

			if (displayed_bo == md) {
				ImGui::Unindent();
				ImGui::Bullet();
				ImGui::Indent();
				ImGui::SameLine();
			}

			ImGui::PushID(i);
			const char *cmd = json_object_get_string(fb, "allocated by");
			int l = strlen(cmd);
			if (l > 15) l = 15;
			ImGui::Text(" #dbde79%.*s", l, cmd);
			ImGui::SameLine();
			ImGui::Text(" %d", (int)json_object_get_number(fb, "id"));
			if (ImGui::IsItemHovered()) {
				ImGui::BeginTooltip();
				ImGui::Text("#dbde79%s", json_object_get_string(fb, "allocated by"));
				ImGui::Text("pid: %d", (int) json_object_get_number(md, "pid"));
				ImGui::Text("fourcc: 0x%08x", (int) json_object_get_number(md, "fourcc"));
				ImGui::EndTooltip();
			}
			ImGui::SameLine();

			char img_label[512];
			snprintf(img_label, sizeof(img_label),
				"%dx%d",
				(int) json_object_get_number(md, "width"),
				(int) json_object_get_number(md, "height"));
			ImGui::Text("%*s", (int)sizeof("resolution") + 15 - l - 1, img_label);
			ImGui::SameLine();

			/* Hack the cursor position to get the button text aligned with the label. */
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() - ImGui::GetStyle().FramePadding.y);
			if (ImGui::Button("View >")) {
				if (texture_id) {
					glDeleteTextures(1, &texture_id);
					texture_id = 0;
				}
				send_peak_bo_command2(json_object_get_wrapping_value(md));
				displayed_bo = md;
			}
			ImGui::PopID();
		}
		ImGui::Unindent();
		ImGui::EndChild();

		ImGui::SameLine();
		ImGui::BeginChild("image", ImVec2(0, 0), false, ImGuiWindowFlags_NoTitleBar |
														ImGuiWindowFlags_HorizontalScrollbar);
		if (last_answer_peak_bo && texture_id) {
			float w, h;
			int width = json_object_get_number(last_answer_peak_bo, "width");
			int height = json_object_get_number(last_answer_peak_bo, "height");
			if (zoom_to_fit) {
				float ratio = height / (float) width;
				w = ImGui::GetContentRegionAvail().x - ImGui::GetStyle().FramePadding.x;
				h = ImGui::GetContentRegionAvail().y - ImGui::GetStyle().FramePadding.y;

				if (w * ratio < h)
					h = w * ratio;
				else
					w = h / ratio;
			} else {
				w = width * zoom;
				h = height * zoom;
			}

			ImGui::Image((ImTextureID) (intptr_t) texture_id, ImVec2(w, h),
										ImVec2(0, 0), ImVec2(1, 1),
										ImVec4(1,1,1,1), ImVec4(1,1,1,1));
		} else if (last_error_peak_bo) {
			ImVec2 space = ImGui::GetContentRegionAvail();
			float w = ImGui::CalcTextSize(last_error_peak_bo).x * 2;
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + space.x / 2 - w / 2);
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + space.y / 2);
			ImGui::Text("Error: %s", last_error_peak_bo);
		}
		ImGui::EndChild();

		return 0;
	}
private:
	void send_gem_info_command() {
		JSON_Value *req = json_value_init_object();
		json_object_set_string(json_object(req), "command", "gem-info");
		send_request(req);
	}

	void send_peak_bo_command(unsigned pid, unsigned handle, int gpu_fd) {
		JSON_Value *req = json_value_init_object();
		json_object_set_string(json_object(req), "command", "peak-bo");
		json_object_set_number(json_object(req), "pid", pid);
		json_object_set_number(json_object(req), "handle", handle);
		json_object_set_number(json_object(req), "gpu_fd", gpu_fd);
		send_request(req);
	}

	void send_peak_bo_command2(JSON_Value *md) {
		JSON_Value *req = json_value_init_object();
		json_object_set_string(json_object(req), "command", "peak-bo");
		json_object_set_value(json_object(req), "metadata", json_value_deep_copy(md));
		send_request(req);
	}

private:
	JSON_Object *last_answer_gem_info;
	JSON_Object *last_answer_peak_bo;
	void *displayed_bo;
	GLuint texture_id;
	void *raw_data;
	unsigned raw_data_size;
	float last_command;
	bool zoom_to_fit;
	float zoom;
	char *last_error_peak_bo;
	bool only_display_pinned_bo;
};

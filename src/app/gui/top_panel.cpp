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

class TopPanel : public Panel {
public:
	TopPanel(struct umr_asic *asic) : Panel(asic), last_accumulate_answer(NULL),
		ipname(NULL), fences_deltas(NULL),
		consumed(false), top_read_interval(0.5),
		last_sensor_read(0) { }

	~TopPanel() {
		if (last_accumulate_answer)
			json_value_free(json_object_get_wrapping_value(last_accumulate_answer));
	}

	void process_server_message(JSON_Object *request, JSON_Value *answer) {
		const char *command = json_object_get_string(request, "command");

		if (!strcmp(command, "accumulate")) {
			if (last_accumulate_answer)
				json_value_free(json_object_get_wrapping_value(last_accumulate_answer));
			last_accumulate_answer = json_object(json_value_deep_copy(answer));
			consumed = false;
		}
	}

	bool display(float dt, const ImVec2& avail, bool can_send_request) {
		static ImColor colors[] = {
			ImColor(232, 45, 45),
			ImColor(12, 232, 56),
			ImColor(120, 115, 200),
			ImColor(80, 210, 156),
		};

		if (!ipname) {
			for (int i = 0; i < (int) asic->no_blocks && !ipname; i++) {
				struct umr_ip_block *b = asic->blocks[i];
				for (int j = 0; j < b->no_regs; j++) {
					if (!strcmp(b->regs[j].regname, "mmGRBM_STATUS")) {
						ipname = b->ipname;
						break;
					}
				}
			}
		}

		if (last_sensor_read > top_read_interval) {
			if (can_send_request) {
				const char *regs[] = {"mmGRBM_STATUS", "mmGRBM_STATUS2", NULL};
				send_accumulate_command(ipname, top_read_interval * 1000, regs);
				last_sensor_read = 0;
			}
		} else {
			last_sensor_read += dt;
		}

		ImGui::DragFloat("Interval (drag to modify)", &top_read_interval, 0.01, 0.1, 1, "%.2f sec");
		ImGui::Separator();
		if (last_accumulate_answer) {
			ImGui::BeginChild("grbm bits", ImVec2(avail.x / 2, 0), false, ImGuiWindowFlags_NoTitleBar);
			ImGui::Text("Hardware blocks busyness:");
			ImGui::Separator();
			JSON_Array *values = json_object_get_array(last_accumulate_answer, "values");
			int max_counter_value = (top_read_interval * 1000) / 10;
			for (int i = 0; i < json_array_get_count(values); i++) {
				JSON_Array *val = json_array_get_array(values, i);
				for (int j = 0; j < json_array_get_count(val); j++) {
					JSON_Object *value = json_object(json_array_get_value(val, j));
					const char *name = json_object_get_string(value, "name");
					const size_t l = strlen(name);
					const char *pos = strstr(name, "_BUSY");
					if (pos && pos == (name + l - 5)) {
						int v = (int)(100 * json_object_get_number(value, "counter") / max_counter_value);

						if (v < 20) {
							ImGui::PushStyleColor(ImGuiCol_PlotHistogram, (ImU32)ImColor(52, 222, 81));
						} else if (v < 60) {
							ImGui::PushStyleColor(ImGuiCol_PlotHistogram, (ImU32)ImColor(229, 169, 41));
						} else {
							ImGui::PushStyleColor(ImGuiCol_PlotHistogram, (ImU32)ImColor(215, 36, 36));
						}

						ImGui::ProgressBar(v / 100.0, ImVec2(avail.x / 3, 0));
						ImGui::SameLine();
						ImGui::TextUnformatted(name, pos);
						ImGui::PopStyleColor(1);
					}
				}
			}
			ImGui::EndChild();
			if (!fences_deltas) {
				num_rings = json_array_get_count(json_object_get_array(info, "rings"));
				fences_deltas = (float *)calloc(num_rings * 100, sizeof(float));
				fence_deltas_offset = 0;
			}
			ImGui::SameLine();
			ImGui::BeginChild("drm bits", ImVec2(avail.x / 2, 0), false, ImGuiWindowFlags_NoTitleBar);
			ImGui::Text("Number of fences signaled per second for each hardware queue:");
			ImGui::Separator();
			JSON_Array *fences = json_object_get_array(last_accumulate_answer, "fences");

			ImVec2 sc = ImGui::GetCursorScreenPos();

			float max_value = 100;
			for (int i = 0; i < num_rings * 100; i++)
				max_value = std::max(max_value, fences_deltas[i]);

			if (max_value > 10000) {
				max_value = 10000 * ceil(max_value / 10000);
			} else if (max_value > 1000) {
				max_value = 1000 * ceil(max_value / 1000);
			} else {
				max_value = 100 * ceil(max_value / 100);
			}

			for (int i = 0; i < json_array_get_count(fences); i++) {
				JSON_Object *fence = json_object(json_array_get_value(fences, i));
				int delta = json_object_get_number(fence, "delta");
				fences_deltas[i * 100 + fence_deltas_offset] = delta / top_read_interval;

				ImGui::SetCursorScreenPos(sc);
				const char *ring_name = json_object_get_string(fence, "name");

				ImColor color;
				if (strstr(ring_name, "gfx")) {
					color = colors[0];
				} else if (strstr(ring_name, "comp")) {
					color = colors[1];
				} else if (strstr(ring_name, "sdma")) {
					color = colors[2];
				} else {
					color = colors[3];
				}
				ImGui::PushStyleColor(ImGuiCol_PlotLines, (ImU32)color);
				ImGui::PushStyleColor(ImGuiCol_Text, (ImU32)color);

				ImGui::PushID(ring_name);
				ImGui::PlotLines("",
					 &fences_deltas[i * 100],
					 100,
					 fence_deltas_offset + 1,
					 NULL,
					 0,
					 max_value * 1.05,
					 ImVec2(0, avail.y / 2),
					 sizeof(float),
					 i != 0);

				ImGui::SameLine();
				ImGui::SetCursorPosY(ImGui::GetCursorPosY() + i * ImGui::GetTextLineHeight());
				ImGui::Text("%12s: %5d fences/sec", ring_name, (int)fences_deltas[i * 100 + fence_deltas_offset]);

				ImGui::PopID();

				ImGui::PopStyleColor(2);

				if (i == 0) {
					ImVec2 start = ImGui::GetCursorScreenPos();
					ImVec2 end = start;

					start.x -= ImGui::GetStyle().FramePadding.x;
					start.y -= 2 * ImGui::GetStyle().FramePadding.y;
					end.x += ImGui::CalcItemWidth();

					float scale = (avail.y / 2) / (max_value * 1.05);
					float base_y = start.y;
					float step = max_value / 5;
					/* Draw max value */
					for (int i = 0; i <= 5; i++) {
						float y = (step * i) * scale;
						start.y = base_y - y;
						end.y = start.y;
						ImGui::GetWindowDrawList()->AddLine(start, end, ImGui::GetColorU32(ImGuiCol_TabActive));

						ImVec2 pos = start;
						pos.x += ImGui::GetStyle().FramePadding.x;
						pos.y -= ImGui::GetTextLineHeight();
						ImGui::SetCursorScreenPos(pos);
						ImGui::Text("%d\n", (int) step * i);
					}
				}
			}

			if (!consumed) {
				fence_deltas_offset = (fence_deltas_offset + 1) % 100;
				consumed = true;
			}
			ImGui::EndChild();
			return true;
		}
		return false;
	}

private:
	void send_accumulate_command(const char *ipname, int ms, const char **regname) {
		JSON_Value *req = json_value_init_object();
		json_object_set_string(json_object(req), "command", "accumulate");
		json_object_set_string(json_object(req), "block", ipname);
		JSON_Value *regs = json_value_init_array();
		for (int i = 0; regname[i]; i++)
			json_array_append_string(json_array(regs), regname[i]);
		json_object_set_value(json_object(req), "registers", regs);
		json_object_set_number(json_object(req), "period", ms);
		json_object_set_number(json_object(req), "step_ms", 10);
		send_request(req);
	}

private:
	JSON_Object *last_accumulate_answer;
	const char *ipname;
	float *fences_deltas;
	int fence_deltas_offset;
	bool consumed;
	float last_sensor_read;
	float top_read_interval;
	int num_rings;
};

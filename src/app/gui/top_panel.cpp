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
		fences_deltas(NULL),
		consumed(false), top_read_interval(0.5),
		last_sensor_read(0) {
		ipname = find_ip_name("mmGRBM_STATUS");
		if (ipname == NULL)
			ipname = find_ip_name("regGRBM_STATUS");
	}

	~TopPanel() {
		if (last_accumulate_answer)
			json_value_free(json_object_get_wrapping_value(last_accumulate_answer));
	}

	void process_server_message(JSON_Object *response, void *raw_data, unsigned raw_data_size) {
		JSON_Value *error = json_object_get_value(response, "error");
		if (error)
			return;

		JSON_Object *request = json_object(json_object_get_value(response, "request"));
		JSON_Value *answer = json_object_get_value(response, "answer");
		const char *command = json_object_get_string(request, "command");

		if (!strcmp(command, "accumulate")) {
			if (last_accumulate_answer)
				json_value_free(json_object_get_wrapping_value(last_accumulate_answer));
			last_accumulate_answer = json_object(json_value_deep_copy(answer));
			consumed = false;
		}
	}

	float CenterText(const char *text, const char *text_end, float base_x, float width) {
		float w = ImGui::CalcTextSize(text, text_end).x;
		return base_x + width / 2 - w / 2;
	}

	bool display(float dt, const ImVec2& avail, bool can_send_request) {
		static ImColor colors[] = {
			ImColor(232, 45, 45),
			ImColor(12, 232, 56),
			ImColor(120, 115, 200),
			ImColor(80, 210, 156),
		};

		if (last_sensor_read > top_read_interval) {
			if (can_send_request) {
				const char *regs[] = {"mmGRBM_STATUS", "mmGRBM_STATUS2", NULL};
				if (ipname == NULL) {
					printf("Couldn't find ip block with reg=%s\n", ipname);
				} else {
					send_accumulate_command(ipname, top_read_interval * 1000, regs);
				}
				last_sensor_read = 0;
			}
		} else {
			last_sensor_read += dt;
		}

		const float padding = ImGui::GetStyle().FramePadding.x;
		const float text_h = ImGui::CalcTextSize("A").y;
		ImGui::DragFloat("Interval (drag to modify)", &top_read_interval, 0.01, 0.1, 5, "%.1f sec");
		ImGui::Separator();

		const char *titles[] = { "CP", "Pipeline", "CB / DB", "Memory" };
		const char *filters[][6] = {
			{ "CP", "CPF", "CPC", "CPG", NULL, NULL },
			{ "GE", "SPI", "WD", "SX", "SC", "PA" },
			{ "CB", "DB", NULL, NULL, NULL, NULL },
			{ "TA", "GDS", "UTCL2", "TCP", "TC", NULL },
		};

		const float box_size = (ImGui::CalcTextSize("UTCL2").x + padding * 2);

		ImGui::BeginChild("top");

		if (ImGui::TreeNodeEx("Hardware blocks busyness", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::BeginChild("grbm bits", ImVec2(0, box_size * 2), false, ImGuiWindowFlags_NoTitleBar);
			if (last_accumulate_answer) {
				JSON_Array *values = json_object_get_array(last_accumulate_answer, "values");
				int max_counter_value = (top_read_interval * 1000) / 10;
				ImVec2 text_base = ImGui::GetCursorScreenPos();
				ImGui::NewLine();
				ImVec2 base = ImGui::GetCursorScreenPos();
				for (int h = 0; h < ARRAY_SIZE(titles); h++) {
					ImGui::SetCursorScreenPos(ImVec2(base.x, text_base.y));
					ImGui::TextUnformatted(titles[h]);
					for (int i = 0; i < json_array_get_count(values); i++) {
						JSON_Array *val = json_array_get_array(values, i);
						for (int j = 0; j < json_array_get_count(val); j++) {
							JSON_Object *value = json_object(json_array_get_value(val, j));
							const char *name = json_object_get_string(value, "name");
							const size_t l = strlen(name);
							const char *pos = strstr(name, "_BUSY");
							if (!pos)
								continue;
							if (pos != (name + l - 5))
								continue;
							bool match = false;
							for (int f = 0; f < 6 && !match; f++) {
								if (filters[h][f] == NULL)
									break;
								match = strncmp(name, filters[h][f], l - 5) == 0;
							}
							if (!match)
								continue;

							double v = (json_object_get_number(value, "counter") / max_counter_value);


							ImVec2 base_filled(base.x, base.y + (1 - v) * box_size);
							ImVec2 end(base.x + box_size, base.y + box_size);

							ImGui::GetWindowDrawList()->AddRectFilled(base_filled, end, (ImU32)ImColor(52, 222, 81));

							ImGui::GetWindowDrawList()->AddRect(base, end, IM_COL32_WHITE);
							float bx = CenterText(name, pos, base.x, box_size);
							ImGui::GetWindowDrawList()->AddText(
								ImVec2(bx, base.y + (box_size - text_h) * 0.5),
								IM_COL32_WHITE, name, pos);

							base.x = end.x + padding;
						}
					}
					base.x += box_size * 0.25;
				}
			}
			ImGui::EndChild();
			ImGui::TreePop();
		}

		if (ImGui::TreeNodeEx("Per-application GPU usage",ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::Separator();
			JSON_Object *fdinfo = json_object_get_object(last_accumulate_answer, "fdinfo");
			JSON_Object *start = json_object(json_object_get_value(fdinfo, "start"));
			JSON_Object *end = json_object(json_object_get_value(fdinfo, "end"));

			/* Entries are per drm-client-id. */
			for (int i = 0; i < json_object_get_count(start); i++) {
				const char *client_id = json_object_get_name(start, i);
				/* Make sure we also have an entry for this client in 'end' */
				JSON_Object *fde = json_object(json_object_get_value(end, client_id));
				if (fde == NULL)
					continue;
				JSON_Object *fds = json_object(json_object_get_value_at(start, i));

				bool client_legend_done = false;

				/* Now that we have a before/after entry for the same fd, lookup engines. */
				uint64_t dt = json_object_get_number(fde, "ts") - json_object_get_number(fds, "ts");
				const int engines = json_object_get_count(fds);
				for (size_t k = 0; k < engines; k++) {
					const char *name = json_object_get_name(fds, k);
					if (strcmp(name, "ts") == 0 || strcmp(name, "app") == 0 || strcmp(name, "fd") == 0)
						continue;
					uint64_t v1 = json_number(json_object_get_value_at(fds, k));
					uint64_t v2 = json_number(json_object_get_value_at(fde, k));

					uint64_t delta_ns = v2 - v1;

					if (v1 && v2 && delta_ns > 0) {
						if (!client_legend_done) {
							JSON_Object *app = json_object(json_object_get_value(fde, "app"));
							ImGui::Text("%d:%s", (int)json_object_get_number(app, "pid"),
												 json_object_get_string(app, "app"));
							client_legend_done = true;
						}
						ImGui::Indent();
						ImGui::Text("%8s", name);
						ImGui::SameLine();
						ImGui::ProgressBar((float)delta_ns / dt, ImVec2(avail.x / 3, 0));
						ImGui::SameLine();
						ImGui::Text("drm-client-id: %s, fd: %d",
							client_id, (int) json_object_get_number(fds, "fd"));
						ImGui::Unindent();
					}
				}
			}
			ImGui::TreePop();
		}

		if (ImGui::TreeNode("Fences count")) {
			if (!fences_deltas) {
				num_rings = json_array_get_count(json_object_get_array(info, "rings"));
				fences_deltas = (float *)calloc(num_rings * 100, sizeof(float));
				fence_deltas_offset = 0;
			}

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

			/* Draw graph (1 per queue) */
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
				ImGui::PlotLines(ring_name,
					 &fences_deltas[i * 100],
					 100,
					 fence_deltas_offset + 1,
					 NULL,
					 0,
					 max_value * 1.05,
					 ImVec2(avail.x, avail.y / 2),
					 sizeof(float),
					 i != 0);

				ImGui::PopID();

				ImGui::PopStyleColor(2);

				if (i == 0) {
					ImVec2 start = ImGui::GetCursorScreenPos();
					ImVec2 end = start;

					start.x -= ImGui::GetStyle().FramePadding.x;
					start.y -= 2 * ImGui::GetStyle().FramePadding.y;
					end.x += avail.x;

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
			ImGui::NewLine();
			/* Draw graph legend */
			for (int i = 0; i < json_array_get_count(fences); i++) {
				JSON_Object *fence = json_object(json_array_get_value(fences, i));
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

				ImGui::PushStyleColor(ImGuiCol_Text, (ImU32)color);

				ImGui::TextUnformatted(ring_name);
				ImGui::PopStyleColor(1);
			}

			if (!consumed) {
				fence_deltas_offset = (fence_deltas_offset + 1) % 100;
				consumed = true;
			}
			ImGui::TreePop();
		}
		ImGui::EndChild();

		return true;
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

	const char *find_ip_name(const char *reg) {
		for (int i = 0; i < (int) asic->no_blocks; i++) {
			struct umr_ip_block *b = asic->blocks[i];
			for (int j = 0; j < b->no_regs; j++) {
				if (!strcmp(b->regs[j].regname, reg)) {
					return b->ipname;
				}
			}
		}
		return NULL;
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

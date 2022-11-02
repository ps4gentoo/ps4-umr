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

#include <regex.h>

class WavesPanel : public Panel {
public:
	WavesPanel(struct umr_asic *asic) : Panel(asic) {
		/* SGPR */
		shader_syntax.add_definition("(s[[:digit:]]+|s\\[[[:digit:]]+:[[:digit:]]+\\])", { "#d33682" });
		/* VGPR */
		shader_syntax.add_definition("(v[[:digit:]]+|v\\[[[:digit:]]+:[[:digit:]]+\\])", { "#6c71c4" });
		/* Constants */
		shader_syntax.add_definition("(0x[[:digit:]]*)\\b", { "#b58900" });
		/* Comments */
		shader_syntax.add_definition("(;)", { "#586e75" });
		/* Keywords */
		shader_syntax.add_definition("(attr[[:digit:]]+|exec|m0|[[:alpha:]]+cnt\\([[:digit:]]\\))", { "#3097a1" });
	}

	void process_server_message(JSON_Object *request, JSON_Value *answer) {
		const char *command = json_object_get_string(request, "command");

		if (strcmp(command, "waves"))
			return;

		active_shader = NULL;
		if (last_answer) {
			json_value_free(json_object_get_wrapping_value(last_answer));
		}
		last_answer = json_object(json_value_deep_copy(answer));

		details.max_vgpr = 0;

		JSON_Array *waves = json_array(json_object_get_value(last_answer, "waves"));
		int wave_count = json_array_get_count(waves);
		for (int i = 0; i < wave_count ; i++) {
			JSON_Object *wave = json_object(json_array_get_value(waves, i));
			JSON_Value *vgpr = json_object_get_value(wave, "vgpr");
			if (vgpr) {
				int s = json_array_get_count(json_array(vgpr));
				details.max_vgpr = std::max(s, details.max_vgpr);
			}
		}

		if (details.max_vgpr) {
			details.vgpr = (bool*)realloc(details.vgpr, wave_count * details.max_vgpr);
			details.view = (int*)realloc(details.view, wave_count * details.max_vgpr * sizeof(int));
			memset(details.vgpr, 0, wave_count * details.max_vgpr);
			memset(details.view, 0, wave_count * details.max_vgpr * sizeof(int));
		}
	}

	bool display(float dt, const ImVec2& avail, bool can_send_request) {
		ImGui::Checkbox("Disable gfxoff", &turn_off_gfxoff);
		ImGui::SameLine();
		ImGui::Checkbox("Halt waves", &halt);
		if (halt) {
			ImGui::SameLine();
			ImGui::Checkbox("Resume waves", &resume);
		}
		ImGui::SameLine();
		ImGui::BeginDisabled(!can_send_request);
		if (ImGui::Button("Query")) {
			send_waves_command(halt, resume, turn_off_gfxoff);
		}
		ImGui::EndDisabled();

		ImGui::Separator();
		if (last_answer) {
			ImGui::BeginChild("Waves", ImVec2(avail.x / 2, 0), false, ImGuiWindowFlags_NoTitleBar);
			JSON_Array *waves = json_object_get_array(last_answer, "waves");
			JSON_Object *shaders = json_object(json_object_get_value(last_answer, "shaders"));
			bool force_scroll = false;
			int w = json_array_get_count(waves);
			for (int i = 0; i < w; i++) {
				JSON_Object *wave = json_object(json_array_get_value(waves, i));
				JSON_Object *status = json_object(json_object_get_value(wave, "status"));

				ImGui::PushID(i);

				int active_threads = -1;
				JSON_Array *threads = json_object_get_array(wave, "threads");
				if (threads) {
					active_threads = 0;
					int s = json_array_get_count(threads);
					for (int i = 0; i < s; i++) {
						active_threads += json_array_get_boolean(threads, i);
					}
				}
				const char *shader_address_str = json_object_get_string(wave, "shader");

				char label[256];
				if (active_threads < 0)
					sprintf(label, "Wave %d", i);
				else if (shader_address_str)
					sprintf(label, "Wave %d (#dbde79%d threads, valid PC)", i, active_threads);
				else
					sprintf(label, "Wave %d (#dbde79%d threads)", i, active_threads);

				if (ImGui::TreeNode(label)) {
					ImGui::Columns(3);
					ImGui::Text("se:            #586e750x%x", (unsigned int)json_object_get_number(wave, "se"));
					ImGui::NextColumn();
					ImGui::Text("sh:            #586e750x%x", (unsigned int)json_object_get_number(wave, "sh"));
					ImGui::NextColumn();
					ImGui::Text("cu: #586e750x%x", (unsigned int)json_object_get_number(wave, "cu"));
					ImGui::NextColumn();
					ImGui::Text("simd_id:       #586e750x%x", (unsigned int)json_object_get_number(wave, "simd_id"));
					ImGui::NextColumn();
					ImGui::Text("wave_id:       #586e750x%x", (unsigned int)json_object_get_number(wave, "wave_id"));
					ImGui::NextColumn();
					ImGui::NextColumn();
					ImGui::Text("wave_inst_dw0: #586e750x%08x", (unsigned int)json_object_get_number(wave, "wave_inst_dw0"));
					ImGui::NextColumn();
					ImGui::Text("wave_inst_dw1: #586e750x%08x", (unsigned int)json_object_get_number(wave, "wave_inst_dw1"));
					ImGui::Columns(1);
					ImGui::Separator();
					ImGui::NextColumn();
					ImGui::Text("PC: #b589000x%" PRIx64, (uint64_t)json_object_get_number(wave, "PC"));
					if (shader_address_str) {
						ImGui::SameLine();
						if (ImGui::Button("View Shader")) {
							active_shader = json_object(json_object_get_value(shaders, shader_address_str));
							sscanf(shader_address_str, "%" PRIx64, &base_address);
							pc = json_object_get_number(wave, "PC");

							force_scroll = true;
						}
					} else {
					}
					ImGui::NextColumn();
					if (ImGui::TreeNodeEx("Status")) {
						ImGui::Columns(4);
						size_t n = json_object_get_count(status);
						for (size_t j = 0; j < n; j++) {
							ImGui::Text("%s: #b58900%d", json_object_get_name(status, j),
														 (unsigned)json_number(json_object_get_value_at(status, j)));
							ImGui::NextColumn();
						}
						ImGui::Columns(1);
						ImGui::TreePop();
					}
					if (ImGui::TreeNodeEx("Hardware Id")) {
						ImGui::Columns(4);
						JSON_Object *hw_id = json_object(json_object_get_value(wave, "hw_id"));
						size_t n = json_object_get_count(hw_id);
						for (size_t j = 0; j < n; j++) {
							ImGui::Text("%s: #b58900%d", json_object_get_name(hw_id, j),
														 (unsigned)json_number(json_object_get_value_at(hw_id, j)));
							ImGui::NextColumn();
						}
						ImGui::Columns(1);
						ImGui::TreePop();
					}
					if (ImGui::TreeNodeEx("GPR Alloc")) {
						ImGui::Columns(4);
						JSON_Object *gpr_alloc = json_object(json_object_get_value(wave, "gpr_alloc"));
						size_t n = json_object_get_count(gpr_alloc);
						for (size_t j = 0; j < n; j++) {
							ImGui::Text("%s: #b58900%d", json_object_get_name(gpr_alloc, j),
														 (unsigned)json_number(json_object_get_value_at(gpr_alloc, j)));
							ImGui::NextColumn();
						}
						ImGui::Columns(1);
						ImGui::TreePop();
					}

					{
						static const char *formats[] = { "s%*d: #d33682%d", "s%*d: #d33682%u", "s%*d: #d33682%08x" };
						JSON_Array *sgpr = json_object_get_array(wave, "sgpr");
						if (sgpr && ImGui::TreeNodeEx("#d33682SGPRs")) {
							static int mode = 2;
							ImGui::Text("Display as:");
							ImGui::SameLine();
							ImGui::RadioButton("int", &mode, 0);
							ImGui::SameLine();
							ImGui::RadioButton("uint", &mode, 1);
							ImGui::SameLine();
							ImGui::RadioButton("hex", &mode, 2);
							ImGui::SameLine();
							ImGui::RadioButton("float", &mode, 3);
							ImGui::Columns(4);
							ImGui::PushID("sgpr");
							int s = json_array_get_count(sgpr);
							int align = s > 99 ? 3 : 2;
							for (int i = 0; i < s; i++) {
								JSON_Value *v = json_array_get_value(sgpr, i);
								ImGui::PushID(v);
								int aaa = (int)json_number(v);
								if (mode == 3) {
									float f = reinterpret_cast<float&>(aaa);
									ImGui::Text("s%*d: #d33682%f", align, i, f);
								} else {
									ImGui::Text(formats[mode], align, i, aaa);
								}
								ImGui::PopID();
								ImGui::NextColumn();
							}
							ImGui::PopID();
							ImGui::Columns(1);
							ImGui::TreePop();
						}
					}

					{
						static const char *formats[] = { "#6c71c4%d", "#6c71c4%u", "#6c71c4%08x" };
						JSON_Array *vgpr = json_object_get_array(wave, "vgpr");
						if (vgpr && ImGui::TreeNodeEx("#6c71c4VGPRs")) {
							int s = json_array_get_count(vgpr);

							ImGui::BeginTable("vgprvalues", 5, ImGuiTableFlags_Borders);
							ImGui::TableSetupColumn("Base");
							ImGui::TableSetupColumn("+ 0");
							ImGui::TableSetupColumn("+ 1");
							ImGui::TableSetupColumn("+ 2");
							ImGui::TableSetupColumn("+ 3");
							ImGui::TableHeadersRow();
							char label[128];
							for (int vg = 0; vg < s; vg++) {
								ImGui::PushID(vg);
								ImGui::TableNextRow();
								ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32(ImGuiCol_TableRowBgAlt));
								ImGui::TableSetColumnIndex(0);
								sprintf(label, "show v%2d", vg);
								ImGui::Checkbox(label, &details.vgpr[i * details.max_vgpr + vg]);
								if (details.vgpr[i * details.max_vgpr + vg]) {
									int *mode = &details.view[i * details.max_vgpr + vg];
									ImGui::TableSetColumnIndex(1);
									ImGui::RadioButton("as int", mode, 0);
									ImGui::TableSetColumnIndex(2);
									ImGui::RadioButton("as uint", mode, 1);
									ImGui::TableSetColumnIndex(3);
									ImGui::RadioButton("as hex", mode, 2);
									ImGui::TableSetColumnIndex(4);
									ImGui::RadioButton("as float", mode, 3);

									JSON_Array *vgp = json_array_get_array(vgpr, vg);
									int num_thread = json_array_get_count(vgp);

									for (int t = 0; t < num_thread; t++) {
										if (t % 4 == 0) {
											ImGui::TableNextRow();
											ImGui::TableSetColumnIndex(0);
											ImGui::Text("%d", t);
										}
										ImGui::TableSetColumnIndex(1 + t % 4);

										JSON_Value *v = json_array_get_value(vgp, t);
										ImGui::PushID(v);
										int aaa = (int)json_number(v);
										if (*mode == 3) {
											float f = reinterpret_cast<float&>(aaa);
											ImGui::Text("#6c71c4%f", f);
										} else {
											ImGui::Text(formats[*mode], aaa);
										}
										ImGui::PopID();
									}
								}
								ImGui::PopID();
							}
							ImGui::EndTable();
							ImGui::TreePop();
						}
					}
					if (ImGui::TreeNode(threads, "Threads")) {
						ImGui::Columns(4);
						int s = json_array_get_count(threads);
						for (int i = 0; i < s; i++) {
							ImGui::Text("%st%d: %s",
								i < 10 ? " " : "", i,
								json_array_get_boolean(threads, i) ?
										"#859900on" : "#dc322foff");
							ImGui::NextColumn();
						}
						ImGui::Columns(1);
						ImGui::TreePop();
					}

					ImGui::TreePop();
				}
				ImGui::PopID();
			}
			ImGui::EndChild();
			ImGui::SameLine();
			ImGui::BeginChild("Shaders", ImVec2(avail.x / 2, 0), false, ImGuiWindowFlags_NoTitleBar);
			if (active_shader) {
				int scroll = 0;
				JSON_Array *op = json_object_get_array(active_shader, "opcodes");
				uint32_t *copy = new uint32_t[json_array_get_count(op)];
				for (size_t j = 0; j < json_array_get_count(op); j++)
					copy[j] = (uint32_t)json_array_get_number(op, j);

				uint64_t base_address = json_object_get_number(active_shader, "address");
				char **opcode_strs = NULL;
				umr_shader_disasm(asic, (uint8_t *)copy, json_array_get_count(op) * 4, base_address, &opcode_strs);

				char tmp[128];
				sprintf(tmp, "0x%" PRIx64, base_address);

				ImGui::BeginTable("shader", 3, ImGuiTableFlags_Borders);
				ImGui::TableSetupColumn(tmp, ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize(" 0x0000000000 ").x);
				ImGui::TableSetupColumn("Raw Value", ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize(  "0x00000000  ").x);
				ImGui::TableSetupColumn("Disassembly");
				ImGui::TableHeadersRow();
				for (size_t j = 0; j < json_array_get_count(op); j++) {
					uint64_t addr = base_address + j * 4;
					bool is_pc = pc == addr;
					if (is_pc) {
						/* PC points to this instruction */
						scroll = ImGui::GetCursorPos().y;
						ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0, 0.5, 0.5, 1));
				   }

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("+ 0x%lx", j * 4);
					if (ImGui::IsItemHovered()) {
						ImGui::BeginTooltip();
						ImGui::Text("0x%" PRIx64, base_address + j * 4);
						ImGui::EndTooltip();
					}
					ImGui::TableSetColumnIndex(1);
					ImGui::Text("0x%08x", (uint32_t)json_array_get_number(op, j));
					ImGui::TableSetColumnIndex(2);
					ImGui::Text("%s", shader_syntax.transform(opcode_strs[j]));
					free(opcode_strs[j]);
					if (is_pc)
						ImGui::PopStyleColor(1);
				}
				ImGui::EndTable();
				free(opcode_strs);
				delete[] copy;

				if (force_scroll) {
					ImGui::SetScrollY(scroll - avail.y / 2);
					force_redraw();
				}
			} else {
				ImGui::Text("Click on a wave's PC to show its shader disassembly");
			}
			ImGui::EndChild();
		} else {
			ImGui::Text("No waves.");
		}
		return false;
	}

private:
	void send_waves_command(bool halt_waves, bool resume_waves, bool disable_gfxoff) {
		JSON_Value *req = json_value_init_object();
		json_object_set_string(json_object(req), "command", "waves");
		json_object_set_boolean(json_object(req), "halt_waves", halt_waves);
		json_object_set_boolean(json_object(req), "resume_waves", halt_waves && resume_waves);
		json_object_set_boolean(json_object(req), "disable_gfxoff", disable_gfxoff);
		json_object_set_string(json_object(req), "ring", asic->family >= FAMILY_NV ? "gfx_0.0.0" : "gfx");
		send_request(req);
	}
private:
	SyntaxHighlighter shader_syntax;
	JSON_Object *last_answer = NULL;
	JSON_Object *active_shader = NULL;
	uint64_t base_address;
	uint64_t pc;
	struct {
		bool *vgpr = NULL;
		int *view = NULL;
		int max_vgpr;
	} details;
	bool halt = true;
	bool resume = true;
	bool turn_off_gfxoff = true;
};

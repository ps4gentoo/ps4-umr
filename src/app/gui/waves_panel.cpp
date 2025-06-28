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
#include <string>
#include <unordered_map>

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

	~WavesPanel() {
		clear_waves_and_shaders();
	}

	void clear_waves_and_shaders() {
		for (const auto &wave : waves)
			json_value_free(json_object_get_wrapping_value(wave.wave));
		waves.clear();

		for (const auto &entry : shaders)
			json_value_free(json_object_get_wrapping_value(entry.second));
		shaders.clear();
	}

	std::string get_wave_id(JSON_Object *wave) {
		unsigned se = (unsigned int)json_object_get_number(wave, "se");
		unsigned sh = (unsigned int)json_object_get_number(wave, "sh");
		unsigned cu = (unsigned int)json_object_get_number(wave, "cu");
		unsigned wgp = (unsigned int)json_object_get_number(wave, "wgp");
		unsigned simd_id = (unsigned int)json_object_get_number(wave, "simd_id");
		unsigned wave_id = (unsigned int)json_object_get_number(wave, "wave_id");

		char id[128];
		if (asic->family < FAMILY_NV) {
			snprintf(id, sizeof(id), "se%u.sa%u.cu%u.simd%u.wave%u", se, sh, cu, simd_id, wave_id);
		} else {
			snprintf(id, sizeof(id), "se%u.sa%u.wgp%u.simd%u.wave%u", se, sh, wgp, simd_id, wave_id);
		}

		return id;
	}

	size_t find_wave_by_id(const std::string &id) {
		size_t i;
		for (i = 0; i < waves.size(); ++i) {
			if (waves[i].id == id)
				break;
		}
		return i;
	}

	void update_shaders(JSON_Object *shaders_dict) {
		int shaders_count = json_object_get_count(shaders_dict);
		for (int i = 0; i < shaders_count; ++i) {
			shaders.emplace(json_object_get_name(shaders_dict, i),
					json_object(json_value_deep_copy(json_object_get_value_at(shaders_dict, i))));
		}
	}

	void process_server_message(JSON_Object *response, void *raw_data, unsigned raw_data_size) {
		JSON_Value *error = json_object_get_value(response, "error");
		if (error)
			return;

		JSON_Object *request = json_object(json_object_get_value(response, "request"));
		JSON_Value *answer = json_object_get_value(response, "answer");
		const char *command = json_object_get_string(request, "command");

		if (strcmp(command, "waves") == 0) {
			active_shader_wave.clear();
			clear_waves_and_shaders();

			JSON_Array *waves_array = json_object_get_array(json_object(answer), "waves");
			int wave_count = json_array_get_count(waves_array);
			for (int i = 0; i < wave_count; ++i) {
				JSON_Object *wave = json_object(json_value_deep_copy(json_array_get_value(waves_array, i)));
				waves.emplace_back(get_wave_id(wave), wave);
			}

			JSON_Object *shaders_dict = json_object_get_object(json_object(answer), "shaders");
			update_shaders(shaders_dict);
		} else if (strcmp(command, "singlestep") == 0) {
			JSON_Object *wave = json_object(json_value_deep_copy(json_object_get_value(json_object(answer), "wave")));
			std::string id = get_wave_id(wave ? wave : request);
			size_t i = find_wave_by_id(id);
			if (i < waves.size()) {
				json_value_free(json_object_get_wrapping_value(waves[i].wave));
				if (wave) {
					waves[i].wave = wave;
				} else {
					waves.erase(waves.begin() + i);
				}
			} else {
				if (wave)
					waves.emplace_back(id, wave);
			}

			JSON_Object *shaders_dict = json_object_get_object(json_object(answer), "shaders");
			update_shaders(shaders_dict);
		} else {
			return; // should be handled by a different panel
		}
	}

	bool display(float dt, const ImVec2& avail, bool can_send_request) {
		ImGui::Checkbox("Disable gfxoff", &turn_off_gfxoff);
		ImGui::SameLine();
		ImGui::Checkbox("Resume waves", &resume);
		ImGui::SameLine();
		ImGui::BeginDisabled(!can_send_request);
		if (ImGui::Button("Query")) {
			send_waves_command(resume, turn_off_gfxoff);
		}
		ImGui::EndDisabled();

		ImGui::Separator();
		if (!waves.empty()) {
			ImGui::BeginChild("Waves", ImVec2(avail.x / 2, 0), false, ImGuiWindowFlags_NoTitleBar);
			bool force_scroll = false;
			for (size_t i = 0; i < waves.size(); ++i) {
				JSON_Object *wave = waves[i].wave;

				int active_threads = -1;
				uint64_t exec = 0;
				JSON_Array *threads = json_object_get_array(wave, "threads");
				if (threads) {
					active_threads = 0;
					int s = json_array_get_count(threads);
					for (int i = 0; i < s; i++) {
						bool active = json_array_get_boolean(threads, i) == 1;
						active_threads += active ? 1 : 0;
						if (active)
							exec |= (uint64_t)1 << i;
					}
				}
				const char *shader_address_str = json_object_get_string(wave, "shader");

				char label[256];
				if (active_threads < 0)
					sprintf(label, "Wave %s", waves[i].id.c_str());
				else if (shader_address_str)
					sprintf(label, "Wave %s (#dbde79%d threads, valid PC)", waves[i].id.c_str(), active_threads);
				else
					sprintf(label, "Wave %s (#dbde79%d threads)", waves[i].id.c_str(), active_threads);

				ImGui::PushID(i);
				if (ImGui::TreeNode(waves[i].id.c_str(), "%s", label)) {
					ImGui::NextColumn();
					ImGui::Text("PC: #b589000x%" PRIx64, (uint64_t)json_object_get_number(wave, "PC"));
					if (shader_address_str) {
						ImGui::SameLine();
						if (ImGui::Button("View Shader")) {
							active_shader_wave = waves[i].id;
							force_scroll = true;
						}
						if (asic->family >= FAMILY_NV) {
							ImGui::SameLine();
							ImGui::BeginDisabled(!can_send_request);
							if (ImGui::Button("Single step")) {
								active_shader_wave = waves[i].id;
								send_singlestep_command(waves[i].wave);
							}
							ImGui::EndDisabled();
						}
					} else {
					}
					ImGui::NextColumn();
					if (ImGui::TreeNodeEx("Registers")) {
						JSON_Object *registers = json_object(json_object_get_value(wave, "registers"));
						size_t n = json_object_get_count(registers);
						for (size_t j = 0; j < n; j++) {
							char label[256];
							JSON_Object *reg = json_object(json_object_get_value_at(registers, j));
							sprintf(label, "%s: #b589000x%08x",
								json_object_get_name(registers, j), (unsigned int)json_object_get_number(reg, "raw"));

							if (ImGui::TreeNodeEx(label)) {
								ImGui::BeginTable(json_object_get_name(registers, j), 2,
												  ImGuiTableFlags_RowBg);
								for (size_t k = 1; k < json_object_get_count(reg); k++) {
									ImGui::TableNextRow();
									ImGui::TableSetColumnIndex(0);
									ImGui::TextUnformatted(json_object_get_name(reg, k));
									ImGui::TableSetColumnIndex(1);
									ImGui::Text("#dbde790x%x", (unsigned int)json_number(json_object_get_value_at(reg, k)));
								}
								ImGui::EndTable();
								ImGui::TreePop();
							}
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
								uint32_t aaa = (uint32_t)json_number(v);
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
						static const char *formats_active[] = { "#6c71c4%d", "#6c71c4%u", "#6c71c4%08x", "#6c71c4%f" };
						static const char *formats_inactive[] = { "#818181%d", "#818181%u", "#818181%08x", "#818181%f" };
						JSON_Array *vgpr = json_object_get_array(wave, "vgpr");
						if (vgpr && ImGui::TreeNodeEx("#6c71c4VGPRs")) {
							int s = json_array_get_count(vgpr);

							char label[128];
							for (int vg = 0; vg < s; vg++) {
								ImGui::PushID(vg);
								sprintf(label, "v%2d", vg);
								if (ImGui::TreeNodeEx(label)) {
									ImGui::BeginTable("vgprvalues", 4, ImGuiTableFlags_Borders);

									int *mode = &waves[i].vgpr_view[vg];
									ImGui::TableNextRow();
									ImGui::TableSetColumnIndex(0);
									ImGui::RadioButton("as int", mode, 0);
									ImGui::TableSetColumnIndex(1);
									ImGui::RadioButton("as uint", mode, 1);
									ImGui::TableSetColumnIndex(2);
									ImGui::RadioButton("as hex", mode, 2);
									ImGui::TableSetColumnIndex(3);
									ImGui::RadioButton("as float", mode, 3);

									JSON_Array *vgp = json_array_get_array(vgpr, vg);
									int num_thread = json_array_get_count(vgp);

									for (int t = 0; t < num_thread; t++) {
										if (t % 4 == 0)
											ImGui::TableNextRow();
										ImGui::TableSetColumnIndex(t % 4);

										const char **formats = (exec >> t) & 1 ? formats_active : formats_inactive;

										JSON_Value *v = json_array_get_value(vgp, t);
										ImGui::PushID(v);
										int aaa = (int)json_number(v);
										if (*mode == 3) {
											float f = reinterpret_cast<float&>(aaa);
											ImGui::Text(formats[3], f);
										} else {
											ImGui::Text(formats[*mode], aaa);
										}
										if (ImGui::IsItemHovered()) {
											ImGui::BeginTooltip();
											ImGui::Text("thread %d", t);
											ImGui::EndTooltip();
										}
										ImGui::PopID();
									}
									ImGui::EndTable();
									ImGui::TreePop();
								}
								ImGui::PopID();
							}
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
			display_active_shader(dt, avail, force_scroll);
			ImGui::EndChild();
		} else {
			ImGui::Text("No waves.");
		}
		return false;
	}

	void display_active_shader(float dt, const ImVec2& avail, bool force_scroll) {
		if (active_shader_wave.empty()) {
			ImGui::Text("Click on a wave's PC to show its shader disassembly");
			return;
		}

		size_t i = find_wave_by_id(active_shader_wave);
		if (i >= waves.size()) {
			ImGui::Text("Selected wave has disappeared");
			return;
		}

		JSON_Object *wave = waves[i].wave;
		const char *shader_address_str = json_object_get_string(wave, "shader");
		auto shader_it = shaders.find(shader_address_str);
		if (shader_it == shaders.end()) {
			ImGui::Text("Error: Shader for selected wave is unavailable");
			return;
		}

		JSON_Object *shader = shader_it->second;
		uint64_t pc = json_object_get_number(wave, "PC");

		int scroll = 0;
		JSON_Array *op = json_object_get_array(shader, "opcodes");
		uint32_t *copy = new uint32_t[json_array_get_count(op)];
		for (size_t j = 0; j < json_array_get_count(op); j++)
			copy[j] = (uint32_t)json_array_get_number(op, j);

		uint64_t base_address = json_object_get_number(shader, "address");
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
	}

private:
	void send_waves_command(bool resume_waves, bool disable_gfxoff) {
		JSON_Value *req = json_value_init_object();
		json_object_set_string(json_object(req), "command", "waves");
		json_object_set_boolean(json_object(req), "resume_waves", resume_waves);
		json_object_set_boolean(json_object(req), "disable_gfxoff", disable_gfxoff);
		json_object_set_string(json_object(req), "ring", asic->family >= FAMILY_NV ? "gfx_0.0.0" : "gfx");
		send_request(req);
	}

	void send_singlestep_command(JSON_Object *wave) {
		assert(asic->family >= FAMILY_NV);
		JSON_Value *req = json_value_init_object();
		json_object_set_string(json_object(req), "command", "singlestep");
		json_object_set_string(json_object(req), "ring", asic->family >= FAMILY_NV ? "gfx_0.0.0" : "gfx");
		json_object_set_number(json_object(req), "se", json_object_get_number(wave, "se"));
		json_object_set_number(json_object(req), "sh", json_object_get_number(wave, "sh"));
		json_object_set_number(json_object(req), "wgp", json_object_get_number(wave, "wgp"));
		json_object_set_number(json_object(req), "simd_id", json_object_get_number(wave, "simd_id"));
		json_object_set_number(json_object(req), "wave_id", json_object_get_number(wave, "wave_id"));
		send_request(req);
	}

private:
	struct Wave {
		std::string id; // "seN.saN.etc"
		JSON_Object *wave;
		int vgpr_view[512] = {};

		Wave(std::string id, JSON_Object *wave) : id(id), wave(wave) {}
	};

	SyntaxHighlighter shader_syntax;
	std::vector<Wave> waves;
	std::unordered_map<std::string, JSON_Object *> shaders;
	std::string active_shader_wave;
	bool resume = true;
	bool turn_off_gfxoff = true;
};

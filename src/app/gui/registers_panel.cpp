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

#include <ctype.h>
#include <SDL.h>

struct PinnedRegister {
	PinnedRegister(struct umr_ip_block *_blk, struct umr_reg *_reg) : blk(_blk), reg(_reg) {
		value_is_valid = false;
		value_is_dirty = false;
	}
	struct umr_ip_block *blk;
	struct umr_reg *reg;
	bool value_is_valid;
	bool value_is_dirty;
};

class RegistersPanel : public Panel {
public:
	RegistersPanel(struct umr_asic *asic) : Panel(asic), autorefresh(false), autorefresh_hz(5), elapsed_since_last_refresh(0) {}

	~RegistersPanel() {}

	void process_server_message(JSON_Object *request, JSON_Value *answer) {
		const char *command = json_object_get_string(request, "command");

		if (strcmp(command, "read") && strcmp(command, "write"))
			return;

		const char *blk = json_object_get_string(request, "block");
		const char *reg = json_object_get_string(request, "register");

		PinnedRegister *pinned = NULL;
		for (int i = 0; i < pinned_registers.size() && !pinned; i++) {
			PinnedRegister &p = pinned_registers[i];
			if (!strcmp(p.blk->ipname, blk) && !strcmp(p.reg->regname, reg))
				pinned = &p;
		}
		if (!pinned) {
			/* This can happen in replay mode: pin the register */
			struct umr_reg *r = umr_find_reg_data_by_ip(asic, blk, reg);
			struct umr_ip_block *b = NULL;
			umr_find_reg_by_addr(asic, r->addr, &b);
			pinned_registers.push_back(PinnedRegister(b, r));
			pinned = &pinned_registers.back();
		}

		pinned->reg->value = json_object_get_number(json_object(answer), "value");
		pinned->value_is_valid = true;
		pinned->value_is_dirty = false;
	}

	bool display(float dt, const ImVec2& avail, bool can_send_request) {
		const float _8digitsize = ImGui::CalcTextSize("0x00000000").x + ImGui::GetStyle().FramePadding.x * 2;

		/* Split pane */
		ImGui::BeginChild("Registers list", ImVec2(avail.x / 3, 0), false,
							ImGuiWindowFlags_NoTitleBar);
		char details[128];
		for (int i = 0; i < (int) asic->no_blocks; i++) {
			unsigned matching = 0;
			struct umr_ip_block *b = asic->blocks[i];
			if (filter[0] != '\0' || field_filter[0] != '\0') {
				for (int j = 0; j < b->no_regs; j++) {
					if (filter[0] != '\0' && fuzzy_match_simple(filter, b->regs[j].regname)) {
						matching++;
					} else if (field_filter[0] != '\0') {
						for (int k = 0; k < b->regs[j].no_bits; k++) {
							if (b->regs[j].bits[k].regname &&
								  fuzzy_match_simple(field_filter, b->regs[j].bits[k].regname)) {
								matching++;
								break;
							}
						}
					}
				}
				if (matching == 0)
					continue;
				sprintf(details, "%d/%d registers", matching, b->no_regs);
			} else {
				sprintf(details, "%d registers", b->no_regs);
			}
			if (ImGui::TreeNodeEx(b->ipname, (matching && matching < 10) ? ImGuiTreeNodeFlags_Leaf : 0, "%12s (%s)", b->ipname, details)) {
				bool at_least_one = matching > 0;
				for (int j = 0; j < b->no_regs; j++) {
					bool pinned = false;
					for (int k = 0; k < (int) pinned_registers.size() && !pinned; k++)
						pinned = pinned_registers[k].reg == &b->regs[j];

					if (filter[0] != '\0' && !fuzzy_match_simple(filter, b->regs[j].regname))
						continue;

					if (field_filter[0] != '\0') {
						bool show = false;
						for (int k = 0; k < b->regs[j].no_bits; k++) {
							if (b->regs[j].bits[k].regname &&
								  fuzzy_match_simple(field_filter, b->regs[j].bits[k].regname)) {
								show = true;
								break;
							}
						}

						if (!show)
							continue;
					}
					at_least_one = true;
					if (pinned) {
						ImGui::TextUnformatted(b->regs[j].regname);
					} else if (ImGui::Button(b->regs[j].regname)) {
						pinned_registers.push_back(PinnedRegister(b, &b->regs[j]));
						send_read_reg_command(&pinned_registers.back());
					}
				}
				if (!at_least_one) {
					ImGui::Text("No matching registers");
				}
				ImGui::TreePop();
			}
		}
		ImGui::EndChild();
		ImGui::SameLine();

		ImGui::BeginChild("Filters:", ImVec2(2 * avail.x / 3, 0), false, ImGuiWindowFlags_NoTitleBar);
		ImGui::NewLine();
		ImGui::Text("Filters");
		if (kb_shortcut(SDLK_f))
			ImGui::SetKeyboardFocusHere();
		ImGui::InputText("Register name	  ", filter, sizeof(filter));
		ImGui::SameLine();
		if (ImGui::Button("Clear") || (kb_shortcut(SDLK_BACKSPACE)))
			filter[0] = '\0';
		ImGui::InputText("Register field name", field_filter, sizeof(field_filter));
		ImGui::SameLine();
		ImGui::PushID("Field");
		if (ImGui::Button("Clear") || (ImGui::GetIO().KeyCtrl && ImGui::IsKeyReleased(SDLK_BACKSPACE)))
			field_filter[0] = '\0';
		ImGui::PopID();
		ImGui::Separator();
		ImGui::Text("Register watchlist:");
		ImGui::NewLine();
		bool auto_read_reg = false;
		if (pinned_registers.empty()) {
			ImGui::Indent();
			ImGui::Text("(Click on a register name to add it to the watchlist.)");
			ImGui::Unindent();
			ImGui::NewLine();
		} else {
			if (dt >= 0 && ImGui::Checkbox("Auto-refresh", &autorefresh)) {
				elapsed_since_last_refresh = 0;
			}
			if (autorefresh) {
				ImGui::SameLine();
				elapsed_since_last_refresh += dt;
				ImGui::SetNextItemWidth(_8digitsize);
				ImGui::InputInt("Frequency", &autorefresh_hz);
				ImGui::SameLine();
				ImGui::ProgressBar(elapsed_since_last_refresh / autorefresh_hz, ImVec2(), "");
				if (elapsed_since_last_refresh >= autorefresh_hz) {
					elapsed_since_last_refresh = 0;
					auto_read_reg = true;
				}
			}
		}

		ImGui::BeginTable("register", 7, ImGuiTableFlags_SizingStretchProp |
										 ImGuiTableFlags_Borders);
		ImGui::TableSetupColumn("Pin", ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize(" R ").x);
		ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize(" 0x00000000 ").x);
		ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, 120);
		ImGui::TableSetupColumn("Bitfield");
		ImGui::TableSetupColumn("R", ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize(" R ").x);
		ImGui::TableSetupColumn("W", ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize(" R ").x);
		ImGui::TableHeadersRow();
		for (int i = 0; i < (int) pinned_registers.size(); ++i) {
			bool pin = true;
			PinnedRegister& pinned = pinned_registers[i];
			ImGui::PushID(pinned.reg->regname);

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			if (ImGui::Checkbox(" ", &pin)) {
				pinned_registers.erase(pinned_registers.begin() + i);
				i--;
			}
			ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(pinned.reg->regname);
			ImGui::TableSetColumnIndex(2); ImGui::Text("0x%08lx", pinned.reg->addr);
			ImGui::TableSetColumnIndex(3);
			{
				char tmp[512];
				bool was_dirty = pinned.value_is_dirty;
				if (was_dirty)
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0, 0, 0, 1));
				sprintf(tmp, "0x%08lx", pinned.reg->value);
				if (ImGui::InputText("", tmp, 16, ImGuiInputTextFlags_CharsHexadecimal)) {
					unsigned value;
					if (sscanf(tmp, "0x%x", &value) == 1) {
						pinned.reg->value = value;
						pinned.value_is_dirty = true;
						force_redraw();
					}
				}
				if (was_dirty)
					ImGui::PopStyleColor();
			}
			ImGui::TableSetColumnIndex(4);
			ImGui::BeginTable("bitfield", 2);
			ImGui::TableSetupColumn("Field");
			ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize(" 0x00000000 ").x);
			for (int j = 0; j < pinned.reg->no_bits; j++) {
				ImGui::TableNextRow();
				struct umr_bitfield *bit = &pinned.reg->bits[j];
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("#b58900%s [%d:%d]", bit->regname, bit->stop, bit->start);

				unsigned mask = 0;
				for (unsigned k = bit->start; k <= bit->stop; k++)
					mask |= 1u << k;
				unsigned v = (pinned.reg->value & mask) >> bit->start;
				ImGui::PushID(bit->regname);
				ImGui::SetNextItemWidth(_8digitsize);
				ImGui::TableSetColumnIndex(1);
				char tmp[16];
				sprintf(tmp, "0x%x", v);
				if (ImGui::InputText("", tmp, 10, ImGuiInputTextFlags_CharsHexadecimal)) {
					if (sscanf(tmp, "0x%x", &v) == 1) {
						v = v & mask;
						pinned.reg->value = (pinned.reg->value & ~mask) | (v << (unsigned)bit->start);
						pinned.value_is_dirty = true;
						force_redraw();
					}
				}
				ImGui::PopID();
			}
			ImGui::EndTable();

			bool read = false;
			ImGui::TableSetColumnIndex(5);
			ImGui::BeginDisabled(!can_send_request);
			if (ImGui::ArrowButton("read", ImGuiDir_Down) || auto_read_reg) {
				send_read_reg_command(&pinned);
			}
			if (pinned.value_is_dirty) {
				ImGui::TableSetColumnIndex(6);
				if (ImGui::ArrowButton("write", ImGuiDir_Up)) {
					send_write_reg_command(&pinned, pinned.reg->value);
				}
			}
			ImGui::EndDisabled();
			ImGui::PopID();
		}
		ImGui::EndTable();
		ImGui::EndChild();

		return autorefresh;
	}

private:
	void send_read_reg_command(PinnedRegister *pinned) {
		JSON_Value *req = json_value_init_object();
		json_object_set_string(json_object(req), "command", "read");
		json_object_set_string(json_object(req), "block", pinned->blk->ipname);
		json_object_set_string(json_object(req), "register", pinned->reg->regname);
		send_request(req);
	}

	void send_write_reg_command(PinnedRegister *pinned, unsigned value) {
		JSON_Value *req = json_value_init_object();
		json_object_set_string(json_object(req), "command", "write");
		json_object_set_string(json_object(req), "block", pinned->blk->ipname);
		json_object_set_string(json_object(req), "register", pinned->reg->regname);
		json_object_set_number(json_object(req), "value", value);
		send_request(req);
	}

	/* From fts_fuzzy_match */
	bool fuzzy_match_simple(char const * pattern, char const * str) {
		while (*pattern != '\0' && *str != '\0')  {
			if (tolower(*pattern) == tolower(*str))
				++pattern;
			++str;
		}
		return *pattern == '\0' ? true : false;
	}

private:
	std::vector<PinnedRegister> pinned_registers;
	bool autorefresh;
	float elapsed_since_last_refresh;
	int autorefresh_hz;
	char filter[32] = {};
	char field_filter[32] = {};
};

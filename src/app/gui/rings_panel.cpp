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

static char ring_decode_buffer[8196];
static int ring_decode_buffer_offset = 0;
static int ring_decode_fn(const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	ring_decode_buffer_offset += vsprintf(&ring_decode_buffer[ring_decode_buffer_offset], fmt, ap);
	va_end(ap);
	return 0;
}

static bool get_ring_name(void *data, int idx, const char **out) {
	JSON_Array *rings = (JSON_Array *)data;
	if (idx >= 0 && idx < json_array_get_count(rings)) {
		*out = json_array_get_string(rings, idx);
		return true;
	}
	return false;
}

class RingsPanel : public Panel {
public:
	RingsPanel(struct umr_asic *asic) : Panel(asic), last_answer(NULL) {
		/* PKT3_ */
		/* PKT3_DRAW_ | PKT3_DISPATCH */
		ib_syntax.add_definition("(PKT3_DRAW[A-Z_0-9]*|PKT3_DISPATCH[A-Z_0-9]*)", { "#a42721" });
		/* number */
		ib_syntax.add_definition("(0x[a-z0-9]*)", { "#dbde79" });
		/* keyword */
		ib_syntax.add_definition("(PKT[0-3],|OPCODE)", { "#8f979c" });

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

		current_item = - 1;
		halt = true;
		rptr_wptr = true;
	}

	~RingsPanel() {
		if (last_answer)
			json_value_free(json_object_get_wrapping_value(last_answer));
	}

	void process_server_message(JSON_Object *request, JSON_Value *answer) {
		const char *command = json_object_get_string(request, "command");

		if (!strcmp(command, "ring")) {
			if (last_answer)
				json_value_free(json_object_get_wrapping_value(last_answer));
			last_answer = json_object(json_value_deep_copy(answer));
		}
	}

	bool display(float dt, const ImVec2& avail, bool can_send_request) {
		const float _8digitsize = ImGui::CalcTextSize("0x00000000").x + ImGui::GetStyle().FramePadding.x * 2;

		JSON_Array *rings = json_object_get_array(info, "rings");
		if (current_item < 0)
			current_item = json_array_get_count(rings) - 1;

		ImGui::Checkbox("Halt waves", &halt);
		ImGui::SameLine();
		ImGui::TextUnformatted("Select ring:");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(_8digitsize * 4);
		ImGui::PushID("selectring");
		ImGui::Combo("", &current_item, get_ring_name, rings, json_array_get_count(rings));
		ImGui::PopID();
		ImGui::SameLine();
		ImGui::BeginDisabled(!can_send_request);
		ImGui::Checkbox("Limit to rptr/wptr", &rptr_wptr);
		ImGui::SameLine();
		ImGui::BeginDisabled(dt < 0);
		if (ImGui::Button("Read")) {
			const char *ring_name;
			get_ring_name(rings, current_item, &ring_name);
			send_ring_command(&ring_name[strlen("amdgpu_ring_")], halt, rptr_wptr);
		}
		ImGui::EndDisabled();
		ImGui::EndDisabled();
		ImGui::Separator();
		if (last_answer) {
			JSON_Array *ibs = json_object_get_array(last_answer, "ibs");
			JSON_Array *shaders = json_object_get_array(last_answer, "shaders");
			JSON_Array *ring = json_object_get_array(json_object(json_object_get_value(last_answer, "ring")), "opcodes");
			int decoder = json_object_get_number(last_answer, "decoder");

			int rptr = json_object_get_number(last_answer, "read_ptr");
			int wptr = json_object_get_number(last_answer, "write_ptr");
			int drv_wptr = json_object_get_number(last_answer, "driver_write_ptr");

			ImGui::BeginTabBar("ringtabs", ImGuiTabBarFlags_FittingPolicyScroll | ImGuiTabBarFlags_TabListPopupButton);

			uint32_t highlight_lo_ib = 0;
			if (ImGui::BeginTabItem("Ring Content")) {
				ImGui::Text("Last signaled fence: #dbde790x%08x",
					(uint32_t)json_object_get_number(last_answer, "last_signaled_fence"));
				ImGui::BeginChild("ringtabs scroll");
				highlight_lo_ib = display_ib(ring, decoder, 0, rptr, wptr, drv_wptr);
				ImGui::EndChild();
				ImGui::EndTabItem();
			}
			for (int i = 0; i < json_array_get_count(ibs); i++) {
				JSON_Object *ib = json_object(json_array_get_value(ibs, i));
				uint64_t base = (uint64_t) json_object_get_number(ib, "address");
				int high = (((uint32_t)base) == highlight_lo_ib);

				if (high)
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0, 0.5, 0.5, 1));

				char tmp[128];
				sprintf(tmp, "IB @ %" PRIx64, base);
				if (ImGui::BeginTabItem(tmp)) {
					if (high)
						ImGui::PopStyleColor();
					ImGui::BeginChild(tmp);
					highlight_lo_ib = display_ib(json_object_get_array(ib, "opcodes"), decoder, base);
					ImGui::EndChild();
					ImGui::EndTabItem();
				} else if (high) {
					ImGui::PopStyleColor();
				}
			}

			for (int i = 0; i < json_array_get_count(shaders); i++) {
				JSON_Object *shader = json_object(json_array_get_value(shaders, i));
				uint64_t base = (uint64_t) json_object_get_number(shader, "address");
				char tmp[128];
				sprintf(tmp, "shader @ %" PRIx64, base);
				ImGui::PushID(i);
				if (ImGui::BeginTabItem(tmp)) {
					JSON_Array *op = json_object_get_array(shader, "opcodes");
					uint32_t *copy = new uint32_t[json_array_get_count(op)];
					for (size_t j = 0; j < json_array_get_count(op); j++)
						copy[j] = (uint32_t)json_array_get_number(op, j);

					char **opcode_strs = NULL;
					umr_shader_disasm(asic, (uint8_t *)copy, json_array_get_count(op) * 4, base, &opcode_strs);

					sprintf(tmp, "0x%" PRIx64, base);

					ImGui::BeginChild(tmp);
					ImGui::BeginTable("shader", 3, ImGuiTableFlags_Borders);
					ImGui::TableSetupColumn(tmp, ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize(" 0x0000000000 ").x);
					ImGui::TableSetupColumn("Raw Value", ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize("0x00000000  ").x);
					ImGui::TableSetupColumn("Disassembly");
					ImGui::TableHeadersRow();
					for (size_t j = 0; j < json_array_get_count(op); j++) {
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::Text("+ 0x%lx", j * 4);
						if (ImGui::IsItemHovered()) {
							ImGui::BeginTooltip();
							ImGui::Text("0x%" PRIx64, base + j * 4);
							ImGui::EndTooltip();
						}
						ImGui::TableSetColumnIndex(1);
						ImGui::Text("0x%08x", (uint32_t)json_array_get_number(op, j));
						ImGui::TableSetColumnIndex(2);
						ImGui::Text("%s", shader_syntax.transform(opcode_strs[j]));
						free(opcode_strs[j]);
					}
					ImGui::EndTable();
					free(opcode_strs);
					delete[] copy;
					ImGui::EndChild();
					ImGui::EndTabItem();
				}
				ImGui::PopID();
			}

			ImGui::EndTabBar();
		}
		return false;
	}
private:
	void send_ring_command(const char *ring_name, bool halt_ring, bool rptr_wptr) {
		JSON_Value *req = json_value_init_object();
		json_object_set_string(json_object(req), "command", "ring");
		json_object_set_string(json_object(req), "ring", ring_name);
		json_object_set_boolean(json_object(req), "halt_waves", halt_ring);
		json_object_set_boolean(json_object(req), "rptr_wptr", rptr_wptr);
		send_request(req);
	}

	uint32_t display_ib(JSON_Array *raw, int decoder_type, uint64_t base, int rptr = -1, int wptr = -1, int drv_wptr = -1) {
		uint32_t addr_lo_ib = 0;

		ImGuiListClipper clipper;
		clipper.Begin(json_array_get_count(raw));
		ImGui::BeginTable("dis", rptr >= 0 ? 5 : 4, ImGuiTableFlags_BordersV);
		ImGui::TableSetupColumn(rptr >= 0 ? "Index" : "Address", ImGuiTableColumnFlags_WidthFixed,
			rptr >= 0 ? ImGui::CalcTextSize(" Index ").x : ImGui::CalcTextSize(" 0x0000000000000000 + 0x0000").x);
		ImGui::TableSetupColumn("Raw Value", ImGuiTableColumnFlags_WidthFixed,
			ImGui::CalcTextSize(" 00000000 ").x);
		if (rptr >= 0)
			ImGui::TableSetupColumn("Pointers", ImGuiTableColumnFlags_WidthFixed,
				ImGui::CalcTextSize("Pointers").x);
		ImGui::TableSetupColumn("Opcode", ImGuiTableColumnFlags_WidthFixed,
				ImGui::CalcTextSize("PKT3_XXXXXXXXXXXXXXXXXXXXXXX").x);
		ImGui::TableSetupColumn("Disassembly");
		ImGui::TableHeadersRow();

		int draw_dispatch_count = 0;
		while (clipper.Step()) {
			struct umr_ring_decoder decoder;
			memset(&decoder, 0, sizeof decoder);
			decoder.pm = decoder_type;
			decoder.sdma.cur_opcode = 0xFFFFFFFF;
			decoder.pm4.cur_opcode = 0xFFFFFFFF;
			asic->options.no_follow_ib = 1;
			asic->options.use_colour = 0;
			asic->options.bitfields = 0;

			bool is_new_pkt;
			int pkt_count = 1;
			bool current_pkt3_is_draw = false;
			bool previous_pkt3_was_draw = false;
			for (int i = 0 ; i < clipper.DisplayEnd; i++) {
				int col = 0;
				uint32_t raw_value = json_array_get_number(raw, i);

				ring_decode_buffer_offset = 0;
				umr_print_decode(asic, &decoder, raw_value, ring_decode_fn);

				char *line = ring_decode_buffer;

				/* New PKT3 packet */
				is_new_pkt =
					(strncmp(line, "PKT3,", 5) == 0 && strstr(line, "OPCODE") != NULL) ||
					strncmp(line, "OPCODE:", strlen("OPCODE:")) == 0;
				if (is_new_pkt) {
					if (decoder_type == 4) {
						line = &line[strlen("PKT3,")];
						previous_pkt3_was_draw = current_pkt3_is_draw;
						current_pkt3_is_draw =
							strstr(line, "PKT3_DRAW_") != NULL ||
							strstr(line, "PKT3_DISPATCH") != NULL;
						if (previous_pkt3_was_draw)
							draw_dispatch_count++;
					}

					pkt_count++;
				}

				if (i < clipper.DisplayStart)
					continue;

				if (is_new_pkt && previous_pkt3_was_draw) {
					ImGui::TableHeadersRow();
				}
				ImGui::TableNextRow();
				ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
					ImGui::GetColorU32((pkt_count % 2) + ImGuiCol_TableRowBg));

				bool indent = false;

				char *ind = strstr(line, "---+");
				if (ind) {
					indent = true;
					line = &ind[5];
				}
				if (strncmp(line, "PKT3 ", strlen("PKT3 ")) == 0)
					line = &line[5];

				ImGui::TableSetColumnIndex(col++);
				if (rptr >= 0) {
					ImGui::Text("%04d", i);
				} else {
					ImGui::Text("0x%" PRIx64 " + #0083d80x%x", base, 4 * i);
				}

				ImGui::TableSetColumnIndex(col++);
				ImGui::Text("%08x", raw_value);

				if (rptr >= 0) {
					ImGui::TableSetColumnIndex(col++);
					if (i == rptr) {
						ImGui::TextUnformatted("#d33682R");
					}
					if (i == wptr) {
						ImGui::SameLine();
						ImGui::TextUnformatted("#b58900W");
					}
					if (i == drv_wptr) {
						ImGui::SameLine();
						ImGui::TextUnformatted("#586e75DW");;
					}
				}

				char *colored = (char*) ib_syntax.transform(line);

				ImGui::TableSetColumnIndex(col++);
				char *opcode_start = NULL;
				char *opcode_end = NULL;
				if (is_new_pkt) {
					opcode_start = strstr(colored, "OPCODE");
					if (opcode_start) {
						opcode_start = strchr(opcode_start, '[');
						if (opcode_start) {
							opcode_end = strchr(opcode_start, ']');
							ImGui::TextUnformatted(opcode_start + 1, opcode_end);
							opcode_end++;
							/* Advance opcode_end to the first alpha-numerical char */
							while (*opcode_end && !isalnum(*opcode_end))
								opcode_end++;
						}
					}
					if (decoder_type == 3) {
						/* sdma packet starts by the opcode, so display the rest of the line. */
						colored = opcode_end;
						opcode_start = NULL;
					}
				}

				ImGui::TableSetColumnIndex(col++);

				if (indent) {
					ImGui::Indent();
					ImGui::Indent();
				}

				char *lnk = strstr(colored, "IB_BASE_LO: ");
				if (lnk) {
					lnk += strlen("IB_BASE_LO: ");
					ImVec2 start = ImGui::CalcTextSize(colored, lnk);
					ImVec2 end = ImGui::CalcTextSize(lnk, &lnk[17]);
					ImVec2 cursor = ImGui::GetCursorScreenPos();
					cursor.x += start.x;
					end.x += cursor.x;
					end.y += cursor.y;
					if (ImGui::IsMouseHoveringRect(cursor, end)) {
						sscanf(lnk + 7, "0x%08x", &addr_lo_ib);
						memcpy(lnk, "#ff8080", 7);
					}
				}

				ImGui::TextUnformatted(colored, opcode_start);
				if (indent) {
					ImGui::Unindent();
					ImGui::Unindent();
				}
			}
		}

		ImGui::EndTable();
		clipper.End();

		return addr_lo_ib;
	}
private:
	JSON_Object *last_answer;
	SyntaxHighlighter ib_syntax;
	SyntaxHighlighter shader_syntax;
	int current_item;
	bool halt;
	bool rptr_wptr;
};

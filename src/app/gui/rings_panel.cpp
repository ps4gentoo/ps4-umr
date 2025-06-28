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

static const char * get_ring_name(JSON_Array *rings, int idx) {
	assert(idx >= 0 && idx < json_array_get_count(rings));
	return json_array_get_string(rings, idx) + strlen("amdgpu_ring_");
}

static void _start_ib(struct umr_stream_decode_ui *, uint64_t, uint32_t, uint64_t, uint32_t, uint32_t, int) {
	/* TODO: mouse over link? */
}

static void _add_shader(struct umr_stream_decode_ui *ui, struct umr_asic *asic, uint64_t ib_addr, uint32_t ib_vmid, struct umr_shaders_pgm *shader) {
	/* TODO: mouse over link? */
}

static void _add_data(struct umr_stream_decode_ui *ui, struct umr_asic *asic, uint64_t ib_addr, uint32_t ib_vmid, uint64_t buf_addr, uint32_t buf_vmid, enum UMR_DATABLOCK_ENUM type, uint64_t etype) {
	/* no-op */
}

static void _ring_unhandled(struct umr_stream_decode_ui *ui, struct umr_asic *asic, uint64_t ib_addr, uint32_t ib_vmid, void *str, enum umr_ring_type rt) {
	/* Ignore */
}

static void _done(struct umr_stream_decode_ui *ui) {
	/* no-op */
}

static void _start_opcode(struct umr_stream_decode_ui *ui, uint64_t ib_addr,
						 uint32_t ib_vmid, int pkttype,
						 uint32_t opcode, uint32_t subop, uint32_t nwords,
						 const char *opcode_name, uint32_t header, const uint32_t* raw_data) {
	ImGui::TableNextRow();
	ImGui::TableSetColumnIndex(0);
	ImGui::Text("#0083d80x%" PRIx64, ib_addr);
	ImGui::TableSetColumnIndex(1);
	ImGui::Text("%08x", header);
	ImGui::TableSetColumnIndex(2);
	if (ImGui::TreeNode((void*)ib_addr, "#8f979c%s", opcode_name)) {
		*((bool*)ui->data) = true;
		ImGui::TreePop();
	} else {
		*((bool*)ui->data) = false;
	}
}

static void _add_field(struct umr_stream_decode_ui *ui, uint64_t ib_addr,
					  uint32_t ib_vmid, const char *field_name,
					  uint64_t value, char *str, int ideal_radix, int field_size)
{
	if (*((bool*)ui->data) == false)
		return;

	ImGui::TableNextRow();
	ImGui::TableSetColumnIndex(0);
	ImGui::Text("#0083d80x%" PRIx64, ib_addr);
	ImGui::TableSetColumnIndex(2);
	ImGui::Indent();

	if (!strcmp(field_name, "REG") && ideal_radix == 16) {
		ImGui::Text("#d33682%s = #dbde790x%x", str, (unsigned int)value);
	} else {
		ImGui::Text("%s =", field_name);
		if (str) {
			ImGui::SameLine();
			ImGui::Text("[%s] ", str);
		}
		if (ideal_radix) {
			ImGui::SameLine();
			if (ideal_radix == 10)
				ImGui::Text("#dbde79%" PRIu64, value);
			else if (ideal_radix == 16)
				ImGui::Text("#dbde790x%" PRIx64, value);
		}
	}
	ImGui::Unindent();
}

class RingsPanel : public Panel {
public:
	RingsPanel(struct umr_asic *asic) : Panel(asic), last_answer(NULL), raw_data(NULL) {
		/* number */
		ib_syntax.add_definition("(0x[a-z0-9]*)", { "#dbde79" });

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
		if (last_answer) {
			json_value_free(json_object_get_wrapping_value(last_answer));
			free(raw_data);
		}
	}

	void process_server_message(JSON_Object *response, void *_raw_data, unsigned raw_data_size) {
		JSON_Value *error = json_object_get_value(response, "error");
		if (error)
			return;

		JSON_Object *request = json_object(json_object_get_value(response, "request"));
		JSON_Value *answer = json_object_get_value(response, "answer");
		const char *command = json_object_get_string(request, "command");

		if (!strcmp(command, "ring")) {
			if (last_answer)
				json_value_free(json_object_get_wrapping_value(last_answer));
			last_answer = json_object(json_value_deep_copy(answer));
			free(this->raw_data);
			this->raw_data = (uint32_t*)malloc(raw_data_size);
			memcpy(this->raw_data, _raw_data, raw_data_size);
		}
	}

	bool display(float dt, const ImVec2& avail, bool can_send_request) {
		const float padding = ImGui::GetStyle().FramePadding.x;
		const float _8digitsize = ImGui::CalcTextSize("0x00000000").x + padding * 2;

		JSON_Array *rings = json_object_get_array(info, "rings");
		if (current_item < 0)
			current_item = json_array_get_count(rings) - 1;

		ImGui::TextUnformatted("Select ring:");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(_8digitsize * 4);
		ImGui::PushID("selectring");

		float max_w = 0;
		for (size_t i = 0; i < json_array_get_count(rings); i++) {
			max_w = std::max(max_w, ImGui::CalcTextSize(get_ring_name(rings, i)).x);
		}
		ImGui::SetNextItemWidth(max_w + padding * 2 + ImGui::GetFrameHeight());
		if (ImGui::BeginCombo("", json_array_get_string(rings, current_item) + strlen("amdgpu_ring_"))) {
			for (size_t i = 0; i < json_array_get_count(rings); i++) {
				const char *ring_name = get_ring_name(rings, i);
				ImGui::BeginDisabled(strstr(ring_name, "jpeg"));
				if (ImGui::Selectable(ring_name, i == current_item)) {
					current_item = i;
				}
				ImGui::EndDisabled();
			}
			ImGui::EndCombo();
		}

		ImGui::PopID();
		ImGui::SameLine();

		ImGui::BeginDisabled(!can_send_request);
		if (ImGui::Button("  Read Ring  ")) {
			const char *ring_name = get_ring_name(rings, current_item);
			send_ring_command(ring_name, halt, rptr_wptr);
		}
		ImGui::SameLine();

		/* Right-align options. */
		float w = ImGui::CalcTextSize("Options:").x +
				  padding +
				  ImGui::CalcTextSize("Halt waves").x + ImGui::GetFrameHeight() + 2 * padding +
				  ImGui::CalcTextSize("Limit to rptr/wptr").x + ImGui::GetFrameHeight() + 2 * padding;

		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - w);
		ImGui::Text("Options:");
		ImGui::SameLine();
		ImGui::Checkbox("Halt waves", &halt);
		ImGui::SameLine();
		ImGui::Checkbox("Limit to rptr/wptr", &rptr_wptr);
		ImGui::EndDisabled();
		ImGui::Separator();
		if (last_answer) {
			JSON_Array *ibs = json_object_get_array(last_answer, "ibs");
			JSON_Array *shaders = json_object_get_array(last_answer, "shaders");
			JSON_Array *vcns = json_object_get_array(last_answer, "vcns");
			JSON_Object *ring = json_object(json_object_get_value(last_answer, "ring"));
			enum umr_ring_type type = (enum umr_ring_type)json_object_get_number(last_answer, "ring_type");

			int rptr = json_object_get_number(last_answer, "read_ptr");
			int wptr = json_object_get_number(last_answer, "write_ptr");
			int drv_wptr = json_object_get_number(last_answer, "driver_write_ptr");

			ImGui::BeginTabBar("ringtabs", ImGuiTabBarFlags_FittingPolicyScroll | ImGuiTabBarFlags_TabListPopupButton);

			uint32_t highlight_lo_ib = 0;
			if (ImGui::BeginTabItem("Ring Content")) {
				ImGui::Text("Last signaled fence: #dbde790x%08x",
					(uint32_t)json_object_get_number(last_answer, "last_signaled_fence"));
				ImGui::BeginChild("ringtabs scroll");
				highlight_lo_ib = display_ib(ring, type, 0, this->raw_data, rptr, wptr, drv_wptr);
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
					highlight_lo_ib = display_ib(ib, type, base, this->raw_data);
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

			for (int i = 0; i < json_array_get_count(vcns); i++) {
				JSON_Object *vcn = json_object(json_array_get_value(vcns, i));
				uint64_t base = (uint64_t) json_object_get_number(vcn, "address");
				uint64_t vmid = (uint64_t) json_object_get_number(vcn, "vmid");
				uint32_t type = (uint64_t) json_object_get_number(vcn, "type");
				char tmp[128];
				sprintf(tmp, "IB 0x%" PRIx64"@0x%" PRIx64, vmid, base);
				ImGui::PushID(i);
				if (ImGui::BeginTabItem(tmp)) {
					JSON_Array *op = json_object_get_array(vcn, "opcodes");
					uint32_t *copy = new uint32_t[json_array_get_count(op)];
					for (size_t j = 0; j < json_array_get_count(op); j++)
						copy[j] = (uint32_t)json_array_get_number(op, j);

					char **opcode_strs = NULL;

					umr_vcn_decode(asic, copy, json_array_get_count(op) * 4, base, type, &opcode_strs);

					sprintf(tmp, "0x%" PRIx64, base);

					ImGui::BeginChild(tmp);
					ImGui::BeginTable("vcn", 3, ImGuiTableFlags_Borders);
					ImGui::TableSetupColumn(tmp, ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize(" 0x0000000000 ").x);
					ImGui::TableSetupColumn("Raw Value", ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize("0x00000000  ").x);
					ImGui::TableSetupColumn("Decoded IB");
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
						ImGui::Text("%s", opcode_strs && opcode_strs[j] ?opcode_strs[j]:"...");
						if (opcode_strs && opcode_strs[j])
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

	uint32_t display_ib(JSON_Object *ib, enum umr_ring_type type, uint64_t base, uint32_t *buffer, int rptr = -1, int wptr = -1, int drv_wptr = -1) {
		uint32_t addr_lo_ib = 0;

		if (buffer == NULL) {
			ImVec2 space = ImGui::GetContentRegionAvail();
			float w = ImGui::CalcTextSize("Empty ring").x;
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + space.x / 2 - w / 2);
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + space.y / 2);
			ImGui::Text("Empty Ring");
			return 0;
		}

		ImGui::BeginTable("dis", 3, ImGuiTableFlags_BordersV);
		ImGui::TableSetupColumn(rptr >= 0 ? "Index" : "Address", ImGuiTableColumnFlags_WidthFixed,
			rptr >= 0 ? ImGui::CalcTextSize(" Index ").x : ImGui::CalcTextSize(" 0x0000000000000000 ").x);
		ImGui::TableSetupColumn("Raw Value", ImGuiTableColumnFlags_WidthFixed,
			ImGui::CalcTextSize(" 00000000 ").x);
		ImGui::TableSetupColumn("Opcode");
		ImGui::TableHeadersRow();

		uint32_t start = (uint32_t)json_object_get_number(ib, "opcode_start");
		uint32_t ndwords = (uint32_t)json_object_get_number(ib, "opcode_count");

		int draw_dispatch_count = 0;

		bool opcode_verbose = false;
		struct umr_stream_decode_ui ui = { };
		ui.rt = type;
		ui.start_ib = _start_ib;
		ui.start_opcode = _start_opcode;
		ui.add_field = _add_field;
		ui.add_shader = _add_shader;
		ui.add_data = _add_data;
		ui.unhandled = _ring_unhandled;
		ui.done = _done;
		ui.data = &opcode_verbose;

		struct umr_packet_stream *str = umr_packet_decode_buffer(
			asic, &ui, 0, base,
			&buffer[start], ndwords,
			type);

		if (str)
			umr_packet_disassemble_stream(str, base, 0, 0, 0, ~0UL, 0, 0);
		umr_packet_free(str);

		ImGui::EndTable();

		return addr_lo_ib;
	}
private:
	JSON_Object *last_answer;
	uint32_t *raw_data;
	SyntaxHighlighter ib_syntax;
	SyntaxHighlighter shader_syntax;
	int current_item;
	bool halt;
	bool rptr_wptr;
};

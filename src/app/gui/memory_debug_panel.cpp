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
#include "imgui_memory_editor.h"

class MemoryDebugPanel : public Panel {
public:
	MemoryDebugPanel(struct umr_asic *asic) : Panel(asic), use_linear(true) {
		strcpy(vram_address, "00100000000");
		vram_content = NULL;
		vram_size = 1024;
		vmid = 1;
		valid_content_size = 0;
		num_page_table_entries = 0;
	}
	~MemoryDebugPanel() {
		free(vram_content);
	}

	void process_server_message(JSON_Object *response, void *raw_data, unsigned raw_data_size) {
		JSON_Value *error = json_object_get_value(response, "error");
		if (error)
			return;

		JSON_Object *request = json_object(json_object_get_value(response, "request"));
		JSON_Value *answer = json_object_get_value(response, "answer");
		const char *command = json_object_get_string(request, "command");

		if (!strcmp(command, "vm-decode") || !strcmp(command, "vm-read")) {
			if (raw_data_size) {
				vram_content = realloc(vram_content, raw_data_size);
				memcpy(vram_content, raw_data, raw_data_size);
				valid_content_size = raw_data_size;
			}

			JSON_Array *pt = json_array(json_object_get_value(json_object(answer), "page_table"));
			if (pt) {
				int num_pt = json_array_get_count(pt);
				for (int k = 0; k < num_pt; k++) {
					JSON_Object *level = json_object(json_array_get_value(pt, k));
					page_table[k].pba = json_object_get_number(level, "pba");
					page_table[k].type = json_object_get_number(level, "type");
					if (page_table[k].type == 2)
						page_table[k].va_mask = json_object_get_number(level, "va_mask");
					page_table[k].system = json_object_get_number(level, "system");
					page_table[k].tmz = json_object_get_number(level, "tmz");
					page_table[k].mtype = json_object_get_number(level, "mtype");
				}
				num_page_table_entries = num_pt;
				decoded_addr = json_object_get_number(request, "address");
				decoded_vmid = json_object_get_number(request, "vmid");
			}
		}
	}

	bool display(float dt, const ImVec2& avail, bool can_send_request) {
		const float _8digitsize = ImGui::CalcTextSize("0x00000000").x + ImGui::GetStyle().FramePadding.x * 2;

		ImGui::Checkbox("Linear (no vmid)", &use_linear);

		ImGui::SameLine();
		ImGui::SetNextItemWidth(_8digitsize / 4);
		if (!use_linear)
			ImGui::InputInt("VMID", &vmid, 0, 0, ImGuiInputTextFlags_CharsDecimal);
		ImGui::SameLine();
		ImGui::Text("Address: 0x");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(_8digitsize * 2);
		ImGui::PushID("Address");
		ImGui::InputText("", vram_address, 20, ImGuiInputTextFlags_CharsHexadecimal);
		ImGui::PopID();
		ImGui::SameLine();
		ImGui::SetNextItemWidth(_8digitsize);
		ImGui::InputInt("Size", &vram_size, 0, 0, ImGuiInputTextFlags_CharsDecimal);
		ImGui::SameLine();
		ImGui::BeginDisabled(!can_send_request || dt < 0);
		if (ImGui::Button("Read")) {
			send_vm_read_command(use_linear);
		}
		ImGui::SameLine();
		if (!use_linear && ImGui::Button("Decode 1 page")) {
			send_vm_read_command(use_linear, true);
		}
		ImGui::EndDisabled();
		ImGui::Separator();

		/* Split pane */
		ImGui::BeginChild("Memory Viewer", ImVec2(avail.x / 2, 0), false, ImGuiWindowFlags_NoTitleBar);
		if (valid_content_size) {
			mem_edit.OptShowDataPreview = true;
			mem_edit.OptShowAscii = false;

			mem_edit.DrawContents(vram_content,
								  valid_content_size,
								  (size_t)0);
		}
		ImGui::EndChild();
		ImGui::SameLine();
		add_vertical_line(avail);

		uint64_t addr;
		sscanf(vram_address, "%" SCNx64, &addr);

		if (num_page_table_entries) {
			ImGui::BeginChild("VM decode", ImVec2(avail.x / 2, 0), false, ImGuiWindowFlags_NoTitleBar);
			const char *names[] = { "BASE", "PDE", "PTE" };
			int i;

			if (!num_page_table_entries) {
				ImGui::Text("No PDE/PTE info to display");
			} else {
				ImGui::Text("PDE/PTE info for 0x%" PRIx64 " @ vmid %d", decoded_addr, decoded_vmid);
				uint64_t combined_va = 0;
				int pop = 0;
				for (i = 0; i < num_page_table_entries; i++) {
					if (ImGui::TreeNodeEx(names[page_table[i].type], ImGuiTreeNodeFlags_DefaultOpen)) {
						ImGui::Columns(2);
						ImGui::Text("Physical Base Address");
						ImGui::NextColumn();
						ImGui::Text("#cb4b160x%" PRIx64, page_table[i].pba);
						ImGui::NextColumn();
						if (i == num_page_table_entries - 1) {
							ImGui::Text("Location");
							ImGui::NextColumn();
							ImGui::Text(page_table[i].system ? "#b58900System #b58900RAM" : "#b58900VRAM");
							ImGui::NextColumn();
							ImGui::Text("mtype");
							ImGui::NextColumn();
							switch (page_table[i].mtype) {
								case 0: ImGui::Text("NC"); break;
								case 1: ImGui::Text("RW"); break;
								case 2: ImGui::Text("CC"); break;
								case 3: ImGui::Text("UC"); break;
							}
							ImGui::NextColumn();
							ImGui::Text("TMZ");
							ImGui::NextColumn();
							ImGui::Text("#b58900%x", page_table[i].tmz);
						}
						ImGui::Columns(1);
						pop++;
					}
				}
				while (pop-- > 0)
					ImGui::TreePop();
				ImGui::EndChild();
			}
		}
		return false;
	}
private:
	void send_vm_read_command(bool use_linear, bool decode_only = false) {
		JSON_Value *req = json_value_init_object();
		json_object_set_string(json_object(req), "command", decode_only ? "vm-decode" : "vm-read");
		uint64_t addr;
		sscanf(vram_address, "%" SCNx64, &addr);
		json_object_set_number(json_object(req), "address", addr);
		json_object_set_number(json_object(req), "size", vram_size);
		if (!use_linear)
			json_object_set_number(json_object(req), "vmid", vmid);
		send_request(req);
	}

private:
	void *vram_content;
	char vram_address[32];
	int vram_size;
	int vmid;
	int valid_content_size;
	struct {
		uint64_t va_mask;
		uint64_t pba;
		int type; /* 0: base, 1: pde, 2: pte */
		int valid;
		int system, tmz, mtype;
	} page_table[64];
	int num_page_table_entries;
	uint64_t decoded_addr;
	int decoded_vmid;
	MemoryEditor mem_edit;
	bool use_linear;
};

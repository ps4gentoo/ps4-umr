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

class InfoPanel : public Panel {
public:
	InfoPanel(struct umr_asic *asic) : Panel(asic) { }

	void process_server_message(JSON_Object *response, void *raw_data, unsigned raw_data_size) {	}

	bool display(float dt, const ImVec2& avail, bool can_send_request) {
		static const char *families[] = {
			"SI", "CIK", "VI", "AI", "NV", "GFX11", "NPI", "CFG",
		};

		ImGui::BeginChild("Info", ImVec2(avail.x / 2, 0), false, ImGuiWindowFlags_NoTitleBar);
		ImGui::BeginTable("Info", 2, ImGuiTableFlags_Borders);
		ImGui::TableSetupColumn("");
		ImGui::TableSetupColumn("Details");
		ImGui::TableHeadersRow();
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0); ImGui::Text("ASIC name");
		ImGui::TableSetColumnIndex(1); ImGui::Text("#b58900%s", asic->asicname);
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0); ImGui::Text("Instance");
		ImGui::TableSetColumnIndex(1); ImGui::Text("#b58900%d", asic->instance);
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0); ImGui::Text("DID");
		ImGui::TableSetColumnIndex(1); ImGui::Text("#b58900%02x:#b58900%02x", (asic->did >> 8) & 0xff, asic->did & 0xff);
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0); ImGui::Text("Family");
		ImGui::TableSetColumnIndex(1); ImGui::Text("#b58900%s", families[asic->family]);
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0); ImGui::Text("VRAM");
		ImGui::TableSetColumnIndex(1); ImGui::Text("#b58900%6d MB", (int) (json_object_get_number(info, "vram_size") / (1024 * 1024)));
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0); ImGui::Text("Visible VRAM");
		ImGui::TableSetColumnIndex(1); ImGui::Text("#b58900%6d MB", (int) (json_object_get_number(info, "vis_vram_size") / (1024 * 1024)));
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0); ImGui::Text("vBios version");
		ImGui::TableSetColumnIndex(1); ImGui::Text("#b58900%s", json_object_get_string(info, "vbios_version"));
		ImGui::EndTable();

		if (ImGui::TreeNode("Firmware versions")) {
			ImGui::BeginTable("Firmwares", 3, ImGuiTableFlags_Borders);
			ImGui::TableSetupColumn("Firmware");
			ImGui::TableSetupColumn("Feature version");
			ImGui::TableSetupColumn("Firmware version");
			ImGui::TableHeadersRow();
			JSON_Array *fws = json_object_get_array(info, "firmwares");
			for (int j = 0; j < json_array_get_count(fws); j++) {
				JSON_Object *fw = json_object(json_array_get_value(fws, j));
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::TextUnformatted(json_object_get_string(fw, "name")); ImGui::NextColumn(); ImGui::NextColumn();
				ImGui::TableSetColumnIndex(1);
				ImGui::Text("#b589000x%x", (unsigned)json_object_get_number(fw, "feature_version")); ImGui::NextColumn();
				ImGui::TableSetColumnIndex(2);
				ImGui::Text("#b589000x%x", (unsigned)json_object_get_number(fw, "firmware_version")); ImGui::NextColumn();
			}
			ImGui::EndTable();
			ImGui::TreePop();
		}

		if (ImGui::TreeNode("Hardware blocks")) {
			ImGui::BeginTable("HW blocks", 1, ImGuiTableFlags_Borders);
			ImGui::TableSetupColumn("HW block names");
			ImGui::TableHeadersRow();
			for (int i = 0; i < asic->no_blocks; i++) {
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::TextUnformatted(asic->blocks[i]->ipname);
			}
			ImGui::EndTable();
			ImGui::TreePop();
		}

		JSON_Object *pcie = json_object(json_object_get_value(info, "pcie"));
		if (pcie && ImGui::TreeNode("PCIe info")) {
			ImGui::BeginTable("PCIe", 2, ImGuiTableFlags_Borders);
			ImGui::TableSetupColumn("Link Speed");
			ImGui::TableSetupColumn("Link Width");
			ImGui::TableHeadersRow();
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::Text("%s", json_object_get_string(pcie, "speed"));
			ImGui::TableSetColumnIndex(1);
			ImGui::Text("x %d", (int)json_object_get_number(pcie, "width"));
			ImGui::EndTable();
			ImGui::TreePop();
		}
		ImGui::EndChild();

		ImGui::SameLine();

		ImGui::BeginChild("IP Discovery", ImVec2(avail.x / 2, 0), false, ImGuiWindowFlags_NoTitleBar);

		ImGui::EndChild();

		return false;
	}
};

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

/* from print_config.c */
extern "C" struct {
	char *name;
	uint64_t mask;
} cg_masks[];
extern "C" struct {
	char *name;
	uint64_t mask;
} pg_masks[];

class PowerPanel : public Panel {
public:
	PowerPanel(struct umr_asic *asic) : Panel(asic), last_answer(NULL),
		sensors_last_answer(NULL),
		runtimepm_last_answer(NULL),
		pp_last_answer(NULL),
		hwmon_last_answer(NULL),
		consumed(true),
		sensor_previous_values(NULL) {}

	~PowerPanel() {
		if (last_answer)
			json_value_free(json_object_get_wrapping_value(last_answer));
		if (sensors_last_answer)
			json_value_free(json_object_get_wrapping_value(sensors_last_answer));
		if (runtimepm_last_answer)
			json_value_free(json_object_get_wrapping_value(runtimepm_last_answer));
		if (pp_last_answer)
			json_value_free(json_object_get_wrapping_value(pp_last_answer));
		if (hwmon_last_answer)
			json_value_free(json_object_get_wrapping_value(hwmon_last_answer));
		free(sensor_previous_values);
	}

	void process_server_message(JSON_Object *response, void *raw_data, unsigned raw_data_size) {
		JSON_Value *error = json_object_get_value(response, "error");
		if (error)
			return;

		JSON_Object *request = json_object(json_object_get_value(response, "request"));
		JSON_Value *answer = json_object_get_value(response, "answer");
		const char *command = json_object_get_string(request, "command");

		if (!strcmp(command, "power")) {
			if (last_answer)
				json_value_free(json_object_get_wrapping_value(last_answer));
			last_answer = json_object(json_value_deep_copy(answer));
		} else if (!strcmp(command, "sensors")) {
			if (sensors_last_answer)
				json_value_free(json_object_get_wrapping_value(sensors_last_answer));
			sensors_last_answer = json_object(json_value_deep_copy(answer));
			consumed = false;
		} else if (!strcmp(command, "runtimepm")) {
			if (runtimepm_last_answer)
				json_value_free(json_object_get_wrapping_value(runtimepm_last_answer));
			runtimepm_last_answer = json_object(json_value_deep_copy(answer));
		} else if (!strcmp(command, "pp_features")) {
			if (pp_last_answer)
				json_value_free(json_object_get_wrapping_value(pp_last_answer));
			pp_last_answer = json_object(json_value_deep_copy(answer));
		} else if (!strcmp(command, "hwmon")) {
			if (hwmon_last_answer)
				json_value_free(json_object_get_wrapping_value(hwmon_last_answer));
			hwmon_last_answer = NULL;
			if (json_object_has_value(json_object(answer), "hwmons"))
				hwmon_last_answer = json_object(json_value_deep_copy(answer));
		}
	}

	bool display(float dt, const ImVec2& avail, bool can_send_request) {
		const float gui_scale = get_gui_scale();

		ImGui::BeginChild("power profiles", ImVec2(avail.x / 5, 0), false, ImGuiWindowFlags_NoTitleBar);
		static float last_sensor_read = 0;
		static float sensor_read_interval = 0.5;
		if (!last_answer) {
			if (can_send_request) {
				send_power_command(NULL);
				last_sensor_read = 0;
			}
		} else {
			ImGui::Text("Select DPM profile :");
			ImGui::Indent();
			ImGui::BeginDisabled(!can_send_request);
			JSON_Array *profiles = json_object_get_array(last_answer, "profiles");
			const char *current = json_object_get_string(last_answer, "current");
			for (int i = 0; i < json_array_get_count(profiles) ; i++) {
				const char *profile = json_array_get_string(profiles, i);
				if (ImGui::RadioButton(profile, !strcmp(current, profile))) {
					send_power_command(profile);
					last_sensor_read = 10;
				}
			}
			ImGui::EndDisabled();
			ImGui::Unindent();
		}
		ImGui::Separator();
		ImGui::Text("Clock Gating Features:");
		ImGui::Indent();
		for (int i = 0; cg_masks[i].name; i++)
			if (asic->config.gfx.cg_flags & cg_masks[i].mask)
				ImGui::Text("%s\n", &cg_masks[i].name[strlen("AMD_CG_SUPPORT_")]);
		ImGui::Unindent();

		ImGui::Text("Power Gating Features:");
		ImGui::Indent();
		for (int i = 0; pg_masks[i].name; i++)
			if (asic->config.gfx.pg_flags & pg_masks[i].mask)
				ImGui::Text("%s\n", &pg_masks[i].name[strlen("AMD_PG_SUPPORT_")]);
		ImGui::Unindent();

		ImGui::EndChild();
		ImGui::SameLine();
		ImGui::BeginChild("power sensors", ImVec2(avail.x * 1.0 / 2, 0), false, ImGuiWindowFlags_NoTitleBar);
		ImGui::SetNextItemWidth(avail.x / 4);
		ImGui::DragFloat("Refresh interval (drag to modify)", &sensor_read_interval, 0.1, 0.1, 5, "%.1f sec");
		bool suspended = false;
		if (runtimepm_last_answer) {
			auto *rpm = runtimepm_last_answer;
			bool rpm_enabled;
			const char *rpm_mode = json_object_get_string(rpm, "runtime_enabled");
			if (!rpm_mode || strcmp(rpm_mode, "forbidden") == 0) {
				ImGui::Text("Runtime Power Management: disabled (always on)");
				rpm_enabled = false;
			} else {
				ImGui::Text("Runtime Power Management");
				rpm_enabled = true;
			}
			ImGui::Text("Status: ");
			ImGui::SameLine();
			ImColor col;
			auto *str_status = json_object_get_string(rpm, "runtime_status");
			if (strcmp(str_status, "suspended") == 0) {
				suspended = true;
				col = palette[2];
			}
			else if (strcmp(str_status, "suspending") == 0 ||
					 strcmp(str_status, "resuming") == 0)
				col = palette[1];
			else if (strcmp(str_status, "active") == 0)
				col = palette[7];
			else
				col = palette[8];

			ImVec2 c1 = ImGui::GetCursorScreenPos();
			c1.x += ImGui::GetStyle().FramePadding.x;
			c1.y += ImGui::GetTextLineHeight() * 0.5;
			ImGui::GetWindowDrawList()->AddCircleFilled(c1, 6 * gui_scale, col);

			if (suspended) {
				c1.x += 20 * gui_scale;
				c1.y -= ImGui::GetTextLineHeight() * 0.5;
				ImGui::SetCursorScreenPos(c1);
				if (ImGui::Button("Wake up"))
					send_wakeup_command();
			} else {
				ImGui::NewLine();
			}

			if (rpm_enabled) {
				ImGui::Text("Statistics:");
				ImGui::Indent();
				double active = json_object_get_number(rpm, "active_time") / 1000;
				ImGui::Text("Active   : %s", format_duration(active));
				double spd = json_object_get_number(rpm, "suspended_time") / 1000;
				ImGui::Text("Suspended: %s", format_duration(spd));
				ImGui::Unindent();
			}

			ImGui::Separator();
		}

		if (last_sensor_read > sensor_read_interval) {
			if (can_send_request) {
				send_runtimepm_command();
				send_pp_features_command(0, false);
				if (!suspended) {
					send_sensors_command();
					send_fans_command(-1, -1, -1);
				}
				last_sensor_read = 0;
			}
		} else {
			last_sensor_read += dt;
		}
		if (sensors_last_answer && !suspended) {
			ImGui::Text("Sensors values:");
			const int old_value_count = 100;
			JSON_Array *values = json_object_get_array(sensors_last_answer, "values");
			int sensors_count = json_array_get_count(values);

			if (!sensor_previous_values) {
				sensor_previous_values = (float *)calloc(sensors_count * old_value_count, sizeof(float));
				sensor_values_offset = 0;
			}

			ImVec2 previous_cursor;
			for (int i = 0; i < sensors_count; i++) {
				JSON_Object *v = json_object(json_array_get_value(values, i));
				sensor_previous_values[i * old_value_count + sensor_values_offset] =
					(int)json_object_get_number(v, "value");

				int same_graph = 0;

				if (i < sensors_count - 1) {
					previous_cursor = ImGui::GetCursorScreenPos();
				} else {
					same_graph = 1;
					ImGui::SetCursorScreenPos(previous_cursor);
				}

				ImGui::PlotLines(json_object_get_string(v, "name"),
								 &sensor_previous_values[i * old_value_count],
								  old_value_count,
								  sensor_values_offset + 1,
								  NULL,
								  json_object_get_number(v, "min"),
								  json_object_get_number(v, "max"),
								  ImVec2(0, avail.y / (2 + sensors_count)),
								  sizeof(float),
								  same_graph);
				ImGui::SameLine();
				if (same_graph) {
					ImVec2 c = ImGui::GetCursorScreenPos();
					c.y += ImGui::GetTextLineHeightWithSpacing();
					ImGui::SetCursorScreenPos(c);
				}
				ImGui::Text("%s: %d %s",
					json_object_get_string(v, "name"),
					(int)sensor_previous_values[i * old_value_count + sensor_values_offset],
					json_object_get_string(v, "unit"));
			}
			if (!consumed) {
				sensor_values_offset = (sensor_values_offset + 1) % old_value_count;
				consumed = true;
			}
		}

		if (hwmon_last_answer) {
			JSON_Array *hwmons = json_object_get_array(hwmon_last_answer, "hwmons");

			for (int i = 0; i < json_array_get_count(hwmons); i++) {
				JSON_Object *hwmon = json_object(json_array_get_value(hwmons, i));
				int hwmon_id = json_object_get_number(hwmon, "id");
				ImGui::Text("hwmon%d:", hwmon_id);
				ImGui::Text("   Fan:");
				ImGui::Separator();
				JSON_Object *fan = json_object(json_object_get_value(hwmon, "fan"));
				float v = json_object_get_number(fan, "value");
				float min = json_object_get_number(fan, "min");
				float max = json_object_get_number(fan, "max");
				int mode = (int) json_object_get_number(fan, "mode");
				int new_mode = -1, new_pwm = -1;
				float percent = 100.0f * v / 255;

				if (ImGui::SliderFloat("", &percent, 0, 100, "%.0f %%"))
					new_pwm = (int) (255.0 * percent / 100.0) ;

				const char *modes[] = { "none (!)", "manual (!)", "auto" };
				ImGui::SameLine();
				ImGui::Text("Control Mode:");
				ImGui::SameLine();
				ImGui::BeginGroup();
				for (int i = 2; i >= 1; i--) {
					if (ImGui::RadioButton(modes[i], mode == i)) {
						new_mode = i;
					}
					if (i == 1 && ImGui::IsItemHovered()) {
						ImGui::SetTooltip("#dbde79Warning: monitor the temperatures to avoid damaging the GPU");
					}
				}
				ImGui::EndGroup();

				if (new_mode >= 0 || new_pwm >= 0) {
					send_fans_command(hwmon_id, new_mode, new_pwm);
				}

				JSON_Array *temps = json_object_get_array(hwmon, "temp");
				ImGui::Text("   Temperatures:");
				ImGui::BeginTable("temps", 3, ImGuiTableFlags_Borders);
				ImGui::TableSetupColumn("Label");
				ImGui::TableSetupColumn("Value (°C)");
				ImGui::TableSetupColumn("Critical (°C)");
				ImGui::TableHeadersRow();
				for (int j = 0; j < json_array_get_count(temps); j++) {
					JSON_Object *temp = json_object(json_array_get_value(temps, j));
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("%s", json_object_get_string(temp, "label"));
					ImGui::TableSetColumnIndex(1);
					ImGui::Text("%d", (int) json_object_get_number(temp, "value") / 1000);
					ImGui::TableSetColumnIndex(2);
					ImGui::Text("%d", (int) json_object_get_number(temp, "critical") / 1000);
				}
				ImGui::EndTable();
			}
		}
		ImGui::EndChild();
		ImGui::SameLine();
		ImGui::BeginChild("power features", ImVec2(0, 0), false, ImGuiWindowFlags_NoTitleBar);
		ImGui::Text("PowerPlay features:");
		if (pp_last_answer) {
			uint64_t raw_value = json_object_get_number(pp_last_answer, "raw_value");
			uint64_t original = raw_value;
			ImGui::BeginTable("shader", 2, ImGuiTableFlags_Borders);
			JSON_Array *bits = json_object_get_array(pp_last_answer, "features");
			for (size_t i = 0; i < json_array_get_count(bits); i++) {
				JSON_Object *feat = json_object(json_array_get_value(bits, i));
				if (!json_object_has_value(feat, "name"))
					continue;
				if ((i % 2) == 0) {
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
				} else {
					ImGui::TableSetColumnIndex(1);
				}
				bool v = json_object_get_boolean(feat, "on");
				if (ImGui::Checkbox(json_object_get_string(feat, "name"), &v)) {
					uint64_t b = 1lu << i;
					if (v)
						raw_value |= b;
					else
						raw_value &= ~b;
				}
			}
			ImGui::EndTable();
			if (raw_value != original)
				send_pp_features_command(raw_value, true);
		} else {
			ImGui::Text("(only available for Vega10 and later dGPUs)");
		}
		ImGui::EndChild();
		return sensors_last_answer != NULL;
	}

private:
	void send_runtimepm_command() {
		JSON_Value *req = json_value_init_object();
		json_object_set_string(json_object(req), "command", "runtimepm");
		send_request(req);
	}

	void send_sensors_command() {
		JSON_Value *req = json_value_init_object();
		json_object_set_string(json_object(req), "command", "sensors");
		send_request(req);
	}

	void send_power_command(const char *new_value) {
		JSON_Value *req = json_value_init_object();
		json_object_set_string(json_object(req), "command", "power");
		if (new_value)
			json_object_set_string(json_object(req), "set", new_value);
		send_request(req);
	}

	void send_wakeup_command() {
		JSON_Value *req = json_value_init_object();
		json_object_set_string(json_object(req), "command", "wakeup");
		send_request(req);
	}

	void send_pp_features_command(uint64_t new_value, bool set) {
		JSON_Value *req = json_value_init_object();
		json_object_set_string(json_object(req), "command", "pp_features");
		if (set)
			json_object_set_number(json_object(req), "set", new_value);
		send_request(req);
	}

	void send_fans_command(int hwmon, int new_mode, int new_pwm) {
		JSON_Value *req = json_value_init_object();
		json_object_set_string(json_object(req), "command", "hwmon");
		if (hwmon >= 0) {
			JSON_Value *set = json_value_init_object();
			json_object_set_number(json_object(set), "hwmon", hwmon);
			json_object_set_number(json_object(set), "mode", new_mode);
			json_object_set_number(json_object(set), "value", new_pwm);
			json_object_set_value(json_object(req), "set", set);
		}
		send_request(req);
	}

private:
	JSON_Object *last_answer;
	JSON_Object *sensors_last_answer;
	JSON_Object *runtimepm_last_answer;
	JSON_Object *pp_last_answer;
	JSON_Object *hwmon_last_answer;
	bool consumed;
	float *sensor_previous_values;
	int sensor_values_offset;
};


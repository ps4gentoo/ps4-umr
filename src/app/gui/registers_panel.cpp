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
#include <set>

#include "kernel_trace_event.h"
#include "drawable_area.h"

struct PinnedRegister {
	PinnedRegister(struct umr_ip_block *_blk, struct umr_reg *_reg) : blk(_blk), reg(_reg) { }
	struct umr_ip_block *blk;
	struct umr_reg *reg;
	uint32_t new_value;
	bool collapsed;
};

const char *skip_register_prefix(const char *reg_name) {
	if (strncmp(reg_name, "mm", 2) == 0)
		return reg_name + 2;
	else if (strncmp(reg_name, "reg", 3) == 0)
		return reg_name + 3;
	return reg_name;
}

static ImColor get_value_color(int index, uint32_t value, uint32_t original, bool highlight)
{
	if (value == original) {
		return palette[highlight ? 0 : (8 + 2 * (index % 2))];
	} else {
		return palette[highlight ? 2 : 3];
	}
}

struct RegisterEvent : public Event {
	RegisterEvent(EventBase b) : Event(b), value(0xffffffff), reg(NULL) { }

	RegisterEvent& operator=(const RegisterEvent& evt) {
		_copy(evt);
		value = evt.value;
		regaddr = evt.regaddr;
		reg = evt.reg;
		return *this;
	}

	RegisterEvent(const RegisterEvent& evt) : Event(evt.type, evt.timestamp, evt.pid) {
		*this = evt;
	}

	void handle_field(int field_idx, char *name, int name_len, char *v, int value_len) {
		if (field_idx == 1)
			regaddr = strtoll(v, NULL, 16);
		if (field_idx == 2)
			value = strtoll(v, NULL, 16);
	}

	uint32_t regaddr;
	uint32_t value;

	umr_reg *reg;
};

#define ALL_REGISTERS ((umr_reg*)0xffffffff)

static void parse_raw_event_buffer(struct umr_asic *asic,
								   void *raw_data, unsigned raw_data_size,
								   std::vector<RegisterEvent>& events,
								   JSON_Array *names) {
	double timestamp;

	EventBase bp = { };
	unsigned consumed = 0;
	double max_fence_duration = -1;
	char *input = static_cast<char*>(raw_data);

	while (consumed < raw_data_size) {
		/* Find the next event. */
		int n = Event::parse_raw_event_buffer(
			input + consumed, raw_data_size - consumed, names, &bp);

		if (n < 0)
			break;
		consumed += n;

		switch (bp.type) {
		case EventType::AmdgpuDeviceWreg:
			break;
		default:
			continue;
		}

		RegisterEvent event(bp);

		/* Parse the fields. */
		consumed += Event::parse_event_fields(input + consumed, &event);

		umr_reg *reg = umr_find_reg_by_addr(asic, event.regaddr, NULL);
		if (reg) {
			event.reg = reg;
			events.push_back(event);
		} else {
			printf("Ignored write to unknown offset 0x%08x\n", event.regaddr);
		}
	}
}

class RegistersPanel : public Panel {
public:
	RegistersPanel(struct umr_asic *asic) : Panel(asic), hightlighted_field(NULL), active_tracking(NULL) {}

	~RegistersPanel() {}

	void process_server_message(JSON_Object *response, void *raw_data, unsigned raw_data_size) {
		JSON_Value *error = json_object_get_value(response, "error");
		if (error)
			return;

		JSON_Object *request = json_object(json_object_get_value(response, "request"));
		JSON_Value *answer = json_object_get_value(response, "answer");
		const char *command = json_object_get_string(request, "command");

		if (strcmp(command, "read") && strcmp(command, "write")) {
			if (str_is(command, "read-trace-buffer")) {
				parse_raw_event_buffer(asic,
					raw_data, raw_data_size, events,
					json_object_get_array(json_object(answer), "names"));
			}
			return;
		}

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
			struct umr_ip_block *b = umr_find_ip_block(asic, blk, 0);
			if (r && b) {
				pinned_registers.push_back(PinnedRegister(b, r));
				pinned = &pinned_registers.back();
			}
		}

		pinned->reg->value = pinned->new_value = json_object_get_number(json_object(answer), "value");
	}

	bool display(float dt, const ImVec2& avail, bool can_send_request) {
		const float gui_scale = get_gui_scale();
		double min_ts = 0, max_ts = 0;
		const ImVec2 c(ImGui::GetCursorScreenPos());
		if (!events.empty()) {
			min_ts = events[0].timestamp;
			max_ts = events.back().timestamp;
		}
		drawable_area.update(c, ImVec2(avail.x, avail.y - c.y), min_ts, max_ts - min_ts, true);

		if (active_tracking && can_send_request) {
			JSON_Value *req = json_value_init_object();
			json_object_set_string(json_object(req), "command", "read-trace-buffer");
			send_request(req);
		}

		/* Split pane */
		ImGui::BeginChild("Registers list", ImVec2(avail.x / 4, drawable_area.get_top_row_height()), false,
							ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_HorizontalScrollbar);

		ImGui::Text("Registers per block");
		ImGui::Separator();
		char details[128];
		for (int i = 0; i < (int) asic->no_blocks; i++) {
			unsigned matching = 0;
			struct umr_ip_block *b = asic->blocks[i];
			if (filter[0] != '\0' || field_filter[0] != '\0') {
				for (int j = 0; j < b->no_regs; j++) {
					if (filter[0] != '\0' && !fuzzy_match_simple(filter, skip_register_prefix(b->regs[j].regname))) {
						continue;
					} else if (field_filter[0] != '\0') {
						for (int k = 0; k < b->regs[j].no_bits; k++) {
							if (b->regs[j].bits[k].regname &&
								  fuzzy_match_simple(field_filter, b->regs[j].bits[k].regname)) {
								matching++;
								break;
							}
						}
					} else {
						matching++;
					}
				}
				sprintf(details, "%d/%d matches", matching, b->no_regs);
			} else {
				sprintf(details, "%d registers", b->no_regs);
			}
			if (ImGui::TreeNodeEx(b->ipname, (matching && matching < 10) ? ImGuiTreeNodeFlags_Leaf : 0, "%12s (%s)", b->ipname, details)) {
				bool at_least_one = matching > 0;
				for (int j = 0; j < b->no_regs; j++) {
					bool pinned = false;
					for (int k = 0; k < (int) pinned_registers.size() && !pinned; k++)
						pinned = pinned_registers[k].reg == &b->regs[j];

					if (filter[0] != '\0' && !fuzzy_match_simple(filter, skip_register_prefix(b->regs[j].regname)))
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
						ImGui::TextUnformatted(skip_register_prefix(b->regs[j].regname));
					} else if (ImGui::Button(skip_register_prefix(b->regs[j].regname))) {
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

		ImGui::BeginChild("filters", ImVec2(3 * avail.x / 4, drawable_area.get_top_row_height()), false,
			ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_HorizontalScrollbar);

		ImGui::BeginDisabled(active_tracking != NULL && active_tracking != ALL_REGISTERS);
		if (ImGui::Button(active_tracking != ALL_REGISTERS ? "Track All Writes" : "Untrack")) {
			if (active_tracking) {
				send_stop_register_tracking();
				active_tracking = NULL;
			} else {
				events.clear();
				send_start_register_tracking(0);
				active_tracking = ALL_REGISTERS;
			}
		}
		ImGui::EndDisabled();
		ImGui::Separator();

		ImGui::PushStyleColor(ImGuiCol_Text, ImU32(palette[4]));
		ImGui::Text("Search register by:");
		ImGui::PopStyleColor();
		if (kb_shortcut(SDLK_f))
			ImGui::SetKeyboardFocusHere();
		ImGui::BulletText("Name:      ");
		ImGui::SameLine();
		ImGui::InputText("", filter, sizeof(filter));
		ImGui::SameLine();
		if (ImGui::Button("Clear") || (kb_shortcut(SDLK_BACKSPACE)))
			filter[0] = '\0';
		ImGui::BulletText("Field Name:");
		ImGui::SameLine();
		ImGui::PushID("field");
		ImGui::InputText("", field_filter, sizeof(field_filter));
		ImGui::PopID();
		ImGui::SameLine();
		ImGui::PushID("Field");
		if (ImGui::Button("Clear") || (ImGui::GetIO().KeyCtrl && ImGui::IsKeyReleased(SDLK_BACKSPACE)))
			field_filter[0] = '\0';
		ImGui::PopID();
		ImGui::Separator();

		if (pinned_registers.empty()) {
			ImGui::Indent();
			ImGui::Text("(Click on a register name to add it to the watchlist.)");
			ImGui::Unindent();
			ImGui::NewLine();
		}

		std::vector<ImVec2> folded_coords;
		struct umr_bitfield *highlighted = NULL;
		for (int i = 0; i < (int) pinned_registers.size(); ++i) {
			PinnedRegister& pinned = pinned_registers[i];

			ImGui::PushID(pinned.reg->regname);
			ImGui::PushStyleColor(ImGuiCol_Text, ImU32(palette[6]));
			if (ImGui::TreeNodeEx(skip_register_prefix(pinned.reg->regname), ImGuiTreeNodeFlags_DefaultOpen)) {
				ImGui::PopStyleColor();
				pinned.collapsed = false;

				struct umr_bitfield *h = draw_register_value(NULL, 0, &pinned, can_send_request);
				if (h)
					highlighted = h;
				ImGui::TreePop();
			} else {
				ImGui::SameLine();
				folded_coords.push_back(ImGui::GetCursorScreenPos());
				ImGui::NewLine();
				pinned.collapsed = true;
				ImGui::PopStyleColor();
			}
		ImGui::PopID();

		}

		ImGui::PushStyleColor(ImGuiCol_Text, ImU32(palette[6]));
		float align_x = 0;
		for (auto v: folded_coords) {
			align_x = std::max(align_x, v.x);
		}
		for (int i = 0, j = 0; i < (int) pinned_registers.size(); ++i) {
			PinnedRegister& pinned = pinned_registers[i];
			if (!pinned.collapsed)
				continue;


			ImGui::SetCursorScreenPos(ImVec2(align_x, folded_coords[j++].y));
			ImGui::Text("... 0x%08x%c",
						pinned.new_value,
						pinned.new_value != pinned.reg->value ? '*' : ' ');
		}
		ImGui::PopStyleColor();

		ImGui::EndChild();

		ImGui::PushClipRect(ImVec2(0, drawable_area.get_row_split_y()),
							ImVec2(avail.x, avail.y), true);

		if (!events.empty()) {
			const float top_y = drawable_area.get_row_split_y() +
									2.5 * ImGui::GetTextLineHeight() + ImGui::GetStyle().FramePadding.y;
			const float bar_height = drawable_area.get_extent().w - top_y;
			const float bit_height = bar_height / 32;
			double offset_ts = drawable_area.get_timestamp_offset();
			const float title_y = top_y - ImGui::GetTextLineHeight();

			const float bar_width = 5 * gui_scale;
			const float spacing = ImGui::GetStyle().FramePadding.x;
			float previous_title_ended_at = -1000;

			std::set<umr_reg*> unique_registers;

			bool tooltip = false;
			for (const auto& evt: events) {
				double x = drawable_area.timestamp_to_x(evt.timestamp - min_ts);

				assert(evt.reg);

				unique_registers.insert(evt.reg);

				const char *name = skip_register_prefix(evt.reg->regname);
				float w = ImGui::CalcTextSize(name).x;
				float xa = previous_title_ended_at + spacing;
				float xb = x - w * 0.5;
				float tx = std::max(xa, xb);
				if (tx < x) {
					ImGui::GetWindowDrawList()->AddText(ImVec2(tx, title_y), palette[6], name);
					previous_title_ended_at = tx + w;
				}

				ImGui::PushClipRect(ImVec2(x, top_y),
									ImVec2(x + bar_width, top_y + bar_height), true);

				/* Display field value. */
				int previous = 0;
				for (int j = 0; j < evt.reg->no_bits; j++) {
					struct umr_bitfield *bit = &evt.reg->bits[j];

					if (previous < bit->start) {
						ImGui::GetWindowDrawList()->AddRectFilled(
							ImVec2(x, top_y + previous * bit_height),
							ImVec2(x + bar_width, top_y + bit->start * bit_height),
							ImColor(1.f, 1.f, 1.f, 0.2f));
					}

					for (int k = bit->start; k <= bit->stop; k++) {
						unsigned mask = 1llu << k;
						ImColor color((evt.value & mask) ? palette[6] : palette[1]);
						ImGui::GetWindowDrawList()->AddRectFilled(
							ImVec2(x, top_y + k * bit_height),
							ImVec2(x + bar_width, top_y + (k + 1) * bit_height), color);
					}

					ImGui::GetWindowDrawList()->AddLine(
						ImVec2(x - 2 * bar_width, top_y + (bit->stop + 1) * bit_height - 1),
						ImVec2(x + 2 * bar_width, top_y + (bit->stop + 1) * bit_height - 1), IM_COL32_WHITE, 1);

					previous = bit->stop + 1;
				}

				ImGui::GetWindowDrawList()->AddRectFilled(
					ImVec2(x, top_y + previous * bit_height),
					ImVec2(x + bar_width, top_y + 32 * bit_height),
					ImColor(1.f, 1.f, 1.f, 0.2f));

				if (!tooltip && ImGui::IsMouseHoveringRect(ImVec2(x, top_y), ImVec2(x + bar_width, top_y + bar_height))) {
					ImGui::BeginTooltip();
					ImGui::Text("Timestamp: %f", evt.timestamp);
					ImGui::Text("PID: %d", evt.pid);
					ImGui::Separator();
					ImGui::PushStyleColor(ImGuiCol_Text, ImU32(palette[6]));
					ImGui::TextUnformatted(name);
					ImGui::PopStyleColor();
					draw_register_value(evt.reg, evt.value, NULL, false);
					if (evt.stacktrace) {
						ImGui::Separator();
						ImGui::Text("Callstack:");
						ImGui::Indent();
						char *ptr = evt.stacktrace;
						while (true) {
							char *end = strchr(ptr, '|');
							if (!end) {
								ImGui::TextUnformatted(ptr);
								break;
							} else {
								ImGui::Text("%.*s", (int)(end - ptr), ptr);
								ptr = end + 1;
							}
						}
					}
					ImGui::EndTooltip();
					tooltip = true;
				}

				ImGui::PopClipRect();
			}

			char label[512];
			sprintf(label, "%ld writes to %ld unique registers", events.size(), unique_registers.size());
			float w = ImGui::CalcTextSize(label).x;
			ImGui::GetWindowDrawList()->AddText(
				ImVec2(
					avail.x * 0.5 - w * 0.5,
					drawable_area.get_row_split_y() + ImGui::GetTextLineHeight() * 0.5),
				IM_COL32_WHITE,
				label);


			ImGui::GetWindowDrawList()->AddText(
				ImVec2(ImGui::GetStyle().FramePadding.x,
					   top_y + 0 * bit_height),
				IM_COL32_WHITE, "bit  0");
			ImGui::GetWindowDrawList()->AddText(
				ImVec2(ImGui::GetStyle().FramePadding.x,
					   top_y + bar_height - ImGui::GetTextLineHeight()),
			IM_COL32_WHITE, "bit 31");
		}


		ImGui::PopClipRect();

		if (hightlighted_field != highlighted) {
			hightlighted_field = highlighted;
			return true;
		}
		return false;
	}

private:
	struct umr_bitfield * draw_register_value(umr_reg *reg, uint32_t value, PinnedRegister *pinned, bool can_send_request) {
		const float _8digitsize = ImGui::CalcTextSize("00000000").x + ImGui::GetStyle().FramePadding.x * 2;
		const float line_height = ImGui::GetTextLineHeight();
		struct umr_bitfield *highlighted = NULL;

		assert(reg == NULL || pinned == NULL);
		if (pinned) {
			reg = pinned->reg;
			value = reg->value;
		}

		char tmp[9];

		if (pinned) {
			bool dirty = pinned->new_value != value;
			if (dirty) {
				ImColor col(get_value_color(0, pinned->new_value, value, false));
				ImGui::PushStyleColor(ImGuiCol_Text, ImU32(col));
			}
			/* Mess up a bit with the cursor to:
			 * - remove horizontal spacing
			 * - aligned vertically with the InputText
			 */
			ImVec2 prev = ImGui::GetCursorScreenPos();
			ImGui::SetCursorScreenPos(ImVec2(prev.x, prev.y + ImGui::GetStyle().FramePadding.y));
			ImGui::Text("Value: 0x");
			ImGui::SameLine();
			prev.x = ImGui::GetCursorScreenPos().x - ImGui::GetStyle().ItemSpacing.x;
			ImGui::SetCursorScreenPos(prev);

			sprintf(tmp, "%08x", pinned->new_value);
			ImGui::SetNextItemWidth(_8digitsize);
			if (ImGui::InputText("", tmp, sizeof(tmp), ImGuiInputTextFlags_CharsHexadecimal)) {
				unsigned new_val;
				if (sscanf(tmp, "%x", &new_val) == 1) {
					pinned->new_value = new_val;
					force_redraw();
				}
			}

			if (dirty)
				ImGui::PopStyleColor();
		} else {
			ImGui::Text("Value: 0x%08x", value);
		}

		if (pinned) {
			ImGui::BeginDisabled(!can_send_request);
			ImGui::SameLine();
			if (ImGui::Button("Read")) {
				send_read_reg_command(pinned);
			}
			ImGui::EndDisabled();

			ImGui::BeginDisabled(!can_send_request || value == pinned->new_value);
			ImGui::SameLine();
			if (ImGui::Button("Write")) {
				send_write_reg_command(pinned, pinned->new_value);
			}
			ImGui::EndDisabled();

			ImGui::SameLine();
			ImGui::BeginDisabled(active_tracking == ALL_REGISTERS);
			if (ImGui::Button(active_tracking == reg ? "Untrack" : "Track")) {
				if (active_tracking) {
					send_stop_register_tracking();
				}
				if (active_tracking != reg) {
					events.clear();
					send_start_register_tracking(reg->addr);
					active_tracking = reg;
				} else {
					active_tracking = NULL;
				}
			}
			ImGui::EndDisabled();
		}

		ImVec2 p = ImGui::GetCursorScreenPos();
		float cx = ImGui::GetFontSize();

		ImVec2 bitfield_pos[32];

		/* Display bits value. */
		int previous_bit = 32;
		for (int j = reg->no_bits - 1; j >= 0; j--) {
			struct umr_bitfield *bit = &reg->bits[j];

			bitfield_pos[j].x = p.x;

			bool outside;
			for (int k = previous_bit - 1; k >= bit->start; k--) {
				if (k > bit->stop) {
					ImGui::GetWindowDrawList()->AddText(p, ImColor(1.f, 1.f, 1.f, 0.2f), "x");
					outside = true;
				} else {
					if (outside) {
						p.x += cx * 0.5;
						outside = false;
					}

					unsigned mask = 1u << k;
					unsigned v_original = (value & mask) >> k;
					unsigned v = pinned ? ((pinned->new_value & mask) >> k) : v_original;

					ImColor col(get_value_color(j, v, v_original, hightlighted_field == bit));

					if (pinned) {
						if (ImGui::IsMouseHoveringRect(p, ImVec2(p.x + cx * 0.5, p.y + line_height))) {
							col = IM_COL32_WHITE;
							if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
								if (v)
									pinned->new_value &= ~mask;
								else
									pinned->new_value |= mask;
							}
						}
					}

					ImGui::GetWindowDrawList()->AddText(p, col, v ? "1" : "0");
				}
				if (ImGui::IsMouseHoveringRect(p, ImVec2(p.x + cx * 0.5, p.y + line_height)))
					highlighted = bit;
				p.x += cx * 0.5;

			}
			bitfield_pos[j].x = p.x - cx * 0.25;
			p.x += cx * 0.5;
			previous_bit = bit->start;
		}

		ImGui::NewLine();
		/* Display bitfields name */
		ImVec2 c[4];
		float max_x_pos = 0;
		for (int j = 0; j < reg->no_bits; j++) {
			struct umr_bitfield *bit = &reg->bits[j];
			ImColor color(get_value_color(j, 0, 0, hightlighted_field == bit));

			ImGui::PushStyleColor(ImGuiCol_Text, ImU32(color));
			ImVec2 cursor(ImGui::GetCursorScreenPos());

			c[0] = ImVec2(bitfield_pos[j].x, p.y + ImGui::GetTextLineHeight());
			c[3] = ImVec2(bitfield_pos[j].x - cx * 0.5 * (bit->stop - bit->start),
						  p.y + ImGui::GetTextLineHeight());
			c[1] = ImVec2(bitfield_pos[j].x, cursor.y + ImGui::GetTextLineHeight() * .5);
			c[2] = ImVec2(bitfield_pos[j].x + cx * 0.5, c[1].y);
			color.Value.w = hightlighted_field == bit ? 0.4 : 0.2;
			ImGui::GetWindowDrawList()->AddPolyline(c, 3, color, 0, 1.0);
			ImGui::GetWindowDrawList()->AddLine(c[3], c[0], color);

			c[2].x += cx * 0.5;
			c[2].y = cursor.y;
			ImGui::SetCursorScreenPos(c[2]);
			ImGui::TextUnformatted(bit->regname);
			if (ImGui::IsItemHovered())
				highlighted = bit;
			ImGui::PopStyleColor();

			bitfield_pos[j].y = c[2].y;

			max_x_pos = std::max(max_x_pos, c[2].x + ImGui::CalcTextSize(bit->regname).x);
		}

		/* Display field value. */
		for (int j = 0; j < reg->no_bits; j++) {
			struct umr_bitfield *bit = &reg->bits[j];

			unsigned mask = (1llu << (1 + (bit->stop - bit->start))) - 1;
			unsigned v = ((pinned ? pinned->new_value : value) >> bit->start) & mask;
			unsigned v_original = (value >> bit->start) & mask;

			ImColor color(get_value_color(j, v, v_original, hightlighted_field == bit));

			ImGui::PushStyleColor(ImGuiCol_Text, ImU32(color));
			ImVec2 cursor(ImGui::GetCursorScreenPos());
			ImGui::SetCursorScreenPos(ImVec2(max_x_pos + cx, bitfield_pos[j].y));
			ImGui::Text("0x%x", v);

			if (ImGui::IsItemHovered())
				highlighted = bit;
			ImGui::PopStyleColor();
		}
		return highlighted;
	}

	void send_stop_register_tracking() {
		JSON_Value *req = json_value_init_object();
		json_object_set_string(json_object(req), "command", "tracing");
		json_object_set_number(json_object(req), "mode", 0);
		send_request(req);
	}

	void send_start_register_tracking(uint32_t regoffset) {
		JSON_Value *req = json_value_init_object();
		json_object_set_string(json_object(req), "command", "tracing");
		json_object_set_number(json_object(req), "mode", 2);
		json_object_set_boolean(json_object(req), "verbose", false);
		if (regoffset)
			json_object_set_number(json_object(req), "reg_offset", regoffset);
		send_request(req);
	}

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

	struct umr_bitfield *hightlighted_field;

	char filter[32] = {};
	char field_filter[32] = {};

	umr_reg *active_tracking;
	std::vector<RegisterEvent> events;

	DrawableArea drawable_area;
};

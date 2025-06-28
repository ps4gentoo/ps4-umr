/*
 * Copyright © 2024 Advanced Micro Devices, Inc.
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
#pragma once

/* Helper class to do free form drawing in a scrollable / zoomable area.
 * It exposes a 2 rows layout, where the horizontal separation can be
 * moved to resize each row.
 */

class DrawableArea {
public:
	DrawableArea() {
		mouse_state = MouseState::None;
		top_row_zoom = 1.0;
		offset_ts = 0;
		top_row_y_offset = 0;
		row_split_y = -1;
	}

	void update(const ImVec2 pos, const ImVec2 size, double min_ts,
				double total_time, bool absolute_timestamps) {
		const float gui_scale = get_gui_scale();
		const float padding = ImGui::GetStyle().FramePadding.x;

		_size = size;
		_pos = pos;
		_scale = top_row_zoom * size.x / std::max(total_time, 0.1);
		_min_ts = min_ts;
		_extra_timestamp_offset = 0;

		if (row_split_y < 0)
			row_split_y = 10 * ImGui::GetTextLineHeight();

		if (top_row_y_offset < 0)
			top_row_y_offset = 0;

		/* Mouse handling */
		switch (mouse_state) {
			case MouseState::None: {
				float dy = std::abs(ImGui::GetIO().MousePos.y - (pos.y + size.y - row_split_y));

				if (dy <= 2 * gui_scale) {
					ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
					if (ImGui::IsMouseDown(0))
						mouse_state = MouseState::ResizingGraph;
				} else if (ImGui::IsMouseHoveringRect(pos, ImVec2(pos.x + size.x,
													  pos.y + size.y))) {
					double x = ImGui::GetMousePos().x;
					double hover_ts = (x - pos.x) / _scale + offset_ts + min_ts;

					/* Zoom */
					float w = ImGui::GetIO().MouseWheel;
					if (w) {
						top_row_zoom += w * 0.1 * top_row_zoom;
						double new_scale = top_row_zoom * size.x / total_time;
						offset_ts = (hover_ts - min_ts) - (x - pos.x) / new_scale;
						_scale = new_scale;
					}
					if (ImGui::IsMouseDragging(0)) {
						mouse_state = MouseState::ScrollingGraph;
					} else if (ImGui::IsMouseDown(1)) {
						mouse_state = MouseState::Ruler;
						ruler_start = ImGui::GetMousePos();
					}
				}

				break;
			}
			case MouseState::ResizingGraph: {
				ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
				if (ImGui::IsMouseDragging(0)) {
					row_split_y -= ImGui::GetMouseDragDelta(0).y;
					if (row_split_y < 0)
						row_split_y = 0;
					ImGui::ResetMouseDragDelta();
				} else if (!ImGui::IsMouseDown(0)) {
					mouse_state = MouseState::None;
				}
				break;
			}
			case MouseState::ScrollingGraph: {
				if (ImGui::IsMouseDragging(0)) {
					offset_ts -= (double)ImGui::GetMouseDragDelta(0).x / _scale;
					top_row_y_offset -= ImGui::GetMouseDragDelta(0).y;

					ImGui::ResetMouseDragDelta();
				} else {
					mouse_state = MouseState::None;
				}
				break;
			}
			case MouseState::Ruler: {
				if (ImGui::IsMouseDown(1)) {
					ImVec2 p = ImGui::GetMousePos();
					ImGui::GetWindowDrawList()->AddLine(ruler_start, p, IM_COL32_WHITE, 2);
					float dt = std::abs(p.x - ruler_start.x) / _scale;
					const char *txt = Panel::format_duration(dt);
					float l = ImGui::CalcTextSize(txt).x;

					ImGui::GetWindowDrawList()->AddText(
						ImVec2((ruler_start.x + p.x - l) / 2, (ruler_start.y + p.y - ImGui::GetTextLineHeight() * 2) / 2), IM_COL32_WHITE, txt);
				} else {
					mouse_state = MouseState::None;
				}
				break;
			}
		}

		if (total_time <= 0) {
			ImGui::GetWindowDrawList()->AddLine(
				ImVec2(_pos.x, get_row_split_y()), ImVec2(_pos.x + _size.x, get_row_split_y()),
			   IM_COL32_WHITE, 2 * gui_scale);
			return;
		}

		const float text_y = get_row_split_y() - ImGui::GetTextLineHeight() * 0.5f;

		/* Visible duration hint. */
		const double displayed_duration = compute_visible_duration(total_time);
		char label[128];
		if (displayed_duration > 0.5)
			sprintf(label, "%.2f sec", displayed_duration);
		else
			sprintf(label, "%.2f ms", 1000 * displayed_duration);
		int w = ImGui::CalcTextSize(label).x / 2;
		float mouse_x = _pos.x + (_size.x - w) / 2;
		ImGui::GetWindowDrawList()->AddText(
			ImVec2(mouse_x, text_y),
			IM_COL32_WHITE,
			label);

		ImGui::GetWindowDrawList()->AddLine(
			ImVec2(_pos.x, get_row_split_y()), ImVec2(mouse_x - padding, get_row_split_y()),
		   	IM_COL32_WHITE, 2 * gui_scale);
		mouse_x += w * 2 + padding * 2;


		/* Timestamp hint. */
		if (ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow)) {
			double mouse_ts = (ImGui::GetMousePos().x / _scale) + get_timestamp_offset() +
							   (absolute_timestamps ? min_ts : 0);
			if (mouse_ts >= 0) {
				char ts[64];
				sprintf(ts, "timestamp: %.6f s", mouse_ts);
				float ts_x = _pos.x + _size.x - ImGui::CalcTextSize(ts).x - padding;

				ImGui::GetWindowDrawList()->AddLine(
					ImVec2(mouse_x, get_row_split_y()), ImVec2(ts_x, get_row_split_y()),
				   	IM_COL32_WHITE, 2 * gui_scale);

				ImGui::GetWindowDrawList()->AddText(ImVec2(ts_x, text_y),
					IM_COL32_WHITE, ts, NULL);
			}
		}


	}

	bool is_input_active() const {
		return mouse_state != MouseState::None;
	}

	double compute_visible_duration(double total_duration) const {
		return total_duration / top_row_zoom;
	}

	void reset() {
		top_row_zoom = 1;
		offset_ts = 0;
	}

	void center_on_timestamp(double min_ts, double total_duration, double ts) {
		double displayed_duration = compute_visible_duration(total_duration);
		offset_ts = ts - displayed_duration * 0.5 - min_ts;
	}

	double get_top_row_y_offset() const {
		return top_row_y_offset;
	}

	float get_top_row_zoom() const {
		return top_row_zoom;
	}

	double get_timestamp_offset() const {
		return offset_ts + _extra_timestamp_offset;
	}

	double get_top_row_height() const {
		return _size.y - row_split_y;
	}

	double get_row_split_y() const {
		return _pos.y + get_top_row_height();
	}

	double timestamp_to_x(double ts) const {
		return _pos.x + (ts - (offset_ts + _extra_timestamp_offset)) * _scale;
	}

	const ImVec2& get_size() const {
		return _size;
	}

	const ImVec2& get_position() const {
		return _pos;
	}

	ImVec4 get_extent() const {
		return ImVec4(_pos.x, _pos.y, _pos.x + _size.x, _pos.y + _size.y);
	}

	void set_extra_timestamp_offset(double off) {
		_extra_timestamp_offset = off;
	}

	double get_scale() const {
		return _scale;
	}

private:
	ImVec2 _size, _pos;
	double _scale, _min_ts;

	/* time offset (horizontal scrolling) */
	double offset_ts;
	/* y offset of the top row */
	float top_row_y_offset;
	/* zoom level of the top row */
	double top_row_zoom;
	double _extra_timestamp_offset;


	float row_split_y;

	enum MouseState {
		None,
		ResizingGraph,
		ScrollingGraph,
		Ruler,
	} mouse_state;
	ImVec2 ruler_start;
};


// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "descr_column.hpp"

#include <algorithm>
#include <cstddef>

#include "config.hpp"
#include "io.hpp"
#include "io_display.hpp"
#include "io_internal.hpp"
#include "panel.hpp"
#include "pos.hpp"
#include "rect.hpp"

int DescrColumn::text_w() const
{
        // Leave room for the scrollbar and its grab zone
        return panels::w(m_panel) - 3;
}

void DescrColumn::reset_scroll()
{
        m_scroll_px = 0;
}

void DescrColumn::set_reserved_bottom_rows(const int nr_rows)
{
        m_reserved_bottom_rows = std::max(0, nr_rows);
}

void DescrColumn::draw(const std::vector<std::string>& lines, const Color& color)
{
        std::vector<std::vector<ColoredString>> colored_lines;

        colored_lines.reserve(lines.size());

        for (const std::string& line : lines) {
                if (line.empty()) {
                        colored_lines.emplace_back();
                }
                else {
                        colored_lines.push_back({{line, color}});
                }
        }

        draw(colored_lines);
}

void DescrColumn::draw(const std::vector<std::vector<ColoredString>>& lines)
{
        const R panel_px = io::panel_logical_px_rect(m_panel);

        const int cell_w = config::gui_cell_px_w();
        const int cell_h = config::gui_cell_px_h();

        // The text starts at the very top of the panel, in line with the
        // first row of the list beside it (the panel itself is inset from
        // the border box, see panels::init). The bottom buffer just keeps
        // the last line off the frame.
        const int pad_bottom_px = cell_h / 2;

        // Any reserved rows are added to the content: the last lines of
        // text can then be scrolled clear of whatever sits there, instead
        // of being stuck under it
        const int reserved_px = m_reserved_bottom_rows * cell_h;

        const int content_h =
                ((int)lines.size() * cell_h) + pad_bottom_px + reserved_px;

        m_max_scroll_px = std::max(0, content_h - panel_px.h());

        m_scroll_px = std::clamp(m_scroll_px, 0, m_max_scroll_px);

        io::set_clip_rect_to_panel(m_panel);

        for (size_t i = 0; i < lines.size(); ++i) {
                const int y_px =
                        panel_px.p0.y +
                        ((int)i * cell_h) -
                        m_scroll_px;

                if (((y_px + cell_h) < panel_px.p0.y) ||
                    (y_px > panel_px.p1.y)) {
                        continue;
                }

                int x_px = panel_px.p0.x;

                for (const ColoredString& segment : lines[i]) {
                        if (!segment.str.empty()) {
                                io::draw_text_at_px(
                                        segment.str,
                                        {x_px, y_px},
                                        segment.color,
                                        io::DrawBg::no,
                                        colors::black());
                        }

                        x_px += (int)segment.str.size() * cell_w;
                }
        }

        io::disable_clip_rect();

        scrollbar::draw_content_fades(
                panel_px,
                m_scroll_px,
                m_max_scroll_px,
                reserved_px);

        // The scrollbar is drawn last, so that the fades do not dim it
        scrollbar::draw(
                scrollbar::track_px_rect(m_panel),
                m_scroll_px,
                m_max_scroll_px);
}

void DescrColumn::set_scroll_from_drag_y(const int drag_px_y)
{
        m_scroll_px =
                m_drag.scroll_px_at(
                        scrollbar::track_px_rect(m_panel),
                        drag_px_y,
                        m_scroll_px,
                        m_max_scroll_px);
}

bool DescrColumn::try_begin_drag(const P& logical_px)
{
        if (m_max_scroll_px <= 0) {
                return false;
        }

        const R track = scrollbar::track_px_rect(m_panel);

        if (!scrollbar::grab_px_rect(track).is_pos_inside(logical_px)) {
                return false;
        }

        // Dragging the bar itself - the thumb follows the finger
        m_drag_active = true;

        m_drag.begin(
                track,
                logical_px.y,
                m_scroll_px,
                m_max_scroll_px);

        set_scroll_from_drag_y(logical_px.y);

        return true;
}

void DescrColumn::on_drag_move(const P& logical_px)
{
        if (!m_drag_active) {
                return;
        }

        set_scroll_from_drag_y(logical_px.y);
}

void DescrColumn::on_drag_end()
{
        m_drag_active = false;
}

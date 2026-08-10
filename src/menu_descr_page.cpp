// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "menu_descr_page.hpp"

#include "panel.hpp"
#include "pos.hpp"
#include "text.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// Global namespace
// -----------------------------------------------------------------------------
void append_descr_text(
        std::vector<std::vector<ColoredString>>& lines,
        const std::string& raw_str,
        const Color& default_color,
        const int w)
{
        Text text(raw_str);

        text.set_w(w);
        text.set_color(default_color);

        Color current_color = default_color;

        std::vector<ColoredString> line;

        for (const TextAction& action : text.actions()) {
                switch (action.id) {
                case TextActionId::write_str:
                        if (!line.empty() &&
                            (line.back().color == current_color)) {
                                line.back().str += action.str;
                        }
                        else {
                                line.emplace_back(
                                        action.str,
                                        current_color);
                        }
                        break;

                case TextActionId::change_color:
                        current_color = action.color;
                        break;

                case TextActionId::newline:
                case TextActionId::done:
                        lines.push_back(line);

                        line.clear();
                        break;
                }
        }
}

// -----------------------------------------------------------------------------
// MenuDescrPageState
// -----------------------------------------------------------------------------
int MenuDescrPageState::list_x0(const int block_w) const
{
        (void)block_w;

        return panels::p0(Panel::menu_descr_list).x;
}

int MenuDescrPageState::list_y0(const int nr_entries_shown) const
{
        (void)nr_entries_shown;

        // Top aligned - the list grows downwards from the same row as the
        // first line of the description text (the panel is inset from the
        // border box, see panels::init)
        return panels::p0(Panel::menu_descr_list).y;
}

int MenuDescrPageState::list_max_x1() const
{
        return panels::p0(Panel::menu_descr_text).x - 2;
}

int MenuDescrPageState::list_h() const
{
        // One row of buffer at the bottom, so that a full list does not
        // run into the border box
        return panels::h(Panel::menu_descr_list) - 1;
}

void MenuDescrPageState::draw_descr_lines(
        const std::vector<std::vector<ColoredString>>& lines)
{
        // Reset the scrolling when another entry becomes marked
        if (m_browser.y() != m_descr_last_marked_idx) {
                m_descr_last_marked_idx = m_browser.y();

                m_descr.reset_scroll();
        }

        m_descr.draw(lines);
}

int MenuDescrPageState::descr_text_w() const
{
        return m_descr.text_w();
}

bool MenuDescrPageState::try_begin_touch_drag(const P& logical_px)
{
        // Only the description SCROLLBAR is a drag zone - the text and the
        // list are operated by swiping/tapping like everywhere else
        return m_descr.try_begin_drag(logical_px);
}

void MenuDescrPageState::on_touch_drag_move(const P& logical_px)
{
        m_descr.on_drag_move(logical_px);
}

void MenuDescrPageState::on_touch_drag_end()
{
        m_descr.on_drag_end();
}

// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "info_screen_state.hpp"

#include <algorithm>

#include "SDL.h"
#include "colors.hpp"
#include "common_text.hpp"
#include "config.hpp"
#include "draw_box.hpp"
#include "io.hpp"
#include "io_display.hpp"
#include "io_internal.hpp"
#include "panel.hpp"
#include "pos.hpp"
#include "rect.hpp"
#include "scrollbar.hpp"

void InfoScreenState::draw_interface() const
{
        draw_box(panels::screen_box_area());

        const int screen_center_x = panels::center_x(Panel::screen);

        io::draw_text_center(
                " " + title() + " ",
                Panel::screen,
                {screen_center_x, panels::screen_box_area().p0.y},
                colors::title());

        const std::string cmd_info =
                (type() == InfoScreenType::scrolling)
                ? common_text::g_scrollable_info_screen_hint
                : common_text::g_screen_exit_hint;

        if (!cmd_info.empty()) {
                io::draw_text_center(
                        " " + cmd_info + " ",
                        Panel::screen,
                        {screen_center_x, panels::screen_box_area().p1.y},
                        colors::title());
        }
}

R InfoScreenState::content_px_rect() const
{
        return io::panel_logical_px_rect(Panel::info_screen_content);
}

void InfoScreenState::draw_scrollable_content() const
{
        const int nr_lines_tot = get_lines_total();

        if (nr_lines_tot <= 0) {
                return;
        }

        const int cell_h = config::gui_cell_px_h();

        const int first = first_visible_line();

        const int last = last_visible_line();

        const R content_px = content_px_rect();

        // Clip partially visible lines at the top and bottom edges
        io::set_clip_rect_px(Panel::info_screen_content, content_px);

        const P panel_p0_px = content_px.p0;

        for (int i = first; i <= last; ++i) {
                const auto line = content_line(i);

                if (line.str.empty()) {
                        continue;
                }

                const P px_pos(
                        panel_p0_px.x,
                        panel_p0_px.y + (i * cell_h) - m_scroll_px);

                io::draw_text_at_px(
                        line.str,
                        px_pos,
                        line.color,
                        io::DrawBg::no,
                        colors::black());
        }

        io::disable_clip_rect();

        draw_scroll_affordances();
}

void InfoScreenState::draw_scroll_affordances() const
{
        const R content_px = content_px_rect();

        scrollbar::draw_content_fades(content_px, m_scroll_px, max_scroll_px());

        scrollbar::draw(
                scrollbar::track_px_rect(content_px),
                m_scroll_px,
                max_scroll_px());
}

bool InfoScreenState::try_begin_touch_drag(const P& logical_px)
{
        if ((type() != InfoScreenType::scrolling) || (max_scroll_px() <= 0)) {
                return false;
        }

        const R track = scrollbar::track_px_rect(content_px_rect());

        if (!scrollbar::grab_px_rect(track).is_pos_inside(logical_px)) {
                return false;
        }

        m_is_scrollbar_drag_active = true;

        m_scrollbar_drag.begin(
                track,
                logical_px.y,
                m_scroll_px,
                max_scroll_px());

        on_touch_drag_move(logical_px);

        return true;
}

void InfoScreenState::on_touch_drag_move(const P& logical_px)
{
        if (!m_is_scrollbar_drag_active) {
                return;
        }

        set_scroll_px(
                m_scrollbar_drag.scroll_px_at(
                        scrollbar::track_px_rect(content_px_rect()),
                        logical_px.y,
                        m_scroll_px,
                        max_scroll_px()));
}

void InfoScreenState::on_touch_drag_end()
{
        m_is_scrollbar_drag_active = false;
}

int InfoScreenState::max_scroll_px() const
{
        const int cell_h = config::gui_cell_px_h();

        const int content_px_h = get_lines_total() * cell_h;

        return std::max(0, content_px_h - content_px_rect().h());
}

void InfoScreenState::set_scroll_px(const int scroll_px)
{
        m_scroll_px = std::clamp(scroll_px, 0, max_scroll_px());
}

void InfoScreenState::scroll_to_bottom()
{
        m_scroll_px = max_scroll_px();
}

int InfoScreenState::first_visible_line() const
{
        return m_scroll_px / config::gui_cell_px_h();
}

int InfoScreenState::last_visible_line() const
{
        const int cell_h = config::gui_cell_px_h();

        const int last = (m_scroll_px + content_px_rect().h() - 1) / cell_h;

        return std::min(get_lines_total() - 1, last);
}

void InfoScreenState::on_confirmed()
{
        states::pop();
}

void InfoScreenState::on_cancelled()
{
        states::pop();
}

void InfoScreenState::update()
{
        const int line_jump_px = 3 * config::gui_cell_px_h();

        const int page_jump_px =
                content_px_rect().h() - config::gui_cell_px_h();

        const auto input = io::read_input();

        switch (input.key) {
        case SDLK_KP_2:
        case SDLK_DOWN: {
                set_scroll_px(m_scroll_px + line_jump_px);
        } break;

        case SDLK_KP_8:
        case SDLK_UP: {
                set_scroll_px(m_scroll_px - line_jump_px);
        } break;

        case SDLK_PAGEUP: {
                set_scroll_px(m_scroll_px - page_jump_px);
        } break;

        case SDLK_PAGEDOWN: {
                set_scroll_px(m_scroll_px + page_jump_px);
        } break;

        case SDLK_HOME: {
                set_scroll_px(0);
        } break;

        case SDLK_END: {
                scroll_to_bottom();
        } break;

        case SDLK_RETURN:
        case SDLK_SPACE: {
                // NOTE: A tap anywhere sends the confirm key (see
                // io_input) - these screens are read and then tapped away,
                // and scrolled with their scrollbar meanwhile
                on_confirmed();
        } break;

        case SDLK_ESCAPE: {
                on_cancelled();
        } break;

        default:
                break;
        }
}

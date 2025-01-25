// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "info_screen_state.hpp"

#include "SDL.h"
#include "colors.hpp"
#include "common_text.hpp"
#include "draw_box.hpp"
#include "io.hpp"
#include "panel.hpp"
#include "pos.hpp"
#include "rect.hpp"

void InfoScreenState::draw_interface() const
{
        draw_box(panels::area(Panel::screen));

        const int screen_center_x = panels::center_x(Panel::screen);

        io::draw_text_center(
                " " + title() + " ",
                Panel::screen,
                {screen_center_x, 0},
                colors::title());

        const std::string cmd_info =
                (type() == InfoScreenType::scrolling)
                ? common_text::g_scrollable_info_screen_hint
                : common_text::g_screen_exit_hint;

        io::draw_text_center(
                " " + cmd_info + " ",
                Panel::screen,
                {screen_center_x, panels::y1(Panel::screen)},
                colors::title());
}

void InfoScreenState::update()
{
        const int line_jump = 3;

        const int nr_lines_tot = get_lines_total();

        const auto input = io::read_input();

        const int panel_h = panels::h(Panel::info_screen_content);

        switch (input.key) {
        case SDLK_KP_2:
        case SDLK_DOWN: {
                m_top_idx += line_jump;

                const int top_nr_max = std::max(0, nr_lines_tot - panel_h);

                m_top_idx = std::min(top_nr_max, m_top_idx);
        } break;

        case SDLK_KP_8:
        case SDLK_UP: {
                m_top_idx = std::max(0, m_top_idx - line_jump);
        } break;

        case SDLK_PAGEUP: {
                m_top_idx = std::max(0, m_top_idx - panel_h + 1);
        } break;

        case SDLK_PAGEDOWN: {
                m_top_idx += panel_h - 1;

                const int top_nr_max = std::max(0, nr_lines_tot - panel_h);

                m_top_idx = std::min(top_nr_max, m_top_idx);
        } break;

        case SDLK_HOME: {
                m_top_idx = 0;
        } break;

        case SDLK_END: {
                m_top_idx = std::max(0, nr_lines_tot - panel_h);
        } break;

        case SDLK_SPACE:
        case SDLK_ESCAPE: {
                // Exit screen
                states::pop();
        } break;

        default:
                break;
        }
}

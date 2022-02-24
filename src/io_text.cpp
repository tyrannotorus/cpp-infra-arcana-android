// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <optional>
#include <string>

#include "colors.hpp"
#include "config.hpp"
#include "io.hpp"
#include "io_internal.hpp"
#include "panel.hpp"
#include "pos.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// io
// -----------------------------------------------------------------------------
namespace io
{
void draw_text_at_px(
        const std::string& str,
        P px_pos,
        const Color& color,
        const io::DrawBg draw_bg,
        const Color& bg_color)
{
        if ((px_pos.y < 0) || (px_pos.y >= panel_px_h(Panel::screen)))
        {
                return;
        }

        const int cell_px_w = config::gui_cell_px_w();
        const int msg_w = (int)str.size();
        const int msg_px_w = msg_w * cell_px_w;

        const auto sdl_color = color.sdl_color();
        const auto sdl_bg_color = bg_color.sdl_color();

        const auto sdl_color_gray = colors::gray();

        const int screen_px_w = panel_px_w(Panel::screen);
        const int msg_px_x1 = px_pos.x + msg_px_w - 1;
        const bool msg_w_fit_on_screen = msg_px_x1 < screen_px_w;

        // X position to start drawing dots ("(..)") instead when the message
        // does not fit on the screen horizontally.
        const char dots[] = "(...)";
        size_t dots_idx = 0;
        const int px_x_dots = screen_px_w - (cell_px_w * 5);

        for (int i = 0; i < msg_w; ++i)
        {
                if (px_pos.x < 0 || px_pos.x >= screen_px_w)
                {
                        return;
                }

                const bool draw_dots =
                        !msg_w_fit_on_screen &&
                        (px_pos.x >= px_x_dots);

                if (draw_dots)
                {
                        draw_character_at_px(
                                dots[dots_idx],
                                px_pos,
                                sdl_color_gray,
                                draw_bg,
                                bg_color);

                        ++dots_idx;
                }
                else
                {
                        // Whole message fits, or we are not yet near the edge
                        draw_character_at_px(
                                str[i],
                                px_pos,
                                sdl_color,
                                draw_bg,
                                sdl_bg_color);
                }

                px_pos.x += cell_px_w;
        }
}

void draw_text(
        Text text,
        const Panel panel,
        P pos,
        Color color,
        const DrawBg draw_bg,
        const Color& bg_color)
{
        text.set_color(color);

        auto origin_pos = pos;

        for (const auto& action : text.actions())
        {
                switch (action.id)
                {
                case TextActionId::write_str:
                {
                        const auto px_pos = gui_to_px_coords(panel, pos);

                        draw_text_at_px(
                                action.str,
                                px_pos,
                                color,
                                draw_bg,
                                bg_color);

                        pos.x += (int)action.str.length();
                }
                break;

                case TextActionId::newline:
                {
                        ++pos.y;
                        pos.x = origin_pos.x;
                }
                break;

                case TextActionId::change_color:
                {
                        color = action.color;
                }
                break;

                case TextActionId::done:
                {
                        return;
                }
                break;
                }
        }
}

void draw_text_center(
        const std::string& str,
        const Panel panel,
        const P pos,
        const Color& color,
        const DrawBg draw_bg,
        const Color& bg_color,
        const bool is_pixel_pos_adj_allowed)
{
        const int len = (int)str.size();
        const int len_half = len / 2;
        const int x_pos_left = pos.x - len_half;

        auto px_pos =
                gui_to_px_coords(
                        panel,
                        P(x_pos_left, pos.y));

        if (is_pixel_pos_adj_allowed)
        {
                const int pixel_x_adj =
                        ((len_half * 2) == len)
                        ? (config::gui_cell_px_w() / 2)
                        : 0;

                px_pos += P(pixel_x_adj, 0);
        }

        draw_text_at_px(str, px_pos, color, draw_bg, bg_color);
}

void draw_text_right(
        const std::string& str,
        const Panel panel,
        const P pos,
        const Color& color,
        const DrawBg draw_bg,
        const Color& bg_color)
{
        const int x_pos_left = pos.x - (int)str.size() + 1;

        auto px_pos =
                gui_to_px_coords(
                        panel,
                        P(x_pos_left, pos.y));

        draw_text_at_px(str, px_pos, color, draw_bg, bg_color);
}

}  // namespace io

// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
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
#include "io_display.hpp"
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
        if ((px_pos.y < 0) || (px_pos.y >= panel_px_h(Panel::screen))) {
                return;
        }

        const int cell_px_w = config::gui_cell_px_w();
        const int msg_w = (int)str.size();
        const int msg_px_w = msg_w * cell_px_w;

        const Color color_gray = colors::gray();

        const int screen_px_w = panel_px_w(Panel::screen);
        const int msg_px_x1 = px_pos.x + msg_px_w - 1;
        const bool msg_w_fit_on_screen = msg_px_x1 < screen_px_w;

        // How many characters of the string actually land on the screen -
        // the drawing stops at the first one that does not
        int nr_drawn = 0;

        for (int x = px_pos.x;
             (nr_drawn < msg_w) && (x >= 0) && (x < screen_px_w);
             x += cell_px_w) {
                ++nr_drawn;
        }

        if (nr_drawn == 0) {
                return;
        }

        // ONE background rectangle for the whole run. A filled rectangle and
        // a glyph copy are drawn by different shaders, so alternating them
        // per character cost two draw calls and a shader switch per
        // character, and nothing could be merged. With the background out of
        // the way first, the glyphs are one unbroken run of copies from the
        // font texture - a single draw call for the whole string.
        if (draw_bg == io::DrawBg::yes) {
                const P run_px_dims(
                        nr_drawn * cell_px_w,
                        config::gui_cell_px_h());

                io::draw_rectangle_filled(
                        {px_pos, px_pos + run_px_dims - 1},
                        bg_color);
        }

        // X position to start drawing dots ("(..)") instead when the message
        // does not fit on the screen horizontally.
        const char dots[] = "(...)";
        size_t dots_idx = 0;
        const int px_x_dots = screen_px_w - (cell_px_w * 5);

        for (int i = 0; i < nr_drawn; ++i) {
                const bool draw_dots =
                        !msg_w_fit_on_screen &&
                        (px_pos.x >= px_x_dots);

                // NOTE: The background is already drawn, but the color is
                // still what decides which font texture is used (contoured
                // glyphs on a non-black background)
                if (draw_dots) {
                        draw_character_at_px(
                                dots[dots_idx],
                                px_pos,
                                color_gray,
                                io::DrawBg::no,
                                bg_color);

                        ++dots_idx;
                }
                else {
                        // Whole message fits, or we are not yet near the edge
                        draw_character_at_px(
                                str[i],
                                px_pos,
                                color,
                                io::DrawBg::no,
                                bg_color);
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
        set_display_for_panel(panel);

        text.set_color(color);

        const P origin_pos = pos;

        for (const TextAction& action : text.actions()) {
                switch (action.id) {
                case TextActionId::write_str: {
                        const P px_pos = gui_to_px_coords(panel, pos);

                        draw_text_at_px(
                                action.str,
                                px_pos,
                                color,
                                draw_bg,
                                bg_color);

                        pos.x += (int)action.str.length();
                } break;

                case TextActionId::newline: {
                        ++pos.y;
                        pos.x = origin_pos.x;
                } break;

                case TextActionId::change_color: {
                        color = action.color;
                } break;

                case TextActionId::done: {
                        return;
                } break;
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
        set_display_for_panel(panel);

        const int len = (int)str.size();
        const int len_half = len / 2;
        const int x_pos_left = pos.x - len_half;

        P px_pos = gui_to_px_coords(panel, {x_pos_left, pos.y});

        if (is_pixel_pos_adj_allowed) {
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
        set_display_for_panel(panel);

        const int x_pos_left = pos.x - (int)str.size() + 1;

        P px_pos = gui_to_px_coords(panel, {x_pos_left, pos.y});

        draw_text_at_px(str, px_pos, color, draw_bg, bg_color);
}

}  // namespace io

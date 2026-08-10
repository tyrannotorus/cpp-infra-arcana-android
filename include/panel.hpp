// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef PANEL_HPP
#define PANEL_HPP

struct R;
struct P;

enum class Panel
{
        screen,
        map,
        map_gui_stats,
        map_gui_stats_border,
        log,
        menu_descr_list,
        menu_descr_text,
        options,
        options_values,
        options_descr,
        inventory_menu,
        inventory_descr,
        info_screen_content,
        action_bar,
        END
};

namespace panels
{
// Margins (in gui cells) between the physical screen edges and edge-flush
// UI: the message log, and fullscreen border boxes with their embedded
// titles, hints and [ x ] close control. Devices may crop the outermost
// pixels (rounded display corners, camera cutouts), and some fonts make
// text reach closer to the cell edges - tune these to keep such UI
// comfortably inside the visible area.
inline constexpr int g_screen_margin_left = 0;
inline constexpr int g_screen_margin_right = 0;
inline constexpr int g_screen_margin_top = 1;
inline constexpr int g_screen_margin_bottom = 0;

void init(const P& max_gui_dims);

R area(Panel panel);

// The screen area inset by the screen edge margins above - fullscreen
// border boxes (and their embedded titles/hints) span this area instead
// of the whole screen
R screen_box_area();

P dims(Panel panel);

P p0(Panel panel);

P p1(Panel panel);

int x0(Panel panel);

int y0(Panel panel);

int x1(Panel panel);

int y1(Panel panel);

int w(Panel panel);

int h(Panel panel);

int center_x(Panel panel);

int center_y(Panel panel);

P center(Panel panel);

}  // namespace panels

#endif  // PANEL_HPP

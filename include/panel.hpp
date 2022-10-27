// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
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
        create_char_menu,
        create_char_descr,
        options,
        options_values,
        options_descr,
        inventory_menu,
        inventory_descr,
        info_screen_content,
        END
};

namespace panels
{
void init(const P& max_gui_dims);

R area(Panel panel);

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

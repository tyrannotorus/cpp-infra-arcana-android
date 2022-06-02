// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "panel.hpp"

#include <algorithm>
#include <cstddef>
#include <ostream>

#include "debug.hpp"
#include "io.hpp"
#include "msg_log.hpp"
#include "pos.hpp"
#include "rect.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static R s_panels[(size_t)Panel::END];

static void set_panel_area(
        const Panel panel,
        const int x0,
        const int y0,
        const int x1,
        const int y1)
{
        s_panels[(size_t)panel] = {x0, y0, x1, y1};
}

static void finalize_screen_dims()
{
        R& screen = s_panels[(size_t)Panel::screen];

        for (const R& panel : s_panels)
        {
                screen.p1.x = std::max(screen.p1.x, panel.p1.x);
                screen.p1.y = std::max(screen.p1.y, panel.p1.y);
        }

        TRACE
                << "Screen GUI size was set to: "
                << panels::w(Panel::screen)
                << "x"
                << panels::h(Panel::screen)
                << std::endl;
}

// -----------------------------------------------------------------------------
// panels
// -----------------------------------------------------------------------------
namespace panels
{
void init(const P& max_gui_dims)
{
        TRACE_FUNC_BEGIN;

        TRACE << "Maximum allowed GUI size: "
              << max_gui_dims.x << "x" << max_gui_dims.y
              << std::endl;

        for (auto& panel : s_panels)
        {
                panel = {0, 0, 0, 0};
        }

        const int map_gui_stats_border_w = 23;

        const int map_gui_border_x0 = max_gui_dims.x - map_gui_stats_border_w;
        const int map_gui_border_y0 = 0;
        const int map_gui_border_x1 = max_gui_dims.x - 1;
        const int map_gui_border_y1 = max_gui_dims.y - 1;

        const auto nr_log_lines = (int)msg_log::g_nr_log_lines;

        const int log_border_x0 = 0;
        const int log_border_y0 = max_gui_dims.y - nr_log_lines - 2;
        const int log_border_x1 = map_gui_border_x0 - 1;
        const int log_border_y1 = max_gui_dims.y - 1;

        set_panel_area(
                Panel::map,
                0,
                0,
                max_gui_dims.x - map_gui_stats_border_w - 1,
                log_border_y0 - 1);

        set_panel_area(
                Panel::log_border,
                log_border_x0,
                log_border_y0,
                log_border_x1,
                log_border_y1);

        set_panel_area(
                Panel::log,
                log_border_x0 + 1,
                log_border_y0 + 1,
                log_border_x1 - 1,
                log_border_y1 - 1);

        set_panel_area(
                Panel::map_gui_stats_border,
                map_gui_border_x0,
                map_gui_border_y0,
                map_gui_border_x1,
                map_gui_border_y1);

        set_panel_area(
                Panel::map_gui_stats,
                map_gui_border_x0 + 1,
                map_gui_border_y0 + 1,
                map_gui_border_x1 - 1,
                map_gui_border_y1 - 1);

        finalize_screen_dims();

        constexpr int create_char_tot_w = io::g_min_nr_gui_cells_x - 2;

        constexpr int create_char_menu_w = 26;

        constexpr int create_char_descr_w =
                create_char_tot_w - create_char_menu_w - 1;

        const int screen_center_x = center_x(Panel::screen);

        const int create_char_menu_x0 =
                screen_center_x - ((create_char_tot_w / 2) - 1);

        const int create_char_menu_x1 =
                create_char_menu_x0 + create_char_menu_w - 1;

        const int create_char_descr_x0 =
                create_char_menu_x1 + 2;

        const int create_char_descr_x1 =
                create_char_descr_x0 + create_char_descr_w - 1;

        set_panel_area(
                Panel::create_char_menu,
                create_char_menu_x0,
                2,
                create_char_menu_x1,
                max_gui_dims.y - 2);

        set_panel_area(
                Panel::create_char_descr,
                create_char_descr_x0,
                2,
                create_char_descr_x1,
                max_gui_dims.y - 2);

        const int inventory_descr_w = 32;

        const int inventory_descr_x0 = max_gui_dims.x - inventory_descr_w - 1;

        const int inventory_menu_x1 = inventory_descr_x0 - 2;

        set_panel_area(
                Panel::inventory_menu,
                1,
                1,
                inventory_menu_x1,
                max_gui_dims.y - 2);

        set_panel_area(
                Panel::inventory_descr,
                inventory_descr_x0,
                1,
                max_gui_dims.x - 2,
                max_gui_dims.y - 2);

        constexpr int info_screen_w = io::g_min_nr_gui_cells_x - 2;

        const int info_screen_x0 =
                screen_center_x - ((info_screen_w / 2) - 1);

        const int info_screen_x1 =
                info_screen_x0 + info_screen_w - 1;

        set_panel_area(
                Panel::info_screen_content,
                info_screen_x0,
                1,
                info_screen_x1,
                max_gui_dims.y - 2);

        TRACE_FUNC_END;
}

R area(const Panel panel)
{
        return s_panels[(size_t)panel];
}

P dims(const Panel panel)
{
        return area(panel).dims();
}

P p0(const Panel panel)
{
        return area(panel).p0;
}

P p1(const Panel panel)
{
        return area(panel).p1;
}

int x0(const Panel panel)
{
        return area(panel).p0.x;
}

int y0(const Panel panel)
{
        return area(panel).p0.y;
}

int x1(const Panel panel)
{
        return area(panel).p1.x;
}

int y1(const Panel panel)
{
        return area(panel).p1.y;
}

int w(const Panel panel)
{
        return area(panel).w();
}

int h(const Panel panel)
{
        return area(panel).h();
}

P center(const Panel panel)
{
        const P center(
                center_x(panel),
                center_y(panel));

        return center;
}

int center_x(const Panel panel)
{
        return (x1(panel) - x0(panel)) / 2;
}

int center_y(const Panel panel)
{
        return (y1(panel) - y0(panel)) / 2;
}

}  // namespace panels

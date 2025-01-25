// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
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

        for (const R& panel : s_panels) {
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

static void set_game_state_panels(const P& max_gui_dims)
{
        constexpr int map_gui_stats_border_w = 23;

        const int map_gui_border_x0 = max_gui_dims.x - map_gui_stats_border_w;
        const int map_gui_border_y0 = 0;
        const int map_gui_border_x1 = max_gui_dims.x - 1;
        const int map_gui_border_y1 = max_gui_dims.y - 1;

        const auto nr_log_lines = (int)msg_log::g_nr_log_lines;

        const int log_x0 = 1;
        const int log_y0 = max_gui_dims.y - nr_log_lines - 1;
        const int log_x1 = map_gui_border_x0 - 1;
        const int log_y1 = max_gui_dims.y - 2;

        const int map_x0 = 0;
        const int map_y0 = 0;
        const int map_x1 = max_gui_dims.x - map_gui_stats_border_w - 1;
        const int map_y1 = log_y0 - 1;

        set_panel_area(
                Panel::map,
                map_x0,
                map_y0,
                map_x1,
                map_y1);

        set_panel_area(
                Panel::log,
                log_x0,
                log_y0,
                log_x1,
                log_y1);

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
}

static void set_create_char_state_panels(const P& max_gui_dims)
{
        constexpr int tot_w = 78;
        constexpr int menu_w = 26;
        constexpr int descr_w = tot_w - menu_w - 1;

        const int screen_center_x = panels::center_x(Panel::screen);

        const int menu_x0 = screen_center_x - ((tot_w / 2) - 1);
        const int menu_x1 = menu_x0 + menu_w - 1;

        const int descr_x0 = menu_x1 + 2;
        const int descr_x1 = descr_x0 + descr_w - 1;

        set_panel_area(
                Panel::create_char_menu,
                menu_x0,
                2,
                menu_x1,
                max_gui_dims.y - 2);

        set_panel_area(
                Panel::create_char_descr,
                descr_x0,
                2,
                descr_x1,
                max_gui_dims.y - 2);
}

static void set_options_state_panels(const P& max_gui_dims)
{
        constexpr int tot_w = 78;
        constexpr int options_w = 29;
        constexpr int values_w = 20;
        constexpr int descr_w = tot_w - options_w - values_w - 2;

        const int screen_center_x = panels::center_x(Panel::screen);

        const int options_x0 = screen_center_x - ((tot_w / 2) - 1);
        const int options_x1 = options_x0 + options_w - 1;

        const int values_x0 = options_x1 + 2;
        const int values_x1 = values_x0 + values_w - 1;

        const int descr_x0 = values_x1 + 2;
        const int descr_x1 = descr_x0 + descr_w - 1;

        set_panel_area(
                Panel::options,
                options_x0,
                2,
                options_x1,
                max_gui_dims.y - 2);

        set_panel_area(
                Panel::options_values,
                values_x0,
                2,
                values_x1,
                max_gui_dims.y - 2);

        set_panel_area(
                Panel::options_descr,
                descr_x0,
                2,
                descr_x1,
                max_gui_dims.y - 2);
}

static void set_inventory_state_panels(const P& max_gui_dims)
{
        constexpr int inventory_descr_w = 32;

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
}

static void set_info_scrreen_panel(const P& max_gui_dims)
{
        constexpr int info_screen_w = 78;

        const int screen_center_x = panels::center_x(Panel::screen);

        const int info_screen_x0 = screen_center_x - ((info_screen_w / 2) - 1);
        const int info_screen_x1 = info_screen_x0 + info_screen_w - 1;

        set_panel_area(
                Panel::info_screen_content,
                info_screen_x0,
                1,
                info_screen_x1,
                max_gui_dims.y - 2);
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

        for (R& panel : s_panels) {
                panel = {0, 0, 0, 0};
        }

        set_game_state_panels(max_gui_dims);
        finalize_screen_dims();
        set_create_char_state_panels(max_gui_dims);
        set_options_state_panels(max_gui_dims);
        set_inventory_state_panels(max_gui_dims);
        set_info_scrreen_panel(max_gui_dims);

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

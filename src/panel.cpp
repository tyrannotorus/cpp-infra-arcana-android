// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "panel.hpp"

#include <algorithm>
#include <cstddef>
#include <ostream>

#include "action_bar.hpp"
#include "config.hpp"
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

// Mirrors a panel's horizontal position within the total gui width (used for
// moving the side stats panel to the left side of the screen, e.g. for
// left/right handedness on touch devices).
static void mirror_panel_horizontal(const Panel panel, const int total_w)
{
        R& area = s_panels[(size_t)panel];

        const int new_x0 = total_w - 1 - area.p1.x;
        const int new_x1 = total_w - 1 - area.p0.x;

        area.p0.x = new_x0;
        area.p1.x = new_x1;
}

static void set_game_state_panels(const P& max_gui_dims)
{
        constexpr int map_gui_stats_border_w = 23;

        // The map container spans the entire screen - the side stats panel,
        // message log, and action bar are overlays drawn on top of it
        // (obscuring it where they have content). The side stats panel spans
        // the full screen height (as on desktop); on touch devices the
        // action bar occupies the bottom rows of the remaining width.
        const int map_gui_border_x0 = max_gui_dims.x - map_gui_stats_border_w;
        const int map_gui_border_y0 = 0;
        const int map_gui_border_x1 = max_gui_dims.x - 1;
        const int map_gui_border_y1 = max_gui_dims.y - 1;

        // Top of the action bar's reserved rows
        const int bar_y0 = std::max(1, max_gui_dims.y - action_bar::g_h_cells);

        set_panel_area(
                Panel::action_bar,
                0,
                bar_y0,
                map_gui_border_x0 - 1,
                max_gui_dims.y - 1);

        const auto nr_log_lines = (int)msg_log::g_nr_log_lines;

        // The log overlays the top of the map column, on the same side as
        // the action bar's buttons (see msg_log::draw; the extra row holds
        // the "more" prompt's own row). Inset by the screen edge margins -
        // NOTE: when the side panel is left, the log panel is MIRRORED to
        // the right screen edge, so the pre-mirror left inset must be the
        // RIGHT screen margin.
        const int log_edge_margin =
                config::is_side_panel_left()
                ? panels::g_screen_margin_right
                : panels::g_screen_margin_left;

        const int log_x0 = 1 + log_edge_margin;
        const int log_y0 = panels::g_screen_margin_top;
        const int log_x1 = map_gui_border_x0 - 1;
        const int log_y1 = log_y0 + nr_log_lines;

        const int map_x0 = 0;
        const int map_y0 = 0;
        const int map_x1 = max_gui_dims.x - 1;
        const int map_y1 = max_gui_dims.y - 1;

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

        if (config::is_side_panel_left()) {
                // NOTE: The map spans the whole screen and needs no
                // mirroring
                mirror_panel_horizontal(Panel::log, max_gui_dims.x);

                mirror_panel_horizontal(
                        Panel::map_gui_stats_border,
                        max_gui_dims.x);

                mirror_panel_horizontal(
                        Panel::map_gui_stats,
                        max_gui_dims.x);

                // The action bar sits under the map/log column, and moves
                // along with it
                mirror_panel_horizontal(Panel::action_bar, max_gui_dims.x);
        }
}

// The two column layout of the description menu pages (character creation,
// the manual, ...): the list in the left column, description text of the
// marked entry in the right one (see MenuDescrPageState)
static void set_menu_descr_page_panels(const P& max_gui_dims)
{
        constexpr int tot_w = 78;
        constexpr int menu_w = 26;
        constexpr int descr_w = tot_w - menu_w - 1;

        const int screen_center_x = panels::center_x(Panel::screen);

        const int menu_x0 = screen_center_x - ((tot_w / 2) - 1);
        const int menu_x1 = menu_x0 + menu_w - 1;

        const int descr_x0 = menu_x1 + 2;
        const int descr_x1 = descr_x0 + descr_w - 1;

        // NOTE: One row further down than the border box, so that neither
        // the list nor the description is flush against it - both columns
        // then simply start at the top of their panel
        const int y0 = panels::g_screen_margin_top + 2;
        const int y1 = max_gui_dims.y - 2 - panels::g_screen_margin_bottom;

        set_panel_area(
                Panel::menu_descr_list,
                menu_x0,
                y0,
                menu_x1,
                y1);

        set_panel_area(
                Panel::menu_descr_text,
                descr_x0,
                y0,
                descr_x1,
                y1);
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

        const int y0 = panels::g_screen_margin_top + 1;
        const int y1 = max_gui_dims.y - 2 - panels::g_screen_margin_bottom;

        set_panel_area(
                Panel::options,
                options_x0,
                y0,
                options_x1,
                y1);

        set_panel_area(
                Panel::options_values,
                values_x0,
                y0,
                values_x1,
                y1);

        set_panel_area(
                Panel::options_descr,
                descr_x0,
                y0,
                descr_x1,
                y1);
}

static void set_inventory_state_panels(const P& max_gui_dims)
{
        // The item rows are just names now (the weight column moved into
        // the description text), so the list column can be narrower and
        // the description wider - about half the screen, but never so
        // wide that a slot row ("Ready  An M1911 Colt (9.0 +0% hit)
        // (7/7)") no longer fits beside it
        const int inventory_descr_w =
                std::clamp((max_gui_dims.x / 2) - 5, 32, 40);

        const int inventory_descr_x0 = max_gui_dims.x - inventory_descr_w - 1;
        const int inventory_menu_x1 = inventory_descr_x0 - 2;

        // NOTE: Inset by one row from the border box at the TOP and at the
        // BOTTOM, so that nothing (the item list, the description, the
        // item action pins in the bottom corner) is flush against it
        const int y0 = panels::g_screen_margin_top + 2;
        const int y1 = max_gui_dims.y - 3 - panels::g_screen_margin_bottom;

        set_panel_area(
                Panel::inventory_menu,
                panels::g_screen_margin_left + 1,
                y0,
                inventory_menu_x1,
                y1);

        set_panel_area(
                Panel::inventory_descr,
                inventory_descr_x0,
                y0,
                max_gui_dims.x - 2 - panels::g_screen_margin_right,
                y1);
}

static void set_info_scrreen_panel(const P& max_gui_dims)
{
        constexpr int info_screen_w = 78;

        const int screen_center_x = panels::center_x(Panel::screen);

        const int info_screen_x0 = screen_center_x - ((info_screen_w / 2) - 1);
        const int info_screen_x1 = info_screen_x0 + info_screen_w - 1;

        // NOTE: Inset by one row from the border box at the TOP and at the
        // BOTTOM (the same inner margin as the inventory screen), so that
        // the content is never flush against the border
        set_panel_area(
                Panel::info_screen_content,
                info_screen_x0,
                panels::g_screen_margin_top + 2,
                info_screen_x1,
                max_gui_dims.y - 3 - panels::g_screen_margin_bottom);
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

        // NOTE: The action bar (touch devices) only shows during play and
        // never covers menu screens - all menus use the full screen height.
        set_game_state_panels(max_gui_dims);
        finalize_screen_dims();
        set_menu_descr_page_panels(max_gui_dims);
        set_options_state_panels(max_gui_dims);
        set_inventory_state_panels(max_gui_dims);
        set_info_scrreen_panel(max_gui_dims);

        TRACE_FUNC_END;
}

R area(const Panel panel)
{
        return s_panels[(size_t)panel];
}

R screen_box_area()
{
        const R screen = area(Panel::screen);

        return {
                screen.p0.x + g_screen_margin_left,
                screen.p0.y + g_screen_margin_top,
                screen.p1.x - g_screen_margin_right,
                screen.p1.y - g_screen_margin_bottom};
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

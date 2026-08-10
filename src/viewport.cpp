// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "viewport.hpp"

#include <algorithm>
#include <cstdlib>

#include "actor.hpp"
#include "config.hpp"
#include "global.hpp"
#include "io.hpp"
#include "io_display.hpp"
#include "io_internal.hpp"
#include "map.hpp"
#include "panel.hpp"
#include "pos.hpp"
#include "rect.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static P s_p0;

// Manual camera pan state (see viewport::pan). While active, the view does
// not follow the player - until the player moves from the anchor position.
static bool s_pan_active = false;
static P s_pan_anchor_player_pos;

// Always move viewport if focused position is closer to the edge than this:
constexpr static int s_min_required_viewport_edge_dist = g_fov_radi_int + 1;

static P get_view_dims()
{
        const auto map_panel_gui_dims = panels::dims(Panel::map);

        return io::gui_to_map_coords(map_panel_gui_dims);
}

// Width in map cells of the side stats panel column
static int side_panel_cells()
{
        const int map_cell_w = config::map_cell_px_w();

        const int side_px_w = io::panel_px_w(Panel::map_gui_stats_border);

        return (side_px_w + map_cell_w - 1) / map_cell_w;
}

// The view's centering point (view cell coordinates). The map's
// "centeredness" on the player is biased left or right ONLY by the user's
// handedness - the side the stats panel is on. It must NEVER depend on
// any other UI geometry (the log, the action bar): those overlays move
// around, and tying the map centering to them breaks it (this happened
// once when the log moved from the bottom to the top of the screen).
static P visible_view_center()
{
        const auto view_dims = get_view_dims();

        const int side_cells = side_panel_cells();

        int x_lo = 0;
        int x_hi = view_dims.x;

        if (config::is_side_panel_left()) {
                x_lo += side_cells;
        }
        else {
                x_hi -= side_cells;
        }

        return {
                x_lo + ((x_hi - x_lo) / 2),
                view_dims.y / 2};
}

// -----------------------------------------------------------------------------
// viewport
// -----------------------------------------------------------------------------
namespace viewport
{
R get_map_view_area()
{
        auto view_dims = get_view_dims();

        const R map_area(s_p0, s_p0 + view_dims - 1);

        return map_area;
}

void show(const P& map_pos, const ForceCentering force_centering)
{
        const auto map_view_area = get_map_view_area();

        const auto smallest_dist_hor =
                std::min(
                        map_pos.x - map_view_area.p0.x,
                        map_view_area.p1.x - map_pos.x);

        const auto smallest_dist_ver =
                std::min(
                        map_pos.y - map_view_area.p0.y,
                        map_view_area.p1.y - map_pos.y);

        const auto centered_pos = map_pos - visible_view_center();

        const P p0_before = s_p0;

        if (force_centering == ForceCentering::yes) {
                // A forced centering (aiming outside the view, entering a new
                // level, ...) always ends any manual camera pan
                s_pan_active = false;
        }

        if (config::always_center_view_on_player() ||
            (force_centering == ForceCentering::yes)) {
                // Always center the view (both X/Y axis)
                s_p0 = centered_pos;
        }
        else {
                // Only center if needed (for X/Y axis separately)
                if (smallest_dist_hor < s_min_required_viewport_edge_dist) {
                        s_p0.x = centered_pos.x;
                }

                if (smallest_dist_ver < s_min_required_viewport_edge_dist) {
                        s_p0.y = centered_pos.y;
                }
        }

        // TODO: Instead of clearing all flash animations, it would be better to update their
        // positions. This prevents showing an attack flash animation the player if they are knocked
        // back (typically changes the viewport).
        if (s_p0 != p0_before) {
                io::clear_all_flash_animations();

                // A single step (following the player around) slides the
                // camera with a quick tween instead of snapping tile to
                // tile. Anything longer (new level, entering/leaving
                // targeting, snapping back after a manual pan) stays
                // instant - and could not be shown anyway: the map display
                // carries exactly ONE cell of overscan, which is all the
                // scroll offset the composite can sample (see io_display).
                const P delta = s_p0 - p0_before;

                if ((std::abs(delta.x) <= 1) && (std::abs(delta.y) <= 1)) {
                        io::start_map_follow_tween(
                                P(-delta.x * config::map_cell_px_w(),
                                  -delta.y * config::map_cell_px_h()));
                }
        }
}

void pan(const P& cell_delta)
{
        if (map::w() == 0) {
                return;
        }

        const auto limits = pan_limits();

        // NOTE: The automatic centering can leave the view origin outside
        // the pan limits (centering on a player near a map edge, with the
        // fullscreen view being larger than the map remainder). The current
        // position is therefore always included in the allowed range here -
        // panning can always move back INTO the limits, just never further
        // out of them.
        s_p0.x =
                std::clamp(
                        s_p0.x + cell_delta.x,
                        std::min(limits.p0.x, s_p0.x),
                        std::max(limits.p1.x, s_p0.x));

        s_p0.y =
                std::clamp(
                        s_p0.y + cell_delta.y,
                        std::min(limits.p0.y, s_p0.y),
                        std::max(limits.p1.y, s_p0.y));

        if (!s_pan_active) {
                s_pan_active = true;

                s_pan_anchor_player_pos = map::g_player->m_pos;
        }

        // The flash animation positions are relative to the view
        io::clear_all_flash_animations();
}

bool should_auto_center()
{
        if (!s_pan_active) {
                return true;
        }

        if (map::g_player->m_pos != s_pan_anchor_player_pos) {
                // The player has moved - drop the manual pan, and snap the
                // camera back to following the player
                s_pan_active = false;

                return true;
        }

        return false;
}

void end_pan()
{
        s_pan_active = false;
}

void end_pan_and_center_on_player()
{
        // NOTE: The centering must be forced - the view may be off center
        // (or even beyond the map edges) with the player still inside it,
        // and show() alone only guarantees that the position is visible
        end_pan();

        show(map::g_player->m_pos, ForceCentering::yes);
}

bool is_pan_active()
{
        return s_pan_active;
}

P center_map_pos()
{
        return s_p0 + visible_view_center();
}

P origin()
{
        return s_p0;
}

R pan_limits()
{
        // Any map cell can be brought to the view's centering point (the
        // look "pin") - the view may scroll beyond the map edges, the
        // centering point must just stay on a map cell
        const auto center = visible_view_center();

        return {
                P(0, 0) - center,
                P(map::w() - 1, map::h() - 1) - center};
}

bool is_in_view(const P& map_pos)
{
        return get_map_view_area().is_pos_inside(map_pos);
}

bool is_in_drawn_view(const P& map_pos)
{
        const auto view = get_map_view_area();

        const R drawn(view.p0 - 2, view.p1 + 2);

        return drawn.is_pos_inside(map_pos);
}

P to_view_pos(const P& map_pos)
{
        return map_pos - s_p0;
}

P to_map_pos(const P& view_pos)
{
        return view_pos + s_p0;
}

}  // namespace viewport

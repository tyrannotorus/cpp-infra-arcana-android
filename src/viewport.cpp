// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "viewport.hpp"

#include <algorithm>

#include "actor.hpp"
#include "config.hpp"
#include "dpad.hpp"
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

// Whole cells of camera lag baked into s_p0 (see place_view_origin) - what
// lets the camera glide further than the one cell of overscan
static P s_camera_whole_cells = {0, 0};

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

// Width in map cells of the movement pad's column, or zero when the pad is
// not the movement mode (see dpad::reserved_px_w)
static int dpad_cells()
{
        const int map_cell_w = config::map_cell_px_w();

        return (dpad::reserved_px_w() + map_cell_w - 1) / map_cell_w;
}

// The view's centering point (view cell coordinates). The map's
// "centeredness" on the player is biased left or right ONLY by the two
// things that permanently take a column of the screen: the user's
// handedness - the side the stats panel is on - and, when it is the chosen
// movement mode, the pad on the opposite side. It must NEVER depend on any
// other UI geometry (the log, the action bar), nor on where the pad has
// been dragged or how far it has been scaled: those overlays move around,
// and tying the map centering to them breaks it (this happened once when
// the log moved from the bottom to the top of the screen).
static P visible_view_center()
{
        const auto view_dims = get_view_dims();

        const int side_cells = side_panel_cells();

        // The pad sits on the edge opposite the stats panel, so handedness
        // alone decides which side each of them takes
        const int pad_cells = dpad_cells();

        int x_lo = 0;
        int x_hi = view_dims.x;

        if (config::is_side_panel_left()) {
                x_lo += side_cells;
                x_hi -= pad_cells;
        }
        else {
                x_lo += pad_cells;
                x_hi -= side_cells;
        }

        return {
                x_lo + ((x_hi - x_lo) / 2),
                view_dims.y / 2};
}

// The framing the camera is heading for
static P camera_target()
{
        return s_p0 - s_camera_whole_cells;
}

// Draws the view where the camera IS: the target, offset by the whole cells
// it is behind. The composite carries the sub-cell remainder.
static void place_view_origin(const P& target)
{
        const P p0_before = s_p0;

        s_camera_whole_cells = io::map_follow_whole_cells();

        io::set_map_follow_drawn_whole_cells(s_camera_whole_cells);

        s_p0 = target + s_camera_whole_cells;

        // TODO: Instead of clearing all flash animations, it would be better to update their
        // positions. This prevents showing an attack flash animation the player if they are knocked
        // back (typically changes the viewport).
        if (s_p0 != p0_before) {
                io::clear_all_flash_animations();
        }
}

// Aims the camera at a new target. It stays where it is and becomes that
// much further behind, easing up per present (see io::offset_map_follow).
static void glide_view_to(const P& target)
{
        const P target_delta = target - camera_target();

        if (target_delta != P(0, 0)) {
                io::offset_map_follow(
                        P(-target_delta.x * config::map_cell_px_w(),
                          -target_delta.y * config::map_cell_px_h()));
        }

        place_view_origin(target);
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
        if (force_centering == ForceCentering::yes) {
                // A forced centering (aiming outside the view, returning to
                // the player after a manual pan, ...) ends any manual pan
                s_pan_active = false;
        }

        // The view is always centered on the focused position: a phone screen
        // shows too few cells to let the player walk near its edge
        glide_view_to(map_pos - visible_view_center());
}

void cut_to(const P& map_pos)
{
        s_pan_active = false;

        io::cancel_map_follow();

        place_view_origin(map_pos - visible_view_center());
}

void advance_camera()
{
        place_view_origin(camera_target());
}

void cut_camera()
{
        io::cancel_map_follow();

        // s_p0 keeps its whole cells - the camera stays where it is drawn
        s_camera_whole_cells = {0, 0};

        io::set_map_follow_drawn_whole_cells({0, 0});
}

bool is_camera_redraw_needed()
{
        return io::map_follow_whole_cells() != s_camera_whole_cells;
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

        // The finger owns the origin now - a glide in flight would fight it
        cut_camera();

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
                // The player has moved - drop the manual pan, and let the
                // camera glide back to following the player
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
        end_pan();

        // Not a cut - the camera glides back from wherever the pan left it
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

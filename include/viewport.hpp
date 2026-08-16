// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef VIEWPORT_HPP
#define VIEWPORT_HPP

struct P;
struct R;

namespace viewport
{
enum class ForceCentering
{
        no,
        yes
};

R get_map_view_area();

// Centers the view on the given position. force_centering additionally ends
// any manual pan.
//
// The camera GLIDES there: the view is drawn where the camera currently is,
// not where it is going (see io::offset_map_follow). Use cut_to for a jump.
void show(
        const P& map_pos,
        ForceCentering force_centering = ForceCentering::no);

// Centers on the position at once, no glide - a new level, a teleport,
// placing the aim reticle.
void cut_to(const P& map_pos);

// Whether the centering point is biased away from the overlays that take a
// column (the stats panel and the movement pad). Turn it off once they have
// left the screen, and the camera glides the player to dead center.
void set_center_bias_enabled(bool is_enabled);

// Brings the drawn view origin up to a gliding camera, same target. Driven
// by the render loop, since not every state re-centers the view every frame
// (the aim marker only does so when its position leaves the view).
void advance_camera();

// Drops any camera lag where the view is drawn - the camera stops where it
// stands. For when something else takes the map over (manual panning, the
// interface changing hands).
void cut_camera();

// Whether the camera has glided into another cell than the map display was
// drawn with, so the map has to be redrawn at the new origin.
bool is_camera_redraw_needed();

// Manual camera panning (dragging on touch devices - the "look"
// interaction, see center_map_pos). Offsets the view by a number of map
// cells, and suppresses automatic re-centering on the player until the
// player moves (see should_auto_center), or until a forced centering or a
// cut occurs (e.g. aiming at a position outside the view, or a new level).
//
// The pan limits allow bringing ANY map cell to the view's centering point
// - the view may scroll beyond the map edges (showing blackness); the only
// constraint is that the cell at the centering point stays a map cell.
void pan(const P& cell_delta);

// Whether a manual pan is currently active (the view is not following the
// player) - i.e. whether the look "pin" is engaged
bool is_pan_active();

// The map cell at the view's centering point (the same point the camera
// places the player on when centering) - the "pin" of look panning
P center_map_pos();

// Whether the view should currently follow the player. Returns true normally.
// While a manual pan is active it returns false - until the player has moved
// from the position where the pan began, at which point the pan is dropped
// and the camera glides back to the player (returning true again).
bool should_auto_center();

// Immediately drops any active manual pan - the camera follows the player
// again from the next draw (e.g. when the side panel changes sides, the
// map glides back to the player alongside the slide animation).
void end_pan();

// Drops any active manual pan AND glides back to centered on the player -
// looking around is over (see game_commands::handle: any action other than
// describing the pinned cell ends drag-to-look)
void end_pan_and_center_on_player();

// The current view origin (map position of the top left view cell)
P origin();

// The range the view origin may be panned within (p0 = minimum, p1 =
// maximum): the map-derived range, extended so that map content obscured by
// the overlays (side stats panel, log, action bar) can be panned into the
// open area. The automatic centering may still place the origin OUTSIDE
// this range - pan() then allows moving back in, never further out.
R pan_limits();

bool is_in_view(const P& map_pos);

// Whether a map position is within the DRAWN view area (the view expanded
// by a small margin for partial cells and the sub-cell scroll overscan).
// Used to cull map drawing - the draw functions iterate map content and
// must skip everything far outside the view instead of relying on
// clipping, which still costs a draw call per cell.
bool is_in_drawn_view(const P& map_pos);

P to_view_pos(const P& map_pos);

P to_map_pos(const P& view_pos);

}  // namespace viewport

#endif  // VIEWPORT_HPP

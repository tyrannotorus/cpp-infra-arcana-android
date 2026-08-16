// =============================================================================
// Copyright Werewolf Camp
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "dpad.hpp"

#include <algorithm>
#include <cmath>

#include "SDL_keycode.h"
#include "action_bar.hpp"
#include "colors.hpp"
#include "config.hpp"
#include "context_pins.hpp"
#include "io.hpp"
#include "io_display.hpp"
#include "io_icons.hpp"
#include "panel.hpp"
#include "pos.hpp"
#include "rect.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------

// The pad's cells, in reading order. The arrow glyph is drawn turned (see
// io::draw_icon), so all eight directions come from one icon.
struct Cell
{
        int key;
        const char* icon;
        double angle;
};

static const Cell s_cells[9] = {
        {SDLK_KP_7, "arrow_forward", -135.0},
        {SDLK_KP_8, "arrow_forward", -90.0},
        {SDLK_KP_9, "arrow_forward", -45.0},
        {SDLK_KP_4, "arrow_forward", 180.0},
        // Waiting one turn, the same command (and icon) as the action bar's
        // "wait" button - the middle of a movement pad is where standing
        // still belongs
        {SDLK_KP_5, "hourglass_empty", 0.0},
        {SDLK_KP_6, "arrow_forward", 0.0},
        {SDLK_KP_1, "arrow_forward", 135.0},
        {SDLK_KP_2, "arrow_forward", 90.0},
        {SDLK_KP_3, "arrow_forward", 45.0}};

// A ceiling on scaling up, keeping the pad within reach of the thumb
// holding the device (the floor is a bar button, see min_scale_pct).
static const int s_max_scale_pct = 150;

// User placement: an offset in logical screen pixels from the default slot,
// and a scale in percent of the pad's default size. Loaded from the config
// file on first use, and written back when a drag ends.
static bool s_is_placement_loaded = false;
static P s_offset_px(0, 0);
static int s_scale_pct = 100;

// Arranging the pad (see begin_edit_mode)
static bool s_is_edit_active = false;
static bool s_is_edit_drag_active = false;
static bool s_is_resizing = false;
static P s_drag_start_px(0, 0);
static P s_drag_start_offset_px(0, 0);
static int s_drag_start_grid_px = 0;

// Whether the pad sits in its default slot, so that clicking into it can be
// felt exactly once (see move_from)
static bool s_is_snapped_to_default = true;

// Whether the pad is the chosen movement mode. NOTE: The setting, not what
// is on screen - the map reserves the pad's column for as long as the mode
// is on (see reserved_px_w), while the pad itself only shows during play
// (see is_visible).
static bool is_movement_mode()
{
        return config::is_dpad_movement();
}

// Whether the pad is anchored to the right screen edge - the edge the action
// bar's buttons flow from, which is the one opposite the side stats panel
static bool is_anchored_right()
{
        return config::is_side_panel_left();
}

// A cell of the pad is never smaller than an action bar button
static int cell_min_px()
{
        return action_bar::button_px_dims().x;
}

// The pad at 100%, which is what it starts out at: cells a quarter larger
// than a bar button. It is the thing the thumb reaches for constantly, so
// it is bigger by default than the buttons it borrows its chrome from -
// and the scale range then runs from exactly a bar button (80%) to nearly
// twice one.
static int base_grid_px()
{
        return ((cell_min_px() * 5) / 4) * 3;
}

static int min_scale_pct()
{
        const int base = base_grid_px();

        // Rounded up, so that a cell cannot land a pixel under a bar button
        return ((cell_min_px() * 3 * 100) + base - 1) / base;
}

static void load_placement_if_needed()
{
        if (s_is_placement_loaded) {
                return;
        }

        s_is_placement_loaded = true;

        s_offset_px.set(config::dpad_offset_px_x(), config::dpad_offset_px_y());

        s_scale_pct = config::dpad_scale_pct();
}

static int scale_pct()
{
        load_placement_if_needed();

        return std::clamp(s_scale_pct, min_scale_pct(), s_max_scale_pct);
}

static int grid_px_w()
{
        return (base_grid_px() * scale_pct()) / 100;
}

static int max_grid_px_w()
{
        return (base_grid_px() * s_max_scale_pct) / 100;
}

// Half the square grab area centred on the resize handle. The pad carries
// exactly this as a transparent margin on its top and outer side, so that
// the whole square is within the pad's own area (a touch outside it belongs
// to whatever is under it).
static int grab_margin_px()
{
        return (cell_min_px() * 2) / 3;
}

// How near the default slot a drag has to land to click back into it: half
// a bar button, which is about a fingertip's worth of slack
static int snap_to_default_px()
{
        return cell_min_px() / 2;
}

// The pad's default bottom edge (exclusive): it clears the action bar's
// buttons AND the context pin row above them, plus a gap. The pins are
// matched against a tap before the pad is (see io_input), so a pad sitting
// on them would have dead cells whenever a pin is up.
static int default_bottom_px()
{
        return action_bar::occupied_top_px() -
                ((context_pins::rows_above_bar() + 1) *
                 config::gui_cell_px_h());
}

// The 3x3 grid at its default slot, at the current scale: in the bottom
// corner the action bar's buttons flow from, its outer edge flush with
// theirs
static R default_grid_px_rect()
{
        const auto bar_px_rect =
                io::panel_logical_px_rect(Panel::action_bar);

        const int size = grid_px_w();

        const int y1 = default_bottom_px() - 1;

        const int x0 =
                is_anchored_right()
                ? (bar_px_rect.p1.x - size + 1)
                : bar_px_rect.p0.x;

        return {P(x0, y1 - size + 1), P(x0 + size - 1, y1)};
}

// The grid plus the margin holding the resize handle's grab square: the
// pad's whole area, and what its display texture covers
static R view_px_rect(const R& grid_px_rect)
{
        const int margin = grab_margin_px();

        return {
                grid_px_rect.p0.with_offsets(
                        is_anchored_right() ? -margin : 0,
                        -margin),
                grid_px_rect.p1.with_offsets(
                        is_anchored_right() ? 0 : margin,
                        0)};
}

// Keeps the pad fully on screen
static P clamped_offset(const P& offset)
{
        const auto screen_px_rect =
                io::panel_logical_px_rect(Panel::screen);

        const R view = view_px_rect(default_grid_px_rect());

        // A pad larger than the screen cannot be placed legally at all -
        // pin it to the top left corner rather than clamping to an empty
        // range
        const int min_x = std::min(
                screen_px_rect.p0.x - view.p0.x,
                screen_px_rect.p1.x - view.p1.x);

        const int min_y = std::min(
                screen_px_rect.p0.y - view.p0.y,
                screen_px_rect.p1.y - view.p1.y);

        return {
                std::clamp(
                        offset.x,
                        min_x,
                        std::max(min_x, screen_px_rect.p1.x - view.p1.x)),
                std::clamp(
                        offset.y,
                        min_y,
                        std::max(min_y, screen_px_rect.p1.y - view.p1.y))};
}

static P placement_offset_px()
{
        load_placement_if_needed();

        return clamped_offset(s_offset_px);
}

// The grid where it actually is: the default slot moved by the player's own
// placement
static R grid_px_rect()
{
        const R grid = default_grid_px_rect();

        const P offset = placement_offset_px();

        return {grid.p0.with_offsets(offset), grid.p1.with_offsets(offset)};
}

// Centred on the corner it drags, the way transform handles sit on a
// selection box: the top corner AWAY from the anchored edge, so that
// dragging outward always grows the pad
static P handle_center_px(const R& grid)
{
        return {is_anchored_right() ? grid.p0.x : grid.p1.x, grid.p0.y};
}

// The square a finger has to come down in to scale the pad instead of
// moving it. Capped at a third of the grid, so that it can never swallow
// the pad it is meant to be a corner of.
static R handle_grab_px_rect(const R& grid)
{
        const P center = handle_center_px(grid);

        const int half = std::min(grab_margin_px(), grid.w() / 3);

        return {center.with_offsets(-half, -half),
                center.with_offsets(half, half)};
}

static void persist_placement()
{
        load_placement_if_needed();

        config::set_dpad_placement(
                s_offset_px.x,
                s_offset_px.y,
                s_scale_pct);
}

// Starts carrying the pad from a finger position, either moving or scaling
// it (see edit_drag_move)
static void begin_drag(const P& logical_px, const bool is_resizing)
{
        const P offset = placement_offset_px();

        s_is_edit_drag_active = true;
        s_is_resizing = is_resizing;
        s_drag_start_px = logical_px;
        s_drag_start_offset_px = offset;
        s_drag_start_grid_px = grid_px_w();
        s_is_snapped_to_default = (offset == P(0, 0));
}

// Moving the pad with the finger, clicking into the default slot when it
// lands near it
static void move_from(const P& logical_px)
{
        const P offset =
                s_drag_start_offset_px.with_offsets(
                        logical_px - s_drag_start_px);

        s_offset_px = clamped_offset(offset);

        const int snap_px = snap_to_default_px();

        // NOTE: Every move recomputes the offset from the finger, so
        // zeroing it here cannot trap the drag in the slot
        const bool is_within =
                ((s_offset_px.x * s_offset_px.x) +
                 (s_offset_px.y * s_offset_px.y)) < (snap_px * snap_px);

        if (is_within) {
                s_offset_px.set(0, 0);
        }

        if (is_within != s_is_snapped_to_default) {
                s_is_snapped_to_default = is_within;

                if (is_within) {
                        io::haptic_feedback(io::HapticFeedback::tick);
                }
        }
}

// Scaling the pad about its anchored corner: dragging the handle away from
// that corner grows it. Both axes feed one edge length, since the pad is
// square.
static void resize_from(const P& logical_px)
{
        const int away_x =
                is_anchored_right()
                ? (s_drag_start_px.x - logical_px.x)
                : (logical_px.x - s_drag_start_px.x);

        const int away_y = s_drag_start_px.y - logical_px.y;

        const int size = s_drag_start_grid_px + ((away_x + away_y) / 2);

        s_scale_pct =
                std::clamp(
                        (int)std::lround(
                                (double)size * 100.0 /
                                (double)base_grid_px()),
                        min_scale_pct(),
                        s_max_scale_pct);
}

// -----------------------------------------------------------------------------
// dpad
// -----------------------------------------------------------------------------
namespace dpad
{
bool is_visible()
{
        return is_movement_mode() && action_bar::is_visible();
}

R display_px_rect()
{
        const P p0 = origin_px();

        // Sized to the LARGEST the pad can be scaled to - the texture must
        // outlive every resize, and is only recreated with the other
        // display textures
        const P dims(
                max_grid_px_w() + grab_margin_px(),
                max_grid_px_w() + grab_margin_px());

        return {p0, p0 + dims - 1};
}

P origin_px()
{
        return view_px_rect(grid_px_rect()).p0;
}

int reserved_px_w()
{
        if (!is_movement_mode()) {
                return 0;
        }

        return base_grid_px();
}

void draw()
{
        if (!is_visible()) {
                // Hiding the pad mid-edit would strand the input layer
                // waiting for a tap that puts down a pad nobody can see
                exit_edit_mode();

                return;
        }

        const R grid = grid_px_rect();

        io::set_display(io::Display::dpad);

        // The pad's texture travels with it - the display's screen origin
        // is where the pad is right now, and its content always starts at
        // the texture's own origin
        io::set_display_px_origin(io::Display::dpad, view_px_rect(grid).p0);

        // Cell edges are computed from the grid's own edges rather than
        // from a cell width, so that rounding cannot leave a seam or spill
        // a pixel past the last column
        auto edge_x = [&grid](const int i) {
                return grid.p0.x + ((grid.w() * i) / 3);
        };

        auto edge_y = [&grid](const int i) {
                return grid.p0.y + ((grid.h() * i) / 3);
        };

        if (s_is_edit_active) {
                // Worn only while arranging: the pad is otherwise just its
                // buttons, which is unreadable as one object to pick up and
                // move
                io::draw_rectangle_filled(grid, colors::extra_dark_gray());

                for (int i = 1; i < 3; ++i) {
                        io::draw_rectangle_filled(
                                {P(edge_x(i), grid.p0.y),
                                 P(edge_x(i), grid.p1.y)},
                                colors::menu_highlight());

                        io::draw_rectangle_filled(
                                {P(grid.p0.x, edge_y(i)),
                                 P(grid.p1.x, edge_y(i))},
                                colors::menu_highlight());
                }

                io::draw_rectangle(grid, colors::menu_highlight());
        }

        for (int i = 0; i < 9; ++i) {
                const int col = i % 3;
                const int row = i / 3;

                const R cell_px_rect(
                        P(edge_x(col), edge_y(row)),
                        P(edge_x(col + 1) - 1, edge_y(row + 1) - 1));

                action_bar::draw_button_face(cell_px_rect, false);

                // Fixed, not proportional: the glyphs stay the size of the
                // action bar's however far the pad is scaled
                io::draw_icon(
                        s_cells[i].icon,
                        cell_px_rect.center(),
                        action_bar::icon_px_size(),
                        colors::light_sepia(),
                        s_cells[i].angle);
        }

        if (!s_is_edit_active) {
                return;
        }

        // The resize handle: a bracket hugging the corner it scales from,
        // just OUTSIDE the grid (in the margin its grab square lives in).
        // Inside, it would read as decoration of the corner key rather than
        // as a control of the pad as a whole.
        const P corner = handle_center_px(grid);

        const int reach = grab_margin_px() / 2;

        const int thickness = std::max(2, reach / 4);

        // Outward from the anchored edge: the handle sits on the pad's open
        // side, which is the side dragging it grows the pad toward
        const int x_dir = is_anchored_right() ? -1 : 1;

        const int arm_x0 = corner.x + (x_dir * thickness);

        auto arm = [](const int x_a, const int y_a, const int x_b, const int y_b) {
                return R(
                        P(std::min(x_a, x_b), std::min(y_a, y_b)),
                        P(std::max(x_a, x_b), std::max(y_a, y_b)));
        };

        io::draw_rectangle_filled(
                arm(arm_x0,
                    corner.y - thickness,
                    corner.x - (x_dir * reach),
                    corner.y - 1),
                colors::menu_highlight());

        io::draw_rectangle_filled(
                arm(arm_x0,
                    corner.y - thickness,
                    corner.x + x_dir,
                    corner.y + reach - 1),
                colors::menu_highlight());
}

int key_at(const P& logical_px)
{
        if (!is_visible() || s_is_edit_active) {
                return 0;
        }

        const R grid = grid_px_rect();

        if (!grid.is_pos_inside(logical_px)) {
                return 0;
        }

        const int col =
                std::clamp(((logical_px.x - grid.p0.x) * 3) / grid.w(), 0, 2);

        const int row =
                std::clamp(((logical_px.y - grid.p0.y) * 3) / grid.h(), 0, 2);

        return s_cells[(row * 3) + col].key;
}

bool is_pos_on_pad(const P& logical_px)
{
        if (!is_visible()) {
                return false;
        }

        return view_px_rect(grid_px_rect()).is_pos_inside(logical_px);
}

void begin_edit_mode(const P& logical_px)
{
        if (!is_visible()) {
                return;
        }

        s_is_edit_active = true;

        // Nothing has visibly happened yet - the pad is still where it was,
        // and only starts following once the finger moves
        io::haptic_feedback(io::HapticFeedback::press);

        // The finger that lifted the pad carries it straight on - it is
        // already down, and moving it is what a held finger is for
        begin_drag(logical_px, false);
}

bool is_edit_active()
{
        return s_is_edit_active;
}

void begin_edit_drag(const P& logical_px)
{
        if (!s_is_edit_active) {
                return;
        }

        // Scaling if the finger came down on the corner handle, moving
        // otherwise
        begin_drag(
                logical_px,
                handle_grab_px_rect(grid_px_rect())
                        .is_pos_inside(logical_px));
}

void edit_drag_move(const P& logical_px)
{
        if (!s_is_edit_drag_active) {
                return;
        }

        if (s_is_resizing) {
                resize_from(logical_px);
        }
        else {
                move_from(logical_px);
        }
}

void end_edit_drag()
{
        if (!s_is_edit_drag_active) {
                return;
        }

        s_is_edit_drag_active = false;

        persist_placement();
}

void exit_edit_mode()
{
        if (!s_is_edit_active) {
                return;
        }

        s_is_edit_active = false;
        s_is_edit_drag_active = false;
        s_is_resizing = false;

        persist_placement();
}

void mirror_placement()
{
        load_placement_if_needed();

        s_offset_px.x = -s_offset_px.x;

        persist_placement();
}

}  // namespace dpad

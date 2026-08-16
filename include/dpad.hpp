// =============================================================================
// Copyright Werewolf Camp
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef DPAD_HPP
#define DPAD_HPP

struct P;
struct R;

// -----------------------------------------------------------------------------
// An optional on-screen movement pad: a 3x3 grid of buttons sending the
// movement keys the game already understands (numpad 1-9), with waiting one
// turn in the middle. It is shown INSTEAD of swipe-to-move, chosen with the
// "Movement" option (see config's MovementModeOption) - the two never
// coexist, or a swipe over the map would move the player while the thumb is
// resting on the pad.
//
// The pad wears the action bar's own chrome, and its cells are never smaller
// than a bar button (see action_bar's button metrics): it is one more part
// of the same control set, not a separate widget. It sits at the bottom
// corner opposite the side stats panel - the same corner the bar's buttons
// flow from - clearing the bar and the context pin row above it, and moves
// to the other corner with the rest of the interface when the player
// changes hands.
//
// Holding a finger on it starts arranging it: dragging the body moves it,
// dragging the corner handle scales it, and a tap anywhere else puts it
// down. Its placement persists in the config file.
// -----------------------------------------------------------------------------
namespace dpad
{
// The pad is only shown (and tappable) while actually playing - the same
// states that show the action bar. This is also what decides whether a
// swipe over the map moves the player: it does NOT while the pad is up, and
// it always does everywhere else (menus are browsed by swiping, and the
// movement mode has no say over that).
bool is_visible();

void draw();

// The key of the pad cell at a position in logical screen pixels, or 0 if
// there is no cell there. Returns 0 while the pad is being arranged - its
// cells are inert then, so that a finger placing it cannot also move the
// player.
int key_at(const P& logical_px);

// Whether a position falls on the pad, including the margin its resize
// handle reaches into. Gestures starting there belong to the pad - they
// never pan the map, and never move the player.
bool is_pos_on_pad(const P& logical_px);

// --- Arranging the pad ---
//
// A held finger lifts the pad into an edit mode: the grid is outlined and
// its cells stop responding, the body follows the finger, and a handle at
// the corner away from the anchored edge scales it. Dropping it near its
// default slot clicks it back into place. A tap outside puts it down. The
// input layer drives this (see io_input) - the pad is an overlay of
// whatever is being played, not a screen of its own.

// Lifts the pad and starts moving it from the position (the held finger)
void begin_edit_mode(const P& logical_px);

bool is_edit_active();

// Starts a drag of an already lifted pad: scaling if the position is on the
// corner handle, moving otherwise
void begin_edit_drag(const P& logical_px);

void edit_drag_move(const P& logical_px);

// Ends the drag and persists the placement (the pad stays lifted)
void end_edit_drag();

// Puts the pad down, persisting its placement. Does nothing if it is not
// lifted.
void exit_edit_mode();

// Mirrors a hand-placed pad when the interface changes hands, so that it
// keeps the same spot relative to its now-opposite anchor
void mirror_placement();

// The pad's display area (see io_display): its own texture, sized to the
// LARGEST the pad can be scaled to and positioned at its current placement.
// The origin moves with the pad; the texture is only recreated when the
// display textures themselves are (see io::set_display_px_origin).
R display_px_rect();

// Top left corner of the pad's display area in logical screen pixels
P origin_px();

// Width in logical screen pixels of the screen edge column the pad occupies
// at its DEFAULT slot and default size, or 0 when the pad is not the
// movement mode. The map's centering keeps the player clear of it (see
// viewport). NOTE: Deliberately independent of where the player has since
// dragged the pad, or how far they have scaled it: the camera must not
// lurch about as the pad is arranged.
int reserved_px_w();

}  // namespace dpad

#endif  // DPAD_HPP

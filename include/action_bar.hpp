// =============================================================================
// Copyright Werewolf Camp
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef ACTION_BAR_HPP
#define ACTION_BAR_HPP

#include <optional>
#include <string>
#include <vector>

struct P;
struct R;

// -----------------------------------------------------------------------------
// An icon bar at the bottom of the screen on touch devices, giving access to
// the keyboard commands. Drawn with the game's own style (Material Symbols
// icons tinted with the game palette).
//
// The bar is user configurable: each action can be shown or hidden, and the
// order can be changed (see ActionsConfigState). The configuration persists
// in the config file. The "hamburger" menu button is always the first
// button (at the screen corner opposite the side stats panel), and is not
// configurable. The bar (including the hamburger) only shows during play -
// never over menu screens.
// -----------------------------------------------------------------------------
namespace action_bar
{
// Number of gui cell rows reserved for the action bar at the bottom of the
// map/log column (see panels::init).
inline constexpr int g_h_cells = 5;

struct Action
{
        // Key to feed to the game's input handling (a printable character,
        // or an SDLK_* value such as escape/return/tab)
        int key {0};

        // If set, the button instead toggles the on-screen keyboard
        bool is_keyboard_toggle {false};
};

struct ActionDef
{
        // Stable identifier used in the config file
        const char* id;

        // Icon file name (svg, without extension)
        const char* icon;

        // Human readable name, shown in the actions configuration screen
        const char* label;

        Action action;

        // Whether the button starts toggled on (a stored actions config
        // takes precedence)
        bool default_enabled {true};
};

// All configurable actions, in default order (excludes the hamburger)
const std::vector<ActionDef>& all_actions();

// The user's current ordering of ALL action ids (enabled and disabled)
const std::vector<std::string>& current_order();

bool is_action_enabled(const std::string& id);

void set_action_enabled(const std::string& id, bool enabled);

// Moves an action to a new index within the current order
void move_action(const std::string& id, int new_idx);

// Persists the current order and enabled set to the config file
void save_to_config();

const ActionDef* action_def(const std::string& id);

// The bar is only shown (and tappable) while actually playing.
bool is_visible();

void draw();

// --- Button metrics, shared with the d-pad ---
//
// The pad reads as more of the same control set rather than as a separate
// widget: its cells wear the bar's chrome, are never smaller than a bar
// button, and its glyphs are drawn at exactly the bar's icon size however
// far the pad itself is scaled (see dpad).

// Size (logical screen px) of a bar button - square, and fixed regardless
// of how many buttons are enabled
P button_px_dims();

// Size (logical screen px) of the glyph drawn inside a bar button
int icon_px_size();

// Draws the face of a button (fill and outline) in a pixel rectangle. A
// highlighted face is outlined like a marked menu entry - the bar uses it
// for a button held for reordering.
void draw_button_face(const R& px_rect, bool is_highlighted);

// Top y (logical screen px) of the bar's occupied button rows - the bar's
// actual height depends on how many buttons are enabled (one or two rows).
// This is where the interface ends and the map begins, and what anything
// wanting to sit ON TOP of the bar anchors to (see context_pins).
//
// NOTE: This is a pure layout question, and does NOT depend on the bar
// being shown: the rows are reserved either way (see panels::init).
int occupied_top_px();

// Hit-test a position in logical screen pixels (the coordinate space of
// io::window_px_to_logical_px) against the bar's buttons. Returns nothing
// while the bar is not visible.
std::optional<Action> action_at(const P& logical_px);

// Whether a position falls on the bar's occupied button rows. Gestures
// starting there belong to the bar - they never pan the map.
bool is_pos_on_bar(const P& logical_px);

// --- Reordering the bar by dragging its buttons ---
//
// Holding a button lifts it, and dragging then carries it along the bar
// with the other buttons making room for it live; releasing drops it in
// place and saves the layout. The same reordering is available in the
// actions configuration screen (which also shows/hides actions).
//
// This is driven by the input layer (see io_input) rather than by a state:
// the bar is an overlay of whatever is being played, not a screen of its
// own. The hamburger is not part of the reordering - it stays in its
// corner, and nothing can be dropped onto its slot.

// Lifts the button at the position, if there is a draggable one there
bool try_begin_reorder_drag(const P& logical_px);

// Carries the lifted button to the position, reordering the bar under it
void reorder_drag_move(const P& logical_px);

// Drops the lifted button and persists the new order
void end_reorder_drag();

bool is_reorder_drag_active();

}  // namespace action_bar

#endif  // ACTION_BAR_HPP

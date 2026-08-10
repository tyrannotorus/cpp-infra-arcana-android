// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef CONTEXT_PINS_HPP
#define CONTEXT_PINS_HPP

#include <string>

#include "colors.hpp"

struct P;

// -----------------------------------------------------------------------------
// Context pins
//
// The tappable [ action ] buttons of whatever is going on right now: the
// [ describe ] / [ kick ] / [ close ] actions of the cell being looked at,
// the [ fire ] / [ swap ] / [ cancel ] of an aim in progress, the
// [ yes ] / [ no ] of a question. Tapping a pin sends its key as input,
// so a pin is nothing more than a touch affordance for a command the game
// already understands.
//
// They are drawn in a row directly on top of the action bar's buttons -
// NOT with the status messages at the top of the screen, which is where
// they used to live: an action to take belongs within reach of the thumb
// that is going to take it, beside the rest of the controls, while the
// top of the screen is for reading.
//
// Two kinds of pin share the row:
//
//   pushed   - added for the context of the current messages, and dropped
//              again as soon as that context is gone (a new message, or
//              the log being cleared - see msg_log, which owns that
//              lifetime and calls clear() itself)
//
//   standing - derived from the game state every frame instead: [ pick up ]
//              stays for as long as the player stands on an item, no
//              matter what is printed meanwhile
// -----------------------------------------------------------------------------
namespace context_pins
{
// Adds a pin for the context of the current messages
void add(
        const std::string& label,
        int key,
        const Color& color = colors::menu_highlight());

// Drops the pushed pins (the standing ones are derived every frame)
void clear();

// Drawn on top of every state, like the action bar itself (see
// states::draw) - the pins are an overlay of whatever is being played,
// not content of a screen.
void draw();

// The key of the pin at a position in logical screen pixels, or 0 if
// there is no pin there
int key_at(const P& logical_px);

// Whether a position falls on the pin row. Gestures starting there belong
// to the pins - they never pan the map.
bool is_pos_on_pins(const P& logical_px);

// How many gui rows above the action bar PANEL the pin row can reach. The
// pins are anchored to the top of the bar's buttons, which is not the top
// of the panel - the bar display is sized to include them (see
// io_display's display_logical_px_rect).
int rows_above_bar();

}  // namespace context_pins

#endif  // CONTEXT_PINS_HPP

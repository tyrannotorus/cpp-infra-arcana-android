// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef DRAW_BOX_HPP
#define DRAW_BOX_HPP

#include "colors.hpp"
#include "panel.hpp"
#include "state.hpp"

struct R;

// NOTE: The panel parameter only determines which display the box is drawn
// to (the border rectangle is in screen gui coordinates as before).
//
// When the border covers the whole screen and the current state is a
// closable fullscreen screen (see screen_has_close_button), a standard
// [ x ] close control is drawn embedded in the top border, at the corner
// on the side stats panel's side. Tapping it sends escape (see io_input).
void draw_box(
        R border,
        const Color& color = colors::dark_gray(),
        Panel panel = Panel::screen);

// Whether a state's screen shows the standard [ x ] close control
bool screen_has_close_button(StateId id);

// Standard horizontal inset (gui cells) of content embedded in the
// fullscreen border - the [ x ] close control, the title screen's version
// string, etc all use this same inset
constexpr int g_screen_border_content_inset = 3;

// Hit area of the close control in logical screen pixels (expanded beyond
// the drawn text for easier tapping)
R screen_close_button_hit_px_rect();

#endif  // DRAW_BOX_HPP

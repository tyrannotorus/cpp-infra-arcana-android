// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef MAP_MODE_GUI_HPP
#define MAP_MODE_GUI_HPP

namespace map_mode_gui
{
void draw();

// The side stats panel scrolls when its content is taller than it. A finger
// movement (logical pixels - the content follows the finger) only moves the
// panel's window over the content (see io::Display::side_content). Returns
// true if the panel must be redrawn: an edge fade came or went.
bool scroll_by_px(float finger_dy_px);

}  // namespace map_mode_gui

#endif  // MAP_MODE_GUI_HPP

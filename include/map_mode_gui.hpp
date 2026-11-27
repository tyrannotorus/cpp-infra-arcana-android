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

// Scrolls the side stats panel content with the finger (logical pixels).
// Returns true if an edge fade came or went - the panel must be redrawn.
bool scroll_by_px(float finger_dy_px);

}  // namespace map_mode_gui

#endif  // MAP_MODE_GUI_HPP

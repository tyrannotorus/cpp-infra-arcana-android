// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef DRAW_MAP_HPP
#define DRAW_MAP_HPP

#include "colors.hpp"

struct P;

namespace draw_map
{
void run();

// Draws the look/aim reticle as corner brackets overlaid on a view cell -
// the cell's content (monster, item, terrain) stays visible under it
// (unlike map draw objects, which replace the whole cell)
void draw_reticle(
        const P& view_pos,
        const Color& color = colors::light_green());

}  // namespace draw_map

#endif  // DRAW_MAP_HPP

// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef IO_ICONS_HPP
#define IO_ICONS_HPP

#include <string>

#include "colors.hpp"

struct P;

// -----------------------------------------------------------------------------
// Vector icon rendering (Material Symbols, vendored as SVG under
// gfx/icons). Icons are rasterized at the exact requested pixel size on
// first use (via SDL_image's bundled nanosvg) and cached; the raster is
// whitened so that icons are tinted with the game's palette at draw time,
// like the tile graphics. The cache is dropped together with all other
// textures on renderer re-initialization (font/scaling changes, GPU
// context loss).
// -----------------------------------------------------------------------------
namespace io
{
// Draws an icon centered on a position (logical pixels, current display).
// The name is the SVG file name without extension (e.g. "backpack").
// Draws nothing (once logging an error) if the icon cannot be loaded.
//
// The angle (degrees, clockwise) turns the drawn icon about its center, so
// that one directional glyph can serve every direction (see dpad, which
// draws its eight arrows from a single "arrow_forward"). The raster is
// cached per name and size - a rotated draw costs no extra texture.
void draw_icon(
        const std::string& name,
        const P& center_px,
        int px_size,
        const Color& color,
        double angle = 0.0);

void cleanup_icons();

}  // namespace io

#endif  // IO_ICONS_HPP

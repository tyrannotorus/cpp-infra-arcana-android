// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include <algorithm>
#include <vector>

#include "actor.hpp"
#include "array2.hpp"
#include "colors.hpp"
#include "config.hpp"
#include "debug.hpp"
#include "gfx.hpp"
#include "io.hpp"
#include "io_display.hpp"
#include "io_internal.hpp"
#include "map.hpp"
#include "panel.hpp"
#include "pos.hpp"
#include "state.hpp"
#include "text_format.hpp"
#include "viewport.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static P map_cell_px_dims()
{
        return {config::map_cell_px_w(), config::map_cell_px_h()};
}

static R map_cell_px_rect(const P& view_pos)
{
        const P px_pos = io::map_to_px_coords(Panel::map, view_pos);

        return {px_pos, px_pos + map_cell_px_dims() - 1};
}

static void draw_filled_rect(const P& view_pos, const Color& color)
{
        io::draw_rectangle_filled(map_cell_px_rect(view_pos), color);
}

static bool is_drawable(const io::MapDrawObj& obj)
{
        return (
                (obj.tile != gfx::TileId::END) &&
                (obj.character != 0) &&
                (obj.character != ' '));
}

// Rectangles of one color, filled together in a single draw call
struct ColoredRects
{
        Color color {};
        std::vector<R> px_rects {};
};

static std::vector<R>& rects_for_color(
        std::vector<ColoredRects>& groups,
        const Color& color)
{
        // NOTE: A frame only ever has a handful of distinct background colors
        // (black for almost everything, plus a few terrain and monster
        // markers), so a linear search is the right lookup here.
        for (ColoredRects& group : groups) {
                if (group.color == color) {
                        return group.px_rects;
                }
        }

        groups.push_back({color, {}});

        return groups.back().px_rects;
}

// -----------------------------------------------------------------------------
// io
// -----------------------------------------------------------------------------
namespace io
{
void TileDrawObj::draw() const
{
        draw_tile(*this);
}

void CharacterDrawObj::draw() const
{
        draw_character(*this);
}

void MapDrawObj::draw() const
{
        io::draw_map_obj(*this);
}

void draw_map_obj(const MapDrawObj& obj)
{
        // NOTE: It is not checked here if the object is inside the map, this is
        // the callers responsibility.

        if (!is_drawable(obj)) {
                return;
        }

        set_clip_rect_to_panel(Panel::map);

        if (!config::is_tiles_mode() &&
            (obj.character == g_filled_rect_char)) {
                draw_filled_rect(obj.pos, obj.color);

                return;
        }

        if (config::is_tiles_mode()) {
                TileDrawObj tile_obj;

                tile_obj.tile = obj.tile;
                tile_obj.panel = Panel::map;
                tile_obj.pos = obj.pos;
                tile_obj.color = obj.color;
                tile_obj.bg_color = obj.color_bg;
                tile_obj.draw_bg = DrawBg::yes;

                tile_obj.draw();
        }
        else {
                CharacterDrawObj char_obj;

                char_obj.character = obj.character;
                char_obj.panel = Panel::map;
                char_obj.pos = obj.pos;
                char_obj.color = obj.color;
                char_obj.bg_color = obj.color_bg;
                char_obj.draw_bg = DrawBg::yes;

                char_obj.draw();
        }

        disable_clip_rect();
}

void MapDrawBuffer::reset(const R& view_area)
{
        m_view_area = view_area;

        const size_t nr_cells =
                (size_t)std::max(0, view_area.w()) *
                (size_t)std::max(0, view_area.h());

        // NOTE: assign() over the existing storage - the buffer keeps its
        // allocation from frame to frame
        m_cells.assign(nr_cells, MapDrawObj());
}

void MapDrawBuffer::put(const MapDrawObj& obj)
{
        if (!is_drawable(obj) || !m_view_area.is_pos_inside(obj.pos)) {
                return;
        }

        const P offset = obj.pos - m_view_area.p0;

        m_cells[((size_t)offset.y * (size_t)m_view_area.w()) +
                (size_t)offset.x] = obj;
}

void MapDrawBuffer::draw() const
{
        if (m_cells.empty()) {
                return;
        }

        set_display(Display::map);

        // NOTE: Set ONCE for the whole map, not per cell - a clip rectangle
        // change is a command of its own, which breaks the run of draws that
        // SDL would otherwise merge
        set_clip_rect_to_panel(Panel::map);

        const bool is_tiles = config::is_tiles_mode();

        // Pass 1: the cell backgrounds, one fill call per distinct color
        std::vector<ColoredRects> bg_groups;

        for (const MapDrawObj& obj : m_cells) {
                if (!is_drawable(obj)) {
                        continue;
                }

                const bool is_filled_rect_char =
                        !is_tiles && (obj.character == g_filled_rect_char);

                // A filled rectangle "glyph" IS its own background
                const Color& color =
                        is_filled_rect_char
                        ? obj.color
                        : obj.color_bg;

                rects_for_color(bg_groups, color)
                        .push_back(map_cell_px_rect(obj.pos));
        }

        for (const ColoredRects& group : bg_groups) {
                draw_rectangles_filled(group.px_rects, group.color);
        }

        // Pass 2: the foregrounds. Everything comes out of one texture, so
        // the whole map merges into a single draw call - except that the
        // contoured and non-contoured variants are separate textures, so
        // they are emitted in two runs rather than interleaved.
        for (int pass = 0; pass < 2; ++pass) {
                const bool want_contours = (pass == 1);

                for (const MapDrawObj& obj : m_cells) {
                        if (!is_drawable(obj)) {
                                continue;
                        }

                        if (!is_tiles &&
                            (obj.character == g_filled_rect_char)) {
                                // Already drawn as its own background
                                continue;
                        }

                        // NOTE: Must match the texture choice made by
                        // draw_tile_at_px / draw_map_character_at_px - the
                        // point of the two runs is that each one uses
                        // exactly one texture
                        const bool has_contours =
                                is_tiles
                                ? ((obj.color != colors::black()) &&
                                   (obj.color_bg != colors::black()))
                                : (obj.color_bg != colors::black());

                        if (has_contours != want_contours) {
                                continue;
                        }

                        const P px_pos =
                                map_to_px_coords(Panel::map, obj.pos);

                        if (is_tiles) {
                                draw_tile_at_px(
                                        obj.tile,
                                        px_pos,
                                        obj.color,
                                        obj.color_bg);
                        }
                        else {
                                draw_map_character_at_px(
                                        obj.character,
                                        px_pos,
                                        obj.color,
                                        obj.color_bg);
                        }
                }
        }

        disable_clip_rect();
}

}  // namespace io

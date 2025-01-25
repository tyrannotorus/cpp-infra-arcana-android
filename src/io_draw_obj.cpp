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
static void draw_filled_rect(const P& view_pos, const Color& color)
{
        const auto px_pos = io::gui_to_px_coords(Panel::map, view_pos);

        const auto px_dims =
                P(config::gui_cell_px_w(),
                  config::gui_cell_px_h());

        io::draw_rectangle_filled({px_pos, px_pos + px_dims - 1}, color);
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

        const bool is_drawable =
                (obj.tile != gfx::TileId::END) &&
                (obj.character != 0) &&
                (obj.character != ' ');

        if (!is_drawable) {
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

}  // namespace io

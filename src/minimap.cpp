// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "minimap.hpp"

#include <climits>

#include "actor_player.hpp"
#include "common_text.hpp"
#include "draw_box.hpp"
#include "io.hpp"
#include "map.hpp"
#include "map_parsing.hpp"
#include "panel.hpp"
#include "terrain.hpp"
#include "terrain_door.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
Array2<Color> s_minimap(0, 0);

static Color map_cell_color(
        const terrain::Terrain* const terrain,
        const item::Item* const item,
        const bool is_blocked)
{
        const auto terrain_id = terrain->id();

        if (item)
        {
                if ((item->data().type == ItemType::ranged_wpn) &&
                    !item->data().ranged.has_infinite_ammo)
                {
                        const auto* wpn = static_cast<const item::Wpn*>(item);

                        if (wpn->m_ammo_loaded == 0)
                        {
                                return colors::magenta();
                        }
                }

                return colors::light_magenta();
        }

        if (terrain_id == terrain::Id::stairs)
        {
                return colors::yellow();
        }

        if (terrain_id == terrain::Id::door)
        {
                const auto* const door =
                        static_cast<const terrain::Door*>(terrain);

                if (!door->is_hidden())
                {
                        if (door->type() == terrain::DoorType::metal)
                        {
                                return colors::light_teal();
                        }
                        else
                        {
                                return colors::light_white();
                        }
                }
        }

        if (terrain_id == terrain::Id::lever)
        {
                return colors::teal();
        }

        if (terrain_id == terrain::Id::liquid_deep)
        {
                return colors::blue();
        }

        if (is_blocked)
        {
                return colors::sepia();
        }

        return colors::dark_gray_brown();
}

static int get_px_w_per_cell()
{
        // TODO: Make this dynamic, i.e. depend on number of map cells and/or
        // window dimensions
        return 6;
}

static R get_map_area_explored()
{
        // Find the most top left and bottom right map cells explored
        R area_explored(INT_MAX, INT_MAX, 0, 0);

        const auto& map_dims = s_minimap.dims();

        for (int x = 0; x < map_dims.x; ++x)
        {
                for (int y = 0; y < map_dims.y; ++y)
                {
                        if (map::g_explored.at(x, y))
                        {
                                area_explored.p0.x = std::min(
                                        area_explored.p0.x,
                                        x);

                                area_explored.p0.y = std::min(
                                        area_explored.p0.y,
                                        y);

                                area_explored.p1.x = std::max(
                                        area_explored.p1.x,
                                        x);

                                area_explored.p1.y = std::max(
                                        area_explored.p1.y,
                                        y);
                        }
                }
        }

        return area_explored;
}

static R get_minimap_px_rect_on_screen(const R& map_area_explored)
{
        const int px_w_per_cell = get_px_w_per_cell();

        const P minimap_px_dims =
                map_area_explored
                        .dims()
                        .scaled_up(px_w_per_cell);

        const P screen_px_dims =
                io::gui_to_px_coords(
                        panels::dims(Panel::screen));

        const P minimap_p0(
                screen_px_dims.x / 2 - minimap_px_dims.x / 2,
                screen_px_dims.y / 2 - minimap_px_dims.y / 2);

        const R px_rect(
                minimap_p0,
                minimap_p0 + minimap_px_dims - 1);

        return px_rect;
}

// -----------------------------------------------------------------------------
// ViewMinimap
// -----------------------------------------------------------------------------
void ViewMinimap::draw()
{
        draw_box(panels::area(Panel::screen));

        io::draw_text_center(
                " Viewing minimap " + common_text::g_minimap_exit_hint + " ",
                Panel::screen,
                P(panels::center_x(Panel::screen), 0),
                colors::title());

        const int px_w_per_cell = get_px_w_per_cell();

        const R area_explored = get_map_area_explored();

        const R minimap_px_rect = get_minimap_px_rect_on_screen(area_explored);

        for (int x = area_explored.p0.x; x <= area_explored.p1.x; ++x)
        {
                for (int y = area_explored.p0.y; y <= area_explored.p1.y; ++y)
                {
                        const P pos_relative_to_explored_area =
                                P(x, y) - area_explored.p0;

                        const P px_pos =
                                pos_relative_to_explored_area
                                        .scaled_up(px_w_per_cell)
                                        .with_offsets(minimap_px_rect.p0);

                        const P px_dims(px_w_per_cell, px_w_per_cell);

                        const R cell_px_rect(px_pos, px_pos + px_dims - 1);

                        Color color;

                        if (map::g_player->m_pos == P(x, y))
                        {
                                color = colors::light_green();
                        }
                        else
                        {
                                color = s_minimap.at(x, y);
                        }

                        io::draw_rectangle_filled(cell_px_rect, color);
                }
        }
}

void ViewMinimap::update()
{
        const auto input = io::get();

        switch (input.key)
        {
        case SDLK_SPACE:
        case SDLK_ESCAPE:
        case 'm':
                // Exit screen
                states::pop();
                break;

        default:
                break;
        }
}

// -----------------------------------------------------------------------------
// minimap
// -----------------------------------------------------------------------------
namespace minimap
{
void clear()
{
        s_minimap.resize(0, 0);
}

void update()
{
        if (s_minimap.dims() != map::dims())
        {
                s_minimap.resize(map::dims());

                for (auto& color : s_minimap)
                {
                        color = colors::black();
                }
        }

        Array2<bool> blocked(map::dims());

        map_parsers::BlocksWalking(ParseActors::no)
                .run(blocked, blocked.rect());

        const size_t nr_positions = map::nr_positions();
        for (size_t i = 0; i < nr_positions; ++i)
        {
                const auto* const terrain = map::g_terrain.at(i);
                const auto* const item = map::g_items.at(i);
                const bool is_seen = map::g_seen.at(i);

                if (is_seen)
                {
                        s_minimap.at(i) =
                                map_cell_color(
                                        terrain,
                                        item,
                                        blocked.at(i));
                }
        }
}

}  // namespace minimap

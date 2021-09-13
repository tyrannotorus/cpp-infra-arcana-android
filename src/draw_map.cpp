// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "draw_map.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <vector>

#include "actor.hpp"
#include "actor_data.hpp"
#include "actor_player.hpp"
#include "actor_see.hpp"
#include "array2.hpp"
#include "colors.hpp"
#include "config.hpp"
#include "game_time.hpp"
#include "gfx.hpp"
#include "inventory.hpp"
#include "io.hpp"
#include "item.hpp"
#include "item_data.hpp"
#include "map.hpp"
#include "panel.hpp"
#include "player_bon.hpp"
#include "pos.hpp"
#include "property_handler.hpp"
#include "rect.hpp"
#include "terrain.hpp"
#include "terrain_data.hpp"
#include "terrain_door.hpp"
#include "viewport.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static Array2<CellRenderData> s_render_array(0, 0);
static Array2<CellRenderData> s_render_array_player_memory(0, 0);

static char s_filled_rect_char = 1;

static void clear_render_array()
{
        std::fill(
                std::begin(s_render_array),
                std::end(s_render_array),
                CellRenderData());
}

static void set_terrains()
{
        const size_t nr_positions = map::nr_positions();
        for (size_t i = 0; i < nr_positions; ++i)
        {
                if (!map::g_seen.at(i))
                {
                        continue;
                }

                auto& render_data = s_render_array.at(i);

                const auto* const t = map::g_terrain.at(i);

                gfx::TileId gore_tile = gfx::TileId::END;

                char gore_character = 0;

                if (t->can_have_gore())
                {
                        gore_tile = t->gore_tile();
                        gore_character = t->gore_character();
                }

                if (gore_tile != gfx::TileId::END)
                {
                        // Override with "gore"
                        render_data.tile = gore_tile;
                        render_data.character = gore_character;
                        render_data.color = colors::red();

                        continue;
                }

                render_data.tile = t->tile();
                render_data.character = t->character();
                render_data.color = t->color();

                const Color terrain_color_bg = t->color_bg();

                if (terrain_color_bg != colors::black())
                {
                        render_data.color_bg =
                                terrain_color_bg;
                }

                // TODO: Do in post_process_wall_tiles instead (but it needs
                // refactoring first)
                if ((map::g_terrain.at(i)->id() == terrain::Id::wall) &&
                    config::text_mode_filled_walls())
                {
                        render_data.character = s_filled_rect_char;
                }
        }
}

static void post_process_wall_tiles()
{
        for (int x = 0; x < map::w(); ++x)
        {
                for (int y = 0; y < (map::h() - 1); ++y)
                {
                        auto& render_data = s_render_array.at(x, y);

                        const auto tile = render_data.tile;

                        const bool is_wall_top_tile =
                                terrain::Wall::is_wall_top_tile(tile);

                        if (!is_wall_top_tile)
                        {
                                continue;
                        }

                        const terrain::Wall* wall = nullptr;

                        {
                                const auto* const t = map::g_terrain.at(x, y);

                                const auto id = t->id();

                                if (id == terrain::Id::wall)
                                {
                                        wall = static_cast<const terrain::Wall*>(t);
                                }
                                else if (id == terrain::Id::door)
                                {
                                        const auto* door = static_cast<const terrain::Door*>(t);

                                        if (door->is_hidden())
                                        {
                                                wall = door->mimic();
                                        }
                                }

                                if (!wall)
                                {
                                        continue;
                                }
                        }

                        if (map::g_explored.at(x, y + 1))
                        {
                                const auto tile_below =
                                        s_render_array.at(x, y + 1).tile;

                                if (terrain::Wall::is_wall_front_tile(tile_below) ||
                                    terrain::Wall::is_wall_top_tile(tile_below) ||
                                    terrain::Door::is_tile_any_door(tile_below))
                                {
                                        render_data.tile =
                                                wall->top_wall_tile();
                                }
                                else if (wall)
                                {
                                        render_data.tile =
                                                wall->front_wall_tile();
                                }
                        }
                        else
                        {
                                // Cell below is not explored
                                if (wall)
                                {
                                        render_data.tile =
                                                wall->front_wall_tile();
                                }
                        }
                }
        }
}

static void set_dead_actors()
{
        for (auto* actor : game_time::g_actors)
        {
                const P& p(actor->m_pos);

                if (!map::g_seen.at(p) ||
                    !actor->is_corpse() ||
                    (actor->m_data->character == 0) ||
                    (actor->m_data->character == ' ') ||
                    (actor->m_data->tile == gfx::TileId::END))
                {
                        continue;
                }

                auto& render_data = s_render_array.at(p);

                render_data.color = actor->color();
                render_data.tile = actor->tile();
                render_data.character = actor->character();
        }
}

static void set_items()
{
        const size_t nr_positions = map::nr_positions();
        for (size_t i = 0; i < nr_positions; ++i)
        {
                const auto* const item = map::g_items.at(i);

                if (!map::g_seen.at(i) || !item)
                {
                        continue;
                }

                auto& render_data = s_render_array.at(i);

                render_data.color = item->color();
                render_data.tile = item->tile();
                render_data.character = item->character();
        }
}

static void copy_seen_cells_to_player_memory()
{
        const size_t nr_positions = map::nr_positions();
        for (size_t i = 0; i < nr_positions; ++i)
        {
                if (map::g_seen.at(i))
                {
                        s_render_array_player_memory.at(i) =
                                s_render_array.at(i);
                }
        }
}

static void set_light_level()
{
        const size_t nr_positions = map::nr_positions();
        for (size_t i = 0; i < nr_positions; ++i)
        {
                const auto* const t = map::g_terrain.at(i);

                if (!map::g_seen.at(i) ||
                    !t->is_los_passable() ||
                    (t->id() == terrain::Id::chasm))
                {
                        continue;
                }

                auto& color = s_render_array.at(i).color;

                if (map::g_light.at(i))
                {
                        color.set_rgb(
                                std::min(255, color.r() + 60),
                                std::min(255, color.g() + 60),
                                color.b());
                }
                else if (map::g_dark.at(i))
                {
                        color = color.shaded(30);
                }
        }
}

static void set_mobiles()
{
        for (auto* mob : game_time::g_mobs)
        {
                const P& p = mob->pos();

                const gfx::TileId mob_tile = mob->tile();

                const char mob_character = mob->character();

                if (map::g_seen.at(p) &&
                    mob_tile != gfx::TileId::END &&
                    mob_character != 0 &&
                    mob_character != ' ')
                {
                        auto& render_data = s_render_array.at(p);

                        render_data.color = mob->color();
                        render_data.tile = mob_tile;
                        render_data.character = mob_character;
                }
        }
}

static void set_living_seen_monster(
        const actor::Actor& mon,
        CellRenderData& render_data)
{
        if (mon.tile() == gfx::TileId::END ||
            mon.character() == 0 ||
            mon.character() == ' ')
        {
                return;
        }

        render_data.color = mon.color();
        render_data.tile = mon.tile();
        render_data.character = mon.character();

        if (map::g_player->is_leader_of(&mon))
        {
                // The monster is player-friendly
                render_data.color_bg = colors::mon_allied_bg();
        }
        else
        {
                // The monster is hostile
                if (mon.is_aware_of_player())
                {
                        // Monster is aware of player
                        const bool has_temporary_negative_prop =
                                mon.m_properties
                                        .has_temporary_negative_prop_mon();

                        if (has_temporary_negative_prop)
                        {
                                render_data.color_bg =
                                        colors::mon_temp_property_bg();
                        }
                }
                else
                {
                        // Monster is not aware of the player
                        render_data.color_bg = colors::mon_unaware_bg();
                }
        }
}

static void set_living_hidden_monster(
        const actor::Actor& mon,
        CellRenderData& render_data)
{
        if (!mon.is_player_aware_of_me())
        {
                return;
        }

        const auto color_bg =
                map::g_player->is_leader_of(&mon)
                ? colors::mon_allied_bg()
                : colors::dark_gray();

        render_data.tile = gfx::TileId::excl_mark;
        render_data.character = '!';
        render_data.color = colors::white();
        render_data.color_bg = color_bg;
}

static void set_living_monsters()
{
        for (auto* actor : game_time::g_actors)
        {
                if (actor->is_player() || !actor->is_alive())
                {
                        continue;
                }

                const P& pos = actor->m_pos;

                auto& render_data = s_render_array.at(pos);

                if (can_player_see_actor(*actor))
                {
                        set_living_seen_monster(*actor, render_data);
                }
                else
                {
                        set_living_hidden_monster(*actor, render_data);
                }
        }
}

static void set_unseen_cells_from_player_memory()
{
        const size_t nr_positions = map::nr_positions();
        for (size_t i = 0; i < nr_positions; ++i)
        {
                auto& render_data = s_render_array.at(i);

                if (map::g_seen.at(i) || !map::g_explored.at(i))
                {
                        continue;
                }

                render_data = s_render_array_player_memory.at(i);

                const double div = 3.0;

                render_data.color = render_data.color.fraction(div);

                if (render_data.color_bg != colors::black())
                {
                        render_data.color_bg =
                                render_data.color_bg.fraction(div);
                }
        }
}

static void draw_filled_rect(const P& view_pos, const Color& color)
{
        const auto px_pos = io::gui_to_px_coords(Panel::map, view_pos);

        const auto px_dims =
                P(config::gui_cell_px_w(),
                  config::gui_cell_px_h());

        io::draw_rectangle_filled({px_pos, px_pos + px_dims - 1}, color);
}

static void draw_render_array_tile(const P& view_pos, CellRenderData& data)
{
        const bool has_drawable_tile = data.tile != gfx::TileId::END;

        if (has_drawable_tile)
        {
                io::draw_tile(
                        data.tile,
                        Panel::map,
                        view_pos,
                        data.color,
                        io::DrawBg::yes,
                        data.color_bg);
        }
}

static void draw_render_array_char(const P& view_pos, CellRenderData& data)
{
        if (data.character == s_filled_rect_char)
        {
                draw_filled_rect(view_pos, data.color);

                return;
        }

        const bool has_drawable_char =
                (data.character != 0) &&
                (data.character != ' ');

        if (!has_drawable_char)
        {
                return;
        }

        io::draw_character(
                data.character,
                Panel::map,
                view_pos,
                data.color,
                io::DrawBg::yes,
                data.color_bg);
}

static void draw_render_array()
{
        const R map_view = viewport::get_map_view_area();

        for (int x = map_view.p0.x; x <= map_view.p1.x; ++x)
        {
                for (int y = map_view.p0.y; y <= map_view.p1.y; ++y)
                {
                        const P map_pos(x, y);
                        const P view_pos = viewport::to_view_pos(map_pos);

                        if (!map::is_pos_inside_map(map_pos))
                        {
                                continue;
                        }

                        auto& render_data = s_render_array.at(map_pos);

                        if (config::is_tiles_mode())
                        {
                                draw_render_array_tile(view_pos, render_data);
                        }
                        else
                        {
                                draw_render_array_char(view_pos, render_data);
                        }
                }
        }
}

static int lifebar_length(const actor::Actor& actor)
{
        const int actor_hp = std::max(0, actor.m_hp);

        const int actor_hp_max = actor::max_hp(actor);

        if (actor_hp < actor_hp_max)
        {
                int hp_percent = (actor_hp * 100) / actor_hp_max;

                return ((config::map_cell_px_w() - 2) * hp_percent) / 100;
        }

        return -1;
}

static void draw_life_bar(const actor::Actor& actor)
{
        const int length = lifebar_length(actor);

        if (length < 0)
        {
                return;
        }

        const P map_pos = actor.m_pos.with_y_offset(1);

        if (!viewport::is_in_view(map_pos))
        {
                return;
        }

        const P cell_dims(config::map_cell_px_w(), config::map_cell_px_h());

        const int w_green = length;

        const int w_bar_tot = cell_dims.x - 2;

        const int w_red = w_bar_tot - w_green;

        const P view_pos = viewport::to_view_pos(map_pos);

        P px_pos = io::map_to_px_coords(
                Panel::map,
                view_pos);

        px_pos.y -= 2;

        const int x0_green = px_pos.x + 1;

        const int x0_red = x0_green + w_green;

        if (w_green > 0)
        {
                const P px_p0_green(x0_green, px_pos.y);

                const R px_rect_green(
                        px_p0_green,
                        px_p0_green + P(w_green, 2) - 1);

                io::draw_rectangle_filled(
                        px_rect_green,
                        colors::light_green());
        }

        if (w_red > 0)
        {
                const P px_p0_red(x0_red, px_pos.y);

                const R px_rect_red(
                        px_p0_red,
                        px_p0_red + P(w_red, 2) - 1);

                io::draw_rectangle_filled(
                        px_rect_red,
                        colors::light_red());
        }
}

static void draw_monster_life_bars()
{
        for (auto* actor : game_time::g_actors)
        {
                if (!actor->is_player() &&
                    actor->is_alive() &&
                    can_player_see_actor(*actor))
                {
                        draw_life_bar(*actor);
                }
        }
}

static void draw_player_character()
{
        const P& pos = map::g_player->m_pos;

        if (!viewport::is_in_view(pos))
        {
                return;
        }

        const auto color = map::g_player->color();
        const auto color_bg = colors::black();

        gfx::TileId tile;

        if (player_bon::is_bg(Bg::ghoul))
        {
                tile = gfx::TileId::ghoul;
        }
        else
        {
                auto* item = map::g_player->m_inv.item_in_slot(SlotId::wpn);

                if (item && item->data().ranged.is_ranged_wpn)
                {
                        tile = gfx::TileId::player_firearm;
                }
                else if (item)
                {
                        tile = gfx::TileId::player_melee;
                }
                else
                {
                        tile = gfx::TileId::player_unarmed;
                }
        }

        const char character = '@';

        auto& player_render_data = s_render_array.at(pos);

        player_render_data.tile = tile;
        player_render_data.character = character;
        player_render_data.color = color;
        player_render_data.color_bg = color_bg;

        io::draw_symbol(
                tile,
                character,
                Panel::map,
                viewport::to_view_pos(pos),
                color,
                io::DrawBg::yes,
                color_bg);

        draw_life_bar(*map::g_player);
}

// -----------------------------------------------------------------------------
// draw_map
// -----------------------------------------------------------------------------
namespace draw_map
{
void clear()
{
        clear_render_array();

        std::fill(
                std::begin(s_render_array_player_memory),
                std::end(s_render_array_player_memory),
                CellRenderData());
}

void run()
{
        if (s_render_array.dims() != map::dims())
        {
                s_render_array.resize(map::dims());

                s_render_array_player_memory.resize(map::dims());
        }

        clear_render_array();

        set_unseen_cells_from_player_memory();

        set_terrains();

        if (config::is_tiles_mode())
        {
                post_process_wall_tiles();
        }

        set_dead_actors();

        set_items();

        copy_seen_cells_to_player_memory();

        set_light_level();

        set_mobiles();

        set_living_monsters();

        draw_render_array();

        draw_monster_life_bars();

        draw_player_character();
}

const CellRenderData& get_drawn_cell(int x, int y)
{
        return s_render_array.at(x, y);
}

const CellRenderData& get_drawn_cell_player_memory(int x, int y)
{
        return s_render_array_player_memory.at(x, y);
}

}  // namespace draw_map

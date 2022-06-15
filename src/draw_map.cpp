// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "draw_map.hpp"

#include <algorithm>
#include <cstddef>
#include <vector>

#include "actor.hpp"
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
#include "misc.hpp"
#include "player_bon.hpp"
#include "pos.hpp"
#include "property_handler.hpp"
#include "rect.hpp"
#include "terrain.hpp"
#include "terrain_data.hpp"
#include "viewport.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static void adapt_color_for_lit_pos(Color& color)
{
        color.set_rgb(
                std::min(255, color.r() + 80),
                std::min(255, color.g() + 80),
                color.b());
}

static void adapt_color_for_dark_pos(Color& color)
{
        color = color.shaded(40);

        color.set_rgb(
                color.r(),
                color.g(),
                std::min(255, color.b() + 20));
}

static void adapt_color_for_light_level(const size_t pos_idx, Color& color)
{
        const auto* const t = map::g_terrain.at(pos_idx);

        if (!map::g_seen.at(pos_idx) ||
            !t->is_los_passable() ||
            (t->id() == terrain::Id::chasm)) {
                return;
        }

        if (map::g_light.at(pos_idx)) {
                adapt_color_for_lit_pos(color);
        }
        else if (map::g_dark.at(pos_idx)) {
                adapt_color_for_dark_pos(color);
        }
}

static void adapt_color_for_distance_to_player(const P& pos, Color& color)
{
        if (map::g_light.at(pos)) {
                return;
        }

        const int dist = king_dist(pos, map::g_player->m_pos);

        const int k = std::clamp(dist - 1, 0, 4);

        if (k > 0) {
                color = color.shaded(k * 15);
        }
}

static void adapt_color_for_light_level(const P& pos, Color& color)
{
        adapt_color_for_light_level(map::g_terrain.pos_to_idx(pos), color);
}

static void draw_terrains()
{
        const size_t nr_positions = map::nr_positions();

        for (size_t i = 0; i < nr_positions; ++i) {
                if (!map::g_seen.at(i)) {
                        continue;
                }

                const auto* const t = map::g_terrain.at(i);

                io::MapDrawObj draw_obj;

                draw_obj.pos = viewport::to_view_pos(t->pos());

                auto gore_tile = gfx::TileId::END;
                char gore_character = 0;

                if (t->can_have_gore()) {
                        gore_tile = t->gore_tile();
                        gore_character = t->gore_character();
                }

                if (gore_tile == gfx::TileId::END) {
                        draw_obj.tile = t->tile();
                        draw_obj.character = t->character();
                        draw_obj.color = t->color();
                }
                else {
                        draw_obj.tile = gore_tile;
                        draw_obj.character = gore_character;
                        draw_obj.color = colors::red();
                }

                const auto terrain_color_bg = t->color_bg();

                if (terrain_color_bg != colors::black()) {
                        draw_obj.color_bg = terrain_color_bg;
                }

                if (config::text_mode_filled_walls()) {
                        if (draw_obj.character == '#') {
                                // Any terrain with the '#' symbol is converted
                                // to a filled rectangle instead.
                                //
                                // NOTE: No other (static) terrain except WALLS
                                // (or terrain imitating walls, such as hidden
                                // doors) must use the '#' character!
                                //
                                draw_obj.character = io::g_filled_rect_char;
                        }
                        else if (t->id() == terrain::Id::grate) {
                                // Since we are using filled rectangle as wall
                                // symbol, then we can use the '#' character for
                                // grates (looks good for this terrain, but
                                // obviously not if walls are also using this).
                                draw_obj.character = '#';
                        }
                }

                adapt_color_for_light_level(i, draw_obj.color);

                adapt_color_for_distance_to_player(t->pos(), draw_obj.color);

                draw_obj.draw();
        }
}

static void draw_dead_actors()
{
        for (auto* actor : game_time::g_actors) {
                const auto& p = actor->m_pos;

                if (!map::g_seen.at(p) || !actor->is_corpse()) {
                        continue;
                }

                io::MapDrawObj draw_obj;

                draw_obj.pos = viewport::to_view_pos(p);
                draw_obj.color = actor->color();
                draw_obj.tile = actor->tile();
                draw_obj.character = actor->character();

                adapt_color_for_light_level(p, draw_obj.color);

                draw_obj.draw();
        }
}

static void draw_items()
{
        const auto map_dims = map::dims();

        for (int x = 0; x < map_dims.x; ++x) {
                for (int y = 0; y < map_dims.y; ++y) {
                        const P p(x, y);

                        if (!map::g_seen.at(p)) {
                                continue;
                        }

                        const auto* const item = map::g_items.at(p);

                        if (!item) {
                                continue;
                        }

                        io::MapDrawObj draw_obj;

                        draw_obj.pos = viewport::to_view_pos(p);
                        draw_obj.color = item->color();
                        draw_obj.tile = item->tile();
                        draw_obj.character = item->character();

                        adapt_color_for_light_level(p, draw_obj.color);

                        draw_obj.draw();
                }
        }
}

static void draw_mobiles()
{
        for (auto* mob : game_time::g_mobs) {
                const auto& p = mob->pos();
                const auto mob_tile = mob->tile();
                const auto mob_character = mob->character();

                if (!map::g_seen.at(p) ||
                    (mob_tile == gfx::TileId::END) ||
                    (mob_character == 0) ||
                    (mob_character == ' ')) {
                        continue;
                }

                io::MapDrawObj draw_obj;

                draw_obj.pos = viewport::to_view_pos(p);
                draw_obj.color = mob->color();
                draw_obj.tile = mob_tile;
                draw_obj.character = mob_character;

                adapt_color_for_light_level(p, draw_obj.color);

                draw_obj.draw();
        }
}

static void draw_living_seen_monster(const actor::Actor& mon)
{
        const auto mon_tile = mon.tile();
        const auto mon_char = mon.character();

        if ((mon_tile == gfx::TileId::END) ||
            (mon_char == 0) ||
            (mon_char == ' ')) {
                return;
        }

        io::MapDrawObj draw_obj;

        draw_obj.pos = viewport::to_view_pos(mon.m_pos);
        draw_obj.color = mon.color();
        draw_obj.tile = mon.tile();
        draw_obj.character = mon.character();

        if (map::g_player->is_leader_of(&mon)) {
                // The monster is player-friendly
                draw_obj.color_bg = colors::mon_allied();
        }
        else {
                // The monster is hostile
                if (mon.is_aware_of_player()) {
                        // Monster is aware of player
                        const bool has_temporary_negative_prop =
                                mon.m_properties
                                        .has_temporary_negative_prop_mon();

                        if (has_temporary_negative_prop) {
                                draw_obj.color_bg =
                                        colors::mon_temp_property();
                        }
                }
                else {
                        // Monster is not aware of the player
                        draw_obj.color_bg = colors::mon_unaware();
                }
        }

        adapt_color_for_light_level(mon.m_pos, draw_obj.color);

        draw_obj.draw();
}

static void draw_living_hidden_monster(const actor::Actor& mon)
{
        if (!mon.is_player_aware_of_me()) {
                return;
        }

        io::MapDrawObj draw_obj;

        const auto color_bg =
                map::g_player->is_leader_of(&mon)
                ? colors::mon_allied()
                : colors::dark_gray();

        draw_obj.pos = viewport::to_view_pos(mon.m_pos);
        draw_obj.tile = gfx::TileId::excl_mark;
        draw_obj.character = '!';
        draw_obj.color = colors::white();
        draw_obj.color_bg = color_bg;

        adapt_color_for_light_level(mon.m_pos, draw_obj.color);

        draw_obj.draw();
}

static void draw_living_monsters()
{
        for (auto* actor : game_time::g_actors) {
                if (actor::is_player(actor) || !actor->is_alive()) {
                        continue;
                }

                if (can_player_see_actor(*actor)) {
                        draw_living_seen_monster(*actor);
                }
                else {
                        draw_living_hidden_monster(*actor);
                }
        }
}

static io::MapDrawObj player_memory_to_draw_obj(
        const map::PlayerMemoryAppearance& d)
{
        io::MapDrawObj draw_obj;

        draw_obj.tile = d.tile;
        draw_obj.color = d.color;
        draw_obj.color_bg = colors::black();
        draw_obj.character = d.character;

        return draw_obj;
}

static void draw_unseen_cells_from_player_memory()
{
        const auto view = viewport::get_map_view_area();

        for (int x = view.p0.x; x < view.p1.x; ++x) {
                for (int y = view.p0.y; y < view.p1.y; ++y) {
                        const P p(x, y);

                        if (!map::is_pos_inside_map(p)) {
                                continue;
                        }

                        if (map::g_seen.at(p)) {
                                continue;
                        }

                        io::MapDrawObj draw_obj;

                        const auto& terrain_memory =
                                map::g_terrain_memory.at(p);

                        const auto& item_memory =
                                map::g_item_memory.at(p);

                        if (terrain_memory.appearance.is_defined()) {
                                draw_obj =
                                        player_memory_to_draw_obj(
                                                terrain_memory.appearance);
                        }

                        if (item_memory.appearance.is_defined()) {
                                draw_obj =
                                        player_memory_to_draw_obj(
                                                item_memory.appearance);
                        }

                        draw_obj.pos = viewport::to_view_pos(p);

                        draw_obj.color = draw_obj.color.shaded(80);

                        draw_obj.draw();
                }
        }
}

static void draw_player_character()
{
        const auto& player = *map::g_player;

        if (!viewport::is_in_view(player.m_pos)) {
                return;
        }

        const auto color = player.color();
        const auto color_bg = colors::black();

        auto tile = gfx::TileId::END;

        if (player_bon::is_bg(Bg::ghoul)) {
                tile = gfx::TileId::ghoul;
        }
        else {
                auto* item = player.m_inv.item_in_slot(SlotId::wpn);

                if (item && item->data().ranged.is_ranged_wpn) {
                        tile = gfx::TileId::player_firearm;
                }
                else if (item) {
                        tile = gfx::TileId::player_melee;
                }
                else {
                        tile = gfx::TileId::player_unarmed;
                }
        }

        io::MapDrawObj draw_obj;

        const char character = '@';

        draw_obj.pos = viewport::to_view_pos(player.m_pos);
        draw_obj.tile = tile;
        draw_obj.character = character;
        draw_obj.color = color;
        draw_obj.color_bg = color_bg;

        draw_obj.draw();
}

// -----------------------------------------------------------------------------
// draw_map
// -----------------------------------------------------------------------------
namespace draw_map
{
void clear()
{
}

void run()
{
        draw_unseen_cells_from_player_memory();
        draw_terrains();
        draw_dead_actors();
        draw_items();
        draw_mobiles();
        draw_living_monsters();
        draw_player_character();
}

}  // namespace draw_map

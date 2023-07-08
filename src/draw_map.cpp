// =============================================================================
// Copyright 2011-2023 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "draw_map.hpp"

#include <algorithm>
#include <cstddef>
#include <unordered_map>
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
const std::unordered_map<item::Id, gfx::TileId> s_wpn_id_to_player_tile {
        {item::Id::axe, gfx::TileId::player_axe},
        {item::Id::club, gfx::TileId::player_club},
        {item::Id::dagger, gfx::TileId::player_dagger},
        {item::Id::electric_gun, gfx::TileId::player_electric_gun},
        {item::Id::flagellant_whip, gfx::TileId::player_flagellant_whip},
        {item::Id::hammer, gfx::TileId::player_hammer},
        {item::Id::hatchet, gfx::TileId::player_hatchet},
        {item::Id::machete, gfx::TileId::player_machete},
        {item::Id::tommy_gun, gfx::TileId::player_tommy_gun},
        {item::Id::morphic_blaster, gfx::TileId::player_morphic_blaster},
        {item::Id::pharaoh_staff, gfx::TileId::player_pharaoh_staff},
        {item::Id::pistol, gfx::TileId::player_pistol},
        {item::Id::revolver, gfx::TileId::player_pistol},
        {item::Id::pitchfork, gfx::TileId::player_pitchfork},
        {item::Id::pump_shotgun, gfx::TileId::player_pump_shotgun},
        {item::Id::rifle, gfx::TileId::player_rifle},
        {item::Id::sawed_off, gfx::TileId::player_sawed_off},
        {item::Id::sledgehammer, gfx::TileId::player_sledgehammer},
        {item::Id::spear, gfx::TileId::player_spear},
        {item::Id::spike_gun, gfx::TileId::player_spike_gun},
        {item::Id::spiked_mace, gfx::TileId::player_spiked_mace},
};

const std::unordered_map<item::Id, gfx::TileId> s_wpn_id_to_player_tile_ghoul {
        {item::Id::axe, gfx::TileId::player_ghoul_axe},
        {item::Id::club, gfx::TileId::player_ghoul_club},
        {item::Id::dagger, gfx::TileId::player_ghoul_dagger},
        {item::Id::electric_gun, gfx::TileId::player_ghoul_electric_gun},
        {item::Id::hammer, gfx::TileId::player_ghoul_hammer},
        {item::Id::hatchet, gfx::TileId::player_ghoul_hatchet},
        {item::Id::machete, gfx::TileId::player_ghoul_machete},
        {item::Id::tommy_gun, gfx::TileId::player_ghoul_tommy_gun},
        {item::Id::morphic_blaster, gfx::TileId::player_ghoul_morphic_blaster},
        {item::Id::pharaoh_staff, gfx::TileId::player_ghoul_pharaoh_staff},
        {item::Id::pistol, gfx::TileId::player_ghoul_pistol},
        {item::Id::revolver, gfx::TileId::player_ghoul_pistol},
        {item::Id::pitchfork, gfx::TileId::player_ghoul_pitchfork},
        {item::Id::pump_shotgun, gfx::TileId::player_ghoul_pump_shotgun},
        {item::Id::rifle, gfx::TileId::player_ghoul_rifle},
        {item::Id::sawed_off, gfx::TileId::player_ghoul_sawed_off},
        {item::Id::sledgehammer, gfx::TileId::player_ghoul_sledgehammer},
        {item::Id::spear, gfx::TileId::player_ghoul_spear},
        {item::Id::spike_gun, gfx::TileId::player_ghoul_spike_gun},
        {item::Id::spiked_mace, gfx::TileId::player_ghoul_spiked_mace},
};

// Background color to draw in cases where one "object" is obscuring another,
// such as when an item is on top of a trap.
static Array2<std::optional<Color>> s_bg_color_obscured(0, 0);

static void set_bg_color_obscured_terrain(
        const terrain::Terrain* const terrain,
        const size_t pos_idx)
{
        std::optional<Color>& value = s_bg_color_obscured.at(pos_idx);

        switch (terrain->id()) {
        case terrain::Id::liquid: {
                value = terrain->color_default();
        } break;

        case terrain::Id::chains: {
                value = terrain->color_default();
        } break;

        case terrain::Id::trap: {
                if (!terrain->is_hidden()) {
                        if (config::use_trap_color_when_obscured()) {
                                value = terrain->color_default();
                        }
                        else {
                                value = colors::yellow();
                        }
                }
        } break;

        default:
        {
        } break;
        }
}

static void set_bg_color_when_obscured_dead_actor(const actor::Actor& actor)
{
        const Color& color_default = colors::gray_brown();
        const Color& color_corpse_rises = colors::dark_teal();

        std::optional<Color>& color_here = s_bg_color_obscured.at(actor.m_pos);

        if (color_here && (color_here.value() == color_corpse_rises)) {
                // This position is colored as containing a corpse that will
                // rise again, do not change the color.
                return;
        }

        const bool is_corpse_rises =
                actor.m_properties.has(prop::Id::corpse_rises);

        const Color& new_color =
                is_corpse_rises
                ? color_corpse_rises
                : color_default;

        s_bg_color_obscured.at(actor.m_pos) = new_color;
}

static void use_bg_color_obscuring(Color& color, const P& p)
{
        color = s_bg_color_obscured.at(p).value_or(color);
}

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

static void adapt_color_for_light_level(Color& color, const size_t pos_idx)
{
        const terrain::Terrain* const t = map::g_terrain.at(pos_idx);

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

static void adapt_color_for_light_level(Color& color, const P& pos)
{
        adapt_color_for_light_level(color, map::g_terrain.pos_to_idx(pos));
}

static void adapt_color_for_distance_to_player(Color& color, const P& pos)
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

static void draw_terrains()
{
        const size_t nr_positions = map::nr_positions();

        for (size_t i = 0; i < nr_positions; ++i) {
                if (!map::g_seen.at(i)) {
                        continue;
                }

                const terrain::Terrain* const t = map::g_terrain.at(i);

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

                const Color terrain_color_bg = t->color_bg();

                if (terrain_color_bg == colors::black()) {
                        // Set background color to use if this terrain is
                        // obscured by another object (e.g. an item on a trap).
                        set_bg_color_obscured_terrain(t, i);
                }
                else {
                        draw_obj.color_bg = terrain_color_bg;

                        s_bg_color_obscured.at(i) = terrain_color_bg;
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

                adapt_color_for_light_level(draw_obj.color, i);

                adapt_color_for_distance_to_player(draw_obj.color, t->pos());

                draw_obj.draw();
        }
}

static void draw_dead_actors()
{
        for (actor::Actor* actor : game_time::g_actors) {
                const P& p = actor->m_pos;

                if (!map::g_seen.at(p) || !actor->is_corpse()) {
                        continue;
                }

                io::MapDrawObj draw_obj;

                draw_obj.pos = viewport::to_view_pos(p);
                draw_obj.color = actor->color();
                draw_obj.tile = actor->tile();
                draw_obj.character = actor->character();

                adapt_color_for_light_level(draw_obj.color, p);

                use_bg_color_obscuring(draw_obj.color_bg, p);

                set_bg_color_when_obscured_dead_actor(*actor);

                draw_obj.draw();
        }
}

static void draw_items()
{
        const P map_dims = map::dims();

        for (int x = 0; x < map_dims.x; ++x) {
                for (int y = 0; y < map_dims.y; ++y) {
                        const P p(x, y);

                        if (!map::g_seen.at(p)) {
                                continue;
                        }

                        const item::Item* const item = map::g_items.at(p);

                        if (!item) {
                                continue;
                        }

                        io::MapDrawObj draw_obj;

                        draw_obj.pos = viewport::to_view_pos(p);
                        draw_obj.color = item->color();
                        draw_obj.tile = item->tile();
                        draw_obj.character = item->character();

                        adapt_color_for_light_level(draw_obj.color, p);

                        use_bg_color_obscuring(draw_obj.color_bg, p);

                        draw_obj.draw();
                }
        }
}

static void draw_mobiles()
{
        for (terrain::Terrain* mob : game_time::g_mobs) {
                const P& p = mob->pos();
                const gfx::TileId mob_tile = mob->tile();
                const char mob_character = mob->character();

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

                adapt_color_for_light_level(draw_obj.color, p);

                draw_obj.draw();
        }
}

static void draw_living_seen_monster(const actor::Actor& mon)
{
        const gfx::TileId mon_tile = mon.tile();
        const char mon_char = mon.character();

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
                        if (mon.m_properties.has_temporary_negative_prop_mon()) {
                                draw_obj.color_bg = colors::mon_temp_property();
                        }
                        else if (mon.m_properties.has(prop::Id::frenzied)) {
                                draw_obj.color_bg = colors::red();
                        }
                }
                else {
                        // Monster is not aware of the player
                        draw_obj.color_bg = colors::mon_unaware();
                }
        }

        adapt_color_for_light_level(draw_obj.color, mon.m_pos);

        draw_obj.draw();
}

static void draw_living_hidden_monster(const actor::Actor& mon)
{
        if (!mon.is_player_aware_of_me()) {
                return;
        }

        io::MapDrawObj draw_obj;

        const Color color_bg =
                map::g_player->is_leader_of(&mon)
                ? colors::mon_allied()
                : colors::dark_gray();

        draw_obj.pos = viewport::to_view_pos(mon.m_pos);
        draw_obj.tile = gfx::TileId::excl_mark;
        draw_obj.character = '!';
        draw_obj.color = colors::white();
        draw_obj.color_bg = color_bg;

        adapt_color_for_light_level(draw_obj.color, mon.m_pos);

        draw_obj.draw();
}

static void draw_living_monsters()
{
        for (actor::Actor* actor : game_time::g_actors) {
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
        R view = viewport::get_map_view_area();

        // Also draw a little bit outside the viewport - we allow showing a
        // fraction of tiles if the map panel size is not aligned with a whole
        // number of map tiles (for example 15.6 map tiles can be shown on the Y
        // axis). The drawing is clipped to the map panel, so pixels outside the
        // map panel will not be drawn.
        view.p1 = view.p1.with_offsets(2, 2);

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

                        const map::PlayerMemoryTerrain& terrain_memory =
                                map::g_terrain_memory.at(p);

                        const map::PlayerMemoryItem& item_memory =
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

static gfx::TileId get_player_tile()
{
        const item::Item* const wpn = map::g_player->m_inv.item_in_slot(SlotId::wpn);

        if (player_bon::is_bg(Bg::ghoul)) {
                if (wpn) {
                        return s_wpn_id_to_player_tile_ghoul.at(wpn->id());
                }
                else {
                        return gfx::TileId::ghoul;
                }
        }
        else {
                if (wpn) {
                        return s_wpn_id_to_player_tile.at(wpn->id());
                }
                else {
                        return gfx::TileId::player_unarmed;
                }
        }
}

static void draw_player_character()
{
        const actor::Actor& player = *map::g_player;

        if (!viewport::is_in_view(player.m_pos)) {
                return;
        }

        const Color color = player.color();
        const Color color_bg = colors::black();

        gfx::TileId tile = get_player_tile();

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
void run()
{
        // NOTE: This will also setup the whole array with default values.
        s_bg_color_obscured.resize(map::dims());

        draw_unseen_cells_from_player_memory();
        draw_terrains();
        draw_dead_actors();
        draw_items();
        draw_mobiles();
        draw_living_monsters();

        draw_player_character();
}

}  // namespace draw_map

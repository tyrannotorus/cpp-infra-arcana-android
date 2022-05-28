// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "map.hpp"

#include <algorithm>
#include <climits>

#include "actor.hpp"
#include "actor_data.hpp"
#include "actor_factory.hpp"
#include "actor_player.hpp"
#include "colors.hpp"
#include "debug.hpp"
#include "direction.hpp"
#include "fov.hpp"
#include "game_time.hpp"
#include "gfx.hpp"
#include "global.hpp"
#include "init.hpp"
#include "io.hpp"
#include "item.hpp"
#include "map_parsing.hpp"
#include "minimap.hpp"
#include "misc.hpp"
#include "panel.hpp"
#include "pos.hpp"
#include "random.hpp"
#include "room.hpp"
#include "saving.hpp"
#include "smell.hpp"
#include "state.hpp"
#include "terrain.hpp"
#include "terrain_data.hpp"
#include "terrain_door.hpp"
#include "terrain_factory.hpp"
#include "text_format.hpp"

#ifndef NDEBUG
#include "viewport.hpp"
#endif  // NDEBUG

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static P s_dims(0, 0);

static void init_arrays_data()
{
        LosResult default_los;
        default_los.is_blocked_hard = true;
        default_los.is_blocked_by_dark = false;

        const size_t nr_positions = map::nr_positions();
        for (size_t i = 0; i < nr_positions; ++i)
        {
                map::g_seen.at(i) = false;
                map::g_los.at(i) = default_los;
                map::g_light.at(i) = false;
                map::g_dark.at(i) = false;
                map::g_smell.at(i) = {};
                map::g_smell_spread.at(i) = {};
                map::g_items.at(i) = nullptr;
                map::g_item_memory.at(i) = {};
                map::g_terrain.at(i) = nullptr;
                map::g_terrain_memory.at(i) = {};
                map::g_terrain_blocks_walking.at(i) = {};
                map::g_terrain_blocks_flying.at(i) = {};
                map::g_terrain_blocks_tiny_flying.at(i) = {};
                map::g_terrain_blocks_ethereal.at(i) = {};
                map::g_terrain_blocks_ooze.at(i) = {};
                map::g_terrain_blocks_small_crawling.at(i) = {};
                map::g_terrain_blocks_burrowing.at(i) = {};
                map::g_terrain_blocks_los.at(i) = {};
        }
}

static void resize_arrays()
{
        map::g_seen.resize_no_init(s_dims);
        map::g_los.resize_no_init(s_dims);
        map::g_light.resize_no_init(s_dims);
        map::g_dark.resize_no_init(s_dims);
        map::g_smell.resize_no_init(s_dims);
        map::g_smell_spread.resize_no_init(s_dims);
        map::g_items.resize_no_init(s_dims);
        map::g_item_memory.resize_no_init(s_dims);
        map::g_terrain.resize_no_init(s_dims);
        map::g_terrain_memory.resize_no_init(s_dims);
        map::g_terrain_blocks_walking.resize_no_init(s_dims);
        map::g_terrain_blocks_flying.resize_no_init(s_dims);
        map::g_terrain_blocks_tiny_flying.resize_no_init(s_dims);
        map::g_terrain_blocks_ethereal.resize_no_init(s_dims);
        map::g_terrain_blocks_ooze.resize_no_init(s_dims);
        map::g_terrain_blocks_small_crawling.resize_no_init(s_dims);
        map::g_terrain_blocks_burrowing.resize_no_init(s_dims);
        map::g_terrain_blocks_los.resize_no_init(s_dims);
}

static void free_layers_owned_memory()
{
        // Free the memory for all memory-owning layers

        const size_t nr_positions = map::nr_positions();
        for (size_t i = 0; i < nr_positions; ++i)
        {
                auto* const terrain_pp = &map::g_terrain.at(i);
                delete *terrain_pp;
                *terrain_pp = nullptr;

                auto* const item_pp = &map::g_items.at(i);
                delete *item_pp;
                *item_pp = nullptr;
        }
}

static void set_map_blocking_info_true_for_all_at(const P& pos)
{
        map::g_terrain_blocks_walking.at(pos) = true;
        map::g_terrain_blocks_flying.at(pos) = true;
        map::g_terrain_blocks_tiny_flying.at(pos) = true;
        map::g_terrain_blocks_ethereal.at(pos) = true;
        map::g_terrain_blocks_ooze.at(pos) = true;
        map::g_terrain_blocks_small_crawling.at(pos) = true;
        map::g_terrain_blocks_burrowing.at(pos) = true;
        map::g_terrain_blocks_los.at(pos) = true;
}

static void set_map_blocking_info_for_terrain(const terrain::Terrain& terrain)
{
        // NOTE: If the position can be walked through, every creature can move
        // through it.
        //
        // TODO: Consider explicitly setting for all terrain whether the terrain
        // blocks walking, flying, small crawling etc (in the terrain data or in
        // overridden functions). Then this function could perhaps just do
        // straightforward assignments instead.
        //

        const auto& p = terrain.pos();

        const bool blocks_walking = !terrain.is_walkable();

        map::g_terrain_blocks_walking.at(p) =
                blocks_walking;

        map::g_terrain_blocks_flying.at(p) =
                blocks_walking &&
                !terrain.is_property_allowing_move(PropId::flying);

        map::g_terrain_blocks_tiny_flying.at(p) =
                blocks_walking &&
                !terrain.is_property_allowing_move(PropId::tiny_flying);

        map::g_terrain_blocks_ethereal.at(p) =
                blocks_walking &&
                !terrain.is_property_allowing_move(PropId::ethereal);

        map::g_terrain_blocks_ooze.at(p) =
                blocks_walking &&
                !terrain.is_property_allowing_move(PropId::ooze);

        map::g_terrain_blocks_small_crawling.at(p) =
                blocks_walking &&
                !terrain.is_property_allowing_move(PropId::small_crawling);

        map::g_terrain_blocks_burrowing.at(p) =
                blocks_walking &&
                !terrain.is_property_allowing_move(PropId::burrowing);

        map::g_terrain_blocks_los.at(p) =
                !terrain.is_los_passable();
}

// This function can only set blocking status from false to true.
static void append_map_blocking_info_for_terrain(
        const terrain::Terrain& terrain)
{
        const auto& p = terrain.pos();

        const bool blocks_walking = !terrain.is_walkable();

        if (!map::g_terrain_blocks_walking.at(p))
        {
                map::g_terrain_blocks_walking.at(p) =
                        blocks_walking;
        }

        if (!map::g_terrain_blocks_flying.at(p))
        {
                map::g_terrain_blocks_flying.at(p) =
                        blocks_walking &&
                        !terrain.is_property_allowing_move(
                                PropId::flying);
        }

        if (!map::g_terrain_blocks_tiny_flying.at(p))
        {
                map::g_terrain_blocks_tiny_flying.at(p) =
                        blocks_walking &&
                        !terrain.is_property_allowing_move(
                                PropId::tiny_flying);
        }

        if (!map::g_terrain_blocks_ethereal.at(p))
        {
                map::g_terrain_blocks_ethereal.at(p) =
                        blocks_walking &&
                        !terrain.is_property_allowing_move(
                                PropId::ethereal);
        }

        if (!map::g_terrain_blocks_ooze.at(p))
        {
                map::g_terrain_blocks_ooze.at(p) =
                        blocks_walking &&
                        !terrain.is_property_allowing_move(
                                PropId::ooze);
        }

        if (!map::g_terrain_blocks_small_crawling.at(p))
        {
                map::g_terrain_blocks_small_crawling.at(p) =
                        blocks_walking &&
                        !terrain.is_property_allowing_move(
                                PropId::small_crawling);
        }

        if (!map::g_terrain_blocks_burrowing.at(p))
        {
                map::g_terrain_blocks_burrowing.at(p) =
                        blocks_walking &&
                        !terrain.is_property_allowing_move(
                                PropId::burrowing);
        }

        if (!map::g_terrain_blocks_los.at(p))
        {
                map::g_terrain_blocks_los.at(p) =
                        !terrain.is_los_passable();
        }
}

// -----------------------------------------------------------------------------
// ChokePointData
// -----------------------------------------------------------------------------
ChokePointData::ChokePointData(const ChokePointData& other) :
        p(other.p),
        player_side(other.player_side),
        stairs_side(other.stairs_side)
{
        sides[0] = other.sides[0];
        sides[1] = other.sides[1];
}

ChokePointData& ChokePointData::operator=(const ChokePointData& other)
{
        if (&other == this)
        {
                return *this;
        }

        p = other.p;

        player_side = other.player_side;
        stairs_side = other.stairs_side;

        sides[0] = other.sides[0];
        sides[1] = other.sides[1];

        return *this;
}

// -----------------------------------------------------------------------------
// map
// -----------------------------------------------------------------------------
namespace map
{
Array2<bool> g_explored(0, 0);
Array2<bool> g_seen(0, 0);
Array2<LosResult> g_los(0, 0);
Array2<bool> g_light(0, 0);
Array2<bool> g_dark(0, 0);
Array2<smell::Smell> g_smell(0, 0);
Array2<smell::Smell> g_smell_spread(0, 0);
Array2<item::Item*> g_items(0, 0);
Array2<PlayerMemoryItem> g_item_memory(0, 0);
Array2<terrain::Terrain*> g_terrain(0, 0);
Array2<PlayerMemoryTerrain> g_terrain_memory(0, 0);
Array2<bool> g_terrain_blocks_walking(0, 0);
Array2<bool> g_terrain_blocks_flying(0, 0);
Array2<bool> g_terrain_blocks_tiny_flying(0, 0);
Array2<bool> g_terrain_blocks_ethereal(0, 0);
Array2<bool> g_terrain_blocks_ooze(0, 0);
Array2<bool> g_terrain_blocks_small_crawling(0, 0);
Array2<bool> g_terrain_blocks_burrowing(0, 0);
Array2<bool> g_terrain_blocks_los(0, 0);

actor::Player* g_player = nullptr;

int g_dlvl = 0;

Color g_wall_color;

std::vector<Room*> g_room_list;

Array2<Room*> g_room_map(0, 0);

std::vector<ChokePointData> g_choke_point_data;

void init()
{
        g_dlvl = 0;

        g_room_list.clear();

        actor::Actor* actor = actor::make(actor::Id::player, {0, 0});

        g_player = static_cast<actor::Player*>(actor);
}

void cleanup()
{
        reset({0, 0});

        // NOTE: The player object is deleted elsewhere
        g_player = nullptr;
}

void save()
{
        saving::put_int(g_dlvl);
}

void load()
{
        g_dlvl = saving::get_int();
}

void reset(const P& dims)
{
        free_layers_owned_memory();

        s_dims = dims;
        resize_arrays();
        init_arrays_data();

        const int map_w = w();
        const int map_h = h();

        for (int x = 0; x < map_w; ++x)
        {
                for (int y = 0; y < map_h; ++y)
                {
                        auto* const wall =
                                terrain::make(terrain::Id::wall, {x, y});

                        set_terrain(wall);
                }
        }

        actor::delete_all_mon();
        game_time::erase_all_mobs();
        game_time::reset_current_actor_idx();

        for (auto* room : g_room_list)
        {
                delete room;
        }

        g_room_list.clear();

        g_room_map.resize(s_dims);

        g_choke_point_data.clear();

        const std::vector<Color> wall_color_bucket = {
                colors::gray(),
                colors::teal().shaded(25),
                colors::red(),
                colors::sepia(),
                colors::dark_sepia(),
                colors::dark_brown(),
                colors::gray_brown(),
        };

        g_wall_color = rnd::element(wall_color_bucket);
}

int w()
{
        return s_dims.x;
}

int h()
{
        return s_dims.y;
}

const P& dims()
{
        return s_dims;
}

R rect()
{
        return {{0, 0}, s_dims - 1};
}

size_t nr_positions()
{
        return (size_t)s_dims.x * (size_t)s_dims.y;
}

void update_map_info()
{
        for (int x = 0; x < s_dims.x; ++x)
        {
                for (int y = 0; y < s_dims.y; ++y)
                {
                        const P pos(x, y);

                        if (map::is_pos_inside_outer_walls(pos))
                        {
                                const terrain::Terrain& terrain =
                                        *g_terrain.at(pos);

                                set_map_blocking_info_for_terrain(terrain);
                        }
                        else
                        {
                                set_map_blocking_info_true_for_all_at(pos);
                        }
                }
        }

        for (const terrain::Terrain* const mob : game_time::g_mobs)
        {
                append_map_blocking_info_for_terrain(*mob);
        }
}

void update_map_info_for_terrain_at(const P& pos)
{
        if (!map::is_pos_inside_outer_walls(pos))
        {
                set_map_blocking_info_true_for_all_at(pos);

                return;
        }

        const terrain::Terrain& terrain = *g_terrain.at(pos);

        set_map_blocking_info_for_terrain(terrain);

        const std::vector<terrain::Terrain*> mobs = game_time::mobs_at(pos);

        for (const terrain::Terrain* const mob : mobs)
        {
                append_map_blocking_info_for_terrain(*mob);
        }
}

void update_vision()
{
        update_light_map();

        g_player->update_fov();

        g_player->update_mon_awareness();

        update_player_memory();

        minimap::update();

        states::draw();
}

void update_player_memory()
{
        const int map_w = w();
        const int map_h = h();

        for (int x = 0; x < map_w; ++x)
        {
                for (int y = 0; y < map_h; ++y)
                {
                        const P p(x, y);

                        if (!g_seen.at(p))
                        {
                                continue;
                        }

                        clear_player_memory_at(p);

                        memorize_terrain_at(p);

                        memorize_item_at(p);
                }
        }
}

void update_terrain(terrain::Terrain* terrain)
{
        set_terrain(terrain);

        terrain->on_placed();

        update_map_info_for_terrain_at(terrain->pos());

        // TODO: This is a very paranoid approach. Updating could be done more
        // selectively (for example check if the updated terrain could possibly
        // change light levels), which might help improve performance.
        update_light_map();
}

void set_terrain(terrain::Terrain* terrain)
{
        ASSERT(terrain);

        const auto p = terrain->pos();

        terrain::Terrain* const prev_terrain = g_terrain.at(p);

        ASSERT(prev_terrain != terrain);

        delete prev_terrain;

        g_terrain.at(p) = terrain;
}

void memorize_terrain_at(const P& p)
{
        const auto* const terrain = g_terrain.at(p);
        auto& memory = g_terrain_memory.at(p);
        const auto id = terrain->id();
        const bool blocks_walking = !terrain->is_walkable();
        const auto minimap_wall_color = colors::sepia();

        if (id == terrain::Id::stairs)
        {
                memory.appearance.minimap_color = colors::yellow();
        }
        else if (id == terrain::Id::door)
        {
                const auto* const door =
                        static_cast<const terrain::Door*>(terrain);

                if (door->is_hidden())
                {
                        memory.appearance.minimap_color = minimap_wall_color;
                }
                else
                {
                        if (door->type() == terrain::DoorType::metal)
                        {
                                memory.appearance.minimap_color =
                                        colors::light_teal();
                        }
                        else
                        {
                                memory.appearance.minimap_color =
                                        colors::light_white();
                        }
                }
        }
        else if (id == terrain::Id::lever)
        {
                memory.appearance.minimap_color = colors::teal();
        }
        else if (id == terrain::Id::liquid)
        {
                memory.appearance.minimap_color = colors::blue();
        }
        else if (blocks_walking)
        {
                memory.appearance.minimap_color = minimap_wall_color;
        }
        else
        {
                memory.appearance.minimap_color = colors::dark_gray_brown();
        }

        const bool is_dark = g_dark.at(p);

        const bool blocks_los =
                map_parsers::BlocksLos().run(p);

        const bool allow_memorize_terrain =
                !is_dark ||
                blocks_los ||
                blocks_walking ||
                (id == terrain::Id::door) ||
                (id == terrain::Id::liquid);

        if (allow_memorize_terrain)
        {
                const std::string name =
                        text_format::first_to_upper(
                                terrain->name(Article::a));

                memory.id = terrain->id();
                memory.blocks_walking = blocks_walking;

                memory.appearance.tile = terrain->tile();
                memory.appearance.character = terrain->character();
                memory.appearance.name = name;
                memory.appearance.color = terrain->color();
        }
}

void memorize_item_at(const P& p)
{
        const auto* const item = g_items.at(p);

        auto& memory = g_item_memory.at(p);

        if (!item)
        {
                memory = {};

                return;
        }

        const std::string name =
                text_format::first_to_upper(
                        item->name(
                                ItemNameType::plural,
                                ItemNameInfo::yes,
                                ItemNameAttackInfo::main_attack_mode));

        memory.id = item->id();

        memory.appearance.tile = item->tile();
        memory.appearance.character = item->character();
        memory.appearance.name = name;
        memory.appearance.color = item->color();

        memory.appearance.minimap_color = colors::light_magenta();

        if ((item->data().type == ItemType::ranged_wpn) &&
            !item->data().ranged.has_infinite_ammo)
        {
                const auto* wpn = static_cast<const item::Wpn*>(item);

                if (wpn->m_ammo_loaded == 0)
                {
                        memory.appearance.minimap_color = colors::magenta();
                }
        }
}

void clear_player_memory_at(const P& p)
{
        map::g_terrain_memory.at(p) = {};

        map::g_terrain_memory.at(p).appearance.minimap_color =
                colors::black();

        map::g_item_memory.at(p) = {};
}

void update_light_map()
{
        Array2<bool> light_tmp(dims());

        for (const auto* const a : game_time::g_actors)
        {
                a->add_light(light_tmp);
        }

        for (const auto* const m : game_time::g_mobs)
        {
                m->add_light(light_tmp);
        }

        for (auto* const terrain : map::g_terrain)
        {
                terrain->add_light(light_tmp);
        }

        // Copy the temporary buffer to the real light map

        // TODO: Maybe just use the Array2 "=" operator, which does std::copy,
        // this should be as fast as memcpy...
        memcpy(g_light.data(), light_tmp.data(), g_light.length());
}

void delete_and_remove_room_from_list(Room* const room)
{
        for (size_t i = 0; i < g_room_list.size(); ++i)
        {
                if (g_room_list[i] == room)
                {
                        delete room;
                        g_room_list.erase(std::begin(g_room_list) + (int)i);
                        return;
                }
        }

        ASSERT(false && "Tried to remove non-existing room");
}

const Array2<bool>& get_blocked_map_info_for_actor(const actor::Actor& actor)
{
        const auto& props = actor.m_properties;

        if (props.has(PropId::ethereal))
        {
                return g_terrain_blocks_ethereal;
        }
        else if (props.has(PropId::tiny_flying))
        {
                return g_terrain_blocks_tiny_flying;
        }
        else if (props.has(PropId::flying))
        {
                return g_terrain_blocks_flying;
        }
        else if (props.has(PropId::ooze))
        {
                return g_terrain_blocks_ooze;
        }
        else if (props.has(PropId::small_crawling))
        {
                return g_terrain_blocks_small_crawling;
        }
        else
        {
                return g_terrain_blocks_walking;
        }
}

bool can_actor_move_into_terrain_at(const actor::Actor& actor, const P& pos)
{
        return !get_blocked_map_info_for_actor(actor).at(pos);
}

actor::Actor* living_actor_at(const P& pos)
{
        for (auto* const actor : game_time::g_actors)
        {
                if ((actor->m_pos == pos) && actor->is_alive())
                {
                        return actor;
                }
        }

        return nullptr;
}

actor::Actor* first_corpse_at(const P& pos)
{
        for (auto* const actor : game_time::g_actors)
        {
                if ((actor->m_pos == pos) &&
                    (actor->m_state == ActorState::corpse))
                {
                        return actor;
                }
        }

        return nullptr;
}

terrain::Terrain* first_mob_at_pos(const P& pos)
{
        for (auto* const mob : game_time::g_mobs)
        {
                if (mob->pos() == pos)
                {
                        return mob;
                }
        }

        return nullptr;
}

actor::Actor* random_closest_actor(
        const P& c,
        const std::vector<actor::Actor*>& actors)
{
        if (actors.empty())
        {
                return nullptr;
        }

        if (actors.size() == 1)
        {
                return actors[0];
        }

        // Find distance to nearest actor(s)
        int dist_to_nearest = INT_MAX;

        for (auto* actor : actors)
        {
                const int current_dist = king_dist(c, actor->m_pos);

                if (current_dist < dist_to_nearest)
                {
                        dist_to_nearest = current_dist;
                }
        }

        ASSERT(dist_to_nearest != INT_MAX);

        // Store all actors with distance equal to the nearest distance
        std::vector<actor::Actor*> closest_actors;

        for (auto* actor : actors)
        {
                if (king_dist(c, actor->m_pos) == dist_to_nearest)
                {
                        closest_actors.push_back(actor);
                }
        }

        ASSERT(!closest_actors.empty());

        return rnd::element(closest_actors);
}

bool is_pos_inside_map(const P& pos)
{
        return (
                (pos.x >= 0) &&
                (pos.y >= 0) &&
                (pos.x < w()) &&
                (pos.y < h()));
}

bool is_pos_inside_outer_walls(const P& pos)
{
        return (
                (pos.x > 0) &&
                (pos.y > 0) &&
                (pos.x < (w() - 1)) &&
                (pos.y < (h() - 1)));
}

bool is_area_inside_map(const R& area)
{
        return is_pos_inside_map(area.p0) && is_pos_inside_map(area.p1);
}

}  // namespace map

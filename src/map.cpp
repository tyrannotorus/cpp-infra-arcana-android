// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
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
#include "text_format.hpp"

#ifndef NDEBUG
#include "viewport.hpp"
#endif  // NDEBUG

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static P s_dims(0, 0);

static void init_layers_data()
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
                map::g_items_memory.at(i) = {};
                map::g_terrain.at(i) = nullptr;
                map::g_terrain_memory.at(i) = {};
        }
}

static void resize_layers()
{
        map::g_seen.resize_no_init(s_dims);
        map::g_los.resize_no_init(s_dims);
        map::g_light.resize_no_init(s_dims);
        map::g_dark.resize_no_init(s_dims);
        map::g_smell.resize_no_init(s_dims);
        map::g_smell_spread.resize_no_init(s_dims);
        map::g_items.resize_no_init(s_dims);
        map::g_items_memory.resize_no_init(s_dims);
        map::g_terrain.resize_no_init(s_dims);
        map::g_terrain_memory.resize_no_init(s_dims);
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
Array2<PlayerMapMemoryData> g_items_memory(0, 0);
Array2<terrain::Terrain*> g_terrain(0, 0);
Array2<PlayerMapMemoryData> g_terrain_memory(0, 0);

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
        resize_layers();
        init_layers_data();

        for (int x = 0; x < w(); ++x)
        {
                for (int y = 0; y < h(); ++y)
                {
                        put(new terrain::Wall({x, y}));
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

        // Occasionally set wall color to something unusual
        if (rnd::one_in(3))
        {
                const std::vector<Color> wall_color_bucket = {
                        colors::red(),
                        colors::sepia(),
                        colors::dark_sepia(),
                        colors::dark_brown(),
                        colors::gray_brown(),
                };

                g_wall_color = rnd::element(wall_color_bucket);
        }
        else
        {
                // Standard wall color
                g_wall_color = colors::gray();
        }
}

int w()
{
        return s_dims.x;
}

int h()
{
        return s_dims.y;
}

P dims()
{
        return s_dims;
}

R rect()
{
        return R({0, 0}, s_dims - 1);
}

size_t nr_positions()
{
        return s_dims.x * s_dims.y;
}

terrain::Terrain* put(terrain::Terrain* const t)
{
        ASSERT(t);

        const auto p = t->pos();

        auto& terrain_ref = g_terrain.at(p);

        delete terrain_ref;

        terrain_ref = t;

#ifndef NDEBUG
        if (init::g_is_demo_mapgen)
        {
                if (t->id() == terrain::Id::floor)
                {
                        viewport::show(p, viewport::ForceCentering::no);

                        for (auto& seen : g_seen)
                        {
                                seen = true;
                        }

                        for (auto& explored : g_explored)
                        {
                                explored = true;
                        }

                        states::draw();

                        io::draw_symbol(
                                gfx::TileId::aim_marker_line,
                                'X',
                                Panel::map,
                                viewport::to_view_pos(p),
                                colors::yellow());

                        io::update_screen();

                        // NOTE: Delay must be > 1 for user input to be read
                        io::sleep(3);
                }
        }
#endif  // NDEBUG

        t->on_placed();

        return t;
}

void update_vision()
{
        game_time::update_light_map();

        g_player->update_fov();

        g_player->update_mon_awareness();

        update_player_memory();

        states::draw();
}

void update_player_memory()
{
        for (int x = 0; x < w(); ++x)
        {
                for (int y = 0; y < h(); ++y)
                {
                        const P p(x, y);

                        if (!g_seen.at(p))
                        {
                                continue;
                        }

                        clear_player_memory_at(p);

                        const bool is_dark = g_dark.at(p);

                        const bool blocks_los =
                                map_parsers::BlocksLos().run(p);

                        const bool blocks_walking =
                                map_parsers::BlocksWalking(ParseActors::no)
                                        .run(p);

                        const bool is_door =
                                g_terrain.at(p)->id() ==
                                terrain::Id::door;

                        const bool allow_memorize_terrain =
                                !is_dark ||
                                blocks_los ||
                                blocks_walking ||
                                is_door;

                        if (allow_memorize_terrain)
                        {
                                memorize_terrain_at(p);
                        }

                        memorize_item_at(p);
                }
        }
}

void memorize_terrain_at(const P& p)
{
        const auto* const terrain = g_terrain.at(p);

        auto& memory = g_terrain_memory.at(p);

        memory.tile = terrain->tile();

        memory.character = terrain->character();

        memory.name = terrain->name(Article::a);

        memory.name = text_format::first_to_upper(memory.name);

        memory.color = terrain->color();
}

void memorize_item_at(const P& p)
{
        const auto* const item = g_items.at(p);

        auto& memory = g_items_memory.at(p);

        if (!item)
        {
                memory = {};

                return;
        }

        memory.tile = item->tile();

        memory.character = item->character();

        memory.name =
                item->name(
                        ItemNameType::plural,
                        ItemNameInfo::yes,
                        ItemNameAttackInfo::main_attack_mode);

        memory.name = text_format::first_to_upper(memory.name);

        memory.color = item->color();
}

void clear_player_memory_at(const P& p)
{
        map::g_terrain_memory.at(p) = {};
        map::g_items_memory.at(p) = {};
}

void make_blood(const P& origin)
{
        for (const auto& d : dir_utils::g_dir_list_w_center)
        {
                if (!rnd::one_in(3))
                {
                        continue;
                }

                const auto p = origin + d;

                g_terrain.at(p)->try_make_bloody();
        }
}

void make_gore(const P& origin)
{
        for (int dx = -1; dx <= 1; ++dx)
        {
                for (int dy = -1; dy <= 1; ++dy)
                {
                        const auto c = origin + P(dx, dy);

                        if (rnd::one_in(3))
                        {
                                g_terrain.at(c)->try_put_gore();
                        }
                }
        }
}

void delete_and_remove_room_from_list(Room* const room)
{
        for (size_t i = 0; i < g_room_list.size(); ++i)
        {
                if (g_room_list[i] == room)
                {
                        delete room;
                        g_room_list.erase(g_room_list.begin() + i);
                        return;
                }
        }

        ASSERT(false && "Tried to remove non-existing room");
}

bool is_pos_seen_by_player(const P& p)
{
        ASSERT(is_pos_inside_map(p));

        return g_seen.at(p);
}

actor::Actor* first_actor_at_pos(const P& pos, ActorState state)
{
        for (auto* const actor : game_time::g_actors)
        {
                if ((actor->m_pos == pos) && (actor->m_state == state))
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

Array2<std::vector<actor::Actor*>> get_actor_array()
{
        Array2<std::vector<actor::Actor*>> a(dims());

        for (auto* actor : game_time::g_actors)
        {
                const P& p = actor->m_pos;

                a.at(p).push_back(actor);
        }

        return a;
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

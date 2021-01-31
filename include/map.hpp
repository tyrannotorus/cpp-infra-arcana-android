// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef MAP_HPP
#define MAP_HPP

#include <vector>

#include "colors.hpp"
#include "config.hpp"
#include "fov.hpp"
#include "pos.hpp"

namespace item
{
class Item;
}  // namespace item

namespace actor
{
class Actor;
class Player;
}  // namespace actor

namespace terrain
{
class Terrain;
}  // namespace terrain

class Room;

struct ChokePointData
{
        ChokePointData() = default;

        ChokePointData(const ChokePointData& other) :
                p(other.p),
                player_side(other.player_side),
                stairs_side(other.stairs_side)
        {
                sides[0] = other.sides[0];
                sides[1] = other.sides[1];
        }

        ChokePointData& operator=(const ChokePointData& other)
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

        P p {};

        // These shall only ever have a value of 0 or 1 (or -1 when undefined)
        int player_side {-1};
        int stairs_side {-1};

        std::vector<P> sides[2] {};
};

namespace map
{
extern Array2<bool> g_explored;
extern Array2<bool> g_seen;
extern Array2<LosResult> g_los;
extern Array2<bool> g_light;
extern Array2<bool> g_dark;
extern Array2<item::Item*> g_items;
extern Array2<terrain::Terrain*> g_terrain;

extern actor::Player* g_player;

extern int g_dlvl;

extern Color g_wall_color;

// This vector is the room memory owner
extern std::vector<Room*> g_room_list;

// Helper array, for convenience and optimization
extern Array2<Room*> g_room_map;

// NOTE: This data is only intended to be used for the purpose of map generation
// (and placing items etc), it is NOT updated while playing the map.
extern std::vector<ChokePointData> g_choke_point_data;

void init();
void cleanup();
void save();
void load();
void reset(const P& dims);

int w();
int h();
P dims();
R rect();
size_t nr_positions();

terrain::Terrain* put(terrain::Terrain* terrain);

// Updates light map, player fov (etc). This should be called when e.g. a door
// is closed, or a wall is destoyed.
void update_vision();

void make_blood(const P& origin);
void make_gore(const P& origin);

void delete_and_remove_room_from_list(Room* room);

bool is_pos_seen_by_player(const P& p);

actor::Actor* first_actor_at_pos(
        const P& pos,
        ActorState state = ActorState::alive);

terrain::Terrain* first_mob_at_pos(const P& pos);

Array2<std::vector<actor::Actor*>> get_actor_array();

actor::Actor* random_closest_actor(
        const P& c,
        const std::vector<actor::Actor*>& actors);

bool is_pos_inside_map(const P& pos);
bool is_pos_inside_outer_walls(const P& pos);
bool is_area_inside_map(const R& area);

}  // namespace map

#endif  // MAP_HPP

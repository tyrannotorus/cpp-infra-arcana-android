// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef MAPGEN_HPP
#define MAPGEN_HPP

#include <vector>

#include "array2.hpp"
#include "map.hpp"
#include "map_templates.hpp"

class Room;

struct Region
{
public:
        Region(const R& rect) :
                main_room(nullptr),
                r(rect),
                is_free(true) {}

        Region() :
                main_room(nullptr),

                is_free(true)
        {}

        R rnd_room_rect() const;

        Room* main_room;
        R r;
        bool is_free;
};

namespace mapgen
{
// This variable is checked at certain points to see if the current map has been
// flagged as "failed". Setting 'is_map_valid' to false will generally stop map
// generation, discard the map, and trigger generation of a new map.
extern bool g_is_map_valid;

// All cells marked as true in this array will be considered for door placement
extern Array2<bool> g_door_proposals;

// Standard dungeon level
// TODO: Consider moving to MapBuilderStd
bool make_std_lvl();

//------------------------------------------------------------------------------
// Map generation steps (in no particular order)
//------------------------------------------------------------------------------
void merge_regions(Array2<Region>& regions);

void make_aux_rooms(Array2<Region>& regions);

void reserve_river(Array2<Region>& regions);

void make_sub_rooms();

void bsp_split_rooms();

void decorate();

bool allow_make_grate_at(const P& pos, const Array2<bool>& blocked);

void make_doors();

void make_warded_doors_and_keys();

void make_monoliths();

void make_pylons();

//------------------------------------------------------------------------------
// Room reshaping utils (called by the room objects)
//------------------------------------------------------------------------------
// NOTE: Some reshape functions below will not change the boundaries of the
// room, but may affect which cells belong to the room. This only affects the
// room map (in the "map" namespace), so the room parameter should be a const
// reference. For other reshape functions, the room may expand beyond its
// initial rectangle, so in those cases the functions need to modify the data of
// the room object.
void cut_room_corners(const Room& room);
void make_pillars_in_room(const Room& room);
void cavify_room(Room& room);

//------------------------------------------------------------------------------
// Room creation
//------------------------------------------------------------------------------
// NOTE: All "make_room..." functions handle all the necessary steps such as
// creating floor on the map, creating room objects and registering them, et c.
Room* make_room(Region& region);

Room* make_room(const R& r, IsSubRoom is_sub_room);

// Low level functions related to room creation - these are only necessary when
// creating rooms by other methods than the "make_room" functions above.
void register_room(Room& room);

void make_floor(const Room& room);

//------------------------------------------------------------------------------
// Misc utils
//------------------------------------------------------------------------------
void connect_rooms();

void valid_corridor_entries(
        const Room& room,
        std::vector<P>& out);

// Used for finding suitable door positions, i.e. positions such as:
// .#.
// .x.
// .#.
//
// ('#' = blocked, '.' = free)
//
// NOTE: Only the cells at or adjacent to 'pos' are checked. Therefore,
// 'blocked' may be just a tiny 3x3 array (if so, 'pos' must be the center
// position!), or it can be a full sized map.
//
bool is_passage(const P& pos, const Array2<bool>& blocked);

bool is_choke_point(
        const P& p,
        const Array2<bool>& blocked,
        ChokePointData* out);

void make_pathfind_corridor(
        Room& room_0,
        Room& room_1,
        Array2<bool>* door_proposals = nullptr);

std::vector<P> rnd_walk(
        const P& p0,
        int len,
        R area,
        bool allow_diagonal = true);

std::vector<P> pathfinder_walk(
        const P& p0,
        const P& p1,
        bool is_smooth);

// Generates a map of spawn chance weights, with emphasis on hidden, optional,
// or hard to reach areas - this can be used e.g. to place items or levers.
void make_explore_spawn_weights(
        const Array2<bool>& blocked,
        std::vector<P>& positions_out,
        std::vector<int>& weights_out);

Array2<bool> allowed_stair_cells();

void move_player_to_nearest_allowed_pos();

P make_stairs_at_random_pos();

void reveal_doors_on_path_to_stairs(const P& stairs_pos);

}  // namespace mapgen

#endif  // MAPGEN_HPP

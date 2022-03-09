// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include <cstddef>
#include <vector>

#include "array2.hpp"
#include "debug.hpp"
#include "global.hpp"
#include "map.hpp"
#include "mapgen.hpp"
#include "misc.hpp"
#include "pos.hpp"
#include "random.hpp"
#include "rect.hpp"
#include "room.hpp"
#include "terrain.hpp"
#include "terrain_data.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static const int s_nr_tries_to_make_room = 100;

// Min allowed size of the sub room, including the walls
static const P s_walls_min_dims(4, 4);

static const int s_min_outer_wall_gap = 1;

static bool is_large_room(const R& area)
{
        const int large_room_min_size = 15;

        const bool is_large_room =
                (area.w() >= large_room_min_size) &&
                (area.h() >= large_room_min_size);

        return is_large_room;
}

static void put_inner_wall(const P& pos)
{
        if (map::g_dlvl >= g_dlvl_first_late_game)
        {
                std::vector<int> terrain_weights = {
                        3,  // Wall
                        1,  // High rubble
                        1,  // Low rubble
                        3,  // Cave floor
                };

                terrain::Terrain* terrain = nullptr;

                switch (rnd::weighted_choice(terrain_weights))
                {
                case 0:
                {
                        terrain = new terrain::Wall(pos);
                }
                break;

                case 1:
                {
                        terrain = new terrain::RubbleHigh(pos);
                }
                break;

                case 2:
                {
                        terrain = new terrain::RubbleLow(pos);
                }
                break;

                case 3:
                {
                        auto* const floor = new terrain::Floor(pos);
                        floor->m_type = terrain::FloorType::cave;
                        terrain = floor;
                }
                break;

                default:
                {
                        ASSERT(false);
                        terrain = new terrain::Wall(pos);
                }
                break;
                }

                map::put(terrain);
        }
        else
        {
                // Not late game
                map::put(new terrain::Wall(pos));
        }
}

static bool is_pos_on_edge(const P& pos, const R& area)
{
        return (
                (pos.x == area.p0.x) ||
                (pos.x == area.p1.x) ||
                (pos.y == area.p0.y) ||
                (pos.y == area.p1.y));
}

static bool is_pos_on_corner(const P& pos, const R& area)
{
        const bool is_on_edge_x = (pos.x == area.p0.x) || (pos.x == area.p1.x);
        const bool is_on_edge_y = (pos.y == area.p0.y) || (pos.y == area.p1.y);

        return is_on_edge_x && is_on_edge_y;
}

static R get_random_inner_walls_area(
        const R& outer_room_area,
        const P& inner_walls_max_dims)
{
        const int walls_min_w = s_walls_min_dims.x;
        const int walls_min_h = s_walls_min_dims.y;
        const int walls_max_w = inner_walls_max_dims.x;
        const int walls_max_h = inner_walls_max_dims.y;

        const P inner_walls_dims(
                rnd::range(walls_min_w, walls_max_w),
                rnd::range(walls_min_h, walls_max_h));

        const int walls_w = inner_walls_dims.x;
        const int walls_h = inner_walls_dims.y;

        const int outer_x0 = outer_room_area.p0.x;
        const int outer_x1 = outer_room_area.p1.x;
        const int outer_y0 = outer_room_area.p0.y;
        const int outer_y1 = outer_room_area.p1.y;

        constexpr int min_gap = s_min_outer_wall_gap;

        const int x0_min = outer_x0 + min_gap;
        const int x0_max = outer_x1 - walls_w - min_gap + 1;
        const int y0_min = outer_y0 + min_gap;
        const int y0_max = outer_y1 - walls_h - min_gap + 1;

        const P inner_walls_p0(
                rnd::range(x0_min, x0_max),
                rnd::range(y0_min, y0_max));

        const auto inner_walls_p1 = inner_walls_p0 + inner_walls_dims - 1;

        const R inner_walls_area(inner_walls_p0, inner_walls_p1);

        ASSERT(map::is_pos_inside_map(inner_walls_p0));
        ASSERT(map::is_pos_inside_map(inner_walls_p1));

        return inner_walls_area;
}

static bool allow_put_inner_walls(
        const R& inner_walls_area,
        const Room& outer_room)
{
        // Rules to allow building:
        // * Cells belonging to the outer room must be floor
        // * Cells not belonging to the outer room must be walls

        const R expanded_area(
                inner_walls_area.p0 - 1,
                inner_walls_area.p1 + 1);

        for (const auto& p : expanded_area.positions())
        {
                if (!map::is_pos_inside_map(p))
                {
                        return false;
                }

                const auto f_id = map::g_terrain.at(p)->id();
                const auto* const room = map::g_room_map.at(p);
                const bool is_outer_oom = (room == &outer_room);

                if ((is_outer_oom && (f_id != terrain::Id::floor)) ||
                    (!is_outer_oom && (f_id != terrain::Id::wall)))
                {
                        return false;
                }
        }

        return true;
}

static bool try_make_inner_room(
        Room& outer_room,
        const P& inner_walls_max_dims)
{
        const auto& outer_room_area = outer_room.m_r;

        const auto inner_walls_area =
                get_random_inner_walls_area(
                        outer_room_area,
                        inner_walls_max_dims);

        ASSERT(map::is_pos_inside_map(inner_walls_area.p0));
        ASSERT(map::is_pos_inside_map(inner_walls_area.p1));

        if (!allow_put_inner_walls(inner_walls_area, outer_room))
        {
                return false;
        }

        // OK, we can build the inner room.

        const R sub_room_area(
                inner_walls_area.p0 + 1,
                inner_walls_area.p1 - 1);

        auto* const sub_room =
                mapgen::make_room(
                        sub_room_area,
                        IsSubRoom::yes);

        outer_room.m_sub_rooms.push_back(sub_room);

        // Make walls and entrance(s) for our new room
        std::vector<P> entrance_bucket;

        for (const auto& p : inner_walls_area.positions())
        {
                if (!is_pos_on_edge(p, inner_walls_area))
                {
                        // Position is not on the inner walls, do not put a wall
                        // or entrance here.
                        continue;
                }

                put_inner_wall(p);

                if (is_pos_on_corner(p, inner_walls_area))
                {
                        // Position is on a corner of the inner walls, do not
                        // put an entrance here.
                        continue;
                }

                entrance_bucket.push_back(p);
        }

        if (entrance_bucket.empty())
        {
                // Not possible to place an entrance to the inner room, Discard
                // this map.
                mapgen::g_is_map_valid = false;

                return false;
        }

        // Sometimes place one entrance, which may have a door (always do this
        // if there are very few possible entries), and sometimes place multiple
        // entrances without doors.
        if (rnd::coin_toss() || entrance_bucket.size() <= 4)
        {
                // One entrance that may have a door.
                const auto door_pos = rnd::element(entrance_bucket);

                map::put(new terrain::Floor(door_pos));

                mapgen::g_door_proposals.at(door_pos) = true;
        }
        else
        {
                // Place multiple "doorless" entrances.
                std::vector<P> positions_placed;
                const int nr_tries = rnd::range(1, 10);

                for (int i = 0; i < nr_tries; ++i)
                {
                        const auto try_p = rnd::element(entrance_bucket);

                        bool is_pos_ok = true;

                        // Never make two adjacent entrances.
                        for (P& prev_pos : positions_placed)
                        {
                                if (is_pos_adj(try_p, prev_pos, true))
                                {
                                        is_pos_ok = false;
                                        break;
                                }
                        }

                        if (is_pos_ok)
                        {
                                map::put(new terrain::Floor(try_p));
                                positions_placed.push_back(try_p);
                        }
                }
        }

        // This point reached means the room has been built
        return true;
}

static void make_sub_rooms_for(Room& outer_room)
{
        if (!outer_room.allow_sub_rooms())
        {
                return;
        }

        const auto outer_room_area = outer_room.m_r;
        const auto outer_room_dims = outer_room_area.dims();

        // Max sub room size, including the walls, in this outer room.
        //
        // The inner room must fit inside the outer room, with at least N number
        // of floor cells between the inner room walls and the outer room walls.
        const auto inner_walls_max_dims =
                outer_room_dims - (s_min_outer_wall_gap * 2);

        if (((inner_walls_max_dims.x < s_walls_min_dims.x)) ||
            ((inner_walls_max_dims.y < s_walls_min_dims.y)))
        {
                // We cannot even build the smallest possible inner room
                // inside this outer room - no point in trying.
                return;
        }

        // const bool is_outer_big =
        //         (outer_room_dims.x > 16) ||
        //         (outer_room_dims.y > 8);

        const bool is_outer_std_room =
                ((int)outer_room.m_type <
                 (int)RoomType::END_OF_STD_ROOMS);

        // To build a room inside a room, the outer room shall:
        // * Be a standard room, and
        // * Be a "big room" - but we occasionally allow "small rooms"
        if (!is_outer_std_room /* || (!is_outer_big && !rnd::one_in(4)) */)
        {
                // Outer room does not meet dimensions criteria - next room.
                return;
        }

        const int max_nr_sub_rooms = rnd::one_in(3) ? 1 : 7;

        for (int inner_nr = 0;
             inner_nr < max_nr_sub_rooms;
             ++inner_nr)
        {
                for (
                        int try_count = 0;
                        try_count < s_nr_tries_to_make_room;
                        ++try_count)
                {
                        const bool did_build =
                                try_make_inner_room(
                                        outer_room,
                                        inner_walls_max_dims);

                        if (!mapgen::g_is_map_valid)
                        {
                                return;
                        }

                        if (did_build)
                        {
                                break;
                        }
                }
        }
}

// -----------------------------------------------------------------------------
// mapgen
// -----------------------------------------------------------------------------
namespace mapgen
{
// Assumes that all rooms are rectangular
void make_sub_rooms()
{
        TRACE_FUNC_BEGIN;

        const bool is_late_game = (map::g_dlvl >= g_dlvl_first_late_game);

        // NOTE: We must iterate by index here since new rooms may be added.
        for (size_t i = 0; i < map::g_room_list.size(); ++i)
        {
                auto* const outer_room = map::g_room_list[i];

                // Put fewer sub rooms late game; If the outer room is not a
                // sub-room, and the outer room is not large, we might skip
                // trying to place sub-rooms in this room. (If the outer room is
                // a sub-room, then we can go ahead and place as many rooms as
                // possible within it.)

                if (is_late_game &&
                    !outer_room->m_is_sub_room &&
                    !is_large_room(outer_room->m_r) &&
                    !rnd::one_in(3))
                {
                        continue;
                }

                make_sub_rooms_for(*outer_room);
        }

        TRACE_FUNC_END;

}  // make_sub_rooms

}  // namespace mapgen

// =============================================================================
// Copyright 2011-2023 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include <algorithm>
#include <cstddef>
#include <vector>

#include "array2.hpp"
#include "colors.hpp"
#include "debug.hpp"
#include "map.hpp"
#include "map_parsing.hpp"
#include "mapgen.hpp"
#include "panel.hpp"
#include "pos.hpp"
#include "random.hpp"
#include "rect.hpp"
#include "room.hpp"
#include "state.hpp"
#include "terrain.hpp"
#include "terrain_data.hpp"

#ifndef NDEBUG
#include "init.hpp"
#include "io.hpp"
#endif  // NDEBUG

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static bool is_standard_room_type(const Room& r)
{
        return (r.m_type < RoomType::END_OF_STD_ROOMS);
}

static bool is_rooms_directly_connected(
        const Room* const room0,
        const Room* const room1)
{
        const std::vector<Room*>& room0_connections = room0->m_rooms_con_to;

        return (
                std::find(
                        std::cbegin(room0_connections),
                        std::cend(room0_connections),
                        room1) != std::end(room0_connections));
}

static bool is_other_room_between_rooms(
        const Room* const room0,
        const Room* const room1)
{
        const P c0(room0->m_r.center());
        const P c1(room1->m_r.center());

        const int x0 = std::min(c0.x, c1.x);
        const int y0 = std::min(c0.y, c1.y);
        const int x1 = std::max(c0.x, c1.x);
        const int y1 = std::max(c0.y, c1.y);

        for (int x = x0; x <= x1; ++x) {
                for (int y = y0; y <= y1; ++y) {
                        const Room* const room_here = map::g_room_map.at(x, y);

                        // TODO: Corridors should perhaps no longer be an
                        // exception now if they are more like regular rooms.
                        if (room_here &&
                            (room_here != room0) &&
                            (room_here != room1) &&
                            (room_here->m_type != RoomType::corridor) &&
                            !room_here->m_is_sub_room) {
                                return true;
                        }
                }
        }

        return false;
}

static void mark_doors_as_non_blocking(Array2<bool>& blocked)
{
        const size_t nr_positions = map::nr_positions();
        for (size_t i = 0; i < nr_positions; ++i) {
                const auto id = map::g_terrain.at(i)->id();

                if (id == terrain::Id::door) {
                        blocked.at(i) = false;
                }
        }
}

static bool try_make_random_connection()
{
        Room* room0 = rnd::element(map::g_room_list);

        // Room 0 must be a connectable room type (an actual room), but
        // occasionally a corridor is allowed.
        const bool is_room_0_corridor = (room0->m_type == RoomType::corridor);

        if (!is_standard_room_type(*room0) &&
            !(is_room_0_corridor && rnd::one_in(4))) {
                return false;
        }

        // Finding second room to connect to.
        Room* room1 = rnd::element(map::g_room_list);

        // Room 1 must not be the same as room 0, and it must be a connectable
        // room (connections are only allowed between two standard rooms, or
        // from a corridor link to a standard room - never between two corridor
        // links).
        while ((room1 == room0) || !is_standard_room_type(*room1)) {
                room1 = rnd::element(map::g_room_list);
        }

        // Do not allow two rooms to be connected twice.
        if (is_rooms_directly_connected(room0, room1)) {
                return false;
        }

        // Do not connect room 0 and 1 if another room (except for sub rooms)
        // lies anywhere in a rectangle defined by the two center points of
        // those rooms.
        if (is_other_room_between_rooms(room0, room1)) {
                // Blocked by room between, trying other combination.
                return false;
        }

        // OK, let's try to connect these rooms.
        const bool did_connect =
                mapgen::try_make_pathfind_corridor(
                        *room0,
                        *room1,
                        &mapgen::g_door_proposals);

        return did_connect;
}

// -----------------------------------------------------------------------------
// mapgen
// -----------------------------------------------------------------------------
namespace mapgen
{
void connect_rooms()
{
        TRACE_FUNC_BEGIN;

        int nr_tries_left = 5000;

        while (true) {
                // NOTE: Keep this counter at the top of the loop, since
                // otherwise a continue statement could bypass it so we get
                // stuck in the loop.
                --nr_tries_left;

                if (nr_tries_left == 0) {
                        mapgen::g_is_map_valid = false;

                        break;
                }

                const bool did_connect = try_make_random_connection();

                if (!did_connect) {
                        continue;
                }

                Array2<bool> blocked(map::dims());

                map_parsers::BlocksWalking(ParseActors::no)
                        .run(blocked, blocked.rect());

                // Do not consider doors blocking.
                mark_doors_as_non_blocking(blocked);

                if (map_parsers::is_map_connected(blocked)) {
                        // OK, we have enough connections for a playable map
                        // (whole map is connected)! But keep making more
                        // connections with a random chance to stop - it's
                        // typically more fun with a lot of connections so the
                        // map isn't so linear.
                        if ((nr_tries_left <= 2) || (rnd::one_in(4))) {
                                break;
                        }
                }
        }

        TRACE_FUNC_END;
}

}  // namespace mapgen

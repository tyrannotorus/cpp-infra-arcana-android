// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
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

                auto rnd_room = []() {
                        return rnd::element(map::g_room_list);
                };

                // Standard rooms are connectable
                auto is_connectable_room = [](const Room& r) {
                        return r.m_type < RoomType::END_OF_STD_ROOMS;
                };

                Room* room0 = rnd_room();

                // Room 0 must be a connectable room, or a corridor link
                if (!is_connectable_room(*room0) &&
                    room0->m_type != RoomType::corr_link) {
                        continue;
                }

                // Finding second room to connect to
                Room* room1 = rnd_room();

                // Room 1 must not be the same as room 0, and it must be a
                // connectable room (connections are only allowed between two
                // standard rooms, or from a corridor link to a standard room -
                // never between two corridor links)
                while ((room1 == room0) || !is_connectable_room(*room1)) {
                        room1 = rnd_room();
                }

                // Do not allow two rooms to be connected twice
                const auto& room0_connections = room0->m_rooms_con_to;

                if (find(room0_connections.begin(),
                         room0_connections.end(),
                         room1) != room0_connections.end()) {
                        // Rooms are already connected, trying other combination
                        continue;
                }

                // Do not connect room 0 and 1 if another room (except for
                // sub rooms) lies anywhere in a rectangle defined by the two
                // center points of those rooms.
                bool is_other_room_in_way = false;

                const P c0(room0->m_r.center());
                const P c1(room1->m_r.center());

                const int x0 = std::min(c0.x, c1.x);
                const int y0 = std::min(c0.y, c1.y);
                const int x1 = std::max(c0.x, c1.x);
                const int y1 = std::max(c0.y, c1.y);

                for (int x = x0; x <= x1; ++x) {
                        for (int y = y0; y <= y1; ++y) {
                                const Room* const room_here =
                                        map::g_room_map.at(x, y);

                                if (room_here &&
                                    room_here != room0 &&
                                    room_here != room1 &&
                                    !room_here->m_is_sub_room) {
                                        is_other_room_in_way = true;
                                        break;
                                }
                        }

                        if (is_other_room_in_way) {
                                break;
                        }
                }

                if (is_other_room_in_way) {
                        // Blocked by room between, trying other combination
                        continue;
                }

                // Alright, let's try to connect these rooms
                make_pathfind_corridor(
                        *room0,
                        *room1,
                        &g_door_proposals);

                Array2<bool> blocked(map::dims());

                map_parsers::BlocksWalking(ParseActors::no)
                        .run(blocked, blocked.rect());

                // Do not consider doors blocking
                const size_t nr_positions = map::nr_positions();
                for (size_t i = 0; i < nr_positions; ++i) {
                        const auto id = map::g_terrain.at(i)->id();

                        if (id == terrain::Id::door) {
                                blocked.at(i) = false;
                        }
                }

                if ((nr_tries_left <= 2 || rnd::one_in(4)) &&
                    map_parsers::is_map_connected(blocked)) {
                        break;
                }
        }

        TRACE_FUNC_END;
}

}  // namespace mapgen

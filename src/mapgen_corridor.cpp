// =============================================================================
// Copyright 2011-2023 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include <algorithm>
#include <climits>
#include <cstring>
#include <iterator>
#include <ostream>
#include <utility>
#include <vector>

#include "actor.hpp"
#include "array2.hpp"
#include "debug.hpp"
#include "direction.hpp"
#include "flood.hpp"
#include "game_time.hpp"
#include "global.hpp"
#include "map.hpp"
#include "map_parsing.hpp"
#include "mapgen.hpp"
#include "misc.hpp"
#include "pathfind.hpp"
#include "pos.hpp"
#include "random.hpp"
#include "rect.hpp"
#include "room.hpp"
#include "terrain.hpp"
#include "terrain_data.hpp"
#include "terrain_door.hpp"
#include "terrain_factory.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static int find_shortest_entry_point_king_dist(
        const std::vector<P>& p0_bucket,
        const std::vector<P>& p1_bucket)
{
        int shortest_dist = INT_MAX;

        for (const P& p0 : p0_bucket) {
                for (const P& p1 : p1_bucket) {
                        const int dist = king_dist(p0, p1);

                        if (dist < shortest_dist) {
                                shortest_dist = dist;
                        }
                }
        }

        return shortest_dist;
}

static std::vector<std::pair<P, P>> get_entries_pairs_with_shortest_dist(
        const std::vector<P>& p0_bucket,
        const std::vector<P>& p1_bucket,
        const int shortest_dist)
{
        std::vector<std::pair<P, P>> closest_entry_pairs;

        for (const P& p0 : p0_bucket) {
                for (const P& p1 : p1_bucket) {
                        const int dist = king_dist(p0, p1);

                        if (dist == shortest_dist) {
                                closest_entry_pairs.emplace_back(p0, p1);
                        }
                }
        }

        return closest_entry_pairs;
}

// NOTE: This function only cares about possible entries for this room by itself
// - it may for example return entry point positions that are actually adjacent
// to floors in other rooms.
static void valid_corridor_entries(const Room& room, std::vector<P>& out)
{
        TRACE_FUNC_BEGIN_VERBOSE;

        // Find all positions around the room, that meets ALL of the following criteria:
        //  (1) Is a wall position
        //  (2) Is a position not belonging to any room
        //  (3) Is not on the edge of the map
        //  (4) Is cardinally adjacent to a floor position belonging to the room
        //  (5) Is cardinally adjacent to a position not in the room or room outline

        out.clear();

        Array2<bool> room_positions(map::dims());
        Array2<bool> room_floor_positions(map::dims());

        const size_t nr_positions = map::nr_positions();
        for (size_t i = 0; i < nr_positions; ++i) {
                const bool is_room_position = map::g_room_map.at(i) == &room;

                room_positions.at(i) = is_room_position;

                const terrain::Terrain* const t = map::g_terrain.at(i);

                room_floor_positions.at(i) = is_room_position && (t->id() == terrain::Id::floor);
        }

        const Array2<bool> room_positions_expanded =
                map_parsers::expand(
                        room_positions,
                        {{room.m_r.p0 - 2}, {room.m_r.p1 + 2}});

        for (int y = room.m_r.p0.y - 1; y <= room.m_r.p1.y + 1; ++y) {
                for (int x = room.m_r.p0.x - 1; x <= room.m_r.p1.x + 1; ++x) {
                        // Condition (1)
                        if (map::g_terrain.at(x, y)->id() != terrain::Id::wall) {
                                continue;
                        }

                        // Condition (2)
                        if (map::g_room_map.at(x, y)) {
                                continue;
                        }

                        // Condition (3)
                        if ((x <= 1) ||
                            (y <= 1) ||
                            (x >= (map::w() - 2)) ||
                            (y >= (map::h() - 2))) {
                                continue;
                        }

                        bool is_adj_to_floor_in_room = false;
                        bool is_adj_to_position_outside = false;

                        const P p(x, y);

                        bool is_adj_to_floor_not_in_room = false;

                        for (const P& d : dir_utils::g_cardinal_list) {
                                const P& p_adj(p + d);

                                // Condition (4)
                                if (room_floor_positions.at(p_adj)) {
                                        is_adj_to_floor_in_room = true;
                                }

                                // Condition (5)
                                if (!room_positions_expanded.at(p_adj)) {
                                        is_adj_to_position_outside = true;
                                }
                        }

                        if (!is_adj_to_floor_not_in_room &&
                            is_adj_to_floor_in_room &&
                            is_adj_to_position_outside) {
                                out.push_back(p);
                        }
                }
        }

        TRACE_FUNC_END_VERBOSE;
}

static bool is_adjacent_to_blocked_position_not_in_rooms(
        const P& pos,
        const Room& room_0,
        const Room& room_1,
        const Array2<bool>& blocked)
{
        for (const P& d : dir_utils::g_dir_list) {
                const P p(pos + d);

                const Room* const other_room = map::g_room_map.at(p);

                const bool is_any_of_rooms =
                        (other_room == &room_0) ||
                        (other_room == &room_1);

                if (blocked.at(p) && !is_any_of_rooms) {
                        return true;
                }
        }

        return false;
}

static bool get_blocked_positions_for_path(
        const P& p0,
        const P& p1,
        const Room& room_0,
        const Room& room_1,
        Array2<bool>& result)
{
        Array2<bool> blocked_tmp(map::dims());

        // Mark all positions as blocked, which is not a wall, or is a room.
        const size_t nr_positions = map::nr_positions();
        for (size_t i = 0; i < nr_positions; ++i) {
                const bool is_wall = map::g_terrain.at(i)->id() == terrain::Id::wall;

                const Room* const room_ptr = map::g_room_map.at(i);

                blocked_tmp.at(i) = !is_wall || room_ptr;
        }

        // NOTE: These checks cannot be added to the "valid_corridor_entries"
        // function, since that function must be able to find positions that are
        // shared between two adjacent rooms (i.e. the entry points to both
        // rooms are the same position).
        if (is_adjacent_to_blocked_position_not_in_rooms(p0, room_0, room_1, blocked_tmp) ||
            is_adjacent_to_blocked_position_not_in_rooms(p1, room_0, room_1, blocked_tmp)) {
                // One or both of the entry points are adjacent to a position
                // that is not a wall terrain, and does not belong to the entry
                // point room - refuse to build a corridor here.
                return false;
        }

        // Expand the blocked positions - we do not want to build the corridor
        // adjacent to any floor.
        result = map_parsers::expand(blocked_tmp, blocked_tmp.rect());

        // We know from above that p0 and p1 are actually OK (not adjacent to
        // non-wall terrain not belonging to the entry point room) - so mark
        // them as free in the expanded blocking array.
        result.at(p0) = false;
        result.at(p1) = false;

        return true;
}

static bool is_path_circle_around_room(
        const std::vector<P>& path,
        const Room& room)
{
        bool is_left_of_room = false;
        bool is_right_of_room = false;
        bool is_above_room = false;
        bool is_below_room = false;

        for (const P& p : path) {
                if (p.x < room.m_r.p0.x) {
                        is_left_of_room = true;
                }

                if (p.x > room.m_r.p1.x) {
                        is_right_of_room = true;
                }

                if (p.y < room.m_r.p0.y) {
                        is_above_room = true;
                }

                if (p.y > room.m_r.p1.y) {
                        is_below_room = true;
                }
        }

        const bool is_left_and_right = (is_left_of_room && is_right_of_room);
        const bool is_above_and_below = (is_above_room && is_below_room);

        return (is_left_and_right || is_above_and_below);
}

static std::vector<P> widen_corridor(
        const P& pos,
        const Array2<bool>& blocked)
{
        std::vector<P> positions_widened;

        for (const P& d : dir_utils::g_dir_list_w_center) {
                const P p_adj(pos + d);

                const bool is_inside_map = map::is_pos_inside_outer_walls(p_adj);

                if (!is_inside_map || blocked.at(p_adj)) {
                        continue;
                }

                map::set_terrain(terrain::make(terrain::Id::floor, p_adj));

                positions_widened.push_back(p_adj);
        }

        return positions_widened;
}

static void make_corridor_room(
        const std::vector<P>& corridor_positions,
        Room& room_0,
        Room& room_1)
{
        R room_rect(INT_MAX, INT_MAX, 0, 0);

        for (const P& p : corridor_positions) {
                room_rect.p0.x = std::min(room_rect.p0.x, p.x);
                room_rect.p0.y = std::min(room_rect.p0.y, p.y);
                room_rect.p1.x = std::max(room_rect.p1.x, p.x);
                room_rect.p1.y = std::max(room_rect.p1.y, p.y);
        }

        ASSERT(map::is_pos_inside_outer_walls(room_rect.p0));
        ASSERT(map::is_pos_inside_outer_walls(room_rect.p1));
        ASSERT(room_rect.p0.x <= room_rect.p1.x);
        ASSERT(room_rect.p0.y <= room_rect.p1.y);

        Room* corridor_room = room_factory::make(RoomType::corridor, room_rect);

        mapgen::register_room(*corridor_room);

        corridor_room->m_rooms_con_to.push_back(&room_0);
        corridor_room->m_rooms_con_to.push_back(&room_1);

        room_0.m_rooms_con_to.push_back(corridor_room);
        room_1.m_rooms_con_to.push_back(corridor_room);
}

static void make_corridor(
        const std::vector<P>& path,
        const Array2<bool>& blocks_path,
        Room& room_0,
        Room& room_1)
{
        std::vector<Room*> prev_links;

        bool should_widen_corridor = false;

        if (map::g_dlvl <= g_dlvl_last_early_game) {
                should_widen_corridor = rnd::one_in(5);
        }
        else if (map::g_dlvl <= g_dlvl_last_mid_game) {
                should_widen_corridor = rnd::one_in(3);
        }
        else {
                should_widen_corridor = true;
        }

        std::vector<P> corridor_positions;
        corridor_positions.reserve(map::nr_positions());

        for (const P& p : path) {
                map::set_terrain(terrain::make(terrain::Id::floor, p));

                corridor_positions.push_back(p);

                if (should_widen_corridor) {
                        // In early and mid game, widen *every* position if the corridor should be
                        // widened at all. In the late game, only randomly widen some positions to
                        // give the corridor a more natural/cave look.
                        const bool should_widen_this_pos =
                                ((map::g_dlvl < g_dlvl_first_late_game) ||
                                 rnd::one_in(6));

                        if (should_widen_this_pos) {
                                std::vector<P> widened_positins = widen_corridor(p, blocks_path);

                                corridor_positions.insert(
                                        std::end(corridor_positions),
                                        std::begin(widened_positins),
                                        std::end(widened_positins));
                        }
                }
        }

        make_corridor_room(corridor_positions, room_0, room_1);
}

// -----------------------------------------------------------------------------
// mapgen
// -----------------------------------------------------------------------------
namespace mapgen
{
bool try_make_pathfind_corridor(
        Room& room_0,
        Room& room_1,
        Array2<bool>* const door_proposals)
{
        ASSERT(map::is_area_inside_map(room_0.m_r));
        ASSERT(map::is_area_inside_map(room_1.m_r));

        std::vector<P> p0_bucket;
        std::vector<P> p1_bucket;

        valid_corridor_entries(room_0, p0_bucket);
        valid_corridor_entries(room_1, p1_bucket);

        if (p0_bucket.empty()) {
                // No entry points found in room 0.

                return false;
        }

        if (p1_bucket.empty()) {
                // No entry points found in room 1.
                return false;
        }

        // Find shortest possible distance between entry points.
        const int shortest_dist = find_shortest_entry_point_king_dist(p0_bucket, p1_bucket);

        // Keep all entry pairs with shortest distance.

        const std::vector<std::pair<P, P>> entries_bucket =
                get_entries_pairs_with_shortest_dist(
                        p0_bucket,
                        p1_bucket,
                        shortest_dist);

        // Pick a random entry pair.
        const std::pair<P, P> entries = rnd::element(entries_bucket);

        const P& p0 = entries.first;
        const P& p1 = entries.second;

        std::vector<P> path;

        Array2<bool> blocks_path(map::dims());

        // Try to find a path to the other entry point.
        //
        // NOTE: This shall work even if p0 and p1 are the same position.
        //
        const bool can_build_path =
                get_blocked_positions_for_path(
                        p0,
                        p1,
                        room_0,
                        room_1,
                        blocks_path);

        if (!can_build_path) {
                return false;
        }

        if (p0 == p1) {
                // Entry points are the same position (rooms are adjacent).
                path.push_back(p0);
        }
        else {
                // Entry points are different positions.

                // Allowing diagonal steps creates a more "cave like" path.
                const bool allow_diagonal = (map::g_dlvl >= g_dlvl_first_late_game);

                // Randomizing step choices (i.e. when to change directions)
                // creates more "snaky" paths (this does not create longer paths
                // - it just randomizes the variation of optimal path).
                const bool randomize_step_choices = true;

                path = pathfind(p0, p1, blocks_path, allow_diagonal, randomize_step_choices);
        }

        const int corridor_max_length = 20;

        if (!path.empty() && (path.size() <= corridor_max_length)) {
                path.push_back(p0);

                // Check that we don't circle around the origin or target room (looks bad).
                if (is_path_circle_around_room(path, room_0) ||
                    is_path_circle_around_room(path, room_1)) {
                        return false;
                }

                // Build corridor on the map.
                make_corridor(path, blocks_path, room_0, room_1);

                if (door_proposals) {
                        door_proposals->at(p0) = true;
                        door_proposals->at(p1) = true;
                }

                room_0.m_rooms_con_to.push_back(&room_1);
                room_1.m_rooms_con_to.push_back(&room_0);

                return true;
        }

        // Failed to connect rooms.

        return false;
}

}  // namespace mapgen

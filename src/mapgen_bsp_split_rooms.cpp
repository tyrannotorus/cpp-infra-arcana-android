// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================
#include <algorithm>
#include <cstddef>
#include <iterator>
#include <vector>

#include "array2.hpp"
#include "bsp.hpp"
#include "debug.hpp"
#include "global.hpp"
#include "map.hpp"
#include "map_parsing.hpp"
#include "mapgen.hpp"
#include "pos.hpp"
#include "random.hpp"
#include "rect.hpp"
#include "room.hpp"
#include "terrain.hpp"
#include "terrain_data.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static const int child_room_min_size = 2;

static bool is_floor(const P& pos)
{
        return map::g_terrain.at(pos)->id() == terrain::Id::floor;
}

static bsp::BlockedSplitPositions find_blocked_split_positions(
        const Room& room)
{
        bsp::BlockedSplitPositions blocked;

        for (int x = (room.m_r.p0.x - 1); x <= (room.m_r.p1.x + 1); ++x)
        {
                if (is_floor({x, room.m_r.p0.y - 1}) ||
                    is_floor({x, room.m_r.p1.y + 1}))
                {
                        blocked.x.push_back(x);
                }
        }

        for (int y = (room.m_r.p0.y - 1); y <= (room.m_r.p1.y + 1); ++y)
        {
                if (is_floor({room.m_r.p0.x - 1, y}) ||
                    is_floor({room.m_r.p1.x + 1, y}))
                {
                        blocked.y.push_back(y);
                }
        }

        return blocked;
}

static bool allow_split_room_size(const R& r)
{
        // Do not split the room if it's too wide or tall (this can create lots
        // of "lines" of adjacent rooms, which looks bad)
        const int min_dim = r.min_dim();
        const int max_dim = r.max_dim();

        return (max_dim) < (min_dim * 2);
}

static std::vector<Room*> try_bsp_split_room(Room& room)
{
        // Abort if any cell in the room rectangle belongs to another room, or
        // contains something other than floor
        for (const P& pos : room.m_r.positions())
        {
                const bool is_this_room =
                        (map::g_room_map.at(pos) == &room);

                if (!is_this_room)
                {
                        return {};
                }

                const bool is_floor =
                        map::g_terrain.at(pos)->id() ==
                        terrain::Id::floor;

                if (!is_floor)
                {
                        return {};
                }
        }

        const auto blocked_positions = find_blocked_split_positions(room);

        const auto child_rects =
                bsp::try_split(
                        room.m_r,
                        child_room_min_size,
                        blocked_positions);

        if (child_rects.size() != 2)
        {
                return {};
        }

        for (const auto pos : room.m_r.positions())
        {
                map::put(new terrain::Wall(pos));
        }

        std::vector<Room*> new_rooms;

        for (const auto& child_rect : child_rects)
        {
                auto* const sub_room =
                        mapgen::make_room(child_rect, IsSubRoom::yes);

                new_rooms.push_back(sub_room);

                room.m_sub_rooms.push_back(sub_room);
        }

        ASSERT(new_rooms.size() == 2);

        return new_rooms;
}

static std::vector<P> find_edge(
        const Room& room_1,
        const Room& room_2)
{
        const auto r1 = room_1.m_r;
        const auto r2 = room_2.m_r;

        R edge_rect;

        if (r1.p0.x == r2.p0.x)
        {
                // The rooms are on the same x position, i.e. they have a
                // vertical layout
                ASSERT(r1.p1.x == r2.p1.x);
                ASSERT(r1.p0.y != r2.p0.y);
                ASSERT(r1.p1.y != r2.p1.y);

                edge_rect.p0.x = r1.p0.x;
                edge_rect.p1.x = r1.p1.x;

                edge_rect.p0.y = edge_rect.p1.y =
                        (r1.p0.y < r2.p0.y)
                        ? (r1.p1.y + 1)  // Room 1 is above
                        : (r2.p1.y + 1);  // Room 2 is above

                ASSERT(edge_rect.h() == 1);
        }
        else
        {
                // The rooms are on different x position, i.e. they have a
                // horizontal layout
                ASSERT(r1.p1.y == r2.p1.y);
                ASSERT(r1.p0.x != r2.p0.x);
                ASSERT(r1.p1.x != r2.p1.x);

                edge_rect.p0.y = r1.p0.y;
                edge_rect.p1.y = r1.p1.y;

                edge_rect.p0.x = edge_rect.p1.x =
                        (r1.p0.x < r2.p0.x)
                        ? (r1.p1.x + 1)  // Room 1 is to the left
                        : (r2.p1.x + 1);  // Room 2 is to the left

                ASSERT(edge_rect.w() == 1);
        }

        std::vector<P> edge_positions;

        for (const auto& pos : edge_rect.positions())
        {
                ASSERT(map::g_room_map.at(pos) != &room_1);
                ASSERT(map::g_room_map.at(pos) != &room_2);
                ASSERT(map::g_room_map.at(pos) != nullptr);

                edge_positions.push_back(pos);
        }

        ASSERT(!edge_positions.empty());

        return edge_positions;
}

static auto split_original_rooms(
        const size_t nr_original_rooms,
        const Fraction& chance_to_split_room)
{
        std::vector<std::vector<P>> edges;

        for (size_t i = 0; i < nr_original_rooms; ++i)
        {
                if (!chance_to_split_room.roll())
                {
                        continue;
                }

                auto& room = *map::g_room_list[i];

                if (!allow_split_room_size(room.m_r))
                {
                        continue;
                }

                const auto new_rooms = try_bsp_split_room(room);

                if (new_rooms.size() != 2)
                {
                        continue;
                }

                // Sanity check - the two new rooms have been appended at the
                // end of the global room list
                ASSERT(
                        *(std::end(map::g_room_list) - 2) ==
                        new_rooms[0]);

                ASSERT(
                        *(std::end(map::g_room_list) - 1) ==
                        new_rooms[1]);

                const auto edge = find_edge(*new_rooms[0], *new_rooms[1]);

                edges.push_back(edge);
        }

        return edges;
}

static auto split_new_rooms(
        const size_t nr_original_rooms,
        const Fraction& chance_to_split_room)
{
        std::vector<std::vector<P>> edges;

        for (size_t i = nr_original_rooms; i < map::g_room_list.size(); ++i)
        {
                if (!chance_to_split_room.roll())
                {
                        continue;
                }

                auto& room = *map::g_room_list[i];

                const auto new_rooms = try_bsp_split_room(room);

                if (new_rooms.size() != 2)
                {
                        continue;
                }

                const auto edge = find_edge(*new_rooms[0], *new_rooms[1]);

                edges.push_back(edge);
        }

        return edges;
}

static bool is_valid_entrance(const P& pos)
{
        Array2<bool> walls_around_pos(3, 3);

        for (int x = 0; x < 3; ++x)
        {
                for (int y = 0; y < 3; ++y)
                {
                        const P map_p = pos.with_offsets(x - 1, y - 1);

                        const auto t_id = map::g_terrain.at(map_p)->id();

                        const bool is_wall =
                                (t_id == terrain::Id::wall) ||
                                (t_id == terrain::Id::grate);

                        if (is_wall)
                        {
                                walls_around_pos.at(x, y) = true;
                        }
                }
        }

        walls_around_pos.at({1, 1}) = false;

        return mapgen::is_passage({1, 1}, walls_around_pos);
}

static std::vector<P> valid_entrances(const std::vector<P>& edge)
{
        if (edge.size() < 3)
        {
                return {};
        }

        std::vector<P> entrances;

        // Do not use the first and last position of the edge (looks ugly)
        for (size_t i = 1; i < (edge.size() - 1); ++i)
        {
                const auto pos = edge[i];

                if (is_valid_entrance(pos))
                {
                        entrances.push_back(pos);
                }
        }

        return entrances;
}

static void make_entrances(const std::vector<std::vector<P>>& edges)
{
        for (const auto& edge : edges)
        {
                const auto entrance_bucket = valid_entrances(edge);

                if (entrance_bucket.empty())
                {
                        // No entrance could be found on this edge - the map
                        // will be unconnected, discard it!
                        mapgen::g_is_map_valid = false;

                        return;
                }

                int nr_edge_entrances = rnd::one_in(4) ? 2 : 1;

                nr_edge_entrances =
                        std::min(
                                (int)entrance_bucket.size(),
                                nr_edge_entrances);

                // NOTE: This may occasionally place entrances on the same
                // position twice, or on two adjacent positions - this is OK.
                for (int i = 0; i < nr_edge_entrances; ++i)
                {
                        const P entr_pos = rnd::element(entrance_bucket);

                        map::put(new terrain::Floor(entr_pos));

                        // Until the door placement algorithm is more
                        // intelligent (i.e. avoids placing a door near an
                        // opening in the same wall), we do not propose multiple
                        // doors on the same edge, as it can create a
                        // nonsensical layout
                        if (rnd::coin_toss() &&
                            (nr_edge_entrances == 1))
                        {
                                mapgen::g_door_proposals.at(entr_pos) = true;
                        }
                }
        }
}

static void make_grates(const std::vector<std::vector<P>>& edges)
{
        Array2<bool> blocked(map::dims());

        map_parsers::BlocksWalking(ParseActors::no)
                .run(blocked, blocked.rect());

        const auto adj_grate_parser =
                map_parsers::AnyAdjIsAnyOfTerrains(
                        std::vector<terrain::Id> {terrain::Id::grate});

        const Fraction chance_to_make_grates_for_edge(1, 2);

        for (const auto& edge : edges)
        {
                if (!chance_to_make_grates_for_edge.roll())
                {
                        continue;
                }

                for (const auto& p : edge)
                {
                        if (!mapgen::allow_make_grate_at(p, blocked))
                        {
                                continue;
                        }

                        map::put(new terrain::Grate(p));
                }
        }
}

// -----------------------------------------------------------------------------
// mapgen
// -----------------------------------------------------------------------------
namespace mapgen
{
void bsp_split_rooms()
{
        TRACE_FUNC_BEGIN;

        std::vector<std::vector<P>> edges;

        const auto nr_rooms_before = map::g_room_list.size();

        {
                const Fraction chance_to_split_original_room(1, 2);

                const auto edges_original_rooms =
                        split_original_rooms(
                                nr_rooms_before,
                                chance_to_split_original_room);

                edges.insert(
                        std::end(edges),
                        std::begin(edges_original_rooms),
                        std::end(edges_original_rooms));
        }

        {
                const Fraction chance_to_split_new_room(3, 4);

                const auto edges_new_rooms =
                        split_new_rooms(
                                nr_rooms_before,
                                chance_to_split_new_room);

                edges.insert(
                        std::end(edges),
                        std::begin(edges_new_rooms),
                        std::end(edges_new_rooms));
        }

        make_entrances(edges);

        make_grates(edges);

        TRACE_FUNC_END;
}

}  // namespace mapgen

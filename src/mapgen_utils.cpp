// =============================================================================
// Copyright 2011-2024 Martin Törnqvist <m.tornq@gmail.com>
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
static bool allow_place_pillar_at(const P& pos)
{
        for (const auto& d : dir_utils::g_dir_list_w_center) {
                const auto check_p = pos + d;

                const auto id = map::g_terrain.at(check_p)->id();

                if ((id == terrain::Id::wall) ||
                    (id == terrain::Id::pillar) ||
                    (id == terrain::Id::rubble_high) ||
                    (id == terrain::Id::grate)) {
                        return false;
                }
        }

        return true;
}

static void place_pillar_at(const P& pos)
{
        auto* const pillar =
                static_cast<terrain::Pillar*>(
                        terrain::make(terrain::Id::pillar, pos));

        if (rnd::one_in(5)) {
                pillar->set_broken();
        }

        map::set_terrain(pillar);
}

static void place_wall_pillar_at(const P& pos)
{
        if (rnd::fraction(4, 5)) {
                map::set_terrain(terrain::make(terrain::Id::wall, pos));
        }
        else {
                map::set_terrain(terrain::make(terrain::Id::rubble_high, pos));
        }
}

// -----------------------------------------------------------------------------
// mapgen
// -----------------------------------------------------------------------------
namespace mapgen
{
bool g_is_map_valid = true;

Array2<bool> g_door_proposals(0, 0);

// Adds the room to the room list and the room map
void register_room(room::Room& room)
{
#ifndef NDEBUG
        // Check that the room has not already been added
        for (room::Room* const room_in_list : map::g_room_list) {
                ASSERT(room_in_list != &room);
        }
#endif  // NDEBUG

        map::g_room_list.push_back(&room);

        const P room_p0(room.m_r.p0);
        const P room_p1(room.m_r.p1);

        ASSERT(map::is_pos_inside_outer_walls(room_p0));
        ASSERT(map::is_pos_inside_outer_walls(room_p1));
        ASSERT(room_p0.x <= room_p1.x);
        ASSERT(room_p0.y <= room_p1.y);

        for (int x = room_p0.x; x <= room_p1.x; ++x) {
                for (int y = room_p0.y; y <= room_p1.y; ++y) {
                        map::g_room_map.at(x, y) = &room;
                }
        }
}

void make_floor(const room::Room& room)
{
        for (int x = room.m_r.p0.x; x <= room.m_r.p1.x; ++x) {
                for (int y = room.m_r.p0.y; y <= room.m_r.p1.y; ++y) {
                        map::set_terrain(
                                terrain::make(
                                        terrain::Id::floor,
                                        {x, y}));
                }
        }
}

void cut_room_corners(const room::Room& room)
{
        // Never cut the room corners if it's a "small" room
        if (room.m_r.min_dim() < 6) {
                return;
        }

        const P max_dims(room.m_r.dims() - 4);

        const P room_p0(room.m_r.p0);
        const P room_p1(room.m_r.p1);

        // NOTE: The "cross" dimensons and coordinates refer to the inner
        // rectangle of the plus shape.
        const P cross_dims(
                rnd::range(2, max_dims.x),
                rnd::range(2, max_dims.y));

        const P cross_x0y0(
                rnd::range(room_p0.x + 2, room_p1.x - cross_dims.x - 1),
                rnd::range(room_p0.y + 2, room_p1.y - cross_dims.y - 1));

        const P cross_x1y1(cross_x0y0 + cross_dims - 1);

        // Which corners to place (up-left, up-right, down-left, down-right)
        // By default, we cut all corners
        bool c[4] = {true, true, true, true};

        // Ocassionally cut only some corners
        if (rnd::fraction(2, 3)) {
                while (true) {
                        int nr_corners = 0;

                        for (int i = 0; i < 4; ++i) {
                                if (rnd::coin_toss()) {
                                        c[i] = true;
                                        ++nr_corners;
                                }
                                else {
                                        // Do not cut this corner
                                        c[i] = false;
                                }
                        }

                        if (nr_corners > 0) {
                                break;
                        }
                }
        }

        for (size_t corner_idx = 0; corner_idx < 4; ++corner_idx) {
                // This corner has been marked for skipping?
                if (!c[corner_idx]) {
                        continue;
                }

                R r;

                switch (corner_idx) {
                case 0:
                        // Up left
                        r.p0 = room_p0;
                        r.p1 = cross_x0y0 - 1;
                        break;

                case 1:
                        // Up right
                        r.p0 = P(cross_x1y1.x + 1, room_p0.y);
                        r.p1 = P(room_p1.x, cross_x0y0.y - 1);
                        break;

                case 2:
                        // Down left
                        r.p0 = P(room_p0.x, cross_x1y1.y + 1);
                        r.p1 = P(cross_x0y0.x - 1, room_p1.y);
                        break;

                case 3:
                        // Down right
                        r.p0 = cross_x1y1 + 1;
                        r.p1 = room_p1;
                        break;

                default:
                        ASSERT(false);
                        break;
                }

                // Check if these positions can be cut
                bool allow_cut = true;

                for (int x = r.p0.x; x <= r.p1.x; ++x) {
                        for (int y = r.p0.y; y <= r.p1.y; ++y) {
                                for (const P& d : dir_utils::g_dir_list_w_center) {
                                        const P p(x, y);

                                        const P check_p(p + d);

                                        const room::Room* const room_here =
                                                map::g_room_map.at(check_p);

                                        const terrain::Id id =
                                                map::g_terrain.at(check_p)
                                                        ->id();

                                        if ((room_here == &room &&
                                             id != terrain::Id::floor) ||
                                            (room_here != &room &&
                                             id != terrain::Id::wall)) {
                                                allow_cut = false;
                                                break;
                                        }
                                }

                                if (!allow_cut) {
                                        break;
                                }
                        }
                }

                if (allow_cut) {
                        // OK, nothing is preventing us from building walls here
                        for (int x = r.p0.x; x <= r.p1.x; ++x) {
                                for (int y = r.p0.y; y <= r.p1.y; ++y) {
                                        map::set_terrain(
                                                terrain::make(
                                                        terrain::Id::wall,
                                                        {x, y}));

                                        map::g_room_map.at(x, y) = nullptr;
                                }
                        }
                }
        }
}

void make_pillars_in_room(const room::Room& room)
{
        const P& room_p0(room.m_r.p0);
        const P& room_p1(room.m_r.p1);

        Range step_range(1, 3);

        const int dx = step_range.roll();
        const int dy = step_range.roll();

        const int place_one_in_n = rnd::range(1, 4);

        const bool use_pillars = rnd::coin_toss();

        for (int y = room_p0.y + 1; y <= room_p1.y - 1; y += dy) {
                for (int x = room_p0.x + 1; x <= room_p1.x - 1; x += dx) {
                        const P p(x, y);

                        if (!allow_place_pillar_at(p)) {
                                continue;
                        }

                        if (rnd::one_in(place_one_in_n)) {
                                if (use_pillars) {
                                        place_pillar_at(p);
                                }
                                else {
                                        place_wall_pillar_at(p);
                                }
                        }
                        else if (rnd::coin_toss()) {
                                map::set_terrain(terrain::make(terrain::Id::rubble_low, p));
                        }
                }
        }
}

void cavify_room(room::Room& room)
{
        if (room.m_is_sub_room) {
                return;
        }

        Array2<bool> is_other_room(map::dims());

        const size_t nr_positions = map::nr_positions();
        for (size_t i = 0; i < nr_positions; ++i) {
                const auto* const room_here = map::g_room_map.at(i);

                is_other_room.at(i) =
                        room_here &&
                        room_here != &room;
        }

        const auto blocked = map_parsers::expand(
                is_other_room,
                is_other_room.rect());

        R& room_rect = room.m_r;

        std::vector<P> origin_bucket;

        const auto& r = room.m_r;

        const int x0 = r.p0.x + 1;
        const int y0 = r.p0.y + 1;
        const int x1 = r.p1.x - 1;
        const int y1 = r.p1.y - 1;

        for (int x = x0; x <= x1; ++x) {
                for (int y = y0; y <= y1; ++y) {
                        // add to origin bucket if we are on the edge
                        if (x == x0 || x == x1 || y == y0 || y == y1) {
                                origin_bucket.emplace_back(x, y);
                        }
                }
        }

        for (const P& origin : origin_bucket) {
                if (blocked.at(origin) ||
                    map::g_room_map.at(origin) != &room) {
                        continue;
                }

                const auto flood = floodfill(
                        origin,
                        blocked,
                        rnd::range(1, 4),
                        P(-1, -1),
                        false);

                for (int x = 0; x < map::w(); ++x) {
                        for (int y = 0; y < map::h(); ++y) {
                                if ((flood.at(x, y) > 0) &&
                                    (map::g_room_map.at(x, y) != &room)) {
                                        map::set_terrain(
                                                terrain::make(
                                                        terrain::Id::floor,
                                                        {x, y}));

                                        map::g_room_map.at(x, y) = &room;

                                        if (x < room_rect.p0.x) {
                                                room_rect.p0.x = x;
                                        }

                                        if (y < room_rect.p0.y) {
                                                room_rect.p0.y = y;
                                        }

                                        if (x > room_rect.p1.x) {
                                                room_rect.p1.x = x;
                                        }

                                        if (y > room_rect.p1.y) {
                                                room_rect.p1.y = y;
                                        }
                                }
                        }
                }
        }

        for (size_t i = 0; i < nr_positions; ++i) {
                if (map::g_room_map.at(i) != &room) {
                        continue;
                }

                auto* const terrain = map::g_terrain.at(i);

                if (terrain->id() == terrain::Id::floor) {
                        static_cast<terrain::Floor*>(terrain)->m_type =
                                terrain::FloorType::cave;
                }
        }
}

bool is_passage(const P& pos, const Array2<bool>& blocked)
{
        ASSERT(pos.x > 0);
        ASSERT(pos.y > 0);
        ASSERT(pos.x < blocked.w());
        ASSERT(pos.y < blocked.h());

        bool is_passage_ver = true;
        bool is_passage_hor = true;

        for (int d = -1; d <= 1; d++) {
                if (blocked.at({pos.x + d, pos.y})) {
                        // Blocked by wall in horziontal direction
                        is_passage_hor = false;
                }

                if (blocked.at({pos.x, pos.y + d})) {
                        // Blocked by wall in vertical direction
                        is_passage_ver = false;
                }

                if (d != 0) {
                        if (!blocked.at({pos.x, pos.y + d})) {
                                // Not surrounded by walls vertically
                                is_passage_hor = false;
                        }

                        if (!blocked.at({pos.x + d, pos.y})) {
                                // Not surrounded by walls horizontally
                                is_passage_ver = false;
                        }
                }
        }

        return is_passage_hor || is_passage_ver;
}

bool is_choke_point(
        const P& p,
        const Array2<bool>& blocked,
        ChokePointData* out)
{
        // Assuming that the tested position is free
        ASSERT(!blocked.at(p));

        // Robustness for release mode
        if (blocked.at(p)) {
                // This is weird, invalidate the map
                g_is_map_valid = false;

                return false;
        }

        // First, there must be exactly two free cells cardinally adjacent to
        // the tested position
        P p_side1;
        P p_side2;

        for (const P& d : dir_utils::g_cardinal_list) {
                const P adj_p(p + d);

                if (!blocked.at(adj_p)) {
                        if (p_side1.x == 0) {
                                p_side1 = adj_p;
                        }
                        else if (p_side2.x == 0) {
                                p_side2 = adj_p;
                        }
                        else {
                                // Both p0 and p1 has already been set

                                // This is not a choke point, bye!
                                return false;
                        }
                }
        }

        // OK, the position has exactly two free cardinally adjacent cells

        // Check that the two sides can reach each other
        auto flood_side1 = floodfill(p_side1, blocked);

        if (flood_side1.at(p_side2) == 0) {
                // The two sides were already separated from each other
                return false;
        }

        // Check if this position can completely separate the two sides
        auto blocked_cpy(blocked);

        blocked_cpy.at(p) = true;

        // Do another floodfill from side 1
        flood_side1 = floodfill(p_side1, blocked_cpy);

        if (flood_side1.at(p_side2) > 0) {
                // The two sides can still reach each other - not a choke point
                return false;
        }

        // OK, this is a "true" choke point, time to gather more information!

        if (out) {
                out->p = p;
        }

        // Do a floodfill from side 2
        const auto flood_side2 = floodfill(p_side2, blocked_cpy);

        if (out) {
                // Prepare for at least the worst case of push-backs
                const size_t nr_positions = map::nr_positions();
                out->sides[0].reserve(nr_positions);
                out->sides[1].reserve(nr_positions);

                // Add the origin positions for both sides (they have flood
                // value 0)
                out->sides[0].push_back(p_side1);
                out->sides[1].push_back(p_side2);

                for (int x = 0; x < map::w(); ++x) {
                        for (int y = 0; y < map::h(); ++y) {
                                if (flood_side1.at(x, y) > 0) {
                                        ASSERT(flood_side2.at(x, y) == 0);

                                        out->sides[0].emplace_back(x, y);
                                }
                                else if (flood_side2.at(x, y) > 0) {
                                        out->sides[1].emplace_back(x, y);
                                }
                        }
                }
        }

        return true;
}

std::vector<P> pathfinder_walk(const P& p0, const P& p1, const bool is_smooth)
{
        std::vector<P> result;

        Array2<bool> blocked(map::dims());

        const auto path = pathfind(p0, p1, blocked);

        std::vector<P> rnd_walk_buffer;

        for (const P& p : path) {
                result.push_back(p);

                if (!is_smooth && rnd::one_in(3)) {
                        const auto rnd_walk_path = rnd_walk(
                                p,
                                rnd::range(1, 6),
                                map::rect(),
                                true);

                        result.reserve(
                                result.size() +
                                rnd_walk_buffer.size());

                        move(begin(rnd_walk_buffer),
                             end(rnd_walk_buffer),
                             back_inserter(result));
                }
        }

        return result;
}

std::vector<P> rnd_walk(
        const P& p0,
        int len,
        R area,
        const bool allow_diagonal)
{
        std::vector<P> result;

        const std::vector<P>& d_list =
                allow_diagonal ? dir_utils::g_dir_list : dir_utils::g_cardinal_list;

        P p(p0);

        while (len > 0) {
                result.push_back(p);

                --len;

                while (true) {
                        const P nxt_pos = p + rnd::element(d_list);

                        if (is_pos_inside(nxt_pos, area)) {
                                p = nxt_pos;
                                break;
                        }
                }
        }

        return result;
}

void make_explore_spawn_weights(
        const Array2<bool>& blocked,
        std::vector<P>& positions_out,
        std::vector<int>& weights_out)
{
        Array2<int> weight_map(map::dims());

        const size_t nr_positions = map::nr_positions();
        for (size_t i = 0; i < nr_positions; ++i) {
                // Give all cells a base weight of 1.
                weight_map.at(i) = 1;

                // Increase weight for dark cells.
                if (map::g_dark.at(i)) {
                        weight_map.at(i) += 10;
                }
        }

        // Put extra weight for "optional" areas behind choke points
        for (const ChokePointData& choke_point : map::g_choke_point_data) {
                // If the player and the stairs are on the same side of the
                // choke point, this means that the "other" side is an optional
                // map branch.
                if (choke_point.player_side == choke_point.stairs_side) {
                        ASSERT(choke_point.player_side == 0 || choke_point.player_side == 1);

                        // Robustness for release mode
                        if (choke_point.player_side != 0 &&
                            choke_point.player_side != 1) {
                                continue;
                        }

                        const int other_side_idx = (choke_point.player_side == 0) ? 1 : 0;

                        const std::vector<P>& other_side_positions =
                                choke_point.sides[other_side_idx];

                        // NOTE: To avoid leaning heavily towards only putting
                        // stuff in big hidden areas, we divide the weight given
                        // per position based on the total number of positions
                        // in the area.

                        const int weight_div = std::max(1, (int)other_side_positions.size() / 2);

                        // Increase weight for being in an optional map branch.
                        int weight_inc = std::max(1, (250 / weight_div));

                        terrain::Terrain* const terrain = map::g_terrain.at(choke_point.p);

                        // Increase weight if behind hidden/stuck/warded doors.
                        if (terrain->id() == terrain::Id::door) {
                                auto* const door = static_cast<terrain::Door*>(terrain);

                                if (door->is_hidden()) {
                                        weight_inc += std::max(1, 200 / weight_div);
                                }
                                else if (door->is_warded()) {
                                        weight_inc += std::max(1, 500 / weight_div);
                                }
                                else if (door->is_stuck()) {
                                        weight_inc += std::max(1, 200 / weight_div);
                                }
                        }

                        for (const P& p : other_side_positions) {
                                weight_map.at(p) += weight_inc;
                        }
                }
        }

        // Prepare for at least the worst case of push-backs.
        positions_out.reserve(nr_positions);
        weights_out.reserve(nr_positions);

        for (int x = 0; x < map::w(); ++x) {
                for (int y = 0; y < map::h(); ++y) {
                        if (blocked.at(x, y)) {
                                continue;
                        }

                        const int weight = weight_map.at(x, y);

                        if (weight > 0) {
                                // We can spawn here, save the position and the
                                // corresponding spawn chance weight.
                                positions_out.emplace_back(x, y);

                                weights_out.push_back(weight);
                        }
                }
        }
}

Array2<bool> allowed_stair_cells()
{
        TRACE_FUNC_BEGIN;

        Array2<bool> result(map::dims());

        // Mark cells as free if all adjacent terrain types are allowed
        std::vector<terrain::Id> feat_ids_ok = {
                terrain::Id::floor,
                terrain::Id::carpet,
                terrain::Id::grass,
                terrain::Id::bush,
                terrain::Id::rubble_low,
                terrain::Id::vines,
                terrain::Id::chains,
                terrain::Id::trap};

        map_parsers::AllAdjIsAnyOfTerrains(feat_ids_ok)
                .run(result, result.rect());

        // Block cells with items
        const size_t nr_positions = map::nr_positions();
        for (size_t i = 0; i < nr_positions; ++i) {
                if (map::g_items.at(i)) {
                        result.at(i) = false;
                }
        }

        // Block cells with actors
        for (const auto* const actor : game_time::g_actors) {
                const P& p(actor->m_pos);

                result.at(p) = false;
        }

        TRACE_FUNC_END;

        return result;
}

void move_player_to_nearest_allowed_pos()
{
        TRACE_FUNC_BEGIN;

        const auto allowed_cells = allowed_stair_cells();

        auto pos_bucket = to_vec(allowed_cells, true, allowed_cells.rect());

        if (pos_bucket.empty()) {
                g_is_map_valid = false;
        }
        else {
                // Valid cells exists
                TRACE << "Sorting the allowed cells vector "
                      << "(" << pos_bucket.size() << " cells)" << std::endl;

                IsCloserToPos is_closer_to_origin(map::g_player->m_pos);

                std::sort(
                        std::begin(pos_bucket),
                        std::end(pos_bucket),
                        is_closer_to_origin);

                map::g_player->m_pos = pos_bucket.front();

                // Ensure that the player always descends to a floor cell (and
                // not into a bush or something)
                map::set_terrain(
                        terrain::make(
                                terrain::Id::floor,
                                map::g_player->m_pos));
        }

        TRACE_FUNC_END;
}

}  // namespace mapgen

// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "room_auto_spawn_terrain.hpp"

#include "direction.hpp"
#include "map.hpp"
#include "map_parsing.hpp"
#include "misc.hpp"
#include "terrain.hpp"
#include "terrain_factory.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static int walk_blockers_in_front_of_dir(const Dir dir, const P& pos)
{
        int nr_blockers = 0;

        switch (dir) {
        case Dir::right:
                for (int dy = -1; dy <= 1; ++dy) {
                        const auto* const t = map::g_terrain.at(pos.x + 1, pos.y + dy);

                        if (!t->is_walkable()) {
                                nr_blockers += 1;
                        }
                }
                break;

        case Dir::down:
                for (int dx = -1; dx <= 1; ++dx) {
                        const auto* const t = map::g_terrain.at(pos.x + dx, pos.y + 1);

                        if (!t->is_walkable()) {
                                nr_blockers += 1;
                        }
                }
                break;

        case Dir::left:
                for (int dy = -1; dy <= 1; ++dy) {
                        const auto* const t = map::g_terrain.at(pos.x - 1, pos.y + dy);

                        if (!t->is_walkable()) {
                                nr_blockers += 1;
                        }
                }
                break;

        case Dir::up:
                for (int dx = -1; dx <= 1; ++dx) {
                        const auto* const t = map::g_terrain.at(pos.x + dx, pos.y - 1);

                        if (!t->is_walkable()) {
                                nr_blockers += 1;
                        }
                }
                break;

        case Dir::down_left:
        case Dir::down_right:
        case Dir::up_left:
        case Dir::up_right:
        case Dir::center:
        case Dir::END:
                break;
        }

        return nr_blockers;
}

static std::vector<P> find_all_walkable_foor_in_room(const room::Room& room)
{
        std::vector<P> result;

        const auto& r = room.m_r;

        for (int x = r.p0.x; x <= r.p1.x; ++x) {
                for (int y = r.p0.y; y <= r.p1.y; ++y) {
                        if (map::g_room_map.at(x, y) != &room) {
                                continue;
                        }

                        const terrain::Terrain* const t = map::g_terrain.at(x, y);

                        if (t->is_walkable() && t->m_data->is_floor_like) {
                                result.emplace_back(x, y);
                        }
                }
        }

        return result;
}

static bool is_door_adjacent_to_pos(const P& pos)
{
        for (int dx = -1; dx <= 1; ++dx) {
                for (int dy = -1; dy <= 1; ++dy) {
                        const P& p_adj = pos.with_offsets(dx, dy);

                        const terrain::Terrain* const t = map::g_terrain.at(p_adj);

                        if (t->id() == terrain::Id::door) {
                                return true;
                        }
                }
        }

        return false;
}

static void get_positions_in_room_relative_to_walls(
        const room::Room& room,
        std::vector<P>& adj_to_walls,
        std::vector<P>& away_from_walls)
{
        adj_to_walls.clear();
        away_from_walls.clear();

        std::vector<P> pos_bucket = find_all_walkable_foor_in_room(room);

        for (P& pos : pos_bucket) {
                const int nr_r = walk_blockers_in_front_of_dir(Dir::right, pos);
                const int nr_d = walk_blockers_in_front_of_dir(Dir::down, pos);
                const int nr_l = walk_blockers_in_front_of_dir(Dir::left, pos);
                const int nr_u = walk_blockers_in_front_of_dir(Dir::up, pos);

                const bool is_zero_all_dir =
                        (nr_r == 0) &&
                        (nr_d == 0) &&
                        (nr_l == 0) &&
                        (nr_u == 0);

                if (is_zero_all_dir) {
                        away_from_walls.push_back(pos);
                        continue;
                }

                if (is_door_adjacent_to_pos(pos)) {
                        continue;
                }

                // NOTE: There must be three "walls" in front of the terrain,
                // otherwise for example a cabinet can spawn in front of a
                // corridor.
                if (((nr_r == 3) && (nr_l == 0)) ||
                    ((nr_u == 3) && (nr_d == 0)) ||
                    ((nr_d == 3) && (nr_u == 0)) ||
                    ((nr_l == 3) && (nr_r == 0))) {
                        adj_to_walls.push_back(pos);

                        continue;
                }
        }
}

// -----------------------------------------------------------------------------
// room
// -----------------------------------------------------------------------------
namespace room
{
void place_auto_terrains(const Room& room)
{
        // Make a terrain bucket
        std::vector<terrain::Id> terrain_bucket;

        const std::vector<RoomAutoTerrainRule> rules = room.auto_terrains_allowed();

        if (rules.empty()) {
                return;
        }

        for (const RoomAutoTerrainRule& rule : rules) {
                // Insert N elements of the given Terrain ID.
                terrain_bucket.insert(
                        std::end(terrain_bucket),
                        rule.nr_allowed,
                        rule.id);
        }

        std::vector<P> adj_to_walls_bucket;
        std::vector<P> away_from_walls_bucket;

        get_positions_in_room_relative_to_walls(
                room,
                adj_to_walls_bucket,
                away_from_walls_bucket);

        const bool should_convert_walls_to_pillars = rnd::coin_toss();

        while (!terrain_bucket.empty()) {
                // TODO: Do a random shuffle of the bucket instead, and pop
                // elements
                const auto terrain_idx = rnd::range(0, (int)terrain_bucket.size() - 1);

                const auto id = terrain_bucket[terrain_idx];

                terrain_bucket.erase(std::begin(terrain_bucket) + terrain_idx);

                const P p =
                        find_auto_terrain_placement(
                                adj_to_walls_bucket,
                                away_from_walls_bucket,
                                id);

                if (p.x >= 0) {
                        // A good position was found

                        const auto& d = terrain::data(id);

                        ASSERT(map::is_pos_inside_outer_walls(p));

                        terrain::Terrain* terrain {nullptr};

                        // If the terrain is a wall, it may be converted to a pillar instead.
                        if (should_convert_walls_to_pillars &&
                            (d.id == terrain::Id::wall)) {
                                terrain = terrain::make(terrain::Id::pillar, p);

                                if (rnd::one_in(5)) {
                                        static_cast<terrain::Pillar*>(terrain)->set_broken();
                                }
                        }
                        else {
                                terrain = terrain::make(d.id, p);
                        }

                        map::set_terrain(terrain);

                        // Erase all adjacent positions
                        auto is_adj = [&](const P& other_p) {
                                return is_pos_adj(p, other_p, true);
                        };

                        adj_to_walls_bucket.erase(
                                std::remove_if(
                                        std::begin(adj_to_walls_bucket),
                                        std::end(adj_to_walls_bucket),
                                        is_adj),
                                std::end(adj_to_walls_bucket));

                        away_from_walls_bucket.erase(
                                std::remove_if(
                                        std::begin(away_from_walls_bucket),
                                        std::end(away_from_walls_bucket),
                                        is_adj),
                                std::end(away_from_walls_bucket));
                }
        }
}

P find_auto_terrain_placement(
        const std::vector<P>& adj_to_walls,
        const std::vector<P>& away_from_walls,
        const terrain::Id id)
{
        const bool is_adj_to_walls_avail = !adj_to_walls.empty();
        const bool is_away_from_walls_avail = !away_from_walls.empty();

        if (!is_adj_to_walls_avail &&
            !is_away_from_walls_avail) {
                return {-1, -1};
        }

        // TODO: This method is crap, use a bucket instead!

        const int nr_attempts_to_find_pos = 100;

        for (int i = 0; i < nr_attempts_to_find_pos; ++i) {
                const auto& d = terrain::data(id);

                if (is_adj_to_walls_avail &&
                    (d.auto_spawn_placement == terrain::TerrainPlacement::adj_to_walls)) {
                        return rnd::element(adj_to_walls);
                }

                if (is_away_from_walls_avail &&
                    (d.auto_spawn_placement == terrain::TerrainPlacement::away_from_walls)) {
                        return rnd::element(away_from_walls);
                }

                if (d.auto_spawn_placement == terrain::TerrainPlacement::either) {
                        if (rnd::coin_toss()) {
                                if (is_adj_to_walls_avail) {
                                        return rnd::element(adj_to_walls);
                                }
                        }
                        else {
                                // Coint toss
                                if (is_away_from_walls_avail) {
                                        return rnd::element(away_from_walls);
                                }
                        }
                }
        }

        return {-1, -1};
}

}  // namespace room

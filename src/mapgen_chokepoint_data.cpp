// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "mapgen.hpp"

#include "actor.hpp"
#include "array2.hpp"
#include "direction.hpp"
#include "flood.hpp"
#include "map.hpp"
#include "map_parsing.hpp"
#include "pos.hpp"
#include "terrain.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static bool is_chokepoint(
        const P& p,
        const Array2<bool>& blocked,
        ChokePointData* out)
{
        // Assuming that the tested position is free.
        ASSERT(!blocked.at(p));

        // Robustness for release mode
        if (blocked.at(p)) {
                // This is weird, invalidate the map.
                mapgen::g_is_map_valid = false;

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

static P find_stairs_pos()
{
        for (const terrain::Terrain* const terrain : map::g_terrain) {
                if (terrain->id() == terrain::Id::stairs) {
                        return terrain->pos();
                }
        }

        return {-1, -1};
}

// -----------------------------------------------------------------------------
// mapgen
// -----------------------------------------------------------------------------
namespace mapgen
{
void calc_chokepoint_data()
{
        const P stairs_pos = find_stairs_pos();

        if (stairs_pos.x == -1) {
                ASSERT(false);

                g_is_map_valid = false;

                return;
        }

        Array2<bool> blocked(map::dims());

        map_parsers::BlocksWalking(ParseActors::no)
                .run(blocked, blocked.rect());

        // TODO: Considering the stairs free is not ideal for checking map
        // connectedness, since the stairs could block other positions.
        const std::vector<terrain::Id> free_terrains = {
                terrain::Id::door,
                terrain::Id::stairs};

        const auto is_free_terrain_parser =
                map_parsers::IsAnyOfTerrains(free_terrains);

        for (const auto& p : map::rect().positions()) {
                if (is_free_terrain_parser.run(p)) {
                        blocked.at(p) = false;
                }
        }

        // TODO: There should perhaps be a check in both release mode and debug
        // mode for if the map is connected (late in the generation process),
        // and in release mode we just invalidate the map and try again.
#ifndef NDEBUG
        if (!map_parsers::is_map_connected(blocked)) {
                for (int y = 0; y < map::h(); ++y) {
                        for (int x = 0; x < map::w(); ++x) {
                                const P print_pos(x, y);

                                std::string sym = " ";

                                const auto t_id = map::g_terrain.at(print_pos)->id();

                                if (print_pos == map::g_player->m_pos) {
                                        sym = "@";
                                }
                                else if (t_id == terrain::Id::stairs) {
                                        sym = ">";
                                }
                                else if (t_id == terrain::Id::tree) {
                                        sym = "|";
                                }
                                else if (blocked.at(print_pos)) {
                                        sym = "#";
                                }
                                else {
                                        sym = ".";
                                }

                                std::cout << sym;
                        }

                        std::cout << std::endl;
                }

                ASSERT(false);
        }
#endif  // NDEBUG

        for (const auto& p : map::rect().positions()) {
                if (blocked.at(p) || !g_door_proposals.at(p)) {
                        continue;
                }

                ChokePointData d;
                const bool is_choke = is_chokepoint(p, blocked, &d);

                // 'is_chokepoint' called above may invalidate the map.
                if (!g_is_map_valid) {
                        return;
                }

                if (!is_choke) {
                        continue;
                }

                // Find player and stair side
                for (size_t side_idx = 0; side_idx < 2; ++side_idx) {
                        for (const auto& side_p : d.sides[side_idx]) {
                                if (side_p == map::g_player->m_pos) {
                                        ASSERT(d.player_side == -1);

                                        d.player_side = (int)side_idx;
                                }

                                if (side_p == stairs_pos) {
                                        ASSERT(d.stairs_side == -1);

                                        d.stairs_side = (int)side_idx;
                                }
                        }
                }

                if ((d.player_side != 0 && d.player_side != 1) ||
                    (d.stairs_side != 0 && d.stairs_side != 1)) {
                        TRACE
                                << "d.player_side: "
                                << d.player_side
                                << "    d.stairs_side: "
                                << d.stairs_side
                                << std::endl;

                        ASSERT(false);

                        // Invalidate the map
                        g_is_map_valid = false;

                        return;
                }

                map::g_chokepoint_data.emplace_back(d);

        }  // Map position loop
}
}  // namespace mapgen

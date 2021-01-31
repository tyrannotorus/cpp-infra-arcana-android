// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "mapgen.hpp"

#include <cstring>

#include "actor.hpp"
#include "actor_player.hpp"
#include "flood.hpp"
#include "game_time.hpp"
#include "map_parsing.hpp"
#include "terrain_door.hpp"

static bool is_wall(const P& p)
{
        return map::g_terrain.at(p)->id() == terrain::Id::wall;
}

static void try_make_door(const P& p)
{
        // Do not allow placing doors adjacent to these terrains
        std::vector<terrain::Id> forbidden_adj_terrains = {
                terrain::Id::door,
                terrain::Id::liquid_shallow,
                terrain::Id::liquid_deep};

        const auto parser =
                map_parsers::AnyAdjIsAnyOfTerrains(
                        forbidden_adj_terrains);

        if (parser.run(p))
        {
                return;
        }

        bool is_good_ver = true;
        bool is_good_hor = true;

        for (int d = -1; d <= 1; d++)
        {
                if (is_wall({p.x + d, p.y}))
                {
                        is_good_hor = false;
                }

                if (is_wall({p.x, p.y + d}))
                {
                        is_good_ver = false;
                }

                if (d != 0)
                {
                        if (!is_wall({p.x, p.y + d}))
                        {
                                is_good_hor = false;
                        }

                        if (!is_wall({p.x + d, p.y}))
                        {
                                is_good_ver = false;
                        }
                }
        }

        if (is_good_hor || is_good_ver)
        {
                // Make most doors wooden doors, and occasionally make gates
                terrain::Door* door = nullptr;

                if (rnd::fraction(4, 5))
                {
                        const auto* const mimic = new terrain::Wall(p);

                        door =
                                new terrain::Door(
                                        p,
                                        mimic,
                                        terrain::DoorType::wood);
                }
                else
                {
                        // Barred gate
                        door =
                                new terrain::Door(
                                        p,
                                        nullptr,
                                        terrain::DoorType::gate);
                }

                map::put(door);
        }
}

namespace mapgen
{
void make_doors()
{
        TRACE << "Placing doors" << std::endl;

        for (int x = 0; x < map::w(); ++x)
        {
                for (int y = 0; y < map::h(); ++y)
                {
                        if (g_door_proposals.at(x, y) && rnd::fraction(4, 5))
                        {
                                try_make_door(P(x, y));
                        }
                }
        }
}

void make_metal_doors_and_levers()
{
        // Only make metal on some maps, and never late game (theme)
        if (map::g_dlvl >= g_dlvl_first_late_game)
        {
                return;
        }

        const std::vector<int> nr_doors_weights = {
                8,  // 0 doors
                3,  // 1 -
                2,  // 2 -
                1,  // 3 -
        };

        const int nr_doors = rnd::weighted_choice(nr_doors_weights);

        for (int door_idx = 0; door_idx < nr_doors; ++door_idx)
        {
                // Find all chokepoints with a door
                std::vector<const ChokePointData*> chokepoint_bucket;

                for (const auto& chokepoint : map::g_choke_point_data)
                {
                        if (chokepoint.sides[0].empty() ||
                            chokepoint.sides[1].empty())
                        {
                                continue;
                        }

                        const P& p = chokepoint.p;

                        auto id = map::g_terrain.at(p)->id();

                        if (id == terrain::Id::door)
                        {
                                chokepoint_bucket.push_back(&chokepoint);
                        }
                }

                if (chokepoint_bucket.empty())
                {
                        return;
                }

                // Shuffle the chokepoint list
                rnd::shuffle(chokepoint_bucket);

                // Only allow levers in cells completely surrounded by floor
                Array2<bool> blocks_levers(map::dims());

                map_parsers::IsNotTerrain(terrain::Id::floor)
                        .run(blocks_levers, blocks_levers.rect());

                blocks_levers = map_parsers::expand(
                        blocks_levers,
                        blocks_levers.rect());

                // Block cells with actors
                for (const auto* const actor : game_time::g_actors)
                {
                        blocks_levers.at(actor->m_pos) = true;
                }

                // Make a floodfill from the player - this will be used to ONLY
                // place levers in cells which are further from the player than
                // the door.  It's probably (always?) more interesting to find
                // the door first.

                Array2<bool> blocks_player(map::dims());

                map_parsers::BlocksWalking(ParseActors::no)
                        .run(blocks_player, blocks_player.rect());

                const std::vector<terrain::Id> free_terrains = {
                        terrain::Id::door,
                        terrain::Id::liquid_deep};

                for (int x = 0; x < blocks_player.w(); ++x)
                {
                        for (int y = 0; y < blocks_player.h(); ++y)
                        {
                                const P p(x, y);

                                const auto parser =
                                        map_parsers::IsAnyOfTerrains(
                                                free_terrains);

                                if (parser.run(p))
                                {
                                        blocks_player.at(p) = false;
                                }
                        }
                }

                const auto player_flood =
                        floodfill(map::g_player->m_pos, blocks_player);

                // Cells blocking the player from reaching the levers
                Array2<bool> blocks_reaching_levers(map::dims());

                map_parsers::BlocksWalking(ParseActors::no)
                        .run(blocks_reaching_levers,
                             blocks_reaching_levers.rect());

                const size_t nr_positions = map::nr_positions();

                for (size_t cell_idx = 0; cell_idx < nr_positions; ++cell_idx)
                {
                        const auto* r = map::g_terrain.at(cell_idx);

                        if (r->id() == terrain::Id::door)
                        {
                                const auto* const door =
                                        static_cast<const terrain::Door*>(r);

                                const bool is_metal =
                                        door->type() ==
                                        terrain::DoorType::metal;

                                blocks_reaching_levers.at(cell_idx) = is_metal;
                        }
                        else if (r->id() == terrain::Id::liquid_deep)
                        {
                                blocks_reaching_levers.at(cell_idx) = false;
                        }
                }

                // Cells blocked on sides 1 and 2, respectively
                Array2<bool> blocks_lever_1(map::dims());
                Array2<bool> blocks_lever_2(map::dims());

                for (const auto* const chokepoint : chokepoint_bucket)
                {
                        const P& door_p = chokepoint->p;

                        {
                                const auto* r = map::g_terrain.at(door_p);

                                if (r->id() == terrain::Id::door)
                                {
                                        const auto* const door =
                                                static_cast<const terrain::Door*>(r);

                                        if (door->type() ==
                                            terrain::DoorType::metal)
                                        {
                                                // There is already a metal door
                                                // here, try next chokepoint
                                                continue;
                                        }
                                }
                        }

                        // We must find a lever position at least on the player
                        // side, the other side is optional however

                        // Cells generally blocked for placing levers -
                        // e.g. cells too close to walls
                        std::memcpy(
                                blocks_lever_1.data(),
                                blocks_levers.data(),
                                nr_positions);

                        std::memcpy(
                                blocks_lever_2.data(),
                                blocks_levers.data(),
                                nr_positions);

                        // Make a floodfill from the door to all
                        // player-reachable cells
                        const auto lever_reach_flood =
                                floodfill(door_p, blocks_reaching_levers);

                        // Require a greater distance from the player to the
                        // lever, than from the lever to the door
                        int min_dist_from_player = player_flood.at(door_p) + 1;

                        // Also require at least a fixed distance from the
                        // player, to avoid situations where both the door and
                        // the lever are very near
                        min_dist_from_player =
                                std::max(10, min_dist_from_player);

                        // Do not allow too great distance from the door to the
                        // lever (annoying and weird)
                        const int max_dist_from_player = 40;

                        for (size_t i = 0; i < nr_positions; ++i)
                        {
                                const bool is_unreachable =
                                        (lever_reach_flood.at(i) == 0);

                                const bool is_too_close_to_player =
                                        (player_flood.at(i) <=
                                         min_dist_from_player);

                                const bool is_too_far_From_door =
                                        (lever_reach_flood.at(i) >
                                         max_dist_from_player);

                                if (is_unreachable ||
                                    is_too_close_to_player ||
                                    is_too_far_From_door)
                                {
                                        blocks_lever_1.at(i) = true;
                                        blocks_lever_2.at(i) = true;
                                }
                        }

                        const auto& side_1 = chokepoint->sides[0];
                        const auto& side_2 = chokepoint->sides[1];

                        ASSERT(!side_1.empty());
                        ASSERT(!side_2.empty());

                        // Block side 2 positions for the side 1 lever
                        for (const auto& p : side_2)
                        {
                                blocks_lever_1.at(p) = true;
                        }

                        // Block side 1 positions for the side 2 lever
                        for (const auto& p : side_1)
                        {
                                blocks_lever_2.at(p) = true;
                        }

                        std::vector<P> spawn_weight_positions_1;
                        std::vector<P> spawn_weight_positions_2;

                        std::vector<int> spawn_weights_1;
                        std::vector<int> spawn_weights_2;

                        mapgen::make_explore_spawn_weights(
                                blocks_lever_1,
                                spawn_weight_positions_1,
                                spawn_weights_1);

                        mapgen::make_explore_spawn_weights(
                                blocks_lever_2,
                                spawn_weight_positions_2,
                                spawn_weights_2);

                        // Can we place a lever on the player side?
                        const int player_side = chokepoint->player_side;

                        if ((spawn_weights_1.empty() && (player_side == 0)) ||
                            (spawn_weights_2.empty() && (player_side == 1)))
                        {
                                // Unable to place lever on player side - try a
                                // metal door on the next chokepoint
                                continue;
                        }

                        // OK, there exists valid positions for the door, and at
                        // least a lever on the player side
                        auto* door = new terrain::Door(
                                door_p,
                                nullptr,  // No mimic needed
                                terrain::DoorType::metal,
                                terrain::DoorSpawnState::closed);

                        map::put(door);

                        terrain::Lever* lever_1 = nullptr;
                        terrain::Lever* lever_2 = nullptr;

                        auto put_lever_random_p = [](
                                                          const std::vector<int>& weights,
                                                          const std::vector<P>& positions,
                                                          terrain::Door* const linked_door) {
                                const size_t lever_p_idx =
                                        rnd::weighted_choice(weights);

                                const P& lever_1_p = positions[lever_p_idx];

                                auto* lever = new terrain::Lever(lever_1_p);

                                lever->set_linked_terrain(*linked_door);

                                map::put(lever);

                                return lever;
                        };

                        // Only make levers on the non player side occasionally,
                        // to avoid spamming the map with levers
                        const bool allow_lever_non_player_side = rnd::one_in(6);

                        if (!spawn_weights_1.empty() &&
                            ((player_side == 0) || allow_lever_non_player_side))
                        {
                                lever_1 =
                                        put_lever_random_p(
                                                spawn_weights_1,
                                                spawn_weight_positions_1,
                                                door);
                        }

                        if (!spawn_weights_2.empty() &&
                            ((player_side == 1) || allow_lever_non_player_side))
                        {
                                lever_2 =
                                        put_lever_random_p(
                                                spawn_weights_2,
                                                spawn_weight_positions_2,
                                                door);
                        }

                        // If we placed two levers, sync them with each other
                        if (lever_1 && lever_2)
                        {
                                lever_1->add_sibbling(lever_2);
                                lever_2->add_sibbling(lever_1);
                        }

                        // We are done with this door
                        break;

                }  // Chokepoint loop

        }  // Number of doors loop

}  // make_metal_doors_and_levers

}  // namespace mapgen

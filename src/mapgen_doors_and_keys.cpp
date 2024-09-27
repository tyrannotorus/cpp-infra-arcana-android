// =============================================================================
// Copyright 2011-2024 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include <algorithm>
#include <cstring>
#include <vector>

#include "actor.hpp"
#include "array2.hpp"
#include "debug.hpp"
#include "flood.hpp"
#include "game_time.hpp"
#include "map.hpp"
#include "map_parsing.hpp"
#include "mapgen.hpp"
#include "pos.hpp"
#include "random.hpp"
#include "terrain.hpp"
#include "terrain_data.hpp"
#include "terrain_door.hpp"
#include "terrain_factory.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static std::vector<const ChokePointData*> find_all_chokepoints_with_door()
{
        std::vector<const ChokePointData*> chokepoints;

        for (const ChokePointData& chokepoint : map::g_chokepoint_data) {
                if (chokepoint.sides[0].empty() || chokepoint.sides[1].empty()) {
                        continue;
                }

                const P& p = chokepoint.p;

                const terrain::Id id = map::g_terrain.at(p)->id();

                if (id != terrain::Id::door) {
                        continue;
                }

                chokepoints.push_back(&chokepoint);
        }

        return chokepoints;
}

static Array2<bool> get_blocked_map_key_positions()
{
        // Only allow keys in positions completely surrounded by floor.

        Array2<bool> blocked(map::dims());

        map_parsers::IsNotTerrain(terrain::Id::floor)
                .run(blocked, blocked.rect());

        for (const actor::Actor* const actor : game_time::g_actors) {
                blocked.at(actor->m_pos) = true;
        }

        return map_parsers::expand(blocked, blocked.rect());
}

static Array2<int> get_player_walk_flood()
{
        Array2<bool> blocks_player(map::dims());

        map_parsers::BlocksWalking(ParseActors::no)
                .run(blocks_player, blocks_player.rect());

        const std::vector<terrain::Id> free_terrains = {terrain::Id::door};

        for (int x = 0; x < blocks_player.w(); ++x) {
                for (int y = 0; y < blocks_player.h(); ++y) {
                        const P p(x, y);

                        const map_parsers::IsAnyOfTerrains parser(free_terrains);

                        if (parser.run(p)) {
                                blocks_player.at(p) = false;
                        }
                }
        }

        return floodfill(map::g_player->m_pos, blocks_player);
}

static Array2<bool> get_blocked_map_prevents_reaching_keys()
{
        Array2<bool> blocks_reaching_keys(map::dims());

        map_parsers::BlocksWalking(ParseActors::no)
                .run(blocks_reaching_keys, blocks_reaching_keys.rect());

        const size_t nr_positions = map::nr_positions();

        for (size_t cell_idx = 0; cell_idx < nr_positions; ++cell_idx) {
                const terrain::Terrain* terrain = map::g_terrain.at(cell_idx);

                if (terrain->id() == terrain::Id::door) {
                        const auto* const door = static_cast<const terrain::Door*>(terrain);

                        blocks_reaching_keys.at(cell_idx) = door->is_warded();
                }
        }

        return blocks_reaching_keys;
}

static bool is_warded_door_at(const P& pos)
{
        const terrain::Terrain* terrain = map::g_terrain.at(pos);

        if (terrain->id() != terrain::Id::door) {
                return false;
        }

        const auto* const door = static_cast<const terrain::Door*>(terrain);

        return door->is_warded();
}

static void block_unreachable_positions(
        Array2<bool>& blocked,
        const Array2<int>& key_reach_flood)
{
        const size_t nr_positions = map::nr_positions();

        for (size_t i = 0; i < nr_positions; ++i) {
                const bool is_unreachable = (key_reach_flood.at(i) == 0);

                if (is_unreachable) {
                        blocked.at(i) = true;
                }
        }
}

static void block_positions_at_wrong_distance(
        Array2<bool>& blocked,
        const P& door_pos,
        const Array2<int>& player_walk_flood,
        const Array2<int>& key_reach_flood)
{
        // Require a greater distance from the player to the
        // key, than from the key to the door
        int min_dist = player_walk_flood.at(door_pos) + 1;

        // Also require at least a fixed distance from the
        // player, to avoid situations where both the door and
        // the key are very near
        min_dist = std::max(10, min_dist);

        // Do not allow too great distance from the door to the
        // key (annoying and weird).
        const int max_dist = 40;

        const size_t nr_positions = map::nr_positions();

        for (size_t i = 0; i < nr_positions; ++i) {
                const bool is_too_close_to_player = (player_walk_flood.at(i) <= min_dist);
                const bool is_too_far_From_door = (key_reach_flood.at(i) > max_dist);

                if (is_too_close_to_player || is_too_far_From_door) {
                        blocked.at(i) = true;
                }
        }
}

static void block_positions(Array2<bool>& blocked, const std::vector<P>& positions)
{
        for (const P& p : positions) {
                blocked.at(p) = true;
        }
}

static terrain::CrystalKey* put_key_at_random_pos(
        const std::vector<int>& weights,
        const std::vector<P>& positions,
        terrain::Door* const linked_door)
{
        const size_t idx = rnd::weighted_choice(weights);

        const P& key_pos = positions[idx];

        auto* crystal =
                static_cast<terrain::CrystalKey*>(
                        terrain::make(terrain::Id::crystal_key, key_pos));

        crystal->set_linked_door(*linked_door);

        map::set_terrain(crystal);

        return crystal;
}

static bool try_make_door_and_keys_for_chokepoint(
        const ChokePointData& chokepoint,
        const Array2<bool>& blocks_placing_keys,
        const Array2<bool>& blocks_reaching_keys,
        const Array2<int>& player_walk_flood)
{
        const P& door_pos = chokepoint.p;

        if (is_warded_door_at(door_pos)) {
                // There is already a warded door here.
                return false;
        }

        // We must find a key position at least on the player side (the other
        // side is optional).

        // Cells blocked on sides 1 and 2, respectively.
        Array2<bool> blocks_keys_1(map::dims());
        Array2<bool> blocks_keys_2(map::dims());

        const size_t nr_positions = map::nr_positions();

        std::memcpy(blocks_keys_1.data(), blocks_placing_keys.data(), nr_positions);
        std::memcpy(blocks_keys_2.data(), blocks_placing_keys.data(), nr_positions);

        // Make a floodfill from the door to all player-reachable cells.
        const Array2<int> key_reach_flood = floodfill(door_pos, blocks_reaching_keys);

        block_unreachable_positions(blocks_keys_1, key_reach_flood);
        block_unreachable_positions(blocks_keys_2, key_reach_flood);

        block_positions_at_wrong_distance(
                blocks_keys_1,
                door_pos,
                player_walk_flood,
                key_reach_flood);

        block_positions_at_wrong_distance(
                blocks_keys_2,
                door_pos,
                player_walk_flood,
                key_reach_flood);

        const std::vector<P>& side_1 = chokepoint.sides[0];
        const std::vector<P>& side_2 = chokepoint.sides[1];

        ASSERT(!side_1.empty());
        ASSERT(!side_2.empty());

        // Block side 2 positions for the side 1 key, and vice versa.
        block_positions(blocks_keys_1, side_2);
        block_positions(blocks_keys_2, side_1);

        // Calculate "interesting" positions to put the keys in.
        std::vector<P> spawn_weight_positions_1;
        std::vector<P> spawn_weight_positions_2;

        std::vector<int> spawn_weights_1;
        std::vector<int> spawn_weights_2;

        mapgen::make_explore_spawn_weights(
                blocks_keys_1,
                spawn_weight_positions_1,
                spawn_weights_1);

        mapgen::make_explore_spawn_weights(
                blocks_keys_2,
                spawn_weight_positions_2,
                spawn_weights_2);

        // Can we place a key on the player side?
        const int player_side = chokepoint.player_side;

        if ((spawn_weights_1.empty() && (player_side == 0)) ||
            (spawn_weights_2.empty() && (player_side == 1))) {
                // Unable to place key on player side.
                return false;
        }

        // OK, there exists valid positions for the door, and for at least a key
        // on the player side.

        auto* const door =
                static_cast<terrain::Door*>(
                        terrain::make(
                                terrain::Id::door,
                                door_pos));

        door->init_type_and_state(
                terrain::DoorType::wood,
                terrain::DoorSpawnState::warded);

        map::set_terrain(door);

        terrain::CrystalKey* key_1 = nullptr;
        terrain::CrystalKey* key_2 = nullptr;

        // Only make keys on the opposite side occasionally, to
        // avoid spamming the map with keys.
        const bool allow_key_non_player_side = rnd::one_in(6);

        if (!spawn_weights_1.empty() &&
            ((player_side == 0) || allow_key_non_player_side)) {
                key_1 = put_key_at_random_pos(spawn_weights_1, spawn_weight_positions_1, door);
        }

        if (!spawn_weights_2.empty() &&
            ((player_side == 1) || allow_key_non_player_side)) {
                key_2 = put_key_at_random_pos(spawn_weights_2, spawn_weight_positions_2, door);
        }

        // If we placed two keys, sync them with each other.
        if (key_1 && key_2) {
                key_1->add_sibbling(key_2);
                key_2->add_sibbling(key_1);
        }

        return true;
}

// -----------------------------------------------------------------------------
// mapgen
// -----------------------------------------------------------------------------
namespace mapgen
{
void make_warded_doors_and_keys()
{
        // NOTE: The generic term "key" is used here to avoid talking about
        // specific game design concepts (a "key" could be anything).

        const std::vector<int> nr_doors_weights = {
                8,  // 0 doors
                3,  // 1 -
                2,  // 2 -
                1,  // 3 -
        };

        const int nr_doors = rnd::weighted_choice(nr_doors_weights);

        for (int door_idx = 0; door_idx < nr_doors; ++door_idx) {
                std::vector<const ChokePointData*> chokepoint_bucket =
                        find_all_chokepoints_with_door();

                if (chokepoint_bucket.empty()) {
                        return;
                }

                rnd::shuffle(chokepoint_bucket);

                const Array2<bool> blocks_placing_keys = get_blocked_map_key_positions();

                // Make a floodfill from the player, for finding positions which
                // are further from the player than the door. It's probably
                // (always?) more interesting to find the door first.
                const Array2<int> player_walk_flood = get_player_walk_flood();

                const Array2<bool> blocks_reaching_keys = get_blocked_map_prevents_reaching_keys();

                for (const ChokePointData* const chokepoint : chokepoint_bucket) {
                        const bool did_make =
                                try_make_door_and_keys_for_chokepoint(
                                        *chokepoint,
                                        blocks_placing_keys,
                                        blocks_reaching_keys,
                                        player_walk_flood);

                        if (did_make) {
                                break;
                        }
                }
        }
}

}  // namespace mapgen

// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "catch.hpp"

#include "actor.hpp"
#include "actor_factory.hpp"
#include "game_time.hpp"
#include "map.hpp"
#include "terrain.hpp"
#include "terrain_factory.hpp"
#include "test_utils.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static actor::Actor* actor_at(const P& pos)
{
        for (actor::Actor* const actor_here : game_time::g_actors) {
                if (actor_here->m_pos == pos) {
                        return actor_here;
                }
        }

        return nullptr;
}

static void init_map()
{
        for (int x = 0; x < map::w(); ++x) {
                for (int y = 0; y < map::h(); ++y) {
                        map::update_terrain(terrain::make(terrain::Id::wall, {x, y}));
                }
        }

        map::update_terrain(terrain::make(terrain::Id::floor, {3, 5}));
        map::update_terrain(terrain::make(terrain::Id::floor, {4, 5}));
        map::update_terrain(terrain::make(terrain::Id::floor, {5, 5}));
        map::update_terrain(terrain::make(terrain::Id::floor, {6, 5}));
        map::update_terrain(terrain::make(terrain::Id::floor, {7, 5}));
}

// -----------------------------------------------------------------------------
// Test cases
// -----------------------------------------------------------------------------
TEST_CASE("Spawn one actor, close to position")
{
        test_utils::init_all();

        init_map();

        const size_t nr_actors_before = game_time::g_actors.size();

        const actor::MonSpawnResult result = actor::spawn({5, 5}, {"MON_RAT"});

        REQUIRE(result.monsters.size() == 1);
        REQUIRE(result.monsters[0]->m_data->id == "MON_RAT");
        REQUIRE(game_time::g_actors.size() == nr_actors_before + 1);
        REQUIRE(actor_at({5, 5})->m_data->id == "MON_RAT");
}

TEST_CASE("Spawn one actor, scattered")
{
        test_utils::init_all();

        init_map();

        const size_t nr_actors_before = game_time::g_actors.size();

        const actor::MonSpawnResult result =
                actor::spawn(
                        {5, 5},
                        {"MON_RAT"},
                        -1,
                        actor::SpawnScattered::yes);

        REQUIRE(result.monsters.size() == 1);
        REQUIRE(result.monsters[0]->m_data->id == "MON_RAT");
        REQUIRE(game_time::g_actors.size() == nr_actors_before + 1);

        const actor::Actor* rat_on_map = nullptr;

        for (const actor::Actor* const actor : game_time::g_actors) {
                if (actor->m_data->id == "MON_RAT") {
                        REQUIRE(!rat_on_map);

                        rat_on_map = actor;
                }
        }

        REQUIRE(result.monsters[0] == rat_on_map);
}

TEST_CASE("Spawn multiple actors, close to position")
{
        test_utils::init_all();

        init_map();

        const size_t nr_actors_before = game_time::g_actors.size();

        const actor::MonSpawnResult result = actor::spawn({5, 5}, {3, "MON_RAT"});

        REQUIRE(result.monsters.size() == 3);
        REQUIRE(result.monsters[0]->m_data->id == "MON_RAT");
        REQUIRE(result.monsters[1]->m_data->id == "MON_RAT");
        REQUIRE(result.monsters[2]->m_data->id == "MON_RAT");

        REQUIRE(game_time::g_actors.size() == nr_actors_before + 3);

        REQUIRE(actor_at({3, 5}) == nullptr);
        REQUIRE(actor_at({4, 5})->m_data->id == "MON_RAT");
        REQUIRE(actor_at({5, 5})->m_data->id == "MON_RAT");
        REQUIRE(actor_at({6, 5})->m_data->id == "MON_RAT");
        REQUIRE(actor_at({7, 5}) == nullptr);
}

TEST_CASE("Spawn multiple actors, scattered")
{
        test_utils::init_all();

        init_map();

        const size_t nr_actors_before = game_time::g_actors.size();

        const actor::MonSpawnResult result = actor::spawn({5, 5}, {3, "MON_RAT"});

        REQUIRE(result.monsters.size() == 3);
        REQUIRE(result.monsters[0]->m_data->id == "MON_RAT");
        REQUIRE(result.monsters[1]->m_data->id == "MON_RAT");
        REQUIRE(result.monsters[2]->m_data->id == "MON_RAT");
        REQUIRE(game_time::g_actors.size() == nr_actors_before + 3);
}

TEST_CASE("Spawn more actors than free positions, close to position")
{
        test_utils::init_all();

        init_map();

        const size_t nr_actors_before = game_time::g_actors.size();

        // Spawn with a vector of 6 actors IDs, but only room for 5 actors on the map.
        const actor::MonSpawnResult result = actor::spawn({5, 5}, {6, "MON_RAT"});

        REQUIRE(result.monsters.size() == 5);
        REQUIRE(result.monsters[0]->m_data->id == "MON_RAT");
        REQUIRE(result.monsters[1]->m_data->id == "MON_RAT");
        REQUIRE(result.monsters[2]->m_data->id == "MON_RAT");
        REQUIRE(result.monsters[3]->m_data->id == "MON_RAT");
        REQUIRE(result.monsters[4]->m_data->id == "MON_RAT");

        REQUIRE(game_time::g_actors.size() == nr_actors_before + 5);

        REQUIRE(actor_at({3, 5})->m_data->id == "MON_RAT");
        REQUIRE(actor_at({4, 5})->m_data->id == "MON_RAT");
        REQUIRE(actor_at({5, 5})->m_data->id == "MON_RAT");
        REQUIRE(actor_at({6, 5})->m_data->id == "MON_RAT");
        REQUIRE(actor_at({7, 5})->m_data->id == "MON_RAT");
}

TEST_CASE("Spawn more actors than free positions, scattered")
{
        test_utils::init_all();

        init_map();

        const size_t nr_actors_before = game_time::g_actors.size();

        // Spawn with a vector of 6 actors IDs, but only room for 5 actors on the map.
        const actor::MonSpawnResult result =
                actor::spawn(
                        {5, 5},
                        {6, "MON_RAT"},
                        -1,
                        actor::SpawnScattered::yes);

        REQUIRE(result.monsters.size() == 5);
        REQUIRE(result.monsters[0]->m_data->id == "MON_RAT");
        REQUIRE(result.monsters[1]->m_data->id == "MON_RAT");
        REQUIRE(result.monsters[2]->m_data->id == "MON_RAT");
        REQUIRE(result.monsters[3]->m_data->id == "MON_RAT");
        REQUIRE(result.monsters[4]->m_data->id == "MON_RAT");

        REQUIRE(game_time::g_actors.size() == nr_actors_before + 5);

        REQUIRE(actor_at({3, 5})->m_data->id == "MON_RAT");
        REQUIRE(actor_at({4, 5})->m_data->id == "MON_RAT");
        REQUIRE(actor_at({5, 5})->m_data->id == "MON_RAT");
        REQUIRE(actor_at({6, 5})->m_data->id == "MON_RAT");
        REQUIRE(actor_at({7, 5})->m_data->id == "MON_RAT");
}

TEST_CASE("Spawn actors, limited by distance, close to position")
{
        test_utils::init_all();

        // For this test, create a U shape to test that the spawning distance is correctly based on
        // a floodfill.

        for (int x = 0; x < map::w(); ++x) {
                for (int y = 0; y < map::h(); ++y) {
                        map::update_terrain(terrain::make(terrain::Id::wall, {x, y}));
                }
        }

        // Create a shape like this:
        //
        //  #######
        //  #.....#
        //  #####.#
        //  #.....#
        //  #######
        //

        map::update_terrain(terrain::make(terrain::Id::floor, {3, 5}));
        map::update_terrain(terrain::make(terrain::Id::floor, {4, 5}));
        map::update_terrain(terrain::make(terrain::Id::floor, {5, 5}));
        map::update_terrain(terrain::make(terrain::Id::floor, {6, 5}));
        map::update_terrain(terrain::make(terrain::Id::floor, {7, 5}));

        map::update_terrain(terrain::make(terrain::Id::floor, {7, 6}));

        map::update_terrain(terrain::make(terrain::Id::floor, {3, 7}));
        map::update_terrain(terrain::make(terrain::Id::floor, {4, 7}));
        map::update_terrain(terrain::make(terrain::Id::floor, {5, 7}));
        map::update_terrain(terrain::make(terrain::Id::floor, {6, 7}));
        map::update_terrain(terrain::make(terrain::Id::floor, {7, 7}));

        const size_t nr_actors_before = game_time::g_actors.size();

        // Spawn with a vector of 7 actors IDs, but with a distance limit that only allows for 6
        // actors.
        const actor::MonSpawnResult result = actor::spawn({3, 5}, {7, "MON_RAT"}, 4);

        REQUIRE(result.monsters.size() == 6);
        REQUIRE(result.monsters[0]->m_data->id == "MON_RAT");
        REQUIRE(result.monsters[1]->m_data->id == "MON_RAT");
        REQUIRE(result.monsters[2]->m_data->id == "MON_RAT");
        REQUIRE(result.monsters[3]->m_data->id == "MON_RAT");
        REQUIRE(result.monsters[4]->m_data->id == "MON_RAT");
        REQUIRE(result.monsters[5]->m_data->id == "MON_RAT");

        REQUIRE(game_time::g_actors.size() == nr_actors_before + 6);

        REQUIRE(actor_at({3, 5})->m_data->id == "MON_RAT");
        REQUIRE(actor_at({4, 5})->m_data->id == "MON_RAT");
        REQUIRE(actor_at({5, 5})->m_data->id == "MON_RAT");
        REQUIRE(actor_at({6, 5})->m_data->id == "MON_RAT");
        REQUIRE(actor_at({7, 5})->m_data->id == "MON_RAT");

        REQUIRE(actor_at({7, 6})->m_data->id == "MON_RAT");

        REQUIRE(actor_at({3, 7}) == nullptr);
        REQUIRE(actor_at({4, 7}) == nullptr);
        REQUIRE(actor_at({5, 7}) == nullptr);
        REQUIRE(actor_at({6, 7}) == nullptr);
        REQUIRE(actor_at({7, 7}) == nullptr);
}

TEST_CASE("Spawn actors, limited by distance, scattered")
{
        test_utils::init_all();

        init_map();

        const size_t nr_actors_before = game_time::g_actors.size();

        // Spawn with a vector of 4 actors IDs, but with a distance limit that only allows for 3
        // actors.
        const actor::MonSpawnResult result =
                actor::spawn(
                        {5, 5},
                        {4, "MON_RAT"},
                        1,
                        actor::SpawnScattered::yes);

        REQUIRE(result.monsters.size() == 3);
        REQUIRE(result.monsters[0]->m_data->id == "MON_RAT");
        REQUIRE(result.monsters[1]->m_data->id == "MON_RAT");
        REQUIRE(result.monsters[2]->m_data->id == "MON_RAT");

        REQUIRE(game_time::g_actors.size() == nr_actors_before + 3);

        REQUIRE(actor_at({3, 5}) == nullptr);
        REQUIRE(actor_at({4, 5})->m_data->id == "MON_RAT");
        REQUIRE(actor_at({5, 5})->m_data->id == "MON_RAT");
        REQUIRE(actor_at({6, 5})->m_data->id == "MON_RAT");
        REQUIRE(actor_at({7, 5}) == nullptr);
}

TEST_CASE("Spawn actors, allow large distance, close to position")
{
        test_utils::init_all();

        init_map();

        const size_t nr_actors_before = game_time::g_actors.size();

        // Spawn with a vector of 5 actors IDs, and with a distance that is just enough to allow all
        // of them to spawn.
        const actor::MonSpawnResult result = actor::spawn({5, 5}, {5, "MON_RAT"}, 2);

        REQUIRE(result.monsters.size() == 5);
        REQUIRE(result.monsters[0]->m_data->id == "MON_RAT");
        REQUIRE(result.monsters[1]->m_data->id == "MON_RAT");
        REQUIRE(result.monsters[2]->m_data->id == "MON_RAT");
        REQUIRE(result.monsters[3]->m_data->id == "MON_RAT");
        REQUIRE(result.monsters[4]->m_data->id == "MON_RAT");

        REQUIRE(game_time::g_actors.size() == nr_actors_before + 5);

        REQUIRE(actor_at({3, 5})->m_data->id == "MON_RAT");
        REQUIRE(actor_at({4, 5})->m_data->id == "MON_RAT");
        REQUIRE(actor_at({5, 5})->m_data->id == "MON_RAT");
        REQUIRE(actor_at({6, 5})->m_data->id == "MON_RAT");
        REQUIRE(actor_at({7, 5})->m_data->id == "MON_RAT");
}

TEST_CASE("Spawn actors, allow large distance, scattered")
{
        test_utils::init_all();

        init_map();

        // Put extra space on the map, so that the spawn distance is actually limited by the
        // specified distance limit, and not constraints on the map.
        map::update_terrain(terrain::make(terrain::Id::floor, {2, 5}));
        map::update_terrain(terrain::make(terrain::Id::floor, {8, 5}));

        const size_t nr_actors_before = game_time::g_actors.size();

        // Spawn with a vector of 5 actors IDs, and with a distance that is just enough to allow all
        // of them to spawn.
        const actor::MonSpawnResult result =
                actor::spawn(
                        {5, 5},
                        {5, "MON_RAT"},
                        2,
                        actor::SpawnScattered::yes);

        REQUIRE(result.monsters.size() == 5);
        REQUIRE(result.monsters[0]->m_data->id == "MON_RAT");
        REQUIRE(result.monsters[1]->m_data->id == "MON_RAT");
        REQUIRE(result.monsters[2]->m_data->id == "MON_RAT");
        REQUIRE(result.monsters[3]->m_data->id == "MON_RAT");
        REQUIRE(result.monsters[4]->m_data->id == "MON_RAT");

        REQUIRE(game_time::g_actors.size() == nr_actors_before + 5);

        REQUIRE(actor_at({3, 5})->m_data->id == "MON_RAT");
        REQUIRE(actor_at({4, 5})->m_data->id == "MON_RAT");
        REQUIRE(actor_at({5, 5})->m_data->id == "MON_RAT");
        REQUIRE(actor_at({6, 5})->m_data->id == "MON_RAT");
        REQUIRE(actor_at({7, 5})->m_data->id == "MON_RAT");
}

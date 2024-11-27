// =============================================================================
// Copyright 2011-2024 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "actor.hpp"
#include "actor_data.hpp"
#include "actor_factory.hpp"
#include "catch.hpp"
#include "game_time.hpp"
#include "map.hpp"
#include "test_utils.hpp"

TEST_CASE("Test player acts twice compared to slow monster")
{
        test_utils::init_all();

        auto* mon = actor::make("MON_BLOATED_ZOMBIE", {7, 7});

        mon->m_mon_aware_state.aware_counter = 100;

        const std::string name_player = "Player";
        const std::string name_mon = "a Bloated Corpse";

        map::g_player->m_data->name_a = name_player;

        REQUIRE(map::g_player->m_delay == 0);
        REQUIRE(mon->m_delay == 0);

        // NOTE: Both the player and the monster start with zero delay, so
        // initially the player will act then the monster - however following
        // this it should always be player --> player --> monster.
        int expected_counter = 1;

        for (int i = 0; i < 1000; ++i) {
                INFO("Iteration index: " + std::to_string(i));
                INFO("Standard turn: " + std::to_string(game_time::turn_nr()));

                std::string expected_name;

                switch (expected_counter) {
                case 0:
                case 1:
                        expected_name = name_player;
                        break;

                case 2:
                        expected_name = name_mon;
                        break;
                }

                ++expected_counter;

                if (expected_counter == 3) {
                        expected_counter = 0;
                }

                const std::string current_actor_name = actor::name_a(*game_time::current_actor());

                REQUIRE(current_actor_name == expected_name);

                game_time::g_allow_tick = true;
                game_time::tick();
        }

        test_utils::cleanup_all();
}

TEST_CASE("Test standard turn incrementation with player only")
{
        test_utils::init_all();

        const std::string name_player = "Player";

        map::g_player->m_data->name_a = name_player;

        for (int i = 0; i < 1000; ++i) {
                INFO("Iteration index: " + std::to_string(i));

                const int expected_turn_nr = i;

                REQUIRE(game_time::turn_nr() == expected_turn_nr);

                const std::string current_actor_name = actor::name_a(*game_time::current_actor());

                REQUIRE(current_actor_name == name_player);

                game_time::g_allow_tick = true;
                game_time::tick();
        }

        test_utils::cleanup_all();
}

TEST_CASE("Test standard turn incrementation with player and monster")
{
        test_utils::init_all();

        auto* mon = actor::make("MON_ZOMBIE", {7, 7});

        mon->m_mon_aware_state.aware_counter = 100;

        const std::string name_player = "Player";
        const std::string name_mon = "a Reanimated Corpse";

        map::g_player->m_data->name_a = name_player;

        bool is_expected_player_is_current = true;

        for (int i = 0; i < 1000; ++i) {
                INFO("Iteration index: " + std::to_string(i));

                const int expected_turn_nr = (i / 2);

                REQUIRE(game_time::turn_nr() == expected_turn_nr);

                std::string expected_name =
                        is_expected_player_is_current
                        ? name_player
                        : name_mon;

                const std::string current_actor_name = actor::name_a(*game_time::current_actor());

                REQUIRE(current_actor_name == expected_name);

                game_time::g_allow_tick = true;
                game_time::tick();

                is_expected_player_is_current = !is_expected_player_is_current;
        }

        test_utils::cleanup_all();
}

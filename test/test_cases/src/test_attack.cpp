// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include <cstddef>

#include "actor.hpp"
#include "actor_factory.hpp"
#include "attack.hpp"
#include "catch.hpp"
#include "game_time.hpp"
#include "item_factory.hpp"
#include "item_weapon.hpp"
#include "map.hpp"
#include "msg_log.hpp"
#include "property_factory.hpp"
#include "terrain.hpp"
#include "terrain_factory.hpp"
#include "test_utils.hpp"

// Puts walls and floor in the following pattern:
//
// .#.
// .#.
// .#.
// .#.
// ...
//
static void init_terrain()
{
        int x = 7;

        for (int y = 5; y <= 9; ++y) {
                map::update_terrain(terrain::make(terrain::Id::floor, {x, y}));
        }

        x = 8;

        for (int y = 5; y <= 8; ++y) {
                map::update_terrain(terrain::make(terrain::Id::wall, {x, y}));
        }

        map::update_terrain(terrain::make(terrain::Id::floor, {x, 9}));

        x = 9;

        for (int y = 5; y <= 9; ++y) {
                map::update_terrain(terrain::make(terrain::Id::floor, {x, 9}));
        }
}

static bool starts_with_any_of(
        const std::string& str,
        const std::vector<std::string>& sub_strings)
{
        const auto& pred = [str](const auto& sub_str) {
                return str.find(sub_str, 0) == 0;
        };

        return (
                std::any_of(
                        std::begin(sub_strings),
                        std::end(sub_strings),
                        pred));
}

TEST_CASE("Seen hostile monster attacking seen friendly monster")
{
        test_utils::init_all();

        player_bon::pick_bg(Bg::war_vet);

        init_terrain();

        map::g_player->m_pos.set(7, 5);

        auto* mon_allied = actor::make("MON_GHOUL", {7, 6});
        auto* mon_hostile = actor::make("MON_ZOMBIE", {7, 7});

        // Make the hostile monster aware so it doesn't become aware by
        // attacking (attacking bumps awareness) and runs its "aware phrase".
        mon_hostile->m_mon_aware_state.aware_counter = 100;

        mon_allied->m_leader = map::g_player;

        map::update_vision();

        // Reset/clear the message history
        msg_log::init();

        auto& wpn =
                static_cast<item::Wpn&>(
                        *mon_hostile->m_inv.m_intrinsics[0]);

        attack::melee(mon_hostile, mon_hostile->m_pos, mon_allied->m_pos, wpn);

        // Send the current message to history
        msg_log::clear();

        const auto& history = msg_log::history();

        REQUIRE(history.size() == 1);

        const std::vector<std::string> possible_messages = {
                "The Reanimated Corpse claws the Ghoul",
                "The Reanimated Corpse misses the Ghoul"};

        REQUIRE(starts_with_any_of(history[0].text(), possible_messages));

        test_utils::cleanup_all();
}

TEST_CASE("Hostile monster outside FOV attacking friendly monster outside FOV")
{
        test_utils::init_all();

        player_bon::pick_bg(Bg::war_vet);

        init_terrain();

        map::g_player->m_pos.set(7, 5);

        auto* mon_allied = actor::make("MON_GHOUL", {9, 6});
        auto* mon_hostile = actor::make("MON_ZOMBIE", {9, 7});

        // Make the hostile monster aware so it doesn't become aware by
        // attacking (attacking bumps awareness) and runs its "aware phrase".
        mon_hostile->m_mon_aware_state.aware_counter = 100;

        const auto& mon_allied_player_aware_counter =
                mon_allied->m_mon_aware_state.player_aware_of_me_counter;

        const auto& mon_hostile_player_aware_counter =
                mon_hostile->m_mon_aware_state.player_aware_of_me_counter;

        mon_allied->m_leader = map::g_player;

        map::update_vision();

        // Reset/clear the message history
        msg_log::init();

        REQUIRE(mon_allied_player_aware_counter == 0);
        REQUIRE(mon_hostile_player_aware_counter == 0);
        REQUIRE(!mon_hostile->m_data->has_player_seen);

        auto& wpn =
                static_cast<item::Wpn&>(
                        *mon_hostile->m_inv.m_intrinsics[0]);

        attack::melee(mon_hostile, mon_hostile->m_pos, mon_allied->m_pos, wpn);

        // Send the current message to history
        msg_log::clear();

        const auto& history = msg_log::history();

        REQUIRE(history.size() == 1);
        REQUIRE(history[0].text() == "I hear fighting.(SE)");

        REQUIRE(mon_allied_player_aware_counter == 0);
        REQUIRE(mon_hostile_player_aware_counter > 0);
        REQUIRE(!mon_hostile->m_data->has_player_seen);

        test_utils::cleanup_all();
}

TEST_CASE("Invisible hostile monster attacking seen friendly monster")
{
        test_utils::init_all();

        player_bon::pick_bg(Bg::war_vet);

        init_terrain();

        map::g_player->m_pos.set(7, 5);

        auto* mon_allied = actor::make("MON_GHOUL", {7, 6});
        auto* mon_hostile = actor::make("MON_ZOMBIE", {7, 7});

        const auto& mon_allied_player_aware_counter =
                mon_allied->m_mon_aware_state.player_aware_of_me_counter;

        const auto& mon_hostile_player_aware_counter =
                mon_hostile->m_mon_aware_state.player_aware_of_me_counter;

        mon_hostile->m_properties.apply(
                property_factory::make(
                        PropId::invis));

        // Make the hostile monster aware so it doesn't become aware by
        // attacking (attacking bumps awareness) and runs its "aware phrase".
        mon_hostile->m_mon_aware_state.aware_counter = 100;

        mon_allied->m_leader = map::g_player;

        map::update_vision();

        // Reset/clear the message history
        msg_log::init();

        REQUIRE(mon_allied_player_aware_counter > 0);
        REQUIRE(mon_hostile_player_aware_counter == 0);
        REQUIRE(!mon_hostile->m_data->has_player_seen);

        auto& wpn =
                static_cast<item::Wpn&>(
                        *mon_hostile->m_inv.m_intrinsics[0]);

        attack::melee(mon_hostile, mon_hostile->m_pos, mon_allied->m_pos, wpn);

        // Send the current message to history
        msg_log::clear();

        const auto& history = msg_log::history();

        REQUIRE(history.size() == 1);

        const std::vector<std::string> possible_messages = {
                "It claws the Ghoul",
                "It misses the Ghoul"};

        REQUIRE(starts_with_any_of(history[0].text(), possible_messages));

        REQUIRE(mon_allied_player_aware_counter > 0);
        REQUIRE(mon_hostile_player_aware_counter > 0);
        REQUIRE(!mon_hostile->m_data->has_player_seen);

        test_utils::cleanup_all();
}

TEST_CASE("Invisible hostile monster attacking invisible friendly monster")
{
        test_utils::init_all();

        player_bon::pick_bg(Bg::war_vet);

        init_terrain();

        map::g_player->m_pos.set(7, 5);

        // First place the monsters outside FOV to not trigger player awareness
        // when vision is updated when invisibility is applied.
        auto* mon_allied = actor::make("MON_GHOUL", {27, 6});
        auto* mon_hostile = actor::make("MON_ZOMBIE", {27, 7});

        auto& mon_allied_player_aware_counter =
                mon_allied->m_mon_aware_state.player_aware_of_me_counter;

        auto& mon_hostile_player_aware_counter =
                mon_hostile->m_mon_aware_state.player_aware_of_me_counter;

        mon_allied->m_properties.apply(
                property_factory::make(
                        PropId::invis));

        mon_hostile->m_properties.apply(
                property_factory::make(
                        PropId::invis));

        mon_allied_player_aware_counter = 0;
        mon_hostile_player_aware_counter = 0;

        // Make the hostile monster aware so it doesn't become aware by
        // attacking (attacking bumps awareness) and runs its "aware phrase".
        mon_hostile->m_mon_aware_state.aware_counter = 100;

        mon_allied->m_leader = map::g_player;

        map::update_vision();

        mon_allied->m_pos.set(7, 6);
        mon_hostile->m_pos.set(7, 7);

        // Reset/clear the message history
        msg_log::init();

        REQUIRE(mon_allied_player_aware_counter == 0);
        REQUIRE(mon_hostile_player_aware_counter == 0);
        REQUIRE(!mon_hostile->m_data->has_player_seen);

        auto& wpn =
                static_cast<item::Wpn&>(
                        *mon_hostile->m_inv.m_intrinsics[0]);

        attack::melee(mon_hostile, mon_hostile->m_pos, mon_allied->m_pos, wpn);

        // Send the current message to history
        msg_log::clear();

        const auto& history = msg_log::history();

        REQUIRE(history.size() == 1);
        REQUIRE(history[0].text() == "I hear fighting.(S)");

        REQUIRE(mon_allied_player_aware_counter == 0);
        REQUIRE(mon_hostile_player_aware_counter > 0);
        REQUIRE(!mon_hostile->m_data->has_player_seen);

        test_utils::cleanup_all();
}

TEST_CASE("Visible friendly monster attacking invisible hostile monster")
{
        test_utils::init_all();

        player_bon::pick_bg(Bg::war_vet);

        init_terrain();

        map::g_player->m_pos.set(7, 5);

        auto* mon_allied = actor::make("MON_GHOUL", {7, 6});
        auto* mon_hostile = actor::make("MON_ZOMBIE", {7, 7});

        auto& mon_allied_player_aware_counter =
                mon_allied->m_mon_aware_state.player_aware_of_me_counter;

        auto& mon_hostile_player_aware_counter =
                mon_hostile->m_mon_aware_state.player_aware_of_me_counter;

        mon_hostile->m_properties.apply(
                property_factory::make(
                        PropId::invis));

        mon_allied_player_aware_counter = 0;
        mon_hostile_player_aware_counter = 0;

        // Make the hostile monster aware so it doesn't become aware by being
        // attacked and runs its "aware phrase".
        mon_hostile->m_mon_aware_state.aware_counter = 100;

        mon_allied->m_leader = map::g_player;

        map::update_vision();

        // Reset/clear the message history
        msg_log::init();

        REQUIRE(mon_hostile_player_aware_counter == 0);

        auto& wpn =
                static_cast<item::Wpn&>(
                        *mon_allied->m_inv.m_intrinsics[0]);

        attack::melee(mon_allied, mon_allied->m_pos, mon_hostile->m_pos, wpn);

        // Send the current message to history
        msg_log::clear();

        const auto& history = msg_log::history();

        REQUIRE(history.size() == 1);

        const std::vector<std::string> possible_messages = {
                "The Ghoul claws it",
                "The Ghoul misses it"};

        REQUIRE(starts_with_any_of(history[0].text(), possible_messages));

        REQUIRE(mon_hostile_player_aware_counter > 0);

        test_utils::cleanup_all();
}

TEST_CASE("Player kicking invisible monster")
{
        test_utils::init_all();

        player_bon::pick_bg(Bg::war_vet);

        init_terrain();

        map::g_player->m_pos.set(7, 5);

        map::update_terrain(terrain::make(terrain::Id::floor, {8, 5}));

        auto* mon = actor::make("MON_ZOMBIE", {8, 5});

        mon->m_properties.apply(property_factory::make(PropId::invis));

        // Make the hostile monster aware so it doesn't become aware and runs
        // its "aware phrase".
        mon->m_mon_aware_state.aware_counter = 100;

        map::update_vision();

        // Reset/clear the message history
        msg_log::init();

        REQUIRE(mon->m_mon_aware_state.player_aware_of_me_counter <= 0);
        REQUIRE(!mon->m_data->has_player_seen);

        map::g_player->kick_mon(*mon);

        // Send the current message to history
        msg_log::clear();

        const auto& history = msg_log::history();

        REQUIRE(history.size() == 1);

        const std::vector<std::string> possible_messages = {
                "I miss",
                "I kick it"};

        REQUIRE(starts_with_any_of(history[0].text(), possible_messages));

        REQUIRE(mon->m_mon_aware_state.player_aware_of_me_counter > 0);
        REQUIRE(mon->m_mon_aware_state.aware_counter > 0);
        REQUIRE(!mon->m_data->has_player_seen);

        test_utils::cleanup_all();
}

TEST_CASE("Test player killing invisible monster")
{
        test_utils::init_all();

        player_bon::pick_bg(Bg::war_vet);

        init_terrain();

        map::g_player->m_pos.set(7, 5);

        REQUIRE(!map::g_player->m_inv.has_item_in_slot(SlotId::wpn));

        auto* const wpn =
                static_cast<item::Wpn*>(
                        item::make(item::Id::dagger));

        wpn->set_melee_plus(100);  // Guaranteed to kill the monster.

        map::g_player->m_inv.put_in_slot(SlotId::wpn, wpn, Verbose::no);

        map::update_terrain(terrain::make(terrain::Id::floor, {8, 5}));

        auto* mon = actor::make("MON_ZOMBIE", {8, 5});

        mon->m_properties.apply(property_factory::make(PropId::invis));

        // Make the hostile monster aware so it doesn't become aware and runs
        // its "aware phrase".
        mon->m_mon_aware_state.aware_counter = 100;

        map::update_vision();

        // Reset/clear the message history
        msg_log::init();

        REQUIRE(mon->m_mon_aware_state.player_aware_of_me_counter <= 0);
        REQUIRE(!mon->m_data->has_player_seen);

        while (true) {
                attack::melee(map::g_player, map::g_player->m_pos, mon->m_pos, *wpn);

                game_time::g_allow_tick = true;

                const bool exists =
                        std::find(
                                std::begin(game_time::g_actors),
                                std::end(game_time::g_actors),
                                mon) != std::end(game_time::g_actors);

                if (!exists || !mon->is_alive()) {
                        break;
                }
        }

        REQUIRE(mon->m_mon_aware_state.player_aware_of_me_counter > 0);
        REQUIRE(!mon->m_data->has_player_seen);

        test_utils::cleanup_all();
}

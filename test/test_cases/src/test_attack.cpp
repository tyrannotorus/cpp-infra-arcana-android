// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include <cstddef>

#include "actor.hpp"
#include "actor_death.hpp"
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

        actor::Actor* mon_allied = actor::make("MON_GHOUL", {7, 6});
        actor::Actor* mon_hostile = actor::make("MON_ZOMBIE", {7, 7});

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

        actor::Actor* mon_allied = actor::make("MON_GHOUL", {9, 6});
        actor::Actor* mon_hostile = actor::make("MON_ZOMBIE", {9, 7});

        // Make the hostile monster aware so it doesn't become aware by
        // attacking (attacking bumps awareness) and runs its "aware phrase".
        mon_hostile->m_mon_aware_state.aware_counter = 100;

        const int& mon_allied_player_aware_counter =
                mon_allied->m_mon_aware_state.player_aware_of_me_counter;

        const int& mon_hostile_player_aware_counter =
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

        actor::Actor* mon_allied = actor::make("MON_GHOUL", {7, 6});
        actor::Actor* mon_hostile = actor::make("MON_ZOMBIE", {7, 7});

        const int& mon_allied_player_aware_counter =
                mon_allied->m_mon_aware_state.player_aware_of_me_counter;

        const int& mon_hostile_player_aware_counter =
                mon_hostile->m_mon_aware_state.player_aware_of_me_counter;

        mon_hostile->m_properties.apply(prop::make(prop::Id::invis));

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
        actor::Actor* mon_allied = actor::make("MON_GHOUL", {27, 6});
        actor::Actor* mon_hostile = actor::make("MON_ZOMBIE", {27, 7});

        int& mon_allied_player_aware_counter =
                mon_allied->m_mon_aware_state.player_aware_of_me_counter;

        int& mon_hostile_player_aware_counter =
                mon_hostile->m_mon_aware_state.player_aware_of_me_counter;

        mon_allied->m_properties.apply(prop::make(prop::Id::invis));
        mon_hostile->m_properties.apply(prop::make(prop::Id::invis));

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

        actor::Actor* mon_allied = actor::make("MON_GHOUL", {7, 6});
        actor::Actor* mon_hostile = actor::make("MON_ZOMBIE", {7, 7});

        int& mon_allied_player_aware_counter =
                mon_allied->m_mon_aware_state.player_aware_of_me_counter;

        int& mon_hostile_player_aware_counter =
                mon_hostile->m_mon_aware_state.player_aware_of_me_counter;

        mon_hostile->m_properties.apply(prop::make(prop::Id::invis));

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

        actor::Actor* mon = actor::make("MON_ZOMBIE", {8, 5});

        mon->m_properties.apply(prop::make(prop::Id::invis));

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

TEST_CASE("Player killing invisible monster")
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

        actor::Actor* mon = actor::make("MON_ZOMBIE", {8, 5});

        mon->m_properties.apply(prop::make(prop::Id::invis));

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

TEST_CASE("Zero damage attacks do not cause damage")
{
        test_utils::init_all();

        player_bon::pick_bg(Bg::war_vet);

        init_terrain();

        for (int x = 7; x <= 9; ++x) {
                map::update_terrain(terrain::make(terrain::Id::floor, {x, 5}));
        }

        map::g_player->m_pos.set(7, 5);

        // Test with a spitting cobra spit.

        actor::Actor* mon = actor::make("MON_SPITTING_COBRA", {9, 5});

        // NOTE: Assuming that spitting is the second intrinsic attack of
        // spitting cobras (first one is biting).
        auto* wpn = static_cast<item::Wpn*>(mon->m_inv.m_intrinsics[1]);

        REQUIRE(wpn->id() == item::Id::intr_snake_venom_spit);
        REQUIRE(!map::g_player->m_properties.has(prop::Id::blind));
        REQUIRE(map::g_player->m_hp == actor::max_hp(*map::g_player));

        while (true) {
                game_time::g_allow_tick = true;

                attack::ranged(mon, mon->m_pos, map::g_player->m_pos, *wpn);

                if (map::g_player->m_properties.has(prop::Id::blind)) {
                        // OK, the cobra did spit on the player now.
                        break;
                }
        }

        // The player should not have lost any hit points.
        REQUIRE(map::g_player->m_hp == actor::max_hp(*map::g_player));

        actor::kill(*mon, IsDestroyed::yes, AllowGore::no, AllowDropItems::no);

        // Test with a deep one net.

        mon = actor::make("MON_DEEP_ONE", {9, 5});

        // NOTE: Assuming that net throwing is the second intrinsic attack of
        // deep ones (first one is spear attack).
        wpn = static_cast<item::Wpn*>(mon->m_inv.m_intrinsics[1]);

        REQUIRE(wpn->id() == item::Id::intr_net_throw);
        REQUIRE(!map::g_player->m_properties.has(prop::Id::entangled));
        REQUIRE(map::g_player->m_hp == actor::max_hp(*map::g_player));

        while (true) {
                game_time::g_allow_tick = true;

                attack::ranged(mon, mon->m_pos, map::g_player->m_pos, *wpn);

                if (map::g_player->m_properties.has(prop::Id::entangled)) {
                        // OK, the deep one threw a net on the player now.
                        break;
                }
        }

        // The player should not have lost any hit points.
        REQUIRE(map::g_player->m_hp == actor::max_hp(*map::g_player));
}

TEST_CASE("Resisting attack damage type also resists paralysis")
{
        test_utils::init_all();

        player_bon::pick_bg(Bg::war_vet);

        init_terrain();

        map::g_player->m_pos.set(7, 5);

        map::g_player->change_max_hp(100000);
        map::g_player->restore_hp(100000, false);

        // Ensure that the spiked mace paralysis always activates.
        item::g_data[(size_t)item::Id::spiked_mace].melee.prop_applied.pct_chance_to_apply = 100;

        actor::Actor* mon = actor::make("MON_ZEALOT", {8, 5});

        // Assuming that Zealots wield a spiked mace.
        auto* const wpn =
                static_cast<item::Wpn*>(
                        mon->m_inv.item_in_slot(SlotId::wpn));

        REQUIRE(wpn->id() == item::Id::spiked_mace);

        REQUIRE(!map::g_player->m_properties.has(prop::Id::paralyzed));
        REQUIRE(map::g_player->m_hp == actor::max_hp(*map::g_player));

        while (true) {
                game_time::g_allow_tick = true;

                REQUIRE(map::g_player->is_alive());

                REQUIRE(map::g_player->is_alive());

                map::g_player->restore_hp(100000, false);
                map::g_player->m_properties.end_prop(prop::Id::wound);
                map::g_player->restore_shock(100, true);

                attack::melee(mon, mon->m_pos, map::g_player->m_pos, *wpn);

                if (map::g_player->m_hp < actor::max_hp(*map::g_player)) {
                        // OK, the player has been hit.
                        break;
                }
        }

        // The player should now be paralyzed.
        REQUIRE(map::g_player->m_properties.has(prop::Id::paralyzed));

        map::g_player->m_properties.end_prop(prop::Id::paralyzed);

        REQUIRE(!map::g_player->m_properties.has(prop::Id::paralyzed));

        // NOTE: Since the player should neither take damage nor get paralyzed
        // now, there is nothing we can check here to know that the player
        // actually got hit (except maybe the message log), therefore just run a
        // huge number of attacks so that it's practically impossible that the
        // player avoided all of them.
        for (int i = 0; i < 10000; ++i) {
                game_time::g_allow_tick = true;

                REQUIRE(map::g_player->is_alive());

                map::g_player->restore_shock(100, true);

                // Make the player resistant to physical damage.
                map::g_player->m_properties.apply(prop::make(prop::Id::r_phys));

                REQUIRE(map::g_player->m_properties.has(prop::Id::r_phys));

                attack::melee(mon, mon->m_pos, map::g_player->m_pos, *wpn);
        }

        // The player should not be paralyzed now.
        REQUIRE(!map::g_player->m_properties.has(prop::Id::paralyzed));

        // The player should not have lost any hit points.
        REQUIRE(map::g_player->m_hp == actor::max_hp(*map::g_player));
}

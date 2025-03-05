// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "actor.hpp"
#include "actor_factory.hpp"
#include "actor_move.hpp"
#include "catch.hpp"
#include "item_factory.hpp"
#include "map.hpp"
#include "msg_log.hpp"
#include "property.hpp"
#include "property_data.hpp"
#include "property_factory.hpp"
#include "property_handler.hpp"
#include "terrain.hpp"
#include "terrain_factory.hpp"
#include "test_utils.hpp"

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

static void clear_log_immediately_and_send_messages_to_history()
{
        // Hack to immediately cler the log without fading.
        msg_log::more_prompt();
}

TEST_CASE("Infection triggers disease")
{
        test_utils::init_all();

        auto& properties = map::g_player->m_properties;

        properties.apply(prop::make(prop::Id::infected));

        REQUIRE(properties.has(prop::Id::infected));
        REQUIRE(!properties.has(prop::Id::diseased));

        // Tick the infected property enough to no longer exist (could use
        // while-true loop, but this could cause a failing test to get stuck)
        for (int i = 0; i < 100000; ++i) {
                properties.on_turn_begin();
        }

        REQUIRE(!properties.has(prop::Id::infected));
        REQUIRE(properties.has(prop::Id::diseased));

        test_utils::cleanup_all();
}

TEST_CASE("Number turns active")
{
        test_utils::init_all();

        auto& properties = map::g_player->m_properties;

        auto* const blind = prop::make(prop::Id::blind);
        blind->set_duration(500);

        properties.apply(blind);

        REQUIRE(properties.prop(prop::Id::blind)->nr_turns_active() == 0);

        properties.on_turn_begin();
        properties.on_turn_begin();
        properties.on_turn_begin();

        REQUIRE(properties.prop(prop::Id::blind)->nr_turns_active() == 3);

        test_utils::cleanup_all();
}

TEST_CASE("Frenzy allows moving away from monster if LOS blocked")
{
        test_utils::init_all();

        auto& properties = map::g_player->m_properties;

        properties.apply(prop::make(prop::Id::frenzied));

        map::g_player->m_pos.set(10, 10);

        auto* const mon = actor::make("MON_ZOMBIE", {14, 10});

        map::update_vision();

        // Try moving away from a seen monster - this should NOT be allowed.
        actor::do_move_action(*map::g_player, Dir::left);

        REQUIRE(map::g_player->m_pos == P(10, 10));

        map::update_terrain(terrain::make(terrain::Id::wall, {12, 10}));

        map::update_vision();

        mon->m_mon_aware_state.player_aware_of_me_counter = 1;

        // Try moving away from a known monster to the right, but with the LOS
        // to the monster blocked - this SHOULD be allowed.
        actor::do_move_action(*map::g_player, Dir::left);

        REQUIRE(map::g_player->m_pos == P(9, 10));

        test_utils::cleanup_all();
}

TEST_CASE("Frenzy allows moving away from unseen known monster")
{
        test_utils::init_all();

        auto& properties = map::g_player->m_properties;

        properties.apply(prop::make(prop::Id::frenzied));

        map::g_player->m_pos.set(10, 10);

        auto* const mon = actor::make("MON_ZOMBIE", {14, 10});

        map::update_vision();

        // Try moving away from a seen monster - this should NOT be allowed.
        actor::do_move_action(*map::g_player, Dir::left);

        REQUIRE(map::g_player->m_pos == P(10, 10));

        mon->m_properties.apply(prop::make(prop::Id::invis));

        map::update_vision();

        mon->m_mon_aware_state.player_aware_of_me_counter = 1;
        mon->m_mon_aware_state.aware_counter = 1;

        // Try moving away from an unseen known monster - this SHOULD be
        // allowed.
        actor::do_move_action(*map::g_player, Dir::left);

        REQUIRE(map::g_player->m_pos == P(9, 10));

        test_utils::cleanup_all();
}

TEST_CASE("Frenzy allows attacking adjacent unseen known monster")
{
        test_utils::init_all();

        map::g_player->m_inv.put_in_slot(
                SlotId::wpn,
                item::make(item::Id::dagger),
                Verbose::no);

        auto& properties = map::g_player->m_properties;

        properties.apply(prop::make(prop::Id::frenzied));

        map::g_player->m_pos.set(10, 10);

        actor::make("MON_ZOMBIE", {14, 10});

        map::update_vision();

        // Try moving away from a seen monster - this should NOT be allowed.
        actor::do_move_action(*map::g_player, Dir::down);

        REQUIRE(map::g_player->m_pos == P(10, 10));

        auto* const mon_2 = actor::make("MON_ZOMBIE", {10, 11});

        mon_2->m_properties.apply(prop::make(prop::Id::invis));

        map::update_vision();

        // Reset/clear the message history
        msg_log::init();

        mon_2->m_mon_aware_state.player_aware_of_me_counter = 1;
        mon_2->m_mon_aware_state.aware_counter = 1;

        // Try attacking an unseen known monster directly below the player, with
        // a seen monster to the right - this SHOULD be allowed.
        actor::do_move_action(*map::g_player, Dir::down);

        clear_log_immediately_and_send_messages_to_history();

        const auto& history = msg_log::history();

        REQUIRE(!history.empty());

        const std::vector<std::string> possible_messages = {
                "I STAB",
                "I MISS"};

        REQUIRE(starts_with_any_of(history[0].text(), possible_messages));

        REQUIRE(map::g_player->m_pos == P(10, 10));

        test_utils::cleanup_all();
}

TEST_CASE("Poison damage")
{
        test_utils::init_all();

        auto& properties = map::g_player->m_properties;

        prop::Prop* const poison = prop::make(prop::Id::poisoned);
        poison->set_duration(10000);
        properties.apply(poison);

        map::g_player->m_hp = 11;
        map::g_player->m_base_max_hp = 11;

        for (int i = 0; i < 10000; ++i) {
                properties.on_turn_begin();
        }

        REQUIRE(map::g_player->m_hp == 5);
        REQUIRE(actor::is_alive(*map::g_player));

        map::g_player->m_hp = 10;
        map::g_player->m_base_max_hp = 10;

        for (int i = 0; i < 10000; ++i) {
                properties.on_turn_begin();
        }

        REQUIRE(map::g_player->m_hp == 5);
        REQUIRE(actor::is_alive(*map::g_player));

        map::g_player->m_hp = 2;
        map::g_player->m_base_max_hp = 2;

        for (int i = 0; i < 10000; ++i) {
                properties.on_turn_begin();
        }

        REQUIRE(map::g_player->m_hp == 1);
        REQUIRE(actor::is_alive(*map::g_player));

        map::g_player->m_hp = 1;
        map::g_player->m_base_max_hp = 1;

        for (int i = 0; i < 10000; ++i) {
                properties.on_turn_begin();
        }

        REQUIRE(map::g_player->m_hp == 1);
        REQUIRE(actor::is_alive(*map::g_player));

        test_utils::cleanup_all();
}

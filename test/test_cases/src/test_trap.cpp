// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "actor.hpp"
#include "actor_data.hpp"
#include "actor_factory.hpp"
#include "actor_move.hpp"
#include "catch.hpp"
#include "direction.hpp"
#include "game_time.hpp"
#include "global.hpp"
#include "map.hpp"
#include "player_bon.hpp"
#include "player_spells.hpp"
#include "pos.hpp"
#include "property_data.hpp"
#include "property_handler.hpp"
#include "spells.hpp"
#include "terrain.hpp"
#include "terrain_factory.hpp"
#include "terrain_trap.hpp"
#include "test_utils.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static void put_unlearn_spell_trap(const P& pos)
{
        map::update_terrain(terrain::make(terrain::Id::floor, pos));

        auto* const trap =
                static_cast<terrain::Trap*>(
                        terrain::make(
                                terrain::Id::trap,
                                pos));

        trap->try_init_type(terrain::TrapId::unlearn_spell);

        trap->set_mimic_terrain(terrain::make(terrain::Id::floor, pos));

        map::update_terrain(trap);

        trap->reveal(terrain::PrintRevealMsg::no);
}

// -----------------------------------------------------------------------------
// Test cases
// -----------------------------------------------------------------------------
TEST_CASE("Spider web")
{
        // Test that a monster can get stuck in a spider web, and that they can
        // break free

        const P pos_l(5, 7);
        const P pos_r(6, 7);

        // TODO: Is getting stuck deterministic now? Perhaps there is no need to
        // run this in a loop?

        bool tested_stuck = false;
        bool tested_unstuck = false;

        while (!(tested_stuck && tested_unstuck)) {
                test_utils::init_all();

                map::update_terrain(terrain::make(terrain::Id::floor, pos_l));

                {
                        auto* const web =
                                static_cast<terrain::Trap*>(
                                        terrain::make(
                                                terrain::Id::trap,
                                                pos_r));

                        web->try_init_type(terrain::TrapId::web);

                        web->set_mimic_terrain(
                                terrain::make(terrain::Id::floor, pos_r));

                        map::update_terrain(web);

                        web->reveal(terrain::PrintRevealMsg::no);
                }

                actor::Actor* const actor =
                        actor::make("MON_ZOMBIE", pos_l);

                // Requirement for triggering traps
                actor->m_ai_state.is_target_seen = true;

                // Awareness > 0 required for triggering trap
                actor->m_mon_aware_state.aware_counter = 42;

                // Move the monster into the trap, and back again
                actor->m_pos = pos_l;
                game_time::g_allow_tick = true;
                actor::do_move_action(*actor, Dir::right);

                // It should never be possible to move on the first try
                REQUIRE(actor->m_pos == pos_r);

                REQUIRE(actor->m_properties.has(PropId::entangled));

                // This may or may not unstuck the monster
                game_time::g_allow_tick = true;
                actor::do_move_action(*actor, Dir::left);

                // If the move above did unstuck the monster, this command will
                // move it one step to the left
                game_time::g_allow_tick = true;
                actor::do_move_action(*actor, Dir::left);

                if (actor->m_pos == pos_r) {
                        tested_stuck = true;
                }
                else if (actor->m_pos == pos_l) {
                        tested_unstuck = true;

                        REQUIRE(!actor->m_properties.has(PropId::entangled));
                }

                test_utils::cleanup_all();
        }

        REQUIRE(tested_stuck);
        REQUIRE(tested_unstuck);
}

TEST_CASE("Forget spells")
{
        // Test that the "forget spell" makes you forget spells.

        const P pos_l(5, 7);
        const P pos_r(6, 7);

        test_utils::init_all();

        map::update_terrain(terrain::make(terrain::Id::floor, pos_l));

        put_unlearn_spell_trap(pos_r);

        player_spells::learn_spell(SpellId::darkbolt, Verbose::no);
        player_spells::learn_spell(SpellId::heal, Verbose::no);

        REQUIRE(player_spells::is_spell_learned(SpellId::darkbolt));
        REQUIRE(player_spells::is_spell_learned(SpellId::heal));

        REQUIRE(!player_spells::is_spell_forgotten(SpellId::darkbolt));
        REQUIRE(!player_spells::is_spell_forgotten(SpellId::heal));

        // Step into the trap
        map::g_player->m_pos = pos_l;
        game_time::g_allow_tick = true;
        actor::do_move_action(*map::g_player, Dir::right);

        REQUIRE(map::g_player->m_pos == pos_r);

        // Both spells should still be learned.
        REQUIRE(player_spells::is_spell_learned(SpellId::darkbolt));
        REQUIRE(player_spells::is_spell_learned(SpellId::heal));

        // One spell should be forgotten.
        const int nr_forgotten_after_first_trigger =
                (int)player_spells::is_spell_forgotten(SpellId::darkbolt) +
                (int)player_spells::is_spell_forgotten(SpellId::heal);

        REQUIRE(nr_forgotten_after_first_trigger == 1);

        // Step into the trap again
        put_unlearn_spell_trap(pos_r);

        map::g_player->m_pos = pos_l;
        game_time::g_allow_tick = true;
        actor::do_move_action(*map::g_player, Dir::right);

        REQUIRE(map::g_player->m_pos == pos_r);

        // Both spells should still be learned.
        REQUIRE(player_spells::is_spell_learned(SpellId::darkbolt));
        REQUIRE(player_spells::is_spell_learned(SpellId::heal));

        // Now both spells should be forgotten.
        REQUIRE(player_spells::is_spell_forgotten(SpellId::darkbolt));
        REQUIRE(player_spells::is_spell_forgotten(SpellId::heal));

        test_utils::cleanup_all();
}

TEST_CASE("Do not forget frenzy")
{
        // Test that the "forget spell" trap will not make you forget Ghoul
        // Frenzy.

        const P pos_l(5, 7);
        const P pos_r(6, 7);

        test_utils::init_all();

        map::update_terrain(terrain::make(terrain::Id::floor, pos_l));

        put_unlearn_spell_trap(pos_r);

        player_bon::pick_bg(Bg::ghoul);

        player_spells::learn_spell(SpellId::darkbolt, Verbose::no);

        REQUIRE(player_spells::is_spell_learned(SpellId::darkbolt));
        REQUIRE(player_spells::is_spell_learned(SpellId::frenzy));

        REQUIRE(!player_spells::is_spell_forgotten(SpellId::darkbolt));
        REQUIRE(!player_spells::is_spell_forgotten(SpellId::frenzy));

        // Step into the trap
        map::g_player->m_pos = pos_l;
        game_time::g_allow_tick = true;
        actor::do_move_action(*map::g_player, Dir::right);

        REQUIRE(map::g_player->m_pos == pos_r);

        // Both spells should still be learned.
        REQUIRE(player_spells::is_spell_learned(SpellId::darkbolt));
        REQUIRE(player_spells::is_spell_learned(SpellId::frenzy));

        // Frenzy should not be forgotten now.
        REQUIRE(player_spells::is_spell_forgotten(SpellId::darkbolt));
        REQUIRE(!player_spells::is_spell_forgotten(SpellId::frenzy));

        // Step into the trap again
        put_unlearn_spell_trap(pos_r);

        map::g_player->m_pos = pos_l;
        game_time::g_allow_tick = true;
        actor::do_move_action(*map::g_player, Dir::right);

        REQUIRE(map::g_player->m_pos == pos_r);

        // Both spells should still be learned.
        REQUIRE(player_spells::is_spell_learned(SpellId::darkbolt));
        REQUIRE(player_spells::is_spell_learned(SpellId::frenzy));

        // Frenzy should STILL not be forgotten now.
        REQUIRE(player_spells::is_spell_forgotten(SpellId::darkbolt));
        REQUIRE(!player_spells::is_spell_forgotten(SpellId::frenzy));

        test_utils::cleanup_all();
}

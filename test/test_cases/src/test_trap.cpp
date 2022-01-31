// =============================================================================
// Copyright 2011-2021 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "actor.hpp"
#include "actor_data.hpp"
#include "actor_factory.hpp"
#include "actor_mon.hpp"
#include "actor_move.hpp"
#include "actor_player.hpp"
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
#include "terrain_trap.hpp"
#include "test_utils.hpp"

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

        while (!(tested_stuck && tested_unstuck))
        {
                test_utils::init_all();

                map::put(new terrain::Floor(pos_l));

                {
                        auto* const web = new terrain::Trap(
                                pos_r,
                                new terrain::Floor(pos_r),
                                terrain::TrapId::web);

                        map::put(web);

                        web->reveal(terrain::PrintRevealMsg::no);
                }

                auto* const actor = actor::make(actor::Id::zombie, pos_l);

                auto* const mon = static_cast<actor::Mon*>(actor);

                // Requirement for triggering traps
                mon->m_ai_state.is_target_seen = true;

                // Awareness > 0 required for triggering trap
                mon->m_mon_aware_state.aware_counter = 42;

                // Move the monster into the trap, and back again
                mon->m_pos = pos_l;
                game_time::g_allow_tick = true;
                actor::move(*mon, Dir::right);

                // It should never be possible to move on the first try
                REQUIRE(mon->m_pos == pos_r);

                REQUIRE(mon->m_properties.has(PropId::entangled));

                // This may or may not unstuck the monster
                game_time::g_allow_tick = true;
                actor::move(*mon, Dir::left);

                // If the move above did unstuck the monster, this command will
                // move it one step to the left
                game_time::g_allow_tick = true;
                actor::move(*mon, Dir::left);

                if (mon->m_pos == pos_r)
                {
                        tested_stuck = true;
                }
                else if (mon->m_pos == pos_l)
                {
                        tested_unstuck = true;

                        REQUIRE(!mon->m_properties.has(PropId::entangled));
                }

                test_utils::cleanup_all();
        }

        REQUIRE(tested_stuck);
        REQUIRE(tested_unstuck);
}

TEST_CASE("Unlearn spells")
{
        // Test that the unlearn spell trap can unlearn spells

        const P pos_l(5, 7);
        const P pos_r(6, 7);

        test_utils::init_all();

        map::put(new terrain::Floor(pos_l));

        {
                auto* const unlearn_trap = new terrain::Trap(
                        pos_r,
                        new terrain::Floor(pos_r),
                        terrain::TrapId::unlearn_spell);

                map::put(unlearn_trap);

                unlearn_trap->reveal(terrain::PrintRevealMsg::no);
        }

        player_spells::learn_spell(SpellId::darkbolt, Verbose::no);
        player_spells::learn_spell(SpellId::heal, Verbose::no);

        REQUIRE(player_spells::is_spell_learned(SpellId::darkbolt));
        REQUIRE(player_spells::is_spell_learned(SpellId::heal));

        // Step into the trap
        map::g_player->m_pos = pos_l;
        game_time::g_allow_tick = true;
        actor::move(*map::g_player, Dir::right);

        REQUIRE(map::g_player->m_pos == pos_r);

        // Now only one spell should be learned
        const int nr_learned_after_first_trigger =
                (int)player_spells::is_spell_learned(SpellId::darkbolt) +
                (int)player_spells::is_spell_learned(SpellId::heal);

        REQUIRE(nr_learned_after_first_trigger == 1);

        // Step into the trap again
        map::g_player->m_pos = pos_l;
        game_time::g_allow_tick = true;
        actor::move(*map::g_player, Dir::right);

        REQUIRE(map::g_player->m_pos == pos_r);

        // Now both spells should be unlearned
        REQUIRE(!player_spells::is_spell_learned(SpellId::darkbolt));
        REQUIRE(!player_spells::is_spell_learned(SpellId::heal));

        test_utils::cleanup_all();
}

TEST_CASE("Do not unlearn frenzy")
{
        // Test that the unlearn spell trap will not unlearn Ghoul Frenzy

        const P pos_l(5, 7);
        const P pos_r(6, 7);

        test_utils::init_all();

        map::put(new terrain::Floor(pos_l));

        {
                auto* const unlearn_trap = new terrain::Trap(
                        pos_r,
                        new terrain::Floor(pos_r),
                        terrain::TrapId::unlearn_spell);

                map::put(unlearn_trap);

                unlearn_trap->reveal(terrain::PrintRevealMsg::no);
        }

        player_bon::pick_bg(Bg::ghoul);

        player_spells::learn_spell(SpellId::darkbolt, Verbose::no);

        REQUIRE(player_spells::is_spell_learned(SpellId::darkbolt));
        REQUIRE(player_spells::is_spell_learned(SpellId::frenzy));

        // Step into the trap
        map::g_player->m_pos = pos_l;
        game_time::g_allow_tick = true;
        actor::move(*map::g_player, Dir::right);

        REQUIRE(map::g_player->m_pos == pos_r);

        // Only frenzy should be learned now
        REQUIRE(!player_spells::is_spell_learned(SpellId::darkbolt));
        REQUIRE(player_spells::is_spell_learned(SpellId::frenzy));

        // Step into the trap again
        map::g_player->m_pos = pos_l;
        game_time::g_allow_tick = true;
        actor::move(*map::g_player, Dir::right);

        REQUIRE(map::g_player->m_pos == pos_r);

        // Still only frenzy should be learned
        REQUIRE(!player_spells::is_spell_learned(SpellId::darkbolt));
        REQUIRE(player_spells::is_spell_learned(SpellId::frenzy));

        test_utils::cleanup_all();
}

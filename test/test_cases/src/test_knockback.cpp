// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "catch.hpp"
#include "actor_factory.hpp"
#include "actor_player.hpp"
#include "knockback.hpp"
#include "map.hpp"
#include "pos.hpp"
#include "terrain.hpp"
#include "test_utils.hpp"
#include "actor.hpp"
#include "actor_data.hpp"
#include "global.hpp"
#include "property_data.hpp"
#include "property_handler.hpp"

TEST_CASE("Player cannot be knocked into deep liquid occupied by other actor")
{
        const P pos_l(5, 7);
        const P pos_r(6, 7);

        test_utils::init_all();

        map::put(new terrain::Floor(pos_l));
        map::put(new terrain::LiquidDeep(pos_r));

        auto* other_actor = actor::make(actor::Id::zombie, pos_r);

        map::g_player->m_pos = pos_l;

        knockback::run(
                *map::g_player,
                pos_l.with_x_offset(-1),
                false);  // Not spike gun

        // Target cell is occupied, nothing should happen
        REQUIRE(map::g_player->m_pos == pos_l);
        REQUIRE(map::g_player->m_state == ActorState::alive);

        // Kill the other actor, and knock the player again
        other_actor->m_state = ActorState::corpse;

        knockback::run(
                *map::g_player,
                pos_l.with_x_offset(-1),
                false);  // Not spike gun

        // Now the player should be knocked back, but not dead
        REQUIRE(map::g_player->m_pos == pos_r);
        REQUIRE(map::g_player->m_state == ActorState::alive);

        test_utils::cleanup_all();
}

TEST_CASE("Creatures are not nailed to occupied feature blocking los")
{
        const P pos_l(5, 7);
        const P pos_r(6, 7);

        test_utils::init_all();

        map::put(new terrain::Floor(pos_l));
        map::put(new terrain::Vines(pos_r));
        map::put(new terrain::Wall(pos_r.with_x_offset(1)));

        auto* other_actor = actor::make(actor::Id::zombie, pos_r);

        map::g_player->m_pos = pos_l;

        knockback::run(
                *map::g_player,
                pos_l.with_x_offset(-1),
                true);  // Is spike gun

        // Target cell is occupied, nothing should happen
        REQUIRE(map::g_player->m_pos == pos_l);
        REQUIRE(!map::g_player->m_properties.has(PropId::nailed));

        // Kill the other actor, and knock the player again
        other_actor->m_state = ActorState::corpse;

        knockback::run(
                *map::g_player,
                pos_l.with_x_offset(-1),
                true);  // Is spike gun

        // Now the player should be knocked back, but not nailed
        REQUIRE(map::g_player->m_pos == pos_r);
        REQUIRE(!map::g_player->m_properties.has(PropId::nailed));

        // Knock the player into the wall
        knockback::run(
                *map::g_player,
                pos_r.with_x_offset(-1),
                true);  // Is spike gun

        // Now the player should not be knocked back, but be nailed
        REQUIRE(map::g_player->m_pos == pos_r);
        REQUIRE(map::g_player->m_properties.has(PropId::nailed));

        test_utils::cleanup_all();
}

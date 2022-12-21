// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "actor.hpp"
#include "actor_data.hpp"
#include "actor_factory.hpp"
#include "catch.hpp"
#include "global.hpp"
#include "knockback.hpp"
#include "map.hpp"
#include "pos.hpp"
#include "property_data.hpp"
#include "property_handler.hpp"
#include "terrain.hpp"
#include "terrain_factory.hpp"
#include "test_utils.hpp"

TEST_CASE("Creatures are not nailed to occupied terrain blocking los")
{
        const P pos_l(5, 7);
        const P pos_r(6, 7);

        test_utils::init_all();

        map::update_terrain(
                terrain::make(terrain::Id::floor, pos_l));

        map::update_terrain(
                terrain::make(terrain::Id::vines, pos_r));

        map::update_terrain(
                terrain::make(terrain::Id::wall, pos_r.with_x_offset(1)));

        actor::Actor* other_actor = actor::make("MON_ZOMBIE", pos_r);

        map::g_player->m_pos = pos_l;

        knockback::run(
                *map::g_player,
                pos_l.with_x_offset(-1),
                knockback::KnockbackSource::spike_gun);

        // Target cell is occupied, nothing should happen
        REQUIRE(map::g_player->m_pos == pos_l);
        REQUIRE(!map::g_player->m_properties.has(PropId::nailed));

        // Kill the other actor, and knock the player again
        other_actor->m_state = ActorState::corpse;

        knockback::run(
                *map::g_player,
                pos_l.with_x_offset(-1),
                knockback::KnockbackSource::spike_gun);

        // Now the player should be knocked back, but not nailed
        REQUIRE(map::g_player->m_pos == pos_r);
        REQUIRE(!map::g_player->m_properties.has(PropId::nailed));

        // Knock the player into the wall
        knockback::run(
                *map::g_player,
                pos_r.with_x_offset(-1),
                knockback::KnockbackSource::spike_gun);

        // Now the player should not be knocked back, but be nailed
        REQUIRE(map::g_player->m_pos == pos_r);
        REQUIRE(map::g_player->m_properties.has(PropId::nailed));

        test_utils::cleanup_all();
}

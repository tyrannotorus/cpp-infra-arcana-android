// =============================================================================
// Copyright 2011-2021 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "actor.hpp"
#include "actor_data.hpp"
#include "actor_factory.hpp"
#include "actor_player.hpp"
#include "catch.hpp"
#include "global.hpp"
#include "knockback.hpp"
#include "map.hpp"
#include "pos.hpp"
#include "property_data.hpp"
#include "property_handler.hpp"
#include "terrain.hpp"
#include "test_utils.hpp"

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

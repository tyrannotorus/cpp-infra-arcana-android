// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "actor_hit.hpp"
#include "actor_player.hpp"
#include "catch.hpp"
#include "map.hpp"
#include "player_bon.hpp"
#include "test_utils.hpp"

TEST_CASE("Hit player with Prolonged Life trait")
{
        test_utils::init_all();

        player_bon::pick_trait(Trait::prolonged_life);

        map::g_player->m_inv.drop_all_non_intrinsic(map::g_player->m_pos);

        map::g_player->m_hp = 8;
        map::g_player->m_base_max_hp = 8;
        map::g_player->m_sp = 4;
        map::g_player->m_base_max_sp = 4;

        // Hit player so that 1 HP remains, should not affect SP.
        actor::hit(*map::g_player, 7, DmgType::blunt, AllowWound::no);

        REQUIRE(map::g_player->m_hp == 1);
        REQUIRE(map::g_player->m_sp == 4);
        REQUIRE(map::g_player->is_alive());

        // Hit player with 1 damage, should decrease SP by one.
        actor::hit(*map::g_player, 1, DmgType::blunt, AllowWound::no);

        REQUIRE(map::g_player->m_hp == 1);
        REQUIRE(map::g_player->m_sp == 3);
        REQUIRE(map::g_player->is_alive());

        // Hit player for 2 more damage to reduce player to 1 SP.
        actor::hit(*map::g_player, 2, DmgType::blunt, AllowWound::no);

        REQUIRE(map::g_player->m_hp == 1);
        REQUIRE(map::g_player->m_sp == 1);
        REQUIRE(map::g_player->is_alive());

        // Hit player for 1 damage - no more SP left to soak up damage so HP
        // should be decreased again.
        actor::hit(*map::g_player, 1, DmgType::blunt, AllowWound::no);

        REQUIRE(map::g_player->m_hp == 0);
        REQUIRE(map::g_player->m_sp == 1);
        REQUIRE(!map::g_player->is_alive());

        test_utils::cleanup_all();
}

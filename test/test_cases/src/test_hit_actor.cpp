// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "actor.hpp"
#include "actor_hit.hpp"
#include "actor_player_state.hpp"
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
        actor::player_state::g_exorcist_fervor = 4;

        // Hit player so that 1 HP remains, should not affect fervor.
        actor::hit(*map::g_player, 7, DmgType::blunt, nullptr, AllowWound::no);

        REQUIRE(map::g_player->m_hp == 1);
        REQUIRE(actor::player_state::g_exorcist_fervor == 4);
        REQUIRE(actor::is_alive(*map::g_player));

        // Hit player with 1 damage, should decrease fervor by one.
        actor::hit(*map::g_player, 1, DmgType::blunt, nullptr, AllowWound::no);

        REQUIRE(map::g_player->m_hp == 1);
        REQUIRE(actor::player_state::g_exorcist_fervor == 3);
        REQUIRE(actor::is_alive(*map::g_player));

        // Hit player for 2 more damage to reduce player to 1 fervor.
        actor::hit(*map::g_player, 2, DmgType::blunt, nullptr, AllowWound::no);

        REQUIRE(map::g_player->m_hp == 1);
        REQUIRE(actor::player_state::g_exorcist_fervor == 1);
        REQUIRE(actor::is_alive(*map::g_player));

        // Hit player for 1 more damage to reduce player to 0 fervor.
        actor::hit(*map::g_player, 1, DmgType::blunt, nullptr, AllowWound::no);

        REQUIRE(map::g_player->m_hp == 1);
        REQUIRE(actor::player_state::g_exorcist_fervor == 0);
        REQUIRE(actor::is_alive(*map::g_player));

        // Hit player for 1 damage - no more fervor left to soak up damage so HP
        // should be decreased to zero.
        actor::hit(*map::g_player, 1, DmgType::blunt, nullptr, AllowWound::no);

        REQUIRE(map::g_player->m_hp == 0);
        REQUIRE(actor::player_state::g_exorcist_fervor == 0);
        REQUIRE(!actor::is_alive(*map::g_player));

        test_utils::cleanup_all();
}

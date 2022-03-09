// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include <cstddef>

#include "ability_values.hpp"
#include "actor.hpp"
#include "actor_data.hpp"
#include "actor_factory.hpp"
#include "actor_player.hpp"
#include "attack_data.hpp"
#include "catch.hpp"
#include "global.hpp"
#include "item.hpp"
#include "item_data.hpp"
#include "item_factory.hpp"
#include "map.hpp"
#include "player_bon.hpp"
#include "pos.hpp"
#include "property_data.hpp"
#include "property_factory.hpp"
#include "property_handler.hpp"
#include "terrain.hpp"
#include "test_utils.hpp"
#include "wpn_dmg.hpp"

TEST_CASE("Melee attack data")
{
        test_utils::init_all();

        player_bon::pick_bg(Bg::war_vet);

        const P p1(20, 10);
        const P p2(21, 10);
        const P p3(20, 11);

        map::put(new terrain::Floor(p1));
        map::put(new terrain::Floor(p2));
        map::put(new terrain::Floor(p3));

        map::g_player->m_pos = p1;

        // Zombie
        auto& mon_1 = *actor::make(actor::Id::zombie, p2);

        // Zombie with invisible property applied
        auto& mon_2 = *actor::make(actor::Id::zombie, p3);

        mon_2.m_properties.apply(property_factory::make(PropId::invis));

        map::g_player->update_fov();

        mon_1.m_mon_aware_state.aware_counter = 1;
        mon_1.m_mon_aware_state.player_aware_of_me_counter = 1;
        mon_2.m_mon_aware_state.aware_counter = 1;
        mon_2.m_mon_aware_state.player_aware_of_me_counter = 1;

        auto& wpn = static_cast<item::Wpn&>(*item::make(item::Id::axe));

        wpn.set_melee_plus(2);

        int expected_hit_chance_vs_mon_1 = 0;
        int expected_hit_chance_vs_mon_2 = 0;

        {
                const auto& player_data =
                        actor::g_data[(size_t)actor::Id::player];

                const auto& mon_data =
                        actor::g_data[(size_t)actor::Id::zombie];

                const int player_skill_mod =
                        player_data.ability_values.val(
                                AbilityId::melee,
                                true,  // Affected by properties
                                *map::g_player);

                const int mon_dodge_mod =
                        -(mon_data.ability_values.val(
                                AbilityId::dodging,
                                true,  // Affected by properties
                                mon_1));

                const int wpn_hit_mod = wpn.data().melee.hit_chance_mod;

                expected_hit_chance_vs_mon_1 =
                        player_skill_mod +
                        mon_dodge_mod +
                        wpn_hit_mod;

                expected_hit_chance_vs_mon_2 =
                        expected_hit_chance_vs_mon_1 -
                        g_hit_chance_pen_vs_unseen;
        }

        auto expected_dmg_range = wpn.data().melee.dmg;

        // +1 from melee trait and +2 from weapon
        expected_dmg_range.set_plus(3);

        const MeleeAttData att_data_1(map::g_player, mon_1, wpn);
        const MeleeAttData att_data_2(map::g_player, mon_2, wpn);

        REQUIRE(att_data_1.hit_chance_tot == expected_hit_chance_vs_mon_1);
        REQUIRE(att_data_1.dmg_range == expected_dmg_range);

        REQUIRE(att_data_2.hit_chance_tot == expected_hit_chance_vs_mon_2);
        REQUIRE(att_data_2.dmg_range == expected_dmg_range);

        test_utils::cleanup_all();
}

TEST_CASE("Melee attack data has reduced damage with weakened player")
{
        test_utils::init_all();

        player_bon::pick_bg(Bg::war_vet);

        const P p1(20, 10);
        const P p2(21, 10);

        map::put(new terrain::Floor(p1));
        map::put(new terrain::Floor(p2));

        map::g_player->m_pos = p1;

        map::g_player->m_properties.apply(
                property_factory::make(PropId::weakened));

        // Zombie
        auto& mon = *actor::make(actor::Id::zombie, p2);

        map::g_player->update_fov();

        mon.m_mon_aware_state.aware_counter = 1;
        mon.m_mon_aware_state.player_aware_of_me_counter = 1;

        auto& wpn = static_cast<item::Wpn&>(*item::make(item::Id::axe));

        wpn.set_base_melee_dmg({20, 60});
        wpn.set_melee_plus(2);

        // Halved damage range due to Weakened property
        // Plus before weakening is +1 from melee trait and +2 from weapon
        const auto expected_dmg_range = WpnDmg(10, 30, 1);

        const MeleeAttData att_data(map::g_player, mon, wpn);

        REQUIRE(att_data.dmg_range == expected_dmg_range);

        test_utils::cleanup_all();
}

TEST_CASE("Melee attack data has reduced damage against pierce resistance")
{
        test_utils::init_all();

        player_bon::pick_bg(Bg::war_vet);

        const P p1(20, 10);
        const P p2(21, 10);

        map::put(new terrain::Floor(p1));
        map::put(new terrain::Floor(p2));

        map::g_player->m_pos = p1;

        // Worm Mass
        auto& mon = *actor::make(actor::Id::worm_mass, p2);

        map::g_player->update_fov();

        mon.m_mon_aware_state.aware_counter = 1;
        mon.m_mon_aware_state.player_aware_of_me_counter = 1;

        // Use pointy weapon
        auto& wpn = static_cast<item::Wpn&>(*item::make(item::Id::dagger));

        wpn.set_base_melee_dmg({20, 60});
        wpn.set_melee_plus(8);

        // Halved damage range due to Weakened property
        // Plus before weakening is +1 from melee trait and +8 from weapon
        const auto expected_dmg_range = WpnDmg(5, 15, 2);

        const MeleeAttData att_data(map::g_player, mon, wpn);

        REQUIRE(att_data.dmg_range == expected_dmg_range);

        test_utils::cleanup_all();
}

TEST_CASE("Ranged attack data")
{
        test_utils::init_all();

        player_bon::pick_bg(Bg::war_vet);

        const P p1(20, 10);
        const P p2(22, 11);  // Distance 2
        const P p3(21, 13);  // Distance 3

        for (int x = 1; x < map::w() - 1; ++x)
        {
                for (int y = 1; y < map::h() - 1; ++y)
                {
                        map::put(new terrain::Floor({x, y}));
                }
        }

        map::g_player->m_pos = p1;

        // Zombie
        auto& mon_1 = *actor::make(actor::Id::zombie, p2);

        // Zombie with invisible property applied
        auto& mon_2 = *actor::make(actor::Id::zombie, p3);

        mon_2.m_properties.apply(property_factory::make(PropId::invis));

        map::g_player->update_fov();

        mon_1.m_mon_aware_state.aware_counter = 1;
        mon_1.m_mon_aware_state.player_aware_of_me_counter = 1;
        mon_2.m_mon_aware_state.aware_counter = 1;
        mon_2.m_mon_aware_state.player_aware_of_me_counter = 1;

        auto& wpn = static_cast<item::Wpn&>(*item::make(item::Id::pistol));

        int expected_hit_chance_vs_mon_1;
        int expected_hit_chance_vs_mon_2;

        {
                const auto& player_data =
                        actor::g_data[(size_t)actor::Id::player];

                const auto& mon_data =
                        actor::g_data[(size_t)actor::Id::zombie];

                const int player_skill_mod =
                        player_data.ability_values.val(
                                AbilityId::ranged,
                                true,  // Affected by properties
                                *map::g_player);

                const int mon_dodge_mod =
                        -(mon_data.ability_values.val(
                                AbilityId::dodging,
                                true,  // Affected by properties
                                mon_1));

                const int wpn_hit_mod = wpn.data().ranged.hit_chance_mod;

                // Using the same calculation as in the ranged attack data
                constexpr int dist_mod_mon_1 = 15 - (5 * 2);
                constexpr int dist_mod_mon_2 = 15 - (5 * 3);

                const int common_hit_chance =
                        player_skill_mod +
                        mon_dodge_mod +
                        wpn_hit_mod;

                expected_hit_chance_vs_mon_1 =
                        common_hit_chance +
                        dist_mod_mon_1;

                expected_hit_chance_vs_mon_2 =
                        common_hit_chance +
                        dist_mod_mon_2 -
                        g_hit_chance_pen_vs_unseen;
        }

        auto expected_dmg_range = wpn.data().ranged.dmg;

        const RangedAttData att_data_1(
                map::g_player,  // Attacker
                map::g_player->m_pos,  // Attacker origin
                {},  // Aim position, doesn't matter here
                mon_1.m_pos,  // Current position
                wpn);  // Weapon

        const RangedAttData att_data_2(
                map::g_player,  // Attacker
                map::g_player->m_pos,  // Attacker origin
                {},  // Aim position, doesn't matter here
                mon_2.m_pos,  // Current position
                wpn);  // Weapon

        REQUIRE(att_data_1.hit_chance_tot == expected_hit_chance_vs_mon_1);
        REQUIRE(att_data_1.dmg_range == expected_dmg_range);

        REQUIRE(att_data_2.hit_chance_tot == expected_hit_chance_vs_mon_2);
        REQUIRE(att_data_2.dmg_range == expected_dmg_range);

        test_utils::cleanup_all();
}

TEST_CASE("Throwing attack data")
{
        test_utils::init_all();

        player_bon::pick_bg(Bg::war_vet);

        const P p1(20, 10);
        const P p2(22, 11);  // Distance 2
        const P p3(21, 13);  // Distance 3

        for (int x = 1; x < map::w() - 1; ++x)
        {
                for (int y = 1; y < map::h() - 1; ++y)
                {
                        map::put(new terrain::Floor({x, y}));
                }
        }

        map::g_player->m_pos = p1;

        // Zombie
        auto& mon_1 = *actor::make(actor::Id::zombie, p2);

        // Zombie with invisible property applied
        auto& mon_2 = *actor::make(actor::Id::zombie, p3);

        mon_2.m_properties.apply(property_factory::make(PropId::invis));

        map::g_player->update_fov();

        mon_1.m_mon_aware_state.aware_counter = 1;
        mon_1.m_mon_aware_state.player_aware_of_me_counter = 1;
        mon_2.m_mon_aware_state.aware_counter = 1;
        mon_2.m_mon_aware_state.player_aware_of_me_counter = 1;

        auto& item = *item::make(item::Id::thr_knife);

        int expected_hit_chance_vs_mon_1;
        int expected_hit_chance_vs_mon_2;

        {
                const auto& player_data =
                        actor::g_data[(size_t)actor::Id::player];

                const auto& mon_data =
                        actor::g_data[(size_t)actor::Id::zombie];

                const int player_skill_mod =
                        player_data.ability_values.val(
                                AbilityId::ranged,
                                true,  // Affected by properties
                                *map::g_player);

                const int mon_dodge_mod =
                        -(mon_data.ability_values.val(
                                AbilityId::dodging,
                                true,  // Affected by properties
                                mon_1));

                const int wpn_hit_mod = item.data().ranged.throw_hit_chance_mod;

                // Using the same calculation as in the ranged attack data
                constexpr int dist_mod_mon_1 = 15 - (5 * 2);
                constexpr int dist_mod_mon_2 = 15 - (5 * 3);

                const int common_hit_chance =
                        player_skill_mod +
                        mon_dodge_mod +
                        wpn_hit_mod;

                expected_hit_chance_vs_mon_1 =
                        common_hit_chance +
                        dist_mod_mon_1;

                expected_hit_chance_vs_mon_2 =
                        common_hit_chance +
                        dist_mod_mon_2 -
                        g_hit_chance_pen_vs_unseen;
        }

        auto expected_dmg_range = item.data().ranged.dmg;

        const ThrowAttData att_data_1(
                map::g_player,  // Attacker
                map::g_player->m_pos,  // Attacker origin
                {},  // Aim position, doesn't matter here
                mon_1.m_pos,  // Current position
                item);  // Thrown item

        const ThrowAttData att_data_2(
                map::g_player,  // Attacker
                map::g_player->m_pos,  // Attacker origin
                {},  // Aim position, doesn't matter here
                mon_2.m_pos,  // Current position
                item);  // Thrown item

        REQUIRE(att_data_1.hit_chance_tot == expected_hit_chance_vs_mon_1);
        REQUIRE(att_data_1.dmg_range == expected_dmg_range);

        REQUIRE(att_data_2.hit_chance_tot == expected_hit_chance_vs_mon_2);
        REQUIRE(att_data_2.dmg_range == expected_dmg_range);

        test_utils::cleanup_all();
}

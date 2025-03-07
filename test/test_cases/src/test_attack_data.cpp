// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include <cstddef>

#include "ability_values.hpp"
#include "actor.hpp"
#include "actor_data.hpp"
#include "actor_factory.hpp"
#include "attack_data.hpp"
#include "catch.hpp"
#include "global.hpp"
#include "item.hpp"
#include "item_data.hpp"
#include "item_factory.hpp"
#include "map.hpp"
#include "misc.hpp"
#include "player_bon.hpp"
#include "pos.hpp"
#include "property_data.hpp"
#include "property_factory.hpp"
#include "property_handler.hpp"
#include "terrain.hpp"
#include "terrain_factory.hpp"
#include "test_utils.hpp"
#include "wpn_dmg.hpp"

TEST_CASE("Melee attack data")
{
        test_utils::init_all();

        player_bon::pick_bg(Bg::war_vet);

        const P p1(20, 10);
        const P p2(21, 10);
        const P p3(20, 11);

        map::update_terrain(terrain::make(terrain::Id::floor, p1));
        map::update_terrain(terrain::make(terrain::Id::floor, p2));
        map::update_terrain(terrain::make(terrain::Id::floor, p3));

        map::g_player->m_pos = p1;

        // Zombie
        actor::Actor& mon_1 = *actor::make("MON_ZOMBIE", p2);

        // Zombie with invisible property applied
        actor::Actor& mon_2 = *actor::make("MON_ZOMBIE", p3);

        mon_2.m_properties.apply(prop::make(prop::Id::invis));

        actor::update_player_fov();

        mon_1.m_mon_aware_state.aware_counter = 1;
        mon_1.m_mon_aware_state.player_aware_of_me_counter = 1;
        mon_2.m_mon_aware_state.aware_counter = 1;
        mon_2.m_mon_aware_state.player_aware_of_me_counter = 1;

        auto& wpn = static_cast<item::Wpn&>(*item::make(item::Id::axe));

        // 10-10 +2 (min and max is the same, for determinism).
        wpn.set_base_melee_dmg({10, 10, 2});

        int expected_hit_chance_vs_mon_1 = 0;
        int expected_hit_chance_vs_mon_2 = 0;

        {
                const actor::ActorData& player_data = actor::g_data["MON_PLAYER"];
                const actor::ActorData& mon_data = actor::g_data["MON_ZOMBIE"];

                const int player_skill_mod =
                        player_data.ability_values.val(
                                AbilityId::melee,
                                AbilityAffectedByProperties::yes,
                                *map::g_player);

                const int mon_dodge_mod =
                        -(mon_data.ability_values.val(
                                AbilityId::dodging,
                                AbilityAffectedByProperties::yes,
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

        // 10-10 +2, and +1 from melee trait.
        const int expected_dmg = 13;

        const MeleeAttData att_data_1(map::g_player, mon_1, wpn);
        const MeleeAttData att_data_2(map::g_player, mon_2, wpn);

        REQUIRE(att_data_1.hit_chance_tot == expected_hit_chance_vs_mon_1);
        REQUIRE(att_data_1.dmg == expected_dmg);

        REQUIRE(att_data_2.hit_chance_tot == expected_hit_chance_vs_mon_2);
        REQUIRE(att_data_2.dmg == expected_dmg);

        test_utils::cleanup_all();
}

TEST_CASE("Melee attack data has reduced damage with weakened player")
{
        test_utils::init_all();

        player_bon::pick_bg(Bg::war_vet);

        const P p1(20, 10);
        const P p2(21, 10);

        map::update_terrain(terrain::make(terrain::Id::floor, p1));
        map::update_terrain(terrain::make(terrain::Id::floor, p2));

        map::g_player->m_pos = p1;

        map::g_player->m_properties.apply(prop::make(prop::Id::weakened));

        // Zombie
        actor::Actor& mon = *actor::make("MON_ZOMBIE", p2);

        actor::update_player_fov();

        mon.m_mon_aware_state.aware_counter = 1;
        mon.m_mon_aware_state.player_aware_of_me_counter = 1;

        auto& wpn = static_cast<item::Wpn&>(*item::make(item::Id::axe));

        // 10-10 +2 (min and max is the same, for determinism).
        wpn.set_base_melee_dmg({10, 10, 2});

        // 10-10 +2, and +1 from melee trait - then halved for weakness.
        const int expected_dmg = 6;

        const MeleeAttData att_data(map::g_player, mon, wpn);

        REQUIRE(att_data.dmg == expected_dmg);

        test_utils::cleanup_all();
}

TEST_CASE("Melee attack data has reduced damage with weakened and poisoned player")
{
        test_utils::init_all();

        player_bon::pick_bg(Bg::war_vet);

        const P p1(20, 10);
        const P p2(21, 10);

        map::update_terrain(terrain::make(terrain::Id::floor, p1));
        map::update_terrain(terrain::make(terrain::Id::floor, p2));

        map::g_player->m_pos = p1;

        map::g_player->m_properties.apply(prop::make(prop::Id::weakened));
        map::g_player->m_properties.apply(prop::make(prop::Id::poisoned));

        // Zombie
        actor::Actor& mon = *actor::make("MON_ZOMBIE", p2);

        actor::update_player_fov();

        mon.m_mon_aware_state.aware_counter = 1;
        mon.m_mon_aware_state.player_aware_of_me_counter = 1;

        auto& wpn = static_cast<item::Wpn&>(*item::make(item::Id::axe));

        // 10-10 +2 (min and max is the same, for determinism).
        wpn.set_base_melee_dmg({10, 10, 2});

        // 10-10 +2, and +1 from melee trait - then reduced to 25% for weakness + poison.
        const int expected_dmg = 3;

        const MeleeAttData att_data(map::g_player, mon, wpn);

        REQUIRE(att_data.dmg == expected_dmg);

        test_utils::cleanup_all();
}

TEST_CASE("Melee attack data has reduced damage against pierce resistance")
{
        test_utils::init_all();

        player_bon::pick_bg(Bg::war_vet);

        const P p1(20, 10);
        const P p2(21, 10);

        map::update_terrain(terrain::make(terrain::Id::floor, p1));
        map::update_terrain(terrain::make(terrain::Id::floor, p2));

        map::g_player->m_pos = p1;

        // Worm Mass
        actor::Actor& mon = *actor::make("MON_WORM_MASS", p2);

        actor::update_player_fov();

        mon.m_mon_aware_state.aware_counter = 1;
        mon.m_mon_aware_state.player_aware_of_me_counter = 1;

        // Use pointy weapon
        auto& wpn = static_cast<item::Wpn&>(*item::make(item::Id::dagger));

        // 10-10 +2 (min and max is the same, for determinism).
        wpn.set_base_melee_dmg({10, 10, 2});

        // 10-10+2, +1 from melee trait - then halved for pierce resistance.
        const int expected_dmg = 3;

        const MeleeAttData att_data(map::g_player, mon, wpn);

        REQUIRE(att_data.dmg == expected_dmg);

        test_utils::cleanup_all();
}

TEST_CASE("Ranged attack data")
{
        test_utils::init_all();

        player_bon::pick_bg(Bg::war_vet);

        for (int x = 1; x < map::w() - 1; ++x) {
                for (int y = 1; y < map::h() - 1; ++y) {
                        map::update_terrain(
                                terrain::make(terrain::Id::floor, {x, y}));
                }
        }

        auto& wpn = static_cast<item::Wpn&>(*item::make(item::Id::pistol));

        const int effective_range_max = wpn.data().ranged.effective_range.max;

        const P p1(20, 10);
        const P p2(p1.x + effective_range_max, 10);      // Within effective range
        const P p3(p1.x + effective_range_max + 1, 10);  // Outside effective range

        map::g_player->m_pos = p1;

        // Zombie
        actor::Actor& mon_1 = *actor::make("MON_ZOMBIE", p2);

        // Zombie with invisible property applied
        actor::Actor& mon_2 = *actor::make("MON_ZOMBIE", p3);

        mon_2.m_properties.apply(prop::make(prop::Id::invis));

        actor::update_player_fov();

        mon_1.m_mon_aware_state.aware_counter = 1;
        mon_1.m_mon_aware_state.player_aware_of_me_counter = 1;
        mon_2.m_mon_aware_state.aware_counter = 1;
        mon_2.m_mon_aware_state.player_aware_of_me_counter = 1;

        // 10-10 +2 (min and max is the same, for determinism).
        wpn.set_base_ranged_dmg({10, 10, 2});

        int expected_hit_chance_vs_mon_1 = 0;
        int expected_hit_chance_vs_mon_2 = 0;

        {
                const actor::ActorData& player_data = actor::g_data["MON_PLAYER"];
                const actor::ActorData& mon_data = actor::g_data["MON_ZOMBIE"];

                const int player_skill_mod =
                        player_data.ability_values.val(
                                AbilityId::ranged,
                                AbilityAffectedByProperties::yes,
                                *map::g_player);

                const int mon_dodge_mod =
                        -(mon_data.ability_values.val(
                                AbilityId::dodging,
                                AbilityAffectedByProperties::yes,
                                mon_1));

                const int wpn_hit_mod = wpn.data().ranged.hit_chance_mod;

                // Using the same calculation as in the ranged attack data
                const int dist_1 = king_dist(p1, p2);
                const int dist_2 = king_dist(p1, p3);
                const int dist_mod_mon_1 = 15 - (5 * dist_1);
                const int dist_mod_mon_2 = 15 - (5 * dist_2);

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

        // Monster 1 is within effective range: 10-10 +2
        const int expected_dmg_1 = 12;

        // Monster 2 is outside effective range: Halved damage.
        const int expected_dmg_2 = 6;

        const RangedAttData att_data_1(
                map::g_player,         // Attacker
                map::g_player->m_pos,  // Attacker origin
                {},                    // Aim position, doesn't matter here
                mon_1.m_pos,           // Current position
                wpn);                  // Weapon

        const RangedAttData att_data_2(
                map::g_player,         // Attacker
                map::g_player->m_pos,  // Attacker origin
                {},                    // Aim position, doesn't matter here
                mon_2.m_pos,           // Current position
                wpn);                  // Weapon

        REQUIRE(att_data_1.hit_chance_tot == expected_hit_chance_vs_mon_1);
        REQUIRE(att_data_1.dmg == expected_dmg_1);

        REQUIRE(att_data_2.hit_chance_tot == expected_hit_chance_vs_mon_2);
        REQUIRE(att_data_2.dmg == expected_dmg_2);

        test_utils::cleanup_all();
}

TEST_CASE("Throwing attack data using throwing weapon")
{
        test_utils::init_all();

        player_bon::pick_bg(Bg::war_vet);

        for (int x = 1; x < map::w() - 1; ++x) {
                for (int y = 1; y < map::h() - 1; ++y) {
                        map::update_terrain(
                                terrain::make(terrain::Id::floor, {x, y}));
                }
        }

        item::Item& item = *item::make(item::Id::thr_knife);

        const int effective_range_max = item.data().ranged.effective_range.max;

        const P p1(20, 10);
        const P p2(p1.x + effective_range_max, 10);      // Within effective range
        const P p3(p1.x + effective_range_max + 1, 10);  // Outside effective range

        map::g_player->m_pos = p1;

        // Zombie
        actor::Actor& mon_1 = *actor::make("MON_ZOMBIE", p2);

        // Zombie with invisible property applied
        actor::Actor& mon_2 = *actor::make("MON_ZOMBIE", p3);

        mon_2.m_properties.apply(prop::make(prop::Id::invis));

        actor::update_player_fov();

        mon_1.m_mon_aware_state.aware_counter = 1;
        mon_1.m_mon_aware_state.player_aware_of_me_counter = 1;
        mon_2.m_mon_aware_state.aware_counter = 1;
        mon_2.m_mon_aware_state.player_aware_of_me_counter = 1;

        // Setup melee and ranged damage of the item - the ranged damage should be used for a weapon
        // that is primarily a throwing weapon.

        // 50-50 +15 (min and max is the same, for determinism).
        item.set_base_melee_dmg({50, 50, 15});

        // 10-10 +2 (min and max is the same, for determinism).
        item.set_base_ranged_dmg({10, 10, 2});

        int expected_hit_chance_vs_mon_1 = 0;
        int expected_hit_chance_vs_mon_2 = 0;

        {
                const actor::ActorData& player_data = actor::g_data["MON_PLAYER"];

                const actor::ActorData& mon_data = actor::g_data["MON_ZOMBIE"];

                const int player_skill_mod =
                        player_data.ability_values.val(
                                AbilityId::ranged,
                                AbilityAffectedByProperties::yes,
                                *map::g_player);

                const int mon_dodge_mod =
                        -(mon_data.ability_values.val(
                                AbilityId::dodging,
                                AbilityAffectedByProperties::yes,
                                mon_1));

                const int wpn_hit_mod = item.data().ranged.throw_hit_chance_mod;

                // Using the same calculation as in the ranged attack data
                const int dist_1 = king_dist(p1, p2);
                const int dist_2 = king_dist(p1, p3);
                const int dist_mod_mon_1 = 15 - (5 * dist_1);
                const int dist_mod_mon_2 = 15 - (5 * dist_2);

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

        // Monster 1 is within effective range: 10-10 +2
        const int expected_dmg_1 = 12;

        // Monster 2 is outside effective range: Halved damage.
        const int expected_dmg_2 = 6;

        const ThrowAttData att_data_1(
                map::g_player,         // Attacker
                map::g_player->m_pos,  // Attacker origin
                {},                    // Aim position, doesn't matter here
                mon_1.m_pos,           // Current position
                item);                 // Thrown item

        const ThrowAttData att_data_2(
                map::g_player,         // Attacker
                map::g_player->m_pos,  // Attacker origin
                {},                    // Aim position, doesn't matter here
                mon_2.m_pos,           // Current position
                item);                 // Thrown item

        REQUIRE(att_data_1.hit_chance_tot == expected_hit_chance_vs_mon_1);
        REQUIRE(att_data_1.dmg == expected_dmg_1);

        REQUIRE(att_data_2.hit_chance_tot == expected_hit_chance_vs_mon_2);
        REQUIRE(att_data_2.dmg == expected_dmg_2);

        test_utils::cleanup_all();
}

TEST_CASE("Throwing attack data using melee weapon")
{
        test_utils::init_all();

        player_bon::pick_bg(Bg::war_vet);

        for (int x = 1; x < map::w() - 1; ++x) {
                for (int y = 1; y < map::h() - 1; ++y) {
                        map::update_terrain(
                                terrain::make(terrain::Id::floor, {x, y}));
                }
        }

        item::Item& item = *item::make(item::Id::axe);

        const P p1(20, 10);
        const P p2(20, 12);

        map::g_player->m_pos = p1;

        // Zombie
        actor::Actor& mon = *actor::make("MON_ZOMBIE", p2);

        actor::update_player_fov();

        mon.m_mon_aware_state.aware_counter = 1;
        mon.m_mon_aware_state.player_aware_of_me_counter = 1;

        // Setup melee and ranged damage of the item - the melee damage should be used for a weapon
        // that is primarily a melee weapon.

        // 50-50 +15 (min and max is the same, for determinism).
        item.set_base_melee_dmg({50, 50, 15});

        // 10-10 +2 (min and max is the same, for determinism).
        item.set_base_ranged_dmg({10, 10, 2});

        int expected_hit_chance_vs_mon = 0;

        {
                const actor::ActorData& player_data = actor::g_data["MON_PLAYER"];

                const actor::ActorData& mon_data = actor::g_data["MON_ZOMBIE"];

                const int player_skill_mod =
                        player_data.ability_values.val(
                                AbilityId::ranged,
                                AbilityAffectedByProperties::yes,
                                *map::g_player);

                const int mon_dodge_mod =
                        -(mon_data.ability_values.val(
                                AbilityId::dodging,
                                AbilityAffectedByProperties::yes,
                                mon));

                const int wpn_hit_mod = item.data().ranged.throw_hit_chance_mod;

                // Using the same calculation as in the ranged attack data
                const int dist = king_dist(p1, p2);
                const int dist_mod_mon = 15 - (5 * dist);

                const int common_hit_chance =
                        player_skill_mod +
                        mon_dodge_mod +
                        wpn_hit_mod;

                expected_hit_chance_vs_mon =
                        common_hit_chance +
                        dist_mod_mon;
        }

        // Melee damage is 50-50 +15, this should be used (not the ranged damage).
        const int expected_dmg = 65;

        const ThrowAttData att_data(
                map::g_player,         // Attacker
                map::g_player->m_pos,  // Attacker origin
                {},                    // Aim position, doesn't matter here
                mon.m_pos,             // Current position
                item);                 // Thrown item

        REQUIRE(att_data.hit_chance_tot == expected_hit_chance_vs_mon);
        REQUIRE(att_data.dmg == expected_dmg);

        test_utils::cleanup_all();
}

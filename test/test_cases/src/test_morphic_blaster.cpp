// =============================================================================
// Copyright 2011-2024 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "actor.hpp"
#include "actor_data.hpp"
#include "actor_factory.hpp"
#include "attack.hpp"
#include "catch.hpp"
#include "item.hpp"
#include "item_data.hpp"
#include "item_factory.hpp"
#include "item_weapon.hpp"
#include "map.hpp"
#include "pos.hpp"
#include "terrain.hpp"
#include "terrain_factory.hpp"
#include "test_utils.hpp"

TEST_CASE("Morphic Blaster projectile explodes on hitting creature")
{
        test_utils::init_all();

        // Ensure that the player has enough HP to fire it.
        map::g_player->m_hp = 1000;

        for (int x = 1; x < (map::w() - 1); ++x) {
                for (int y = 1; y < (map::h() - 1); ++y) {
                        map::update_terrain(
                                terrain::make(terrain::Id::floor, {x, y}));
                }
        }

        const P p0(20, 20);  // Player position
        const P p1(25, 20);  // Rat 1 position, aim position
        const P p2(25, 21);  // Rat 2 position

        map::g_player->m_pos = p0;

        const actor::Actor* const rat_1 = actor::make("MON_RAT", p1);
        const actor::Actor* const rat_2 = actor::make("MON_RAT", p2);

        auto* const wpn =
                static_cast<item::Wpn*>(
                        item::make(item::Id::morphic_blaster));

        REQUIRE(actor::is_alive(*rat_1));
        REQUIRE(actor::is_alive(*rat_2));

        attack::ranged(map::g_player, map::g_player->m_pos, rat_1->m_pos, *wpn);

        REQUIRE(!actor::is_alive(*rat_1));
        REQUIRE(!actor::is_alive(*rat_2));

        test_utils::cleanup_all();
}

TEST_CASE("Morphic Blaster projectile explodes on hitting floor")
{
        test_utils::init_all();

        // Ensure that the player has enough HP to fire it.
        map::g_player->m_hp = 1000;

        for (int x = 1; x < (map::w() - 1); ++x) {
                for (int y = 1; y < (map::h() - 1); ++y) {
                        map::update_terrain(
                                terrain::make(terrain::Id::floor, {x, y}));
                }
        }

        // Shooting at a floor position should create an explosion centered at
        // that position, which should always kill small creatures at the edge
        // of the explosion.

        const P p0(20, 20);  // Player position
        const P p1(24, 20);  // Rat 1 position
        const P p2(25, 20);  // Aim position
        const P p3(26, 20);  // Rat 2 position

        map::g_player->m_pos = p0;

        const actor::Actor* const rat_1 = actor::make("MON_RAT", p1);
        const actor::Actor* const rat_2 = actor::make("MON_RAT", p3);

        auto* const wpn =
                static_cast<item::Wpn*>(
                        item::make(item::Id::morphic_blaster));

        REQUIRE(actor::is_alive(*rat_1));
        REQUIRE(actor::is_alive(*rat_2));

        attack::ranged(map::g_player, map::g_player->m_pos, p2, *wpn);

        REQUIRE(!actor::is_alive(*rat_1));
        REQUIRE(!actor::is_alive(*rat_2));

        test_utils::cleanup_all();
}

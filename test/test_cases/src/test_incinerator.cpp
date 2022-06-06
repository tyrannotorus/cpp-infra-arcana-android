// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
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
#include "map.hpp"
#include "pos.hpp"
#include "terrain.hpp"
#include "terrain_factory.hpp"
#include "test_utils.hpp"

TEST_CASE("Incinerator projectile explodes on hitting creature")
{
        test_utils::init_all();

        for (int x = 1; x < (map::w() - 1); ++x) {
                for (int y = 1; y < (map::h() - 1); ++y) {
                        map::update_terrain(
                                terrain::make(terrain::Id::floor, {x, y}));
                }
        }

        const P p0(20, 20);
        const P p1(25, 20);
        const P p2(25, 21);

        map::g_player->m_pos = p0;

        const auto* const rat_1 = actor::make(actor::Id::rat, p1);
        const auto* const rat_2 = actor::make(actor::Id::rat, p2);

        auto* const wpn =
                static_cast<item::Wpn*>(
                        item::make(item::Id::incinerator));

        wpn->m_ammo_loaded = 1;

        REQUIRE(rat_1->is_alive());
        REQUIRE(rat_2->is_alive());

        attack::ranged(
                map::g_player,
                map::g_player->m_pos,
                rat_1->m_pos,
                *wpn);

        REQUIRE(!rat_1->is_alive());
        REQUIRE(!rat_2->is_alive());

        test_utils::cleanup_all();
}

TEST_CASE("Incinerator projectile explodes on hitting floor")
{
        test_utils::init_all();

        for (int x = 1; x < (map::w() - 1); ++x) {
                for (int y = 1; y < (map::h() - 1); ++y) {
                        map::update_terrain(
                                terrain::make(terrain::Id::floor, {x, y}));
                }
        }

        // Shooting at a floor position should create an explosion centered at
        // that position, which should always kill small creatures at the edge
        // of the explosion.

        const P p0(20, 20);
        const P p1(23, 20);
        const P p2(25, 20);  // Aim position
        const P p3(27, 20);

        map::g_player->m_pos = p0;

        const auto* const rat_1 = actor::make(actor::Id::rat, p1);
        const auto* const rat_2 = actor::make(actor::Id::rat, p3);

        auto* const wpn =
                static_cast<item::Wpn*>(
                        item::make(item::Id::incinerator));

        wpn->m_ammo_loaded = 1;

        REQUIRE(rat_1->is_alive());
        REQUIRE(rat_2->is_alive());

        attack::ranged(
                map::g_player,
                map::g_player->m_pos,
                p2,
                *wpn);

        REQUIRE(!rat_1->is_alive());
        REQUIRE(!rat_2->is_alive());

        test_utils::cleanup_all();
}

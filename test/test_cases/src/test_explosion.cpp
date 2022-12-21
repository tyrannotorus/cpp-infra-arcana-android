// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "actor.hpp"
#include "actor_data.hpp"
#include "actor_death.hpp"
#include "actor_factory.hpp"
#include "array2.hpp"
#include "catch.hpp"
#include "colors.hpp"
#include "explosion.hpp"
#include "global.hpp"
#include "inventory.hpp"
#include "item_data.hpp"
#include "item_factory.hpp"
#include "map.hpp"
#include "misc.hpp"
#include "pos.hpp"
#include "property.hpp"
#include "property_data.hpp"
#include "property_factory.hpp"
#include "property_handler.hpp"
#include "terrain.hpp"
#include "terrain_data.hpp"
#include "terrain_factory.hpp"
#include "test_utils.hpp"

TEST_CASE("Explosions damage walls")
{
        test_utils::init_all();

        for (int x = 0; x < map::w(); ++x) {
                for (int y = 0; y < map::h(); ++y) {
                        map::update_terrain(
                                terrain::make(terrain::Id::wall, {x, y}));
                }
        }

        const P origin(5, 7);

        map::update_terrain(terrain::make(terrain::Id::floor, origin));

        // Run enough explosions to guarantee destroying adjacent walls
        for (int i = 0; i < 100; ++i) {
                explosion::run(origin, ExplType::expl);
        }

        int nr_destroyed = 0;
        int nr_walls = 0;

        for (int x = (origin.x - 2); x <= (origin.x + 2); ++x) {
                for (int y = (origin.y - 2); y <= (origin.y + 2); ++y) {
                        const P p(x, y);

                        const int dist = king_dist(origin, p);

                        if (dist == 0) {
                                continue;
                        }

                        const auto id = map::g_terrain.at(p)->id();

                        if (dist == 1) {
                                // Adjacent to center - should be destroyed
                                REQUIRE(id != terrain::Id::wall);
                        }
                        else {
                                // Two steps away - should NOT be destroyed
                                REQUIRE(id == terrain::Id::wall);
                        }

                        if (id == terrain::Id::wall) {
                                ++nr_walls;
                        }
                        else {
                                ++nr_destroyed;
                        }
                }
        }

        REQUIRE(nr_destroyed == 8);
        REQUIRE(nr_walls == 16);

        test_utils::cleanup_all();
}

TEST_CASE("Explosions at map edge")
{
        // Check that explosions can handle the map edge correctly (e.g. that
        // they do not destroy the edge wall, or go outside the map - possibly
        // causing a crash)

        test_utils::init_all();

        for (int x = 0; x < map::w(); ++x) {
                for (int y = 0; y < map::h(); ++y) {
                        map::update_terrain(
                                terrain::make(terrain::Id::wall, {x, y}));
                }
        }

        // North-west edge
        int x = 1;
        int y = 1;

        map::update_terrain(terrain::make(terrain::Id::floor, {x, y}));

        const auto wall_id = terrain::Id::wall;

        REQUIRE(map::g_terrain.at(x + 1, y)->id() == wall_id);
        REQUIRE(map::g_terrain.at(x, y + 1)->id() == wall_id);
        REQUIRE(map::g_terrain.at(x - 1, y)->id() == wall_id);
        REQUIRE(map::g_terrain.at(x, y - 1)->id() == wall_id);

        for (int i = 0; i < 100; ++i) {
                explosion::run({x, y}, ExplType::expl);
        }

        REQUIRE(map::g_terrain.at(x + 1, y)->id() != wall_id);
        REQUIRE(map::g_terrain.at(x, y + 1)->id() != wall_id);
        REQUIRE(map::g_terrain.at(x - 1, y)->id() == wall_id);
        REQUIRE(map::g_terrain.at(x, y - 1)->id() == wall_id);

        // South-east edge
        x = map::w() - 2;
        y = map::h() - 2;

        map::update_terrain(terrain::make(terrain::Id::floor, {x, y}));

        REQUIRE(map::g_terrain.at(x - 1, y)->id() == wall_id);
        REQUIRE(map::g_terrain.at(x, y - 1)->id() == wall_id);
        REQUIRE(map::g_terrain.at(x + 1, y)->id() == wall_id);
        REQUIRE(map::g_terrain.at(x, y + 1)->id() == wall_id);

        for (int i = 0; i < 100; ++i) {
                explosion::run(P(x, y), ExplType::expl);
        }

        REQUIRE(map::g_terrain.at(x - 1, y)->id() != wall_id);
        REQUIRE(map::g_terrain.at(x, y - 1)->id() != wall_id);
        REQUIRE(map::g_terrain.at(x + 1, y)->id() == wall_id);
        REQUIRE(map::g_terrain.at(x, y + 1)->id() == wall_id);
}

TEST_CASE("Explosions damage actors")
{
        test_utils::init_all();

        const P origin(5, 7);

        auto* a1 = actor::make("MON_RAT", origin.with_x_offset(1));

        REQUIRE(a1->m_state == ActorState::alive);

        explosion::run(origin, ExplType::expl);

        REQUIRE(a1->m_state == ActorState::destroyed);

        test_utils::cleanup_all();
}

TEST_CASE("Explosions damage corpses")
{
        test_utils::init_all();

        const P origin(5, 7);

        for (int i = 0; i < 100; ++i) {
                explosion::run(origin, ExplType::expl);
        }

        const int nr_corpses = 3;

        actor::Actor* corpses[nr_corpses];

        for (int i = 0; i < nr_corpses; ++i) {
                corpses[i] =
                        actor::make(
                                "MON_RAT",
                                origin.with_x_offset(1));

                actor::kill(
                        *corpses[i],
                        IsDestroyed::no,
                        AllowGore::no,
                        AllowDropItems::no);
        }

        // Check that living and dead actors on the same cell can be destroyed
        auto* a1 = actor::make("MON_RAT", origin.with_x_offset(1));

        for (int i = 0; i < nr_corpses; ++i) {
                REQUIRE(corpses[i]->m_state == ActorState::corpse);
        }

        explosion::run(origin, ExplType::expl);

        for (int i = 0; i < nr_corpses; ++i) {
                REQUIRE(corpses[i]->m_state == ActorState::destroyed);
        }

        REQUIRE(a1->m_state == ActorState::destroyed);

        test_utils::cleanup_all();
}

TEST_CASE("Fire explosion applies burning to actors")
{
        test_utils::init_all();

        const P origin(5, 7);

        const int nr_corpses = 3;

        actor::Actor* corpses[nr_corpses];

        for (int i = 0; i < nr_corpses; ++i) {
                corpses[i] =
                        actor::make(
                                "MON_RAT",
                                origin.with_x_offset(1));

                actor::kill(
                        *corpses[i],
                        IsDestroyed::no,
                        AllowGore::no,
                        AllowDropItems::no);
        }

        auto* const a1 =
                actor::make(
                        "MON_RAT",
                        origin.with_x_offset(-1));

        auto* const a2 =
                actor::make(
                        "MON_RAT",
                        origin.with_x_offset(1));

        explosion::run(
                origin,
                ExplType::apply_prop,
                EmitExplSnd::no,
                0,
                ExplExclCenter::no,
                {new PropBurning()});

        for (int i = 0; i < nr_corpses; ++i) {
                REQUIRE(corpses[i]->m_properties.has(PropId::burning));
        }

        REQUIRE(a1->m_properties.has(PropId::burning));
        REQUIRE(a2->m_properties.has(PropId::burning));

        test_utils::cleanup_all();
}

TEST_CASE("Gas explosions not affecting gas immune creatures")
{
        test_utils::init_all();

        auto run_explosion = [](const P& pos) {
                auto* const prop = property_factory::make(PropId::confused);

                explosion::run(
                        pos,
                        ExplType::apply_prop,
                        EmitExplSnd::no,
                        0,
                        ExplExclCenter::no,
                        {prop},
                        {},
                        ExplIsGas::yes);
        };

        const P origin(5, 7);

        map::update_terrain(terrain::make(terrain::Id::floor, origin));

        auto* const actor = actor::make("MON_ZOMBIE", origin);

        REQUIRE(!actor->m_properties.has(PropId::confused));

        run_explosion(origin);

        REQUIRE(actor->m_properties.has(PropId::confused));

        actor->m_properties.end_prop(PropId::confused);

        REQUIRE(!actor->m_properties.has(PropId::confused));

        actor->m_properties.apply(property_factory::make(PropId::r_breath));

        run_explosion(origin);

        REQUIRE(!actor->m_properties.has(PropId::confused));

        test_utils::cleanup_all();
}

TEST_CASE("Gas mask protects against gas explosions")
{
        test_utils::init_all();

        auto run_explosion = [](const P& pos) {
                auto* const prop = property_factory::make(PropId::confused);

                explosion::run(
                        pos,
                        ExplType::apply_prop,
                        EmitExplSnd::no,
                        0,
                        ExplExclCenter::no,
                        {prop},
                        {},
                        ExplIsGas::yes);
        };

        const P origin(5, 7);

        map::update_terrain(terrain::make(terrain::Id::floor, origin));

        auto& player = *map::g_player;

        map::g_player->m_pos = origin;

        player.m_inv.drop_all_non_intrinsic(origin);

        REQUIRE(!player.m_properties.has(PropId::confused));

        run_explosion(origin);

        REQUIRE(player.m_properties.has(PropId::confused));

        player.m_properties.end_prop(PropId::confused);

        REQUIRE(!player.m_properties.has(PropId::confused));

        player.m_inv.put_in_slot(
                SlotId::head,
                item::make(item::Id::gas_mask),
                Verbose::no);

        run_explosion(origin);

        REQUIRE(!player.m_properties.has(PropId::confused));

        test_utils::cleanup_all();
}

TEST_CASE("Asbestos suite protects against gas explosions")
{
        test_utils::init_all();

        auto run_explosion = [](const P& pos) {
                auto* const prop = property_factory::make(PropId::confused);

                explosion::run(
                        pos,
                        ExplType::apply_prop,
                        EmitExplSnd::no,
                        0,
                        ExplExclCenter::no,
                        {prop},
                        {},
                        ExplIsGas::yes);
        };

        const P origin(5, 7);

        map::update_terrain(terrain::make(terrain::Id::floor, origin));

        auto& player = *map::g_player;

        map::g_player->m_pos = origin;

        player.m_inv.drop_all_non_intrinsic(origin);

        REQUIRE(!player.m_properties.has(PropId::confused));

        run_explosion(origin);

        REQUIRE(player.m_properties.has(PropId::confused));

        player.m_properties.end_prop(PropId::confused);

        REQUIRE(!player.m_properties.has(PropId::confused));

        player.m_inv.put_in_slot(
                SlotId::body,
                item::make(item::Id::armor_asb_suit),
                Verbose::no);

        run_explosion(origin);

        REQUIRE(!player.m_properties.has(PropId::confused));

        test_utils::cleanup_all();
}

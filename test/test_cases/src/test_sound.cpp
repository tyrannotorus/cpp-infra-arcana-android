// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include <string>

#include "actor.hpp"
#include "actor_data.hpp"
#include "actor_factory.hpp"
#include "actor_move.hpp"
#include "actor_player.hpp"
#include "audio_data.hpp"
#include "catch.hpp"
#include "direction.hpp"
#include "map.hpp"
#include "pos.hpp"
#include "property_data.hpp"
#include "property_handler.hpp"
#include "sound.hpp"
#include "terrain.hpp"
#include "test_utils.hpp"

TEST_CASE("Sound alerts monster")
{
        test_utils::init_all();

        for (int x = 0; x < map::w(); ++x)
        {
                for (int y = 0; y < map::h(); ++y)
                {
                        map::put(new terrain::Wall({x, y}));
                }
        }

        const P snd_origin(5, 7);
        const P wall_pos(6, 7);
        const P mon_pos(7, 7);

        // Fill a 3x3 area with floor
        for (int x = wall_pos.x - 1; x <= wall_pos.x + 1; ++x)
        {
                for (int y = wall_pos.y - 1; y <= wall_pos.y + 1; ++y)
                {
                        map::put(new terrain::Floor({x, y}));
                }
        }

        // Put a wall in the middle (the sound will travel around this wall)
        map::put(new terrain::Wall(wall_pos));

        auto* const zombie = actor::make(actor::Id::zombie, mon_pos);

        REQUIRE(!zombie->is_aware_of_player());

        // First run a sound that does NOT alert monsters
        Snd snd(
                "",
                audio::SfxId::END,
                IgnoreMsgIfOriginSeen::no,
                snd_origin,
                nullptr,
                SndVol::low,
                AlertsMon::no);

        snd.run();

        REQUIRE(!zombie->is_aware_of_player());

        // Now run a sound that does alert monsters
        snd.set_alerts_mon(AlertsMon::yes);

        snd.run();

        REQUIRE(zombie->is_aware_of_player());

        test_utils::cleanup_all();
}

TEST_CASE("Player wading alerts monsters")
{
        test_utils::init_all();

        map::put(new terrain::Floor({4, 5}));
        map::put(new terrain::Floor({5, 5}));
        map::put(new terrain::Liquid({6, 5}));
        map::put(new terrain::Floor({7, 5}));

        map::g_player->m_pos = {4, 5};

        auto* const zombie = actor::make(actor::Id::zombie, {7, 5});

        REQUIRE(!zombie->is_aware_of_player());

        // Move player into floor
        actor::move(*map::g_player, Dir::right);

        REQUIRE(!zombie->is_aware_of_player());

        // Move player into water (wading)
        actor::move(*map::g_player, Dir::right);

        REQUIRE(zombie->is_aware_of_player());

        test_utils::cleanup_all();
}

TEST_CASE("Monster wading does not alert monsters")
{
        test_utils::init_all();

        map::put(new terrain::Floor({5, 5}));
        map::put(new terrain::Liquid({6, 5}));
        map::put(new terrain::Floor({7, 5}));

        auto* const zombie_1 = actor::make(actor::Id::zombie, {5, 5});
        auto* const zombie_2 = actor::make(actor::Id::zombie, {7, 5});

        REQUIRE(!zombie_1->is_aware_of_player());
        REQUIRE(!zombie_2->is_aware_of_player());

        // Move zombie 1 into water (wading)
        actor::move(*zombie_1, Dir::right);

        REQUIRE(!zombie_1->is_aware_of_player());
        REQUIRE(!zombie_2->is_aware_of_player());

        test_utils::cleanup_all();
}

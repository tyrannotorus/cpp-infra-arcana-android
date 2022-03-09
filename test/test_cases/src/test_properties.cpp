// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "actor_player.hpp"
#include "catch.hpp"
#include "map.hpp"
#include "property.hpp"
#include "property_data.hpp"
#include "property_factory.hpp"
#include "property_handler.hpp"
#include "test_utils.hpp"

TEST_CASE("Infection triggers disease")
{
        test_utils::init_all();

        auto& properties = map::g_player->m_properties;

        properties.apply(property_factory::make(PropId::infected));

        REQUIRE(properties.has(PropId::infected));
        REQUIRE(!properties.has(PropId::diseased));

        // Tick the infected property enough to no longer exist (could use
        // while-true loop, but this could cause a failing test to get stuck)
        for (int i = 0; i < 100000; ++i)
        {
                properties.on_turn_begin();
        }

        REQUIRE(!properties.has(PropId::infected));
        REQUIRE(properties.has(PropId::diseased));

        test_utils::cleanup_all();
}

TEST_CASE("Number turns active")
{
        test_utils::init_all();

        auto& properties = map::g_player->m_properties;

        auto* const blind = property_factory::make(PropId::blind);
        blind->set_duration(500);

        properties.apply(blind);

        REQUIRE(properties.prop(PropId::blind)->nr_turns_active() == 0);

        properties.on_turn_begin();
        properties.on_turn_begin();
        properties.on_turn_begin();

        REQUIRE(properties.prop(PropId::blind)->nr_turns_active() == 3);

        test_utils::cleanup_all();
}

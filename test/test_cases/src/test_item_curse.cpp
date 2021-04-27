// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include <stddef.h>
#include <memory>

#include "catch.hpp"
#include "actor_player.hpp"
#include "inventory.hpp"
#include "item.hpp"
#include "item_curse.hpp"
#include "item_data.hpp"
#include "item_factory.hpp"
#include "map.hpp"
#include "property_handler.hpp"
#include "test_utils.hpp"
#include "property_data.hpp"

TEST_CASE("Activate curse")
{
        test_utils::init_all();

        auto& props = map::g_player->m_properties;

        for (size_t i = 0; i < (size_t)PropId::END; ++i)
        {
                REQUIRE(!props.has((PropId)i));
        }

        auto* const item = item::make(item::Id::horn_of_malice);

        item->set_curse(
                item_curse::Curse(
                        std::make_unique<item_curse::CannotRead>()));

        map::g_player->m_inv.put_in_backpack(item);

        REQUIRE(!item->current_curse().is_active());

        REQUIRE(!map::g_player->m_properties.has(PropId::cannot_read_curse));

        for (int i = 0; i < 10; ++i)
        {
                item->current_curse().on_player_reached_new_dlvl();
        }

        REQUIRE(!item->current_curse().is_active());

        REQUIRE(!map::g_player->m_properties.has(PropId::cannot_read_curse));

        for (int i = 0; i < 5000; ++i)
        {
                item->current_curse().on_new_turn(*item);
        }

        REQUIRE(item->current_curse().is_active());

        REQUIRE(map::g_player->m_properties.has(PropId::cannot_read_curse));

        test_utils::cleanup_all();
}

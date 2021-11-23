// =============================================================================
// Copyright 2011-2021 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include <stddef.h>

#include "catch.hpp"
#include "actor_player.hpp"
#include "drop.hpp"
#include "explosion.hpp"
#include "inventory.hpp"
#include "item_factory.hpp"
#include "map.hpp"
#include "property_data.hpp"
#include "property_handler.hpp"
#include "test_utils.hpp"
#include "array2.hpp"
#include "global.hpp"
#include "item.hpp"
#include "item_data.hpp"

TEST_CASE("Properties from item applied and removed for actor")
{
        test_utils::init_all();

        auto& inv = map::g_player->m_inv;

        InvSlot& body_slot = inv.m_slots[(size_t)SlotId::body];

        delete body_slot.item;

        body_slot.item = nullptr;

        auto& props = map::g_player->m_properties;

        for (size_t i = 0; i < (size_t)PropId::END; ++i)
        {
                REQUIRE(!props.has((PropId)i));
        }

        // Wear asbesthos suit
        auto* item = item::make(item::Id::armor_asb_suit);

        inv.put_in_slot(
                SlotId::body,
                item,
                Verbose::yes);

        // Check that the expected properties are applied
        int nr_props = 0;

        for (size_t i = 0u; i < (size_t)PropId::END; ++i)
        {
                if (props.has((PropId)i))
                {
                        ++nr_props;
                }
        }

        REQUIRE(nr_props == 3);

        REQUIRE(props.has(PropId::r_fire));
        REQUIRE(props.has(PropId::r_elec));
        REQUIRE(props.has(PropId::r_acid));

        // Take off asbeshos suit
        inv.unequip_slot(SlotId::body);

        REQUIRE(inv.backpack_idx(item::Id::armor_asb_suit) != -1);

        // Check that the properties are cleared
        for (int i = 0; i < (int)PropId::END; ++i)
        {
                REQUIRE(!props.has((PropId)i));
        }

        // Wear the asbeshos suit again
        inv.equip_backpack_item(
                inv.backpack_idx(item::Id::armor_asb_suit),
                SlotId::body);

        // Check that the props are applied
        nr_props = 0;

        for (int i = 0; i < (int)PropId::END; ++i)
        {
                if (props.has((PropId)i))
                {
                        ++nr_props;
                }
        }

        REQUIRE(nr_props == 3);

        REQUIRE(props.has(PropId::r_fire));
        REQUIRE(props.has(PropId::r_elec));
        REQUIRE(props.has(PropId::r_acid));

        // Drop the asbeshos suit on the ground
        item_drop::drop_item_from_inv(
                *map::g_player,
                InvType::slots,
                (int)SlotId::body,
                1);

        REQUIRE(!body_slot.item);

        REQUIRE(map::g_items.at(map::g_player->m_pos));

        // Check that the properties are cleared
        for (int i = 0; i < (int)PropId::END; ++i)
        {
                REQUIRE(!props.has((PropId)i));
        }

        // Wear the same dropped asbesthos suit again
        inv.put_in_slot(
                SlotId::body,
                map::g_items.at(map::g_player->m_pos),
                Verbose::yes);

        map::g_items.at(map::g_player->m_pos) = nullptr;

        // Check that the properties are applied
        nr_props = 0;

        for (int i = 0; i < (int)PropId::END; ++i)
        {
                if (props.has((PropId)i))
                {
                        ++nr_props;
                }
        }

        REQUIRE(nr_props == 3);

        REQUIRE(props.has(PropId::r_fire));
        REQUIRE(props.has(PropId::r_elec));
        REQUIRE(props.has(PropId::r_acid));

        // Destroy the asbesthos suit by explosions
        for (int i = 0; i < 10; ++i)
        {
                map::g_player->restore_hp(99999, true /* Restoring above max */);

                explosion::run(map::g_player->m_pos, ExplType::expl);

                props.end_prop(PropId::wound);
        }

        REQUIRE(!body_slot.item);

        // Check that the properties are cleared
        for (int i = 0; i < (int)PropId::END; ++i)
        {
                REQUIRE(!props.has((PropId)i));
        }

        test_utils::cleanup_all();
}

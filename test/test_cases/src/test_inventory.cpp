// =============================================================================
// Copyright 2011-2024 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include <cstddef>

#include "actor.hpp"
#include "array2.hpp"
#include "catch.hpp"
#include "drop.hpp"
#include "explosion.hpp"
#include "global.hpp"
#include "inventory.hpp"
#include "item.hpp"
#include "item_data.hpp"
#include "item_factory.hpp"
#include "map.hpp"
#include "property_data.hpp"
#include "property_handler.hpp"
#include "test_utils.hpp"

TEST_CASE("Properties from item applied and removed for actor")
{
        test_utils::init_all();

        auto& inv = map::g_player->m_inv;

        InvSlot& body_slot = inv.m_slots[(size_t)SlotId::body];

        delete body_slot.item;

        body_slot.item = nullptr;

        auto& props = map::g_player->m_properties;

        for (size_t i = 0; i < (size_t)prop::Id::END; ++i) {
                REQUIRE(!props.has((prop::Id)i));
        }

        // Wear asbesthos suit
        auto* item = item::make(item::Id::armor_asb_suit);

        inv.put_in_slot(
                SlotId::body,
                item,
                Verbose::yes);

        // Check that the expected properties are applied
        int nr_props = 0;

        for (size_t i = 0u; i < (size_t)prop::Id::END; ++i) {
                if (props.has((prop::Id)i)) {
                        ++nr_props;
                }
        }

        REQUIRE(nr_props == 3);

        REQUIRE(props.has(prop::Id::r_fire));
        REQUIRE(props.has(prop::Id::r_elec));
        REQUIRE(props.has(prop::Id::r_acid));

        // Take off asbeshos suit
        inv.unequip_slot(SlotId::body);

        REQUIRE(inv.backpack_idx(item::Id::armor_asb_suit) != -1);

        // Check that the properties are cleared
        for (int i = 0; i < (int)prop::Id::END; ++i) {
                REQUIRE(!props.has((prop::Id)i));
        }

        // Wear the asbeshos suit again
        inv.equip_backpack_item(
                inv.backpack_idx(item::Id::armor_asb_suit),
                SlotId::body);

        // Check that the props are applied
        nr_props = 0;

        for (int i = 0; i < (int)prop::Id::END; ++i) {
                if (props.has((prop::Id)i)) {
                        ++nr_props;
                }
        }

        REQUIRE(nr_props == 3);

        REQUIRE(props.has(prop::Id::r_fire));
        REQUIRE(props.has(prop::Id::r_elec));
        REQUIRE(props.has(prop::Id::r_acid));

        // Drop the asbeshos suit on the ground
        item_drop::drop_item_from_inv(
                *map::g_player,
                InvType::slots,
                (int)SlotId::body,
                1);

        REQUIRE(!body_slot.item);

        REQUIRE(map::g_items.at(map::g_player->m_pos));

        // Check that the properties are cleared
        for (int i = 0; i < (int)prop::Id::END; ++i) {
                REQUIRE(!props.has((prop::Id)i));
        }

        // Wear the same dropped asbesthos suit again
        inv.put_in_slot(
                SlotId::body,
                map::g_items.at(map::g_player->m_pos),
                Verbose::yes);

        map::g_items.at(map::g_player->m_pos) = nullptr;

        // Check that the properties are applied
        nr_props = 0;

        for (int i = 0; i < (int)prop::Id::END; ++i) {
                if (props.has((prop::Id)i)) {
                        ++nr_props;
                }
        }

        REQUIRE(nr_props == 3);

        REQUIRE(props.has(prop::Id::r_fire));
        REQUIRE(props.has(prop::Id::r_elec));
        REQUIRE(props.has(prop::Id::r_acid));

        // Destroy the asbesthos suit by explosions
        for (int i = 0; i < 10; ++i) {
                actor::restore_hp(*map::g_player, 99999, actor::AllowRestoreAboveMax::yes);

                explosion::run(map::g_player->m_pos, ExplType::expl);

                props.end_prop(prop::Id::wound);
        }

        REQUIRE(!body_slot.item);

        // Check that the properties are cleared
        for (int i = 0; i < (int)prop::Id::END; ++i) {
                REQUIRE(!props.has((prop::Id)i));
        }

        test_utils::cleanup_all();
}

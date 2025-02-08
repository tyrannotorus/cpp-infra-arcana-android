// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "pickup.hpp"

#include <cstddef>
#include <string>

#include "actor.hpp"
#include "array2.hpp"
#include "audio.hpp"
#include "audio_data.hpp"
#include "debug.hpp"
#include "game_time.hpp"
#include "global.hpp"
#include "inventory.hpp"
#include "item.hpp"
#include "item_ammo.hpp"
#include "item_data.hpp"
#include "item_factory.hpp"
#include "item_weapon.hpp"
#include "map.hpp"
#include "msg_log.hpp"
#include "state.hpp"

namespace item_pickup
{
void try_pick()
{
        msg_log::clear();

        const P& pos = map::g_player->m_pos;
        item::Item* const item = map::g_items.at(pos);

        if (!item) {
                msg_log::add("I see nothing to pick up here.");

                return;
        }

        const ItemPrePickResult pre_pickup_result = item->pre_pickup_hook();

        switch (pre_pickup_result) {
        case ItemPrePickResult::do_pickup: {
                audio::play(audio::SfxId::pickup);

                const std::string item_name = item->name(ItemNameType::plural);

                msg_log::add("I pick up " + item_name + ".");

                // NOTE: This may destroy the item (e.g. combine with others)
                map::g_player->m_inv.put_in_backpack(item);

                map::g_items.at(pos) = nullptr;
        } break;

        case ItemPrePickResult::destroy_item: {
                delete item;
                map::g_items.at(pos) = nullptr;
        } break;

        case ItemPrePickResult::do_nothing: {
        } break;
        }

        // NOTE: The player might have won the game by picking up the Trapezohedron, if so do not
        // tick time.
        if (states::contains_state(StateId::game)) {
                game_time::tick();
        }
}

item::Item* unload_ranged_wpn(item::Wpn& wpn)
{
        ASSERT(!wpn.data().ranged.has_infinite_ammo);

        const int nr_ammo_loaded = wpn.m_ammo_loaded;

        if (nr_ammo_loaded == 0) {
                return nullptr;
        }

        const item::Id ammo_id = wpn.data().ranged.ammo_item_id;

        item::ItemData& ammo_data = item::g_data[(size_t)ammo_id];

        item::Item* spawned_ammo = item::make(ammo_id);

        if (ammo_data.type == ItemType::ammo_mag) {
                // Unload a mag
                static_cast<item::AmmoMag*>(spawned_ammo)->m_ammo =
                        nr_ammo_loaded;
        }
        else {
                // Unload loose ammo
                spawned_ammo->m_nr_items = nr_ammo_loaded;
        }

        wpn.m_ammo_loaded = 0;

        return spawned_ammo;
}

void try_unload_or_pick()
{
        item::Item* item = map::g_items.at(map::g_player->m_pos);

        if (item &&
            item->data().ranged.is_ranged_wpn &&
            !item->data().ranged.has_infinite_ammo) {
                auto* const wpn = static_cast<item::Wpn*>(item);

                item::Item* const spawned_ammo = unload_ranged_wpn(*wpn);

                if (spawned_ammo) {
                        audio::play(audio::SfxId::pickup);

                        const std::string name_a =
                                item->name(
                                        ItemNameType::a,
                                        ItemNameInfo::yes);

                        msg_log::add("I unload " + name_a + ".");

                        map::g_player->m_inv.put_in_backpack(spawned_ammo);

                        game_time::tick();

                        return;
                }
        }

        // Did not unload a ranged weapon, run the normal item picking instead

        try_pick();
}

}  // namespace item_pickup

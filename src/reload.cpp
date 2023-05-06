// =============================================================================
// Copyright 2011-2023 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "reload.hpp"

#include <climits>
#include <cstddef>
#include <string>
#include <vector>

#include "actor.hpp"
#include "actor_see.hpp"
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
#include "property_data.hpp"
#include "property_handler.hpp"
#include "random.hpp"
#include "text_format.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static void msg_reload_fumble(
        const actor::Actor& actor,
        const item::Item& ammo)
{
        const std::string ammo_name = ammo.name(ItemNameType::a);

        if (actor::is_player(&actor)) {
                msg_log::add("I fumble with " + ammo_name + ".");
        }
        else {
                // Is monster
                if (actor::can_player_see_actor(actor)) {
                        const std::string name_the =
                                text_format::first_to_upper(
                                        actor.name_the());

                        msg_log::add(
                                name_the +
                                " fumbles with " +
                                ammo_name +
                                ".");
                }
        }
}

static void msg_reloaded(
        const actor::Actor& actor,
        const item::Wpn& wpn,
        const item::Item& ammo)
{
        if (actor::is_player(&actor)) {
                const auto ammo_loaded = wpn.m_ammo_loaded;
                const auto ammo_max = wpn.data().ranged.max_ammo;
                const auto ammo_loaded_str = std::to_string(ammo_loaded);
                const auto ammo_max_str = std::to_string(ammo_max);

                audio::play(wpn.data().ranged.reload_sfx);

                // TODO: This is a hack
                if ((wpn.id() == item::Id::revolver) &&
                    (ammo_loaded == ammo_max)) {
                        audio::play(audio::SfxId::revolver_spin);
                }

                if (ammo.data().type == ItemType::ammo_mag) {
                        const std::string wpn_name =
                                wpn.name(
                                        ItemNameType::plain,
                                        ItemNameInfo::none);

                        msg_log::add(
                                "I reload my " +
                                wpn_name +
                                " (" +
                                ammo_loaded_str +
                                "/" +
                                ammo_max_str +
                                ").");
                }
                else {
                        // Not a magazine
                        const std::string ammo_name =
                                ammo.name(ItemNameType::a);

                        msg_log::add(
                                "I load " +
                                ammo_name +
                                " (" +
                                ammo_loaded_str +
                                "/" +
                                ammo_max_str +
                                ").");
                }
        }
        else {
                // Is monster
                if (actor::can_player_see_actor(actor)) {
                        const std::string name_the =
                                text_format::first_to_upper(
                                        actor.name_the());

                        msg_log::add(name_the + " reloads.");
                }
        }
}

// -----------------------------------------------------------------------------
// reload
// -----------------------------------------------------------------------------
namespace reload
{
void try_reload(actor::Actor& actor, item::Item* const item_to_reload)
{
        if (!item_to_reload) {
                msg_log::add("I am not wielding a weapon.");

                return;
        }

        ASSERT(item_to_reload->data().type == ItemType::melee_wpn ||
               item_to_reload->data().type == ItemType::ranged_wpn);

        auto* const wpn = static_cast<item::Wpn*>(item_to_reload);

        const int wpn_max_ammo = wpn->data().ranged.max_ammo;

        if (wpn->data().ranged.has_infinite_ammo ||
            (wpn_max_ammo == 0)) {
                msg_log::add("This weapon does not use ammo.");
                return;
        }

        const int ammo_loaded_before = wpn->m_ammo_loaded;

        if (ammo_loaded_before >= wpn_max_ammo) {
                const std::string item_name =
                        wpn->name(
                                ItemNameType::plain,
                                ItemNameInfo::none);

                msg_log::add("My " + item_name + " is already loaded.");
                return;
        }

        const auto ammo_item_id = wpn->data().ranged.ammo_item_id;

        const auto& ammo_data = item::g_data[(size_t)ammo_item_id];

        const bool is_using_mag = ammo_data.type == ItemType::ammo_mag;

        item::Item* ammo_item = nullptr;

        size_t ammo_backpack_idx = 0;

        int max_mag_ammo = 0;

        // Find ammo in backpack to reload from
        for (size_t i = 0; i < actor.m_inv.m_backpack.size(); ++i) {
                auto* const item = actor.m_inv.m_backpack[i];

                if (item->id() == ammo_item_id) {
                        if (is_using_mag) {
                                // Find mag with most ammo in it

                                const auto* const mag =
                                        static_cast<const item::AmmoMag*>(item);

                                const int nr_ammo = mag->m_ammo;

                                if (nr_ammo > max_mag_ammo) {
                                        max_mag_ammo = nr_ammo;

                                        if (nr_ammo > ammo_loaded_before) {
                                                // Magazine is a candidate for
                                                // reloading
                                                ammo_item = item;

                                                ammo_backpack_idx = i;
                                        }
                                }
                        }
                        else {
                                // Not using mag

                                // Just use first item with matching ammo id
                                ammo_item = item;

                                ammo_backpack_idx = i;

                                break;
                        }
                }
        }

        if (!ammo_item) {
                // Loaded mag has more ammo than any mag in backpack
                if (is_using_mag &&
                    max_mag_ammo > 0 &&
                    max_mag_ammo <= ammo_loaded_before) {
                        const auto idx = (size_t)ItemNameType::plain;

                        const std::string mag_name =
                                ammo_data.base_name.names[idx];

                        msg_log::add(
                                "I carry no " +
                                mag_name +
                                " with more ammo than already loaded.");
                }
                else {
                        msg_log::add("I carry no ammunition for this weapon.");
                }

                return;
        }

        // Being blinded or terrified makes it harder to reload
        const bool is_blind = !actor.m_properties.allow_see();

        const bool is_terrified = actor.m_properties.has(prop::Id::terrified);

        const int k = 48;

        const int fumble_pct = (k * is_blind) + (k * is_terrified);

        if (rnd::percent(fumble_pct)) {
                msg_reload_fumble(actor, *ammo_item);
        }
        else {
                // Not fumbling
                bool is_mag = ammo_item->data().type == ItemType::ammo_mag;

                if (is_mag) {
                        auto* mag_item = static_cast<item::AmmoMag*>(ammo_item);

                        wpn->m_ammo_loaded = mag_item->m_ammo;

                        msg_reloaded(actor, *wpn, *ammo_item);

                        // Destroy loaded mag
                        actor.m_inv.remove_item_in_backpack_with_idx(
                                ammo_backpack_idx,
                                true);

                        // If weapon previously contained ammo, create a new mag
                        if (ammo_loaded_before > 0) {
                                ammo_item = item::make(ammo_item_id);

                                mag_item =
                                        static_cast<item::AmmoMag*>(ammo_item);

                                mag_item->m_ammo = ammo_loaded_before;

                                actor.m_inv.put_in_backpack(mag_item);
                        }
                }
                else {
                        // Not using mag
                        ++wpn->m_ammo_loaded;

                        msg_reloaded(actor, *wpn, *ammo_item);

                        actor.m_inv.decr_item_in_backpack(ammo_backpack_idx);
                }
        }

        game_time::tick();
}

void player_arrange_pistol_mags()
{
        auto& player = *map::g_player;

        item::Wpn* wielded_pistol = nullptr;

        auto* const wielded_item = player.m_inv.item_in_slot(SlotId::wpn);

        if (wielded_item && wielded_item->id() == item::Id::pistol) {
                wielded_pistol = static_cast<item::Wpn*>(wielded_item);
        }

        // Find the two non-full mags in the backpack with the least/most ammo.
        // NOTE: These may be the same item.
        int min_mag_ammo = INT_MAX;

        int max_mag_ammo = 0;

        item::AmmoMag* min_mag = nullptr;  // Most empty magazine

        item::AmmoMag* max_mag = nullptr;  // Most full magazine

        size_t min_mag_backpack_idx = 0;

        const int pistol_max_ammo =
                item::g_data[(size_t)item::Id::pistol].ranged.max_ammo;

        for (size_t i = 0; i < player.m_inv.m_backpack.size(); ++i) {
                auto* const item = player.m_inv.m_backpack[i];

                if (item->id() != item::Id::pistol_mag) {
                        continue;
                }

                auto* const mag = static_cast<item::AmmoMag*>(item);

                if (mag->m_ammo == pistol_max_ammo) {
                        continue;
                }

                // NOTE: For min mag, we check for lesser OR EQUAL rounds
                // loaded.  This way, when several "least full mags" are found,
                // the last one will be picked as THE max mag to use. The
                // purpose of this is that we should try avoid picking the same
                // mag as min and max (e.g. if we have two mags with 6 bullets
                // each, then we want to move a bullet).
                if (mag->m_ammo <= min_mag_ammo) {
                        min_mag_ammo = mag->m_ammo;
                        min_mag = mag;
                        min_mag_backpack_idx = i;
                }

                // Use the first "most full mag" that we find, as the max mag
                if (mag->m_ammo > max_mag_ammo) {
                        max_mag_ammo = mag->m_ammo;
                        max_mag = mag;
                }
        }

        if (!min_mag) {
                // No least full mag exists, do nothing
                return;
        }

        // If wielded pistol is not fully loaded, move round from least full mag
        if (wielded_pistol &&
            wielded_pistol->m_ammo_loaded < pistol_max_ammo) {
                --min_mag->m_ammo;
                ++wielded_pistol->m_ammo_loaded;

                const std::string name =
                        wielded_pistol->name(
                                ItemNameType::plain,
                                ItemNameInfo::yes);

                msg_log::add(
                        "I move a round from a magazine to my " +
                        name +
                        ".");
        }
        // Otherwise, if two non-full mags exists, move from least to most full
        else if (max_mag && (min_mag != max_mag)) {
                --min_mag->m_ammo;
                ++max_mag->m_ammo;

                msg_log::add("I move a round from one magazine to another.");
        }

        if (min_mag->m_ammo == 0) {
                player.m_inv.remove_item_in_backpack_with_idx(
                        min_mag_backpack_idx,
                        true);
        }
}

}  // namespace reload

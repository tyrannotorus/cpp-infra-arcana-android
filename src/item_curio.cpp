// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "item_curio.hpp"

#include <memory>
#include <string>

#include "actor.hpp"
#include "actor_data.hpp"
#include "create_character.hpp"
#include "map.hpp"
#include "msg_log.hpp"
#include "property.hpp"
#include "property_data.hpp"
#include "property_factory.hpp"
#include "property_handler.hpp"
#include "random.hpp"
#include "state.hpp"

namespace item
{
// -----------------------------------------------------------------------------
// Zombie Dust
// -----------------------------------------------------------------------------
void ZombieDust::on_ranged_hit(actor::Actor& actor_hit)
{
        if (actor_hit.is_alive() && !actor_hit.m_data->is_undead) {
                actor_hit.m_properties.apply(
                        property_factory::make(PropId::paralyzed));
        }
}

// -----------------------------------------------------------------------------
// Witches Eye
// -----------------------------------------------------------------------------
ConsumeItem WitchEye::activate(actor::Actor* actor)
{
        (void)actor;

        const auto item_name = name(ItemNameType::plain);

        msg_log::add("I clutch the " + item_name + "...");

        auto* const search =
                static_cast<PropMagicSearching*>(
                        property_factory::make(PropId::magic_searching));

        search->set_range(g_fov_radi_int);

        search->set_allow_reveal_items();
        search->set_allow_reveal_creatures();

        search->set_duration(rnd::range(60, 80));

        map::g_player->m_properties.apply(search);

        map::g_player->incr_shock(12.0, ShockSrc::use_strange_item);

        if (rnd::one_in(3)) {
                msg_log::add("The eye decomposes.");

                return ConsumeItem::yes;
        }
        else {
                return ConsumeItem::no;
        }
}

// -----------------------------------------------------------------------------
// Flask of Damning
// -----------------------------------------------------------------------------
// ConsumeItem FlaskOfDamning::activate(actor::Actor* actor)
// {
//         // TODO

//         (void)actor;

//         return ConsumeItem::no;
// }

// -----------------------------------------------------------------------------
// Obsidian Charm
// -----------------------------------------------------------------------------
// ConsumeItem ObsidianCharm::activate(actor::Actor* actor)
// {
//         // TODO

//         (void)actor;

//         return ConsumeItem::no;
// }

// -----------------------------------------------------------------------------
// Fluctuating Material
// -----------------------------------------------------------------------------
ConsumeItem FluctuatingMaterial::activate(actor::Actor* actor)
{
        (void)actor;

        const auto item_name = name(ItemNameType::plain);

        msg_log::add(
                "I stare into the " +
                item_name +
                ", and feel myself changing...");

        // First lose one trait, then gain one trait

        states::push(
                std::make_unique<PickTraitState>(
                        "Which trait do you gain?"));

        states::push(std::make_unique<RemoveTraitState>());

        map::g_player->incr_shock(12.0, ShockSrc::use_strange_item);

        // TODO: Print a message that the item disappears

        return ConsumeItem::yes;
}

// -----------------------------------------------------------------------------
// Bat Wing Salve
// -----------------------------------------------------------------------------
// ConsumeItem BatWingSalve::activate(actor::Actor* actor)
// {
//         // TODO

//         (void)actor;

//         return ConsumeItem::no;
// }

// -----------------------------------------------------------------------------
// Astral Opium
// -----------------------------------------------------------------------------
ConsumeItem AstralOpium::activate(actor::Actor* actor)
{
        (void)actor;

        const auto item_name = name(ItemNameType::plain);

        msg_log::add("I use the " + item_name + "...");

        map::g_player->m_properties.apply(
                property_factory::make(
                        PropId::astral_opium_addiction));

        map::g_player->m_properties.end_prop(
                PropId::frenzied);

        map::g_player->m_properties.apply(
                property_factory::make(
                        PropId::r_shock));

        map::g_player->m_properties.apply(
                property_factory::make(
                        PropId::r_fear));

        map::g_player->restore_shock(999, false);

        auto* const halluc =
                property_factory::make(
                        PropId::hallucinating);

        map::g_player->m_properties.apply(halluc);

        return ConsumeItem::yes;
}

}  // namespace item

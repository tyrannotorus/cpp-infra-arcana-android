// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "item_armor.hpp"

#include <algorithm>

#include "actor.hpp"
#include "debug.hpp"
#include "item_data.hpp"
#include "map.hpp"
#include "msg_log.hpp"
#include "player_bon.hpp"
#include "property.hpp"
#include "property_data.hpp"
#include "property_factory.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// item
// -----------------------------------------------------------------------------
namespace item
{
Armor::Armor(ItemData* const item_data) :
        Item(item_data) {}

void Armor::hit(const int dmg)
{
        const int ap_before = armor_points();

        // Damage factor
        const auto dmg_db = double(dmg);

        // Armor durability factor
        const double df = m_data->armor.dmg_to_durability_factor;

        // Scaling factor
        const double k = 2.0;

        // Armor lasts twice as long for War Vets
        double war_vet_k = 1.0;

        ASSERT(m_actor_carrying);

        if (actor::is_player(m_actor_carrying) &&
            player_bon::is_bg(Bg::war_vet)) {
                war_vet_k = 0.5;
        }

        m_durability -= (int)(dmg_db * df * k * war_vet_k);

        m_durability = std::max(0, m_durability);

        const int ap_after = armor_points();

        if ((ap_after < ap_before) &&
            (ap_after != 0)) {
                const std::string armor_name = name(ItemNameType::plain);

                msg_log::add(
                        "My " + armor_name + " is damaged!",
                        colors::msg_note());
        }
}

std::string Armor::name_info_str() const
{
        const int ap = armor_points();

        const std::string ap_str = std::to_string(std::max(1, ap));

        return "(" + ap_str + " armor)";
}

void ArmorAsbSuit::on_equip_hook(const Verbose verbose)
{
        (void)verbose;

        prop::Prop* prop_r_fire = prop::make(prop::Id::r_fire);
        prop::Prop* prop_r_acid = prop::make(prop::Id::r_acid);
        prop::Prop* prop_r_elec = prop::make(prop::Id::r_elec);

        prop_r_fire->set_indefinite();
        prop_r_acid->set_indefinite();
        prop_r_elec->set_indefinite();

        add_carrier_prop(prop_r_fire, Verbose::no);
        add_carrier_prop(prop_r_acid, Verbose::no);
        add_carrier_prop(prop_r_elec, Verbose::no);
}

void ArmorAsbSuit::on_unequip_hook()
{
        clear_carrier_props();
}

void ArmorMiGo::on_equip_hook(const Verbose verbose)
{
        if (verbose == Verbose::yes) {
                msg_log::add(
                        "The armor joins with my skin!",
                        colors::text(),
                        MsgInterruptPlayer::no,
                        MorePromptOnMsg::yes);

                map::g_player->incr_shock(12.0, ShockSrc::use_strange_item);
        }
}

}  // namespace item

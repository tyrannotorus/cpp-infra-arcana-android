// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>

#include "actor.hpp"
#include "actor_data.hpp"
#include "colors.hpp"
#include "debug.hpp"
#include "game.hpp"
#include "global.hpp"
#include "item.hpp"
#include "item_curse.hpp"
#include "item_data.hpp"
#include "map.hpp"
#include "msg_log.hpp"
#include "player_bon.hpp"
#include "property.hpp"
#include "property_data.hpp"
#include "property_handler.hpp"
#include "random.hpp"
#include "saving.hpp"
#include "text_format.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static ItemNameAttackInfo att_mode_to_name_att_info(AttackMode att_mode)
{
        switch (att_mode) {
        case AttackMode::melee:
                return ItemNameAttackInfo::melee;

        case AttackMode::ranged:
                return ItemNameAttackInfo::ranged;

        case AttackMode::thrown:
                return ItemNameAttackInfo::thrown;

        case AttackMode::none:
                return ItemNameAttackInfo::none;
        }

        ASSERT(false);

        return (ItemNameAttackInfo)0;
}

// -----------------------------------------------------------------------------
// item
// -----------------------------------------------------------------------------
namespace item
{
// -----------------------------------------------------------------------------
// Item
// -----------------------------------------------------------------------------
Item::Item(ItemData* item_data) :
        m_data(item_data),
        m_base_melee_dmg(item_data->melee.dmg),
        m_base_ranged_dmg(item_data->ranged.dmg),
        m_durability(rnd::range(80, 100))
{
}

Item& Item::operator=(const Item& other)
{
        if (&other == this) {
                return *this;
        }

        m_nr_items = other.m_nr_items;
        m_data = other.m_data;
        m_actor_carrying = other.m_actor_carrying;
        m_carrier_props = other.m_carrier_props;
        m_base_melee_dmg = other.m_base_melee_dmg;
        m_base_ranged_dmg = other.m_base_ranged_dmg;

        return *this;
}

Item::~Item()
{
        for (auto* prop : m_carrier_props) {
                delete prop;
        }
}

Id Item::id() const
{
        return m_data->id;
}

void Item::save()
{
        saving::put_int(m_base_melee_dmg.base_min());
        saving::put_int(m_base_melee_dmg.base_max());
        saving::put_int(m_base_melee_dmg.plus());

        saving::put_int(m_base_ranged_dmg.base_min());
        saving::put_int(m_base_ranged_dmg.base_max());
        saving::put_int(m_base_ranged_dmg.plus());

        saving::put_int(m_durability);

        m_curse.save();

        save_hook();
}

void Item::load()
{
        m_base_melee_dmg.set_base_min(saving::get_int());
        m_base_melee_dmg.set_base_max(saving::get_int());
        m_base_melee_dmg.set_plus(saving::get_int());

        m_base_ranged_dmg.set_base_min(saving::get_int());
        m_base_ranged_dmg.set_base_max(saving::get_int());
        m_base_ranged_dmg.set_plus(saving::get_int());

        m_durability = saving::get_int();

        m_curse.load();

        load_hook();
}

ItemData& Item::data() const
{
        return *m_data;
}

Color Item::color() const
{
        return m_data->color;
}

char Item::character() const
{
        return m_data->character;
}

gfx::TileId Item::tile() const
{
        return m_data->tile;
}

std::vector<std::string> Item::descr() const
{
        auto full_descr = descr_hook();

        if (m_curse.is_active()) {
                full_descr.push_back(m_curse.descr());
        }

        return full_descr;
}

std::vector<std::string> Item::descr_hook() const
{
        return m_data->base_descr;
}

WpnDmg Item::melee_dmg(const actor::Actor* const attacker) const
{
        auto range = m_base_melee_dmg;

        if (range.total_range().max == 0) {
                return range;
        }

        if (actor::is_player(attacker)) {
                // Bonus damage from melee traits.
                if (player_bon::has_trait(Trait::adept_melee)) {
                        range.set_plus(range.plus() + 1);
                }

                if (player_bon::has_trait(Trait::expert_melee)) {
                        range.set_plus(range.plus() + 1);
                }

                if (player_bon::has_trait(Trait::master_melee)) {
                        range.set_plus(range.plus() + 1);
                }

                // TODO: This should be handled via the 'specific_dmg_mod' hook
                if (id() == Id::player_ghoul_claw) {
                        if (player_bon::has_trait(Trait::foul)) {
                                range.set_plus(range.plus() + 1);
                        }

                        if (player_bon::has_trait(Trait::toxic)) {
                                range.set_plus(range.plus() + 1);
                        }
                }

                // Bonus damage from Flagellant Moribund?
                if (attacker && attacker->m_properties.has(prop::Id::moribund)) {
                        auto* const moribund =
                                static_cast<prop::Moribund*>(
                                        attacker->m_properties.prop(
                                                prop::Id::moribund));

                        const bool has_moribund_bon = moribund->has_bonus();

                        if (has_moribund_bon) {
                                int moribund_bon = 3;

                                if (player_bon::has_trait(Trait::death_sense)) {
                                        moribund_bon *= 2;
                                }

                                range.set_plus(range.plus() + moribund_bon);
                        }
                }
        }

        // Bonus damage from being frenzied?
        if (attacker && attacker->m_properties.has(prop::Id::frenzied)) {
                range.set_plus(range.plus() + 1);
        }

        specific_dmg_mod(range, attacker);

        return range;
}

WpnDmg Item::ranged_dmg(const actor::Actor* const attacker) const
{
        auto range = m_base_ranged_dmg;

        specific_dmg_mod(range, attacker);

        return range;
}

WpnDmg Item::thrown_dmg(const actor::Actor* const attacker) const
{
        // Melee weapons do throw damage based on their melee damage
        auto range =
                (m_data->type == ItemType::melee_wpn)
                ? m_base_melee_dmg
                : m_base_ranged_dmg;

        if (range.total_range().max == 0) {
                return range;
        }

        specific_dmg_mod(range, attacker);

        return range;
}

ItemAttackProp& Item::prop_applied_on_melee(
        const actor::Actor* const attacker) const
{
        auto* const intr = prop_applied_intr_attack(attacker);

        if (intr) {
                return *intr;
        }

        return data().melee.prop_applied;
}

ItemAttackProp& Item::prop_applied_on_ranged(
        const actor::Actor* const attacker) const
{
        auto* const intr = prop_applied_intr_attack(attacker);

        if (intr) {
                return *intr;
        }

        return data().ranged.prop_applied;
}

ItemAttackProp* Item::prop_applied_intr_attack(
        const actor::Actor* const attacker) const
{
        if (attacker) {
                const auto& intr_attacks = attacker->m_data->intr_attacks;

                for (const auto& att : intr_attacks) {
                        if (att->item_id == id()) {
                                return &att->prop_applied;
                        }
                }
        }

        return nullptr;
}

int Item::weight() const
{
        auto w = (int)m_data->weight * m_nr_items;

        if (m_curse.is_active()) {
                w = m_curse.affect_weight(w);
        }

        return w;
}

std::string Item::weight_str() const
{
        const int wgt = weight();

        if (wgt <= ((int)Weight::extra_light + (int)Weight::light) / 2) {
                return "very light";
        }

        if (wgt <= ((int)Weight::light + (int)Weight::medium) / 2) {
                return "light";
        }

        if (wgt <= ((int)Weight::medium + (int)Weight::heavy) / 2) {
                return "a bit heavy";
        }

        return "heavy";
}

ConsumeItem Item::activate(actor::Actor* const actor)
{
        (void)actor;

        msg_log::add("I cannot apply that.");

        return ConsumeItem::no;
}

void Item::on_std_turn_in_inv(const InvType inv_type)
{
        ASSERT(m_actor_carrying);

        if (actor::is_player(m_actor_carrying)) {
                m_curse.on_new_turn(*this);
        }

        on_std_turn_in_inv_hook(inv_type);
}

void Item::on_actor_turn_in_inv(const InvType inv_type)
{
        ASSERT(m_actor_carrying);

        on_actor_turn_in_inv_hook(inv_type);
}

void Item::on_pickup(actor::Actor& actor)
{
        ASSERT(!m_actor_carrying);

        m_actor_carrying = &actor;

        if (actor::is_player(m_actor_carrying)) {
                discover();
        }

        m_curse.on_item_picked_up(*this);

        on_pickup_hook();
}

void Item::on_equip(const Verbose verbose)
{
        ASSERT(m_actor_carrying);

        on_equip_hook(verbose);
}

void Item::on_unequip()
{
        ASSERT(m_actor_carrying);

        on_unequip_hook();
}

void Item::on_removed_from_inv()
{
        m_curse.on_item_dropped();

        on_removed_from_inv_hook();

        m_actor_carrying = nullptr;
}

void Item::discover()
{
        if ((m_data->xp_on_found > 0) && !m_data->is_found) {
                const std::string item_name =
                        name(
                                ItemNameType::a,
                                ItemNameInfo::yes);

                msg_log::more_prompt();

                msg_log::add("I have discovered " + item_name + "!");

                game::incr_player_xp(m_data->xp_on_found, Verbose::yes);

                game::add_history_event("Discovered " + item_name);
        }

        m_data->is_found = true;
}

void Item::on_player_reached_new_dlvl()
{
        m_curse.on_player_reached_new_dlvl();

        on_player_reached_new_dlvl_hook();
}

std::string Item::name(
        const ItemNameType name_type,
        const ItemNameInfo info,
        const ItemNameAttackInfo attack_info) const
{
        auto name_type_used = name_type;

        // If requested name type is "plural" and this is a single item, use
        // name type "a" instead.
        if ((name_type == ItemNameType::plural) &&
            (!m_data->is_stackable || (m_nr_items == 1))) {
                name_type_used = ItemNameType::a;
        }

        std::string nr_str;

        if (name_type_used == ItemNameType::plural) {
                nr_str = std::to_string(m_nr_items);
        }

        auto dmg_str = this->dmg_str(attack_info, ItemNameDmg::average);
        auto hit_str = hit_mod_str(attack_info, AbbrevItemAttackInfo::no);

        std::string info_str;

        if (info == ItemNameInfo::yes) {
                info_str = name_info_str();
        }

        const auto& names_used =
                m_data->is_identified
                ? m_data->base_name
                : m_data->base_name_un_id;

        const std::string base_name =
                names_used.names[(size_t)name_type_used];

        std::string full_name;

        text_format::append_with_space(full_name, nr_str);
        text_format::append_with_space(full_name, base_name);

        if (!dmg_str.empty() || !hit_str.empty()) {
                std::string plus_info = plus_str(attack_info);

                text_format::append_with_space(full_name, plus_info);

                std::string combat_info;

                text_format::append_with_space(combat_info, dmg_str);
                text_format::append_with_space(combat_info, hit_str);

                combat_info = "(" + combat_info + ")";

                text_format::append_with_space(full_name, combat_info);
        }

        text_format::append_with_space(full_name, info_str);

        ASSERT(!full_name.empty());

        return full_name;
}

std::string Item::hit_mod_str(
        const ItemNameAttackInfo attack_info,
        const AbbrevItemAttackInfo abbrev) const
{
        auto get_hit_mod_str = [abbrev](const int hit_mod) {
                std::string str;

                if (hit_mod >= 0) {
                        str = "+";
                }

                str += std::to_string(hit_mod) + "%";

                if (abbrev == AbbrevItemAttackInfo::no) {
                        str += " hit";
                }

                return str;
        };

        auto attack_info_used = attack_info;

        // If caller requested attack info depending on main attack mode, set
        // the attack info used to a specific type
        if (attack_info == ItemNameAttackInfo::main_attack_mode) {
                switch (m_data->main_attack_mode) {
                case AttackMode::melee:
                        attack_info_used = ItemNameAttackInfo::melee;
                        break;

                case AttackMode::ranged:
                        attack_info_used = ItemNameAttackInfo::ranged;
                        break;

                case AttackMode::thrown:
                        attack_info_used = ItemNameAttackInfo::thrown;
                        break;

                case AttackMode::none:
                        attack_info_used = ItemNameAttackInfo::none;
                        break;
                }
        }

        switch (attack_info_used) {
        case ItemNameAttackInfo::melee:
                return get_hit_mod_str(m_data->melee.hit_chance_mod);

        case ItemNameAttackInfo::ranged:
                return get_hit_mod_str(m_data->ranged.hit_chance_mod);

        case ItemNameAttackInfo::thrown:
                return get_hit_mod_str(m_data->ranged.throw_hit_chance_mod);

        case ItemNameAttackInfo::none:
                return "";

        case ItemNameAttackInfo::main_attack_mode:
                ASSERT(false);
                break;
        }

        return "";
}

std::string Item::dmg_str(
        const ItemNameAttackInfo attack_info,
        const ItemNameDmg dmg_value) const
{
        if (!m_data->allow_display_dmg) {
                return "";
        }

        std::string dmg_str;

        auto att_inf_used = attack_info;

        if (attack_info == ItemNameAttackInfo::main_attack_mode) {
                att_inf_used =
                        att_mode_to_name_att_info(
                                m_data->main_attack_mode);
        }

        switch (att_inf_used) {
        case ItemNameAttackInfo::melee: {
                if (m_base_melee_dmg.total_range().max > 0) {
                        const auto dmg_range = melee_dmg(map::g_player);

                        const auto str_avg = dmg_range.total_range().str_avg();

                        switch (dmg_value) {
                        case ItemNameDmg::average: {
                                dmg_str = str_avg;
                        } break;

                        case ItemNameDmg::range: {
                                dmg_str = dmg_range.total_range().str();
                        } break;
                        }
                }
        } break;

        case ItemNameAttackInfo::ranged: {
                if (m_base_ranged_dmg.total_range().max > 0) {
                        auto dmg_range = ranged_dmg(map::g_player);

                        if (m_data->ranged.is_machine_gun) {
                                const int n = g_nr_mg_projectiles;

                                const int min = n * dmg_range.base_min();
                                const int max = n * dmg_range.base_max();
                                const int plus = n * dmg_range.plus();

                                dmg_range = WpnDmg(min, max, plus);
                        }

                        if (dmg_value == ItemNameDmg::average) {
                                dmg_str = dmg_range.total_range().str_avg();
                        }
                        else {
                                dmg_str = dmg_range.total_range().str();
                        }
                }
        } break;

        case ItemNameAttackInfo::thrown: {
                // Print damage if non-zero throwing damage, or melee weapon
                // with non zero melee damage (melee weapons use melee damage
                // when thrown)
                if ((m_data->ranged.dmg.total_range().max > 0) ||
                    ((m_data->main_attack_mode == AttackMode::melee) &&
                     (m_base_melee_dmg.total_range().max > 0))) {
                        // NOTE: "thrown_dmg" will return melee damage if this
                        // is primarily a melee weapon
                        const auto dmg_range = thrown_dmg(map::g_player);

                        const std::string str_avg =
                                dmg_range.total_range().str_avg();

                        switch (dmg_value) {
                        case ItemNameDmg::average: {
                                dmg_str = dmg_range.total_range().str_avg();
                        } break;

                        case ItemNameDmg::range: {
                                dmg_str = dmg_range.total_range().str();
                        } break;
                        }
                }
        } break;

        case ItemNameAttackInfo::none:
                break;

        case ItemNameAttackInfo::main_attack_mode:
                ASSERT(false);
                break;
        }

        return dmg_str;
}

std::string Item::plus_str(const ItemNameAttackInfo attack_info) const
{
        if (!m_data->allow_display_dmg) {
                return "";
        }

        std::string plus_str;

        auto att_inf_used = attack_info;

        if (attack_info == ItemNameAttackInfo::main_attack_mode) {
                att_inf_used =
                        att_mode_to_name_att_info(
                                m_data->main_attack_mode);
        }

        switch (att_inf_used) {
        case ItemNameAttackInfo::melee: {
                if (m_base_melee_dmg.total_range().max > 0) {
                        return m_base_melee_dmg.str_plus();
                }
        } break;

        // "Plus" damage as a stat on non-melee_weapons is currently not
        // supported (they can do extra damage with traits, but the weapon
        // itself cannot have a "plus" damage).
        case ItemNameAttackInfo::ranged:
        case ItemNameAttackInfo::thrown:
        case ItemNameAttackInfo::none: {
                return "";
        } break;

        case ItemNameAttackInfo::main_attack_mode:
                break;
        }

        ASSERT(false);
        return "";
}

void Item::add_carrier_prop(prop::Prop* const prop, const Verbose verbose)
{
        ASSERT(m_actor_carrying);
        ASSERT(prop);

        m_actor_carrying->m_properties
                .add_prop_from_equipped_item(
                        this,
                        prop,
                        verbose);
}

void Item::clear_carrier_props()
{
        ASSERT(m_actor_carrying);

        m_actor_carrying->m_properties.remove_props_for_item(this);
}

int Item::armor_points() const
{
        // NOTE: AP must be able to reach zero, otherwise the armor will never
        // count as destroyed.

        const int ap_max = m_data->armor.armor_points;

        if (m_durability > 60) {
                return ap_max;
        }
        else if (m_durability > 40) {
                return std::max(0, ap_max - 1);
        }
        else if (m_durability > 25) {
                return std::max(0, ap_max - 2);
        }
        else if (m_durability > 15) {
                return std::max(0, ap_max - 3);
        }

        return 0;
}

}  // namespace item

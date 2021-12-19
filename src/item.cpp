// =============================================================================
// Copyright 2011-2021 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include <algorithm>
#include <cstddef>
#include <memory>
#include <ostream>
#include <string>

#include "actor.hpp"
#include "actor_data.hpp"
#include "actor_eat.hpp"
#include "actor_factory.hpp"
#include "actor_hit.hpp"
#include "actor_player.hpp"
#include "actor_see.hpp"
#include "array2.hpp"
#include "audio_data.hpp"
#include "colors.hpp"
#include "common_text.hpp"
#include "config.hpp"
#include "debug.hpp"
#include "explosion.hpp"
#include "game.hpp"
#include "game_over.hpp"
#include "game_time.hpp"
#include "global.hpp"
#include "inventory.hpp"
#include "item.hpp"
#include "item_curse.hpp"
#include "item_data.hpp"
#include "item_factory.hpp"
#include "map.hpp"
#include "msg_log.hpp"
#include "player_bon.hpp"
#include "pos.hpp"
#include "property.hpp"
#include "property_data.hpp"
#include "property_factory.hpp"
#include "property_handler.hpp"
#include "query.hpp"
#include "random.hpp"
#include "saving.hpp"
#include "sound.hpp"
#include "state.hpp"
#include "terrain.hpp"
#include "terrain_data.hpp"
#include "terrain_mob.hpp"
#include "text_format.hpp"

struct ItemAttProp;

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static ItemNameAttackInfo att_mode_to_name_att_info(AttackMode att_mode)
{
        switch (att_mode)
        {
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
        m_base_ranged_dmg(item_data->ranged.dmg)
{
}

Item& Item::operator=(const Item& other)
{
        if (&other == this)
        {
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
        for (auto* prop : m_carrier_props)
        {
                delete prop;
        }
}

Id Item::id() const
{
        return m_data->id;
}

void Item::save()
{
        m_curse.save();

        save_hook();
}

void Item::load()
{
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

        if (m_curse.is_active())
        {
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

        if (range.total_range().max == 0)
        {
                return range;
        }

        if (actor::is_player(attacker))
        {
                if (player_bon::has_trait(Trait::adept_melee))
                {
                        range.set_plus(range.plus() + 1);
                }

                if (player_bon::has_trait(Trait::expert_melee))
                {
                        range.set_plus(range.plus() + 1);
                }

                if (player_bon::has_trait(Trait::master_melee))
                {
                        range.set_plus(range.plus() + 1);
                }

                // TODO: This should be handled via the 'specific_dmg_mod' hook
                if (id() == Id::player_ghoul_claw)
                {
                        if (player_bon::has_trait(Trait::foul))
                        {
                                range.set_plus(range.plus() + 1);
                        }

                        if (player_bon::has_trait(Trait::toxic))
                        {
                                range.set_plus(range.plus() + 1);
                        }
                }
        }

        // Bonus damage from being frenzied?
        if (attacker && attacker->m_properties.has(PropId::frenzied))
        {
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

        if (range.total_range().max == 0)
        {
                return range;
        }

        specific_dmg_mod(range, attacker);

        return range;
}

ItemAttackProp& Item::prop_applied_on_melee(
        const actor::Actor* const attacker) const
{
        auto* const intr = prop_applied_intr_attack(attacker);

        if (intr)
        {
                return *intr;
        }

        return data().melee.prop_applied;
}

ItemAttackProp& Item::prop_applied_on_ranged(
        const actor::Actor* const attacker) const
{
        auto* const intr = prop_applied_intr_attack(attacker);

        if (intr)
        {
                return *intr;
        }

        return data().ranged.prop_applied;
}

ItemAttackProp* Item::prop_applied_intr_attack(
        const actor::Actor* const attacker) const
{
        if (attacker)
        {
                const auto& intr_attacks = attacker->m_data->intr_attacks;

                for (const auto& att : intr_attacks)
                {
                        if (att->item_id == id())
                        {
                                return &att->prop_applied;
                        }
                }
        }

        return nullptr;
}

int Item::weight() const
{
        auto w = (int)m_data->weight * m_nr_items;

        if (m_curse.is_active())
        {
                w = m_curse.affect_weight(w);
        }

        return w;
}

std::string Item::weight_str() const
{
        const int wgt = weight();

        if (wgt <= ((int)Weight::extra_light + (int)Weight::light) / 2)
        {
                return "very light";
        }

        if (wgt <= ((int)Weight::light + (int)Weight::medium) / 2)
        {
                return "light";
        }

        if (wgt <= ((int)Weight::medium + (int)Weight::heavy) / 2)
        {
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

        if (actor::is_player(m_actor_carrying))
        {
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

        if (actor::is_player(m_actor_carrying))
        {
                on_player_found();
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

void Item::on_player_found()
{
        if ((m_data->xp_on_found > 0) && !m_data->is_found)
        {
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
            (!m_data->is_stackable || (m_nr_items == 1)))
        {
                name_type_used = ItemNameType::a;
        }

        std::string nr_str;

        if (name_type_used == ItemNameType::plural)
        {
                nr_str = std::to_string(m_nr_items);
        }

        auto dmg_str = this->dmg_str(attack_info, ItemNameDmg::average);
        auto hit_str = hit_mod_str(attack_info, AbbrevItemAttackInfo::no);

        std::string info_str;

        if (info == ItemNameInfo::yes)
        {
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

        if (!dmg_str.empty() || !hit_str.empty())
        {
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

                if (hit_mod >= 0)
                {
                        str = "+";
                }

                str += std::to_string(hit_mod) + "%";

                if (abbrev == AbbrevItemAttackInfo::no)
                {
                        str += " hit";
                }

                return str;
        };

        auto attack_info_used = attack_info;

        // If caller requested attack info depending on main attack mode, set
        // the attack info used to a specific type
        if (attack_info == ItemNameAttackInfo::main_attack_mode)
        {
                switch (m_data->main_attack_mode)
                {
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

        switch (attack_info_used)
        {
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
        if (!m_data->allow_display_dmg)
        {
                return "";
        }

        std::string dmg_str;

        auto att_inf_used = attack_info;

        if (attack_info == ItemNameAttackInfo::main_attack_mode)
        {
                att_inf_used =
                        att_mode_to_name_att_info(
                                m_data->main_attack_mode);
        }

        switch (att_inf_used)
        {
        case ItemNameAttackInfo::melee:
        {
                if (m_base_melee_dmg.total_range().max > 0)
                {
                        const auto dmg_range = melee_dmg(map::g_player);

                        const auto str_avg = dmg_range.total_range().str_avg();

                        switch (dmg_value)
                        {
                        case ItemNameDmg::average:
                        {
                                dmg_str = str_avg;
                        }
                        break;

                        case ItemNameDmg::range:
                        {
                                dmg_str = dmg_range.total_range().str();
                        }
                        break;
                        }
                }
        }
        break;

        case ItemNameAttackInfo::ranged:
        {
                if (m_base_ranged_dmg.total_range().max > 0)
                {
                        auto dmg_range = ranged_dmg(map::g_player);

                        if (m_data->ranged.is_machine_gun)
                        {
                                const int n = g_nr_mg_projectiles;

                                const int min = n * dmg_range.base_min();
                                const int max = n * dmg_range.base_max();
                                const int plus = n * dmg_range.plus();

                                dmg_range = WpnDmg(min, max, plus);
                        }

                        if (dmg_value == ItemNameDmg::average)
                        {
                                dmg_str = dmg_range.total_range().str_avg();
                        }
                        else
                        {
                                dmg_str = dmg_range.total_range().str();
                        }
                }
        }
        break;

        case ItemNameAttackInfo::thrown:
        {
                // Print damage if non-zero throwing damage, or melee weapon
                // with non zero melee damage (melee weapons use melee damage
                // when thrown)
                if ((m_data->ranged.dmg.total_range().max > 0) ||
                    ((m_data->main_attack_mode == AttackMode::melee) &&
                     (m_base_melee_dmg.total_range().max > 0)))
                {
                        // NOTE: "thrown_dmg" will return melee damage if this
                        // is primarily a melee weapon
                        const auto dmg_range = thrown_dmg(map::g_player);

                        const std::string str_avg =
                                dmg_range.total_range().str_avg();

                        switch (dmg_value)
                        {
                        case ItemNameDmg::average:
                        {
                                dmg_str = dmg_range.total_range().str_avg();
                        }
                        break;

                        case ItemNameDmg::range:
                        {
                                dmg_str = dmg_range.total_range().str();
                        }
                        break;
                        }
                }
        }
        break;

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
        if (!m_data->allow_display_dmg)
        {
                return "";
        }

        std::string plus_str;

        auto att_inf_used = attack_info;

        if (attack_info == ItemNameAttackInfo::main_attack_mode)
        {
                att_inf_used =
                        att_mode_to_name_att_info(
                                m_data->main_attack_mode);
        }

        switch (att_inf_used)
        {
        case ItemNameAttackInfo::melee:
        {
                if (m_base_melee_dmg.total_range().max > 0)
                {
                        return m_base_melee_dmg.str_plus();
                }
        }
        break;

        // "Plus" damage as a stat on non-melee_weapons is currently not
        // supported (they can do extra damage with traits, but the weapon
        // itself cannot have a "plus" damage).
        case ItemNameAttackInfo::ranged:
        case ItemNameAttackInfo::thrown:
        case ItemNameAttackInfo::none:
        {
                return "";
        }
        break;

        case ItemNameAttackInfo::main_attack_mode:
                break;
        }

        ASSERT(false);
        return "";
}

void Item::add_carrier_prop(Prop* const prop, const Verbose verbose)
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

// -----------------------------------------------------------------------------
// Trapezohedron
// -----------------------------------------------------------------------------
Trapez::Trapez(ItemData* item_data) :
        Item(item_data)
{
}

ItemPrePickResult Trapez::pre_pickup_hook()
{
        game::add_history_event("Beheld The Shining Trapezohedron");

        saving::erase_save();

        states::pop();

        on_game_over(IsWin::yes);

        states::push(std::make_unique<WinGameState>());

        return ItemPrePickResult::do_nothing;
}

// -----------------------------------------------------------------------------
// Armor
// -----------------------------------------------------------------------------
Armor::Armor(ItemData* const item_data) :
        Item(item_data),
        m_dur(rnd::range(80, 100)) {}

void Armor::save_hook() const
{
        saving::put_int(m_dur);
}

void Armor::load_hook()
{
        m_dur = saving::get_int();
}

int Armor::armor_points() const
{
        // NOTE: AP must be able to reach zero, otherwise the armor will never
        // count as destroyed.

        const int ap_max = m_data->armor.armor_points;

        if (m_dur > 60)
        {
                return ap_max;
        }

        if (m_dur > 40)
        {
                return std::max(0, ap_max - 1);
        }

        if (m_dur > 25)
        {
                return std::max(0, ap_max - 2);
        }

        if (m_dur > 15)
        {
                return std::max(0, ap_max - 3);
        }

        return 0;
}

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
            player_bon::is_bg(Bg::war_vet))
        {
                war_vet_k = 0.5;
        }

        m_dur -= (int)(dmg_db * df * k * war_vet_k);

        m_dur = std::max(0, m_dur);

        const int ap_after = armor_points();

        if ((ap_after < ap_before) &&
            (ap_after != 0))
        {
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

        auto* prop_r_fire = property_factory::make(PropId::r_fire);
        auto* prop_r_acid = property_factory::make(PropId::r_acid);
        auto* prop_r_elec = property_factory::make(PropId::r_elec);

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
        if (verbose == Verbose::yes)
        {
                msg_log::add(
                        "The armor joins with my skin!",
                        colors::text(),
                        MsgInterruptPlayer::no,
                        MorePromptOnMsg::yes);

                map::g_player->incr_shock(12.0, ShockSrc::use_strange_item);
        }
}

// -----------------------------------------------------------------------------
// Weapon
// -----------------------------------------------------------------------------
Wpn::Wpn(ItemData* const item_data) :
        Item(item_data),
        m_ammo_loaded(0),
        m_ammo_data(nullptr)
{
        const auto ammo_item_id = m_data->ranged.ammo_item_id;

        if (ammo_item_id != Id::END)
        {
                m_ammo_data = &item::g_data[(size_t)ammo_item_id];
                m_ammo_loaded = m_data->ranged.max_ammo;
        }
}

void Wpn::save_hook() const
{
        saving::put_int(m_base_melee_dmg.base_min());
        saving::put_int(m_base_melee_dmg.base_max());
        saving::put_int(m_base_melee_dmg.plus());

        saving::put_int(m_base_ranged_dmg.base_min());
        saving::put_int(m_base_ranged_dmg.base_max());
        saving::put_int(m_base_ranged_dmg.plus());

        saving::put_int(m_ammo_loaded);
}

void Wpn::load_hook()
{
        m_base_melee_dmg.set_base_min(saving::get_int());
        m_base_melee_dmg.set_base_max(saving::get_int());
        m_base_melee_dmg.set_plus(saving::get_int());

        m_base_ranged_dmg.set_base_min(saving::get_int());
        m_base_ranged_dmg.set_base_max(saving::get_int());
        m_base_ranged_dmg.set_plus(saving::get_int());

        m_ammo_loaded = saving::get_int();
}

Color Wpn::color() const
{
        if (m_data->ranged.is_ranged_wpn &&
            !m_data->ranged.has_infinite_ammo &&
            (m_ammo_loaded == 0))
        {
                return m_data->color.fraction(2.0);
        }

        return m_data->color;
}

std::string Wpn::name_info_str() const
{
        if (!m_data->ranged.is_ranged_wpn ||
            m_data->ranged.has_infinite_ammo)
        {
                return "";
        }

        return (
                "(" +
                std::to_string(m_ammo_loaded) +
                "/" +
                std::to_string(m_data->ranged.max_ammo) +
                ")");
}

// -----------------------------------------------------------------------------
// Spiked Mace
// -----------------------------------------------------------------------------
void SpikedMace::on_melee_hit(actor::Actor& actor_hit, const int dmg)
{
        (void)dmg;

        if (!actor_hit.is_alive())
        {
                return;
        }

        const int stun_pct = 25;

        if (rnd::percent(stun_pct))
        {
                auto* prop = new PropParalyzed();

                prop->set_duration(2);

                actor_hit.m_properties.apply(prop);
        }
}

// -----------------------------------------------------------------------------
// Player ghoul claw
// -----------------------------------------------------------------------------
void PlayerGhoulClaw::on_melee_hit(actor::Actor& actor_hit, const int dmg)
{
        (void)dmg;

        // TODO: If some "constructed" monster is added (something not made of
        // flesh, e.g. a golem), then a Ghoul player would be able to feed from
        // it, which would be a problem. In that case, there should probably be
        // a field in the actor data called something like either
        // "is_flesh_body", or "is_construct".

        // Ghoul feeding from Ravenous trait?

        // NOTE: Player should never feed on monsters such as Ghosts or Shadows.
        // Checking that the monster is not Ethereal and that it can bleed
        // should be a pretty good rule for this. We should NOT check if the
        // monster can leave a corpse however, since some monsters such as
        // Worms don't leave a corpse, and you SHOULD be able to feed on those.
        const auto& d = *actor_hit.m_data;

        const bool is_ethereal = actor_hit.m_properties.has(PropId::ethereal);

        const bool is_hp_missing =
                map::g_player->m_hp < actor::max_hp(*map::g_player);

        const bool is_wounded = map::g_player->m_properties.has(PropId::wound);

        const bool is_feed_needed = is_hp_missing || is_wounded;

        if (!is_ethereal &&
            d.can_bleed &&
            player_bon::has_trait(Trait::ravenous) &&
            is_feed_needed &&
            rnd::one_in(6))
        {
                Snd snd(
                        "",
                        audio::SfxId::bite,
                        IgnoreMsgIfOriginSeen::yes,
                        actor_hit.m_pos,
                        map::g_player,
                        SndVol::low,
                        AlertsMon::yes);

                snd.run();

                actor::heal_from_eating(*map::g_player);
        }

        if (actor_hit.is_alive())
        {
                // Poison victim from Ghoul Toxic trait?
                if (player_bon::has_trait(Trait::toxic) &&
                    rnd::fraction(3, 4))
                {
                        actor_hit.m_properties.apply(new PropPoisoned());
                }

                // Terrify victim from Ghoul Indomitable Fury trait?
                if (player_bon::has_trait(Trait::indomitable_fury) &&
                    map::g_player->m_properties.has(PropId::frenzied))
                {
                        actor_hit.m_properties.apply(new PropTerrified());
                }
        }
}

void PlayerGhoulClaw::on_melee_kill(actor::Actor& actor_killed)
{
        // TODO: See TODO note in melee hit hook for Ghoul claw concerning
        // "constructed monsters".

        const auto& d = *actor_killed.m_data;

        const bool is_ethereal =
                actor_killed.m_properties.has(PropId::ethereal);

        if (player_bon::has_trait(Trait::foul) &&
            !is_ethereal &&
            d.can_leave_corpse &&
            rnd::one_in(3))
        {
                actor::spawn(
                        actor_killed.m_pos,
                        {1, actor::Id::worm_mass},
                        map::rect())
                        .make_aware_of_player()
                        .set_leader(map::g_player);
        }
}

// -----------------------------------------------------------------------------
// Mi-go Electric Gun
// -----------------------------------------------------------------------------
MiGoGun::MiGoGun(ItemData* const item_data) :
        Wpn(item_data) {}

void MiGoGun::specific_dmg_mod(
        WpnDmg& range,
        const actor::Actor* const actor) const
{
        if (actor::is_player(actor) &&
            player_bon::has_trait(Trait::elec_incl))
        {
                range.set_plus(range.plus() + 1);
        }
}

void MiGoGun::pre_ranged_attack()
{
        if (!actor::is_player(m_actor_carrying))
        {
                return;
        }

        const auto wpn_name = name(ItemNameType::plain, ItemNameInfo::none);

        const auto msg =
                "The " +
                wpn_name +
                " feeds on my energy!";

        msg_log::add(msg, colors::msg_bad());

        actor::hit(
                *m_actor_carrying,
                g_mi_go_gun_hp_drained,
                DmgType::pure,
                AllowWound::no);

        auto* disabled_regen =
                property_factory::make(PropId::disabled_hp_regen);

        disabled_regen->set_duration(
                rnd::range(
                        g_mi_go_gun_regen_disabled_min_turns,
                        g_mi_go_gun_regen_disabled_max_turns));

        m_actor_carrying->m_properties.apply(disabled_regen);
}

// -----------------------------------------------------------------------------
// Incinerator
// -----------------------------------------------------------------------------
Incinerator::Incinerator(ItemData* const item_data) :
        Wpn(item_data) {}

void Incinerator::on_projectile_blocked(const P& pos)
{
        explosion::run(pos, ExplType::expl);
}

// -----------------------------------------------------------------------------
// Raven peck
// -----------------------------------------------------------------------------
void RavenPeck::on_melee_hit(actor::Actor& actor_hit, const int dmg)
{
        (void)dmg;

        if (!actor_hit.is_alive())
        {
                return;
        }

        // Gas mask and Asbestos suit protects against blindness
        Item* const head_item = actor_hit.m_inv.item_in_slot(SlotId::head);
        Item* const body_item = actor_hit.m_inv.item_in_slot(SlotId::body);

        if ((head_item && head_item->id() == Id::gas_mask) ||
            (body_item && body_item->id() == Id::armor_asb_suit))
        {
                return;
        }

        if (rnd::coin_toss())
        {
                auto* const prop = property_factory::make(PropId::blind);

                prop->set_duration(2);

                actor_hit.m_properties.apply(prop);
        }
}

// -----------------------------------------------------------------------------
// Vampire Bat Bite
// -----------------------------------------------------------------------------
void VampiricBite::on_melee_hit(actor::Actor& actor_hit, const int dmg)
{
        if (!actor_hit.is_alive())
        {
                return;
        }

        m_actor_carrying->restore_hp(
                dmg,
                false,
                Verbose::yes);
}

// -----------------------------------------------------------------------------
// Mind Leech Sting
// -----------------------------------------------------------------------------
void MindLeechSting::on_melee_hit(actor::Actor& actor_hit, const int dmg)
{
        (void)dmg;

        if (!actor_hit.is_alive() ||
            !actor::is_player(&actor_hit))
        {
                return;
        }

        auto* const mon = m_actor_carrying;

        if (map::g_player->ins() >= 50 ||
            map::g_player->m_properties.has(PropId::confused) ||
            map::g_player->m_properties.has(PropId::frenzied))
        {
                const bool player_see_mon = actor::can_player_see_actor(*mon);

                if (player_see_mon)
                {
                        const std::string mon_name_the =
                                text_format::first_to_upper(mon->name_the());

                        msg_log::add(mon_name_the + " looks shocked!");
                }

                actor::hit(*mon, rnd::range(3, 15), DmgType::pure);

                if (mon->is_alive())
                {
                        mon->m_properties.apply(new PropConfused());

                        mon->m_properties.apply(new PropTerrified());
                }
        }
        else
        {
                // Player mind can be eaten
                auto* prop_mind_sap = new PropMindSap();

                prop_mind_sap->set_indefinite();

                map::g_player->m_properties.apply(prop_mind_sap);

                // Make the monster pause, so things don't get too crazy
                auto* prop_waiting = new PropWaiting();

                prop_waiting->set_duration(2);

                mon->m_properties.apply(prop_waiting);
        }
}

// -----------------------------------------------------------------------------
// Dust vortex enguld
// -----------------------------------------------------------------------------
void DustEngulf::on_melee_hit(actor::Actor& actor_hit, const int dmg)
{
        (void)dmg;

        if (!actor_hit.is_alive())
        {
                return;
        }

        // Gas mask and Asbestos suit protects against blindness
        Item* const head_item = actor_hit.m_inv.item_in_slot(SlotId::head);
        Item* const body_item = actor_hit.m_inv.item_in_slot(SlotId::body);

        if ((head_item && head_item->id() == Id::gas_mask) ||
            (body_item && body_item->id() == Id::armor_asb_suit))
        {
                return;
        }

        auto* const prop = property_factory::make(PropId::blind);

        actor_hit.m_properties.apply(prop);
}

// -----------------------------------------------------------------------------
// Spitting cobra spit
// -----------------------------------------------------------------------------
void SnakeVenomSpit::on_ranged_hit(actor::Actor& actor_hit)
{
        if (!actor_hit.is_alive())
        {
                return;
        }

        // Gas mask and Asbestos suit protects against blindness
        Item* const head_item = actor_hit.m_inv.item_in_slot(SlotId::head);
        Item* const body_item = actor_hit.m_inv.item_in_slot(SlotId::body);

        if ((head_item && head_item->id() == Id::gas_mask) ||
            (body_item && body_item->id() == Id::armor_asb_suit))
        {
                return;
        }

        auto* const prop = property_factory::make(PropId::blind);

        prop->set_duration(7);

        actor_hit.m_properties.apply(prop);
}

// -----------------------------------------------------------------------------
// Ammo mag
// -----------------------------------------------------------------------------
AmmoMag::AmmoMag(ItemData* const item_data) :
        Ammo(item_data)
{
        set_full_ammo();
}

void AmmoMag::save_hook() const
{
        saving::put_int(m_ammo);
}

void AmmoMag::load_hook()
{
        m_ammo = saving::get_int();
}

void AmmoMag::set_full_ammo()
{
        m_ammo = m_data->ranged.max_ammo;
}

std::string AmmoMag::name_info_str() const
{
        return "(" + std::to_string(m_ammo) + ")";
}

// -----------------------------------------------------------------------------
// Medical bag
// -----------------------------------------------------------------------------
MedicalBag::MedicalBag(ItemData* const item_data) :
        Item(item_data),
        m_nr_supplies(24),
        m_nr_turns_left_action(-1),
        m_current_action(MedBagAction::END) {}

void MedicalBag::save_hook() const
{
        saving::put_int(m_nr_supplies);
}

void MedicalBag::load_hook()
{
        m_nr_supplies = saving::get_int();
}

void MedicalBag::on_pickup_hook()
{
        ASSERT(m_actor_carrying);

        if (!actor::is_player(m_actor_carrying))
        {
                return;
        }

        // Check for existing medical bag in inventory
        for (Item* const other : m_actor_carrying->m_inv.m_backpack)
        {
                if ((other != this) &&
                    (other->id() == id()))
                {
                        static_cast<MedicalBag*>(other)->m_nr_supplies +=
                                m_nr_supplies;

                        m_actor_carrying->m_inv
                                .remove_item_in_backpack_with_ptr(this, true);

                        return;
                }
        }
}

ConsumeItem MedicalBag::activate(actor::Actor* const actor)
{
        (void)actor;

        if (player_bon::bg() == Bg::ghoul)
        {
                msg_log::add("It is of no use to me.");

                m_current_action = MedBagAction::END;

                return ConsumeItem::no;
        }

        // OK, we are allowed to use the medical bag

        m_current_action = choose_action();

        if (m_current_action == MedBagAction::END)
        {
                msg_log::clear();

                msg_log::add("I have no wounds to treat.");

                return ConsumeItem::no;
        }

        const int nr_supplies_needed = tot_suppl_for_action(m_current_action);

        const bool is_enough_supplies = m_nr_supplies >= nr_supplies_needed;

        if (!is_enough_supplies)
        {
                msg_log::add("I do not have enough medical supplies.");

                m_current_action = MedBagAction::END;

                return ConsumeItem::no;
        }

        // Action can be done
        map::g_player->m_active_medical_bag = this;

        m_nr_turns_left_action = tot_turns_for_action();

        std::string start_msg;

        switch (m_current_action)
        {
        case MedBagAction::treat_wound:
                start_msg = "I start treating a wound";
                break;

        case MedBagAction::sanitize_infection:
                start_msg = "I start to sanitize an infection";
                break;

        case MedBagAction::END:
                ASSERT(false);
                break;
        }

        start_msg +=
                " (" +
                std::to_string(m_nr_turns_left_action) +
                " turns)...";

        msg_log::add(start_msg);

        game_time::tick();

        return ConsumeItem::no;
}

MedBagAction MedicalBag::choose_action() const
{
        // Infection?
        if (map::g_player->m_properties.has(PropId::infected))
        {
                return MedBagAction::sanitize_infection;
        }

        // Wound?
        if (map::g_player->m_properties.has(PropId::wound))
        {
                return MedBagAction::treat_wound;
        }

        return MedBagAction::END;
}

void MedicalBag::continue_action()
{
        ASSERT(m_current_action != MedBagAction::END);

        --m_nr_turns_left_action;

        if (m_nr_turns_left_action <= 0)
        {
                finish_current_action();
        }
        else
        {
                // Time still remaining on the current action
                game_time::tick();
        }
}

void MedicalBag::finish_current_action()
{
        map::g_player->m_active_medical_bag = nullptr;

        switch (m_current_action)
        {
        case MedBagAction::treat_wound:
        {
                Prop* const wound_prop =
                        map::g_player->m_properties.prop(PropId::wound);

                ASSERT(wound_prop);

                auto* const wound = static_cast<PropWound*>(wound_prop);

                wound->heal_one_wound();
        }
        break;

        case MedBagAction::sanitize_infection:
        {
                map::g_player->m_properties.end_prop(PropId::infected);
        }
        break;

        case MedBagAction::END:
                ASSERT(false);
                break;
        }

        m_nr_supplies -= tot_suppl_for_action(m_current_action);

        m_current_action = MedBagAction::END;

        if (m_nr_supplies <= 0)
        {
                map::g_player->m_inv
                        .remove_item_in_backpack_with_ptr(this, true);

                game::add_history_event("Ran out of medical supplies");
        }
}

void MedicalBag::interrupted(const ForceInterruptActions is_forced)
{
        bool should_continue = true;

        if (is_forced == ForceInterruptActions::no)
        {
                // Query interruption.
                const std::string item_name =
                        name(
                                ItemNameType::plain,
                                ItemNameInfo::none);

                const std::string msg =
                        "Continue using " +
                        item_name +
                        " (" +
                        std::to_string(m_nr_turns_left_action) +
                        " turns left)? " +
                        common_text::g_yes_or_no_hint;

                msg_log::add(
                        msg,
                        colors::light_white(),
                        MsgInterruptPlayer::no,
                        MorePromptOnMsg::no,
                        CopyToMsgHistory::no);

                should_continue =
                        (query::yes_or_no(
                                 std::nullopt,
                                 AllowSpaceCancel::no) ==
                         BinaryAnswer::yes);

                msg_log::clear();
        }
        else
        {
                // Forced interruption.
                msg_log::add("My healing is disrupted.");

                should_continue = false;
        }

        if (!should_continue)
        {
                m_current_action = MedBagAction::END;

                m_nr_turns_left_action = -1;

                map::g_player->m_active_medical_bag = nullptr;
        }
}

int MedicalBag::tot_suppl_for_action(const MedBagAction action) const
{
        const bool is_healer = player_bon::has_trait(Trait::healer);

        const int div = is_healer ? 2 : 1;

        switch (action)
        {
        case MedBagAction::treat_wound:
                return 8 / div;

        case MedBagAction::sanitize_infection:
                return 2 / div;

        case MedBagAction::END:
                break;
        }

        ASSERT(false);

        return 0;
}

int MedicalBag::tot_turns_for_action() const
{
        int nr_turns = 10;

        if (player_bon::has_trait(Trait::healer))
        {
                nr_turns /= 2;
        }

        return nr_turns;
}

std::string MedicalBag::name_info_str() const
{
        return "(" + std::to_string(m_nr_supplies) + " supplies)";
}

// -----------------------------------------------------------------------------
// Gas mask
// -----------------------------------------------------------------------------
void GasMask::on_equip_hook(const Verbose verbose)
{
        (void)verbose;
}

void GasMask::on_unequip_hook()
{
        clear_carrier_props();
}

void GasMask::decr_turns_left(Inventory& carrier_inv)
{
        --m_nr_turns_left;

        if (m_nr_turns_left <= 0)
        {
                const std::string item_name =
                        name(ItemNameType::plain, ItemNameInfo::none);

                msg_log::add(
                        "My " + item_name + " expires.",
                        colors::msg_note(),
                        MsgInterruptPlayer::yes,
                        MorePromptOnMsg::yes);

                carrier_inv.decr_item(this);
        }
}

std::string GasMask::name_info_str() const
{
        return "(" + std::to_string(m_nr_turns_left) + " turns)";
}

// -----------------------------------------------------------------------------
// Explosive
// -----------------------------------------------------------------------------
ConsumeItem Explosive::activate(actor::Actor* const actor)
{
        (void)actor;

        if (map::g_player->m_properties.has(PropId::burning))
        {
                msg_log::add("Not while burning.");

                return ConsumeItem::no;
        }

        const Explosive* const held_explosive =
                map::g_player->m_active_explosive;

        if (held_explosive)
        {
                const std::string name_held =
                        held_explosive->name(
                                ItemNameType::a,
                                ItemNameInfo::none);

                msg_log::add("I am already holding " + name_held + ".");

                return ConsumeItem::no;
        }

        if (config::warn_on_light_explosive())
        {
                const std::string name = this->name(ItemNameType::a);

                const std::string msg =
                        "Light " +
                        name +
                        "? " +
                        common_text::g_yes_or_no_hint;

                msg_log::add(
                        msg,
                        colors::light_white(),
                        MsgInterruptPlayer::no,
                        MorePromptOnMsg::no,
                        CopyToMsgHistory::no);

                auto result = query::yes_or_no();

                msg_log::clear();

                if (result == BinaryAnswer::no)
                {
                        return ConsumeItem::no;
                }
        }

        // Make a copy to use as the held ignited explosive.
        auto* cpy = static_cast<Explosive*>(item::make(data().id, 1));

        cpy->m_fuse_turns = std_fuse_turns();

        map::g_player->m_active_explosive = cpy;

        cpy->on_player_ignite();

        return ConsumeItem::yes;
}

// -----------------------------------------------------------------------------
// Dynamite
// -----------------------------------------------------------------------------
void Dynamite::on_player_ignite() const
{
        msg_log::add("I light a dynamite stick.");

        game_time::tick();
}

void Dynamite::on_std_turn_player_hold_ignited()
{
        --m_fuse_turns;

        if (m_fuse_turns > 0)
        {
                std::string fuse_msg = "***F";

                for (int i = 0; i < m_fuse_turns; ++i)
                {
                        fuse_msg += "Z";
                }

                fuse_msg += "***";

                const auto more_prompt =
                        (m_fuse_turns <= 2)
                        ? MorePromptOnMsg::yes
                        : MorePromptOnMsg::no;

                msg_log::add(
                        fuse_msg,
                        colors::yellow(),
                        MsgInterruptPlayer::yes,
                        more_prompt);
        }
        else
        {
                // Fuse has run out
                msg_log::add("The dynamite explodes in my hand!");

                map::g_player->m_active_explosive = nullptr;

                explosion::run(map::g_player->m_pos, ExplType::expl);

                m_fuse_turns = -1;

                delete this;
        }
}

void Dynamite::on_thrown_ignited_landing(const P& p)
{
        game_time::add_mob(new terrain::LitDynamite(p, m_fuse_turns));
}

void Dynamite::on_player_paralyzed()
{
        msg_log::add("The lit Dynamite stick falls from my hand!");

        map::g_player->m_active_explosive = nullptr;

        const auto& p = map::g_player->m_pos;

        const auto t_id = map::g_terrain.at(p)->id();

        if (t_id != terrain::Id::chasm)
        {
                game_time::add_mob(new terrain::LitDynamite(p, m_fuse_turns));
        }

        delete this;
}

// -----------------------------------------------------------------------------
// Molotov
// -----------------------------------------------------------------------------
void Molotov::on_player_ignite() const
{
        msg_log::add("I light a Molotov Cocktail.");

        game_time::tick();
}

void Molotov::on_std_turn_player_hold_ignited()
{
        --m_fuse_turns;

        if (m_fuse_turns == 2)
        {
                msg_log::add(
                        "The Molotov Cocktail will soon explode.",
                        colors::text(),
                        MsgInterruptPlayer::no,
                        MorePromptOnMsg::yes);
        }

        if (m_fuse_turns == 1)
        {
                msg_log::add(
                        "The Molotov Cocktail is about to explode!",
                        colors::text(),
                        MsgInterruptPlayer::yes,
                        MorePromptOnMsg::yes);
        }

        if (m_fuse_turns <= 0)
        {
                msg_log::add("The Molotov Cocktail explodes in my hand!");

                map::g_player->m_active_explosive = nullptr;

                const P player_pos = map::g_player->m_pos;

                Snd snd(
                        "I hear an explosion!",
                        audio::SfxId::explosion_molotov,
                        IgnoreMsgIfOriginSeen::yes,
                        player_pos,
                        nullptr,
                        SndVol::high,
                        AlertsMon::yes);

                snd.run();

                explosion::run(
                        player_pos,
                        ExplType::apply_prop,
                        EmitExplSnd::no,
                        0,
                        ExplExclCenter::no,
                        {new PropBurning()});

                delete this;
        }
}

void Molotov::on_thrown_ignited_landing(const P& p)
{
        Snd snd(
                "I hear an explosion!",
                audio::SfxId::explosion_molotov,
                IgnoreMsgIfOriginSeen::yes,
                p,
                nullptr,
                SndVol::high,
                AlertsMon::yes);

        snd.run();

        explosion::run(
                p,
                ExplType::apply_prop,
                EmitExplSnd::no,
                0,
                ExplExclCenter::no,
                {new PropBurning()});
}

void Molotov::on_player_paralyzed()
{
        msg_log::add("The lit Molotov Cocktail falls from my hand!");

        map::g_player->m_active_explosive = nullptr;

        const P player_pos = map::g_player->m_pos;

        Snd snd(
                "I hear an explosion!",
                audio::SfxId::explosion_molotov,
                IgnoreMsgIfOriginSeen::yes,
                player_pos,
                nullptr,
                SndVol::high,
                AlertsMon::yes);

        snd.run();

        explosion::run(
                player_pos,
                ExplType::apply_prop,
                EmitExplSnd::no,
                0,
                ExplExclCenter::no,
                {new PropBurning()});

        delete this;
}

// -----------------------------------------------------------------------------
// Flare
// -----------------------------------------------------------------------------
void Flare::on_player_ignite() const
{
        msg_log::add("I light a Flare.");

        game_time::tick();
}

void Flare::on_std_turn_player_hold_ignited()
{
        --m_fuse_turns;

        if (m_fuse_turns <= 0)
        {
                msg_log::add("The flare is extinguished.");

                map::g_player->m_active_explosive = nullptr;

                delete this;
        }
}

void Flare::on_thrown_ignited_landing(const P& p)
{
        game_time::add_mob(new terrain::LitFlare(p, m_fuse_turns));
}

void Flare::on_player_paralyzed()
{
        msg_log::add("The lit Flare falls from my hand!");

        map::g_player->m_active_explosive = nullptr;

        const auto& p = map::g_player->m_pos;

        const auto t_id = map::g_terrain.at(p)->id();

        if (t_id != terrain::Id::chasm)
        {
                game_time::add_mob(new terrain::LitFlare(p, m_fuse_turns));
        }

        delete this;
}

// -----------------------------------------------------------------------------
// Smoke grenade
// -----------------------------------------------------------------------------
void SmokeGrenade::on_player_ignite() const
{
        msg_log::add("I ignite a smoke grenade.");

        game_time::tick();
}

void SmokeGrenade::on_std_turn_player_hold_ignited()
{
        if (m_fuse_turns < std_fuse_turns() && rnd::coin_toss())
        {
                explosion::run_smoke_explosion_at(map::g_player->m_pos);
        }

        --m_fuse_turns;

        if (m_fuse_turns <= 0)
        {
                msg_log::add("The smoke grenade is extinguished.");

                map::g_player->m_active_explosive = nullptr;

                delete this;
        }
}

void SmokeGrenade::on_thrown_ignited_landing(const P& p)
{
        explosion::run_smoke_explosion_at(p, 0);
}

void SmokeGrenade::on_player_paralyzed()
{
        msg_log::add("The ignited smoke grenade falls from my hand!");

        map::g_player->m_active_explosive = nullptr;

        const P& p = map::g_player->m_pos;

        const auto t_id = map::g_terrain.at(p)->id();

        if (t_id != terrain::Id::chasm)
        {
                explosion::run_smoke_explosion_at(map::g_player->m_pos);
        }

        delete this;
}

Color SmokeGrenade::ignited_projectile_color() const
{
        return data().color;
}

}  // namespace item

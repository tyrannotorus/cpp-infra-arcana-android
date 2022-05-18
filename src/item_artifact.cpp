// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include <cmath>
#include <memory>
#include <vector>

#include "actor.hpp"
#include "actor_data.hpp"
#include "actor_hit.hpp"
#include "actor_player.hpp"
#include "actor_see.hpp"
#include "array2.hpp"
#include "audio_data.hpp"
#include "colors.hpp"
#include "common_text.hpp"
#include "debug.hpp"
#include "draw_blast.hpp"
#include "fov.hpp"
#include "game.hpp"
#include "game_time.hpp"
#include "io.hpp"
#include "item_artifact.hpp"
#include "item_data.hpp"
#include "item_factory.hpp"
#include "item_scroll.hpp"
#include "map.hpp"
#include "map_parsing.hpp"
#include "msg_log.hpp"
#include "player_spells.hpp"
#include "property.hpp"
#include "property_data.hpp"
#include "property_factory.hpp"
#include "property_handler.hpp"
#include "random.hpp"
#include "saving.hpp"
#include "spells.hpp"
#include "text_format.hpp"
#include "wpn_dmg.hpp"

// -----------------------------------------------------------------------------
// item
// -----------------------------------------------------------------------------
namespace item
{
struct ItemData;

// -----------------------------------------------------------------------------
// Staff of the pharaohs
// -----------------------------------------------------------------------------
PharaohStaff::PharaohStaff(ItemData* const item_data) :
        Wpn(item_data)
{
}

void PharaohStaff::on_std_turn_in_inv_hook(const InvType inv_type)
{
        (void)inv_type;

        if (!actor::is_player(actor_carrying()))
        {
                return;
        }

        Array2<bool> blocked_los(map::dims());

        map_parsers::BlocksLos()
                .run(blocked_los,
                     fov::fov_rect(map::g_player->m_pos, map::dims()),
                     MapParseMode::overwrite);

        for (auto* const actor : game_time::g_actors)
        {
                if (actor::is_player(actor) || !actor->is_alive())
                {
                        continue;
                }

                if (!actor->is_aware_of_player())
                {
                        continue;
                }

                const bool mon_see_player =
                        actor::can_mon_see_actor(
                                *actor,
                                *map::g_player,
                                blocked_los);

                if (!mon_see_player)
                {
                        continue;
                }

                on_mon_see_player_carrying(*actor);
        }
}

void PharaohStaff::on_mon_see_player_carrying(actor::Actor& mon) const
{
        // TODO: Consider an "is_mummy" actor data field
        if ((mon.id() != actor::Id::mummy) &&
            (mon.id() != actor::Id::croc_head_mummy))
        {
                return;
        }

        if (mon.is_actor_my_leader(map::g_player))
        {
                return;
        }

        const int convert_pct_chance = 10;

        if (rnd::percent(convert_pct_chance))
        {
                mon.m_leader = map::g_player;

                if (actor::can_player_see_actor(mon))
                {
                        const auto name_the =
                                text_format::first_to_upper(
                                        mon.name_the());

                        msg_log::add(name_the + " bows before me.");
                }
        }
}

void PharaohStaff::on_melee_hit(actor::Actor& actor_hit, const int dmg)
{
        (void)dmg;

        if (!actor_hit.is_alive())
        {
                return;
        }

        const int doomed_pct = 50;

        if (rnd::percent(doomed_pct))
        {
                auto* prop = property_factory::make(PropId::doomed);

                prop->set_duration(rnd::range(3, 4));

                actor_hit.m_properties.apply(prop);
        }
}

// -----------------------------------------------------------------------------
// Talisman of Reflection
// -----------------------------------------------------------------------------
ReflTalisman::ReflTalisman(ItemData* const item_data) :
        Item(item_data)
{
}

void ReflTalisman::on_pickup_hook()
{
        auto* prop = property_factory::make(PropId::spell_reflect);

        prop->set_indefinite();

        add_carrier_prop(prop, Verbose::no);
}

void ReflTalisman::on_removed_from_inv_hook()
{
        clear_carrier_props();
}

// -----------------------------------------------------------------------------
// Talisman of Resurrection
// -----------------------------------------------------------------------------
ResurrectTalisman::ResurrectTalisman(ItemData* const item_data) :
        Item(item_data)
{
}

// -----------------------------------------------------------------------------
// Talisman of Teleporation Control
// -----------------------------------------------------------------------------
TeleCtrlTalisman::TeleCtrlTalisman(ItemData* const item_data) :
        Item(item_data)
{
}

void TeleCtrlTalisman::on_pickup_hook()
{
        auto* prop = property_factory::make(PropId::tele_ctrl);

        prop->set_indefinite();

        add_carrier_prop(prop, Verbose::no);
}

void TeleCtrlTalisman::on_removed_from_inv_hook()
{
        clear_carrier_props();
}

// -----------------------------------------------------------------------------
// Horn of Malice
// -----------------------------------------------------------------------------
void HornOfMaliceHeard::run(actor::Actor& actor) const
{
        if (!actor::is_player(&actor))
        {
                actor.m_properties.apply(
                        property_factory::make(PropId::conflict));
        }
}

HornOfMalice::HornOfMalice(ItemData* const item_data) :
        Item(item_data),
        m_charges(rnd::range(4, 6))
{
}

std::string HornOfMalice::name_info_str() const
{
        return "(" + std::to_string(m_charges) + " uses)";
}

void HornOfMalice::save_hook() const
{
        saving::put_int(m_charges);
}

void HornOfMalice::load_hook()
{
        m_charges = saving::get_int();
}

ConsumeItem HornOfMalice::activate(actor::Actor* const actor)
{
        (void)actor;

        if (m_charges <= 0)
        {
                msg_log::add("It makes no sound.");

                return ConsumeItem::no;
        }

        auto effect =
                std::shared_ptr<SndHeardEffect>(
                        new HornOfMaliceHeard);

        Snd snd(
                "The Horn of Malice resounds!",
                audio::SfxId::END,  // TODO: Make a sound effect
                IgnoreMsgIfOriginSeen::no,
                map::g_player->m_pos,
                map::g_player,
                SndVol::high,
                AlertsMon::yes,
                effect);

        snd.run();

        --m_charges;

        game_time::tick();

        return ConsumeItem::no;
}

// -----------------------------------------------------------------------------
// Horn of Banishment
// -----------------------------------------------------------------------------
void HornOfBanishmentHeard::run(actor::Actor& actor) const
{
        if (actor.m_properties.has(PropId::summoned))
        {
                if (actor::can_player_see_actor(actor))
                {
                        const std::string name_the =
                                text_format::first_to_upper(
                                        actor.name_the());

                        msg_log::add(
                                name_the +
                                " " +
                                common_text::g_mon_disappear);
                }

                actor.m_state = ActorState::destroyed;
        }
}

HornOfBanishment::HornOfBanishment(ItemData* const item_data) :
        Item(item_data),
        m_charges(rnd::range(4, 6))
{
}

std::string HornOfBanishment::name_info_str() const
{
        return "(" + std::to_string(m_charges) + " uses)";
}

void HornOfBanishment::save_hook() const
{
        saving::put_int(m_charges);
}

void HornOfBanishment::load_hook()
{
        m_charges = saving::get_int();
}

ConsumeItem HornOfBanishment::activate(actor::Actor* const actor)
{
        (void)actor;

        if (m_charges <= 0)
        {
                msg_log::add("It makes no sound.");

                return ConsumeItem::no;
        }

        auto effect = std::shared_ptr<SndHeardEffect>(
                new HornOfBanishmentHeard);

        Snd snd(
                "The Horn of Banishment resounds!",
                audio::SfxId::END,  // TODO: Make a sound effect
                IgnoreMsgIfOriginSeen::no,
                map::g_player->m_pos,
                map::g_player,
                SndVol::high,
                AlertsMon::yes,
                effect);

        snd.run();

        --m_charges;

        game_time::tick();

        return ConsumeItem::no;
}

// -----------------------------------------------------------------------------
// Holy Symbol
// -----------------------------------------------------------------------------
HolySymbol::HolySymbol(ItemData* item_data) :
        Item(item_data)
{
}

ConsumeItem HolySymbol::activate(actor::Actor* actor)
{
        (void)actor;

        if (!map::g_player->m_properties.allow_pray(Verbose::yes))
        {
                return ConsumeItem::no;
        }

        if (m_has_failed_attempt)
        {
                msg_log::add(
                        "I have no faith that this would help me at "
                        "the moment.");

                return ConsumeItem::no;
        }

        // Is not in failed state

        const std::string my_name =
                name(
                        ItemNameType::plain,
                        ItemNameInfo::none);

        std::string pray_msg;

        if (map::g_player->m_properties.has(PropId::terrified))
        {
                pray_msg = "With trembling hands ";
        }

        pray_msg += "I make a prayer over the " + my_name + "...";

        msg_log::add(
                pray_msg,
                colors::text(),
                MsgInterruptPlayer::no,
                MorePromptOnMsg::yes);

        // If the item is still charging, roll for success
        if ((m_nr_charge_turns_left > 0) && !rnd::percent(25))
        {
                // Failed!
                m_has_failed_attempt = true;

                // Set a recharge duration that is longer than normal
                m_nr_charge_turns_left = nr_turns_to_recharge().max;

                const int duration_pct = rnd::range(175, 200);

                m_nr_charge_turns_left =
                        (m_nr_charge_turns_left * duration_pct) / 100;

                msg_log::add("This feels useless!");

                map::g_player->incr_shock(4.0, ShockSrc::misc);

                game_time::tick();

                return ConsumeItem::no;
        }

        // Item can be used

        m_nr_charge_turns_left = nr_turns_to_recharge().roll();

        run_effect();

        game_time::tick();

        return ConsumeItem::no;
}

void HolySymbol::on_std_turn_in_inv_hook(InvType inv_type)
{
        (void)inv_type;

        // Already fully charged?
        if (m_nr_charge_turns_left == 0)
        {
                return;
        }

        ASSERT(m_nr_charge_turns_left > 0);

        --m_nr_charge_turns_left;

        if (m_nr_charge_turns_left == 0)
        {
                m_has_failed_attempt = false;

                const std::string my_name =
                        name(
                                ItemNameType::plain,
                                ItemNameInfo::none);

                msg_log::add(
                        "I feel like praying over the " +
                        my_name +
                        " would be beneficent again.");
        }
}

std::string HolySymbol::name_info_str() const
{
        if (m_nr_charge_turns_left <= 0)
        {
                return "";
        }

        const auto turns_left_str = std::to_string(m_nr_charge_turns_left);

        std::string str = "(" + turns_left_str + " turns";

        if (m_has_failed_attempt)
        {
                str += ", failed";
        }

        str += ")";

        return str;
}

void HolySymbol::save_hook() const
{
        saving::put_int(m_nr_charge_turns_left);
        saving::put_bool(m_has_failed_attempt);
}

void HolySymbol::load_hook()
{
        m_nr_charge_turns_left = saving::get_int();
        m_has_failed_attempt = saving::get_bool();
}

void HolySymbol::run_effect()
{
        map::g_player->restore_sp(rnd::range(1, 4));

        const int prop_duration = rnd::range(6, 12);

        auto* r_fear = property_factory::make(PropId::r_fear);
        auto* r_shock = property_factory::make(PropId::r_shock);

        r_fear->set_duration(prop_duration);
        r_shock->set_duration(prop_duration);

        map::g_player->m_properties.apply(r_fear);
        map::g_player->m_properties.apply(r_shock);
}

Range HolySymbol::nr_turns_to_recharge() const
{
        return {125, 175};
}

// -----------------------------------------------------------------------------
// Arcane Clockwork
// -----------------------------------------------------------------------------
Clockwork::Clockwork(ItemData* const item_data) :
        Item(item_data),
        m_charges(rnd::range(4, 6))
{
}

std::string Clockwork::name_info_str() const
{
        return "(" + std::to_string(m_charges) + " uses)";
}

void Clockwork::save_hook() const
{
        saving::put_int(m_charges);
}

void Clockwork::load_hook()
{
        m_charges = saving::get_int();
}

ConsumeItem Clockwork::activate(actor::Actor* const actor)
{
        (void)actor;

        if (m_charges <= 0)
        {
                msg_log::add("Nothing happens.");

                return ConsumeItem::no;
        }

        if (map::g_player->m_properties.has(PropId::extra_hasted))
        {
                msg_log::add("It will not move.");

                return ConsumeItem::no;
        }

        msg_log::add("I wind up the clockwork.");

        map::g_player->incr_shock(12.0, ShockSrc::use_strange_item);

        if (!map::g_player->is_alive())
        {
                return ConsumeItem::no;
        }

        map::g_player->m_properties.apply(
                property_factory::make(
                        PropId::extra_hasted));

        --m_charges;

        game_time::tick();

        return ConsumeItem::no;
}

// -----------------------------------------------------------------------------
// Shadow Dagger
// -----------------------------------------------------------------------------
ShadowDagger::ShadowDagger(ItemData* const item_data) :
        Wpn(item_data)
{
}

void ShadowDagger::on_melee_hit(actor::Actor& actor_hit, const int dmg)
{
        (void)dmg;

        if (actor_hit.m_state == ActorState::destroyed)
        {
                return;
        }

        if (is_radiant_creature(actor_hit))
        {
                hit_radiant_creature(actor_hit);
        }
        else
        {
                hit_normal_creature(actor_hit);
        }
}

void ShadowDagger::on_ranged_hit(actor::Actor& actor_hit)
{
        const int dmg = 1;  // Doesn't matter.

        on_melee_hit(actor_hit, dmg);
}

bool ShadowDagger::is_radiant_creature(const actor::Actor& actor) const
{
        const std::vector<PropId> radiant_props = {
                PropId::radiant_self,
                PropId::radiant_adjacent,
                PropId::radiant_fov};

        return std::any_of(
                std::cbegin(radiant_props),
                std::cend(radiant_props),
                [&actor](const auto id) {
                        return actor.m_data->natural_props[(size_t)id];
                });
}

void ShadowDagger::hit_normal_creature(actor::Actor& actor) const
{
        {
                auto* const prop =
                        property_factory::make(
                                PropId::light_sensitive);

                prop->set_indefinite();

                actor.m_properties.apply(prop);
        }

        {
                auto* const prop =
                        actor.m_properties.prop(
                                PropId::light_sensitive);

                if (!prop)
                {
                        ASSERT(false);

                        return;
                }

                auto* const lgt_sens = static_cast<PropLgtSens*>(prop);

                lgt_sens->raise_extra_damage_to(1);
        }
}

void ShadowDagger::hit_radiant_creature(actor::Actor& actor) const
{
        if (!actor.is_alive())
        {
                return;
        }

        if (actor::can_player_see_actor(actor))
        {
                const auto actor_name =
                        text_format::first_to_upper(
                                actor.name_the());

                msg_log::add(actor_name + " is assailed by dark energy.");

                draw_blast_at_seen_actors({&actor}, colors::gray());
        }

        const int dmg = rnd::range(1, 4);

        actor::hit(actor, dmg, DmgType::pure);
}

// -----------------------------------------------------------------------------
// Orb of Life
// -----------------------------------------------------------------------------
OrbOfLife::OrbOfLife(ItemData* const item_data) :
        Item(item_data)
{
}

void OrbOfLife::on_pickup_hook()
{
        map::g_player->change_max_hp(4, Verbose::yes);

        auto* prop_r_poison = property_factory::make(PropId::r_poison);
        prop_r_poison->set_indefinite();
        add_carrier_prop(prop_r_poison, Verbose::yes);

        auto* prop_r_disease = property_factory::make(PropId::r_disease);
        prop_r_disease->set_indefinite();
        add_carrier_prop(prop_r_disease, Verbose::yes);
}

void OrbOfLife::on_removed_from_inv_hook()
{
        map::g_player->change_max_hp(-4, Verbose::yes);

        clear_carrier_props();
}

// -----------------------------------------------------------------------------
// Necronomicon
// -----------------------------------------------------------------------------
Necronomicon::Necronomicon(ItemData* const item_data) :
        Item(item_data)
{
}

void Necronomicon::on_std_turn_in_inv_hook(const InvType inv_type)
{
        (void)inv_type;

        if (rnd::percent(2))
        {
                Snd snd(
                        "",
                        audio::SfxId::END,
                        IgnoreMsgIfOriginSeen::yes,
                        map::g_player->m_pos,
                        map::g_player,
                        SndVol::high,
                        AlertsMon::yes);

                snd.run();
        }
}

ItemPrePickResult Necronomicon::pre_pickup_hook()
{
        if (!player_bon::is_bg(Bg::exorcist))
        {
                return ItemPrePickResult::do_pickup;
        }

        // Is exorcist

        msg_log::add("I destroy the profane text!");

        game::incr_player_xp(10);

        map::g_player->restore_sp(999, false, Verbose::no);
        map::g_player->restore_sp(11, true);

        return ItemPrePickResult::destroy_item;
}

void Necronomicon::on_pickup_hook()
{
}

}  // namespace item

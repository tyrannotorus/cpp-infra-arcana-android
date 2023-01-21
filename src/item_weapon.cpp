// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "item_weapon.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <vector>

#include "actor.hpp"
#include "actor_data.hpp"
#include "actor_eat.hpp"
#include "actor_factory.hpp"
#include "actor_hit.hpp"
#include "actor_see.hpp"
#include "array2.hpp"
#include "audio_data.hpp"
#include "debug.hpp"
#include "draw_blast.hpp"
#include "explosion.hpp"
#include "fov.hpp"
#include "game_time.hpp"
#include "inventory.hpp"
#include "item_data.hpp"
#include "map.hpp"
#include "map_parsing.hpp"
#include "msg_log.hpp"
#include "player_bon.hpp"
#include "property.hpp"
#include "property_data.hpp"
#include "property_factory.hpp"
#include "property_handler.hpp"
#include "random.hpp"
#include "saving.hpp"
#include "sound.hpp"
#include "text_format.hpp"
#include "wpn_dmg.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// item
// -----------------------------------------------------------------------------
namespace item
{
Wpn::Wpn(ItemData* const item_data) :
        Item(item_data),
        m_ammo_loaded(0),
        m_ammo_data(nullptr)
{
        const auto ammo_item_id = m_data->ranged.ammo_item_id;

        if (ammo_item_id != Id::END) {
                m_ammo_data = &item::g_data[(size_t)ammo_item_id];
                m_ammo_loaded = m_data->ranged.max_ammo;
        }
}

void Wpn::save_hook() const
{
        saving::put_int(m_ammo_loaded);
}

void Wpn::load_hook()
{
        m_ammo_loaded = saving::get_int();
}

Color Wpn::color() const
{
        if (m_data->ranged.is_ranged_wpn &&
            !m_data->ranged.has_infinite_ammo &&
            (m_ammo_loaded == 0)) {
                return m_data->color.shaded(50);
        }

        return m_data->color;
}

std::string Wpn::name_info_str() const
{
        if (!m_data->ranged.is_ranged_wpn ||
            m_data->ranged.has_infinite_ammo) {
                return "";
        }

        return (
                "(" +
                std::to_string(m_ammo_loaded) +
                "/" +
                std::to_string(m_data->ranged.max_ammo) +
                ")");
}

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
            rnd::one_in(6)) {
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

        if (actor_hit.is_alive()) {
                // Poison victim from Ghoul Toxic trait?
                if (player_bon::has_trait(Trait::toxic) &&
                    rnd::fraction(3, 4)) {
                        actor_hit.m_properties.apply(
                                property_factory::make(PropId::poisoned));
                }

                // Terrify victim from Ghoul Indomitable Fury trait?
                if (player_bon::has_trait(Trait::indomitable_fury) &&
                    map::g_player->m_properties.has(PropId::frenzied)) {
                        actor_hit.m_properties.apply(
                                property_factory::make(PropId::terrified));
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
            rnd::one_in(3)) {
                actor::spawn(
                        actor_killed.m_pos,
                        {1, "MON_WORM_MASS"},
                        map::rect())
                        .make_aware_of_player()
                        .set_leader(map::g_player);
        }
}

MiGoGun::MiGoGun(ItemData* const item_data) :
        Wpn(item_data) {}

void MiGoGun::specific_dmg_mod(
        WpnDmg& range,
        const actor::Actor* const actor) const
{
        if (actor::is_player(actor) &&
            player_bon::has_trait(Trait::elec_incl)) {
                range.set_plus(range.plus() + 1);
        }
}

void MiGoGun::pre_ranged_attack()
{
        if (!actor::is_player(m_actor_carrying)) {
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
                nullptr,
                AllowWound::no);

        auto* disabled_regen =
                property_factory::make(PropId::disabled_hp_regen);

        disabled_regen->set_duration(
                rnd::range(
                        g_mi_go_gun_regen_disabled_min_turns,
                        g_mi_go_gun_regen_disabled_max_turns));

        m_actor_carrying->m_properties.apply(disabled_regen);
}

Incinerator::Incinerator(ItemData* const item_data) :
        Wpn(item_data) {}

void Incinerator::on_projectile_blocked(const P& pos)
{
        explosion::run(pos, ExplType::expl);
}

void RavenPeck::on_melee_hit(actor::Actor& actor_hit, const int dmg)
{
        (void)dmg;

        if (!actor_hit.is_alive()) {
                return;
        }

        // Gas mask and Asbestos suit protects against blindness.
        Item* const head_item = actor_hit.m_inv.item_in_slot(SlotId::head);
        Item* const body_item = actor_hit.m_inv.item_in_slot(SlotId::body);

        if ((head_item && head_item->id() == Id::gas_mask) ||
            (body_item && body_item->id() == Id::armor_asb_suit)) {
                return;
        }

        if (rnd::coin_toss()) {
                Prop* const prop = property_factory::make(PropId::blind);

                prop->set_duration(2);

                actor_hit.m_properties.apply(prop);
        }
}

void VampiricBite::on_melee_hit(actor::Actor& actor_hit, const int dmg)
{
        if (!actor_hit.is_alive()) {
                return;
        }

        m_actor_carrying->restore_hp(
                dmg,
                false,
                Verbose::yes);
}

void MindLeechSting::on_melee_hit(actor::Actor& actor_hit, const int dmg)
{
        (void)dmg;

        if (!actor_hit.is_alive() ||
            !actor::is_player(&actor_hit)) {
                return;
        }

        auto* const mon = m_actor_carrying;

        if (map::g_player->insanity() >= 50 ||
            map::g_player->m_properties.has(PropId::confused) ||
            map::g_player->m_properties.has(PropId::frenzied)) {
                const bool player_see_mon = actor::can_player_see_actor(*mon);

                if (player_see_mon) {
                        const std::string mon_name_the =
                                text_format::first_to_upper(mon->name_the());

                        msg_log::add(mon_name_the + " looks shocked!");
                }

                actor::hit(
                        *mon,
                        rnd::range(3, 15),
                        DmgType::pure,
                        &actor_hit);

                if (mon->is_alive()) {
                        mon->m_properties.apply(
                                property_factory::make(PropId::confused));

                        mon->m_properties.apply(
                                property_factory::make(PropId::terrified));
                }
        }
        else {
                // Player mind can be eaten
                Prop* prop_mind_sap = property_factory::make(PropId::mind_sap);

                prop_mind_sap->set_indefinite();

                map::g_player->m_properties.apply(prop_mind_sap);

                // Make the monster pause, so things don't get too crazy
                auto* prop_waiting = property_factory::make(PropId::waiting);

                prop_waiting->set_duration(2);

                mon->m_properties.apply(prop_waiting);
        }
}

void DustEngulf::on_melee_hit(actor::Actor& actor_hit, const int dmg)
{
        (void)dmg;

        if (!actor_hit.is_alive()) {
                return;
        }

        // Gas mask and Asbestos suit protects against blindness.
        Item* const head_item = actor_hit.m_inv.item_in_slot(SlotId::head);
        Item* const body_item = actor_hit.m_inv.item_in_slot(SlotId::body);

        if ((head_item && head_item->id() == Id::gas_mask) ||
            (body_item && body_item->id() == Id::armor_asb_suit)) {
                return;
        }

        Prop* const prop = property_factory::make(PropId::blind);

        actor_hit.m_properties.apply(prop);
}

void SnakeVenomSpit::on_ranged_hit(actor::Actor& actor_hit)
{
        if (!actor_hit.is_alive()) {
                return;
        }

        // Gas mask and Asbestos suit protects against blindness.
        Item* const head_item = actor_hit.m_inv.item_in_slot(SlotId::head);
        Item* const body_item = actor_hit.m_inv.item_in_slot(SlotId::body);

        if ((head_item && head_item->id() == Id::gas_mask) ||
            (body_item && body_item->id() == Id::armor_asb_suit)) {
                return;
        }

        Prop* const prop = property_factory::make(PropId::blind);

        prop->set_duration(7);

        actor_hit.m_properties.apply(prop);
}

PharaohStaff::PharaohStaff(ItemData* const item_data) :
        Wpn(item_data)
{
}

void PharaohStaff::on_std_turn_in_inv_hook(const InvType inv_type)
{
        (void)inv_type;

        if (!actor::is_player(actor_carrying())) {
                return;
        }

        Array2<bool> blocked_los(map::dims());

        map_parsers::BlocksLos()
                .run(blocked_los,
                     fov::fov_rect(map::g_player->m_pos, map::dims()),
                     MapParseMode::overwrite);

        for (auto* const actor : game_time::g_actors) {
                if (actor::is_player(actor) || !actor->is_alive()) {
                        continue;
                }

                if (!actor->is_aware_of_player()) {
                        continue;
                }

                const bool mon_see_player =
                        actor::can_mon_see_actor(
                                *actor,
                                *map::g_player,
                                blocked_los);

                if (!mon_see_player) {
                        continue;
                }

                on_mon_see_player_carrying(*actor);
        }
}

void PharaohStaff::on_mon_see_player_carrying(actor::Actor& mon) const
{
        // TODO: Consider an "is_mummy" actor data field
        if ((mon.id() != "MON_MUMMY") &&
            (mon.id() != "MON_CROC_HEAD_MUMMY")) {
                return;
        }

        if (mon.is_actor_my_leader(map::g_player)) {
                return;
        }

        const int convert_pct_chance = 10;

        if (rnd::percent(convert_pct_chance)) {
                mon.m_leader = map::g_player;

                if (actor::can_player_see_actor(mon)) {
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

        if (!actor_hit.is_alive()) {
                return;
        }

        const int doomed_pct = 50;

        if (rnd::percent(doomed_pct)) {
                auto* prop = property_factory::make(PropId::doomed);

                prop->set_duration(rnd::range(3, 4));

                actor_hit.m_properties.apply(prop);
        }
}

ShadowDagger::ShadowDagger(ItemData* const item_data) :
        Wpn(item_data)
{
}

void ShadowDagger::on_melee_hit(actor::Actor& actor_hit, const int dmg)
{
        (void)dmg;

        if (actor_hit.m_state == ActorState::destroyed) {
                return;
        }

        if (is_radiant_creature(actor_hit)) {
                hit_radiant_creature(actor_hit);
        }
        else {
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

                if (!prop) {
                        ASSERT(false);

                        return;
                }

                auto* const lgt_sens = static_cast<PropLgtSens*>(prop);

                lgt_sens->raise_extra_damage_to(1);
        }
}

void ShadowDagger::hit_radiant_creature(actor::Actor& actor) const
{
        if (!actor.is_alive()) {
                return;
        }

        const bool player_see_target = actor::can_player_see_actor(actor);
        const bool player_see_pos = map::g_seen.at(actor.m_pos);

        if (player_see_target || player_see_pos) {
                const std::string target_name =
                        player_see_target
                        ? text_format::first_to_upper(actor.name_the())
                        : "It";

                msg_log::add(target_name + " is assailed by dark energy.");

                draw_blast_at_seen_actors({&actor}, colors::gray());
        }

        const int dmg = rnd::range(1, 4);

        actor::hit(actor, dmg, DmgType::pure, m_actor_carrying);
}

void ZombieDust::on_ranged_hit(actor::Actor& actor_hit)
{
        if (actor_hit.is_alive() && !actor_hit.m_data->is_undead) {
                actor_hit.m_properties.apply(
                        property_factory::make(PropId::paralyzed));
        }
}

}  // namespace item

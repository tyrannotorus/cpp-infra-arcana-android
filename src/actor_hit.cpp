// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "actor_hit.hpp"

#include <algorithm>
#include <ostream>
#include <string>

#include "actor.hpp"
#include "actor_data.hpp"
#include "actor_death.hpp"
#include "actor_player_state.hpp"
#include "actor_see.hpp"
#include "array2.hpp"
#include "audio_data.hpp"
#include "colors.hpp"
#include "config.hpp"
#include "debug.hpp"
#include "game.hpp"
#include "inventory.hpp"
#include "item.hpp"
#include "item_armor.hpp"
#include "map.hpp"
#include "msg_log.hpp"
#include "player_bon.hpp"
#include "property.hpp"
#include "property_data.hpp"
#include "property_factory.hpp"
#include "property_handler.hpp"
#include "random.hpp"
#include "sound.hpp"
#include "terrain.hpp"
#include "terrain_data.hpp"
#include "text_format.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static void destroy_armor(actor::Actor& actor, const item::Armor* const armor)
{
        TRACE << "Armor was destroyed" << std::endl;

        if (actor::is_player(&actor)) {
                const std::string armor_name =
                        armor->name(ItemNameType::plain, ItemNameInfo::none);

                const std::string msg = "My " + armor_name + " is torn apart!";

                msg_log::add(msg, colors::msg_note());
        }

        actor.m_inv.remove_item_in_slot(SlotId::body, true);
}

static void damage_armor(actor::Actor& actor, int dmg)
{
        item::Item* const item = actor.m_inv.item_in_slot(SlotId::body);

        if (!item) {
                return;
        }

        auto* const armor = static_cast<item::Armor*>(item);

        armor->hit(dmg);

        if (armor->armor_points() <= 0) {
                destroy_armor(actor, armor);
        }
}

static int hit_armor_and_calc_new_damage(actor::Actor& actor, int dmg)
{
        // NOTE: We retrieve armor points BEFORE damaging the armor, since the armor should reduce
        // damage taken by the current (pre-hit) armor value even if it gets damaged or destroyed.
        const int armor_points = actor::armor_points(actor);

        // NOTE: The player having extra armor points from properties or non-destructible worn items
        // (e.g. flagellant torture collar) does NOT help with protecting the durability of the worn
        // body armor item, it only helps with reducing the final damage taken. This makes some
        // sense in a way - why would the player being thick skinned make their leather jacket last
        // longer?
        //
        // If there ever is for example a destructible head item (e.g. soldier's helmet), then
        // perhaps randomize if the body armor or head armor should take the durability damage - or
        // distribute it among them.
        //

        // Damage worn armor.
        if (actor.m_data->is_humanoid) {
                damage_armor(actor, dmg);
        }

        dmg -= armor_points;

        dmg = std::max(1, dmg);

        return dmg;
}

static void hit_corpse_destroy_success(
        actor::Actor& actor,
        const DmgType dmg_type)
{
        if ((dmg_type == DmgType::kicking) ||
            (dmg_type == DmgType::blunt) ||
            (dmg_type == DmgType::slashing) ||
            (dmg_type == DmgType::piercing)) {
                Snd snd(
                        "*Crack!*",
                        audio::SfxId::hit_corpse_break,
                        IgnoreMsgIfOriginSeen::yes,
                        actor.m_pos,
                        nullptr,
                        SndVol::low,
                        AlertsMon::yes);

                snd.run();
        }

        actor.m_state = ActorState::destroyed;

        actor.m_properties.on_destroyed_corpse();

        if (actor.m_data->is_humanoid) {
                terrain::make_blood(actor.m_pos);
                terrain::make_gore(actor.m_pos);
        }

        if (map::g_seen.at(actor.m_pos)) {
                const auto name =
                        text_format::first_to_upper(
                                actor.m_data->corpse_name_the);

                msg_log::add(name + " is destroyed.");
        }
}

static void hit_corpse_destroy_fail(
        const actor::Actor& actor,
        const DmgType dmg_type)
{
        if ((dmg_type == DmgType::kicking) ||
            (dmg_type == DmgType::blunt) ||
            (dmg_type == DmgType::slashing) ||
            (dmg_type == DmgType::piercing)) {
                std::string msg;

                if ((dmg_type == DmgType::blunt) ||
                    (dmg_type == DmgType::kicking)) {
                        msg = "*Thud!*";
                }
                else {
                        msg = "*Chop!*";
                }

                Snd snd(
                        msg,
                        audio::SfxId::hit_medium,
                        IgnoreMsgIfOriginSeen::yes,
                        actor.m_pos,
                        nullptr,
                        SndVol::low,
                        AlertsMon::yes);

                snd.run();
        }
}

static void hit_corpse(
        actor::Actor& actor,
        int dmg,
        const DmgType dmg_type)
{
        ASSERT(actor.m_data->can_leave_corpse);

        // Chance to destroy is X in Y, where:
        // X = damage dealt * 4
        // Y = maximum actor hit points

        const int den = actor::max_hp(actor);
        const int num = std::min(dmg * 4, den);

        if (rnd::fraction(num, den)) {
                hit_corpse_destroy_success(actor, dmg_type);
        }
        else {
                hit_corpse_destroy_fail(actor, dmg_type);
        }
}

static void kill_actor_by_hit(actor::Actor& actor, const int dmg)
{
        const auto f_id = map::g_terrain.at(actor.m_pos)->id();

        const bool is_on_bottomless = (f_id == terrain::Id::chasm);

        // Immediately destroy the actor if the killing blow damage is either:
        //
        // * Above a threshold relative to maximum hit points, or
        // * Above a fixed value threshold
        //
        // The purpose of the first case is to make it likely that small
        // creatures like rats are destroyed.
        //
        // The purpose of the second point is that powerful attacks like
        // explosions should always destroy the corpse, even if the
        // creature has a very high pool of hit points.

        const int dmg_threshold_relative = (max_hp(actor) * 3) / 2;

        const int dmg_threshold_absolute = 14;

        const auto is_destroyed =
                (!actor.m_data->can_leave_corpse ||
                 is_on_bottomless ||
                 actor.m_properties.has(prop::Id::summoned) ||
                 (dmg >= dmg_threshold_relative) ||
                 (dmg >= dmg_threshold_absolute))
                ? IsDestroyed::yes
                : IsDestroyed::no;

        const auto allow_gore =
                is_on_bottomless
                ? AllowGore::no
                : AllowGore::yes;

        const auto allow_drop_items =
                is_on_bottomless
                ? AllowDropItems::no
                : AllowDropItems::yes;

        kill(actor, is_destroyed, allow_gore, allow_drop_items);
}

static void on_actor_not_killed_by_hit(
        actor::Actor& actor,
        const int hp_pct_before)
{
        if (!actor::is_player(&actor)) {
                return;
        }

        const int hp_pct_after = (actor.m_hp * 100) / max_hp(actor);
        const int hp_warn_lvl = 25;

        if (((hp_pct_before > hp_warn_lvl)) &&
            ((hp_pct_after <= hp_warn_lvl))) {
                map::update_vision();

                msg_log::more_prompt();

                msg_log::add(
                        "-LOW HP WARNING!-",
                        colors::msg_bad(),
                        MsgInterruptPlayer::no,
                        MorePromptOnMsg::yes);
        }
}

static bool is_light_sensitive(const actor::Actor& actor)
{
        return (
                actor.m_properties.has(prop::Id::light_sensitive) ||
                actor.m_properties.has(prop::Id::light_sensitive_curse));
}

static int calc_new_dmg_for_light_sensitive(
        const actor::Actor& actor,
        const int dmg)
{
        // NOTE: Only the basic "light sensitive" property has a damage
        // modifier, not the player light sensitive curse.
        auto* const prop = actor.m_properties.prop(prop::Id::light_sensitive);

        if (prop) {
                auto* const lgt_sens = static_cast<prop::LgtSens*>(prop);

                return dmg + lgt_sens->get_extra_damage();
        }
        else {
                return dmg;
        }
}

static void on_light_sensitive_player_hit_by_light()
{
        map::g_player->interrupt_actions(ForceInterruptActions::no);

        msg_log::add("I am wracked by light!", colors::msg_bad());
}

static int absorb_dmg_for_prolonged_life_exorcist(int dmg)
{
        // Of the damage that the player does not have HP to cover, soak up as
        // much damage as possible with fervor.
        const int missing_hp = (dmg - map::g_player->m_hp) + 1;

        if (missing_hp > 0) {
                const int fervor_dmg =
                        std::min(
                                missing_hp,
                                actor::player_state::g_exorcist_fervor);

                actor::player_state::g_exorcist_fervor -= fervor_dmg;

                dmg -= fervor_dmg;
        }

        return dmg;
}

static int nr_wounds(const prop::PropHandler& properties)
{
        if (properties.has(prop::Id::wound)) {
                const auto* const prop = properties.prop(prop::Id::wound);

                const auto* const wound = static_cast<const prop::Wound*>(prop);

                return wound->nr_wounds();
        }
        else {
                return 0;
        }
}

static void on_player_hit(
        const int dmg,
        const DmgType dmg_type,
        const AllowWound allow_wound)
{
        // NOTE: Here we interrupt player multi-turn actions, UNLESS the damage
        // is a small number of "pure" damage (i.e. not physical, electrical,
        // etc). The idea is that something like taking a pistol shot should
        // realistically stop you from treating wounds or handling equipment
        // etc, while taking a minor hit by something like poison ticking would
        // not necessarily stop you.
        const bool is_small_pure_damage = ((dmg_type == DmgType::pure) && (dmg <= 1));

        if (!is_small_pure_damage) {
                map::g_player->interrupt_actions(ForceInterruptActions::yes);
        }

        map::g_player->incr_shock(1.0, ShockSrc::take_damage);

        const bool is_enough_dmg_for_wound = (dmg >= g_min_dmg_to_wound);
        const bool is_physical = is_physical_dmg_type(dmg_type);

        // Ghoul trait Indomitable Fury grants immunity to wounds while frenzied
        const bool is_ghoul_resist_wound =
                player_bon::has_trait(Trait::indomitable_fury) &&
                map::g_player->m_properties.has(prop::Id::frenzied);

        const bool is_wounded =
                (allow_wound == AllowWound::yes) &&
                ((map::g_player->m_hp - dmg) > 0) &&
                is_enough_dmg_for_wound &&
                is_physical &&
                !is_ghoul_resist_wound &&
                !config::is_bot_playing();

        if (is_wounded) {
                prop::Prop* const prop = prop::make(prop::Id::wound);

                prop->set_indefinite();

                const int nr_wounds_before = nr_wounds(map::g_player->m_properties);

                map::g_player->m_properties.apply(prop);

                const int nr_wounds_after = nr_wounds(map::g_player->m_properties);

                if (nr_wounds_after > nr_wounds_before) {
                        game::add_history_event("Sustained a severe wound");
                }
        }
}

// -----------------------------------------------------------------------------
// actor
// -----------------------------------------------------------------------------
namespace actor
{
void hit(
        Actor& actor,
        int dmg,
        const DmgType dmg_type,
        actor::Actor* const attacker,
        const AllowWound allow_wound)
{
        if (actor.m_state == ActorState::destroyed) {
                return;
        }

        if (dmg_type == DmgType::light) {
                if (!is_light_sensitive(actor)) {
                        return;
                }

                if (actor::is_player(&actor)) {
                        on_light_sensitive_player_hit_by_light();
                }

                dmg = calc_new_dmg_for_light_sensitive(actor, dmg);
        }

        const int hp_pct_before = (actor.m_hp * 100) / max_hp(actor);

        if (actor::is_corpse(actor) && !actor::is_player(&actor)) {
                hit_corpse(actor, dmg, dmg_type);

                return;
        }

        if (dmg_type == DmgType::spirit) {
                hit_sp(actor, dmg, attacker);

                return;
        }

        // Property resists damage?
        const auto verbose = actor::is_alive(actor) ? Verbose::yes : Verbose::no;

        const bool is_dmg_resisted = actor.m_properties.is_resisting_dmg(dmg_type, verbose);

        if (is_dmg_resisted) {
                return;
        }

        if ((dmg > 0)) {
                if (is_physical_dmg_type(dmg_type)) {
                        // NOTE: Armor never reduces damage to zero.
                        dmg = hit_armor_and_calc_new_damage(actor, dmg);
                }

                // Soaking up damage with fervor instead due to Prolonged Life?
                if (actor::is_player(&actor) && player_bon::has_trait(Trait::prolonged_life)) {
                        dmg = absorb_dmg_for_prolonged_life_exorcist(dmg);

                        if (dmg <= 0) {
                                map::g_player->interrupt_actions(ForceInterruptActions::no);

                                return;
                        }
                }
        }

        if (is_player(&actor)) {
                on_player_hit(dmg, dmg_type, allow_wound);
        }

        actor.m_properties.on_hit(dmg, dmg_type, attacker);

        if (!actor::is_alive(actor)) {
                // This could happen for example if the player dies from too
                // many wounds.
                return;
        }

        if ((dmg > 0) &&
            !(actor::is_player(&actor) && config::is_bot_playing())) {
                actor.m_hp -= dmg;
        }

        if (actor.m_hp <= 0) {
                kill_actor_by_hit(actor, dmg);

                return;
        }
        else {
                on_actor_not_killed_by_hit(actor, hp_pct_before);

                return;
        }
}

void hit_sp(
        Actor& actor,
        const int dmg,
        actor::Actor* const attacker,
        const Verbose verbose)
{
        if (verbose == Verbose::yes) {
                if (actor::is_player(&actor)) {
                        msg_log::add(
                                "My spirit is drained!",
                                colors::msg_bad());
                }
        }

        actor.m_properties.on_hit(dmg, DmgType::spirit, attacker);

        if (!actor::is_player(&actor) || !config::is_bot_playing()) {
                actor.m_sp = std::max(0, actor.m_sp - dmg);
        }

        if (actor.m_sp > 0) {
                if (actor::is_player(&actor)) {
                        map::g_player->interrupt_actions(ForceInterruptActions::no);
                }

                return;
        }

        // Spirit is zero or lower

        if (actor::is_player(&actor)) {
                msg_log::add(
                        "All my spirit is depleted, I am devoid of life!",
                        colors::msg_bad());
        }
        else if (can_player_see_actor(actor)) {
                const std::string actor_name_the =
                        text_format::first_to_upper(
                                actor::name_the(actor));

                msg_log::add(actor_name_the + " has no spirit left!");
        }

        const auto terrain_id = map::g_terrain.at(actor.m_pos)->id();

        const bool is_on_bottomless = (terrain_id == terrain::Id::chasm);

        const auto is_destroyed =
                (!actor.m_data->can_leave_corpse ||
                 is_on_bottomless ||
                 actor.m_properties.has(prop::Id::summoned))
                ? IsDestroyed::yes
                : IsDestroyed::no;

        kill(actor, is_destroyed, AllowGore::no, AllowDropItems::yes);
}

}  // namespace actor

// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
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
#include "actor_player.hpp"
#include "actor_see.hpp"
#include "array2.hpp"
#include "audio_data.hpp"
#include "colors.hpp"
#include "config.hpp"
#include "debug.hpp"
#include "inventory.hpp"
#include "item.hpp"
#include "map.hpp"
#include "msg_log.hpp"
#include "player_bon.hpp"
#include "property_data.hpp"
#include "property_handler.hpp"
#include "random.hpp"
#include "sound.hpp"
#include "terrain.hpp"
#include "terrain_data.hpp"
#include "text_format.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static int hit_armor(actor::Actor& actor, int dmg)
{
        // NOTE: We retrieve armor points BEFORE damaging the armor, since it
        // should reduce damage taken even if it gets damaged or destroyed.
        const int ap = actor.armor_points();

        // Danage worn armor
        if (actor.m_data->is_humanoid)
        {
                auto* const item = actor.m_inv.item_in_slot(SlotId::body);

                if (item)
                {
                        TRACE_VERBOSE << "Has armor, running hit on armor"
                                      << std::endl;

                        auto* const armor = static_cast<item::Armor*>(item);

                        armor->hit(dmg);

                        if (armor->is_destroyed())
                        {
                                TRACE << "Armor was destroyed" << std::endl;

                                if (actor::is_player(&actor))
                                {
                                        const std::string armor_name =
                                                armor->name(
                                                        ItemNameType::plain,
                                                        ItemNameInfo::none);

                                        msg_log::add(
                                                ("My " +
                                                 armor_name +
                                                 " is torn apart!"),
                                                colors::msg_note());
                                }

                                actor.m_inv.remove_item_in_slot(
                                        SlotId::body, true);
                        }
                }
        }

        // Reduce damage by the total ap value - the new damage value may be
        // negative, this is the callers resonsibility to handle
        dmg -= ap;

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
            (dmg_type == DmgType::piercing))
        {
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

        if (actor.m_data->is_humanoid)
        {
                terrain::make_blood(actor.m_pos);
                terrain::make_gore(actor.m_pos);
        }

        if (map::g_seen.at(actor.m_pos))
        {
                const auto name =
                        text_format::first_to_upper(
                                actor.m_data->corpse_name_the);

                msg_log::add(name + " is destroyed.");
        }
}

static void hit_corpse_destroy_fail(
        actor::Actor& actor,
        const DmgType dmg_type)
{
        if ((dmg_type == DmgType::kicking) ||
            (dmg_type == DmgType::blunt) ||
            (dmg_type == DmgType::slashing) ||
            (dmg_type == DmgType::piercing))
        {
                std::string msg;

                if ((dmg_type == DmgType::blunt) ||
                    (dmg_type == DmgType::kicking))
                {
                        msg = "*Thud!*";
                }
                else
                {
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

        if (rnd::fraction(num, den))
        {
                hit_corpse_destroy_success(actor, dmg_type);
        }
        else
        {
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
                 actor.m_properties.has(PropId::summoned) ||
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
        if (!actor::is_player(&actor))
        {
                return;
        }

        const int hp_pct_after = (actor.m_hp * 100) / max_hp(actor);
        const int hp_warn_lvl = 25;

        if (((hp_pct_before > hp_warn_lvl)) &&
            ((hp_pct_after <= hp_warn_lvl)))
        {
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
                actor.m_properties.has(PropId::light_sensitive) ||
                actor.m_properties.has(PropId::light_sensitive_curse));
}

static int calc_new_dmg_for_light_sensitive(
        const actor::Actor& actor,
        const int dmg)
{
        // NOTE: Only the basic "light sensitive" property has a damage
        // modifier, not the player light sensitive curse.
        auto* const prop = actor.m_properties.prop(PropId::light_sensitive);

        if (prop)
        {
                auto* const lgt_sens = static_cast<PropLgtSens*>(prop);

                return dmg + lgt_sens->get_extra_damage();
        }
        else
        {
                return dmg;
        }
}

static void on_light_sensitive_player_hit_by_light()
{
        map::g_player->interrupt_actions(
                ForceInterruptActions::no);

        msg_log::add(
                "I am wracked by light!",
                colors::msg_bad());
}

static int absorb_dmg_for_prolonged_life_player(int dmg)
{
        // Soak up as much damage as possible with SP instead of HP (but never
        // reduce SP below 1).
        const int missing_hp = (dmg - map::g_player->m_hp) + 1;

        if (missing_hp > 0)
        {
                const int sp_dmg =
                        std::min(
                                missing_hp,
                                map::g_player->m_sp - 1);

                map::g_player->m_sp -= sp_dmg;

                dmg -= sp_dmg;
        }

        return dmg;
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
        const AllowWound allow_wound)
{
        if (actor.m_state == ActorState::destroyed)
        {
                return;
        }

        if (dmg_type == DmgType::light)
        {
                if (!is_light_sensitive(actor))
                {
                        return;
                }

                if (actor::is_player(&actor))
                {
                        on_light_sensitive_player_hit_by_light();
                }

                dmg = calc_new_dmg_for_light_sensitive(actor, dmg);
        }

        const int hp_pct_before = (actor.m_hp * 100) / max_hp(actor);

        if (actor.is_corpse() && !actor::is_player(&actor))
        {
                hit_corpse(actor, dmg, dmg_type);

                return;
        }

        if (dmg_type == DmgType::spirit)
        {
                hit_sp(actor, dmg);

                return;
        }

        // Property resists damage?
        const auto verbose = actor.is_alive() ? Verbose::yes : Verbose::no;

        const bool is_dmg_resisted =
                actor.m_properties.is_resisting_dmg(
                        dmg_type,
                        verbose);

        if (is_dmg_resisted)
        {
                return;
        }

        if ((dmg > 0))
        {
                if (is_physical_dmg_type(dmg_type))
                {
                        // NOTE: Armor never reduces damage to zero.
                        dmg = hit_armor(actor, dmg);
                }

                // Soaking up damage with SP instead due to Prolonged Life?
                if (actor::is_player(&actor) &&
                    player_bon::has_trait(Trait::prolonged_life))
                {
                        dmg = absorb_dmg_for_prolonged_life_player(dmg);

                        if (dmg <= 0)
                        {
                                map::g_player->interrupt_actions(
                                        ForceInterruptActions::no);

                                return;
                        }
                }
        }

        actor.on_hit(dmg, dmg_type, allow_wound);

        actor.m_properties.on_hit();

        if ((dmg > 0) &&
            !(actor::is_player(&actor) &&
              config::is_bot_playing()))
        {
                actor.m_hp -= dmg;
        }

        if (actor.m_hp <= 0)
        {
                kill_actor_by_hit(actor, dmg);

                return;
        }
        else
        {
                on_actor_not_killed_by_hit(actor, hp_pct_before);

                return;
        }
}

void hit_sp(
        Actor& actor,
        const int dmg,
        const Verbose verbose)
{
        if (verbose == Verbose::yes)
        {
                if (actor::is_player(&actor))
                {
                        msg_log::add(
                                "My spirit is drained!",
                                colors::msg_bad());
                }
        }

        actor.m_properties.on_hit();

        if (!actor::is_player(&actor) || !config::is_bot_playing())
        {
                actor.m_sp = std::max(0, actor.m_sp - dmg);
        }

        if (actor.m_sp > 0)
        {
                if (actor::is_player(&actor))
                {
                        map::g_player->interrupt_actions(
                                ForceInterruptActions::no);
                }

                return;
        }

        // Spirit is zero or lower

        if (actor::is_player(&actor))
        {
                msg_log::add(
                        "All my spirit is depleted, I am devoid of life!",
                        colors::msg_bad());
        }
        else if (can_player_see_actor(actor))
        {
                const std::string actor_name_the =
                        text_format::first_to_upper(
                                actor.name_the());

                msg_log::add(actor_name_the + " has no spirit left!");
        }

        const auto terrain_id = map::g_terrain.at(actor.m_pos)->id();

        const bool is_on_bottomless = (terrain_id == terrain::Id::chasm);

        const auto is_destroyed =
                (!actor.m_data->can_leave_corpse ||
                 is_on_bottomless ||
                 actor.m_properties.has(PropId::summoned))
                ? IsDestroyed::yes
                : IsDestroyed::no;

        kill(actor, is_destroyed, AllowGore::no, AllowDropItems::yes);
}

}  // namespace actor

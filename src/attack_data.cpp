// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "attack_data.hpp"

#include <algorithm>
#include <ostream>

#include "ability_values.hpp"
#include "actor.hpp"
#include "actor_player.hpp"
#include "actor_see.hpp"
#include "array2.hpp"
#include "config.hpp"
#include "debug.hpp"
#include "fov.hpp"
#include "global.hpp"
#include "inventory.hpp"
#include "item.hpp"
#include "item_data.hpp"
#include "map.hpp"
#include "map_parsing.hpp"
#include "misc.hpp"
#include "player_bon.hpp"
#include "property_data.hpp"
#include "property_handler.hpp"
#include "random.hpp"
#include "rect.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static bool is_defender_aware_of_attack(
        const actor::Actor* const attacker,
        const actor::Actor& defender)
{
        if (!attacker)
        {
                return true;
        }
        else if (defender.is_player())
        {
                // Monster attacking player
                return attacker->is_player_aware_of_me();
        }
        else if (attacker->is_player())
        {
                // Player attacking monster
                return (
                        defender.is_aware_of_player() ||
                        defender.is_actor_my_leader(map::g_player));
        }
        else if (defender.is_actor_my_leader(map::g_player))
        {
                // Hostile monster attacking player-allied monster

                // The player-allied monster is considered aware, if the player
                // is aware of the attacker
                return attacker->is_player_aware_of_me();
        }
        else
        {
                // Player-allied monster attacking hostile monster

                // The hostile monster is considererd aware, if it is aware of
                // the player
                return attacker->is_aware_of_player();
        }
}

static bool is_mon_hit_chance_penalty(
        const actor::Actor* const attacker,
        const actor::Actor* const defender)
{
        return (
                attacker &&
                !attacker->is_player() &&
                defender &&
                defender->is_player() &&
                attacker->m_give_hit_chance_penalty_vs_player);
}

// -----------------------------------------------------------------------------
// Attack data
// -----------------------------------------------------------------------------
AttData::AttData(
        actor::Actor* const the_attacker,
        actor::Actor* const the_defender,
        const item::Item& the_att_item) :
        attacker(the_attacker),
        defender(the_defender),
        att_item(&the_att_item),
        is_intrinsic_att(
                (att_item->data().type == ItemType::melee_wpn_intr) ||
                (att_item->data().type == ItemType::ranged_wpn_intr))
{}

MeleeAttData::MeleeAttData(
        actor::Actor* const the_attacker,
        actor::Actor& the_defender,
        const item::Wpn& wpn) :
        AttData(the_attacker, &the_defender, wpn)
{
        const bool is_defender_aware =
                is_defender_aware_of_attack(attacker, *defender);

        // Determine attack result
        const int skill_mod =
                attacker
                ? attacker->ability(AbilityId::melee, true)
                : 50;

        const int wpn_mod = wpn.data().melee.hit_chance_mod;

        int dodging_mod = 0;

        const bool player_is_handling_armor =
                (map::g_player->m_equip_armor_countdown > 0) ||
                (map::g_player->m_remove_armor_countdown);

        const int dodging_ability = defender->ability(AbilityId::dodging, true);

        // Player gets melee dodging bonus from wielding a Pitchfork
        if (defender->is_player())
        {
                const auto* const item =
                        defender->m_inv.item_in_slot(SlotId::wpn);

                if (item && (item->id() == item::Id::pitch_fork))
                {
                        dodging_mod -= 15;
                }
        }

        const bool allow_positive_doge =
                is_defender_aware &&
                !(defender->is_player() &&
                  player_is_handling_armor);

        if (allow_positive_doge || (dodging_ability < 0))
        {
                dodging_mod -= dodging_ability;
        }

        // Attacker gets a penalty against unseen targets

        // NOTE: The AI never attacks unseen targets, so in the case of a
        // monster attacker, we can assume the target is seen. We only need to
        // check if target is seen when player is attacking.
        bool can_attacker_see_tgt = true;

        if (attacker && attacker->is_player())
        {
                can_attacker_see_tgt = can_player_see_actor(*defender);
        }

        // Check for extra attack bonuses, such as defender being immobilized.

        // TODO: This is weird - just handle dodging penalties with properties!
        bool is_big_att_bon = false;
        bool is_small_att_bon = false;

        if (!is_defender_aware)
        {
                // Give big attack bonus if defender is unaware of the attacker.
                is_big_att_bon = true;
        }

        if (!is_big_att_bon)
        {
                // Check if attacker gets a bonus due to a defender property.

                const bool has_big_bon_prop =
                        defender->m_properties.has(PropId::paralyzed) ||
                        defender->m_properties.has(PropId::nailed) ||
                        defender->m_properties.has(PropId::fainted) ||
                        defender->m_properties.has(PropId::entangled);

                const bool has_small_bon_prop =
                        defender->m_properties.has(PropId::confused) ||
                        defender->m_properties.has(PropId::slowed) ||
                        defender->m_properties.has(PropId::burning) ||
                        !defender->m_properties.allow_see();

                if (has_big_bon_prop)
                {
                        is_big_att_bon = true;
                }
                else if (has_small_bon_prop)
                {
                        is_small_att_bon = true;
                }
        }

        int state_mod =
                is_big_att_bon
                ? 50
                : (is_small_att_bon ? 20 : 0);

        // Lower hit chance if attacker cannot see target (e.g. attacking
        // invisible creature)
        if (!can_attacker_see_tgt)
        {
                state_mod -= g_hit_chance_pen_vs_unseen;
        }

        // Lower hit chance if defender is ethereal (except if Bane of the
        // Undead bonus applies)
        const bool apply_undead_bane_bon =
                (attacker && attacker->is_player()) &&
                player_bon::gets_undead_bane_bon(*defender->m_data);

        const bool apply_ethereal_defender_pen =
                defender->m_properties.has(PropId::ethereal) &&
                !apply_undead_bane_bon;

        if (apply_ethereal_defender_pen)
        {
                state_mod -= 50;
        }

        hit_chance_tot =
                skill_mod +
                wpn_mod +
                dodging_mod +
                state_mod;

        if (is_mon_hit_chance_penalty(attacker, defender))
        {
                hit_chance_tot = 0;
        }

        hit_chance_tot = std::clamp(hit_chance_tot, 5, 99);

        dmg_range = wpn.melee_dmg(attacker);

        if (apply_undead_bane_bon)
        {
                dmg_range.set_plus(dmg_range.plus() + 2);
        }

        if (attacker && attacker->m_properties.has(PropId::weakened))
        {
                // Weak attack (halved damage)
                dmg_range = dmg_range.scaled_pct(50);

                is_weak_attack = true;
        }
        // Attacker not weakened, or not an actor attacking (e.g. a trap)
        else if (attacker && !is_defender_aware)
        {
                // The attack is a backstab
                int dmg_pct = 150;

                // Extra backstab damage from traits?
                if (attacker && attacker->is_player())
                {
                        if (player_bon::has_trait(Trait::vicious))
                        {
                                dmg_pct += 100;
                        }

                        if (player_bon::has_trait(Trait::ruthless))
                        {
                                dmg_pct += 100;
                        }
                }

                // +200% damage if attacking with a dagger
                const auto id = wpn.data().id;

                if ((id == item::Id::dagger) ||
                    (id == item::Id::spirit_dagger))
                {
                        dmg_pct += 200;
                }

                dmg_range = dmg_range.scaled_pct(dmg_pct);

                is_backstab = true;
        }

        // Defender takes reduced damage from piercing attacks?
        if (defender->m_properties.has(PropId::reduced_pierce_dmg) &&
            (wpn.data().melee.dmg_type == DmgType::piercing))
        {
                dmg_range = dmg_range.scaled_pct(25);
        }

        if (config::is_gj_mode() && attacker && defender->is_player())
        {
                dmg_range = dmg_range.scaled_pct(200);
        }
}

RangedAttData::RangedAttData(
        actor::Actor* const the_attacker,
        const P& attacker_origin,
        const P& the_aim_pos,
        const P& current_pos,
        const item::Wpn& wpn,
        std::optional<actor::Size> aim_lvl_override) :
        AttData(the_attacker, nullptr, wpn),
        aim_pos(the_aim_pos)
{
        // Determine aim level
        if (aim_lvl_override)
        {
                aim_lvl = aim_lvl_override.value();
        }
        else
        {
                // Aim level not overriden by caller

                // TODO: Quick hack, Incinerators always aim at the floor
                if (wpn.id() == item::Id::incinerator)
                {
                        aim_lvl = actor::Size::floor;
                }
                else
                {
                        // Not incinerator
                        auto* const actor_aimed_at =
                                map::first_actor_at_pos(aim_pos);

                        if (actor_aimed_at)
                        {
                                aim_lvl = actor_aimed_at->m_data->actor_size;
                        }
                        else
                        {
                                // No actor aimed at
                                const bool is_cell_blocked =
                                        map_parsers::BlocksProjectiles()
                                                .run(aim_pos);

                                aim_lvl =
                                        is_cell_blocked
                                        ? actor::Size::humanoid
                                        : actor::Size::floor;
                        }
                }
        }

        defender = map::first_actor_at_pos(current_pos);

        if (!defender || (defender == attacker))
        {
                return;
        }

        TRACE_VERBOSE << "Defender found" << std::endl;

        const int skill_mod =
                attacker
                ? attacker->ability(AbilityId::ranged, true)
                : 50;

        const int wpn_mod = wpn.data().ranged.hit_chance_mod;

        const bool is_defender_aware =
                is_defender_aware_of_attack(attacker, *defender);

        int dodging_mod = 0;

        const bool player_is_handling_armor =
                (map::g_player->m_equip_armor_countdown > 0) ||
                (map::g_player->m_remove_armor_countdown);

        const bool allow_positive_doge =
                is_defender_aware &&
                !(defender->is_player() &&
                  player_is_handling_armor);

        const int dodging_ability =
                defender->ability(AbilityId::dodging, true);

        if (allow_positive_doge || (dodging_ability < 0))
        {
                const int defender_dodging =
                        defender->ability(
                                AbilityId::dodging,
                                true);

                dodging_mod = -defender_dodging;
        }

        const auto dist = king_dist(attacker_origin, current_pos);

        const auto effective_range = wpn.data().ranged.effective_range;

        int dist_mod = 0;

        if (dist >= effective_range.min)
        {
                // Normal distance modifier
                dist_mod = 15 - (dist * 5);
        }
        else
        {
                // Closer than effective range
                dist_mod = -50 + (dist * 5);
        }

        defender_size = defender->m_data->actor_size;

        int state_mod = 0;

        // Lower hit chance if attacker cannot see target (e.g.
        // attacking invisible creature)
        if (attacker)
        {
                bool can_attacker_see_tgt = true;

                if (attacker->is_player())
                {
                        can_attacker_see_tgt =
                                can_player_see_actor(*defender);
                }
                else
                {
                        // Attacker is monster
                        Array2<bool> hard_blocked_los(map::dims());

                        const R fov_rect =
                                fov::fov_rect(
                                        attacker->m_pos,
                                        hard_blocked_los.dims());

                        map_parsers::BlocksLos()
                                .run(hard_blocked_los,
                                     fov_rect,
                                     MapParseMode::overwrite);

                        can_attacker_see_tgt =
                                can_mon_see_actor(
                                        *attacker,
                                        *defender,
                                        hard_blocked_los);
                }

                if (!can_attacker_see_tgt)
                {
                        state_mod -= g_hit_chance_pen_vs_unseen;
                }
        }

        // Player gets attack bonus for attacking unaware monster
        if (attacker && attacker->is_player())
        {
                if (!defender->is_aware_of_player())
                {
                        state_mod += 25;
                }
        }

        const bool apply_undead_bane_bon =
                (attacker == map::g_player) &&
                player_bon::gets_undead_bane_bon(*defender->m_data);

        const bool apply_ethereal_defender_pen =
                defender->m_properties.has(PropId::ethereal) &&
                !apply_undead_bane_bon;

        if (apply_ethereal_defender_pen)
        {
                state_mod -= 50;
        }

        hit_chance_tot =
                skill_mod +
                wpn_mod +
                dodging_mod +
                dist_mod +
                state_mod;

        if (is_mon_hit_chance_penalty(attacker, defender))
        {
                hit_chance_tot = 0;
        }

        hit_chance_tot = std::clamp(hit_chance_tot, 5, 99);

        dmg_range = wpn.ranged_dmg(attacker);

        if ((attacker == map::g_player) &&
            player_bon::gets_undead_bane_bon(*defender->m_data))
        {
                dmg_range.set_plus(dmg_range.plus() + 2);
        }

        const bool is_player_with_aiming_prop =
                attacker &&
                attacker->is_player() &&
                attacker->m_properties.has(PropId::aiming);

        if (is_player_with_aiming_prop)
        {
                dmg_range.set_base_min(dmg_range.base_max());
        }

        // Positions further than max range have halved damage
        if (dist > effective_range.max)
        {
                dmg_range = dmg_range.scaled_pct(50);
        }

        // Defender takes reduced damage from piercing attacks?
        if (defender->m_properties.has(PropId::reduced_pierce_dmg) &&
            (wpn.data().ranged.dmg_type == DmgType::piercing))
        {
                dmg_range = dmg_range.scaled_pct(25);
        }

        if (config::is_gj_mode() && attacker && defender->is_player())
        {
                dmg_range = dmg_range.scaled_pct(200);
        }
}

ThrowAttData::ThrowAttData(
        actor::Actor* const the_attacker,
        const P& attacker_origin,
        const P& aim_pos,
        const P& current_pos,
        const item::Item& item) :
        AttData(the_attacker, nullptr, item)
{
        if (!attacker)
        {
                ASSERT(false);

                return;
        }

        auto* const actor_aimed_at = map::first_actor_at_pos(aim_pos);

        // Determine aim level
        if (actor_aimed_at)
        {
                aim_lvl = actor_aimed_at->m_data->actor_size;
        }
        else
        {
                // Not aiming at actor
                const bool is_cell_blocked =
                        map_parsers::BlocksProjectiles()
                                .run(current_pos);

                aim_lvl =
                        is_cell_blocked
                        ? actor::Size::humanoid
                        : actor::Size::floor;
        }

        defender = map::first_actor_at_pos(current_pos);

        if (!defender || (defender == attacker))
        {
                return;
        }

        TRACE_VERBOSE << "Defender found" << std::endl;

        const int skill_mod =
                attacker
                ? attacker->ability(AbilityId::ranged, true)
                : 50;

        const int wpn_mod = item.data().ranged.throw_hit_chance_mod;

        const bool is_defender_aware =
                is_defender_aware_of_attack(attacker, *defender);

        int dodging_mod = 0;

        const bool player_is_handling_armor =
                (map::g_player->m_equip_armor_countdown > 0) ||
                (map::g_player->m_remove_armor_countdown);

        const int dodging_ability =
                defender->ability(AbilityId::dodging, true);

        const bool allow_positive_doge =
                is_defender_aware &&
                !(defender->is_player() &&
                  player_is_handling_armor);

        if (allow_positive_doge || (dodging_ability < 0))
        {
                const int defender_dodging =
                        defender->ability(AbilityId::dodging, true);

                dodging_mod = -defender_dodging;
        }

        const auto dist = king_dist(attacker_origin, current_pos);

        const auto effective_range = item.data().ranged.effective_range;

        int dist_mod = 0;

        if (dist >= effective_range.min)
        {
                // Normal distance modifier
                dist_mod = 15 - (dist * 5);
        }
        else
        {
                // Closer than effective range
                dist_mod = -50 + (dist * 5);
        }

        defender_size = defender->m_data->actor_size;

        int state_mod = 0;

        bool can_attacker_see_tgt = true;

        if (attacker == map::g_player)
        {
                can_attacker_see_tgt =
                        can_player_see_actor(*defender);
        }

        // Lower hit chance if attacker cannot see target (e.g.
        // attacking invisible creature)
        if (!can_attacker_see_tgt)
        {
                state_mod -= g_hit_chance_pen_vs_unseen;
        }

        // Player gets attack bonus for attacking unaware monster
        if (attacker == map::g_player)
        {
                if (!defender->is_aware_of_player())
                {
                        state_mod += 25;
                }
        }

        const bool apply_undead_bane_bon =
                (attacker == map::g_player) &&
                player_bon::gets_undead_bane_bon(*defender->m_data);

        const bool apply_ethereal_defender_pen =
                defender->m_properties.has(PropId::ethereal) &&
                !apply_undead_bane_bon;

        if (apply_ethereal_defender_pen)
        {
                state_mod -= 50;
        }

        hit_chance_tot =
                skill_mod +
                wpn_mod +
                dodging_mod +
                dist_mod +
                state_mod;

        if (is_mon_hit_chance_penalty(attacker, defender))
        {
                hit_chance_tot = 0;
        }

        hit_chance_tot = std::clamp(hit_chance_tot, 5, 99);

        dmg_range = item.thrown_dmg(attacker);

        if (apply_undead_bane_bon)
        {
                dmg_range.set_plus(dmg_range.plus() + 2);
        }

        const bool is_player_with_aiming_prop =
                (attacker == map::g_player) &&
                attacker->m_properties.has(PropId::aiming);

        if (is_player_with_aiming_prop)
        {
                dmg_range.set_base_min(dmg_range.base_max());
        }

        // Positions further than max range have halved damage
        if (dist > effective_range.max)
        {
                dmg_range = dmg_range.scaled_pct(50);
        }

        // Defender takes reduced damage from piercing attacks?
        if (defender->m_properties.has(PropId::reduced_pierce_dmg) &&
            (item.data().ranged.dmg_type == DmgType::piercing))
        {
                dmg_range = dmg_range.scaled_pct(25);
        }

        if (config::is_gj_mode() && attacker && defender->is_player())
        {
                dmg_range = dmg_range.scaled_pct(200);
        }
}

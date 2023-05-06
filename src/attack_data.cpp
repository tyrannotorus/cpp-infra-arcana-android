// =============================================================================
// Copyright 2011-2023 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "attack_data.hpp"

#include <algorithm>
#include <ostream>

#include "ability_values.hpp"
#include "actor.hpp"
#include "actor_data.hpp"
#include "actor_player_state.hpp"
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
#include "wpn_dmg.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static bool is_defender_aware_of_attack(
        const actor::Actor* const attacker,
        const actor::Actor& defender)
{
        if (!attacker) {
                return true;
        }
        else if (actor::is_player(&defender)) {
                // Monster attacking player
                return attacker->is_player_aware_of_me();
        }
        else if (actor::is_player(attacker)) {
                // Player attacking monster
                return (
                        defender.is_aware_of_player() ||
                        defender.is_actor_my_leader(map::g_player));
        }
        else if (defender.is_actor_my_leader(map::g_player)) {
                // Hostile monster attacking player-allied monster

                // The player-allied monster is considered aware, if the player
                // is aware of the attacker.
                return attacker->is_player_aware_of_me();
        }
        else {
                // Player-allied monster attacking hostile monster

                // The hostile monster is considererd aware, if it is aware of
                // the player.
                return defender.is_aware_of_player();
        }
}

static bool is_mon_hit_chance_penalty(
        const actor::Actor* const attacker,
        const actor::Actor* const defender)
{
        return (
                attacker &&
                !actor::is_player(attacker) &&
                actor::is_player(defender) &&
                attacker->m_give_hit_chance_penalty_vs_player);
}

static int get_attacker_melee_skill(const actor::Actor* const attacker)
{
        if (attacker) {
                return attacker->ability(AbilityId::melee, true);
        }
        else {
                // No attacker (e.g. trap), use default value
                return 50;
        }
}

static int get_attacker_ranged_skill(const actor::Actor* const attacker)
{
        if (attacker) {
                return attacker->ability(AbilityId::ranged, true);
        }
        else {
                // No attacker (e.g. trap), use default value
                return 50;
        }
}

static actor::Size calc_aim_lvl_at(const P& aim_pos)
{
        auto* const actor_aimed_at = map::living_actor_at(aim_pos);

        if (actor_aimed_at) {
                return actor_aimed_at->m_data->actor_size;
        }
        else {
                // No actor aimed at
                const bool is_cell_blocked =
                        map_parsers::BlocksProjectiles()
                                .run(aim_pos);

                const auto aim_lvl =
                        is_cell_blocked
                        ? actor::Size::humanoid
                        : actor::Size::floor;

                return aim_lvl;
        }
}

static int calc_ranged_dist_hit_mod(
        const int dist,
        const Range& effective_range)
{
        if (dist >= effective_range.min) {
                // Normal distance modifier
                return 15 - (dist * 5);
        }
        else {
                // Closer than effective range
                return -50 + (dist * 5);
        }
}

static bool is_player_undead_bane_bon(
        const actor::Actor* const attacker,
        const actor::ActorData& defender_data)
{
        return (
                actor::is_player(attacker) &&
                player_bon::has_trait(Trait::undead_bane) &&
                defender_data.is_undead);
}

static bool is_reduced_pierce_dmg(
        const DmgType dmg_type,
        const actor::Actor& defender)
{
        return (
                defender.m_properties.has(prop::Id::reduced_pierce_dmg) &&
                (dmg_type == DmgType::piercing));
}

static bool is_player_handling_armor()
{
        return (
                (actor::player_state::g_equip_armor_countdown > 0) ||
                (actor::player_state::g_remove_armor_countdown));
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

        const int skill_mod = get_attacker_melee_skill(attacker);
        const int wpn_mod = wpn.data().melee.hit_chance_mod;

        int dodging_mod = 0;

        const int dodging_ability = defender->ability(AbilityId::dodging, true);

        const bool allow_positive_doge =
                is_defender_aware &&
                !(actor::is_player(defender) && is_player_handling_armor());

        if (allow_positive_doge || (dodging_ability < 0)) {
                dodging_mod -= dodging_ability;
        }

        // Attacker gets a penalty against unseen targets

        // NOTE: The AI never attacks unseen targets, so in the case of a
        // monster attacker, we can assume the target is seen. We only need to
        // check if target is seen when player is attacking.
        bool can_attacker_see_tgt = true;

        if (actor::is_player(attacker)) {
                can_attacker_see_tgt = can_player_see_actor(*defender);
        }

        // Check for extra attack bonuses, such as defender being immobilized.

        // TODO: This is weird - just handle dodging penalties with properties!
        bool is_big_att_bon = false;
        bool is_small_att_bon = false;

        if (!is_defender_aware) {
                // Give big attack bonus if defender is unaware of the attacker.
                is_big_att_bon = true;
        }

        if (!is_big_att_bon) {
                // Check if attacker gets a bonus due to a defender property.

                const bool has_big_bon_prop =
                        defender->m_properties.has(prop::Id::paralyzed) ||
                        defender->m_properties.has(prop::Id::nailed) ||
                        defender->m_properties.has(prop::Id::fainted) ||
                        defender->m_properties.has(prop::Id::entangled) ||
                        defender->m_properties.has(prop::Id::stuck);

                const bool has_small_bon_prop =
                        defender->m_properties.has(prop::Id::confused) ||
                        defender->m_properties.has(prop::Id::slowed) ||
                        defender->m_properties.has(prop::Id::burning) ||
                        !defender->m_properties.allow_see();

                if (has_big_bon_prop) {
                        is_big_att_bon = true;
                }
                else if (has_small_bon_prop) {
                        is_small_att_bon = true;
                }
        }

        int state_mod = 0;

        if (is_big_att_bon) {
                state_mod = 50;
        }
        else if (is_small_att_bon) {
                state_mod = 20;
        }

        if (!can_attacker_see_tgt) {
                state_mod -= g_hit_chance_pen_vs_unseen;
        }

        const bool apply_undead_bane_bon =
                is_player_undead_bane_bon(
                        attacker,
                        *defender->m_data);

        const bool apply_ethereal_defender_pen =
                defender->m_properties.has(prop::Id::ethereal) &&
                !apply_undead_bane_bon;

        if (apply_ethereal_defender_pen) {
                state_mod -= 50;
        }

        hit_chance_tot =
                skill_mod +
                wpn_mod +
                dodging_mod +
                state_mod;

        if (is_mon_hit_chance_penalty(attacker, defender)) {
                hit_chance_tot = 0;
        }

        hit_chance_tot = std::clamp(hit_chance_tot, 5, 99);

        dmg_range = wpn.melee_dmg(attacker);

        if (apply_undead_bane_bon) {
                dmg_range.set_plus(dmg_range.plus() + 2);
        }

        if (attacker && attacker->m_properties.has(prop::Id::weakened)) {
                // Weak attack (halved damage)
                dmg_range = dmg_range.scaled_pct(50);

                is_weak_attack = true;
        }
        // Attacker not weakened, or not an actor attacking (e.g. a trap)
        else if (attacker && !is_defender_aware) {
                // The attack is a backstab
                int dmg_pct = 150;

                // Extra backstab damage from traits?
                if (actor::is_player(attacker)) {
                        if (player_bon::has_trait(Trait::vicious)) {
                                dmg_pct += 100;
                        }

                        if (player_bon::has_trait(Trait::ruthless)) {
                                dmg_pct += 100;
                        }
                }

                // +200% damage if attacking with a dagger
                const auto id = wpn.data().id;

                if ((id == item::Id::dagger) ||
                    (id == item::Id::shadow_dagger)) {
                        dmg_pct += 200;
                }

                dmg_range = dmg_range.scaled_pct(dmg_pct);

                is_backstab = true;
        }

        if (is_reduced_pierce_dmg(wpn.data().melee.dmg_type, *defender)) {
                dmg_range = dmg_range.scaled_pct(25);
        }

        if (config::is_gj_mode() && attacker && actor::is_player(defender)) {
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
        if (aim_lvl_override) {
                aim_lvl = aim_lvl_override.value();
        }
        else {
                // Aim level not overriden by caller

                // This weapon always aim at the floor
                if (wpn.id() == item::Id::incinerator) {
                        aim_lvl = actor::Size::floor;
                }
                else {
                        aim_lvl = calc_aim_lvl_at(aim_pos);
                }
        }

        defender = map::living_actor_at(current_pos);

        if (!defender || (defender == attacker)) {
                return;
        }

        TRACE_VERBOSE << "Defender found" << std::endl;

        const int skill_mod = get_attacker_ranged_skill(attacker);
        const int wpn_mod = wpn.data().ranged.hit_chance_mod;

        const bool is_defender_aware =
                is_defender_aware_of_attack(attacker, *defender);

        int dodging_mod = 0;

        const bool allow_positive_doge =
                is_defender_aware &&
                !(actor::is_player(defender) && is_player_handling_armor());

        const int dodging_ability =
                defender->ability(AbilityId::dodging, true);

        if (allow_positive_doge || (dodging_ability < 0)) {
                const int defender_dodging =
                        defender->ability(
                                AbilityId::dodging,
                                true);

                dodging_mod = -defender_dodging;
        }

        const auto dist = king_dist(attacker_origin, current_pos);

        const auto effective_range = wpn.data().ranged.effective_range;

        const int dist_mod = calc_ranged_dist_hit_mod(dist, effective_range);

        defender_size = defender->m_data->actor_size;

        int state_mod = 0;

        // Lower hit chance if attacker cannot see target (e.g.
        // attacking invisible creature)
        if (attacker) {
                bool can_attacker_see_tgt = true;

                if (actor::is_player(attacker)) {
                        can_attacker_see_tgt =
                                can_player_see_actor(*defender);
                }
                else {
                        // Attacker is monster
                        Array2<bool> hard_blocked_los(map::dims());

                        const auto fov_rect =
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

                if (!can_attacker_see_tgt) {
                        state_mod -= g_hit_chance_pen_vs_unseen;
                }
        }

        if (actor::is_player(attacker) && !defender->is_aware_of_player()) {
                state_mod += 25;
        }

        const bool apply_undead_bane_bon =
                is_player_undead_bane_bon(
                        attacker,
                        *defender->m_data);

        const bool apply_ethereal_defender_pen =
                defender->m_properties.has(prop::Id::ethereal) &&
                !apply_undead_bane_bon;

        if (apply_ethereal_defender_pen) {
                state_mod -= 50;
        }

        hit_chance_tot =
                skill_mod +
                wpn_mod +
                dodging_mod +
                dist_mod +
                state_mod;

        if (is_mon_hit_chance_penalty(attacker, defender)) {
                hit_chance_tot = 0;
        }

        hit_chance_tot = std::clamp(hit_chance_tot, 5, 99);

        dmg_range = wpn.ranged_dmg(attacker);

        if (apply_undead_bane_bon) {
                dmg_range.set_plus(dmg_range.plus() + 2);
        }

        const bool is_player_with_aiming_prop =
                actor::is_player(attacker) &&
                attacker->m_properties.has(prop::Id::aiming);

        if (is_player_with_aiming_prop) {
                dmg_range.set_base_min(dmg_range.base_max());
        }

        if (!effective_range.is_in_range(dist)) {
                dmg_range = dmg_range.scaled_pct(50);
        }

        if (is_reduced_pierce_dmg(wpn.data().ranged.dmg_type, *defender)) {
                dmg_range = dmg_range.scaled_pct(25);
        }

        if (config::is_gj_mode() && attacker && actor::is_player(defender)) {
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
        if (!attacker) {
                ASSERT(false);

                return;
        }

        aim_lvl = calc_aim_lvl_at(aim_pos);

        defender = map::living_actor_at(current_pos);

        if (!defender || (defender == attacker)) {
                return;
        }

        TRACE_VERBOSE << "Defender found" << std::endl;

        const int skill_mod = get_attacker_ranged_skill(attacker);
        const int wpn_mod = item.data().ranged.throw_hit_chance_mod;

        const bool is_defender_aware =
                is_defender_aware_of_attack(attacker, *defender);

        int dodging_mod = 0;

        const int dodging_ability =
                defender->ability(AbilityId::dodging, true);

        const bool allow_positive_doge =
                is_defender_aware &&
                !(actor::is_player(defender) && is_player_handling_armor());

        if (allow_positive_doge || (dodging_ability < 0)) {
                const int defender_dodging =
                        defender->ability(AbilityId::dodging, true);

                dodging_mod = -defender_dodging;
        }

        const auto dist = king_dist(attacker_origin, current_pos);

        const auto effective_range = item.data().ranged.effective_range;

        const int dist_mod = calc_ranged_dist_hit_mod(dist, effective_range);

        defender_size = defender->m_data->actor_size;

        int state_mod = 0;

        bool can_attacker_see_tgt = true;

        if (actor::is_player(attacker)) {
                can_attacker_see_tgt =
                        can_player_see_actor(*defender);
        }

        if (!can_attacker_see_tgt) {
                state_mod -= g_hit_chance_pen_vs_unseen;
        }

        if (actor::is_player(attacker) && !defender->is_aware_of_player()) {
                state_mod += 25;
        }

        const bool apply_undead_bane_bon =
                is_player_undead_bane_bon(
                        attacker,
                        *defender->m_data);

        const bool apply_ethereal_defender_pen =
                defender->m_properties.has(prop::Id::ethereal) &&
                !apply_undead_bane_bon;

        if (apply_ethereal_defender_pen) {
                state_mod -= 50;
        }

        hit_chance_tot =
                skill_mod +
                wpn_mod +
                dodging_mod +
                dist_mod +
                state_mod;

        if (is_mon_hit_chance_penalty(attacker, defender)) {
                hit_chance_tot = 0;
        }

        hit_chance_tot = std::clamp(hit_chance_tot, 5, 99);

        dmg_range = item.thrown_dmg(attacker);

        if (apply_undead_bane_bon) {
                dmg_range.set_plus(dmg_range.plus() + 2);
        }

        const bool is_player_with_aiming_prop =
                actor::is_player(attacker) &&
                attacker->m_properties.has(prop::Id::aiming);

        if (is_player_with_aiming_prop) {
                dmg_range.set_base_min(dmg_range.base_max());
        }

        if (!effective_range.is_in_range(dist)) {
                dmg_range = dmg_range.scaled_pct(50);
        }

        if (is_reduced_pierce_dmg(item.data().ranged.dmg_type, *defender)) {
                dmg_range = dmg_range.scaled_pct(25);
        }

        if (config::is_gj_mode() && attacker && actor::is_player(defender)) {
                dmg_range = dmg_range.scaled_pct(200);
        }
}

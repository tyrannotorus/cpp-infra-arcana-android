// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "actor_std_turn.hpp"

#include <algorithm>
#include <cstddef>
#include <vector>

#include "actor.hpp"
#include "actor_data.hpp"
#include "actor_hit.hpp"
#include "actor_player_state.hpp"
#include "ai.hpp"
#include "array2.hpp"
#include "debug.hpp"
#include "game_time.hpp"
#include "global.hpp"
#include "inventory.hpp"
#include "item.hpp"
#include "item_data.hpp"
#include "map.hpp"
#include "msg_log.hpp"
#include "player_bon.hpp"
#include "property.hpp"
#include "property_data.hpp"
#include "property_factory.hpp"
#include "property_handler.hpp"
#include "random.hpp"
#include "smell.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static int calc_player_turns_per_hp_regen_rate()
{
        auto& player = *map::g_player;

        int nr_turns_per_hp = 0;

        // Rapid Recoverer trait affects hp regen?
        if (player_bon::has_trait(Trait::rapid_recoverer)) {
                nr_turns_per_hp = 2;
        }
        else {
                nr_turns_per_hp = 20;
        }

        // Wounds affect hp regen?
        int nr_wounds = 0;

        if (player.m_properties.has(PropId::wound)) {
                auto* const wound =
                        static_cast<PropWound*>(
                                player.m_properties.prop(PropId::wound));

                nr_wounds = wound->nr_wounds();
        }

        int wound_turns_penalty = nr_wounds * 4;

        if (player_bon::has_trait(Trait::survivalist)) {
                wound_turns_penalty /= 2;
        }

        nr_turns_per_hp += wound_turns_penalty;

        // Items affect hp regen?
        for (const auto& slot : player.m_inv.m_slots) {
                if (slot.item) {
                        nr_turns_per_hp +=
                                slot.item->hp_regen_change(
                                        InvType::slots);
                }
        }

        for (const auto* const item : player.m_inv.m_backpack) {
                nr_turns_per_hp +=
                        item->hp_regen_change(InvType::backpack);
        }

        nr_turns_per_hp = std::max(1, nr_turns_per_hp);

        return nr_turns_per_hp;
}

static void player_regen_hp()
{
        auto& player = *map::g_player;

        if ((player.m_hp >= actor::max_hp(player)) ||
            (game_time::turn_nr() <= 1) ||
            player.m_properties.has(PropId::poisoned) ||
            player.m_properties.has(PropId::disabled_hp_regen) ||
            (player_bon::bg() == Bg::ghoul)) {
                return;
        }

        const int nr_turns_per_hp = calc_player_turns_per_hp_regen_rate();
        const int turn = game_time::turn_nr();

        if ((turn % nr_turns_per_hp) != 0) {
                return;
        }

        ++player.m_hp;
}

static Range calc_nr_turns_range_to_recharge_spell_shield()
{
        Range range;

        if (player_bon::has_trait(Trait::mighty_spirit)) {
                range = {25, 50};
        }
        else if (player_bon::has_trait(Trait::strong_spirit)) {
                range = {75, 100};
        }
        else {
                range = {125, 150};
        }

        // Halved number of turns due to the Talisman of Reflection?
        if (map::g_player->m_inv.has_item_in_backpack(item::Id::refl_talisman)) {
                range.min /= 2;
                range.max /= 2;
        }

        return range;
}

static void player_regen_spell_shield()
{
        auto& player = *map::g_player;

        if (player.m_properties.has(PropId::r_spell)) {
                // Player already has spell resistance. Keep resetting the
                // countdown to "uninitialized" while in this state, and do
                // nothing else. This will trigger a reroll of the duration when
                // the countdown can begin again.
                actor::player_state::g_nr_turns_until_r_spell = -1;

                return;
        }

        // Spell shield not currently active.

        if (!player_bon::has_trait(Trait::stout_spirit)) {
                return;
        }

        // Player has at least stout spirit.

        if (actor::player_state::g_nr_turns_until_r_spell <= 0) {
                // Cooldown has finished, OR countdown not initialized.

                if (actor::player_state::g_nr_turns_until_r_spell == 0) {
                        // Cooldown has finished
                        auto* prop = property_factory::make(PropId::r_spell);

                        prop->set_indefinite();

                        player.m_properties.apply(prop);

                        msg_log::more_prompt();
                }

                actor::player_state::g_nr_turns_until_r_spell =
                        calc_nr_turns_range_to_recharge_spell_shield()
                                .roll();
        }

        if (!player.m_properties.has(PropId::r_spell) &&
            (actor::player_state::g_nr_turns_until_r_spell > 0)) {
                // Spell resistance is in cooldown state, decrement number of
                // remaining turns.
                --actor::player_state::g_nr_turns_until_r_spell;
        }
}

static void player_regen_meditative_focused()
{
        auto& player = *map::g_player;

        if (player.m_properties.has(PropId::meditative_focused) ||
            player.m_properties.has(PropId::frenzied)) {
                // Player is already focused, or is frenzied. Keep resetting the
                // countdown to "uninitialized" while in this state, and do
                // nothing else. This will trigger a reroll of the duration when
                // the countdown can begin again.
                actor::player_state::g_nr_turns_until_meditative_focused = -1;

                return;
        }

        // Meditative focused not currently active.

        if (!player_bon::has_trait(Trait::meditative)) {
                return;
        }

        // Player has meditative trait.

        int& nr_turns_until_focused =
                actor::player_state::g_nr_turns_until_meditative_focused;

        if (nr_turns_until_focused <= 0) {
                // Cooldown has finished, OR countdown not initialized.

                if (nr_turns_until_focused == 0) {
                        // Cooldown has finished
                        Prop* prop =
                                property_factory::make(
                                        PropId::meditative_focused);

                        prop->set_indefinite();

                        player.m_properties.apply(prop);

                        msg_log::more_prompt();
                }

                nr_turns_until_focused = rnd::range(125, 150);
        }

        if (!player.m_properties.has(PropId::meditative_focused) &&
            (nr_turns_until_focused > 0)) {
                // Meditative focused is in cooldown state, decrement number of
                // remaining turns.
                --nr_turns_until_focused;
        }
}

static void player_std_turn()
{
        auto& player = *map::g_player;

#ifndef NDEBUG
        // Disease and infection should not be active at the same time
        ASSERT(!player.m_properties.has(PropId::diseased) ||
               !player.m_properties.has(PropId::infected));
#endif  // NDEBUG

        if (!player.is_alive()) {
                return;
        }

        player_regen_spell_shield();

        player_regen_meditative_focused();

        if (actor::player_state::g_active_explosive) {
                actor::player_state::g_active_explosive
                        ->on_std_turn_player_hold_ignited();

                if (!map::g_player->is_alive()) {
                        return;
                }
        }

        player_regen_hp();
}

static void mon_std_turn(actor::Actor& mon)
{
        smell::put_smell_for_mon(mon);

        // Countdown all spell cooldowns
        for (auto& spell : mon.m_mon_spells) {
                int& cooldown = spell.cooldown;

                if (cooldown > 0) {
                        --cooldown;
                }
        }

        // NOTE: Monsters try to detect the player visually on standard turns,
        // otherwise very fast monsters are much better at finding the player.
        if (mon.is_alive() &&
            mon.m_data->ai[(size_t)actor::AiId::looks] &&
            !actor::is_player(mon.m_leader) &&
            !map::g_player->m_properties.has(PropId::sanctuary) &&
            (actor::is_player(mon.m_ai_state.target) ||
             !mon.m_ai_state.target)) {
                ai::info::look(mon);
        }
}

static void std_turn_common(actor::Actor& actor)
{
        // Do light damage if in lit cell
        if (map::g_light.at(actor.m_pos)) {
                actor::hit(actor, 1, DmgType::light, nullptr);
        }

        if (!actor.is_alive()) {
                return;
        }

        // Slowly decrease current HP/spirit if above max
        const int decr_above_max_n_turns = 7;

        // Monsters decrement their HP every standard turn when above max (so
        // that things draining max HP will have an affect faster), while the
        // player can have HP above max longer.
        bool decr_this_turn = true;

        if (actor::is_player(&actor)) {
                decr_this_turn =
                        ((game_time::turn_nr() %
                          decr_above_max_n_turns) == 0);
        }

        const bool is_hp_above_max = (actor.m_hp > actor::max_hp(actor));

        if (is_hp_above_max && decr_this_turn) {
                TRACE << actor.m_hp << std::endl;

                --actor.m_hp;
        }

        const bool is_sp_above_max = (actor.m_sp > actor::max_sp(actor));

        const bool is_exorcist = player_bon::is_bg(Bg::exorcist);

        if (!is_exorcist && is_sp_above_max && decr_this_turn) {
                --actor.m_sp;
        }

        // Regenerate spirit
        int regen_sp_n_turns = 18;

        if (actor::is_player(&actor)) {
                if (player_bon::has_trait(Trait::stout_spirit)) {
                        regen_sp_n_turns -= 4;
                }

                if (player_bon::has_trait(Trait::strong_spirit)) {
                        regen_sp_n_turns -= 4;
                }

                if (player_bon::has_trait(Trait::mighty_spirit)) {
                        regen_sp_n_turns -= 4;
                }
        }
        else {
                // Is monster

                // Monsters regen spirit very quickly, so spell casters
                // doesn't suddenly get completely handicapped
                regen_sp_n_turns = 1;
        }

        const bool regen_sp_this_turn =
                ((game_time::turn_nr() % regen_sp_n_turns) == 0);

        if (regen_sp_this_turn) {
                actor.restore_sp(1, false, Verbose::no);
        }
}

// -----------------------------------------------------------------------------
// actor
// -----------------------------------------------------------------------------
namespace actor
{
void std_turn(Actor& actor)
{
        std_turn_common(actor);

        if (actor::is_player(&actor)) {
                player_std_turn();
        }
        else {
                mon_std_turn(actor);
        }
}

}  // namespace actor

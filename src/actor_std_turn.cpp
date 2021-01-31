// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "actor_std_turn.hpp"

#include "actor.hpp"
#include "actor_hit.hpp"
#include "actor_mon.hpp"
#include "actor_player.hpp"
#include "ai.hpp"
#include "game_time.hpp"
#include "map.hpp"
#include "misc.hpp"
#include "property_factory.hpp"
#include "terrain.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static void player_std_turn()
{
        auto& player = *map::g_player;

#ifndef NDEBUG
        // Disease and infection should not be active at the same time
        ASSERT(!player.m_properties.has(PropId::diseased) ||
               !player.m_properties.has(PropId::infected));
#endif  // NDEBUG

        if (!player.is_alive())
        {
                return;
        }

        // Spell resistance
        const int spell_shield_turns_base = 125 + rnd::range(0, 25);

        const int spell_shield_turns_bon =
                player_bon::has_trait(Trait::mighty_spirit)
                ? 100
                : (player_bon::has_trait(Trait::strong_spirit) ? 50 : 0);

        int nr_turns_to_recharge_spell_shield = std::max(
                1,
                spell_shield_turns_base - spell_shield_turns_bon);

        // Halved number of turns due to the Talisman of Reflection?
        if (player.m_inv.has_item_in_backpack(item::Id::refl_talisman))
        {
                nr_turns_to_recharge_spell_shield /= 2;
        }

        if (player.m_properties.has(PropId::r_spell))
        {
                // We already have spell resistance (e.g. from casting the Spell
                // Shield spell), (re)set the cooldown to max number of turns
                player.m_nr_turns_until_rspell =
                        nr_turns_to_recharge_spell_shield;
        }
        else if (player_bon::has_trait(Trait::stout_spirit))
        {
                // Spell shield not active, and we have at least stout spirit
                if (player.m_nr_turns_until_rspell <= 0)
                {
                        // Cooldown has finished, OR countdown not initialized

                        if (player.m_nr_turns_until_rspell == 0)
                        {
                                // Cooldown has finished
                                auto* prop =
                                        property_factory::make(PropId::r_spell);

                                prop->set_indefinite();

                                player.m_properties.apply(prop);

                                msg_log::more_prompt();
                        }

                        player.m_nr_turns_until_rspell =
                                nr_turns_to_recharge_spell_shield;
                }

                if (!player.m_properties.has(PropId::r_spell) &&
                    (player.m_nr_turns_until_rspell > 0))
                {
                        // Spell resistance is in cooldown state, decrement
                        // number of remaining turns
                        --player.m_nr_turns_until_rspell;
                }
        }

        if (player.m_active_explosive)
        {
                player.m_active_explosive->on_std_turn_player_hold_ignited();
        }

        // Regenerate Hit Points
        if (!player.m_properties.has(PropId::poisoned) &&
            !player.m_properties.has(PropId::disabled_hp_regen) &&
            (player_bon::bg() != Bg::ghoul))
        {
                int nr_turns_per_hp = 0;

                // Rapid Recoverer trait affects hp regen?
                if (player_bon::has_trait(Trait::rapid_recoverer))
                {
                        nr_turns_per_hp = 2;
                }
                else
                {
                        nr_turns_per_hp = 20;
                }

                // Wounds affect hp regen?
                int nr_wounds = 0;

                if (player.m_properties.has(PropId::wound))
                {
                        auto* const prop =
                                player.m_properties.prop(PropId::wound);

                        auto* const wound =
                                static_cast<PropWound*>(prop);

                        nr_wounds = wound->nr_wounds();
                }

                const bool is_survivalist =
                        player_bon::has_trait(Trait::survivalist);

                const int wound_effect_div = is_survivalist ? 2 : 1;

                nr_turns_per_hp +=
                        ((nr_wounds * 4) / wound_effect_div);

                // Items affect hp regen?
                for (const auto& slot : player.m_inv.m_slots)
                {
                        if (slot.item)
                        {
                                nr_turns_per_hp +=
                                        slot.item->hp_regen_change(
                                                InvType::slots);
                        }
                }

                for (const auto* const item : player.m_inv.m_backpack)
                {
                        nr_turns_per_hp +=
                                item->hp_regen_change(InvType::backpack);
                }

                nr_turns_per_hp = std::max(1, nr_turns_per_hp);

                const int turn = game_time::turn_nr();
                const int max_hp = actor::max_hp(player);

                if ((player.m_hp < max_hp) &&
                    ((turn % nr_turns_per_hp) == 0) &&
                    turn > 1)
                {
                        ++player.m_hp;
                }
        }

        // Try to spot hidden traps and doors

        // NOTE: Skill value retrieved here is always at least 1
        const int player_search_skill =
                map::g_player->ability(AbilityId::searching, true);

        if (!player.m_properties.has(PropId::confused) &&
            player.m_properties.allow_see())
        {
                const size_t nr_positions = map::nr_positions();
                for (size_t i = 0; i < nr_positions; ++i)
                {
                        if (!map::g_seen.at(i))
                        {
                                continue;
                        }

                        auto* t = map::g_terrain.at(i);

                        if (!t->is_hidden())
                        {
                                continue;
                        }

                        const int lit_mod = map::g_light.at(i) ? 5 : 0;

                        const int dist = king_dist(player.m_pos, t->pos());

                        const int dist_mod = -((dist - 1) * 5);

                        int skill_tot =
                                player_search_skill +
                                lit_mod +
                                dist_mod;

                        if (skill_tot > 0)
                        {
                                const bool is_spotted =
                                        ability_roll::roll(skill_tot) >=
                                        ActionResult::success;

                                if (is_spotted)
                                {
                                        t->reveal(Verbose::yes);

                                        t->on_revealed_from_searching();

                                        msg_log::more_prompt();
                                }
                        }
                }
        }
}

static void mon_std_turn(actor::Mon& mon)
{
        // Countdown all spell cooldowns
        for (auto& spell : mon.m_mon_spells)
        {
                int& cooldown = spell.cooldown;

                if (cooldown > 0)
                {
                        --cooldown;
                }
        }

        // Monsters try to detect the player visually on standard turns,
        // otherwise very fast monsters are much better at finding the player
        if (mon.is_alive() &&
            mon.m_data->ai[(size_t)actor::AiId::looks] &&
            (mon.m_leader != map::g_player) &&
            !map::g_player->m_properties.has(PropId::sanctuary) &&
            (!mon.m_ai_state.target || mon.m_ai_state.target->is_player()))
        {
                ai::info::look(mon);
        }
}

static void std_turn_common(actor::Actor& actor)
{
        // Do light damage if in lit cell
        if (map::g_light.at(actor.m_pos))
        {
                actor::hit(actor, 1, DmgType::light);
        }

        if (!actor.is_alive())
        {
                return;
        }

        // Slowly decrease current HP/spirit if above max
        const int decr_above_max_n_turns = 7;

        const bool decr_this_turn =
                ((game_time::turn_nr() % decr_above_max_n_turns) == 0);

        const bool is_hp_above_max = (actor.m_hp > actor::max_hp(actor));

        if (is_hp_above_max && decr_this_turn)
        {
                --actor.m_hp;
        }

        const bool is_sp_above_max = (actor.m_sp > actor::max_sp(actor));

        const bool is_exorcist = player_bon::is_bg(Bg::exorcist);

        if (!is_exorcist && is_sp_above_max && decr_this_turn)
        {
                --actor.m_sp;
        }

        // Regenerate spirit
        int regen_sp_n_turns = 18;

        if (actor.is_player())
        {
                if (player_bon::has_trait(Trait::stout_spirit))
                {
                        regen_sp_n_turns -= 4;
                }

                if (player_bon::has_trait(Trait::strong_spirit))
                {
                        regen_sp_n_turns -= 4;
                }

                if (player_bon::has_trait(Trait::mighty_spirit))
                {
                        regen_sp_n_turns -= 4;
                }
        }
        else
        {
                // Is monster

                // Monsters regen spirit very quickly, so spell casters
                // doesn't suddenly get completely handicapped
                regen_sp_n_turns = 1;
        }

        const bool regen_sp_this_turn =
                ((game_time::turn_nr() % regen_sp_n_turns) == 0);

        if (regen_sp_this_turn)
        {
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

        if (actor.is_player())
        {
                player_std_turn();
        }
        else
        {
                auto& mon = static_cast<Mon&>(actor);
                mon_std_turn(mon);
        }
}

}  // namespace actor

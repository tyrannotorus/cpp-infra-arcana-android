// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "actor_std_turn.hpp"

#include <algorithm>
#include <cstddef>
#include <vector>

#include "ability_values.hpp"
#include "actor.hpp"
#include "actor_data.hpp"
#include "actor_hit.hpp"
#include "actor_mon.hpp"
#include "actor_player.hpp"
#include "ai.hpp"
#include "array2.hpp"
#include "debug.hpp"
#include "game_time.hpp"
#include "global.hpp"
#include "inventory.hpp"
#include "item.hpp"
#include "item_data.hpp"
#include "map.hpp"
#include "misc.hpp"
#include "msg_log.hpp"
#include "player_bon.hpp"
#include "property.hpp"
#include "property_data.hpp"
#include "property_factory.hpp"
#include "property_handler.hpp"
#include "random.hpp"
#include "smell.hpp"
#include "terrain.hpp"
#include "terrain_data.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static int calc_player_turns_per_hp_regen_rate()
{
        auto& player = *map::g_player;

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
                auto* const wound =
                        static_cast<PropWound*>(
                                player.m_properties.prop(PropId::wound));

                nr_wounds = wound->nr_wounds();
        }

        int wound_turns_penalty = nr_wounds * 4;

        if (player_bon::has_trait(Trait::survivalist))
        {
                wound_turns_penalty /= 2;
        }

        nr_turns_per_hp += wound_turns_penalty;

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

        return nr_turns_per_hp;
}

static void player_regen_hp()
{
        auto& player = *map::g_player;

        if ((player.m_hp >= actor::max_hp(player)) ||
            (game_time::turn_nr() <= 1) ||
            player.m_properties.has(PropId::poisoned) ||
            player.m_properties.has(PropId::disabled_hp_regen) ||
            (player_bon::bg() == Bg::ghoul))
        {
                return;
        }

        const int nr_turns_per_hp = calc_player_turns_per_hp_regen_rate();
        const int turn = game_time::turn_nr();

        if ((turn % nr_turns_per_hp) != 0)
        {
                return;
        }

        ++player.m_hp;
}

static int calc_player_spot_terrain_tot_skill(
        const P& p,
        const int player_search_skill)
{
        const auto& player = *map::g_player;

        const int lit_mod = map::g_light.at(p) ? 10 : 0;
        const int dist = king_dist(player.m_pos, p);
        const int dist_mod = -((dist - 1) * 5);

        return player_search_skill + lit_mod + dist_mod;
}

static void player_try_spot_hidden_terrain()
{
        auto& player = *map::g_player;

        if (player.m_properties.has(PropId::confused) ||
            !player.m_properties.allow_see())
        {
                return;
        }

        // NOTE: Skill value retrieved here is always at least 1
        const int player_search_skill =
                map::g_player->ability(
                        AbilityId::searching,
                        true);

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

                const auto& p = t->pos();

                bool is_spotted = false;

                if (p.is_adjacent(player.m_pos) &&
                    t->id() == terrain::Id::door)
                {
                        // Player is adjacent to a hidden door - detection is
                        // guaranteed.
                        is_spotted = true;
                }
                else
                {
                        // Not adjacent to hidden door, roll for success.
                        int skill_tot =
                                calc_player_spot_terrain_tot_skill(
                                        p,
                                        player_search_skill);

                        if (skill_tot <= 0)
                        {
                                continue;
                        }

                        const auto result = ability_roll::roll(skill_tot);

                        is_spotted = result >= ActionResult::success;
                }

                if (is_spotted)
                {
                        t->reveal(Verbose::yes);

                        t->on_revealed_from_searching();

                        msg_log::more_prompt();
                }
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

        player_regen_hp();

        player_try_spot_hidden_terrain();
}

static void mon_std_turn(actor::Mon& mon)
{
        smell::put_smell_for_mon(mon);

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

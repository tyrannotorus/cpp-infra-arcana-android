// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "actor_start_turn.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

#include "ability_values.hpp"
#include "actor.hpp"
#include "actor_player_state.hpp"
#include "actor_see.hpp"
#include "actor_sneak.hpp"
#include "array2.hpp"
#include "colors.hpp"
#include "common_text.hpp"
#include "config.hpp"
#include "debug.hpp"
#include "direction.hpp"
#include "flood.hpp"
#include "game_time.hpp"
#include "global.hpp"
#include "hints.hpp"
#include "insanity.hpp"
#include "inventory.hpp"
#include "item.hpp"
#include "item_data.hpp"
#include "map.hpp"
#include "map_parsing.hpp"
#include "misc.hpp"
#include "msg_log.hpp"
#include "player_bon.hpp"
#include "pos.hpp"
#include "property_data.hpp"
#include "property_handler.hpp"
#include "rect.hpp"
#include "smell.hpp"
#include "terrain.hpp"
#include "terrain_data.hpp"
#include "terrain_door.hpp"
#include "text_format.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static bool is_hostile_living_mon(const actor::Actor& actor)
{
        if (actor::is_player(&actor))
        {
                return false;
        }

        if (map::g_player->is_leader_of(&actor))
        {
                return false;
        }

        if (!actor.is_alive())
        {
                return false;
        }

        return true;
}

static Array2<int> calc_player_vigilant_flood()
{
        Array2<int> vigilant_flood(map::dims());

        auto& player = *map::g_player;

        if (player_bon::has_trait(Trait::vigilant))
        {
                const int d = 3;

                const R area(
                        P(std::max(0, player.m_pos.x - d),
                          std::max(0, player.m_pos.y - d)),
                        P(std::min(map::w() - 1, player.m_pos.x + d),
                          std::min(map::h() - 1, player.m_pos.y + d)));

                Array2<bool> blocks_sound(map::dims());

                map_parsers::BlocksSound()
                        .run(blocks_sound, area, MapParseMode::overwrite);

                vigilant_flood = floodfill(player.m_pos, blocks_sound, d);
        }

        return vigilant_flood;
}

static bool should_burning_terrain_interrupt_player()
{
        const bool should_interrupt =
                map::g_player->is_busy() &&
                actor::is_player_seeing_burning_terrain();

        return should_interrupt;
}

static void interrupt_player_burning_terrain()
{
        msg_log::add(
                common_text::g_fire_prevent_cmd,
                colors::text(),
                MsgInterruptPlayer::yes);
}

static bool is_within_vigilant_dist(
        const P& pos,
        const Array2<int>& vigilant_flood)
{
        // NOTE: We only run the flodofill within a limited area, so ANY cell
        // reached by the flood is considered as within distance
        return vigilant_flood.at(pos) > 0;
}

static bool can_detect_pos_by_vigilant(
        const P& pos,
        const Array2<int>& vigilant_flood)
{
        const bool is_vigilant = player_bon::has_trait(Trait::vigilant);

        const bool dist_ok = is_within_vigilant_dist(pos, vigilant_flood);

        return is_vigilant && dist_ok;
}

// Checks if the Vigilant trait should make the player aware of a monster which
// cannot be seen (either due to invisibility, or being in an unseen position)
static bool should_vigilant_make_aware_of_unseeable_mon(
        const actor::Actor& mon,
        const Array2<int>& vigilant_flood)
{
        if (!can_detect_pos_by_vigilant(mon.m_pos, vigilant_flood))
        {
                return false;
        }

        const bool is_cell_seen = map::g_seen.at(mon.m_pos);

        const bool is_mon_invis =
                mon.m_properties.has(PropId::invis) ||
                mon.m_properties.has(PropId::cloaked);

        const bool can_player_see_invis =
                map::g_player->m_properties.has(PropId::see_invis);

        if (is_mon_invis && !can_player_see_invis)
        {
                // The monster is invisible, and player cannot see invisible
                return true;
        }

        if (!is_cell_seen)
        {
                // The monster is in an unseen cell
                return true;
        }

        return false;
}

static void make_aware_of_unseeable_mon_by_vigilant(actor::Actor& mon)
{
        const bool is_cell_seen = map::g_seen.at(mon.m_pos);

        if (!mon.is_player_aware_of_me())
        {
                if (is_cell_seen)
                {
                        // The cell is seen - the monster must be invisible
                        ASSERT(mon.m_properties.has(PropId::invis));

                        print_aware_invis_mon_msg(mon);
                }
                else
                {
                        // Became aware of a monster in an unseen cell

                        // Abort quick move
                        actor::player_state::g_auto_move_dir = Dir::END;
                }
        }

        mon.make_player_aware_of_me();
}

static void on_player_spot_sneaking_mon(actor::Actor& mon)
{
        mon.make_player_aware_of_me();

        const std::string mon_name = mon.name_a();

        msg_log::add(
                "I spot " + mon_name + "!",
                colors::msg_note(),
                MsgInterruptPlayer::yes,
                MorePromptOnMsg::yes);

        mon.m_mon_aware_state.is_msg_mon_in_view_printed = true;
}

static bool player_try_spot_sneaking_mon(
        const actor::Actor& mon,
        const Array2<int>& vigilant_flood)
{
        ActionResult sneak_result = {};

        if (can_detect_pos_by_vigilant(mon.m_pos, vigilant_flood))
        {
                // Sneaking monster is in a position covered by Vigilant
                sneak_result = ActionResult::fail;
        }
        else
        {
                // Cannot be detected by Vigilant
                const bool is_cell_seen = map::g_seen.at(mon.m_pos);

                if (is_cell_seen)
                {
                        actor::SneakParameters p;
                        p.actor_sneaking = &mon;
                        p.actor_searching = map::g_player;

                        sneak_result = roll_sneak(p);
                }
                else
                {
                        sneak_result = ActionResult::success;
                }
        }

        const bool is_spot_success = (sneak_result <= ActionResult::fail);

        return is_spot_success;
}

static void warn_player_about_mon(const actor::Actor& actor)
{
        // NOTE: To avoid redundant messages, a message is only printed if the
        // message log is empty, otherwise only a "more" prompt is added.

        const bool is_busy = map::g_player->is_busy_queryable_action();

        if (msg_log::is_empty() || is_busy)
        {
                // The message log is empty, or player is busy with an action,
                // print a warning with a message.

                const auto name_a = text_format::first_to_upper(actor.name_a());

                // If the player is busy, there is no need for a "more" prompt,
                // since the player will be queried to abort anyway.
                const auto add_more_prompt =
                        is_busy
                        ? MorePromptOnMsg::no
                        : MorePromptOnMsg::yes;

                msg_log::add(
                        name_a + " is in my view.",
                        colors::text(),
                        MsgInterruptPlayer::yes,
                        add_more_prompt);
        }
        else
        {
                // The message log contains messages, and player is not busy,
                // just run a "more" prompt.
                msg_log::more_prompt();
        }
}

static void update_player_seen_monster(actor::Actor& mon)
{
        if (mon.m_mon_aware_state.is_msg_mon_in_view_printed)
        {
                actor::player_state::g_allow_print_mon_warning = false;
        }

        mon.m_mon_aware_state.is_msg_mon_in_view_printed = true;

        const bool should_warn =
                map::g_player->is_busy() ||
                (config::always_warn_new_mon() &&
                 actor::player_state::g_allow_print_mon_warning);

        if (should_warn)
        {
                actor::player_state::g_seen_mon_to_warn_about = &mon;
        }
        else
        {
                // If we should not warn about this seen monster, it means we
                // should not warn about any seen monster.
                actor::player_state::g_seen_mon_to_warn_about = nullptr;
        }

        mon.m_mon_aware_state.is_player_feeling_msg_allowed = false;
}

static void update_player_unseen_monster(
        actor::Actor& mon,
        const Array2<int>& vigilant_flood)
{
        if (!mon.is_player_aware_of_me())
        {
                mon.m_mon_aware_state.is_msg_mon_in_view_printed = false;
        }

        const bool is_vigilant_detect_unseeable =
                should_vigilant_make_aware_of_unseeable_mon(
                        mon,
                        vigilant_flood);

        if (is_vigilant_detect_unseeable)
        {
                make_aware_of_unseeable_mon_by_vigilant(mon);
        }
        else
        {
                // Monster is seeable (in a seen cell and not invisible), or not
                // detectable due to Vigilant.
                const bool is_spotting_sneaking =
                        mon.is_sneaking() &&
                        player_try_spot_sneaking_mon(mon, vigilant_flood);

                if (is_spotting_sneaking)
                {
                        on_player_spot_sneaking_mon(mon);

                        actor::player_state::g_seen_mon_to_warn_about = nullptr;
                        actor::player_state::g_allow_print_mon_warning = false;
                }
        }
}

static void update_player_monster_detection()
{
        const auto vigilant_flood = calc_player_vigilant_flood();

        for (auto* const actor : game_time::g_actors)
        {
                if (!is_hostile_living_mon(*actor))
                {
                        continue;
                }

                if (can_player_see_actor(*actor))
                {
                        update_player_seen_monster(*actor);
                }
                else
                {
                        update_player_unseen_monster(*actor, vigilant_flood);
                }

                if (actor->is_player_aware_of_me())
                {
                        actor->m_give_hit_chance_penalty_vs_player = false;
                }
        }

        if (actor::player_state::g_seen_mon_to_warn_about)
        {
                warn_player_about_mon(
                        *actor::player_state::g_seen_mon_to_warn_about);
        }
}

static void on_player_shock_over_limit()
{
        auto& player = *map::g_player;

        if (player.m_properties.has(PropId::r_shock))
        {
                // Player is shock resistant, pause the countdown
                return;
        }

        hints::display(hints::Id::high_shock);

        if (actor::player_state::g_nr_turns_until_insanity < 0)
        {
                actor::player_state::g_nr_turns_until_insanity = 3;
        }
        else
        {
                --actor::player_state::g_nr_turns_until_insanity;
        }

        if (actor::player_state::g_nr_turns_until_insanity > 0)
        {
                msg_log::add(
                        "I feel my sanity slipping...",
                        colors::msg_note(),
                        MsgInterruptPlayer::yes,
                        MorePromptOnMsg::yes);
        }
        else
        {
                // Time to go crazy!
                actor::player_state::g_nr_turns_until_insanity = -1;

                player.incr_insanity();
        }
}

static void player_incr_passive_shock()
{
        if (map::g_player->m_properties.allow_act())
        {
                double passive_shock_taken = 0.095;

                if (player_bon::bg() == Bg::rogue)
                {
                        passive_shock_taken *= 0.75;
                }

                map::g_player->incr_shock(passive_shock_taken, ShockSrc::time);
        }
}

static void player_items_start_turn()
{
        auto& inv = map::g_player->m_inv;

        for (auto* const item : inv.m_backpack)
        {
                item->on_actor_turn_in_inv(InvType::backpack);
        }

        for (InvSlot& slot : inv.m_slots)
        {
                if (!slot.item)
                {
                        continue;
                }

                slot.item->on_actor_turn_in_inv(InvType::slots);
        }
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
                    (t->id() == terrain::Id::door))
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
                        t->reveal(terrain::PrintRevealMsg::if_seen);

                        t->on_revealed_from_searching();

                        map::update_vision();

                        msg_log::more_prompt();
                }
        }
}

static void player_detect_stuck_doors()
{
        for (const auto& d : dir_utils::g_dir_list)
        {
                const auto p = map::g_player->m_pos + d;
                auto* const terrain = map::g_terrain.at(p);

                if (terrain->id() != terrain::Id::door)
                {
                        continue;
                }

                auto* const door = static_cast<terrain::Door*>(terrain);

                if (door->is_hidden() ||
                    (door->type() == terrain::DoorType::metal))
                {
                        continue;
                }

                door->reveal_stuck_status();
        }
}

static bool should_print_unload_wpn_hint()
{
        const auto* const item = map::g_items.at(map::g_player->m_pos);

        if (!item)
        {
                return false;
        }

        const auto d = item->data();

        const bool is_ranged_wpn_using_ammo =
                (d.type == ItemType::ranged_wpn) &&
                !d.ranged.has_infinite_ammo &&
                (d.ranged.max_ammo > 0);

        if (is_ranged_wpn_using_ammo)
        {
                const auto* const wpn = static_cast<const item::Wpn*>(item);

                if (wpn->m_ammo_loaded > 0)
                {
                        return true;
                }
        }

        return false;
}

static void player_start_turn()
{
        auto& player = *map::g_player;

        map::update_vision();

        // Set current temporary shock from darkness etc
        player.update_tmp_shock();

        if (should_burning_terrain_interrupt_player())
        {
                interrupt_player_burning_terrain();
        }

        smell::on_player_turn_start();

        // Handle monsters coming into vision, detect sneaking monsters, ...
        update_player_monster_detection();

        player.mon_feeling();

        const auto player_seen_actors = actor::seen_actors(player);

        for (auto* actor : player_seen_actors)
        {
                actor::make_player_aware_mon(*actor);

                actor->m_properties.on_player_see();
        }

        player.add_shock_from_seen_monsters();

        player_incr_passive_shock();

        player_try_spot_hidden_terrain();

        player_detect_stuck_doors();

        player_items_start_turn();

        if (should_print_unload_wpn_hint())
        {
                hints::display(hints::Id::unload_weapons);
        }

        if (player.enc_percent() >= 100)
        {
                hints::display(hints::Id::overburdened);
        }

        if (player.shock_tot() >= 100)
        {
                // NOTE: This may kill the player
                on_player_shock_over_limit();

                if (!player.is_alive())
                {
                        return;
                }
        }
        else
        {
                // Shock < 100%
                actor::player_state::g_nr_turns_until_insanity = -1;
        }

        insanity::on_new_player_turn(player_seen_actors);

        actor::player_state::g_seen_mon_to_warn_about = nullptr;
        actor::player_state::g_allow_print_mon_warning = true;
}

static void mon_start_turn(actor::Actor& mon)
{
        if (mon.is_aware_of_player())
        {
                --mon.m_mon_aware_state.aware_counter;
                --mon.m_mon_aware_state.wary_counter;
        }
}

// -----------------------------------------------------------------------------
// actor
// -----------------------------------------------------------------------------
namespace actor
{
void start_turn(Actor& actor)
{
        actor.m_properties.on_turn_begin();

        if (actor::is_player(&actor))
        {
                player_start_turn();
        }
        else
        {
                mon_start_turn(actor);
        }
}

}  // namespace actor

// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "actor_start_turn.hpp"

#include <algorithm>
#include <string>
#include <vector>

#include "ability_values.hpp"
#include "actor.hpp"
#include "actor_mon.hpp"
#include "actor_player.hpp"
#include "actor_see.hpp"
#include "actor_sneak.hpp"
#include "array2.hpp"
#include "colors.hpp"
#include "common_text.hpp"
#include "config.hpp"
#include "debug.hpp"
#include "direction.hpp"
#include "flood.hpp"
#include "game.hpp"
#include "game_time.hpp"
#include "global.hpp"
#include "hints.hpp"
#include "insanity.hpp"
#include "inventory.hpp"
#include "item.hpp"
#include "item_data.hpp"
#include "map.hpp"
#include "map_parsing.hpp"
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
        if (actor.is_player())
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
                Array2<bool> blocks_sound(map::dims());

                const int d = 3;

                const R area(
                        P(std::max(0, player.m_pos.x - d),
                          std::max(0, player.m_pos.y - d)),
                        P(std::min(map::w() - 1, player.m_pos.x + d),
                          std::min(map::h() - 1, player.m_pos.y + d)));

                map_parsers::BlocksSound()
                        .run(blocks_sound,
                             area,
                             MapParseMode::overwrite);

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
        const actor::Mon& mon,
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

static void make_aware_of_unseeable_mon_by_vigilant(actor::Mon& mon)
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
                        map::g_player->m_auto_move_dir = Dir::END;
                }
        }

        mon.set_player_aware_of_me();
}

static void on_player_spot_sneaking_mon(actor::Mon& mon)
{
        mon.set_player_aware_of_me();

        const std::string mon_name = mon.name_a();

        msg_log::add(
                "I spot " + mon_name + "!",
                colors::msg_note(),
                MsgInterruptPlayer::yes,
                MorePromptOnMsg::yes);

        mon.m_mon_aware_state.is_msg_mon_in_view_printed = true;
}

static bool player_try_spot_sneaking_mon(
        const actor::Mon& mon,
        const Array2<int>& vigilant_flood)
{
        ActionResult sneak_result;

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
        const auto name_a = text_format::first_to_upper(actor.name_a());

        msg_log::add(
                name_a + " is in my view.",
                colors::text(),
                MsgInterruptPlayer::yes,
                MorePromptOnMsg::yes);
}

static void player_discover_monsters()
{
        const auto vigilant_flood = calc_player_vigilant_flood();

        actor::Actor* seen_mon_to_warn_about = nullptr;
        bool is_any_mon_already_seen = false;

        for (auto* const actor : game_time::g_actors)
        {
                if (!is_hostile_living_mon(*actor))
                {
                        continue;
                }

                auto& mon = static_cast<actor::Mon&>(*actor);

                if (can_player_see_actor(*actor))
                {
                        if (mon.m_mon_aware_state
                                    .is_msg_mon_in_view_printed)
                        {
                                is_any_mon_already_seen = true;
                        }

                        mon.m_mon_aware_state
                                .is_msg_mon_in_view_printed = true;

                        const bool should_warn =
                                map::g_player->is_busy() ||
                                (config::always_warn_new_mon() &&
                                 !is_any_mon_already_seen);

                        if (should_warn)
                        {
                                seen_mon_to_warn_about = &mon;
                        }
                        else
                        {
                                // If we should not warn about this seen
                                // monster, it means we should not warn about
                                // any seen monster
                                seen_mon_to_warn_about = nullptr;
                        }

                        mon.m_mon_aware_state
                                .is_player_feeling_msg_allowed = false;
                }
                else
                {
                        if (!mon.is_player_aware_of_me())
                        {
                                mon.m_mon_aware_state
                                        .is_msg_mon_in_view_printed = false;
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
                                // Monster is seeable (in a seen cell and not
                                // invisible), or not detectable due to Vigilant
                                const bool is_spotting_sneaking =
                                        mon.is_sneaking() &&
                                        player_try_spot_sneaking_mon(
                                                mon,
                                                vigilant_flood);

                                if (is_spotting_sneaking)
                                {
                                        on_player_spot_sneaking_mon(mon);

                                        seen_mon_to_warn_about = nullptr;
                                        is_any_mon_already_seen = true;
                                }
                        }
                }
        }

        if (seen_mon_to_warn_about)
        {
                warn_player_about_mon(*seen_mon_to_warn_about);
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

        player.m_nr_turns_until_ins =
                (player.m_nr_turns_until_ins < 0)
                ? 3
                : (player.m_nr_turns_until_ins - 1);

        if (player.m_nr_turns_until_ins > 0)
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
                player.m_nr_turns_until_ins = -1;

                player.incr_insanity();

                if (player.is_alive())
                {
                        game_time::tick();
                }
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

        player.update_fov();

        player.update_mon_awareness();

        // Set current temporary shock from darkness etc
        map::g_player->update_tmp_shock();

        if (should_burning_terrain_interrupt_player())
        {
                interrupt_player_burning_terrain();
        }

        smell::on_player_turn_start();

        // Handle monsters coming into vision, detect sneaking monsters, ...
        player_discover_monsters();

        player.mon_feeling();

        const auto my_seen_foes = seen_foes(player);

        for (auto* actor : my_seen_foes)
        {
                static_cast<actor::Mon*>(actor)->set_player_aware_of_me();

                game::player_discover_monster(*actor);

                actor->m_properties.on_player_see();
        }

        player.add_shock_from_seen_monsters();

        player_incr_passive_shock();

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
                player.m_nr_turns_until_ins = -1;
        }

        insanity::on_new_player_turn(my_seen_foes);
}

static void mon_start_turn(actor::Mon& mon)
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

        if (actor.is_player())
        {
                player_start_turn();
        }
        else
        {
                auto& mon = static_cast<Mon&>(actor);
                mon_start_turn(mon);
        }
}

}  // namespace actor

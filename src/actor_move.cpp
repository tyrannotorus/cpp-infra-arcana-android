// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "actor_move.hpp"

#include <ostream>
#include <string>
#include <vector>

#include "actor.hpp"
#include "actor_data.hpp"
#include "actor_mon.hpp"
#include "actor_player.hpp"
#include "actor_see.hpp"
#include "array2.hpp"
#include "attack.hpp"
#include "colors.hpp"
#include "common_text.hpp"
#include "config.hpp"
#include "debug.hpp"
#include "game_time.hpp"
#include "global.hpp"
#include "inventory.hpp"
#include "item.hpp"
#include "item_data.hpp"
#include "map.hpp"
#include "map_parsing.hpp"
#include "msg_log.hpp"
#include "player_bon.hpp"
#include "pos.hpp"
#include "property.hpp"
#include "property_data.hpp"
#include "property_handler.hpp"
#include "query.hpp"
#include "reload.hpp"
#include "terrain.hpp"
#include "terrain_data.hpp"
#include "terrain_door.hpp"
#include "text_format.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static BinaryAnswer query_player_attack_mon_with_ranged_wpn(
        const item::Wpn& wpn,
        const actor::Mon& mon)
{
        const std::string wpn_name = wpn.name(ItemRefType::a);

        const bool can_see_mon = can_player_see_actor(mon);

        const std::string mon_name =
                can_see_mon
                ? mon.name_the()
                : "it";

        const std::string msg =
                "Attack " +
                mon_name +
                " with " +
                wpn_name +
                "? " +
                common_text::g_yes_or_no_hint;

        msg_log::add(
                msg,
                colors::light_white(),
                MsgInterruptPlayer::no,
                MorePromptOnMsg::no,
                CopyToMsgHistory::no);

        const auto answer = query::yes_or_no();

        return answer;
}

static void player_bump_known_hostile_mon(actor::Mon& mon)
{
        auto& player = *map::g_player;

        if (!player.m_properties.allow_attack_melee(Verbose::yes))
        {
                return;
        }

        auto* const wpn_item = player.m_inv.item_in_slot(SlotId::wpn);

        if (!wpn_item)
        {
                player.hand_att(mon);

                return;
        }

        auto& wpn = static_cast<item::Wpn&>(*wpn_item);

        // If this is also a ranged weapon, ask if player really
        // intended to use it as melee weapon
        if (wpn.data().ranged.is_ranged_wpn &&
            config::warn_on_ranged_wpn_melee())
        {
                const auto answer =
                        query_player_attack_mon_with_ranged_wpn(wpn, mon);

                msg_log::clear();

                if (answer == BinaryAnswer::no)
                {
                        return;
                }
        }

        player.m_tgt = &mon;

        attack::melee(&player, player.m_pos, mon, wpn);
}

static void player_bump_unkown_hostile_mon(actor::Mon& mon)
{
        mon.set_player_aware_of_me();

        actor::print_aware_invis_mon_msg(mon);
}

static void player_bump_allied_mon(actor::Mon& mon)
{
        if (mon.is_player_aware_of_me())
        {
                std::string mon_name =
                        can_player_see_actor(mon)
                        ? mon.name_a()
                        : "it";

                msg_log::add("I displace " + mon_name + ".");
        }

        mon.m_pos = map::g_player->m_pos;
}

static void player_walk_on_item(item::Item* const item)
{
        if (!item)
        {
                return;
        }

        // Only print the item name if the item will not be "found" by stepping
        // on it, otherwise there would be redundant messages, e.g. "A Muddy
        // Potion." --> "I have found a Muddy Potion!"
        if ((item->data().xp_on_found <= 0) || item->data().is_found)
        {
                std::string item_name =
                        item->name(
                                ItemRefType::plural,
                                ItemRefInf::yes,
                                ItemRefAttInf::wpn_main_att_mode);

                item_name = text_format::first_to_upper(item_name);

                msg_log::add(item_name + ".");
        }

        item->on_player_found();
}

static void print_corpses_at_player_msgs()
{
        for (auto* const actor : game_time::g_actors)
        {
                if ((actor->m_pos == map::g_player->m_pos) &&
                    (actor->m_state == ActorState::corpse))
                {
                        const std::string name =
                                text_format::first_to_upper(
                                        actor->m_data->corpse_name_a);

                        msg_log::add(name + ".");
                }
        }
}

static bool is_player_staggering_from_wounds()
{
        Prop* const wound_prop =
                map::g_player->m_properties.prop(PropId::wound);

        int nr_wounds = 0;

        if (wound_prop)
        {
                nr_wounds = static_cast<PropWound*>(wound_prop)->nr_wounds();
        }

        const int min_nr_wounds_for_stagger = 3;

        return nr_wounds >= min_nr_wounds_for_stagger;
}

static AllowAction pre_bump_terrains(
        actor::Actor& actor,
        const P& target)
{
        const auto mobs = game_time::mobs_at_pos(target);

        for (auto* mob : mobs)
        {
                const auto result = mob->pre_bump(actor);

                if (result == AllowAction::no)
                {
                        return result;
                }
        }

        const auto result = map::g_terrain.at(target)->pre_bump(actor);

        return result;
}

static void print_ooze_enter_terrain_msg(
        const actor::Actor& actor,
        const terrain::Terrain& terrain)
{
        const auto mon_name = text_format::first_to_upper(actor.name_the());
        const auto ter_name = terrain.name(Article::the);

        std::string preposition = "through";

        if (terrain.id() == terrain::Id::door)
        {
                const auto& door =
                        static_cast<const terrain::Door&>(terrain);

                switch (door.type())
                {
                case terrain::DoorType::gate:
                        break;

                case terrain::DoorType::wood:
                case terrain::DoorType::metal:
                        preposition = "under";
                        break;
                }
        }

        msg_log::add(mon_name + " seeps " + preposition + " " + ter_name + ".");
}

static void print_small_crawling_enter_terrain_msg(
        const actor::Actor& actor,
        const terrain::Terrain& terrain)
{
        const auto mon_name = text_format::first_to_upper(actor.name_the());
        const auto ter_name = terrain.name(Article::the);

        msg_log::add(mon_name + " squirms through " + ter_name + ".");
}

static void print_mon_enter_non_walkable_terrain_msg(
        const actor::Actor& actor,
        const terrain::Terrain& terrain)
{
        if (actor.m_properties.has(PropId::ooze))
        {
                print_ooze_enter_terrain_msg(actor, terrain);
        }
        else if (actor.m_properties.has(PropId::small_crawling))
        {
                print_small_crawling_enter_terrain_msg(actor, terrain);
        }
}

static void bump_terrains(actor::Actor& actor, const P& target)
{
        const auto mobs = game_time::mobs_at_pos(target);

        for (auto* mob : mobs)
        {
                mob->bump(actor);
        }

        auto* const terrain = map::g_terrain.at(target);

        if (!actor.is_player() &&
            !terrain->is_walkable() &&
            (terrain->data().matl_type != Matl::fluid) &&
            can_player_see_actor(actor))
        {
                print_mon_enter_non_walkable_terrain_msg(actor, *terrain);
        }

        terrain->bump(actor);
}

static void on_player_waiting()
{
        auto did_action = DidAction::no;

        // Ghoul feed on corpses?
        if (player_bon::bg() == Bg::ghoul)
        {
                did_action = map::g_player->try_eat_corpse();
        }

        if (did_action == DidAction::no)
        {
                // Reorganize pistol magazines?
                const auto seen_foes = actor::seen_foes(*map::g_player);

                if (seen_foes.empty())
                {
                        reload::player_arrange_pistol_mags();
                }
        }
}

static bool should_player_be_immobile()
{
        return map::g_player->enc_percent() >= g_enc_immobile_lvl;
}

static bool should_player_stagger(const P& target)
{
        const int enc = map::g_player->enc_percent();

        const auto terrain_id = map::g_terrain.at(target)->id();

        if (terrain_id == terrain::Id::liquid_shallow ||
            terrain_id == terrain::Id::liquid_deep)
        {
                return false;
        }
        else
        {
                return enc >= 100 || is_player_staggering_from_wounds();
        }
}

static void move_player_non_center_direction(const P& target)
{
        auto& player = *map::g_player;

        const bool is_terrain_blocking_move =
                map_parsers::BlocksActor(player, ParseActors::no)
                        .run(target);

        auto* const mon =
                static_cast<actor::Mon*>(
                        map::first_actor_at_pos(target));

        const auto is_aware_of_mon = (mon && mon->is_player_aware_of_me());

        if (mon && !player.is_leader_of(mon) && is_aware_of_mon)
        {
                player_bump_known_hostile_mon(*mon);

                return;
        }

        const auto pre_move_result = pre_bump_terrains(player, target);

        if (pre_move_result == AllowAction::no)
        {
                return;
        }

        if (mon &&
            !player.is_leader_of(mon) &&
            !is_terrain_blocking_move &&
            !is_aware_of_mon)
        {
                player_bump_unkown_hostile_mon(*mon);

                return;
        }

        if (!is_terrain_blocking_move)
        {
                if (should_player_be_immobile())
                {
                        // TODO: Currently you can attempt to attack hidden
                        // adjacent monsters "for free" while you are too
                        // encumbered to move (very minor issue, but it's weird)
                        msg_log::add("I am too encumbered to move!");

                        return;
                }
                else if (should_player_stagger(target))
                {
                        msg_log::add("I stagger.", colors::msg_note());

                        player.m_properties.apply(new PropWaiting());
                }

                if (mon && player.is_leader_of(mon))
                {
                        player_bump_allied_mon(*mon);
                }

                map::g_terrain.at(player.m_pos)->on_leave(player);

                player.m_pos = target;

                player_walk_on_item(map::g_items.at(player.m_pos));

                print_corpses_at_player_msgs();

                // Moving ends sanctuary
                player.m_properties.end_prop(PropId::sanctuary);
        }

        bump_terrains(player, target);
}

static void move_player(Dir dir)
{
        auto& player = *map::g_player;

        if (!player.is_alive())
        {
                return;
        }

        const auto intended_dir = dir;

        player.m_properties.affect_move_dir(player.m_pos, dir);

        const auto target = player.m_pos + dir_utils::offset(dir);

        if (intended_dir == Dir::center)
        {
                on_player_waiting();
        }
        else if (dir != Dir::center)
        {
                move_player_non_center_direction(target);
        }

        if (player.m_pos == target)
        {
                // We are at the target position, this means that either:
                // * the player moved to a different position, or
                // * the player waited in the current position on purpose, or
                // * the player was stuck (e.g. in a spider web)

                game_time::tick();
        }
}

static void move_mon(actor::Mon& mon, Dir dir)
{
#ifndef NDEBUG
        if (dir == Dir::END)
        {
                TRACE << "Illegal direction parameter" << std::endl;
                ASSERT(false);
        }

        if (!map::is_pos_inside_outer_walls(mon.m_pos))
        {
                TRACE << "Monster outside map" << std::endl;
                ASSERT(false);
        }
#endif  // NDEBUG

        mon.m_properties.affect_move_dir(mon.m_pos, dir);

        // Movement direction is stored for AI purposes
        mon.m_ai_state.last_dir_moved = dir;

        const auto target_p = mon.m_pos + dir_utils::offset(dir);

#ifndef NDEBUG
        if (target_p != mon.m_pos)
        {
                const bool is_blocked =
                        map_parsers::BlocksActor(mon, ParseActors::yes)
                                .run(target_p);

                ASSERT(!is_blocked);
        }
#endif  // NDEBUG

        if ((dir != Dir::center) && map::is_pos_inside_outer_walls(target_p))
        {
                // Leave current cell (e.g. stop swimming)
                map::g_terrain.at(mon.m_pos)->on_leave(mon);

                mon.m_pos = target_p;

                bump_terrains(mon, mon.m_pos);
        }

        game_time::tick();
}

// -----------------------------------------------------------------------------
// actor
// -----------------------------------------------------------------------------
namespace actor
{
void move(Actor& actor, const Dir dir)
{
        if (actor.is_player())
        {
                move_player(dir);
        }
        else
        {
                move_mon(static_cast<Mon&>(actor), dir);
        }
}

}  // namespace actor

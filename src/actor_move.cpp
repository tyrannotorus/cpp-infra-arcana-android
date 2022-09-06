// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "actor_move.hpp"

#include <ostream>
#include <string>
#include <vector>

#include "actor.hpp"
#include "actor_data.hpp"
#include "actor_eat.hpp"
#include "actor_player_state.hpp"
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
#include "msg_log.hpp"
#include "player_bon.hpp"
#include "pos.hpp"
#include "property.hpp"
#include "property_data.hpp"
#include "property_factory.hpp"
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
        const actor::Actor& mon)
{
        const std::string wpn_name = wpn.name(ItemNameType::a);

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

static void player_bump_known_hostile_mon(actor::Actor& mon)
{
        auto& player = *map::g_player;

        if (!player.m_properties.allow_attack_melee(Verbose::yes)) {
                return;
        }

        auto* const wpn_item = player.m_inv.item_in_slot(SlotId::wpn);

        if (!wpn_item) {
                player.hand_att(mon);

                return;
        }

        auto& wpn = static_cast<item::Wpn&>(*wpn_item);

        // If this is also a ranged weapon, ask if player really
        // intended to use it as melee weapon
        if (wpn.data().ranged.is_ranged_wpn &&
            config::warn_on_ranged_wpn_melee()) {
                const auto answer =
                        query_player_attack_mon_with_ranged_wpn(
                                wpn,
                                mon);

                msg_log::clear();

                if (answer == BinaryAnswer::no) {
                        return;
                }
        }

        actor::player_state::g_target = &mon;

        attack::melee(&player, player.m_pos, mon, wpn);
}

static void player_bump_unkown_hostile_mon(actor::Actor& mon)
{
        actor::print_aware_invis_mon_msg(mon);

        mon.make_player_aware_of_me();

        map::update_vision();
}

static void player_displace_allied_mon(actor::Actor& mon, const P& new_mon_pos)
{
        if (mon.is_player_aware_of_me()) {
                std::string mon_name =
                        can_player_see_actor(mon)
                        ? mon.name_a()
                        : "it";

                msg_log::add("I displace " + mon_name + ".");
        }

        actor::set_position(mon, new_mon_pos);
}

static void player_walk_on_item(item::Item* const item)
{
        if (!item) {
                return;
        }

        // Only print the item name if the item will not be "found" by stepping
        // on it, otherwise there would be redundant messages, e.g. "A Muddy
        // Potion." --> "I have found a Muddy Potion!"
        if ((item->data().xp_on_found <= 0) || item->data().is_found) {
                std::string item_name =
                        item->name(
                                ItemNameType::plural,
                                ItemNameInfo::yes,
                                ItemNameAttackInfo::main_attack_mode);

                item_name = text_format::first_to_upper(item_name);

                msg_log::add(item_name + ".");
        }

        item->on_player_found();
}

static void print_corpses_at_player_msgs()
{
        for (auto* const actor : game_time::g_actors) {
                if (actor->m_pos != map::g_player->m_pos) {
                        continue;
                }

                if (actor->m_state != ActorState::corpse) {
                        continue;
                }

                const std::string name =
                        text_format::first_to_upper(
                                actor->m_data->corpse_name_a);

                msg_log::add(name + ".");
        }
}

static bool is_player_staggering_from_wounds()
{
        Prop* const wound_prop =
                map::g_player->m_properties.prop(PropId::wound);

        int nr_wounds = 0;

        if (wound_prop) {
                nr_wounds = static_cast<PropWound*>(wound_prop)->nr_wounds();
        }

        const int min_nr_wounds_for_stagger = 3;

        return nr_wounds >= min_nr_wounds_for_stagger;
}

static AllowAction pre_bump_terrains(
        actor::Actor& actor,
        const P& target)
{
        const auto mobs = game_time::mobs_at(target);

        for (auto* mob : mobs) {
                const auto result = mob->pre_bump(actor);

                if (result == AllowAction::no) {
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
        const auto mon_name =
                text_format::first_to_upper(actor.name_the());

        const auto ter_name =
                terrain.name(Article::the);

        std::string preposition = "through";

        if (terrain.id() == terrain::Id::door) {
                const auto& door =
                        static_cast<const terrain::Door&>(terrain);

                switch (door.type()) {
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

static void print_small_creature_enter_terrain_msg(
        const actor::Actor& actor,
        const terrain::Terrain& terrain)
{
        const auto mon_name =
                text_format::first_to_upper(actor.name_the());

        const auto ter_name =
                terrain.name(Article::the);

        msg_log::add(mon_name + " squirms through " + ter_name + ".");
}

static void print_mon_enter_non_walkable_terrain_msg(
        const actor::Actor& actor,
        const terrain::Terrain& terrain)
{
        const auto& props = actor.m_properties;

        const bool is_ooze = props.has(PropId::ooze);

        const bool is_small =
                props.has(PropId::small_crawling) ||
                props.has(PropId::tiny_flying);

        if (is_ooze) {
                print_ooze_enter_terrain_msg(actor, terrain);
        }
        else if (is_small) {
                print_small_creature_enter_terrain_msg(actor, terrain);
        }
}

static void bump_terrains(actor::Actor& actor, const P& target)
{
        const auto mobs = game_time::mobs_at(target);

        for (auto* mob : mobs) {
                mob->bump(actor);
        }

        auto* const terrain = map::g_terrain.at(target);

        if (!actor::is_player(&actor) &&
            !terrain->is_walkable() &&
            (terrain->m_data->matl_type != Matl::fluid) &&
            can_player_see_actor(actor)) {
                print_mon_enter_non_walkable_terrain_msg(actor, *terrain);
        }

        terrain->bump(actor);
}

static void on_player_waiting()
{
        auto did_action = DidAction::no;

        // Ghoul feed on corpses?
        if (player_bon::bg() == Bg::ghoul) {
                actor::try_eat_corpse(*map::g_player);
        }

        if (did_action == DidAction::no) {
                // Reorganize pistol magazines?
                const auto seen_foes = actor::seen_foes(*map::g_player);

                const bool is_burning =
                        map::g_player->m_properties.has(PropId::burning);

                if (seen_foes.empty() && !is_burning) {
                        reload::player_arrange_pistol_mags();
                }
        }
}

static bool should_player_be_immobile()
{
        return map::g_player->enc_percent() >= g_enc_immobile_lvl;
}

static bool is_player_stagger()
{
        const int enc = map::g_player->enc_percent();

        return enc >= 100 || is_player_staggering_from_wounds();
}

static bool is_player_torture_collared()
{
        return (
                map::g_player->m_inv.has_item_in_slot(
                        SlotId::head,
                        item::Id::torture_collar));
}

static void handle_player_slowed_movement(const P& target)
{
        if (map::g_player->m_properties.has(PropId::crimson_passage)) {
                return;
        }

        const terrain::Id terrain_id = map::g_terrain.at(target)->id();

        if (terrain_id == terrain::Id::liquid) {
                return;
        }

        bool should_wait = false;

        if (is_player_torture_collared()) {
                should_wait = true;
        }
        else if (is_player_stagger()) {
                msg_log::add("I stagger.", colors::msg_note());

                should_wait = true;
        }

        if (should_wait) {
                map::g_player->m_properties.apply(
                        property_factory::make(
                                PropId::waiting));
        }
}

static void move_player_non_center_direction(const P& target)
{
        actor::Actor& player = *map::g_player;

        const bool is_terrain_blocking_move =
                !map::can_actor_move_into_terrain_at(player, target);

        actor::Actor* const mon = map::living_actor_at(target);

        const bool is_aware_of_mon = (mon && mon->is_player_aware_of_me());

        if (mon && !player.is_leader_of(mon) && is_aware_of_mon) {
                player_bump_known_hostile_mon(*mon);

                return;
        }

        const AllowAction pre_move_result = pre_bump_terrains(player, target);

        if (pre_move_result == AllowAction::no) {
                return;
        }

        if (mon &&
            !player.is_leader_of(mon) &&
            !is_terrain_blocking_move &&
            !is_aware_of_mon) {
                player_bump_unkown_hostile_mon(*mon);

                return;
        }

        if (!is_terrain_blocking_move) {
                if (should_player_be_immobile()) {
                        // TODO: Currently you can attempt to attack hidden
                        // adjacent monsters "for free" while you are too
                        // encumbered to move (very minor issue, but it's weird)
                        msg_log::add("I am too encumbered to move!");

                        return;
                }

                handle_player_slowed_movement(target);

                if (mon && player.is_leader_of(mon)) {
                        player_displace_allied_mon(*mon, map::g_player->m_pos);
                }

                map::g_terrain.at(player.m_pos)->on_leave(player);

                actor::set_position(player, target);

                player_walk_on_item(map::g_items.at(player.m_pos));

                print_corpses_at_player_msgs();

                player.m_properties.on_moved_non_center_dir();
        }

        bump_terrains(player, target);
}

static void do_move_action_player(Dir dir)
{
        auto& player = *map::g_player;

        if (!player.is_alive()) {
                return;
        }

        if (!player.m_properties.allow_move_dir(dir)) {
                return;
        }

        const auto intended_dir = dir;

        player.m_properties.affect_move_dir(dir);

        const auto target = player.m_pos + dir_utils::offset(dir);

        if (intended_dir == Dir::center) {
                on_player_waiting();
        }
        else if (dir != Dir::center) {
                const int dlvl_before = map::g_dlvl;

                // NOTE: The player might bump the stairs here and go to a new
                // dungeon level:
                move_player_non_center_direction(target);

                if (map::g_dlvl != dlvl_before) {
                        return;
                }

                map::update_vision();
                actor::make_player_aware_seen_monsters();
        }

        if (player.m_pos == target) {
                // We are at the target position, this means that either:
                // * the player moved to a different position, or
                // * the player waited in the current position on purpose, or
                // * the player was stuck (e.g. in a spider web)

                const bool is_free_move =
                        (player.m_properties.has(PropId::crimson_passage) &&
                         (dir != Dir::center) &&
                         (dir != Dir::END));

                if (!is_free_move) {
                        game_time::tick();
                }
        }
}

#ifndef NDEBUG
static void sanity_check_mon_direction(const actor::Actor& mon, const Dir dir)
{
        if (dir != Dir::END) {
                return;
        }

        TRACE
                << "Illegal direction parameter "
                << "'" << (int)dir << "' "
                << "given for monster '" << mon.name_a() + "'"
                << std::endl;
        PANIC;
}
#endif  // NDEBUG

#ifndef NDEBUG
static void sanity_check_mon_not_outside_map(const actor::Actor& mon)
{
        if (map::is_pos_inside_outer_walls(mon.m_pos)) {
                return;
        }

        TRACE
                << "Monster '" << mon.name_a() << "' "
                << "outside map, at "
                << mon.m_pos.x << "," << mon.m_pos.y
                << std::endl;
        PANIC;
}
#endif  // NDEBUG

#ifndef NDEBUG
static void sanity_check_mon_can_move_into_terrain(
        const actor::Actor& mon,
        const P& target_pos)
{
        if (map::can_actor_move_into_terrain_at(mon, target_pos)) {
                return;
        }

        const std::string mon_name = mon.name_a();

        const std::string terrain_name =
                map::g_terrain.at(target_pos)->name(Article::a);

        TRACE
                << "Monster '" << mon_name << "' "
                << "tried to move into terrain it cannot move into: "
                << '"' << terrain_name << "'"
                << std::endl;

        TRACE
                << ("The following mobile terrains also exists at "
                    "target position:")
                << std::endl;

        for (terrain::Terrain* mob : game_time::g_mobs) {
                if (mob->pos() == target_pos) {
                        TRACE
                                << mob->name(Article::a)
                                << std::endl;
                }
        }

        PANIC;
}
#endif  // NDEBUG

#ifndef NDEBUG
static void sanity_check_no_living_actor_at_target_pos(
        const actor::Actor& mon,
        const P& target_pos)
{
        const actor::Actor* const mon_2 = map::living_actor_at(target_pos);

        if (!mon_2) {
                return;
        }

        const std::string mon_name_1 = mon.name_a();
        const std::string mon_name_2 = mon_2->name_a();

        TRACE
                << "Monster '" << mon_name_1 << "' "
                << "tried to move into a position with monster "
                << '"' << mon_name_2 << "'"
                << std::endl;

        PANIC;
}
#endif  // NDEBUG

static void do_move_action_mon(actor::Actor& mon, Dir dir)
{
#ifndef NDEBUG
        sanity_check_mon_direction(mon, dir);
        sanity_check_mon_not_outside_map(mon);
#endif  // NDEBUG

        mon.m_properties.affect_move_dir(dir);

        // Movement direction is stored for AI purposes
        mon.m_ai_state.last_dir_moved = dir;

        const auto target_p = mon.m_pos + dir_utils::offset(dir);

#ifndef NDEBUG
        if (target_p != mon.m_pos) {
                sanity_check_mon_can_move_into_terrain(mon, target_p);
                sanity_check_no_living_actor_at_target_pos(mon, target_p);
        }
#endif  // NDEBUG

        if ((dir != Dir::center) && map::is_pos_inside_outer_walls(target_p)) {
                // Leave current cell
                map::g_terrain.at(mon.m_pos)->on_leave(mon);

                actor::set_position(mon, target_p);

                bump_terrains(mon, mon.m_pos);

                mon.m_properties.on_moved_non_center_dir();

                if (actor::can_player_see_actor(mon)) {
                        actor::make_player_aware_mon(mon);
                }
        }

        game_time::tick();
}

// -----------------------------------------------------------------------------
// actor
// -----------------------------------------------------------------------------
namespace actor
{
void do_move_action(Actor& actor, const Dir dir)
{
        if (actor::is_player(&actor)) {
                do_move_action_player(dir);
        }
        else {
                do_move_action_mon(actor, dir);
        }
}

void set_position(Actor& actor, const P& pos)
{
        actor.m_pos = pos;

        // Update light map if necessary.

        // NOTE: For the player, we take the lazy approach and always update the
        // light map (in case the lantern is active for example). It couuld
        // possibly be done more selectively, but it would probably not be worth
        // the trouble.
        const auto& props = actor.m_properties;

        if (is_player(&actor) ||
            props.has(PropId::radiant_self) ||
            props.has(PropId::radiant_adjacent) ||
            props.has(PropId::radiant_fov)) {
                map::update_light_map();
        }
}

}  // namespace actor

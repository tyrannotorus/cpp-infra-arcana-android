// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "actor_act.hpp"

#include <stddef.h>
#include <algorithm>
#include <iterator>
#include <ostream>
#include <string>
#include <vector>

#include "actor.hpp"
#include "actor_mon.hpp"
#include "actor_move.hpp"
#include "actor_player.hpp"
#include "actor_see.hpp"
#include "ai.hpp"
#include "bot.hpp"
#include "config.hpp"
#include "drop.hpp"
#include "game_commands.hpp"
#include "game_time.hpp"
#include "io.hpp"
#include "map.hpp"
#include "terrain.hpp"
#include "terrain_door.hpp"
#include "terrain_trap.hpp"
#include "actor_data.hpp"
#include "array2.hpp"
#include "debug.hpp"
#include "direction.hpp"
#include "global.hpp"
#include "inventory.hpp"
#include "item.hpp"
#include "item_data.hpp"
#include "pos.hpp"
#include "property_data.hpp"
#include "property_handler.hpp"
#include "random.hpp"
#include "sound.hpp"
#include "terrain_data.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static void remove_player_with_sanctuary(std::vector<actor::Actor*>& actors)
{
        if (!map::g_player->m_properties.has(PropId::sanctuary))
        {
                return;
        }

        for (auto it = std::begin(actors); it != std::end(actors); ++it)
        {
                auto* const actor = *it;

                if (actor->is_player())
                {
                        actors.erase(it);

                        return;
                }
        }
}

static void player_act()
{
        actor::Player& player = *map::g_player;

        if (!player.is_alive())
        {
                return;
        }

        if (player.m_properties.on_act() == DidAction::yes)
        {
                return;
        }

        if (player.m_tgt && (player.m_tgt->m_state != ActorState::alive))
        {
                player.m_tgt = nullptr;
        }

        if (player.m_active_medical_bag)
        {
                player.m_active_medical_bag->continue_action();

                return;
        }

        if (player.m_remove_armor_countdown > 0)
        {
                --player.m_remove_armor_countdown;

                // Done removing the armor now?
                if (player.m_remove_armor_countdown == 0)
                {
                        if (player.m_is_dropping_armor_from_body_slot)
                        {
                                item_drop::drop_item_from_inv(
                                        *map::g_player,
                                        InvType::slots,
                                        (size_t)SlotId::body);

                                player.m_is_dropping_armor_from_body_slot =
                                        false;
                        }
                        else
                        {
                                // Taking off armor
                                player.m_inv.unequip_slot(SlotId::body);
                        }
                }
                else
                {
                        // Not done removing armor yet
                        game_time::tick();
                }

                return;
        }

        if (player.m_equip_armor_countdown > 0)
        {
                --player.m_equip_armor_countdown;

                // Done wearing armor now?
                if (player.m_equip_armor_countdown == 0)
                {
                        if (player.m_item_equipping)
                        {
                                // Putting on armor
                                ASSERT(player.m_item_equipping->data().type ==
                                       ItemType::armor);

                                player.m_inv.equip_backpack_item(
                                        player.m_item_equipping,
                                        SlotId::body);

                                player.m_item_equipping = nullptr;
                        }
                }
                else
                {
                        // Not done wearing armor yet
                        game_time::tick();
                }

                return;
        }

        if (player.m_item_equipping)
        {
                // NOTE: Armor is handled above - no need to consider that here
                SlotId slot_id = SlotId::END;

                switch (player.m_item_equipping->data().type)
                {
                case ItemType::melee_wpn:
                case ItemType::ranged_wpn:
                        slot_id = SlotId::wpn;
                        break;

                case ItemType::head_wear:
                        slot_id = SlotId::head;
                        break;

                default:
                        ASSERT(false);
                        break;
                }

                player.m_inv.equip_backpack_item(
                        player.m_item_equipping,
                        slot_id);

                player.m_item_equipping = nullptr;

                game_time::tick();

                return;
        }

        if (player.m_wait_turns_left > 0)
        {
                --player.m_wait_turns_left;

                move(player, Dir::center);

                return;
        }

        // Auto move
        if (player.m_auto_move_dir != Dir::END)
        {
                const auto target =
                        player.m_pos +
                        dir_utils::offset(player.m_auto_move_dir);

                bool is_target_adj_to_unseen_cell = false;

                for (const P& d : dir_utils::g_dir_list_w_center)
                {
                        if (!map::g_seen.at(target + d))
                        {
                                is_target_adj_to_unseen_cell = true;
                                break;
                        }
                }

                // If this is not the first step of auto moving, stop before
                // blocking terrains, fire, known traps, etc - otherwise allow
                // bumping terrains as with normal movement
                if (player.m_has_taken_auto_move_step)
                {
                        bool should_abort = false;

                        const auto* const target_terrain =
                                map::g_terrain.at(target);

                        if (!target_terrain->can_move(player))
                        {
                                should_abort = true;
                        }
                        else
                        {
                                const auto is_target_seen =
                                        map::g_seen.at(target);

                                const auto target_terrain_id =
                                        target_terrain->id();

                                const bool is_target_known_trap =
                                        is_target_seen &&
                                        (target_terrain_id == terrain::Id::trap) &&
                                        !static_cast<const terrain::Trap*>(target_terrain)->is_hidden();

                                should_abort =
                                        is_target_known_trap ||
                                        (target_terrain_id == terrain::Id::chains) ||
                                        (target_terrain_id == terrain::Id::liquid_shallow) ||
                                        (target_terrain_id == terrain::Id::vines) ||
                                        (target_terrain->is_burning());
                        }

                        if (should_abort)
                        {
                                player.m_auto_move_dir = Dir::END;

                                return;
                        }
                }

                auto adj_known_closed_doors = [](const P& p) {
                        std::vector<const terrain::Door*> doors;

                        for (const P& d : dir_utils::g_dir_list_w_center)
                        {
                                const P p_adj(p + d);

                                const auto* const adj_terrain =
                                        map::g_terrain.at(p_adj);

                                const bool is_adj_seen = map::g_seen.at(p_adj);

                                if (is_adj_seen &&
                                    (adj_terrain->id() == terrain::Id::door))
                                {
                                        const auto* const door =
                                                static_cast<const terrain::Door*>(adj_terrain);

                                        if (!door->is_hidden() && !door->is_open())
                                        {
                                                doors.push_back(door);
                                        }
                                }
                        }

                        return doors;
                };

                const auto adj_known_closed_doors_before =
                        adj_known_closed_doors(player.m_pos);

                move(player, player.m_auto_move_dir);

                player.m_has_taken_auto_move_step = true;

                player.update_fov();

                if (player.m_auto_move_dir == Dir::END)
                {
                        return;
                }

                const auto adj_known_closed_doors_after =
                        adj_known_closed_doors(player.m_pos);

                bool is_new_known_adj_closed_door = false;

                for (const auto* const door_after : adj_known_closed_doors_after)
                {
                        is_new_known_adj_closed_door =
                                std::find(
                                        std::begin(adj_known_closed_doors_before),
                                        end(adj_known_closed_doors_before),
                                        door_after) ==
                                std::end(adj_known_closed_doors_before);

                        if (is_new_known_adj_closed_door)
                        {
                                break;
                        }
                }

                if (is_target_adj_to_unseen_cell ||
                    is_new_known_adj_closed_door)
                {
                        player.m_auto_move_dir = Dir::END;
                }

                return;
        }

        // If this point is reached - read input
        if (config::is_bot_playing())
        {
                bot::act();
        }
        else
        {
                // Not bot playing
                const auto input = io::get();

                const auto game_cmd = game_commands::to_cmd(input);

                game_commands::handle(game_cmd);
        }
}

static void mon_act(actor::Mon& mon)
{
        const bool is_player_leader = mon.is_actor_my_leader(map::g_player);

#ifndef NDEBUG
        // Sanity check - verify that monster is not outside the map
        if (!map::is_pos_inside_outer_walls(mon.m_pos))
        {
                TRACE << "Monster outside map" << std::endl;

                ASSERT(false);
        }

        // Sanity check - verify that monster's leader does not have a leader
        // (never allowed)
        if (mon.m_leader && !is_player_leader)
        {
                const auto* const leader_leader = mon.m_leader->m_leader;

                if (leader_leader)
                {
                        TRACE << "Monster with name '"
                              << mon.name_a()
                              << "' has a leader with name '"
                              << mon.m_leader->name_a()
                              << "', which also has a leader (not allowed!), "
                              << "with name '"
                              << leader_leader->name_a()
                              << "'"
                              << std::endl
                              << "Monster is summoned?: "
                              << mon.m_properties.has(PropId::summoned)
                              << std::endl
                              << "Leader is summoned?: "
                              << mon.m_leader->m_properties.has(
                                         PropId::summoned)
                              << std::endl
                              << "Leader's leader is summoned?: "
                              << leader_leader->m_properties.has(
                                         PropId::summoned)
                              << std::endl;

                        ASSERT(false);
                }
        }
#endif  // NDEBUG

        if (!mon.is_aware_of_player() &&
            !mon.is_wary_of_player() &&
            !is_player_leader)
        {
                mon.m_ai_state.is_waiting = !mon.m_ai_state.is_waiting;

                if (mon.m_ai_state.is_waiting)
                {
                        game_time::tick();

                        return;
                }
        }
        else
        {
                // Is wary/aware, or player is leader
                mon.m_ai_state.is_waiting = false;
        }

        // Pick a target
        std::vector<actor::Actor*> target_bucket;

        if (mon.m_properties.has(PropId::conflict))
        {
                target_bucket = seen_actors(mon);

                remove_player_with_sanctuary(target_bucket);

                mon.m_ai_state.is_target_seen = !target_bucket.empty();
        }
        else
        {
                // Not conflicted
                target_bucket = seen_foes(mon);

                remove_player_with_sanctuary(target_bucket);

                if (target_bucket.empty())
                {
                        // There are no seen foes
                        mon.m_ai_state.is_target_seen = false;

                        target_bucket =
                                static_cast<actor::Mon&>(mon)
                                        .foes_aware_of();

                        remove_player_with_sanctuary(target_bucket);
                }
                else
                {
                        // There are seen foes
                        mon.m_ai_state.is_target_seen = true;
                }
        }

        // TODO: This just returns the actor with the closest COORDINATES,
        // not the actor with the shortest free path to it - so the monster
        // could select a target which is not actually the closest to reach.
        // This could perhaps lead to especially dumb situations if the monster
        // does not move by pathding, and considers an enemy behind a wall to be
        // closer than another enemy who is in the same room.
        mon.m_ai_state.target =
                map::random_closest_actor(mon.m_pos, target_bucket);

        if (mon.is_aware_of_player() || mon.is_wary_of_player())
        {
                mon.m_ai_state.is_roaming_allowed = MonRoamingAllowed::yes;

                // Occasionally make a sound - but only if the monster does not
                // have a leader - otherwise it gets very spammy
                const bool has_living_leader =
                        mon.m_leader && mon.m_leader->is_alive();

                if (!has_living_leader &&
                    mon.is_alive() &&
                    rnd::one_in(12))
                {
                        mon.speak_phrase(AlertsMon::no);
                }
        }

        // ---------------------------------------------------------------------
        // Special monster actions
        // ---------------------------------------------------------------------
        // TODO: This will be removed eventually
        if ((mon.m_leader != map::g_player) &&
            (!mon.m_ai_state.target || mon.m_ai_state.target->is_player()))
        {
                if (mon.on_act() == DidAction::yes)
                {
                        return;
                }
        }

        // ---------------------------------------------------------------------
        // Property actions (e.g. Zombie rising, Vortex pulling, ...)
        // ---------------------------------------------------------------------
        if (mon.m_properties.on_act() == DidAction::yes)
        {
                return;
        }

        // ---------------------------------------------------------------------
        // Common actions (moving, attacking, casting spells, etc)
        // ---------------------------------------------------------------------

        // NOTE: Monsters try to detect the player visually on standard turns,
        // otherwise very fast monsters are much better at finding the player

        if (mon.m_data->ai[(size_t)actor::AiId::avoids_blocking_friend] &&
            !is_player_leader &&
            (mon.m_ai_state.target == map::g_player) &&
            mon.m_ai_state.is_target_seen &&
            rnd::coin_toss())
        {
                const auto did_act = ai::action::make_room_for_friend(mon);

                if (did_act == DidAction::yes)
                {
                        return;
                }
        }

        // Cast instead of attacking?
        if (rnd::one_in(5))
        {
                const auto did_act = ai::action::try_cast_random_spell(mon);

                if (did_act == DidAction::yes)
                {
                        return;
                }
        }

        if (mon.m_data->ai[(size_t)actor::AiId::attacks] &&
            mon.m_ai_state.target &&
            mon.m_ai_state.is_target_seen)
        {
                const auto did_act = mon.try_attack(*mon.m_ai_state.target);

                if (did_act == DidAction::yes)
                {
                        return;
                }
        }

        if (rnd::fraction(3, 4))
        {
                const auto did_act = ai::action::try_cast_random_spell(mon);

                if (did_act == DidAction::yes)
                {
                        return;
                }
        }

        int erratic_move_pct = (int)mon.m_data->erratic_move_pct;

        // Never move erratically if frenzied
        if (mon.m_properties.has(PropId::frenzied))
        {
                erratic_move_pct = 0;
        }

        // Move less erratically if allied to player
        if (is_player_leader)
        {
                erratic_move_pct /= 2;
        }

        // Move more erratically if confused
        if (mon.m_properties.has(PropId::confused) &&
            (erratic_move_pct > 0))
        {
                erratic_move_pct += 50;
        }

        erratic_move_pct = std::clamp(erratic_move_pct, 0, 95);

        // Occasionally move erratically
        if (mon.m_data->ai[(size_t)actor::AiId::moves_randomly_when_unaware] &&
            rnd::percent(erratic_move_pct))
        {
                const auto did_act = ai::action::move_to_random_adj_cell(mon);

                if (did_act == DidAction::yes)
                {
                        return;
                }
        }

        const bool is_terrified = mon.m_properties.has(PropId::terrified);

        if (mon.m_data->ai[(size_t)actor::AiId::moves_to_target_when_los] &&
            !is_terrified)
        {
                const auto did_act = ai::action::move_to_target_simple(mon);

                if (did_act == DidAction::yes)
                {
                        return;
                }
        }

        std::vector<P> path;

        if ((mon.m_data->ai[(size_t)actor::AiId::paths_to_target_when_aware] ||
             is_player_leader) &&
            !is_terrified)
        {
                path = ai::info::find_path_to_target(mon);
        }

        {
                const auto did_act =
                        ai::action::handle_closed_blocking_door(mon, path);

                if (did_act == DidAction::yes)
                {
                        return;
                }
        }

        {
                const auto did_act = ai::action::step_path(mon, path);

                if (did_act == DidAction::yes)
                {
                        return;
                }
        }

        if ((mon.m_data->ai[(size_t)actor::AiId::moves_to_leader] ||
             is_player_leader) &&
            !is_terrified)
        {
                path = ai::info::find_path_to_leader(mon);

                const auto did_act = ai::action::step_path(mon, path);

                if (did_act == DidAction::yes)
                {
                        return;
                }
        }

        if (mon.m_data->ai[(size_t)actor::AiId::moves_to_lair] &&
            !is_player_leader &&
            (!mon.m_ai_state.target || mon.m_ai_state.target->is_player()))
        {
                auto did_act =
                        ai::action::step_to_lair_if_los(
                                mon,
                                mon.m_ai_state.spawn_pos);

                if (did_act == DidAction::yes)
                {
                        return;
                }
                else
                {
                        // No LOS to lair

                        // Try to use pathfinder to travel to lair
                        path =
                                ai::info::find_path_to_lair_if_no_los(
                                        mon,
                                        mon.m_ai_state.spawn_pos);

                        did_act = ai::action::step_path(mon, path);

                        if (did_act == DidAction::yes)
                        {
                                return;
                        }
                }
        }

        if (mon.m_data->ai[(size_t)actor::AiId::moves_randomly_when_unaware] &&
            (!is_player_leader || rnd::one_in(8)))
        {
                const auto did_act = ai::action::move_to_random_adj_cell(mon);

                if (did_act == DidAction::yes)
                {
                        return;
                }
        }

        // No action could be performed, just let someone else act
        game_time::tick();
}

// -----------------------------------------------------------------------------
// actor
// -----------------------------------------------------------------------------
namespace actor
{
void act(Actor& actor)
{
        if (actor.is_player())
        {
                player_act();
        }
        else
        {
                auto& mon = static_cast<Mon&>(actor);
                mon_act(mon);
        }
}

}  // namespace actor

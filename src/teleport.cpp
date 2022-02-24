// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "teleport.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "actor.hpp"
#include "actor_data.hpp"
#include "actor_mon.hpp"
#include "actor_move.hpp"
#include "actor_player.hpp"
#include "actor_see.hpp"
#include "array2.hpp"
#include "common_text.hpp"
#include "debug.hpp"
#include "direction.hpp"
#include "flood.hpp"
#include "fov.hpp"
#include "game.hpp"
#include "game_time.hpp"
#include "map.hpp"
#include "marker.hpp"
#include "misc.hpp"
#include "msg_log.hpp"
#include "pos.hpp"
#include "property.hpp"
#include "property_data.hpp"
#include "property_factory.hpp"
#include "property_handler.hpp"
#include "random.hpp"
#include "rect.hpp"
#include "state.hpp"
#include "terrain.hpp"
#include "terrain_data.hpp"
#include "terrain_door.hpp"
#include "text_format.hpp"
#include "viewport.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static bool is_void_traveler_affecting_player_teleport(
        const actor::Actor& actor)
{
        const auto actor_id = actor.id();

        const bool is_void_traveler =
                (actor_id == actor::Id::void_traveler) ||
                (actor_id == actor::Id::elder_void_traveler);

        return (
                is_void_traveler &&
                (actor.m_state == ActorState::alive) &&
                actor.m_properties.allow_act() &&
                !actor.is_actor_my_leader(map::g_player) &&
                actor.is_aware_of_player());
}

static std::vector<P> get_free_positions_around_pos(
        const P& p,
        const Array2<bool>& blocked)
{
        std::vector<P> free_positions;

        for (const P& d : dir_utils::g_dir_list)
        {
                const P adj_p(p + d);

                if (!blocked.at(adj_p))
                {
                        free_positions.push_back(adj_p);
                }
        }

        return free_positions;
}

static void make_all_mon_not_seeing_player_unaware()
{
        for (auto* const mon : game_time::g_actors)
        {
                if (actor::is_player(mon))
                {
                        continue;
                }

                const bool can_mon_see_player =
                        can_mon_see_actor(
                                *mon,
                                *map::g_player,
                                map::g_terrain_blocks_los);

                if (!can_mon_see_player)
                {
                        mon->m_mon_aware_state.aware_counter = 0;
                }
        }
}

static void confuse_player()
{
        msg_log::add("I suddenly find myself in a different location!");

        auto* prop = property_factory::make(PropId::confused);

        prop->set_duration(8);

        map::g_player->m_properties.apply(prop);
}

static bool should_player_ctrl_tele(const ShouldCtrlTele ctrl_tele)
{
        switch (ctrl_tele)
        {
        case ShouldCtrlTele::always:
        {
                return true;
        }

        case ShouldCtrlTele::never:
        {
                return false;
        }

        case ShouldCtrlTele::if_tele_ctrl_prop:
        {
                const bool has_tele_ctrl =
                        map::g_player->m_properties.has(PropId::tele_ctrl);

                const bool is_confused =
                        map::g_player->m_properties.has(PropId::confused);

                return has_tele_ctrl && !is_confused;
        }
        }

        ASSERT(false);

        return false;
}

static void filter_out_near(const P& origin, std::vector<P>& positions)
{
        // Find the distance of the furthest position, so that we know the
        // highest possible minimum distance
        int furthest_dist = 0;

        for (const auto& p : positions)
        {
                const int d = king_dist(origin, p);

                furthest_dist = std::max(d, furthest_dist);
        }

        int min_dist = 0;

        {
                const int desired_min_dist = g_fov_radi_int;

                min_dist = std::min(desired_min_dist, furthest_dist);
        }

        // Remove all positions close than the minimum distance
        for (auto it = std::begin(positions); it != std::end(positions);)
        {
                const auto p = *it;

                const int d = king_dist(origin, p);

                if (d < min_dist)
                {
                        positions.erase(it);
                }
                else
                {
                        ++it;
                }
        }
}

// -----------------------------------------------------------------------------
// teleport
// -----------------------------------------------------------------------------
void teleport(
        actor::Actor& actor,
        const ShouldCtrlTele ctrl_tele,
        const int max_dist)
{
        // First run a floodfill with some terrain unblocked - some terrain
        // shall block teleporting past them, and some shall be allowed to
        // teleport past even though they are normally blocking.
        Array2<bool> blocked = map::get_blocked_map_info_for_actor(actor);

        const size_t nr_positions = map::nr_positions();

        // Allow teleporting past non-metal doors for the player, and past any
        // door for monsters
        for (size_t i = 0; i < nr_positions; ++i)
        {
                const auto* const r = map::g_terrain.at(i);

                if (r->id() != terrain::Id::door)
                {
                        // Not a door
                        continue;
                }

                const auto* const door = static_cast<const terrain::Door*>(r);

                if ((door->type() == terrain::DoorType::metal) &&
                    actor::is_player(&actor))
                {
                        // Metal door, player teleporting - keep it blocked
                        continue;
                }

                blocked.at(i) = false;
        }

        // Allow teleporting past Force Fields, since they are temporary
        for (const auto* const mob : game_time::g_mobs)
        {
                if (mob->id() == terrain::Id::force_field)
                {
                        blocked.at(mob->pos()) = false;
                }
        }

        // Run the floodfill, mark every position as blocked that is either
        // unreached or too far away.
        const auto flood = floodfill(actor.m_pos, blocked);

        for (const auto p : map::rect().positions())
        {
                if (flood.at(p) <= 0)
                {
                        // Unreached.
                        blocked.at(p) = true;
                }

                if (max_dist > 0)
                {
                        const int dist = king_dist(actor.m_pos, p);

                        if (dist > max_dist)
                        {
                                // Too far away.
                                blocked.at(p) = true;
                        }
                }
        }

        // Do not allow teleporting into any position that:
        // * Has terrain blocking this actor, or
        // * Has another living actor, or
        // * Blocks walking (otherwise for example ethereal creatures could
        //   teleport far into the walls)
        const auto blocked_for_actor =
                map::get_blocked_map_info_for_actor(actor);

        for (size_t i = 0; i < nr_positions; ++i)
        {
                if (blocked_for_actor.at(i) ||
                    map::g_terrain_blocks_walking.at(i))
                {
                        blocked.at(i) = true;
                }
        }

        for (const auto* const actor_found : game_time::g_actors)
        {
                if (actor_found->is_alive())
                {
                        blocked.at(actor_found->m_pos) = true;
                }
        }

        blocked.at(actor.m_pos) = false;

        // Teleport control?
        if (actor::is_player(&actor) && should_player_ctrl_tele(ctrl_tele))
        {
                auto tele_ctrl_state =
                        std::make_unique<CtrlTele>(
                                actor.m_pos,
                                blocked,
                                max_dist);

                states::push(std::move(tele_ctrl_state));

                return;
        }

        // No teleport control - teleport randomly.
        auto pos_bucket =
                to_vec(
                        blocked,
                        false,  // Store false values
                        blocked.rect());

        filter_out_near(actor.m_pos, pos_bucket);

        if (pos_bucket.empty())
        {
                return;
        }

        const auto tgt_pos = rnd::element(pos_bucket);

        teleport(actor, tgt_pos, blocked);
}

void teleport(actor::Actor& actor, P p, const Array2<bool>& blocked)
{
        const bool player_can_see_actor_before =
                actor::can_player_see_actor(actor);

        if (!actor::is_player(&actor))
        {
                actor.m_mon_aware_state.player_aware_of_me_counter = 0;
        }

        const std::vector<PropId> props_ended = {
                PropId::entangled,
                PropId::nailed};

        for (const auto id : props_ended)
        {
                const PropEndConfig cfg(
                        PropEndAllowCallEndHook::no,
                        PropEndAllowMsg::no,
                        PropEndAllowHistoricMsg::yes);

                actor.m_properties.end_prop(id, cfg);
        }

        // Hostile void travelers "intercepts" players teleporting, and calls
        // the player to them
        bool is_affected_by_void_traveler = false;

        if (actor::is_player(&actor))
        {
                for (auto* const other_actor : game_time::g_actors)
                {
                        if (!is_void_traveler_affecting_player_teleport(
                                    *other_actor))
                        {
                                continue;
                        }

                        const std::vector<P> p_bucket =
                                get_free_positions_around_pos(
                                        other_actor->m_pos,
                                        blocked);

                        if (p_bucket.empty())
                        {
                                continue;
                        }

                        // Set new teleport destination
                        p = rnd::element(p_bucket);

                        const std::string actor_name_a =
                                text_format::first_to_upper(
                                        other_actor->name_a());

                        msg_log::add(
                                actor_name_a +
                                " intercepts my teleportation!");

                        static_cast<actor::Mon*>(other_actor)
                                ->become_aware_player(
                                        actor::AwareSource::other);

                        is_affected_by_void_traveler = true;

                        break;
                }
        }

        // Leave current cell
        map::g_terrain.at(actor.m_pos)->on_leave(actor);

        // Update actor position to new position
        actor::set_position(actor, p);

        if (actor::is_player(&actor))
        {
                viewport::show(
                        map::g_player->m_pos,
                        viewport::ForceCentering::yes);
        }

        map::update_vision();

        if (actor::is_player(&actor))
        {
                static_cast<actor::Player&>(actor).update_tmp_shock();

                make_all_mon_not_seeing_player_unaware();
        }
        else if (player_can_see_actor_before)
        {
                const bool player_can_see_actor =
                        actor::can_player_see_actor(actor);

                if (!player_can_see_actor)
                {
                        actor.m_mon_aware_state.player_aware_of_me_counter = 0;
                }

                const std::string actor_name_the =
                        text_format::first_to_upper(
                                actor.name_the());

                const auto msg_ending =
                        player_can_see_actor
                        ? common_text::g_mon_disappear_reappear
                        : common_text::g_mon_disappear;

                msg_log::add(actor_name_the + " " + msg_ending);
        }

        actor::make_player_aware_seen_monsters();

        const bool has_tele_ctrl = actor.m_properties.has(PropId::tele_ctrl);

        const bool is_confused = actor.m_properties.has(PropId::confused);

        if (actor::is_player(&actor) &&
            (!has_tele_ctrl ||
             is_confused ||
             is_affected_by_void_traveler))
        {
                confuse_player();
        }

        // Bump the target terrain.
        map::g_terrain.at(p)->bump(actor);
}

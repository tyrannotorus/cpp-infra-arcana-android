// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "actor_see.hpp"

#include <algorithm>
#include <iterator>

#include "actor.hpp"
#include "fov.hpp"
#include "game_time.hpp"
#include "global.hpp"
#include "init.hpp"
#include "map.hpp"
#include "pos.hpp"
#include "property_data.hpp"
#include "property_handler.hpp"
#include "rect.hpp"
#include "terrain.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
// Return value 'true' means it is possible to see the other actor (i.e.  it's
// not impossible due to invisibility, etc), but the actor may or may not
// currently be seen due to (lack of) awareness.
static bool is_seeable_for_mon(
        const actor::Actor& mon,
        const actor::Actor& other,
        const Array2<bool>& hard_blocked_los)
{
        if ((&mon == &other) || (!other.is_alive()))
        {
                return true;
        }

        FovMap fov_map;
        fov_map.hard_blocked = &hard_blocked_los;
        fov_map.light = &map::g_light;
        fov_map.dark = &map::g_dark;

        const LosResult los = fov::check_cell(mon.m_pos, other.m_pos, fov_map);

        // LOS blocked hard (e.g. a wall or smoke)?
        if (los.is_blocked_hard)
        {
                return false;
        }

        const auto& properties = mon.m_properties;
        const auto& other_properties = other.m_properties;

        // NOTE: It should be much more common that a monster's LOS is blocked
        // than a monster being blind, so for performance it actually makes
        // sense to just run the LOS then only check to see if the monster is
        // blind if the LOS succeeds.
        if (!properties.allow_see())
        {
                // Monster is blind.
                return false;
        }

        // Actor is invisible, and monster cannot see invisible?
        if ((other_properties.has(PropId::invis) ||
             other_properties.has(PropId::cloaked)) &&
            !properties.has(PropId::see_invis))
        {
                return false;
        }

        // Blocked by darkness, and not seeing actor with infravision?
        if (los.is_blocked_by_dark &&
            !properties.has(PropId::darkvision) &&
            !properties.has(PropId::see_invis))
        {
                return false;
        }

        // OK, all checks passed, actor can bee seen
        return true;
}

static std::vector<actor::Actor*> seen_actors_player()
{
        std::vector<actor::Actor*> result;

        for (auto* const actor : game_time::g_actors)
        {
                if (actor::is_player(actor))
                {
                        continue;
                }

                if (!actor->is_alive())
                {
                        continue;
                }

                if (!actor::can_player_see_actor(*actor))
                {
                        continue;
                }

                result.push_back(actor);
        }

        return result;
}

static std::vector<actor::Actor*> seen_foes_player()
{
        std::vector<actor::Actor*> result;

        const auto seen_actors = seen_actors_player();

        result.reserve(std::size(seen_actors));

        for (auto* const actor : seen_actors)
        {
                if (map::g_player->is_leader_of(actor))
                {
                        continue;
                }

                result.push_back(actor);
        }

        return result;
}

static std::vector<actor::Actor*> seen_actors_mon(const actor::Actor& mon)
{
        std::vector<actor::Actor*> result;

        R los_rect(
                std::max(0, mon.m_pos.x - g_fov_radi_int),
                std::max(0, mon.m_pos.y - g_fov_radi_int),
                std::min(map::w() - 1, mon.m_pos.x + g_fov_radi_int),
                std::min(map::h() - 1, mon.m_pos.y + g_fov_radi_int));

        for (auto* const other_actor : game_time::g_actors)
        {
                if (other_actor == &mon)
                {
                        continue;
                }

                if (!other_actor->is_alive())
                {
                        continue;
                }

                const bool can_see_actor =
                        actor::can_mon_see_actor(
                                mon,
                                *other_actor,
                                map::g_terrain_blocks_los);

                if (!can_see_actor)
                {
                        continue;
                }

                result.push_back(other_actor);
        }

        return result;
}

static std::vector<actor::Actor*> seen_foes_mon(const actor::Actor& mon)
{
        std::vector<actor::Actor*> result;

        const auto seen_actors = seen_actors_mon(mon);

        result.reserve(std::size(seen_actors));

        for (auto* const other_actor : seen_actors)
        {
                const bool is_hostile_to_player =
                        !mon.is_actor_my_leader(map::g_player);

                const bool is_other_hostile_to_player =
                        actor::is_player(other_actor)
                        ? false
                        : !other_actor->is_actor_my_leader(map::g_player);

                const bool is_enemy =
                        (is_hostile_to_player !=
                         is_other_hostile_to_player);

                if (!is_enemy)
                {
                        continue;
                }

                result.push_back(other_actor);
        }

        return result;
}

// -----------------------------------------------------------------------------
// actor
// -----------------------------------------------------------------------------
namespace actor
{
bool can_player_see_actor(const Actor& other)
{
        if (is_player(&other))
        {
                return true;
        }

        const auto& player = *map::g_player;

        if (init::g_is_cheat_vision_enabled)
        {
                return true;
        }

        if (!other.is_alive() && map::g_seen.at(other.m_pos))
        {
                // Dead actor in seen cell
                return true;
        }

        if (!player.m_properties.allow_see())
        {
                // Player is blind
                return false;
        }

        if (map::g_los.at(other.m_pos).is_blocked_hard)
        {
                // LOS blocked hard (e.g. a wall)
                return false;
        }

        const bool can_see_invis =
                player.m_properties.has(PropId::see_invis);

        const bool is_mon_invis =
                (other.m_properties.has(PropId::invis) ||
                 other.m_properties.has(PropId::cloaked));

        if (is_mon_invis && !can_see_invis)
        {
                // Monster is invisible, and player cannot see invisible
                return false;
        }

        const bool has_darkvision =
                player.m_properties.has(PropId::darkvision);

        const bool can_see_other_in_drk = can_see_invis || has_darkvision;

        if (map::g_los.at(other.m_pos).is_blocked_by_dark &&
            !can_see_other_in_drk)
        {
                // Blocked by darkness, and cannot see creatures in darkness
                return false;
        }

        if (other.is_sneaking() && !can_see_invis)
        {
                return false;
        }

        // All checks passed, actor can bee seen
        return true;
}

bool can_mon_see_actor(
        const Actor& mon,
        const Actor& other,
        const Array2<bool>& hard_blocked_los)
{
        if (!is_seeable_for_mon(mon, other, hard_blocked_los))
        {
                return false;
        }

        if (mon.is_actor_my_leader(map::g_player))
        {
                // Monster is allied to player

                if (is_player(&other))
                {
                        // Player-allied monster looking at the player
                        return true;
                }

                // Player-allied monster looking at other monster

                return other.is_player_aware_of_me();
        }

        // Monster is hostile to player

        return mon.is_aware_of_player();
}

std::vector<Actor*> seen_actors(const Actor& actor)
{
        if (actor::is_player(&actor))
        {
                return seen_actors_player();
        }
        else
        {
                return seen_actors_mon(actor);
        }
}

std::vector<Actor*> seen_foes(const Actor& actor)
{
        if (actor::is_player(&actor))
        {
                return seen_foes_player();
        }
        else
        {
                return seen_foes_mon(actor);
        }
}

std::vector<Actor*> seeable_foes_for_mon(const Actor& mon)
{
        std::vector<Actor*> result;

        for (auto* other_actor : game_time::g_actors)
        {
                if (other_actor == &mon)
                {
                        continue;
                }

                if (!other_actor->is_alive())
                {
                        continue;
                }

                const bool is_hostile_to_player =
                        !mon.is_actor_my_leader(map::g_player);

                const bool is_other_hostile_to_player =
                        is_player(other_actor)
                        ? false
                        : !other_actor->is_actor_my_leader(map::g_player);

                const bool is_enemy =
                        is_hostile_to_player !=
                        is_other_hostile_to_player;

                if (!is_enemy)
                {
                        continue;
                }

                if (!is_seeable_for_mon(
                            mon,
                            *other_actor,
                            map::g_terrain_blocks_los))
                {
                        continue;
                }

                result.push_back(other_actor);
        }

        return result;
}

bool is_player_seeing_burning_terrain()
{
        const auto& player = *map::g_player;

        const auto fov_r = fov::fov_rect(player.m_pos, map::dims());

        for (const auto& pos : fov_r.positions())
        {
                const bool is_seen = map::g_seen.at(pos);
                const auto* const terrain = map::g_terrain.at(pos);

                if (is_seen && terrain->is_burning())
                {
                        return true;
                }
        }

        return false;
}

}  // namespace actor

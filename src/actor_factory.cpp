// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "actor_factory.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <ostream>

#include "actor.hpp"
#include "actor_data.hpp"
#include "array2.hpp"
#include "debug.hpp"
#include "game_time.hpp"
#include "map.hpp"
#include "map_parsing.hpp"
#include "misc.hpp"
#include "pos.hpp"
#include "property_data.hpp"
#include "property_handler.hpp"
#include "random.hpp"
#include "terrain.hpp"
#include "terrain_data.hpp"
#include "terrain_door.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static std::vector<P> free_spawn_positions(const R& area)
{
        // NOTE: Here we only allow spawning on positions that do not block
        // walking. This is a simple rule but somewhat strict rule; all
        // creatures may exist on such positions, but some creatures could
        // potentially be spawned elsewhere (such as a flying creature over a
        // chasm). The current method should be good enough however.

        Array2<bool> blocked = map::g_terrain_blocks_walking;

        for (const actor::Actor* const actor : game_time::g_actors) {
                if (actor->is_alive()) {
                        blocked.at(actor->m_pos) = true;
                }
        }

        return to_vec(blocked, false, area);
}

static actor::Actor* spawn_at(const P& pos, const actor::Id id)
{
        if (!map::is_pos_inside_outer_walls(pos)) {
                TRACE
                        << ("Attempted to spawn monster at position not "
                            "completely inside the outer walls: ")
                        << pos.x << "," << pos.y
                        << " - position blocks walking in map info cache?: "
                        << map::g_terrain_blocks_walking.at(pos)
                        << std::endl;

                ASSERT(false);

                return nullptr;
        }

        return actor::make(id, pos);
}

static actor::MonSpawnResult spawn_at_positions(
        const std::vector<P>& positions,
        const std::vector<actor::Id>& ids)
{
        actor::MonSpawnResult result;

        const size_t nr_to_spawn = std::min(positions.size(), ids.size());

        for (size_t i = 0; i < nr_to_spawn; ++i) {
                const auto& pos = positions[i];
                const auto id = ids[i];

                actor::Actor* const new_mon = spawn_at(pos, id);

                if (new_mon) {
                        result.monsters.push_back(new_mon);
                }
        }

        return result;
}

// -----------------------------------------------------------------------------
// actor
// -----------------------------------------------------------------------------
namespace actor
{
MonSpawnResult& MonSpawnResult::set_leader(Actor* const leader)
{
        std::for_each(
                std::begin(monsters),
                std::end(monsters),
                [leader](auto mon) {
                        mon->m_leader = leader;
                });

        return *this;
}

MonSpawnResult& MonSpawnResult::make_aware_of_player()
{
        std::for_each(
                std::begin(monsters),
                std::end(monsters),
                [](auto mon) {
                        mon->m_mon_aware_state.aware_counter =
                                mon->m_data->nr_turns_aware;
                });

        return *this;
}

Actor* make(const Id id, const P& pos)
{
        auto* actor = new Actor();

        init_actor(*actor, pos, g_data[(size_t)id]);

        if (actor->m_data->nr_left_allowed_to_spawn > 0) {
                --actor->m_data->nr_left_allowed_to_spawn;
        }

        game_time::add_actor(actor);

        actor->m_properties.on_placed();

#ifndef NDEBUG
        if (map::nr_positions() != 0) {
                const auto* const t = map::g_terrain.at(pos);

                if (t->id() == terrain::Id::door) {
                        const auto* const door =
                                static_cast<const terrain::Door*>(t);

                        ASSERT(
                                door->is_open() ||
                                actor->m_properties.has(PropId::ooze) ||
                                actor->m_properties.has(PropId::ethereal));
                }
        }
#endif  // NDEBUG

        return actor;
}

void delete_all_mon()
{
        std::vector<Actor*>& actors = game_time::g_actors;

        for (auto it = std::begin(actors); it != std::end(actors);) {
                Actor* const actor = *it;

                if (actor::is_player(actor)) {
                        ++it;
                }
                else {
                        // Is monster
                        delete actor;

                        it = actors.erase(it);
                }
        }
}

MonSpawnResult spawn(
        const P& origin,
        const std::vector<Id>& monster_ids,
        const R& area_allowed)
{
        auto free_positions = free_spawn_positions(area_allowed);

        if (free_positions.empty()) {
                return {};
        }

        std::sort(
                std::begin(free_positions),
                std::end(free_positions),
                IsCloserToPos(origin));

        return spawn_at_positions(free_positions, monster_ids);
}

MonSpawnResult spawn_random_position(
        const std::vector<Id>& monster_ids,
        const R& area_allowed)
{
        auto free_positions = free_spawn_positions(area_allowed);

        if (free_positions.empty()) {
                return {};
        }

        rnd::shuffle(free_positions);

        return spawn_at_positions(free_positions, monster_ids);
}

}  // namespace actor

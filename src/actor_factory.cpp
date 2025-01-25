// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "actor_factory.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <map>
#include <ostream>
#include <unordered_map>
#include <utility>

#include "actor.hpp"
#include "actor_data.hpp"
#include "array2.hpp"
#include "debug.hpp"
#include "flood.hpp"
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
static void set_all_adj_positions_to_blocking(const P& pos, Array2<bool>& blocked)
{
        for (int x = -1; x <= 1; ++x) {
                for (int y = -1; y <= 1; ++y) {
                        const P p_adj = pos.with_offsets(x, y);

                        blocked.at(p_adj) = true;
                }
        }
}

// Returns a vector of pairs of positions and the travel distance to each position.
static std::vector<std::pair<P, int>> free_spawn_positions(
        const P& origin,
        const int max_dist,
        const actor::AllowSpawnAdjToCurrentActors allow_adj_to_actors)
{
        // NOTE: Here we only allow spawning on positions that do not block walking. This is a
        // simple, but somewhat strict rule; all creatures may exist on such positions, but some
        // creatures could potentially be spawned elsewhere (such as a flying creature over a
        // chasm). The current method should be good enough however.

        Array2<bool> blocked(map::dims());

        // Get a blocking map for running a floodfill, where actors and doors are free.
        map_parsers::BlocksWalking(ParseActors::no)
                .run(blocked, map::rect(), MapParseMode::overwrite);

        // NOTE: Doors are blocking or not depending on their open/closed status. This is because
        // closed doors should contain breeding monsters.

        Array2<int> flood = floodfill(origin, blocked, max_dist);

        // Now mark actors as blocking.
        for (const actor::Actor* const actor : game_time::g_actors) {
                if (!actor::is_alive(*actor)) {
                        continue;
                }

                if (allow_adj_to_actors == actor::AllowSpawnAdjToCurrentActors::yes) {
                        blocked.at(actor->m_pos) = true;
                }
                else {
                        set_all_adj_positions_to_blocking(actor->m_pos, blocked);
                }
        }

        // Now mark ALL doors as blocking (regardless of open/closed status). We don't want
        // stationary monsters like Mold to spawn ON open doors.
        for (const terrain::Terrain* const terrain : map::g_terrain) {
                if (terrain->id() == terrain::Id::door) {
                        blocked.at(terrain->pos()) = true;
                }
        }

        std::vector<std::pair<P, int>> free_positions;

        free_positions.reserve(map::nr_positions());

        for (const P& p : map::positions()) {
                if (!blocked.at(p)) {
                        const int flood_value = flood.at(p);

                        if ((flood_value > 0) || (p == origin)) {
                                free_positions.emplace_back(p, flood_value);
                        }
                }
        }

        return free_positions;
}

static actor::Actor* spawn_at(const P& pos, const std::string& id)
{
        ASSERT(map::is_pos_inside_outer_walls(pos));

        return actor::make(id, pos);
}

static actor::MonSpawnResult spawn_at_positions_close(
        const std::vector<std::pair<P, int>>& positions,
        const std::vector<std::string>& ids)
{
        actor::MonSpawnResult result;

        const size_t nr_to_spawn = std::min(positions.size(), ids.size());

        // Calculate distances from the origin and group positions by distance.
        std::map<int, std::vector<P>> distance_groups;

        for (const auto& [pos, distance] : positions) {
                distance_groups[distance].push_back(pos);
        }

        size_t id_idx = 0;

        // Iterate through distances in ascending order.
        for (auto& [distance, distance_group] : distance_groups) {
                while (!distance_group.empty() && (id_idx < nr_to_spawn)) {
                        const P pos = rnd::element(distance_group);

                        // Remove the selected position from the group.
                        distance_group.erase(
                                std::remove(
                                        std::begin(distance_group),
                                        std::end(distance_group),
                                        pos),
                                std::end(distance_group));

                        actor::Actor* const mon = spawn_at(pos, ids[id_idx]);

                        if (mon) {
                                result.monsters.push_back(mon);
                        }

                        ++id_idx;
                }

                if (id_idx >= nr_to_spawn) {
                        break;
                }
        }

        return result;
}

static actor::MonSpawnResult spawn_at_positions_scattered(
        std::vector<std::pair<P, int>>& positions,
        const std::vector<std::string>& ids)
{
        actor::MonSpawnResult result;

        const size_t nr_to_spawn = std::min(positions.size(), ids.size());

        for (size_t i = 0; i < nr_to_spawn; ++i) {
                const size_t pos_idx = rnd::idx(positions);

                const P pos = positions[pos_idx].first;

                // Remove this position.
                std::swap(positions[pos_idx], positions.back());
                positions.pop_back();

                const std::string& id = ids[i];

                actor::Actor* const mon = spawn_at(pos, id);

                if (mon) {
                        result.monsters.push_back(mon);
                }
        }

        return result;
}

static actor::MonSpawnResult spawn_at_positions(
        std::vector<std::pair<P, int>>& positions,
        const std::vector<std::string>& ids,
        const actor::SpawnScattered spawn_scattered)
{
        if (spawn_scattered == actor::SpawnScattered::no) {
                return spawn_at_positions_close(positions, ids);
        }
        else {
                return spawn_at_positions_scattered(positions, ids);
        }
}

static std::vector<std::string> ids_for_starting_allies(
        const actor::StartingAllyEntry& allies_entry)
{
        return {(size_t)allies_entry.nr.roll(), allies_entry.id};
}

static void disable_player_feeling_msg(
        const std::vector<actor::Actor*>& actors)
{
        std::for_each(
                std::begin(actors),
                std::end(actors),
                [](actor::Actor* mon) {
                        mon->m_mon_aware_state.is_player_feeling_msg_allowed = false;
                });
}

static actor::Actor* find_top_leader(actor::Actor& actor)
{
        if (actor.m_leader) {
                return find_top_leader(*actor.m_leader);
        }
        else {
                return &actor;
        }
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
                [leader](Actor* const mon) {
                        mon->m_leader = leader;
                });

        return *this;
}

MonSpawnResult& MonSpawnResult::make_aware_of_player()
{
        std::for_each(
                std::begin(monsters),
                std::end(monsters),
                [](Actor* const mon) {
                        mon->m_mon_aware_state.aware_counter = mon->nr_turns_to_be_aware();
                });

        return *this;
}

Actor* make(const std::string& id, const P& pos)
{
        auto* actor = new Actor();

        const auto data_result = g_data.find(id);

        if (data_result == std::end(g_data)) {
                TRACE_ERROR_RELEASE
                        << "Undefined monster ID: "
                        << "'" << id << "'" << std::endl;

                PANIC;
        }

        init_actor(*actor, pos, data_result->second);

        if (actor->m_data->nr_left_allowed_to_spawn > 0) {
                --actor->m_data->nr_left_allowed_to_spawn;
        }

        game_time::add_actor(actor);

        actor->m_properties.on_placed();

#ifndef NDEBUG
        if (map::nr_positions() != 0) {
                const terrain::Terrain* const t = map::g_terrain.at(pos);

                if (t->id() == terrain::Id::door) {
                        const auto* const door = static_cast<const terrain::Door*>(t);

                        ASSERT(
                                door->is_open() ||
                                actor->m_properties.has(prop::Id::ooze) ||
                                actor->m_properties.has(prop::Id::ethereal));
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
        const std::vector<std::string>& monster_ids,
        const int max_dist,
        const SpawnScattered spawn_scattered,
        const AllowSpawnAdjToCurrentActors allow_adj_to_actors)
{
        std::vector<std::pair<P, int>> free_positions =
                free_spawn_positions(origin, max_dist, allow_adj_to_actors);

        if (free_positions.empty()) {
                return {};
        }

        return spawn_at_positions(free_positions, monster_ids, spawn_scattered);
}

void spawn_starting_allies(actor::Actor& main_actor)
{
        const std::vector<actor::StartingAllyEntry>& allies =
                main_actor.m_data->starting_allies;

        for (const actor::StartingAllyEntry& entry : allies) {
                const std::vector<std::string> ids = ids_for_starting_allies(entry);

                actor::Actor* const top_leader = find_top_leader(main_actor);

                const actor::MonSpawnResult summoned =
                        spawn(main_actor.m_pos, ids)
                                .set_leader(top_leader);

                disable_player_feeling_msg(summoned.monsters);
        }
}

}  // namespace actor

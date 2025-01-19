// =============================================================================
// Copyright 2011-2024 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef ACTOR_FACTORY_HPP
#define ACTOR_FACTORY_HPP

#include <algorithm>
#include <string>
#include <vector>

struct P;
struct R;

namespace actor
{
class Actor;

enum class MakeMonAware
{
        no,
        yes
};

// Spawn somewhere in a "nearby area" around the chosen position (using a floodfill), instead of as
// close to it as possible?
enum class SpawnScattered
{
        no,
        yes,
};

enum class AllowSpawnAdjToCurrentActors
{
        no,
        yes,
};

struct MonSpawnResult
{
public:
        MonSpawnResult() = default;

        MonSpawnResult& set_leader(Actor* leader);

        MonSpawnResult& make_aware_of_player();

        std::vector<Actor*> monsters;
};

void delete_all_mon();

Actor* make(const std::string& id, const P& pos);

// NOTE: If spawning scattered, it is important to set the max distance, otherwise the monsters will
// spawn at a random position potentially anywhere on the whole map.
MonSpawnResult spawn(
        const P& origin,
        const std::vector<std::string>& monster_ids,
        int max_dist = -1,
        SpawnScattered spawn_scattered = SpawnScattered::no,
        AllowSpawnAdjToCurrentActors allow_adj_to_actors = AllowSpawnAdjToCurrentActors::yes);

void spawn_starting_allies(actor::Actor& main_actor);

}  // namespace actor

#endif  // ACTOR_FACTORY_HPP

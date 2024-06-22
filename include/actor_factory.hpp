// =============================================================================
// Copyright 2011-2024 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef ACTOR_FACTORY_HPP
#define ACTOR_FACTORY_HPP

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

MonSpawnResult spawn(
        const P& origin,
        const std::vector<std::string>& monster_ids,
        const R& area_allowed);

MonSpawnResult spawn_random_position(
        const std::vector<std::string>& monster_ids,
        const R& area_allowed);

}  // namespace actor

#endif  // ACTOR_FACTORY_HPP

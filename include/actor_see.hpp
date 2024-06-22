// =============================================================================
// Copyright 2011-2024 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef ACTOR_SEE_HPP
#define ACTOR_SEE_HPP

#include <vector>

#include "array2.hpp"

namespace actor
{
class Actor;

bool can_player_see_actor(const Actor& other);

bool can_mon_see_actor(
        const Actor& mon,
        const Actor& other,
        const Array2<bool>& hard_blocked_los);

std::vector<Actor*> seen_actors(const Actor& actor);

std::vector<Actor*> seen_foes(const Actor& actor);

// Finds actors which are POSSIBLE to see (not prevented by invisibility, for
// example), but may or may not currently be seen due to (lack of) awareness.
std::vector<Actor*> seeable_foes_for_mon(const Actor& mon);

bool is_player_seeing_burning_terrain();

}  // namespace actor

#endif  // ACTOR_SEE_HPP

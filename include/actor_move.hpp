// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef ACTOR_MOVE_HPP
#define ACTOR_MOVE_HPP

#include "direction.hpp"

struct P;

namespace actor
{
class Actor;

void do_move_action(Actor& actor, Dir dir);

// NOTE: Changing an actor's position shall always be done through this function
// (not by directly changing their position data):
void set_position(Actor& actor, const P& pos);

}  // namespace actor

#endif  // ACTOR_MOVE_HPP

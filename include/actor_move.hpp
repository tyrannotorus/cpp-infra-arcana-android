// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef ACTOR_MOVE_HPP
#define ACTOR_MOVE_HPP

#include "direction.hpp"

namespace actor
{
class Actor;

void move(Actor& actor, Dir dir);

}  // namespace actor

#endif  // ACTOR_MOVE_HPP

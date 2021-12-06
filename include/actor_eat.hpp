// =============================================================================
// Copyright 2011-2021 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef ACTOR_EAT_HPP
#define ACTOR_EAT_HPP

#include "actor.hpp"
#include "global.hpp"

namespace actor
{
DidAction try_eat_corpse(actor::Actor& actor);

void heal_from_eating(actor::Actor& actor);

}  // namespace actor

#endif  // ACTOR_EAT_HPP

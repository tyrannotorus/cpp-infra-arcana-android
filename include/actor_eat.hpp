// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef ACTOR_EAT_HPP
#define ACTOR_EAT_HPP

#include "global.hpp"

namespace actor
{
class Actor;

DidAction try_eat_corpse(actor::Actor& actor);

void heal_from_eating(actor::Actor& actor);

}  // namespace actor

#endif  // ACTOR_EAT_HPP

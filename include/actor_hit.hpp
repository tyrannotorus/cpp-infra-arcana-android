// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef ACTOR_HIT_HPP
#define ACTOR_HIT_HPP

#include "global.hpp"

namespace actor
{
class Actor;

void hit(
        Actor& actor,
        int dmg,
        DmgType dmg_type,
        actor::Actor* attacker,
        AllowWound allow_wound = AllowWound::yes);

void hit_sp(
        Actor& actor,
        int dmg,
        actor::Actor* attacker,
        Verbose verbose = Verbose::yes);

}  // namespace actor

#endif  // ACTOR_HIT_HPP

// =============================================================================
// Copyright 2011-2023 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef ACTOR_SNEAK_HPP
#define ACTOR_SNEAK_HPP

enum class ActionResult;

namespace actor
{
class Actor;

struct SneakParameters
{
        const actor::Actor* actor_sneaking {nullptr};
        const actor::Actor* actor_searching {nullptr};
};

int calc_total_sneak_ability(const SneakParameters& data);

// This function is not concerned with whether actors are within FOV, or if they
// are actually hidden or not. It merely performs a skill check, taking various
// conditions such as light/dark into concern.
ActionResult roll_sneak(const SneakParameters& data);

}  // namespace actor

#endif  // ACTOR_SNEAK_HPP

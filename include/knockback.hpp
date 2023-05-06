// =============================================================================
// Copyright 2011-2023 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef KNOCKBACK_HPP
#define KNOCKBACK_HPP

#include "global.hpp"

namespace actor
{
class Actor;
}  // namespace actor

struct P;

namespace knockback
{
enum class KnockbackSource
{
        spike_gun,
        other
};

void run(
        actor::Actor& actor,
        const P& attacked_from_pos,
        KnockbackSource source,
        Verbose verbose = Verbose::yes,
        int paralyze_extra_turns = 0);

}  // namespace knockback

#endif  // KNOCKBACK_HPP

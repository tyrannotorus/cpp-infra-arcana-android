// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef TELEPORT_HPP
#define TELEPORT_HPP

#include "global.hpp"

struct P;

namespace actor
{
class Actor;
}  // namespace actor

template <typename T>
class Array2;

void teleport(
        actor::Actor& actor,
        ShouldCtrlTele ctrl_tele = ShouldCtrlTele::if_tele_ctrl_prop,
        int max_dist = -1);

void teleport(
        actor::Actor& actor,
        P pos,
        const Array2<bool>& blocked,
        // Only used to decide if the actor should be confused.
        bool has_tele_ctrl);

#endif  // TELEPORT_HPP

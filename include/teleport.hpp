// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
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
        P p,
        const Array2<bool>& blocked);

#endif  // TELEPORT_HPP

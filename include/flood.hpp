// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef FLOOD_HPP
#define FLOOD_HPP

#include "array2.hpp"
#include "pos.hpp"

Array2<int> floodfill(
        const P& p0,
        const Array2<bool>& blocked,
        int travel_lmt = -1,
        const P& p1 = P(-1, -1),
        bool allow_diagonal = true);

#endif  // FLOOD_HPP

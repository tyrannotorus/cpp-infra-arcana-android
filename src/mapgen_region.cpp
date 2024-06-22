// =============================================================================
// Copyright 2011-2024 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "mapgen.hpp"
#include "pos.hpp"
#include "random.hpp"
#include "rect.hpp"

R Region::rnd_room_rect() const
{
        const int max_dim = 21;

        // Set random size
        const Range w_range(2, std::min(max_dim, r.p1.x - r.p0.x + 1));
        const Range h_range(2, std::min(max_dim, r.p1.y - r.p0.y + 1));

        const int w = w_range.roll();
        const int h = h_range.roll();

        // Set random position
        const P p0(
                r.p0.x + rnd::range(0, w_range.max - w),
                r.p0.y + rnd::range(0, h_range.max - h));

        const P p1(p0.x + w - 1, p0.y + h - 1);

        return R(p0, p1);

}  // rnd_room_rect

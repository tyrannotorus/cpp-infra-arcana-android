// =============================================================================
// Copyright 2011-2024 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef FOV_HPP
#define FOV_HPP

#include "array2.hpp"
#include "rect.hpp"

struct P;

struct FovMap
{
        // NOTE: These fields are NOT optional, even though they are pointers
        const Array2<bool>* hard_blocked {nullptr};
        const Array2<bool>* light {nullptr};
        const Array2<bool>* dark {nullptr};
};

struct LosResult
{
        bool is_blocked_hard {false};
        bool is_blocked_by_dark {false};
};

namespace fov
{
R fov_rect(const P& p, const P& map_dims);

bool is_in_fov_range(const P& p0, const P& p1);

LosResult check_cell(
        const P& p0,
        const P& p1,
        const FovMap& map);

Array2<LosResult> run(const P& p0, const FovMap& map);

}  // namespace fov

#endif  // FOV_HPP

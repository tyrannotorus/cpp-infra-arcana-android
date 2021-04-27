// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef BSP_HPP
#define BSP_HPP

#include <vector>

struct R;

namespace bsp
{
struct BlockedSplitPositions
{
        std::vector<int> x {};
        std::vector<int> y {};
};

std::vector<R> try_split(
        const R& rect,
        int child_min_size,
        const BlockedSplitPositions& blocked_split_positions);

}  // namespace bsp

#endif  // BSP_HPP

// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "bsp.hpp"

#include <algorithm>
#include <iterator>
#include <optional>

#include "global.hpp"
#include "pos.hpp"
#include "random.hpp"
#include "rect.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static Axis random_child_layout(const P& parent_dims)
{
        const bool split_largest_dim =
                (parent_dims.x != parent_dims.y) &&
                rnd::fraction(2, 3);

        if (split_largest_dim)
        {
                return (
                        (parent_dims.x > parent_dims.y)
                                ? Axis::hor
                                : Axis::ver);
        }
        else
        {
                return (
                        rnd::coin_toss()
                                ? Axis::hor
                                : Axis::ver);
        }
}

static std::optional<Axis> child_layout(
        const P& parent_dims,
        const int child_min_size)
{
        const int parent_min_size = (child_min_size * 2) + 1;

        const bool is_w_ok = parent_dims.x > parent_min_size;
        const bool is_h_ok = parent_dims.y > parent_min_size;

        if (is_w_ok && is_h_ok)
        {
                return random_child_layout(parent_dims);
        }
        else if (is_w_ok)
        {
                return Axis::hor;
        }
        else if (is_h_ok)
        {
                return Axis::ver;
        }
        else
        {
                // No split possible
                return {};
        }
}

static std::vector<int> split_pos_candidates(
        const Range& pos_range,
        const std::vector<int>& blocked_positions)
{
        std::vector<int> candidates;

        for (int pos = pos_range.min; pos <= pos_range.max; ++pos)
        {
                const auto is_free =
                        std::find(
                                std::begin(blocked_positions),
                                std::end(blocked_positions),
                                pos) == std::end(blocked_positions);

                if (is_free)
                {
                        candidates.push_back(pos);
                }
        }

        return candidates;
}

// -----------------------------------------------------------------------------
// bsp
// -----------------------------------------------------------------------------
namespace bsp
{
std::vector<R> try_split(
        const R& rect,
        const int child_min_size,
        const BlockedSplitPositions& blocked_split_positions)
{
        const auto layout = child_layout(rect.dims(), child_min_size);

        if (!layout)
        {
                // No splitting possible
                return {};
        }

        auto child_rect_1 = rect;
        auto child_rect_2 = rect;

        if (layout == Axis::hor)
        {
                // Horizontal split
                const Range split_range(
                        rect.p0.x + child_min_size,
                        rect.p1.x - child_min_size);

                const auto pos_bucket =
                        split_pos_candidates(
                                split_range,
                                blocked_split_positions.x);

                if (pos_bucket.empty())
                {
                        return {};
                }

                const int split_pos = rnd::element(pos_bucket);

                child_rect_1.p1.x = split_pos - 1;
                child_rect_2.p0.x = split_pos + 1;
        }
        else
        {
                // Vertical split
                const Range split_range(
                        rect.p0.y + child_min_size,
                        rect.p1.y - child_min_size);

                const auto pos_bucket =
                        split_pos_candidates(
                                split_range,
                                blocked_split_positions.y);

                if (pos_bucket.empty())
                {
                        return {};
                }

                const int split_pos = rnd::element(pos_bucket);

                child_rect_1.p1.y = split_pos - 1;
                child_rect_2.p0.y = split_pos + 1;
        }

        return {child_rect_1, child_rect_2};
}

}  // namespace bsp

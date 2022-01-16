// =============================================================================
// Copyright 2011-2021 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef RECT_HPP
#define RECT_HPP

#include <vector>

#include "pos.hpp"

#include <iostream>

struct R
{
public:
        R() = default;

        R(const P& p0_val, const P& p1_val) :
                p0(p0_val),
                p1(p1_val)
        {}

        R(const int x0, const int y0, const int x1, const int y1) :
                p0({x0, y0}),
                p1({x1, y1})
        {}

        R(const R& r) = default;

        R& operator=(const R& other) = default;

        int w() const
        {
                return p1.x - p0.x + 1;
        }

        int h() const
        {
                return p1.y - p0.y + 1;
        }

        int area() const
        {
                return w() * h();
        }

        P dims() const
        {
                return {w(), h()};
        }

        int min_dim() const
        {
                return std::min(w(), h());
        }

        int max_dim() const
        {
                return std::max(w(), h());
        }

        P center() const
        {
                const int x = (p0.x + p1.x) / 2;
                const int y = (p0.y + p1.y) / 2;

                return {x, y};
        }

        bool is_pos_inside(const P p) const
        {
                const bool is_inside =
                        (p.x >= p0.x) &&
                        (p.y >= p0.y) &&
                        (p.x <= p1.x) &&
                        (p.y <= p1.y);

                return is_inside;
        }

        R with_offset(const P offset) const
        {
                return {
                        p0 + P(offset.x, offset.y),
                        p1 + P(offset.x, offset.y)};
        }

        R with_offset(const int x_offset, const int y_offset) const
        {
                return {
                        p0 + P(x_offset, y_offset),
                        p1 + P(x_offset, y_offset)};
        }

        R scaled_up(const int factor) const
        {
                return scaled_up(factor, factor);
        }

        R scaled_up(const int x_factor, const int y_factor) const
        {
                const auto new_p0 = p0.scaled_up(x_factor, y_factor);

                const auto new_dims = dims().scaled_up(x_factor, y_factor);

                const R scaled_rect(new_p0, new_p0 + new_dims - 1);

                return scaled_rect;
        }

        std::vector<P> positions() const
        {
                std::vector<P> result;

                result.reserve(area());

                for (int x = p0.x; x <= p1.x; ++x)
                {
                        for (int y = p0.y; y <= p1.y; ++y)
                        {
                                result.emplace_back(x, y);
                        }
                }

                return result;
        }

        P p0 {};
        P p1 {};
};

#endif  // RECT_HPP

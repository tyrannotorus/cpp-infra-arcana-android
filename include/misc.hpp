// =============================================================================
// Copyright 2011-2023 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef MISC_HPP
#define MISC_HPP

// TODO: "misc" is not a good name...

#include <string>
#include <vector>

struct P;
struct R;

template <typename T>
class Array2;

// Takes a boolean map array, and populates a vector with positions inside the
// given area matching the specified value to store (true/false).
std::vector<P> to_vec(
        const Array2<bool>& a,
        bool value_to_store,
        const R& area_to_parse);

bool is_pos_inside(const P& pos, const R& area);

bool is_area_inside(
        const R& inner,
        const R& outer,
        bool count_equal_as_inside);

bool is_pos_adj(
        const P& pos1,
        const P& pos2,
        bool count_same_cell_as_adj);

P closest_pos(const P& p, const std::vector<P>& positions);

// Distance as the king moves in chess. This is also referred to as the
// "chessboard distance" or "Chebyshev distance".
//
// The distance between (x0, y0) and (x1, y1) is defined as:
// max(|x1 - x0|, |y1 - y0|).
// This is typically the model used for movement in roguelikes.
int king_dist(int x0, int y0, int x1, int y1);

int king_dist(const P& p0, const P& p1);

// Taxicab distance, or "rectilinear distance", "Manhattan distance", etc.
// The distance between (x0, y0) and (x1, y1) is defined as:
// |x1 - x0| + |y1 - y0|.
int taxi_dist(const P& p0, const P& p1);

int to_int(const std::string& in);

#endif  // MISC_HPP

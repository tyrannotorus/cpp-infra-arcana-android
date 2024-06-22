// =============================================================================
// Copyright 2011-2024 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "array2.hpp"
#include "catch.hpp"
#include "pos.hpp"

TEST_CASE("2d array transformation")
{
        Array2<char> a(3, 5);

        REQUIRE(a.dims() == P(3, 5));

        a.at(0, 0) = 'x';

        REQUIRE(a.at(0, 0) == 'x');

        // Rotate clock wise
        a.rotate_cw();

        REQUIRE(a.dims() == P(5, 3));

        REQUIRE(a.at(0, 0) == 0);

        REQUIRE(a.at(4, 0) == 'x');

        // Flip vertically
        a.flip_ver();

        REQUIRE(a.at(4, 0) == 0);

        REQUIRE(a.at(4, 2) == 'x');

        // Rotate counter clock wise
        a.rotate_ccw();

        REQUIRE(a.dims() == P(3, 5));

        REQUIRE(a.at(2, 0) == 'x');

        // Flip horizontally
        a.flip_hor();

        REQUIRE(a.at(2, 0) == 0);

        REQUIRE(a.at(0, 0) == 'x');
}

TEST_CASE("2d array position index conversion")
{
        Array2<char> a(3, 5);

        REQUIRE(a.idx_to_pos(0) == P(0, 0));
        REQUIRE(a.pos_to_idx(0, 0) == 0);

        REQUIRE(a.idx_to_pos(1) == P(0, 1));
        REQUIRE(a.pos_to_idx(0, 1) == 1);

        REQUIRE(a.idx_to_pos(2) == P(0, 2));
        REQUIRE(a.pos_to_idx(0, 2) == 2);

        REQUIRE(a.idx_to_pos(4) == P(0, 4));
        REQUIRE(a.pos_to_idx(0, 4) == 4);

        REQUIRE(a.idx_to_pos(5) == P(1, 0));
        REQUIRE(a.pos_to_idx(1, 0) == 5);

        REQUIRE(a.idx_to_pos(7) == P(1, 2));
        REQUIRE(a.pos_to_idx(1, 2) == 7);

        REQUIRE(a.idx_to_pos(10) == P(2, 0));
        REQUIRE(a.pos_to_idx(2, 0) == 10);

        REQUIRE(a.idx_to_pos(14) == P(2, 4));
        REQUIRE(a.pos_to_idx(2, 4) == 14);
}

// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "catch.hpp"
#include "array2.hpp"
#include "pos.hpp"

TEST_CASE("Dynamic 2d array")
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

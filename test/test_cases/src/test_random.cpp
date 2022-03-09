// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "catch.hpp"

#include "random.hpp"

TEST_CASE("Range")
{
        // Check in range
        REQUIRE(Range(3, 7).is_in_range(5));
        REQUIRE(Range(3, 7).is_in_range(3));
        REQUIRE(Range(3, 7).is_in_range(7));
        REQUIRE(Range(-5, 12).is_in_range(10));
        REQUIRE(Range(-5, 12).is_in_range(-5));
        REQUIRE(Range(-5, 12).is_in_range(12));

        REQUIRE(Range(-1, 1).is_in_range(0));
        REQUIRE(Range(-1, 1).is_in_range(-1));
        REQUIRE(Range(-1, 1).is_in_range(1));

        REQUIRE(Range(5, 5).is_in_range(5));

        // Check NOT in range
        REQUIRE(!Range(3, 7).is_in_range(2));
        REQUIRE(!Range(3, 7).is_in_range(8));
        REQUIRE(!Range(3, 7).is_in_range(-1));

        REQUIRE(!Range(-5, 12).is_in_range(-9));
        REQUIRE(!Range(-5, 12).is_in_range(13));

        REQUIRE(!Range(1, 2).is_in_range(0));

        REQUIRE(!Range(5, 5).is_in_range(4));
        REQUIRE(!Range(5, 5).is_in_range(6));
}

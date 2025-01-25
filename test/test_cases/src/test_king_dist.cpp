// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "catch.hpp"

#include "misc.hpp"
#include "pos.hpp"

TEST_CASE("King distance")
{
        REQUIRE(king_dist(P(1, 2), P(2, 3)) == 1);
        REQUIRE(king_dist(P(1, 2), P(2, 4)) == 2);
        REQUIRE(king_dist(P(1, 2), P(1, 2)) == 0);
        REQUIRE(king_dist(P(10, 3), P(1, 4)) == 9);
}

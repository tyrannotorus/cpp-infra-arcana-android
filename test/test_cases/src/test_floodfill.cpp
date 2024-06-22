// =============================================================================
// Copyright 2011-2024 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include <climits>

#include "array2.hpp"
#include "catch.hpp"
#include "flood.hpp"
#include "pos.hpp"

TEST_CASE("Floodfill")
{
        const P map_dims(30, 20);

        Array2<bool> blocked(map_dims);

        // Set the edge of the map as blocking
        for (int y = 0; y < map_dims.y; ++y) {
                blocked.at(0, y) = blocked.at(map_dims.x - 1, y) = true;
        }

        for (int x = 0; x < map_dims.x; ++x) {
                blocked.at(x, 0) = blocked.at(x, map_dims.y - 1) = true;
        }

        const bool allow_diagonal = true;

        const auto flood =
                floodfill(
                        P(20, 10),
                        blocked,
                        INT_MAX,
                        P(-1, -1),
                        allow_diagonal);

        REQUIRE(flood.at(20, 10) == 0);
        REQUIRE(flood.at(19, 10) == 1);
        REQUIRE(flood.at(21, 10) == 1);
        REQUIRE(flood.at(20, 11) == 1);
        REQUIRE(flood.at(21, 11) == 1);
        REQUIRE(flood.at(24, 12) == 4);
        REQUIRE(flood.at(24, 14) == 4);
        REQUIRE(flood.at(24, 15) == 5);
        REQUIRE(flood.at(0, 0) == 0);
        REQUIRE(flood.at(map_dims.with_offsets(-1, -1)) == 0);
}

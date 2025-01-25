// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include <memory>

#include "array2.hpp"
#include "catch.hpp"
#include "fov.hpp"
#include "global.hpp"
#include "line_calc.hpp"
#include "pos.hpp"

TEST_CASE("fov")
{
        Array2<bool> blocked(80, 80);
        Array2<bool> dark(blocked.dims());
        Array2<bool> light(blocked.dims());

        FovMap fov_map;
        fov_map.hard_blocked = &blocked;
        fov_map.dark = &dark;
        fov_map.light = &light;

        const int x = 40;
        const int y = 40;

        // Required to precalculate FOV lines
        line_calc::init();

        const auto fov = fov::run(P(x, y), fov_map);

        const int r = g_fov_radi_int;

        SECTION("Not blocked")
        {
                REQUIRE(!fov.at(x, y).is_blocked_hard);
                REQUIRE(!fov.at(x + 1, y).is_blocked_hard);
                REQUIRE(!fov.at(x - 1, y).is_blocked_hard);
                REQUIRE(!fov.at(x, y + 1).is_blocked_hard);
                REQUIRE(!fov.at(x, y - 1).is_blocked_hard);
                REQUIRE(!fov.at(x + 2, y + 2).is_blocked_hard);
                REQUIRE(!fov.at(x - 2, y + 2).is_blocked_hard);
                REQUIRE(!fov.at(x + 2, y - 2).is_blocked_hard);
                REQUIRE(!fov.at(x - 2, y - 2).is_blocked_hard);
                REQUIRE(!fov.at(x + r, y).is_blocked_hard);
                REQUIRE(!fov.at(x - r, y).is_blocked_hard);
                REQUIRE(!fov.at(x, y + r).is_blocked_hard);
                REQUIRE(!fov.at(x, y - r).is_blocked_hard);
        }

        SECTION("Blocked due to outside FOV radius")
        {
                REQUIRE(fov.at(x + r + 1, y).is_blocked_hard);
                REQUIRE(fov.at(x - r - 1, y).is_blocked_hard);
                REQUIRE(fov.at(x, y + r + 1).is_blocked_hard);
                REQUIRE(fov.at(x, y - r - 1).is_blocked_hard);
        }

        SECTION("Blocked in corners of FOV")
        {
                REQUIRE(fov.at(x + r, y - r).is_blocked_hard);
                REQUIRE(fov.at(x - r, y - r).is_blocked_hard);
                REQUIRE(fov.at(x + r, y + r).is_blocked_hard);
                REQUIRE(fov.at(x - r, y + r).is_blocked_hard);

                REQUIRE(fov.at(x + r - 1, y - r + 1).is_blocked_hard);
                REQUIRE(fov.at(x - r + 1, y - r + 1).is_blocked_hard);
                REQUIRE(fov.at(x + r - 1, y + r - 1).is_blocked_hard);
                REQUIRE(fov.at(x - r + 1, y + r - 1).is_blocked_hard);
        }
}

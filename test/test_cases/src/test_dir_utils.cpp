// =============================================================================
// Copyright 2011-2021 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include <string>

#include "catch.hpp"
#include "direction.hpp"
#include "pos.hpp"

using namespace dir_utils;

TEST_CASE("Compass direction name")
{
        const P p(20, 20);

        REQUIRE(compass_dir_name(p, p.with_x_offset(1)) == "E");
        REQUIRE(compass_dir_name(p, p.with_offsets(1, 1)) == "SE");
        REQUIRE(compass_dir_name(p, p.with_y_offset(1)) == "S");
        REQUIRE(compass_dir_name(p, p.with_offsets(-1, 1)) == "SW");
        REQUIRE(compass_dir_name(p, p.with_x_offset(-1)) == "W");
        REQUIRE(compass_dir_name(p, p.with_offsets(-1, -1)) == "NW");
        REQUIRE(compass_dir_name(p, p.with_y_offset(-1)) == "N");
        REQUIRE(compass_dir_name(p, p.with_offsets(1, -1)) == "NE");
        REQUIRE(compass_dir_name(p, p.with_offsets(3, 1)) == "E");
        REQUIRE(compass_dir_name(p, p.with_offsets(2, 3)) == "SE");
        REQUIRE(compass_dir_name(p, p.with_offsets(1, 3)) == "S");
        REQUIRE(compass_dir_name(p, p.with_offsets(-3, 2)) == "SW");
        REQUIRE(compass_dir_name(p, p.with_offsets(-3, 1)) == "W");
        REQUIRE(compass_dir_name(p, p.with_offsets(-3, -2)) == "NW");
        REQUIRE(compass_dir_name(p, p.with_offsets(1, -3)) == "N");
        REQUIRE(compass_dir_name(p, p.with_offsets(3, -2)) == "NE");
        REQUIRE(compass_dir_name(p, p.with_x_offset(10000)) == "E");
        REQUIRE(compass_dir_name(p, p.with_offsets(10000, 10000)) == "SE");
        REQUIRE(compass_dir_name(p, p.with_y_offset(10000)) == "S");
        REQUIRE(compass_dir_name(p, p.with_offsets(-10000, 10000)) == "SW");
        REQUIRE(compass_dir_name(p, p.with_x_offset(-10000)) == "W");
        REQUIRE(compass_dir_name(p, p.with_offsets(-10000, -10000)) == "NW");
        REQUIRE(compass_dir_name(p, p.with_y_offset(-10000)) == "N");
        REQUIRE(compass_dir_name(p, p.with_offsets(10000, -10000)) == "NE");
}

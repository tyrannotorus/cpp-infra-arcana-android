// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include <ext/alloc_traits.h>
#include <vector>

#include "catch.hpp"
#include "global.hpp"
#include "line_calc.hpp"
#include "map.hpp"
#include "pos.hpp"

TEST_CASE("Line calculation")
{
        P origin(0, 0);
        std::vector<P> line;

        const bool should_stop_at_target = true;
        const bool is_allowed_outside_map = true;

        line = line_calc::calc_new_line(
                origin,
                P(3, 0),
                should_stop_at_target,
                999,
                is_allowed_outside_map);

        REQUIRE(line.size() == 4);
        REQUIRE(line[0] == origin);
        REQUIRE(line[1] == P(1, 0));
        REQUIRE(line[2] == P(2, 0));
        REQUIRE(line[3] == P(3, 0));

        line = line_calc::calc_new_line(
                origin,
                P(-3, 0),
                should_stop_at_target,
                999,
                is_allowed_outside_map);

        REQUIRE(line.size() == 4);
        REQUIRE(line[0] == origin);
        REQUIRE(line[1] == P(-1, 0));
        REQUIRE(line[2] == P(-2, 0));
        REQUIRE(line[3] == P(-3, 0));

        line = line_calc::calc_new_line(
                origin,
                P(0, 3),
                should_stop_at_target,
                999,
                is_allowed_outside_map);

        REQUIRE(line.size() == 4);
        REQUIRE(line[0] == origin);
        REQUIRE(line[1] == P(0, 1));
        REQUIRE(line[2] == P(0, 2));
        REQUIRE(line[3] == P(0, 3));

        line = line_calc::calc_new_line(
                origin,
                P(0, -3),
                should_stop_at_target,
                999,
                is_allowed_outside_map);

        REQUIRE(line.size() == 4);
        REQUIRE(line[0] == origin);
        REQUIRE(line[1] == P(0, -1));
        REQUIRE(line[2] == P(0, -2));
        REQUIRE(line[3] == P(0, -3));

        line = line_calc::calc_new_line(
                origin,
                P(3, 3),
                should_stop_at_target,
                999,
                is_allowed_outside_map);

        REQUIRE(line.size() == 4);
        REQUIRE(line[0] == origin);
        REQUIRE(line[1] == P(1, 1));
        REQUIRE(line[2] == P(2, 2));
        REQUIRE(line[3] == P(3, 3));

        line = line_calc::calc_new_line(
                P(9, 9),
                P(6, 12),
                should_stop_at_target,
                999,
                is_allowed_outside_map);

        REQUIRE(line.size() == 4);
        REQUIRE(line[0] == P(9, 9));
        REQUIRE(line[1] == P(8, 10));
        REQUIRE(line[2] == P(7, 11));
        REQUIRE(line[3] == P(6, 12));

        line = line_calc::calc_new_line(
                origin,
                P(-3, 3),
                should_stop_at_target,
                999,
                is_allowed_outside_map);

        REQUIRE(line.size() == 4);
        REQUIRE(line[0] == origin);
        REQUIRE(line[1] == P(-1, 1));
        REQUIRE(line[2] == P(-2, 2));
        REQUIRE(line[3] == P(-3, 3));

        line = line_calc::calc_new_line(
                origin,
                P(3, -3),
                should_stop_at_target,
                999,
                is_allowed_outside_map);

        REQUIRE(line.size() == 4);
        REQUIRE(line[0] == origin);
        REQUIRE(line[1] == P(1, -1));
        REQUIRE(line[2] == P(2, -2));
        REQUIRE(line[3] == P(3, -3));

        line = line_calc::calc_new_line(
                origin,
                P(-3, -3),
                should_stop_at_target,
                999,
                is_allowed_outside_map);

        REQUIRE(line.size() == 4);
        REQUIRE(line[0] == origin);
        REQUIRE(line[1] == P(-1, -1));
        REQUIRE(line[2] == P(-2, -2));
        REQUIRE(line[3] == P(-3, -3));

        // Test travel limit parameter
        line = line_calc::calc_new_line(
                origin,
                P(20, 0),
                should_stop_at_target,
                2,
                is_allowed_outside_map);

        REQUIRE(line.size() == 3);
        REQUIRE(line[0] == origin);
        REQUIRE(line[1] == P(1, 0));
        REQUIRE(line[2] == P(2, 0));
}

TEST_CASE("Line calculation - not allowed outside map")
{
        P origin(0, 0);
        std::vector<P> line;

        const bool should_stop_at_target = true;
        const bool is_allowed_outside_map = false;

        line_calc::init();

        map::init();

        map::reset(P(10, 10));

        // Test disallowing outside map
        line = line_calc::calc_new_line(
                P(1, 0),
                P(-9, 0),
                should_stop_at_target,
                999,
                is_allowed_outside_map);

        REQUIRE(line.size() == 2);
        REQUIRE(line[0] == P(1, 0));
        REQUIRE(line[1] == P(0, 0));
}

TEST_CASE("Line calculation - limit travel distance")
{
        P origin(0, 0);
        std::vector<P> line;

        const bool should_stop_at_target = true;
        const bool is_allowed_outside_map = true;

        line = line_calc::calc_new_line(
                origin,
                P(20, 0),
                should_stop_at_target,
                2,
                is_allowed_outside_map);

        REQUIRE(line.size() == 3);
        REQUIRE(line[0] == origin);
        REQUIRE(line[1] == P(1, 0));
        REQUIRE(line[2] == P(2, 0));
}

TEST_CASE("Get pre-calculated lines")
{
        P origin(0, 0);
        std::vector<P> line;

        const std::vector<P>* delta_line;

        line_calc::init();

        delta_line =
                line_calc::fov_delta_line(
                        P(3, 3),
                        g_fov_radi_db);

        REQUIRE(delta_line->size() == 4);
        REQUIRE(delta_line->at(0) == P(0, 0));
        REQUIRE(delta_line->at(1) == P(1, 1));
        REQUIRE(delta_line->at(2) == P(2, 2));
        REQUIRE(delta_line->at(3) == P(3, 3));

        delta_line =
                line_calc::fov_delta_line(
                        P(-3, 3),
                        g_fov_radi_db);

        REQUIRE(delta_line->size() == 4);
        REQUIRE(delta_line->at(0) == P(0, 0));
        REQUIRE(delta_line->at(1) == P(-1, 1));
        REQUIRE(delta_line->at(2) == P(-2, 2));
        REQUIRE(delta_line->at(3) == P(-3, 3));

        delta_line =
                line_calc::fov_delta_line(
                        P(3, -3),
                        g_fov_radi_db);

        REQUIRE(delta_line->size() == 4);
        REQUIRE(delta_line->at(0) == P(0, 0));
        REQUIRE(delta_line->at(1) == P(1, -1));
        REQUIRE(delta_line->at(2) == P(2, -2));
        REQUIRE(delta_line->at(3) == P(3, -3));

        delta_line =
                line_calc::fov_delta_line(
                        P(-3, -3),
                        g_fov_radi_db);

        REQUIRE(delta_line->size() == 4);
        REQUIRE(delta_line->at(0) == P(0, 0));
        REQUIRE(delta_line->at(1) == P(-1, -1));
        REQUIRE(delta_line->at(2) == P(-2, -2));
        REQUIRE(delta_line->at(3) == P(-3, -3));

        // Check constraints for retrieving FOV offset lines
        // Delta > parameter max distance
        delta_line =
                line_calc::fov_delta_line(
                        P(3, 0),
                        2);

        REQUIRE(!delta_line);

        // Delta > limit of precalculated
        delta_line =
                line_calc::fov_delta_line(
                        P(50, 0),
                        999);

        REQUIRE(!delta_line);
}

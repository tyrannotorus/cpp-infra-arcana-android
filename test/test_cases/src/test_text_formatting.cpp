// =============================================================================
// Copyright 2011-2021 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include <string>
#include <vector>

#include "catch.hpp"
#include "text_format.hpp"

TEST_CASE("Text formatting")
{
        std::string str = "one two three four";

        std::vector<std::string> lines;

        lines = text_format::split(str, 100);
        REQUIRE(lines[0] == str);
        REQUIRE(lines.size() == 1);

        lines = text_format::split(str, 18);
        REQUIRE("one two three four" == lines[0]);
        REQUIRE(1 == (int)lines.size());

        lines = text_format::split(str, 17);
        REQUIRE("one two three" == lines[0]);
        REQUIRE("four" == lines[1]);
        REQUIRE(2 == (int)lines.size());

        lines = text_format::split(str, 15);
        REQUIRE("one two three" == lines[0]);
        REQUIRE("four" == lines[1]);
        REQUIRE(2 == (int)lines.size());

        lines = text_format::split(str, 11);
        REQUIRE("one two" == lines[0]);
        REQUIRE("three four" == lines[1]);
        REQUIRE(2 == (int)lines.size());

        str = "123456";
        lines = text_format::split(str, 4);
        REQUIRE("123456" == lines[0]);
        REQUIRE(1 == (int)lines.size());

        str = "12 345678";
        lines = text_format::split(str, 4);
        REQUIRE("12" == lines[0]);
        REQUIRE("345678" == lines[1]);
        REQUIRE(2 == (int)lines.size());

        str = "";
        lines = text_format::split(str, 4);
        REQUIRE(lines.empty());
}

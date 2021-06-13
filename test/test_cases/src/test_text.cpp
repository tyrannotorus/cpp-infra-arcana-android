// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include <ext/alloc_traits.h>
#include <string>
#include <vector>

#include "catch.hpp"
#include "text.hpp"

TEST_CASE("Text with color codes, no split")
{
        std::string str = "aaa {light_magenta}bbbbb{color_reset} cc";

        Text text(str);

        text.set_color(colors::white());

        const auto actions = text.actions();

        size_t idx = 0;

        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == "aaa");

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == " ");

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::change_color);
        REQUIRE(actions[idx].color == colors::light_magenta());

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == "bbbbb");

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::change_color);
        REQUIRE(actions[idx].color == colors::white());

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == " ");

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == "cc");
}

TEST_CASE("Split before space")
{
        std::string str = "aaa {light_magenta}bbbbb{color_reset} cc";

        Text text(str);

        text.set_w(9);
        text.set_color(colors::white());

        const auto actions = text.actions();

        size_t idx = 0;

        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == "aaa");

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == " ");

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::change_color);
        REQUIRE(actions[idx].color == colors::light_magenta());

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == "bbbbb");

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::change_color);
        REQUIRE(actions[idx].color == colors::white());

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::newline);

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == "cc");
}

TEST_CASE("Split after space")
{
        std::string str = "aaa {light_magenta}bbbbb{color_reset} cc";

        Text text(str);

        text.set_w(10);
        text.set_color(colors::white());

        const auto actions = text.actions();

        size_t idx = 0;

        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == "aaa");

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == " ");

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::change_color);
        REQUIRE(actions[idx].color == colors::light_magenta());

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == "bbbbb");

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::change_color);
        REQUIRE(actions[idx].color == colors::white());

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::newline);

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == "cc");
}

TEST_CASE("Split before non-breaking space")
{
        std::string str = "aaa {light_magenta}bbbbb{color_reset}{_}cc";

        Text text(str);

        text.set_w(9);
        text.set_color(colors::white());

        const auto actions = text.actions();

        size_t idx = 0;

        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == "aaa");

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == " ");

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::change_color);
        REQUIRE(actions[idx].color == colors::light_magenta());

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::newline);

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == "bbbbb");

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::change_color);
        REQUIRE(actions[idx].color == colors::white());

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == " ");

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == "cc");
}

TEST_CASE("Split after non-breaking space")
{
        std::string str = "aaa {light_magenta}bbbbb{color_reset}{_}cc";

        Text text(str);

        text.set_w(10);
        text.set_color(colors::white());

        const auto actions = text.actions();

        size_t idx = 0;

        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == "aaa");

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == " ");

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::change_color);
        REQUIRE(actions[idx].color == colors::light_magenta());

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::newline);

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == "bbbbb");

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::change_color);
        REQUIRE(actions[idx].color == colors::white());

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == " ");

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == "cc");
}

TEST_CASE("Text with color codes, split before dot")
{
        std::string str = "aaa {light_magenta}bbbbb{color_reset}. cc";

        Text text(str);

        text.set_w(9);
        text.set_color(colors::white());

        const auto actions = text.actions();

        size_t idx = 0;

        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == "aaa");

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == " ");

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::change_color);
        REQUIRE(actions[idx].color == colors::light_magenta());

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::newline);

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == "bbbbb");

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::change_color);
        REQUIRE(actions[idx].color == colors::white());

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == ".");

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == " ");

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == "cc");
}

TEST_CASE("Text with newlines")
{
        std::string str = "aaa bbbbb cc\n\nddddd eeee";

        Text text(str);

        text.set_w(14);
        text.set_color(colors::white());

        const auto actions = text.actions();

        size_t idx = 0;

        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == "aaa");

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == " ");

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == "bbbbb");

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == " ");

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == "cc");

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::newline);

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::newline);

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == "ddddd");

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == " ");

        ++idx;
        REQUIRE(actions[idx].id == TextActionId::write_str);
        REQUIRE(actions[idx].str == "eeee");
}

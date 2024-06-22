// =============================================================================
// Copyright 2011-2023 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "messages.hpp"

#include <fstream>
#include <iostream>
#include <ostream>
#include <string>
#include <vector>

#include "debug.hpp"
#include "paths.hpp"
#include "random.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static std::vector<std::string> s_menu_quotes;

static std::vector<std::string> s_terrain_inscription_messages_generic;
static std::vector<std::string> s_terrain_inscription_messages_reveal_knowledge;

static std::vector<std::string> read_msg_file(const std::string& filename)
{
        TRACE << "Reading message file at: '" << filename << "'" << std::endl;

        std::ifstream msg_file(filename.c_str());

        if (!msg_file) {
                TRACE_ERROR_RELEASE
                        << "Unable to load message file: "
                        << filename
                        << std::endl;

                PANIC;

                return {};
        }

        std::string str;
        std::vector<std::string> lines;

        while (std::getline(msg_file, str)) {
                if (!str.empty() && (str[0] != ' ') && (str[0] != '#')) {
                        TRACE << str << std::endl;

                        lines.push_back(str);
                }
        }

        msg_file.close();

        return lines;
}

// -----------------------------------------------------------------------------
// messages
// -----------------------------------------------------------------------------
namespace messages
{
void init()
{
        const std::string dir = paths::messages_dir();

        s_menu_quotes =
                read_msg_file(dir + "menu_quotes.txt");

        s_terrain_inscription_messages_generic =
                read_msg_file(dir + "terrain_inscription_messages_generic.txt");

        s_terrain_inscription_messages_reveal_knowledge =
                read_msg_file(dir + "terrain_inscription_messages_reveal_knowledge.txt");
}

std::string get_random_menu_quote()
{
        return rnd::element(s_menu_quotes);
}

std::string get_random_terrain_inscription_msg_generic()
{
        return rnd::element(s_terrain_inscription_messages_generic);
}

std::string get_random_terrain_inscription_msg_reveal_knowledge()
{
        return rnd::element(s_terrain_inscription_messages_reveal_knowledge);
}

}  // namespace messages

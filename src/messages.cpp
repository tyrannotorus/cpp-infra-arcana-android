// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
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
static std::vector<std::string> menu_quotes;

static std::vector<std::string> read_message_file(const std::string& filename)
{
        std::ifstream message_file(filename.c_str());

        if (!message_file) {
                TRACE_ERROR_RELEASE
                        << "Unable to load message file: "
                        << filename
                        << std::endl;

                PANIC;

                return {};
        }

        std::string str;
        std::vector<std::string> lines;

        while (std::getline(message_file, str)) {
                if (!str.empty()) {
                        lines.push_back(str);
                }
        }

        message_file.close();

        return lines;
}

// -----------------------------------------------------------------------------
// messages
// -----------------------------------------------------------------------------
namespace messages
{
void init()
{
        menu_quotes =
                read_message_file(
                        paths::messages_dir() +
                        "menu_quotes.txt");
}

std::string get_random_menu_quote()
{
        return rnd::element(menu_quotes);
}

}  // namespace messages

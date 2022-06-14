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

static std::vector<std::string> read_message_file(std::string file_name)
{
        std::vector<std::string> out;
        std::ifstream message_file(file_name.c_str());
        if (!message_file) {
                TRACE_ERROR_RELEASE
                    << "Unable to load message file: "
                    << file_name
                    << std::endl;
                PANIC;
                return out;
        }
        std::string str;
        while (std::getline(message_file, str)) {
                if (!str.empty()) {
                        out.push_back(str);
                }
        }
        message_file.close();
        return out;
}

// -----------------------------------------------------------------------------
// messages
// -----------------------------------------------------------------------------
namespace messages
{
void init()
{
        menu_quotes = read_message_file(paths::messages_dir() + "menu_quotes.txt");
}

std::string get_random_menu_quote()
{
        return rnd::element(menu_quotes);
}

}  // namespace messages

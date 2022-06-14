// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef MESSAGES_HPP
#define MESSAGES_HPP

#include <string>

namespace messages
{

void init();

// Get random main menu quote
std::string get_random_menu_quote();

}  // namespace messages

#endif  // MESSAGES_HPP
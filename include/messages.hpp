// =============================================================================
// Copyright 2011-2024 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef MESSAGES_HPP
#define MESSAGES_HPP

#include <string>

namespace messages
{
void init();

// Get random main menu quote.
std::string get_random_menu_quote();

// For things like inscribed pillars that can be studied.
std::string get_random_terrain_inscription_msg_generic();
std::string get_random_terrain_inscription_msg_reveal_knowledge();

}  // namespace messages

#endif  // MESSAGES_HPP

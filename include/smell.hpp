// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef SMELL_HPP
#define SMELL_HPP

#include <string>

namespace actor
{
class Actor;
}  // namespace actor

namespace smell
{
struct Smell
{
        // Pointer to a message stored elsewhere (typically in monster data)
        const std::string* msg_ptr {nullptr};

        int strength_pct {0};
};

void init();

void save();

void load();

void on_std_turn();

void put_smell_for_mon(const actor::Actor& mon);

void on_player_turn_start();

}  // namespace smell

#endif  // SMELL_HPP

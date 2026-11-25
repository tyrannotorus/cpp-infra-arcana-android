// =============================================================================
// Copyright Werewolf Camp
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef RUN_MODE_HPP
#define RUN_MODE_HPP

#include <string>

enum class Dir;

// -----------------------------------------------------------------------------
// The action bar's run / walk toggle. While running, every move is an auto
// move (shift + direction on a keyboard): the player keeps going until the
// auto move rules stop them.
// -----------------------------------------------------------------------------
namespace run_mode
{
bool is_running();

// Starting to run is refused, with the reason in the log, while an auto
// move would be
void toggle();

// Why an auto move is refused right now - empty if it is allowed
std::string refusal_reason();

// Runs in the direction, if running. Returns false if the move is to be
// walked instead - running is off, or switched off right here because it
// is refused (with the reason in the log).
bool try_run(Dir dir);

// Call at the start of the player's turn, before it is drawn: running
// switches off, with the reason in the log, as soon as it would be refused
void on_player_turn();

}  // namespace run_mode

#endif  // RUN_MODE_HPP

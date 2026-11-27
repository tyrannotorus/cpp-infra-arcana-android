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
// move (as shift + direction).
// -----------------------------------------------------------------------------
namespace run_mode
{
bool is_running();

// Refuses to start running (reason logged) while an auto move would be
void toggle();

// Why an auto move is refused right now - empty if it is allowed
std::string refusal_reason();

// Auto moves in the direction if running. Returns false if the move is to
// be walked: not running, or running just stopped as refused (reason logged).
bool try_run(Dir dir);

// Call at the start of the player's turn, before drawing: stops running
// (reason logged) once it would be refused
void on_player_turn();

}  // namespace run_mode

#endif  // RUN_MODE_HPP

// =============================================================================
// Copyright Werewolf Camp
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef DEATH_CINEMATIC_HPP
#define DEATH_CINEMATIC_HPP

namespace death_cinematic
{
// Zooms slowly in on the player as the map drains to red, then fades to
// black. Only the map reddens - the interface keeps its colors. Blocks
// until it has played out, so its pacing never depends on when the player
// taps. Call reset once the game state is gone.
void run();

void reset();

}  // namespace death_cinematic

#endif  // DEATH_CINEMATIC_HPP

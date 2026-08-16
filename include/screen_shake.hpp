// =============================================================================
// Copyright Werewolf Camp
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef SCREEN_SHAKE_HPP
#define SCREEN_SHAKE_HPP

#include <cstdint>

// What each event does to the screen, over io::start_map_shake. Amplitudes
// and durations live here, not at the call sites, so the shakes stay in
// proportion. They differ only in those two - every shake rattles both
// axes, and none is ever directional (a directional throw travels the way a
// camera step does, and reads as the view sliding).
namespace screen_shake
{
// A weapon hit, a kick, bashing a door or statue - short and light
void on_player_blow();

// Longer and heavier, scaled by the fraction of max hp lost
void on_player_damaged(int dmg, int max_hp);

// The hardest jolt in the game, lasting the blast animation
void on_explosion(uint32_t duration_ms);

}  // namespace screen_shake

#endif  // SCREEN_SHAKE_HPP

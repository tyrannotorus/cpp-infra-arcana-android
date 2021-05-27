// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef AUDIO_HPP
#define AUDIO_HPP

#include "audio_data.hpp"
#include "direction.hpp"

namespace audio
{
inline constexpr int g_allocated_channels = 16;

void init();
void cleanup();

void play(SfxId sfx, int vol_pct_tot = 100, int vol_pct_l = 50);

void play(SfxId sfx, Dir dir, int distance_pct);

void try_play_amb(int one_in_n_chance_to_play);

// Plays music if not already playing any music
void play_music(MusId mus);

void fade_out_music();

void set_music_volume(int volume_pct);

}  // namespace audio

#endif  // AUDIO_HPP

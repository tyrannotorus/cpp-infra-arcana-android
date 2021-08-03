// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "audio.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <ostream>
#include <ratio>
#include <string>
#include <type_traits>
#include <vector>

#include "SDL_events.h"
#include "SDL_mixer.h"
#include "SDL_timer.h"
#include "actor_player.hpp"
#include "audio_data.hpp"
#include "config.hpp"
#include "debug.hpp"
#include "global.hpp"
#include "map.hpp"
#include "paths.hpp"
#include "property_data.hpp"
#include "property_handler.hpp"
#include "random.hpp"
#include "text_format.hpp"

using namespace std::chrono_literals;

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static_assert(
        std::is_integral<std::chrono::system_clock::rep>::value,
        "Representation of ticks isn't an integral value.");

static std::vector<Mix_Chunk*> s_audio_chunks;
static std::vector<Mix_Music*> s_mus_chunks;

// TODO: Also use std::chrono for sound effects?
static uint32_t s_ms_at_sfx_played[(size_t)audio::SfxId::END];
static int s_current_channel = 0;
static const auto s_min_seconds_between_amb = 20s;
static auto s_seconds_at_amb_played = 0s;
static int s_nr_files_loaded = 0;

static void load(const audio::SfxId sfx, const std::string& filename)
{
        // Sound already loaded?
        if (s_audio_chunks[(size_t)sfx])
        {
                return;
        }

        // Read events, so that we don't freeze the game while we loading sounds
        SDL_PumpEvents();

        const std::string file_rel_path = paths::audio_dir() + filename;

        TRACE << "Loading audio file: " << file_rel_path << std::endl;

        s_audio_chunks[(size_t)sfx] = Mix_LoadWAV(file_rel_path.c_str());

        if (!s_audio_chunks[(size_t)sfx])
        {
                TRACE
                        << "Problem loading audio file with name: "
                        << filename << std::endl
                        << "Mix_GetError(): "
                        << Mix_GetError() << std::endl;

                ASSERT(false);
        }

        ++s_nr_files_loaded;
}

static int next_channel(const int from)
{
        ASSERT(from >= 0 && from < audio::g_allocated_channels);

        int ret = from + 1;

        if (ret == audio::g_allocated_channels)
        {
                ret = 0;
        }

        return ret;
}

static int find_free_channel(const int from)
{
        ASSERT(from >= 0 && from < audio::g_allocated_channels);

        int ret = from;

        for (int i = 0; i < audio::g_allocated_channels; ++i)
        {
                ret = next_channel(ret);

                if (Mix_Playing(ret) == 0)
                {
                        return ret;
                }
        }

        // Failed to find free channel
        return -1;
}

static std::string amb_sfx_filename(const audio::SfxId sfx)
{
        const int amb_nr = (int)sfx - (int)audio::SfxId::AMB_START;

        const std::string padding_str =
                (amb_nr < 10) ? "00" : (amb_nr < 100) ? "0"
                                                      : "";

        const std::string idx_str = std::to_string(amb_nr);

        return "amb_" + padding_str + idx_str + ".ogg";
}

// -----------------------------------------------------------------------------
// audio
// -----------------------------------------------------------------------------
namespace audio
{
void init()
{
        TRACE_FUNC_BEGIN;

        cleanup();

        if (config::master_volume_pct() == 0)
        {
                TRACE_FUNC_END;

                return;
        }

        s_audio_chunks.resize((size_t)SfxId::END);

        for (size_t i = 0; i < s_audio_chunks.size(); ++i)
        {
                s_audio_chunks[i] = nullptr;
        }

        // Pre-load the action sounds

        for (int i = 0; i < (int)SfxId::AMB_START; ++i)
        {
                const auto id = (SfxId)i;

                const std::string id_str = sfx_id_to_str(id);

                if (id_str.empty())
                {
                        TRACE
                                << "Could not find an id string for audio "
                                   "with id number: "
                                << (int)id
                                << std::endl;

                        ASSERT(false);

                        continue;
                }

                const auto filename = "sfx_" + id_str + ".ogg";

                load(id, filename);
        }

        ASSERT(s_nr_files_loaded == (int)SfxId::AMB_START);

        if (config::is_amb_audio_preloaded() &&
            config::is_amb_audio_enabled())
        {
                for (auto i = (int)SfxId::AMB_START + 1;
                     i < (int)SfxId::END;
                     ++i)
                {
                        const int amb_nr = i - (int)SfxId::AMB_START;

                        auto amb_nr_str = std::to_string(amb_nr);

                        amb_nr_str =
                                text_format::pad_before(
                                        amb_nr_str,
                                        3,  // Total width
                                        '0');

                        const std::string amb_name =
                                "amb_" + amb_nr_str + ".ogg";

                        load((SfxId)i, amb_name);
                }
        }

        // Load music
        s_mus_chunks.resize((size_t)MusId::END);

        const std::string music_path =
                paths::audio_dir() +
                "musica_cthulhiana_fragment_madness.ogg";

        s_mus_chunks[(size_t)MusId::cthulhiana_madness] =
                Mix_LoadMUS(music_path.c_str());

        set_music_volume(config::master_volume_pct());

        TRACE_FUNC_END;
}

void cleanup()
{
        TRACE_FUNC_BEGIN;

        for (size_t i = 0; i < (size_t)SfxId::END; ++i)
        {
                s_ms_at_sfx_played[i] = 0;
        }

        for (Mix_Chunk* chunk : s_audio_chunks)
        {
                Mix_FreeChunk(chunk);
        }

        s_audio_chunks.clear();

        for (Mix_Music* chunk : s_mus_chunks)
        {
                Mix_FreeMusic(chunk);
        }

        s_mus_chunks.clear();

        s_current_channel = 0;
        s_seconds_at_amb_played = 0s;

        s_nr_files_loaded = 0;

        TRACE_FUNC_END;
}

void play(const SfxId sfx, int vol_pct_tot, const int vol_pct_l)
{
        // TODO: Ugly hack (although this is probably more robust than for
        // example disabling the audio when deaf is applied, and enabling it
        // when deafness ends, or when the gameplay session ends, etc(?))
        if (map::g_player && map::g_player->m_properties.has(PropId::deaf))
        {
                return;
        }

        if (s_audio_chunks.empty() ||
            (sfx == SfxId::AMB_START) ||
            (sfx == SfxId::END))
        {
                return;
        }

        // Adjust for master volume
        vol_pct_tot = (vol_pct_tot * config::master_volume_pct()) / 100;

        // Is this an ambient sound which has not yet been loaded?
        if (((int)sfx > (int)SfxId::AMB_START) &&
            !s_audio_chunks[(size_t)sfx])
        {
                load(sfx, amb_sfx_filename(sfx));
        }

        const int free_channel = find_free_channel(s_current_channel);

        const auto ms_now = SDL_GetTicks();

        auto& ms_last = s_ms_at_sfx_played[(size_t)sfx];

        const auto ms_diff = ms_now - ms_last;

        if ((free_channel >= 0) &&
            (ms_diff >= g_min_ms_between_same_sfx))
        {
                s_current_channel = free_channel;

                const int vol_tot = (255 * vol_pct_tot) / 100;
                const int vol_l = (vol_pct_l * vol_tot) / 100;
                const int vol_r = vol_tot - vol_l;

                Mix_SetPanning(
                        s_current_channel,
                        vol_l,
                        vol_r);

                Mix_PlayChannel(
                        s_current_channel,
                        s_audio_chunks[(size_t)sfx],
                        0);

                ms_last = SDL_GetTicks();
        }
}

void play(const SfxId sfx, const Dir dir, const int distance_pct)
{
        if (dir == Dir::END)
        {
                return;
        }

        // The distance is scaled down to avoid too much volume reduction
        const int vol_pct_tot = 100 - ((distance_pct * 2) / 3);

        int vol_pct_l = 0;

        switch (dir)
        {
        case Dir::left:
                vol_pct_l = 85;
                break;

        case Dir::up_left:
                vol_pct_l = 75;
                break;

        case Dir::down_left:
                vol_pct_l = 75;
                break;

        case Dir::up:
                vol_pct_l = 50;
                break;

        case Dir::center:
                vol_pct_l = 50;
                break;

        case Dir::down:
                vol_pct_l = 50;
                break;

        case Dir::up_right:
                vol_pct_l = 25;
                break;

        case Dir::down_right:
                vol_pct_l = 25;
                break;

        case Dir::right:
                vol_pct_l = 15;
                break;

        case Dir::END:
                vol_pct_l = 50;
                break;
        }

        play(sfx, vol_pct_tot, vol_pct_l);
}

void try_play_amb(const int one_in_n_chance_to_play)
{
        if (!config::is_amb_audio_enabled() ||
            s_audio_chunks.empty() ||
            !rnd::one_in(one_in_n_chance_to_play))
        {
                return;
        }

        const auto seconds_now =
                std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::system_clock::now()
                                .time_since_epoch());

        if ((seconds_now - s_seconds_at_amb_played) >
            s_min_seconds_between_amb)
        {
                s_seconds_at_amb_played = seconds_now;

                const int vol_pct = rnd::range(25, 100);
                const int first_int = (int)SfxId::AMB_START + 1;
                const int last_int = (int)SfxId::END - 1;
                const auto sfx = (SfxId)rnd::range(first_int, last_int);

                // NOTE: The ambient sound effect will be loaded by 'play', if
                // not already loaded (only action sound effects are pre-loaded)
                play(sfx, vol_pct);
        }
}

void play_music(const MusId mus)
{
        // NOTE: We do not play if already playing  music
        if (s_mus_chunks.empty() || Mix_PlayingMusic())
        {
                return;
        }

        auto* const chunk = s_mus_chunks[(size_t)mus];

        // Loop forever
        Mix_PlayMusic(chunk, -1);
}

void fade_out_music()
{
        Mix_FadeOutMusic(2000);
}

void set_music_volume(int volume_pct)
{
        Mix_VolumeMusic(volume_pct);
}

}  // namespace audio

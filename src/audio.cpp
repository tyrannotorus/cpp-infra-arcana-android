// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
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
#include "actor.hpp"
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
static const int s_ambient_sfx_channel = 0;
static const int s_first_standard_sfx_channel = s_ambient_sfx_channel + 1;
static const auto s_min_seconds_between_ambient = 20s;
static auto s_seconds_at_ambient_played = 0s;
static int s_nr_files_loaded = 0;

static void load(const audio::SfxId sfx, const std::string& filename)
{
        // Sound already loaded?
        if (s_audio_chunks[(size_t)sfx])
        {
                return;
        }

        // Read events, so that we don't freeze the game while we load sounds
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

static std::string get_audio_str(const audio::SfxId id)
{
        auto id_str = sfx_id_to_str(id);

        if (id_str.empty())
        {
                TRACE
                        << "Could not find an id string for "
                           "audio with id number: "
                        << (int)id
                        << std::endl;

                ASSERT(false);
        }

        return id_str;
}

static void load_audio_id(
        const audio::SfxId id,
        const std::string& filename_prefix = "")
{
        const std::string id_str = get_audio_str(id);

        if (id_str.empty())
        {
                return;
        }

        const auto filename = filename_prefix + id_str + ".ogg";

        load(id, filename);
}

static void load_game_sound_effects()
{
        for (int i = 0; i < (int)audio::SfxId::AMB_START; ++i)
        {
                const auto id = (audio::SfxId)i;

                load_audio_id(id, "sfx_");
        }
}

static void load_ambient_sounds()
{
        for (auto i = (int)audio::SfxId::AMB_START + 1;
             i < (int)audio::SfxId::END;
             ++i)
        {
                const auto id = (audio::SfxId)i;

                load_audio_id(id);
        }
}

static int next_channel(
        const int from,
        const int first_channel,
        const int last_channel)
{
        int channel = from + 1;

        if (channel >= last_channel)
        {
                channel = first_channel;
        }

        return channel;
}

static int find_free_channel(
        const int from,
        const int first_channel,
        const int last_channel)
{
        int channel = from;

        for (int i = first_channel; i <= last_channel; ++i)
        {
                channel = next_channel(channel, first_channel, last_channel);

                if (Mix_Playing(channel) == 0)
                {
                        return channel;
                }
        }

        // Failed to find free channel
        return -1;
}

static bool is_loaded(const audio::SfxId id)
{
        if ((id == audio::SfxId::AMB_START) ||
            (id == audio::SfxId::END))
        {
                return false;
        }

        if (s_audio_chunks.empty())
        {
                return false;
        }

        return s_audio_chunks[(size_t)id];
}

static bool is_ambient_sound(const audio::SfxId id)
{
        return (
                ((int)id > (int)audio::SfxId::AMB_START) &&
                (id != audio::SfxId::END));
}

static int volume_pct_left_for_direction(const Dir dir)
{
        switch (dir)
        {
        case Dir::left:
                return 75;
                break;

        case Dir::up_left:
                return 60;
                break;

        case Dir::down_left:
                return 60;
                break;

        case Dir::up:
                return 50;
                break;

        case Dir::center:
                return 50;
                break;

        case Dir::down:
                return 50;
                break;

        case Dir::up_right:
                return 40;
                break;

        case Dir::down_right:
                return 40;
                break;

        case Dir::right:
                return 25;
                break;

        case Dir::END:
                break;
        }

        return 50;
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

        s_audio_chunks.resize((size_t)SfxId::END);

        for (size_t i = 0; i < s_audio_chunks.size(); ++i)
        {
                s_audio_chunks[i] = nullptr;
        }

        // Pre-load the game sound effects.
        load_game_sound_effects();

        ASSERT(s_nr_files_loaded == (int)SfxId::AMB_START);

        if (config::is_ambient_audio_preloaded() &&
            config::is_ambient_audio_enabled())
        {
                load_ambient_sounds();

                // -1 to adjust for SfxId::AMB_START.
                ASSERT(s_nr_files_loaded == ((int)SfxId::END - 1));
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
        s_seconds_at_ambient_played = 0s;

        s_nr_files_loaded = 0;

        TRACE_FUNC_END;
}

void play(const SfxId sfx, int vol_pct_tot, const int vol_pct_l)
{
        if (config::master_volume_pct() == 0)
        {
                return;
        }

        // TODO: Ugly hack, this low level code should not know about player
        // status effects.
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

        const bool is_amb = is_ambient_sound(sfx);

        if (is_amb && !is_loaded(sfx))
        {
                load_audio_id(sfx);
        }

        int from_channel = 0;
        int first_channel = 0;
        int last_channel = 0;

        if (is_amb)
        {
                from_channel = s_ambient_sfx_channel;
                first_channel = s_ambient_sfx_channel;
                last_channel = s_ambient_sfx_channel;
        }
        else
        {
                from_channel = s_current_channel;
                first_channel = s_first_standard_sfx_channel;
                last_channel = g_allocated_channels - 1;
        }

        const int channel =
                find_free_channel(
                        from_channel,
                        first_channel,
                        last_channel);

        if (channel < 0)
        {
                return;
        }

        const auto ms_now = SDL_GetTicks();
        auto& ms_last = s_ms_at_sfx_played[(size_t)sfx];
        const auto ms_diff = ms_now - ms_last;

        if (ms_diff < g_min_ms_between_same_sfx)
        {
                return;
        }

        if (!is_amb)
        {
                s_current_channel = channel;
        }

        const int vol_tot = (255 * vol_pct_tot) / 100;
        const int vol_l = (vol_tot * vol_pct_l) / 100;
        const int vol_r = vol_tot - vol_l;

        if (!Mix_SetPanning(channel, vol_l, vol_r))
        {
                TRACE
                        << "Failed to set panning, "
                        << "l=" << vol_l << " "
                        << "r=" << vol_r
                        << ": " << Mix_GetError() << std::endl;
        }

        auto* const chunk = s_audio_chunks[(size_t)sfx];

        Mix_PlayChannel(channel, chunk, 0);

        ms_last = SDL_GetTicks();
}

void play_from_direction(
        const SfxId sfx,
        const Dir dir,
        const int distance_pct)
{
        if (config::master_volume_pct() == 0)
        {
                return;
        }

        if (dir == Dir::END)
        {
                return;
        }

        // NOTE: Distance is scaled down to avoid too much volume reduction.
        const int distance_pct_scaled = (distance_pct * 2) / 3;

        const int vol_pct_tot = 100 - distance_pct_scaled;

        const int vol_pct_l = volume_pct_left_for_direction(dir);

        play(sfx, vol_pct_tot, vol_pct_l);
}

void stop_ambient()
{
        Mix_HaltChannel(s_ambient_sfx_channel);
}

void try_play_ambient(const int one_in_n_chance_to_play)
{
        if (config::master_volume_pct() == 0)
        {
                return;
        }

        if (!config::is_ambient_audio_enabled())
        {
                return;
        }

        if (s_audio_chunks.empty())
        {
                // Ambient sounds not loaded.
                return;
        }

        if (!rnd::one_in(one_in_n_chance_to_play))
        {
                // Random chance to play ambient sound failed.
                return;
        }

        const auto seconds_now =
                std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::system_clock::now()
                                .time_since_epoch());

        if ((seconds_now - s_seconds_at_ambient_played) >
            s_min_seconds_between_ambient)
        {
                s_seconds_at_ambient_played = seconds_now;

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
        if (config::master_volume_pct() == 0)
        {
                return;
        }

        if (Mix_PlayingMusic())
        {
                // Already playing music.
                return;
        }

        if (s_mus_chunks.empty())
        {
                // Music not loaded.
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

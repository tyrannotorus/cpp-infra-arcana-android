// =============================================================================
// Copyright Werewolf Camp
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "death_cinematic.hpp"

#include <algorithm>
#include <cmath>

#include "SDL_timer.h"
#include "actor.hpp"
#include "colors.hpp"
#include "config.hpp"
#include "easing.hpp"
#include "io.hpp"
#include "io_display.hpp"
#include "map.hpp"
#include "msg_log.hpp"
#include "panel.hpp"
#include "pos.hpp"
#include "rect.hpp"
#include "state.hpp"
#include "viewport.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
// Tracks on one timeline, deliberately overlapping: the interface leaves
// first, and the map is fully red long before the vignette has closed. The
// vignette runs the zoom's length, so the two always land together.
static const uint32_t s_red_duration_ms = 5000;
static const uint32_t s_zoom_duration_ms = 9000;

// A tap swaps the vignette for this, from the moment it is tapped
static const uint32_t s_skip_fade_ms = 3000;

// The interface leaves at once - it is not part of the shot
static const uint32_t s_ui_exit_ms = 700;

// Roughly 60 frames per second - a full redraw per step
static const uint32_t s_step_delay_ms = 16;

static const float s_zoom_end = 1.7f;

// Slides the stats panel, the movement pad and the action bar off by their
// nearest edge. The log stays - the death message is read from it.
static void set_ui_exit_progress(const float progress)
{
        const R screen_rect = io::panel_logical_px_rect(Panel::screen);

        const int travel_x =
                (int)std::lround((float)screen_rect.w() * progress);

        const int travel_y =
                (int)std::lround((float)screen_rect.h() * progress);

        // The pad sits on the edge opposite the stats panel, so handedness
        // alone decides which way each of them leaves
        const bool is_side_left = config::is_side_panel_left();

        io::set_display_px_offset(
                io::Display::side,
                {is_side_left ? -travel_x : travel_x, 0});

        io::set_display_px_offset(
                io::Display::dpad,
                {is_side_left ? travel_x : -travel_x, 0});

        io::set_display_px_offset(io::Display::bar, {0, travel_y});
}

// How far into a track, 0 to 1. Tracks that have not started yet sit at 0,
// finished ones at 1.
static float track_progress(
        const uint32_t elapsed_ms,
        const uint32_t start_ms,
        const uint32_t duration_ms)
{
        if (elapsed_ms <= start_ms) {
                return 0.0f;
        }

        if (duration_ms == 0) {
                return 1.0f;
        }

        return std::min(
                1.0f,
                (float)(elapsed_ms - start_ms) / (float)duration_ms);
}

// Where the player sits in the map panel, in logical pixels
static P player_center_px()
{
        const P view_pos = viewport::to_view_pos(map::g_player->m_pos);

        const int cell_w = config::map_cell_px_w();
        const int cell_h = config::map_cell_px_h();

        return {
                (view_pos.x * cell_w) + (cell_w / 2),
                (view_pos.y * cell_h) + (cell_h / 2)};
}

// The tracks that run whichever way the sequence ends
static void apply_tracks(const uint32_t elapsed_ms)
{
        set_ui_exit_progress(
                ease::out(track_progress(elapsed_ms, 0, s_ui_exit_ms)));

        const float zoom =
                1.0f +
                ((s_zoom_end - 1.0f) *
                 ease::in(track_progress(elapsed_ms, 0, s_zoom_duration_ms)));

        io::set_map_zoom(zoom, player_center_px());

        // Green and blue drain away, leaving the red channel at full
        // strength - so the map reddens rather than darkening
        const auto channel =
                (uint8_t)std::lround(
                        255.0f *
                        (1.0f -
                         track_progress(elapsed_ms, 0, s_red_duration_ms)));

        io::set_map_tint(Color(255, channel, channel));
}

// How the sequence goes dark: the vignette closes on the player over the
// whole zoom, unless a tap has swapped it for a flat fade from wherever it
// had closed to.
static void draw_ending(
        const uint32_t elapsed_ms,
        const uint32_t fade_start_ms)
{
        if (fade_start_ms == 0) {
                io::set_map_vignette(
                        player_center_px(),
                        1.0f -
                                ease::out(
                                        track_progress(
                                                elapsed_ms,
                                                0,
                                                s_zoom_duration_ms)));

                return;
        }

        const float fade_t =
                track_progress(elapsed_ms, fade_start_ms, s_skip_fade_ms);

        io::set_display(io::Display::overlay);

        io::draw_rectangle_filled(
                io::panel_logical_px_rect(Panel::screen),
                colors::black(),
                (uint8_t)std::lround(255.0f * fade_t));
}

// -----------------------------------------------------------------------------
// death_cinematic
// -----------------------------------------------------------------------------
namespace death_cinematic
{
void run()
{
        if (config::is_bot_playing() || !map::g_player) {
                return;
        }

        const uint32_t start_ms = SDL_GetTicks();

        // The death message carries the prompt's hint from the first frame,
        // but is not allowed to wait on it - the tap is read here instead
        msg_log::set_more_prompt_hint_shown(true);

        // Nothing is taking a column any more, so the camera glides the
        // player to dead center as the panels leave
        viewport::set_center_bias_enabled(false);

        // Zero until a tap cuts the sequence short - the vignette carries it
        // to black on its own otherwise
        uint32_t fade_start_ms = 0;

        while (true) {
                const uint32_t elapsed_ms = SDL_GetTicks() - start_ms;

                if ((fade_start_ms == 0) && io::poll_any_input()) {
                        fade_start_ms = std::max(1u, elapsed_ms);
                }

                const bool is_done =
                        (fade_start_ms > 0)
                        ? (elapsed_ms >= (fade_start_ms + s_skip_fade_ms))
                        : (elapsed_ms >= s_zoom_duration_ms);

                apply_tracks(elapsed_ms);

                states::draw();

                draw_ending(elapsed_ms, fade_start_ms);

                io::update_screen();

                if (is_done) {
                        break;
                }

                io::sleep(s_step_delay_ms);
        }

        msg_log::set_more_prompt_hint_shown(false);

        io::clear_input();
}

void reset()
{
        io::set_map_zoom(1.0f, {0, 0});

        io::clear_map_tint();

        io::clear_map_vignette();

        io::reset_display_px_offsets();

        viewport::set_center_bias_enabled(true);
}

}  // namespace death_cinematic

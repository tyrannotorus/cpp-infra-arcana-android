// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "io.hpp"

#include <cstdint>

#include "SDL.h"
#include "SDL_video.h"
#include "config.hpp"
#include "debug.hpp"
#include "io_display.hpp"
#include "io_internal.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
// Window size the panels were last laid out for
static P s_layout_px_dims;

static SDL_Window* create_sdl_window()
{
        TRACE_FUNC_BEGIN;

        // Request only - fullscreen takes the surface size
        SDL_Rect display_bounds {};

        if (SDL_GetDisplayBounds(0, &display_bounds) != 0) {
                TRACE_ERROR_RELEASE
                        << "Failed to read display bounds"
                        << std::endl
                        << SDL_GetError()
                        << std::endl;

                PANIC;
        }

        auto* const window =
                SDL_CreateWindow(
                        "Infra Arcana",
                        SDL_WINDOWPOS_CENTERED,
                        SDL_WINDOWPOS_CENTERED,
                        display_bounds.w,
                        display_bounds.h,
                        SDL_WINDOW_FULLSCREEN_DESKTOP);

        if (!window) {
                TRACE << "Failed to create window: "
                      << std::endl
                      << SDL_GetError()
                      << std::endl;
        }

        TRACE_FUNC_END;

        return window;
}

static P calc_offsets_to_center_rectangle(
        const P& outer_dims,
        const P& inner_dims)
{
        const auto extra_space = outer_dims - inner_dims;

        return extra_space.scaled_down(2);
}

static void update_rendering_offsets()
{
        // NOTE: The window may be scaled up, but the screen panel is never
        // scaled up. Therefore we scale down the window size to compare it with
        // the screen panel, and then scale up the offsets again afterwards.

        const int scale_factor = config::video_scale_factor();

        const auto window_logical_px_dims =
                io::window_px_dims().scaled_down(scale_factor);

        const auto screen_panel_logical_px_dims = io::panel_px_dims(Panel::screen);

        const auto rendering_px_offset =
                calc_offsets_to_center_rectangle(
                        window_logical_px_dims,
                        screen_panel_logical_px_dims);

        io::g_rendering_px_offset = rendering_px_offset.scaled_up(scale_factor);
}

// -----------------------------------------------------------------------------
// io
// -----------------------------------------------------------------------------
namespace io
{
SDL_Window* g_sdl_window = nullptr;
SDL_Renderer* g_sdl_renderer = nullptr;

void init_window()
{
        g_sdl_window = create_sdl_window();

        if (!g_sdl_window) {
                TRACE_ERROR_RELEASE
                        << "Failed to set up window"
                        << std::endl
                        << SDL_GetError()
                        << std::endl;

                PANIC;
        }

        const auto px_dims = window_px_dims();

        TRACE << "Window size: "
              << px_dims.x << "x"
              << px_dims.y
              << std::endl;
}

void layout_window()
{
        // The window can be smaller than the display (cutout, nav bar,
        // multi-window)
        s_layout_px_dims = window_px_dims();

        panels::init(sdl_window_gui_dims());

        update_rendering_offsets();
}

bool on_window_resized()
{
        const auto px_dims = window_px_dims();

        if (px_dims == s_layout_px_dims) {
                return false;
        }

        TRACE << "New window size: "
              << px_dims.x << "x"
              << px_dims.y
              << std::endl;

        layout_window();

        // The display textures are sized to the screen panel
        init_displays();

        states::on_window_resized();

        return true;
}

P sdl_window_gui_dims()
{
        const auto logical_px_dims =
                window_px_dims().scaled_down(
                        config::video_scale_factor());

        return io::px_to_gui_coords(logical_px_dims);
}

P window_px_dims()
{
        P px_dims;

        SDL_GetWindowSize(g_sdl_window, &px_dims.x, &px_dims.y);

        return px_dims;
}

#ifndef NDEBUG
bool g_allow_render = false;
#endif  // NDEBUG

// How long the last present spent waiting for vblank (see update_screen)
static uint32_t s_last_present_block_ms = 0;

uint32_t take_last_present_block_ms()
{
        const uint32_t ms = s_last_present_block_ms;

        s_last_present_block_ms = 0;

        return ms;
}

void update_screen()
{
#ifndef NDEBUG
        const bool is_game_state =
                !states::is_empty() &&
                (states::current_state()->id() == StateId::game);

        if (is_game_state) {
                ASSERT(g_allow_render);
        }
#endif  // NDEBUG

        composite_display_textures();

        const uint32_t present_start_ms = SDL_GetTicks();

        SDL_RenderPresent(g_sdl_renderer);

        s_last_present_block_ms = SDL_GetTicks() - present_start_ms;

#ifndef NDEBUG
        if (is_game_state) {
                g_allow_render = false;
        }
#endif  // NDEBUG
}

void clear_screen()
{
#ifndef NDEBUG
        const bool is_game_state =
                !states::is_empty() &&
                (states::current_state()->id() == StateId::game);

        if (is_game_state) {
                // Do not allow rendering after screen is cleared, we do not want to just show a
                // black screen. The map should always be shown.
                g_allow_render = false;
        }
#endif  // NDEBUG

        clear_display_textures();
}

}  // namespace io

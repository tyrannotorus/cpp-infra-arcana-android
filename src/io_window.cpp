// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "io.hpp"

#include <cstdint>

#include "SDL.h"
#include "SDL_video.h"
#include "config.hpp"
#include "debug.hpp"
#include "io_internal.hpp"
#include "version.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static P get_sdl_window_px_dims()
{
        P px_dims;

        SDL_GetWindowSize(io::g_sdl_window, &px_dims.x, &px_dims.y);

        return px_dims;
}

static std::string make_window_title()
{
        std::string title = "Infra Arcana ";

        title += version_info::g_version_str;

        const auto git_sha1_result = version_info::read_git_sha1_str_from_file();

        if (git_sha1_result) {
                title += " (" + git_sha1_result.value() + ")";
        }

        return title;
}

static uint32_t get_sdl_window_flags(const IsFullscreen is_fullscreen)
{
        if (is_fullscreen == IsFullscreen::no) {
                return SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;
        }
        else {
                return SDL_WINDOW_FULLSCREEN_DESKTOP;
        }
}

static SDL_Window* create_sdl_window(
        const P& px_dims,
        const IsFullscreen is_fullscreen)
{
        TRACE_FUNC_BEGIN;

        TRACE << "Attempting to create window with size: "
              << px_dims.x << ", " << px_dims.y << std::endl;

        SDL_Window* window = nullptr;

        const auto title = make_window_title();

        const auto sdl_window_flags = get_sdl_window_flags(is_fullscreen);

        window =
                SDL_CreateWindow(
                        title.c_str(),
                        SDL_WINDOWPOS_CENTERED,
                        SDL_WINDOWPOS_CENTERED,
                        px_dims.x,
                        px_dims.y,
                        sdl_window_flags);

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

        const auto offsets = extra_space.scaled_down(2);

        return offsets;
}

static P calc_rendering_offsets(
        const P& window_px_dims,
        const P& rendering_area_px_dims)
{
        const auto offsets =
                calc_offsets_to_center_rectangle(
                        window_px_dims,
                        rendering_area_px_dims);

        return offsets;
}

static void update_rendering_offsets()
{
        // NOTE: The window may be scaled up, but the screen panel is never
        // scaled up. Therefore we scale down the window size to compare it with
        // the screen panel, and then scale up the offsets again afterwards.

        const auto window_px_dims = get_sdl_window_px_dims();

        const int scale_factor = config::video_scale_factor();

        const auto window_logical_px_dims = window_px_dims.scaled_down(scale_factor);

        const auto screen_panel_logical_px_dims = io::panel_px_dims(Panel::screen);

        const auto rendering_px_offset =
                calc_rendering_offsets(
                        window_logical_px_dims,
                        screen_panel_logical_px_dims);

        io::g_rendering_px_offset = rendering_px_offset.scaled_up(scale_factor);
}

static SDL_Window* init_window_fullscreen()
{
        TRACE << "Initializing with fullscreen" << std::endl;

        const auto native_resolution = io::get_native_resolution();

        auto window_size = native_resolution;

        window_size = window_size.scaled_down(config::video_scale_factor());

        TRACE
                << "Fullscreen window size: "
                << window_size.x << "x"
                << window_size.y
                << std::endl;

        panels::init(io::px_to_gui_coords(window_size));

        auto* const window = create_sdl_window(window_size, IsFullscreen::yes);

        return window;
}

static SDL_Window* init_window_windowed()
{
        const auto window_size =
                P(
                        config::window_px_w(),
                        config::window_px_h());

        const auto window_logical_size =
                window_size.scaled_down(
                        config::video_scale_factor());

        TRACE
                << "Window size: "
                << window_size.x << "x"
                << window_size.y
                << std::endl;

        TRACE << "Window logical size: "
              << window_logical_size.x << "x"
              << window_logical_size.y
              << std::endl;

        panels::init(io::px_to_gui_coords(window_logical_size));

        auto* const window = create_sdl_window(window_size, IsFullscreen::no);

        return window;
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
        if (g_sdl_window) {
                SDL_DestroyWindow(g_sdl_window);

                g_sdl_window = nullptr;
        }

        const P native_resolution = get_native_resolution();

        TRACE
                << "Native resolution: "
                << native_resolution.x << "x"
                << native_resolution.y
                << std::endl;

        if (config::is_fullscreen()) {
                g_sdl_window = init_window_fullscreen();

                if (!g_sdl_window) {
                        // Fullscreen failed, do windowed mode instead.
                        config::set_fullscreen(false);
                }
        }

        // NOTE: Fullscreen may have been disabled while attempting to set up a
        // fullscreen window (see above), so we check again here if fullscreen
        // is enabled.
        if (!config::is_fullscreen()) {
                g_sdl_window = init_window_windowed();
        }

        if (!g_sdl_window) {
                TRACE_ERROR_RELEASE
                        << "Failed to set up window"
                        << std::endl
                        << SDL_GetError()
                        << std::endl;

                PANIC;
        }

        update_rendering_offsets();
}

void try_set_window_gui_cells(P new_gui_dims)
{
        const auto new_logical_px_size = io::gui_to_px_coords(new_gui_dims);

        const auto new_px_size =
                new_logical_px_size.scaled_up(
                        config::video_scale_factor());

        SDL_SetWindowSize(g_sdl_window, new_px_size.x, new_px_size.y);
}

void on_window_resized()
{
        const auto new_px_dims = get_sdl_window_px_dims();

        const auto new_logical_px_dims =
                new_px_dims.scaled_down(
                        config::video_scale_factor());

        TRACE << "New window size: "
              << new_px_dims.x << "x"
              << new_px_dims.y
              << std::endl;

        TRACE << "New logical window size: "
              << new_logical_px_dims.x << "x"
              << new_logical_px_dims.y
              << std::endl;

        config::set_window_px_w(new_px_dims.x);
        config::set_window_px_h(new_px_dims.y);

        panels::init(io::px_to_gui_coords(new_logical_px_dims));

        update_rendering_offsets();

        states::on_window_resized();
}

bool is_window_maximized()
{
        // TODO: This does not seem to work very well:
        // * The flag is sometimes not set when maximizing the window
        // * The flag sometimes gets "stuck" after the window is restored from
        //   being maximized. The flag is only cleared again after tabbing to
        //   another window and back again, or after minimizing and restoring
        //   the window
        //
        // Is this an SDL bug?
        //
        return SDL_GetWindowFlags(g_sdl_window) & SDL_WINDOW_MAXIMIZED;
}

P sdl_window_gui_dims()
{
        const auto px_dims = get_sdl_window_px_dims();

        const auto logical_px_dims =
                px_dims.scaled_down(
                        config::video_scale_factor());

        return io::px_to_gui_coords(logical_px_dims);
}

void update_screen()
{
        SDL_RenderPresent(g_sdl_renderer);
}

void clear_screen()
{
        SDL_SetRenderDrawColor(g_sdl_renderer, 0U, 0U, 0U, 0xFFU);

        SDL_RenderClear(g_sdl_renderer);
}

P get_native_resolution()
{
        SDL_DisplayMode display_mode;

        const auto result = SDL_GetDesktopDisplayMode(0, &display_mode);

        if (result != 0) {
                TRACE_ERROR_RELEASE
                        << "Failed to read native resolution"
                        << std::endl
                        << SDL_GetError()
                        << std::endl;

                PANIC;
        }

        return {display_mode.w, display_mode.h};
}

}  // namespace io

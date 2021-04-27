// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "io.hpp"

#include <algorithm>
#include <ostream>
#include <stddef.h>

#include "SDL.h"
#include "SDL_error.h"
#include "SDL_events.h"
#include "SDL_filesystem.h"
#include "SDL_image.h"
#include "SDL_mixer.h"
#include "SDL_pixels.h"
#include "SDL_rect.h"
#include "SDL_render.h"
#include "SDL_stdinc.h"
#include "SDL_surface.h"
#include "SDL_timer.h"
#include "SDL_video.h"
#include "audio.hpp"
#include "config.hpp"
#include "debug.hpp"
#include "direction.hpp"
#include "io_internal.hpp"
#include "paths.hpp"
#include "state.hpp"
#include "version.hpp"

struct SDL_Texture;

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static SDL_Surface* load_surface(const std::string& path)
{
        auto* const surface = IMG_Load(path.c_str());

        if (!surface)
        {
                TRACE_ERROR_RELEASE
                        << "Failed to load surface from path '"
                        << path
                        << "': "
                        << IMG_GetError()
                        << std::endl;

                PANIC;
        }

        return surface;
}

static void swap_surface_color(
        SDL_Surface& surface,
        const Color& color_before,
        const Color& color_after)
{
        for (int x = 0; x < surface.w; ++x)
        {
                for (int y = 0; y < surface.h; ++y)
                {
                        const P p(x, y);

                        const auto color = io::read_px_on_surface(surface, p);

                        if (color == color_before)
                        {
                                io::put_px_on_surface(surface, p, color_after);
                        }
                }
        }
}

static bool should_put_contour_at(
        const SDL_Surface& surface,
        const P& surface_px_pos,
        const R& surface_px_rect,
        const Color& bg_color,
        const Color& contour_color)
{
        // Only allow drawing a contour at pixels with the same color as the
        // background color parameter
        {
                const auto color =
                        io::read_px_on_surface(
                                surface,
                                surface_px_pos);

                if (color != bg_color)
                {
                        return false;
                }
        }

        // Draw a contour here if it has a neighbour with different color than
        // the background or contour color (i.e. if it has a neighbour with a
        // color that will be drawn to the screen)
        for (const auto& d : dir_utils::g_dir_list)
        {
                const auto adj_p = surface_px_pos + d;

                if (!surface_px_rect.is_pos_inside(adj_p))
                {
                        continue;
                }

                const auto adj_color = io::read_px_on_surface(surface, adj_p);

                if ((adj_color == bg_color) || (adj_color == contour_color))
                {
                        continue;
                }

                return true;
        }

        return false;
}

static void draw_black_contour_for_surface(
        SDL_Surface& surface,
        const Color& bg_color)
{
        const R surface_px_rect({0, 0}, {surface.w - 1, surface.h - 1});

        const auto contour_color = colors::black();

        for (const auto& surface_px_pos : surface_px_rect.positions())
        {
                const bool should_put_contour =
                        should_put_contour_at(
                                surface,
                                surface_px_pos,
                                surface_px_rect,
                                bg_color,
                                contour_color);

                if (should_put_contour)
                {
                        io::put_px_on_surface(
                                surface,
                                surface_px_pos,
                                contour_color);
                }
        }
}

static void verify_texture_size(
        SDL_Texture* texture,
        const P& expected_size,
        const std::string& img_path)
{
        P size;

        SDL_QueryTexture(texture, nullptr, nullptr, &size.x, &size.y);

        // Verify width and height of loaded image
        if (size != expected_size)
        {
                TRACE_ERROR_RELEASE
                        << "Tile image at \""
                        << img_path
                        << "\" has wrong size: "
                        << size.x
                        << "x"
                        << size.x
                        << ", expected: "
                        << expected_size.x
                        << "x"
                        << expected_size.y
                        << std::endl;

                PANIC;
        }
}

static SDL_Texture* create_texture_from_surface(SDL_Surface& surface)
{
        auto* const texture =
                SDL_CreateTextureFromSurface(
                        io::g_sdl_renderer,
                        &surface);

        if (!texture)
        {
                TRACE_ERROR_RELEASE
                        << "Failed to create texture from surface: "
                        << IMG_GetError()
                        << std::endl;

                PANIC;
        }

        return texture;
}

static void set_surface_color_key(SDL_Surface& surface, const Color& color)
{
        const int v =
                SDL_MapRGB(
                        surface.format,
                        color.r(),
                        color.g(),
                        color.b());

        const bool enable_color_key = true;

        SDL_SetColorKey(&surface, enable_color_key, v);
}

static SDL_Texture* load_texture(const std::string& path)
{
        auto* const surface = load_surface(path);

        set_surface_color_key(*surface, colors::black());

        auto* const texture = create_texture_from_surface(*surface);

        SDL_FreeSurface(surface);

        return texture;
}

static P sdl_window_px_dims()
{
        P px_dims;

        SDL_GetWindowSize(io::g_sdl_window, &px_dims.x, &px_dims.y);

        return px_dims;
}

static SDL_Renderer* create_renderer()
{
        TRACE_FUNC_BEGIN;

        auto* const renderer =
                SDL_CreateRenderer(
                        io::g_sdl_window,
                        -1,
                        SDL_RENDERER_SOFTWARE);

        if (!renderer)
        {
                TRACE_ERROR_RELEASE
                        << "Failed to create SDL renderer"
                        << std::endl
                        << SDL_GetError()
                        << std::endl;

                PANIC;
        }

        TRACE_FUNC_END;

        return renderer;
}

static void cleanup_sdl()
{
        if (!SDL_WasInit(SDL_INIT_EVERYTHING))
        {
                return;
        }

        IMG_Quit();

        Mix_AllocateChannels(0);

        Mix_CloseAudio();

        SDL_Quit();
}

static void init_sdl()
{
        TRACE_FUNC_BEGIN;

        cleanup_sdl();

        const uint32_t sdl_init_flags =
                SDL_INIT_VIDEO |
                SDL_INIT_AUDIO |
                SDL_INIT_EVENTS;

        if (SDL_Init(sdl_init_flags) == -1)
        {
                TRACE_ERROR_RELEASE
                        << "Failed to init SDL"
                        << std::endl
                        << SDL_GetError()
                        << std::endl;

                PANIC;
        }

        const uint32_t sdl_img_flags = IMG_INIT_PNG;

        if (IMG_Init(sdl_img_flags) == -1)
        {
                TRACE_ERROR_RELEASE
                        << "Failed to init SDL_image"
                        << std::endl
                        << SDL_GetError()
                        << std::endl;

                PANIC;
        }

        const int audio_freq = 44100;
        const Uint16 audio_format = MIX_DEFAULT_FORMAT;
        const int audio_channels = 2;
        const int audio_buffers = 1024;

        const int result =
                Mix_OpenAudio(
                        audio_freq,
                        audio_format,
                        audio_channels,
                        audio_buffers);

        if (result == -1)
        {
                TRACE_ERROR_RELEASE
                        << "Failed to init SDL_mixer"
                        << std::endl
                        << SDL_GetError()
                        << std::endl;

                ASSERT(false);
        }

        Mix_AllocateChannels(audio::g_allocated_channels);

        TRACE_FUNC_END;
}

static void init_window(const P& px_dims)
{
        TRACE_FUNC_BEGIN;

        TRACE
                << "Attempting to create window with size: "
                << px_dims.x
                << ", "
                << px_dims.y
                << std::endl;

        std::string title = "Infra Arcana ";

        if (version_info::g_version_str.empty())
        {
                const auto git_sha1_str =
                        version_info::read_git_sha1_str_from_file();

                title +=
                        "(build " +
                        git_sha1_str +
                        ", " +
                        version_info::g_date_str +
                        ")";
        }
        else
        {
                // Version string defined
                title += version_info::g_version_str;
        }

        if (io::g_sdl_window)
        {
                SDL_DestroyWindow(io::g_sdl_window);
        }

        uint32_t sdl_window_flags = 0;

        if (config::is_fullscreen())
        {
                TRACE << "Fullscreen mode" << std::endl;

                sdl_window_flags = SDL_WINDOW_FULLSCREEN_DESKTOP;
        }
        else
        {
                TRACE << "Windowed mode" << std::endl;

                sdl_window_flags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;
        }

        io::g_sdl_window =
                SDL_CreateWindow(
                        title.c_str(),
                        SDL_WINDOWPOS_CENTERED,
                        SDL_WINDOWPOS_CENTERED,
                        px_dims.x,
                        px_dims.y,
                        sdl_window_flags);

        if (!io::g_sdl_window)
        {
                TRACE
                        << "Failed to create window: "
                        << std::endl
                        << SDL_GetError()
                        << std::endl;
        }

        TRACE_FUNC_END;
}

static void init_renderer()
{
        TRACE_FUNC_BEGIN;

        if (io::g_sdl_renderer)
        {
                SDL_DestroyRenderer(io::g_sdl_renderer);
        }

        io::g_sdl_renderer = create_renderer();

        TRACE_FUNC_END;
}

static P native_resolution_from_sdl()
{
        SDL_DisplayMode display_mode;

        const auto result = SDL_GetDesktopDisplayMode(0, &display_mode);

        if (result != 0)
        {
                TRACE_ERROR_RELEASE
                        << "Failed to read native resolution"
                        << std::endl
                        << SDL_GetError()
                        << std::endl;

                PANIC;
        }

        return {display_mode.w, display_mode.h};
}

static void update_rendering_offsets()
{
        const auto screen_panel_dims = io::panel_px_dims(Panel::screen);

        // TODO: Is centering needed in fullscreen?
        const bool is_centering_allowed =
                panels::is_valid() &&
                !config::is_fullscreen();

        if (is_centering_allowed)
        {
                P window_dims;

                SDL_GetWindowSize(
                        io::g_sdl_window,
                        &window_dims.x,
                        &window_dims.y);

                const P extra_space(window_dims - screen_panel_dims);

                io::g_rendering_px_offset = extra_space.scaled_down(2);
        }
        else
        {
                io::g_rendering_px_offset.set(0, 0);
        }
}

static void load_logo()
{
        TRACE_FUNC_BEGIN;

        io::g_logo_texture = load_texture(paths::logo_img_path());

        TRACE_FUNC_END;
}

static void load_font()
{
        TRACE_FUNC_BEGIN;

        const auto img_path = paths::fonts_dir() + config::font_name();

        auto* const surface = load_surface(img_path);

        swap_surface_color(*surface, colors::black(), colors::magenta());

        set_surface_color_key(*surface, colors::magenta());

        // Create the non-contour version
        auto* texture = create_texture_from_surface(*surface);

        io::g_font_texture = texture;

        draw_black_contour_for_surface(*surface, colors::magenta());

        // Create the version with contour
        texture = create_texture_from_surface(*surface);

        io::g_font_texture_with_contours = texture;

        SDL_FreeSurface(surface);

        TRACE_FUNC_END;
}

static void load_tile(const gfx::TileId id, const P& cell_px_dims)
{
        const auto img_name = gfx::tile_id_to_str(id);
        const auto img_path = paths::tiles_dir() + img_name + ".png";

        auto* const surface = load_surface(img_path);

        swap_surface_color(*surface, colors::black(), colors::magenta());

        set_surface_color_key(*surface, colors::magenta());

        // Create the non-contour version
        auto* texture = create_texture_from_surface(*surface);

        io::g_tile_textures[(size_t)id] = texture;

        draw_black_contour_for_surface(*surface, colors::magenta());

        // Create the version with contour
        texture = create_texture_from_surface(*surface);

        io::g_tile_textures_with_contours[(size_t)id] = texture;

        verify_texture_size(texture, cell_px_dims, img_path);

        SDL_FreeSurface(surface);
}

static void load_tiles()
{
        TRACE_FUNC_BEGIN;

        const P cell_px_dims(
                config::map_cell_px_w(),
                config::map_cell_px_h());

        for (size_t i = 0; i < (size_t)gfx::TileId::END; ++i)
        {
                load_tile((gfx::TileId)i, cell_px_dims);
        }

        TRACE_FUNC_END;
}

// -----------------------------------------------------------------------------
// io
// -----------------------------------------------------------------------------
namespace io
{
SDL_Window* g_sdl_window = nullptr;
SDL_Renderer* g_sdl_renderer = nullptr;
SDL_Texture* g_font_texture_with_contours = nullptr;
SDL_Texture* g_font_texture = nullptr;
SDL_Texture* g_tile_textures[(size_t)gfx::TileId::END] = {};
SDL_Texture* g_tile_textures_with_contours[(size_t)gfx::TileId::END] = {};
SDL_Texture* g_logo_texture = nullptr;

P g_rendering_px_offset = {};

void init()
{
        TRACE_FUNC_BEGIN;

        cleanup();

        init_sdl();

        if (config::is_fullscreen())
        {
                TRACE << "Initializing with fullscreen" << std::endl;

                const auto native_resolution = native_resolution_from_sdl();

                TRACE
                        << "Native resolution: "
                        << native_resolution.x
                        << ", "
                        << native_resolution.y
                        << std::endl;

                auto window_resolution = native_resolution;

                if (config::is_2x_scale_fullscreen_enabled())
                {
                        window_resolution = window_resolution.scaled_down(2);
                }

                TRACE
                        << "Attempting fullscreen with window resolution: "
                        << window_resolution.x
                        << ", "
                        << window_resolution.y
                        << std::endl;

                panels::init(io::px_to_gui_coords(window_resolution));

                if (!panels::is_valid() &&
                    config::is_2x_scale_fullscreen_enabled())
                {
                        TRACE
                                << "2x scaled fullscreen not possible, "
                                   "disabling 2x scaling"
                                << std::endl;

                        // Disable the actual scaling setting (but keep the user
                        // option to request 2x scaling enabled)
                        config::set_2x_scale_fullscreen_enabled(false);

                        window_resolution = native_resolution;

                        panels::init(
                                io::px_to_gui_coords(
                                        window_resolution));
                }

                if (panels::is_valid())
                {
                        // The panels think we're OK - try creating the window
                        init_window(window_resolution);
                }

                if (!g_sdl_window)
                {
                        // Fullscreen failed, fall back on windowed mode instead
                        config::set_fullscreen(false);
                }
        }

        // NOTE: Fullscreen may have been disabled while attempting to set up a
        // fullscreen "window" (see above), so we check again here if fullscreen
        // is enabled
        if (!config::is_fullscreen())
        {
                const auto min_gui_dims = io::min_screen_gui_dims();

                const auto config_res =
                        P(config::screen_px_w(),
                          config::screen_px_h());

                const auto config_gui_dims = px_to_gui_coords(config_res);

                const auto native_res = native_resolution_from_sdl();

                TRACE << "Minimum required GUI dimensions: "
                      << min_gui_dims.x
                      << ","
                      << min_gui_dims.y
                      << std::endl;

                TRACE << "Config GUI dimensions: "
                      << config_gui_dims.x
                      << ","
                      << config_gui_dims.y
                      << std::endl;

                TRACE << "Config resolution: "
                      << config_res.x
                      << ","
                      << config_res.y
                      << std::endl;

                TRACE << "Native resolution: "
                      << native_res.x
                      << ","
                      << native_res.y
                      << std::endl;

                const auto screen_gui_dims_used =
                        ((config_res.x <= native_res.x) &&
                         (config_res.y <= native_res.y))
                        ? config_gui_dims
                        : min_gui_dims;

                TRACE << "Max number of GUI cells used (based on desired and "
                      << "native resolution): "
                      << std::endl
                      << screen_gui_dims_used.x
                      << ","
                      << screen_gui_dims_used.y
                      << std::endl;

                panels::init(screen_gui_dims_used);

                const auto screen_panel_px_dims =
                        panel_px_dims(Panel::screen);

                const P window_px_dims(
                        std::max(screen_panel_px_dims.x, config_res.x),
                        std::max(screen_panel_px_dims.y, config_res.y));

                init_window(window_px_dims);
        }

        if (!g_sdl_window)
        {
                TRACE_ERROR_RELEASE
                        << "Failed to set up window"
                        << std::endl
                        << SDL_GetError()
                        << std::endl;

                PANIC;
        }

        init_renderer();

        load_font();

        if (config::is_tiles_mode())
        {
                load_tiles();

                load_logo();
        }

        update_rendering_offsets();

        TRACE_FUNC_END;
}

void cleanup()
{
        TRACE_FUNC_BEGIN;

        if (g_sdl_renderer)
        {
                SDL_DestroyRenderer(g_sdl_renderer);
                g_sdl_renderer = nullptr;
        }

        if (g_sdl_window)
        {
                SDL_DestroyWindow(g_sdl_window);
                g_sdl_window = nullptr;
        }

        cleanup_sdl();

        TRACE_FUNC_END;
}

void update_screen()
{
        const auto screen_panel_dims = panel_px_dims(Panel::screen);

        if (!panels::is_valid() &&
            (screen_panel_dims.x > config::gui_cell_px_w()) &&
            (screen_panel_dims.y > config::gui_cell_px_h()))
        {
                draw_text_at_px(
                        "Window too small",
                        {0, 0},
                        colors::light_white(),
                        DrawBg::no,
                        colors::black());
        }

        SDL_RenderPresent(g_sdl_renderer);
}

void clear_screen()
{
        SDL_SetRenderDrawColor(g_sdl_renderer, 0U, 0U, 0U, 0xFFU);

        SDL_RenderClear(g_sdl_renderer);
}

P min_screen_gui_dims()
{
        // Use the maximum of:
        // * The hard minimum required number of gui cells
        // * The minimum required resolution, converted to gui cells, rounded up

        P gui_cell_px_dims(
                config::gui_cell_px_w(),
                config::gui_cell_px_h());

        if (config::is_fullscreen() &&
            config::is_2x_scale_fullscreen_enabled())
        {
                gui_cell_px_dims = gui_cell_px_dims.scaled_up(2);
        }

        P min_nr_gui_cells(
                (g_min_res_w + gui_cell_px_dims.x - 1) / gui_cell_px_dims.x,
                (g_min_res_h + gui_cell_px_dims.y - 1) / gui_cell_px_dims.y);

        min_nr_gui_cells.set(
                std::max(min_nr_gui_cells.x, g_min_nr_gui_cells_x),
                std::max(min_nr_gui_cells.y, g_min_nr_gui_cells_y));

        return min_nr_gui_cells;
}

void on_fullscreen_toggled()
{
        clear_screen();

        update_screen();

        // TODO: This should not be necessary - but without this call there is
        // some strange behavior with the window losing focus, and becoming
        // hidden behind other windows when toggling fullscreen (until you
        // alt-tab back to the window)
        init();

        states::draw();

        update_screen();
}

void draw_character_at_px(
        const char character,
        P px_pos,
        const Color& color,
        const io::DrawBg draw_bg,
        const Color& bg_color)
{
        // TODO: Black foreground looks terrible with grayscale shaded font
        // image (all shades of white are colored black). The current solution
        // to this is to simply never use black foreground, since it's not
        // really necessary.
        ASSERT(color != colors::black());

        P gui_cell_px_dims(config::gui_cell_px_w(), config::gui_cell_px_h());

        if (draw_bg == io::DrawBg::yes)
        {
                // NOTE: No rendering offsets or scaling calculated yet, the
                // rectangle function performs its own offsets and scaling
                io::draw_rectangle_filled(
                        {px_pos, px_pos + gui_cell_px_dims - 1},
                        bg_color);
        }

        // Set up the texture clip rectangle, before calculating scaling
        // NOTE: We expect one pixel separator between each glyph
        auto char_px_pos = gfx::character_pos(character);

        char_px_pos.x *= gui_cell_px_dims.x + 1;
        char_px_pos.y *= gui_cell_px_dims.y;

        SDL_Rect clip_rect;

        clip_rect.x = char_px_pos.x;
        clip_rect.y = char_px_pos.y;
        clip_rect.w = gui_cell_px_dims.x;
        clip_rect.h = gui_cell_px_dims.y;

        // * Now apply offset and scaling, if needed *

        // Scaling
        if (config::is_fullscreen() &&
            config::is_2x_scale_fullscreen_enabled())
        {
                // We are running fullscreen 2x scale - scale up the position
                // and rendering size
                px_pos = px_pos.scaled_up(2);
                gui_cell_px_dims = gui_cell_px_dims.scaled_up(2);
        }

        // Apply rendering offsets (to center the graphics in the window)
        px_pos = px_pos.with_offsets(g_rendering_px_offset);

        SDL_Rect render_rect;

        render_rect.x = px_pos.x;
        render_rect.y = px_pos.y;
        render_rect.w = gui_cell_px_dims.x;
        render_rect.h = gui_cell_px_dims.y;

        SDL_Texture* texture = nullptr;

        // TODO: If black foreground will not be allowed, the contour version
        // can probably always be used
        if (/* (color == colors::black()) || */
            (bg_color == colors::black()))
        {
                texture = g_font_texture;
        }
        else
        {
                texture = g_font_texture_with_contours;
        }

        SDL_SetTextureColorMod(texture, color.r(), color.g(), color.b());

        SDL_RenderCopy(g_sdl_renderer, texture, &clip_rect, &render_rect);
}

void draw_tile(
        const gfx::TileId tile,
        const Panel panel,
        const P& pos,
        const Color& color,
        const DrawBg draw_bg,
        const Color& bg_color)
{
        if (!panels::is_valid())
        {
                return;
        }

        P px_pos = map_to_px_coords(panel, pos);

        P map_cell_px_dims(config::map_cell_px_w(), config::map_cell_px_h());

        if (draw_bg == DrawBg::yes)
        {
                // NOTE: No rendering offsets or scaling calculated yet, the
                // rectangle function performs its own offsets and scaling
                draw_rectangle_filled(
                        {px_pos, px_pos + map_cell_px_dims - 1},
                        bg_color);
        }

        // * Now apply offset and scaling, if needed *

        // Scaling
        if (config::is_fullscreen() &&
            config::is_2x_scale_fullscreen_enabled())
        {
                // We are running fullscreen 2x scale - scale up the position
                // and rendering size
                px_pos = px_pos.scaled_up(2);
                map_cell_px_dims = map_cell_px_dims.scaled_up(2);
        }

        // Apply rendering offsets (to center the graphics in the window)
        px_pos = px_pos.with_offsets(g_rendering_px_offset);

        SDL_Rect render_rect;

        render_rect.x = px_pos.x;
        render_rect.y = px_pos.y;
        render_rect.w = map_cell_px_dims.x;
        render_rect.h = map_cell_px_dims.y;

        SDL_Texture* texture = nullptr;

        if ((color == colors::black()) || (bg_color == colors::black()))
        {
                // Foreground or background is black - no contours
                texture = g_tile_textures[(size_t)tile];
        }
        else
        {
                // Both foreground and background are non-black - use contours
                texture = g_tile_textures_with_contours[(size_t)tile];
        }

        SDL_SetTextureColorMod(texture, color.r(), color.g(), color.b());

        SDL_RenderCopy(
                g_sdl_renderer,
                texture,
                nullptr,  // No clipping needed, drawing whole texture
                &render_rect);
}

void cover_panel(const Panel panel, const Color& color)
{
        if (!panels::is_valid())
        {
                return;
        }

        const auto px_area = gui_to_px_rect(panels::area(panel));

        draw_rectangle_filled(px_area, color);
}

void cover_area(
        const Panel panel,
        const R& area,
        const Color& color)
{
        const auto panel_p0 = panels::p0(panel);

        const auto screen_area = area.with_offset(panel_p0);

        const auto px_area =
                gui_to_px_rect(screen_area)
                        .with_offset(g_rendering_px_offset);

        draw_rectangle_filled(px_area, color);
}

void cover_area(
        const Panel panel,
        const P& offset,
        const P& dims,
        const Color& color)
{
        const auto area = R(offset, offset + dims - 1);

        cover_area(panel, area, color);
}

void cover_cell(const Panel panel, const P& offset)
{
        cover_area(panel, offset, {1, 1});
}

void draw_logo()
{
        if (!panels::is_valid())
        {
                return;
        }

        // Set pixel position *before* applying rendering offset and scaling
        const int screen_px_w = panel_px_w(Panel::screen);

        P img_px_dims;

        SDL_QueryTexture(
                g_logo_texture,
                nullptr,
                nullptr,
                &img_px_dims.x,
                &img_px_dims.y);

        P px_pos((screen_px_w - img_px_dims.x) / 2, 0);

        // * Now apply offset and scaling, if needed *

        // Scaling
        if (config::is_fullscreen() &&
            config::is_2x_scale_fullscreen_enabled())
        {
                // We are running fullscreen 2x scale - scale up the position
                // and rendering size
                px_pos = px_pos.scaled_up(2);
                img_px_dims = img_px_dims.scaled_up(2);
        }

        // Apply rendering offsets (to center the graphics in the window)
        px_pos = px_pos.with_offsets(g_rendering_px_offset);

        SDL_Rect render_rect;

        render_rect.x = px_pos.x;
        render_rect.y = px_pos.y;
        render_rect.w = img_px_dims.x;
        render_rect.h = img_px_dims.y;

        SDL_RenderCopy(
                g_sdl_renderer,
                g_logo_texture,
                nullptr,  // No clipping needed, drawing whole texture
                &render_rect);
}

void try_set_window_gui_cells(P new_gui_dims)
{
        const P min_gui_dims = io::min_screen_gui_dims();

        new_gui_dims.x = std::max(new_gui_dims.x, min_gui_dims.x);
        new_gui_dims.y = std::max(new_gui_dims.y, min_gui_dims.y);

        const P new_px_dims = io::gui_to_px_coords(new_gui_dims);

        SDL_SetWindowSize(g_sdl_window, new_px_dims.x, new_px_dims.y);
}

void on_window_resized()
{
        const auto new_px_dims = sdl_window_px_dims();

        config::set_screen_px_w(new_px_dims.x);
        config::set_screen_px_h(new_px_dims.y);

        TRACE << "New window size: "
              << new_px_dims.x
              << ", "
              << new_px_dims.y
              << std::endl;

        panels::init(io::px_to_gui_coords(new_px_dims));

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
        const P px_dims = sdl_window_px_dims();

        return io::px_to_gui_coords(px_dims);
}

std::string sdl_pref_dir()
{
        std::string version_str;

        if (version_info::g_version_str.empty())
        {
                version_str = version_info::read_git_sha1_str_from_file();
        }
        else
        {
                version_str = version_info::g_version_str;
        }

        auto* const path_ptr =
                // NOTE: This is somewhat of a hack, see the function arguments
                SDL_GetPrefPath(
                        "infra_arcana",  // "Organization"
                        version_str.c_str());  // "Application"

        // NOTE: Using non-const value to allow move on return
        std::string path_str = path_ptr;

        SDL_free(path_ptr);

        TRACE << "User data directory: " << path_str << std::endl;

        return path_str;
}

void sleep(const uint32_t duration)
{
        if ((duration == 0) || config::is_bot_playing())
        {
                return;
        }
        else if (duration == 1)
        {
                SDL_Delay(duration);
        }
        else
        {
                // Duration longer than 1 ms
                const auto wait_until = SDL_GetTicks() + duration;

                while (SDL_GetTicks() < wait_until)
                {
                        SDL_PumpEvents();
                }
        }
}

}  // namespace io

// =============================================================================
// Copyright 2011-2024 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "io.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <optional>
#include <ostream>

#include "SDL.h"
#include "SDL_blendmode.h"
#include "SDL_error.h"
#include "SDL_events.h"
#include "SDL_filesystem.h"
#include "SDL_image.h"
#include "SDL_mixer.h"
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
#include "text_format.hpp"
#include "version.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static SDL_Surface* load_surface(const std::string& path)
{
        auto* const surface = IMG_Load(path.c_str());

        if (!surface) {
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
        for (int x = 0; x < surface.w; ++x) {
                for (int y = 0; y < surface.h; ++y) {
                        const P p(x, y);

                        const auto color = io::read_px_on_surface(surface, p);

                        if (color == color_before) {
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
        // background color parameter.
        const auto color = io::read_px_on_surface(surface, surface_px_pos);

        if (color != bg_color) {
#ifndef NDEBUG
                if ((color.r() != color.g()) ||
                    (color.r() != color.b())) {
                        TRACE
                                << "Found color other than grayscale color: "
                                << (int)color.r()
                                << ","
                                << (int)color.g()
                                << ","
                                << (int)color.b()
                                << " - at position: "
                                << surface_px_pos.x
                                << "x"
                                << surface_px_pos.y
                                << std::endl
                                << "(Background color is: "
                                << (int)bg_color.r()
                                << ","
                                << (int)bg_color.g()
                                << ","
                                << (int)bg_color.b()
                                << ")"
                                << std::endl;

                        PANIC;
                }
#endif  // NDEBUG

                return false;
        }

        // Draw a contour here if it has a neighbour with different color than
        // the background or contour color (i.e. if it has a neighbour with a
        // color that will be drawn to the screen).
        auto pred = [&](const auto d) {
                const auto adj_p = surface_px_pos + d;

                if (!surface_px_rect.is_pos_inside(adj_p)) {
                        return false;
                }

                const auto adj_color = io::read_px_on_surface(surface, adj_p);

                const bool is_bg_color = (adj_color == bg_color);
                const bool is_contour_color = (adj_color == contour_color);

                return !is_bg_color && !is_contour_color;
        };

        return (
                std::any_of(
                        std::cbegin(dir_utils::g_dir_list),
                        std::cend(dir_utils::g_dir_list),
                        pred));
}

static void draw_black_contour_for_surface(
        SDL_Surface& surface,
        const Color& bg_color)
{
        const R surface_px_rect({0, 0}, {surface.w - 1, surface.h - 1});

        const auto contour_color = colors::black();

        for (const auto& surface_px_pos : surface_px_rect.positions()) {
                const bool should_put_contour =
                        should_put_contour_at(
                                surface,
                                surface_px_pos,
                                surface_px_rect,
                                bg_color,
                                contour_color);

                if (should_put_contour) {
                        io::put_px_on_surface(
                                surface,
                                surface_px_pos,
                                contour_color);
                }
        }
}

static void verify_tile_colors(
        const SDL_Surface& surface,
        const std::string& img_path)
{
#ifndef NDEBUG
        const Color full_black(0, 0, 0);
        const Color full_white(255, 255, 255);

        for (int x = 0; x < surface.w; ++x) {
                for (int y = 0; y < surface.h; ++y) {
                        const Color color = io::read_px_on_surface(surface, {x, y});

                        if ((color == full_black) || (color == full_white)) {
                                continue;
                        }

                        TRACE
                                << "Found illegal tile color in image '"
                                << img_path
                                << "': "
                                << (int)color.r()
                                << ","
                                << (int)color.g()
                                << ","
                                << (int)color.b()
                                << " - at position: "
                                << x
                                << "x"
                                << y
                                << std::endl;
                        PANIC;
                }
        }
#endif  // NDEBUG
}

static void verify_texture_size(
        SDL_Texture* texture,
        const P& expected_size,
        const std::string& img_path)
{
        P size;

        SDL_QueryTexture(texture, nullptr, nullptr, &size.x, &size.y);

        // Verify width and height of loaded image
        if (size != expected_size) {
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

        if (!texture) {
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
        const auto v =
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

static SDL_Renderer* create_renderer()
{
        TRACE_FUNC_BEGIN;

        uint32_t flags = 0U;

        switch (config::renderer_type()) {
        case RendererType::auto_select:
                break;

        case RendererType::sw:
                flags = SDL_RENDERER_SOFTWARE;
                break;

        case RendererType::END:
                ASSERT(false);
                break;
        }

        SDL_Renderer* const renderer =
                SDL_CreateRenderer(io::g_sdl_window, -1, flags);

        if (!renderer) {
                TRACE_ERROR_RELEASE
                        << "Failed to create SDL renderer: "
                        << SDL_GetError()
                        << std::endl;

                PANIC;
        }

        SDL_RendererInfo info;

        if (SDL_GetRendererInfo(renderer, &info) != 0) {
                TRACE_ERROR_RELEASE
                        << "Could not get RendererInfo: "
                        << SDL_GetError()
                        << std::endl;

                PANIC;
        }

        TRACE
                << "Created SDL Renderer with name: '" << info.name << "'"
                << std::endl;

        std::string flags_str;

        if (info.flags & SDL_RENDERER_SOFTWARE) {
                text_format::append_as_comma_list(
                        flags_str, "SDL_RENDERER_SOFTWARE");
        }

        if (info.flags & SDL_RENDERER_ACCELERATED) {
                text_format::append_as_comma_list(
                        flags_str, "SDL_RENDERER_ACCELERATED");
        }

        if (info.flags & SDL_RENDERER_PRESENTVSYNC) {
                text_format::append_as_comma_list(
                        flags_str, "SDL_RENDERER_PRESENTVSYNC");
        }

        if (info.flags & SDL_RENDERER_TARGETTEXTURE) {
                text_format::append_as_comma_list(
                        flags_str, "SDL_RENDERER_TARGETTEXTURE");
        }

        TRACE << "Flags: [" + flags_str + "]" << std::endl;

        TRACE_FUNC_END;

        return renderer;
}

static void init_renderer()
{
        TRACE_FUNC_BEGIN;

        if (io::g_sdl_renderer) {
                SDL_DestroyRenderer(io::g_sdl_renderer);
        }

        io::g_sdl_renderer = create_renderer();

        TRACE_FUNC_END;
}

static void load_logo()
{
        TRACE_FUNC_BEGIN;

        // Use a smaller image if graphics are scaled, otherwise the logo looks
        // gigantic.
        const std::string img_path =
                (config::video_scale_factor() == 1)
                ? paths::logo_img_path()
                : paths::logo_small_img_path();

        io::g_logo_texture = load_texture(img_path);

        TRACE_FUNC_END;
}

static void load_font()
{
        TRACE_FUNC_BEGIN;

        const std::string img_path = paths::fonts_dir() + config::font_name();

        TRACE << "Loading font image: " << img_path << std::endl;

        SDL_Surface* const surface = load_surface(img_path);

        swap_surface_color(*surface, colors::black(), colors::magenta());

        set_surface_color_key(*surface, colors::magenta());

        // Create the non-contour version
        SDL_Texture* texture = create_texture_from_surface(*surface);

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
        const std::string img_path = paths::tiles_dir() + gfx::tile_id_to_filename(id);

        TRACE << "Loading tile image: " << img_path << std::endl;

        SDL_Surface* const surface = load_surface(img_path);

        verify_tile_colors(*surface, img_path);

        swap_surface_color(*surface, colors::black(), colors::magenta());

        set_surface_color_key(*surface, colors::magenta());

        // Create the non-contour version
        SDL_Texture* texture = create_texture_from_surface(*surface);

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

        for (size_t i = 0; i < (size_t)gfx::TileId::END; ++i) {
                load_tile((gfx::TileId)i, cell_px_dims);
        }

        TRACE_FUNC_END;
}

// -----------------------------------------------------------------------------
// io
// -----------------------------------------------------------------------------
namespace io
{
SDL_Texture* g_font_texture_with_contours = nullptr;
SDL_Texture* g_font_texture = nullptr;
SDL_Texture* g_tile_textures[(size_t)gfx::TileId::END] = {};
SDL_Texture* g_tile_textures_with_contours[(size_t)gfx::TileId::END] = {};
SDL_Texture* g_logo_texture = nullptr;

P g_rendering_px_offset = {};

void init_sdl()
{
        TRACE_FUNC_BEGIN;

        cleanup_sdl();

        const uint32_t sdl_init_flags =
                SDL_INIT_VIDEO |
                SDL_INIT_AUDIO |
                SDL_INIT_EVENTS;

        if (SDL_Init(sdl_init_flags) == -1) {
                TRACE_ERROR_RELEASE
                        << "Failed to init SDL"
                        << std::endl
                        << SDL_GetError()
                        << std::endl;

                PANIC;
        }

        SDL_ShowCursor(SDL_FALSE);

        const uint32_t sdl_img_flags = IMG_INIT_PNG;

        if (IMG_Init(sdl_img_flags) == -1) {
                TRACE_ERROR_RELEASE
                        << "Failed to init SDL_image"
                        << std::endl
                        << SDL_GetError()
                        << std::endl;

                PANIC;
        }

        TRACE_FUNC_END;
}

void cleanup_sdl()
{
        if (!SDL_WasInit(SDL_INIT_EVERYTHING)) {
                return;
        }

        IMG_Quit();

        SDL_Quit();
}

void init_sdl_audio()
{
        cleanup_sdl_audio();

        const int audio_freq = 44100;
        const Uint16 audio_format = MIX_DEFAULT_FORMAT;
        const int audio_channels = MIX_DEFAULT_CHANNELS;
        const int audio_buffers = config::audio_buffer_size();

        const int result =
                Mix_OpenAudio(
                        audio_freq,
                        audio_format,
                        audio_channels,
                        audio_buffers);

        if (result == -1) {
                TRACE_ERROR_RELEASE
                        << "Failed to init SDL_mixer"
                        << std::endl
                        << SDL_GetError()
                        << std::endl;

                ASSERT(false);
        }

        Mix_AllocateChannels(audio::g_allocated_channels);
}

void cleanup_sdl_audio()
{
        Mix_AllocateChannels(0);

        Mix_CloseAudio();
}

void init_other()
{
        TRACE_FUNC_BEGIN;

        cleanup_other();

        init_window();
        init_renderer();

        SDL_SetRenderDrawBlendMode(io::g_sdl_renderer, SDL_BLENDMODE_BLEND);

        load_font();

        if (config::is_tiles_mode()) {
                load_tiles();
                load_logo();
        }

        init_input();
        init_animation();

        TRACE_FUNC_END;
}

void cleanup_other()
{
        TRACE_FUNC_BEGIN;

        if (g_sdl_renderer) {
                SDL_DestroyRenderer(g_sdl_renderer);
                g_sdl_renderer = nullptr;
        }

        if (g_sdl_window) {
                SDL_DestroyWindow(g_sdl_window);
                g_sdl_window = nullptr;
        }

        TRACE_FUNC_END;
}

void on_user_toggle_fullscreen()
{
        TRACE_FUNC_BEGIN;

        init_other();

        states::draw();
        update_screen();

        TRACE_FUNC_END;
}

void on_user_toggle_scaling()
{
        TRACE_FUNC_BEGIN;

        init_other();

        states::draw();
        update_screen();

        TRACE_FUNC_END;
}

void set_clip_rect_to_panel(const Panel panel)
{
        auto px_area = gui_to_px_rect(panels::area(panel));

        px_area = px_area.scaled_up(config::video_scale_factor());

        const SDL_Rect clip_rect {
                px_area.p0.x,
                px_area.p0.y,
                px_area.w(),
                px_area.h()};

        SDL_RenderSetClipRect(g_sdl_renderer, &clip_rect);
}

void disable_clip_rect()
{
        SDL_RenderSetClipRect(g_sdl_renderer, nullptr);
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

        if (draw_bg == io::DrawBg::yes) {
                // NOTE: No rendering offsets or scaling calculated yet, the
                // rectangle function performs its own offsets and scaling.
                io::draw_rectangle_filled(
                        {px_pos, px_pos + gui_cell_px_dims - 1},
                        bg_color);
        }

        // Set up the texture clip rectangle, before calculating scaling
        // NOTE: We expect one pixel separator between each glyph.
        auto char_px_pos = gfx::character_pos(character);

        char_px_pos.x *= (gui_cell_px_dims.x + 1);
        char_px_pos.y *= (gui_cell_px_dims.y);

        SDL_Rect clip_rect;

        clip_rect.x = char_px_pos.x;
        clip_rect.y = char_px_pos.y;
        clip_rect.w = gui_cell_px_dims.x;
        clip_rect.h = gui_cell_px_dims.y;

        // * Now apply offset and scaling *

        // Scaling
        const int scale_factor = config::video_scale_factor();

        px_pos = px_pos.scaled_up(scale_factor);
        gui_cell_px_dims = gui_cell_px_dims.scaled_up(scale_factor);

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
            (bg_color == colors::black())) {
                texture = g_font_texture;
        }
        else {
                texture = g_font_texture_with_contours;
        }

        const Color color_adapted = color.with_brightness(config::brightness_pct());

        SDL_SetTextureColorMod(
                texture,
                color_adapted.r(),
                color_adapted.g(),
                color_adapted.b());

        SDL_RenderCopy(g_sdl_renderer, texture, &clip_rect, &render_rect);
}

void draw_character(const CharacterDrawObj& obj)
{
        const auto px_pos = gui_to_px_coords(obj.panel, obj.pos);

        const auto sdl_color = obj.color.sdl_color();
        const auto sdl_color_bg = obj.bg_color.sdl_color();

        draw_character_at_px(
                obj.character,
                px_pos,
                sdl_color,
                obj.draw_bg,
                sdl_color_bg);
}

void draw_tile(const TileDrawObj& obj)
{
        P px_pos = map_to_px_coords(obj.panel, obj.pos);

        P map_cell_px_dims(config::map_cell_px_w(), config::map_cell_px_h());

        if (obj.draw_bg == DrawBg::yes) {
                // NOTE: No rendering offsets or scaling calculated yet, the
                // rectangle function performs its own offsets and scaling
                draw_rectangle_filled(
                        {px_pos, px_pos + map_cell_px_dims - 1},
                        obj.bg_color);
        }

        // * Now apply offset and scaling *

        // Scaling
        const int scale_factor = config::video_scale_factor();

        px_pos = px_pos.scaled_up(scale_factor);
        map_cell_px_dims = map_cell_px_dims.scaled_up(scale_factor);

        // Apply rendering offsets (to center the graphics in the window)
        px_pos = px_pos.with_offsets(g_rendering_px_offset);

        SDL_Rect render_rect;

        render_rect.x = px_pos.x;
        render_rect.y = px_pos.y;
        render_rect.w = map_cell_px_dims.x;
        render_rect.h = map_cell_px_dims.y;

        SDL_Texture* texture = nullptr;

        if ((obj.color == colors::black()) ||
            (obj.bg_color == colors::black())) {
                // Foreground or background is black - no contours
                texture = g_tile_textures[(size_t)obj.tile];
        }
        else {
                // Both foreground and background are non-black - use contours
                texture = g_tile_textures_with_contours[(size_t)obj.tile];
        }

        const Color color = obj.color.with_brightness(config::brightness_pct());

        SDL_SetTextureColorMod(texture, color.r(), color.g(), color.b());

        SDL_RenderCopy(g_sdl_renderer, texture, nullptr, &render_rect);
}

void cover_panel(const Panel panel, const Color& color)
{
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

        // * Now apply offset and scaling *

        // Scaling
        const int scale_factor = config::video_scale_factor();

        px_pos = px_pos.scaled_up(scale_factor);
        img_px_dims = img_px_dims.scaled_up(scale_factor);

        // Apply rendering offsets (to center the graphics in the window)
        px_pos = px_pos.with_offsets(g_rendering_px_offset);

        SDL_Rect render_rect;

        render_rect.x = px_pos.x;
        render_rect.y = px_pos.y;
        render_rect.w = img_px_dims.x;
        render_rect.h = img_px_dims.y;

        const int mod_value = std::min(255, (config::brightness_pct() * 255) / 100);

        SDL_SetTextureColorMod(g_logo_texture, mod_value, mod_value, mod_value);

        SDL_RenderCopy(g_sdl_renderer, g_logo_texture, nullptr, &render_rect);
}

std::string sdl_pref_dir()
{
        TRACE_FUNC_BEGIN;

        std::string subdir_str = version_info::g_version_str;

        std::replace(std::begin(subdir_str), std::end(subdir_str), '.', '_');
        std::replace(std::begin(subdir_str), std::end(subdir_str), '-', '_');

        const auto sha1_result = version_info::read_git_sha1_str_from_file();

        if (sha1_result) {
                subdir_str += "_" + sha1_result.value();
        }

        char* const path_ptr =
                // NOTE: This is somewhat of a hack, see the function arguments.
                SDL_GetPrefPath(
                        "infra_arcana",       // "Organization"
                        subdir_str.c_str());  // "Application"

        std::string path_str = path_ptr;

        SDL_free(path_ptr);

        TRACE << "SDL_GetPrefPath returned path '" << path_str << "'" << std::endl;

        TRACE_FUNC_END;

        return path_str;
}

void sleep(const uint32_t duration)
{
        if ((duration == 0) || config::is_bot_playing()) {
                return;
        }
        else if (duration == 1) {
                SDL_Delay(duration);
        }
        else {
                // Duration longer than 1 ms
                const auto wait_until = SDL_GetTicks() + duration;

                while (SDL_GetTicks() < wait_until) {
                        SDL_PumpEvents();
                }
        }
}

}  // namespace io

// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
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
#include "io_display.hpp"
#include "io_icons.hpp"
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

// -----------------------------------------------------------------------------
// Image preparation
//
// The tile atlas and the font image are recolored and contoured at startup.
// Both passes used to run through read_px_on_surface / put_px_on_surface -
// a function call, a switch on the pixel size and an SDL_GetRGB or
// SDL_MapRGB, for every pixel and every one of its eight neighbours. Over
// the atlas and the 2048x32 font image that is around one and a half
// million of them, which is real time on a phone or tablet CPU, spent
// before the game shows anything.
//
// The surfaces are converted to one known 32 bit format up front (see
// load_surface_as_rgba32) so both passes can address pixels directly.
// Comparisons mask the alpha channel off, matching the old behaviour -
// SDL_GetRGB ignored alpha, and SDL_MapRGB wrote it fully opaque.
// -----------------------------------------------------------------------------
static uint32_t rgb_mask_of(const SDL_Surface& surface)
{
        return ~surface.format->Amask;
}

static uint32_t map_color(const SDL_Surface& surface, const Color& color)
{
        return SDL_MapRGB(
                surface.format,
                color.r(),
                color.g(),
                color.b());
}

// Loads an image, converted to a known 32 bit pixel format
static SDL_Surface* load_surface_as_rgba32(const std::string& path)
{
        auto* const surface = load_surface(path);

        if (surface->format->format == SDL_PIXELFORMAT_RGBA32) {
                return surface;
        }

        auto* const converted =
                SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGBA32, 0);

        SDL_FreeSurface(surface);

        if (!converted) {
                TRACE_ERROR_RELEASE
                        << "Failed to convert surface from path '"
                        << path
                        << "': "
                        << SDL_GetError()
                        << std::endl;

                PANIC;
        }

        return converted;
}

static void swap_surface_color(
        SDL_Surface& surface,
        const Color& color_before,
        const Color& color_after)
{
        ASSERT(surface.format->BytesPerPixel == 4);

        const uint32_t rgb_mask = rgb_mask_of(surface);

        const uint32_t before = map_color(surface, color_before) & rgb_mask;

        // NOTE: Not masked - as SDL_MapRGB did, this writes opaque alpha
        const uint32_t after = map_color(surface, color_after);

        SDL_LockSurface(&surface);

        auto* const pixels = (uint32_t*)surface.pixels;

        const int stride = surface.pitch / 4;

        for (int y = 0; y < surface.h; ++y) {
                uint32_t* const row = pixels + ((size_t)y * (size_t)stride);

                for (int x = 0; x < surface.w; ++x) {
                        if ((row[x] & rgb_mask) == before) {
                                row[x] = after;
                        }
                }
        }

        SDL_UnlockSurface(&surface);
}

// -----------------------------------------------------------------------------
// Tile atlas
//
// Every tile image is packed into ONE texture, and drawn with a source
// rectangle into it. SDL's renderer can only merge consecutive draws that use
// the same texture (see GLES2_RunCommandQueue) - with a texture per tile, as
// this used to have, every single map cell cost its own draw call and its own
// shader/texture bind. Out of one atlas the whole map merges into a single
// draw call.
//
// Cells are padded by one pixel of background, so that nothing of a neighbour
// can bleed in when the atlas is sampled.
// -----------------------------------------------------------------------------
static const int s_atlas_nr_cols = 16;

static const int s_atlas_cell_pad_px = 1;

static int atlas_cell_px_size()
{
        return config::g_tile_img_px + (s_atlas_cell_pad_px * 2);
}

static int atlas_nr_rows()
{
        const int nr_tiles = (int)gfx::TileId::END;

        return ((nr_tiles + s_atlas_nr_cols - 1) / s_atlas_nr_cols);
}

// Where a tile's image starts in the atlas (inside its padding)
static P atlas_tile_px_pos(const gfx::TileId id)
{
        const int i = (int)id;

        return {
                ((i % s_atlas_nr_cols) * atlas_cell_px_size()) +
                        s_atlas_cell_pad_px,
                ((i / s_atlas_nr_cols) * atlas_cell_px_size()) +
                        s_atlas_cell_pad_px};
}

// Contours a single image within a surface. Only pixels inside the given
// rectangle are read, so that images packed side by side in an atlas contour
// exactly as they would on their own.
//
// A background pixel gets a contour if any of its eight neighbours holds
// something that will actually be drawn - i.e. neither the background nor a
// contour. Writing in place is safe: a contour pixel is never background
// afterwards, so it is never revisited, and the neighbour test excludes the
// contour color, so a contour never seeds another one.
static void draw_black_contour_in_rect(
        SDL_Surface& surface,
        const R& surface_px_rect,
        const Color& bg_color)
{
        ASSERT(surface.format->BytesPerPixel == 4);

        const uint32_t rgb_mask = rgb_mask_of(surface);

        const uint32_t bg = map_color(surface, bg_color) & rgb_mask;

        const uint32_t contour = map_color(surface, colors::black());

        const uint32_t contour_rgb = contour & rgb_mask;

        SDL_LockSurface(&surface);

        auto* const pixels = (uint32_t*)surface.pixels;

        const int stride = surface.pitch / 4;

        for (int y = surface_px_rect.p0.y; y <= surface_px_rect.p1.y; ++y) {
                const int adj_y0 = std::max(surface_px_rect.p0.y, y - 1);
                const int adj_y1 = std::min(surface_px_rect.p1.y, y + 1);

                uint32_t* const row = pixels + ((size_t)y * (size_t)stride);

                for (int x = surface_px_rect.p0.x;
                     x <= surface_px_rect.p1.x;
                     ++x) {
                        if ((row[x] & rgb_mask) != bg) {
#ifndef NDEBUG
                                // The art is grayscale on the background
                                // color - anything else means the image (or
                                // the recolor pass before this) is wrong
                                uint8_t r;
                                uint8_t g;
                                uint8_t b;

                                SDL_GetRGB(row[x], surface.format, &r, &g, &b);

                                if ((r != g) || (r != b)) {
                                        TRACE
                                                << "Found color other than "
                                                << "grayscale color: "
                                                << (int)r << ","
                                                << (int)g << ","
                                                << (int)b
                                                << " - at position: "
                                                << x << "x" << y
                                                << std::endl;

                                        PANIC;
                                }
#endif  // NDEBUG

                                continue;
                        }

                        const int adj_x0 =
                                std::max(surface_px_rect.p0.x, x - 1);

                        const int adj_x1 =
                                std::min(surface_px_rect.p1.x, x + 1);

                        bool has_drawn_neighbour = false;

                        for (int adj_y = adj_y0;
                             (adj_y <= adj_y1) && !has_drawn_neighbour;
                             ++adj_y) {
                                const uint32_t* const adj_row =
                                        pixels +
                                        ((size_t)adj_y * (size_t)stride);

                                for (int adj_x = adj_x0;
                                     adj_x <= adj_x1;
                                     ++adj_x) {
                                        if ((adj_x == x) && (adj_y == y)) {
                                                continue;
                                        }

                                        const uint32_t adj =
                                                adj_row[adj_x] & rgb_mask;

                                        if ((adj != bg) &&
                                            (adj != contour_rgb)) {
                                                has_drawn_neighbour = true;

                                                break;
                                        }
                                }
                        }

                        if (has_drawn_neighbour) {
                                row[x] = contour;
                        }
                }
        }

        SDL_UnlockSurface(&surface);
}

static void draw_black_contour_for_surface(
        SDL_Surface& surface,
        const Color& bg_color)
{
        const R surface_px_rect({0, 0}, {surface.w - 1, surface.h - 1});

        draw_black_contour_in_rect(surface, surface_px_rect, bg_color);
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

static void verify_surface_size(
        const SDL_Surface& surface,
        const P& expected_size,
        const std::string& img_path)
{
        if ((surface.w != expected_size.x) || (surface.h != expected_size.y)) {
                TRACE_ERROR_RELEASE
                        << "Tile image at \""
                        << img_path
                        << "\" has wrong size: "
                        << surface.w
                        << "x"
                        << surface.h
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

        // NOTE: A known 32 bit format - the recolor and contour passes below
        // address the pixels directly
        SDL_Surface* const surface = load_surface_as_rgba32(img_path);

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

// Blits one tile image into its cell of the atlas surface.
static void blit_tile_into_atlas(
        const gfx::TileId id,
        SDL_Surface& atlas,
        const P& img_px_dims)
{
        const std::string img_path =
                paths::tiles_dir() + gfx::tile_id_to_filename(id);

        SDL_Surface* const surface = load_surface(img_path);

        verify_surface_size(*surface, img_px_dims, img_path);

        verify_tile_colors(*surface, img_path);

        // A straight copy - the atlas background must not blend through
        SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_NONE);

        const P tile_px_pos = atlas_tile_px_pos(id);

        SDL_Rect dst_rect {
                tile_px_pos.x,
                tile_px_pos.y,
                img_px_dims.x,
                img_px_dims.y};

        SDL_BlitSurface(surface, nullptr, &atlas, &dst_rect);

        SDL_FreeSurface(surface);
}

static void load_tiles()
{
        TRACE_FUNC_BEGIN;

        // NOTE: The source images always have this size - when the map is
        // drawn scaled up, the tiles are stretched at draw time (see
        // draw_tile, which uses the logical map cell size).
        const P img_px_dims(
                config::g_tile_img_px,
                config::g_tile_img_px);

        const P atlas_px_dims(
                s_atlas_nr_cols * atlas_cell_px_size(),
                atlas_nr_rows() * atlas_cell_px_size());

        TRACE
                << "Building tile atlas of "
                << atlas_px_dims.x << "x" << atlas_px_dims.y
                << " pixels for "
                << (int)gfx::TileId::END << " tiles"
                << std::endl;

        SDL_Surface* const atlas =
                SDL_CreateRGBSurfaceWithFormat(
                        0,
                        atlas_px_dims.x,
                        atlas_px_dims.y,
                        32,
                        SDL_PIXELFORMAT_RGBA32);

        if (!atlas) {
                TRACE_ERROR_RELEASE
                        << "Failed to create tile atlas surface: "
                        << SDL_GetError()
                        << std::endl;

                PANIC;
        }

        // The whole atlas - the cell padding included - starts out as the
        // color that is keyed away, so that padding never draws
        const auto& bg = colors::magenta();

        SDL_FillRect(
                atlas,
                nullptr,
                SDL_MapRGBA(atlas->format, bg.r(), bg.g(), bg.b(), 0xFFU));

        for (size_t i = 0; i < (size_t)gfx::TileId::END; ++i) {
                blit_tile_into_atlas((gfx::TileId)i, *atlas, img_px_dims);
        }

        // The tile art is black on white; black becomes the keyed-away
        // background (as it is for the font image)
        swap_surface_color(*atlas, colors::black(), bg);

        set_surface_color_key(*atlas, bg);

        io::g_tile_atlas = create_texture_from_surface(*atlas);

        // The contoured variant - each tile is contoured within its own cell
        // rectangle, so no tile can see its neighbours
        for (size_t i = 0; i < (size_t)gfx::TileId::END; ++i) {
                const P tile_px_pos = atlas_tile_px_pos((gfx::TileId)i);

                const R tile_px_rect(
                        tile_px_pos,
                        tile_px_pos + img_px_dims - 1);

                draw_black_contour_in_rect(*atlas, tile_px_rect, bg);
        }

        io::g_tile_atlas_with_contours = create_texture_from_surface(*atlas);

        SDL_FreeSurface(atlas);

        TRACE_FUNC_END;
}

// -----------------------------------------------------------------------------
// io
// -----------------------------------------------------------------------------
namespace io
{
SDL_Texture* g_font_texture_with_contours = nullptr;
SDL_Texture* g_font_texture = nullptr;
SDL_Texture* g_tile_atlas = nullptr;
SDL_Texture* g_tile_atlas_with_contours = nullptr;
SDL_Texture* g_logo_texture = nullptr;

R tile_atlas_px_rect(const gfx::TileId tile)
{
        const P p0 = atlas_tile_px_pos(tile);

        const P dims(config::g_tile_img_px, config::g_tile_img_px);

        return {p0, p0 + dims - 1};
}

P g_rendering_px_offset = {};

void init_sdl()
{
        TRACE_FUNC_BEGIN;

        cleanup_sdl();

        // The touch translation layer (io_input.cpp) owns all finger events -
        // do not synthesize mouse events from touches.
        SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");

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

        init_displays();

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

        cleanup_icons();

        cleanup_displays();

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

void set_clip_rect_px(const Panel panel, R px_area)
{
        set_display_for_panel(panel);

        px_area = px_area.with_offset(current_display_draw_px_offset());

        const SDL_Rect clip_rect {
                px_area.p0.x,
                px_area.p0.y,
                px_area.w(),
                px_area.h()};

        SDL_RenderSetClipRect(g_sdl_renderer, &clip_rect);
}

void set_clip_rect_to_panel(const Panel panel)
{
        auto px_area = gui_to_px_rect(panels::area(panel));

        if (panel == Panel::map) {
                // The map display renders one cell of overscan beyond each
                // panel edge, for smooth sub-cell scrolling (see io_display)
                const P overscan(
                        config::map_cell_px_w(),
                        config::map_cell_px_h());

                px_area.p0 = px_area.p0 - overscan;
                px_area.p1 = px_area.p1 + overscan;
        }

        set_clip_rect_px(panel, px_area);
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

        // Set up the texture clip rectangle
        // NOTE: We expect one pixel separator between each glyph.
        auto char_px_pos = gfx::character_pos(character);

        char_px_pos.x *= (gui_cell_px_dims.x + 1);
        char_px_pos.y *= (gui_cell_px_dims.y);

        SDL_Rect clip_rect;

        clip_rect.x = char_px_pos.x;
        clip_rect.y = char_px_pos.y;
        clip_rect.w = gui_cell_px_dims.x;
        clip_rect.h = gui_cell_px_dims.y;

        // NOTE: Drawing happens at logical resolution - video scaling and
        // window centering are applied when compositing the display textures
        // to the window (see io_display.cpp).
        px_pos = px_pos.with_offsets(current_display_draw_px_offset());

        SDL_Rect render_rect;

        render_rect.x = px_pos.x;
        render_rect.y = px_pos.y;
        render_rect.w = gui_cell_px_dims.x;
        render_rect.h = gui_cell_px_dims.y;

        mark_current_display_used(
                {px_pos, px_pos + gui_cell_px_dims - 1});

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
        set_display_for_panel(obj.panel);

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

void draw_tile_at_px(
        const gfx::TileId tile,
        const P& px_pos,
        const Color& color,
        const Color& bg_color)
{
        const P map_cell_px_dims(
                config::map_cell_px_w(),
                config::map_cell_px_h());

        const P offset_px_pos =
                px_pos.with_offsets(current_display_draw_px_offset());

        const SDL_Rect render_rect {
                offset_px_pos.x,
                offset_px_pos.y,
                map_cell_px_dims.x,
                map_cell_px_dims.y};

        mark_current_display_used(
                {offset_px_pos, offset_px_pos + map_cell_px_dims - 1});

        SDL_Texture* texture = nullptr;

        if ((color == colors::black()) || (bg_color == colors::black())) {
                // Foreground or background is black - no contours
                texture = g_tile_atlas;
        }
        else {
                // Both foreground and background are non-black - use contours
                texture = g_tile_atlas_with_contours;
        }

        const auto src = tile_atlas_px_rect(tile);

        const SDL_Rect src_rect {
                src.p0.x,
                src.p0.y,
                src.w(),
                src.h()};

        const Color color_adapted =
                color.with_brightness(config::brightness_pct());

        SDL_SetTextureColorMod(
                texture,
                color_adapted.r(),
                color_adapted.g(),
                color_adapted.b());

        SDL_RenderCopy(g_sdl_renderer, texture, &src_rect, &render_rect);
}

void draw_tile(const TileDrawObj& obj)
{
        set_display_for_panel(obj.panel);

        const P px_pos = map_to_px_coords(obj.panel, obj.pos);

        if (obj.draw_bg == DrawBg::yes) {
                const P map_cell_px_dims(
                        config::map_cell_px_w(),
                        config::map_cell_px_h());

                // NOTE: The rectangle function applies the display draw
                // offset itself
                draw_rectangle_filled(
                        {px_pos, px_pos + map_cell_px_dims - 1},
                        obj.bg_color);
        }

        draw_tile_at_px(obj.tile, px_pos, obj.color, obj.bg_color);
}

void cover_panel(const Panel panel, const Color& color)
{
        set_display_for_panel(panel);

        const auto px_area = gui_to_px_rect(panels::area(panel));

        draw_rectangle_filled(px_area, color);
}

void cover_area(
        const Panel panel,
        const R& area,
        const Color& color)
{
        set_display_for_panel(panel);

        const auto panel_p0 = panels::p0(panel);

        const auto screen_area = area.with_offset(panel_p0);

        const auto px_area = gui_to_px_rect(screen_area);

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
        set_display(Display::screen);

        const int screen_px_w = panel_px_w(Panel::screen);

        P img_px_dims;

        SDL_QueryTexture(
                g_logo_texture,
                nullptr,
                nullptr,
                &img_px_dims.x,
                &img_px_dims.y);

        const P px_pos((screen_px_w - img_px_dims.x) / 2, 0);

        SDL_Rect render_rect;

        render_rect.x = px_pos.x;
        render_rect.y = px_pos.y;
        render_rect.w = img_px_dims.x;
        render_rect.h = img_px_dims.y;

        mark_current_display_used({px_pos, px_pos + img_px_dims - 1});

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

                        // A map shake animates through any wait - notably
                        // the blast animation's own delay, which is exactly
                        // when an explosion should be shaking the ground.
                        // Stepping it is a composite and a present, the
                        // drawn frame is reused as it stands.
                        if (step_map_shake()) {
                                update_screen();
                        }
                }
        }
}

}  // namespace io

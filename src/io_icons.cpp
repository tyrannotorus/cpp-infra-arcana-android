// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "io_icons.hpp"

#include <algorithm>
#include <cstdint>
#include <map>
#include <ostream>
#include <set>
#include <string>

#include "SDL_image.h"
#include "SDL_render.h"
#include "SDL_rwops.h"
#include "SDL_surface.h"
#include "config.hpp"
#include "debug.hpp"
#include "io.hpp"
#include "io_display.hpp"
#include "io_internal.hpp"
#include "paths.hpp"
#include "pos.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------

// Rasterized icon textures, keyed by "name#size"
static std::map<std::string, SDL_Texture*> s_icon_textures;

// Icons that failed to load (logged once, then silently skipped)
static std::set<std::string> s_failed_icons;

// Sets all pixels to white, preserving the alpha channel, so that the
// texture can be tinted with SDL_SetTextureColorMod (like the tile
// graphics, which are white on transparent).
static void whiten_surface(SDL_Surface& surface)
{
        SDL_LockSurface(&surface);

        auto* const pixels = (uint32_t*)surface.pixels;

        const auto* const format = surface.format;

        const int nr_pixels = (surface.pitch / 4) * surface.h;

        for (int i = 0; i < nr_pixels; ++i) {
                uint8_t r;
                uint8_t g;
                uint8_t b;
                uint8_t a;

                SDL_GetRGBA(pixels[i], format, &r, &g, &b, &a);

                pixels[i] = SDL_MapRGBA(format, 0xFF, 0xFF, 0xFF, a);
        }

        SDL_UnlockSurface(&surface);
}

static SDL_Texture* load_icon_texture(
        const std::string& name,
        const int px_size)
{
        const std::string path = paths::icons_dir() + name + ".svg";

        auto* const rw = SDL_RWFromFile(path.c_str(), "rb");

        if (!rw) {
                return nullptr;
        }

        auto* const surface = IMG_LoadSizedSVG_RW(rw, px_size, px_size);

        SDL_RWclose(rw);

        if (!surface) {
                return nullptr;
        }

        // Ensure a known 32 bit format for the whitening pass
        auto* const converted =
                SDL_ConvertSurfaceFormat(
                        surface,
                        SDL_PIXELFORMAT_ARGB8888,
                        0);

        SDL_FreeSurface(surface);

        if (!converted) {
                return nullptr;
        }

        whiten_surface(*converted);

        auto* const texture =
                SDL_CreateTextureFromSurface(io::g_sdl_renderer, converted);

        SDL_FreeSurface(converted);

        if (texture) {
                SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);

                // Chunky pixels when stretched (match the tile art)
                SDL_SetTextureScaleMode(texture, SDL_ScaleModeNearest);
        }

        return texture;
}

// -----------------------------------------------------------------------------
// io
// -----------------------------------------------------------------------------
namespace io
{
void draw_icon(
        const std::string& name,
        const P& center_px,
        const int px_size,
        const Color& color,
        const double angle)
{
        // Rasterize the vector art at exactly the size it is drawn at, so
        // that the icon is crisp.
        //
        // NOTE: This must NOT be tied to the map scale factor ("Game
        // scale"). Icons are interface furniture - the action bar and the
        // actions configuration screen - and they live in gui cells, not
        // in map cells. Rasterizing them at map density and stretching the
        // result up (as was done before) blurred every button as soon as
        // the game world was zoomed to 2x or 3x, for no reason at all.
        const int raster_size = std::max(8, px_size);

        const std::string key = name + "#" + std::to_string(raster_size);

        auto it = s_icon_textures.find(key);

        if (it == s_icon_textures.end()) {
                if (s_failed_icons.count(key) > 0) {
                        return;
                }

                auto* const texture = load_icon_texture(name, raster_size);

                if (!texture) {
                        TRACE_ERROR_RELEASE
                                << "Failed to load icon '"
                                << name
                                << "'"
                                << std::endl;

                        s_failed_icons.insert(key);

                        return;
                }

                it = s_icon_textures.emplace(key, texture).first;
        }

        auto* const texture = it->second;

        const auto px_pos =
                center_px
                        .with_offsets(-px_size / 2, -px_size / 2)
                        .with_offsets(current_display_draw_px_offset());

        const SDL_Rect render_rect {
                px_pos.x,
                px_pos.y,
                px_size,
                px_size};

        mark_current_display_used(
                {px_pos, px_pos.with_offsets(px_size - 1, px_size - 1)});

        const auto color_adapted =
                color.with_brightness(config::brightness_pct());

        SDL_SetTextureColorMod(
                texture,
                color_adapted.r(),
                color_adapted.g(),
                color_adapted.b());

        if (angle == 0.0) {
                SDL_RenderCopy(g_sdl_renderer, texture, nullptr, &render_rect);

                return;
        }

        // Turned about the destination rectangle's own center (the default
        // pivot), so the glyph stays where it was placed
        SDL_RenderCopyEx(
                g_sdl_renderer,
                texture,
                nullptr,
                &render_rect,
                angle,
                nullptr,
                SDL_FLIP_NONE);
}

void cleanup_icons()
{
        for (auto& entry : s_icon_textures) {
                SDL_DestroyTexture(entry.second);
        }

        s_icon_textures.clear();

        s_failed_icons.clear();
}

}  // namespace io

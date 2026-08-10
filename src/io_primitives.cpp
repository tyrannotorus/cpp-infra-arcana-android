// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "io.hpp"

#include <cstdint>

#include "SDL_blendmode.h"
#include "SDL_rect.h"
#include "SDL_render.h"
#include "colors.hpp"
#include "config.hpp"
#include "io_display.hpp"
#include "io_internal.hpp"
#include "pos.hpp"
#include "rect.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// io
// -----------------------------------------------------------------------------
namespace io
{
void draw_rectangle(R px_rect, const Color& color)
{
        // NOTE: Drawing happens at logical resolution - video scaling is
        // applied when compositing the display textures to the window, which
        // also scales up the rectangle line thickness.
        px_rect = px_rect.with_offset(current_display_draw_px_offset());

        mark_current_display_used(px_rect);

        const SDL_Rect rect {
                px_rect.p0.x,
                px_rect.p0.y,
                px_rect.w(),
                px_rect.h()};

        const Color color_adapted = color.with_brightness(config::brightness_pct());

        SDL_SetRenderDrawColor(
                g_sdl_renderer,
                color_adapted.r(),
                color_adapted.g(),
                color_adapted.b(),
                0xFFU);

        SDL_RenderDrawRect(g_sdl_renderer, &rect);
}

void draw_rectangle_filled(
        R px_rect,
        const Color& color,
        const uint8_t alpha)
{
        px_rect = px_rect.with_offset(current_display_draw_px_offset());

        mark_current_display_used(px_rect);

        const SDL_Rect rect {
                px_rect.p0.x,
                px_rect.p0.y,
                px_rect.w(),
                px_rect.h()};

        const Color color_adapted = color.with_brightness(config::brightness_pct());

        SDL_SetRenderDrawColor(
                g_sdl_renderer,
                color_adapted.r(),
                color_adapted.g(),
                color_adapted.b(),
                alpha);

        SDL_RenderFillRect(g_sdl_renderer, &rect);
}

void draw_rectangles_filled(
        const std::vector<R>& px_rects,
        const Color& color,
        const uint8_t alpha)
{
        if (px_rects.empty()) {
                return;
        }

        const P draw_offset = current_display_draw_px_offset();

        std::vector<SDL_Rect> sdl_rects;

        sdl_rects.reserve(px_rects.size());

        for (const R& px_rect : px_rects) {
                const R offset_rect = px_rect.with_offset(draw_offset);

                mark_current_display_used(offset_rect);

                sdl_rects.push_back(
                        SDL_Rect {
                                offset_rect.p0.x,
                                offset_rect.p0.y,
                                offset_rect.w(),
                                offset_rect.h()});
        }

        const Color color_adapted = color.with_brightness(config::brightness_pct());

        SDL_SetRenderDrawColor(
                g_sdl_renderer,
                color_adapted.r(),
                color_adapted.g(),
                color_adapted.b(),
                alpha);

        SDL_RenderFillRects(
                g_sdl_renderer,
                sdl_rects.data(),
                (int)sdl_rects.size());
}

void draw_rectangle_filled_mod_blending(
        R px_rect,
        const Color& color,
        uint8_t alpha)
{
        SDL_SetRenderDrawBlendMode(io::g_sdl_renderer, SDL_BLENDMODE_MOD);

        draw_rectangle_filled(px_rect, color, alpha);

        SDL_SetRenderDrawBlendMode(io::g_sdl_renderer, SDL_BLENDMODE_BLEND);
}

}  // namespace io

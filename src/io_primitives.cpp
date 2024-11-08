// =============================================================================
// Copyright 2011-2024 Martin Törnqvist <m.tornq@gmail.com>
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
        // NOTE: To handle graphics scaling, we draw extra inner rectangles -
        // this is somewhat hacky, but it fulfills the purpose...
        int nr_rects = 1;

        const int scale_factor = config::video_scale_factor();

        px_rect = px_rect.scaled_up(scale_factor);
        nr_rects = scale_factor;

        px_rect = px_rect.with_offset(g_rendering_px_offset);

        for (int i = 0; i < nr_rects; ++i) {
                SDL_Rect rect;

                rect.x = px_rect.p0.x;
                rect.y = px_rect.p0.y;
                rect.w = px_rect.w();
                rect.h = px_rect.h();

                const Color color_adapted = color.with_brightness(config::brightness_pct());

                SDL_SetRenderDrawColor(
                        g_sdl_renderer,
                        color_adapted.r(),
                        color_adapted.g(),
                        color_adapted.b(),
                        0xFFU);

                SDL_RenderDrawRect(g_sdl_renderer, &rect);

                px_rect.p0 = px_rect.p0 + 1;
                px_rect.p1 = px_rect.p1 - 1;
        }
}

void draw_rectangle_filled(
        R px_rect,
        const Color& color,
        const uint8_t alpha)
{
        px_rect = px_rect.scaled_up(config::video_scale_factor());

        px_rect = px_rect.with_offset(g_rendering_px_offset);

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

// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
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
        // NOTE: To handle graphics scaling, we draw an extra inner rectangle -
        // this is somewhat hacky, but it fulfills the purpose...
        int nr_rects = 1;

        if (config::is_2x_scale_enabled()) {
                px_rect = px_rect.scaled_up(2);

                nr_rects = 2;
        }

        px_rect = px_rect.with_offset(g_rendering_px_offset);

        for (int i = 0; i < nr_rects; ++i) {
                SDL_Rect rect;

                rect.x = px_rect.p0.x;
                rect.y = px_rect.p0.y;
                rect.w = px_rect.w();
                rect.h = px_rect.h();

                SDL_SetRenderDrawColor(
                        g_sdl_renderer,
                        color.r(),
                        color.g(),
                        color.b(),
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
        if (config::is_2x_scale_enabled()) {
                px_rect = px_rect.scaled_up(2);
        }

        px_rect = px_rect.with_offset(g_rendering_px_offset);

        const SDL_Rect rect {
                px_rect.p0.x,
                px_rect.p0.y,
                px_rect.w(),
                px_rect.h()};

        SDL_SetRenderDrawColor(
                g_sdl_renderer,
                color.r(),
                color.g(),
                color.b(),
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

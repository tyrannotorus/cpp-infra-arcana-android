// =============================================================================
// Copyright 2011-2021 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "SDL_rect.h"
#include "SDL_render.h"
#include "colors.hpp"
#include "config.hpp"
#include "io.hpp"
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

        if (config::is_fullscreen() &&
            config::is_2x_scale_fullscreen_enabled())
        {
                px_rect = px_rect.scaled_up(2);

                nr_rects = 2;
        }

        px_rect = px_rect.with_offset(g_rendering_px_offset);

        for (int i = 0; i < nr_rects; ++i)
        {
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

void draw_rectangle_filled(R px_rect, const Color& color)
{
        if (config::is_fullscreen() &&
            config::is_2x_scale_fullscreen_enabled())
        {
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
                0xFFU);

        SDL_RenderFillRect(g_sdl_renderer, &rect);
}

}  // namespace io

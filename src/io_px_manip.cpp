// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include <cstdint>
#include <ostream>

#include "SDL_endian.h"
#include "SDL_pixels.h"
#include "SDL_stdinc.h"
#include "SDL_surface.h"
#include "colors.hpp"
#include "debug.hpp"
#include "io_internal.hpp"
#include "pos.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static void put_px8(
        const SDL_Surface& surface,
        const P& px_pos,
        const Uint32 px)
{
        // p is the address to the pixel we want to set
        auto* const p =
                (Uint8*)surface.pixels +
                (px_pos.y * surface.pitch) +
                (px_pos.x * surface.format->BytesPerPixel);

        *p = px;
}

static void put_px16(
        const SDL_Surface& surface,
        const P& px_pos,
        const Uint32 px)
{
        // p is the address to the pixel we want to set
        auto* const p =
                (Uint8*)surface.pixels +
                (px_pos.y * surface.pitch) +
                (px_pos.x * surface.format->BytesPerPixel);

        *(Uint16*)p = px;
}

static void put_px24(
        const SDL_Surface& surface,
        const P& px_pos,
        const Uint32 px)
{
        // p is the address to the pixel we want to set
        auto* const p =
                (Uint8*)surface.pixels +
                (px_pos.y * surface.pitch) +
                (px_pos.x * surface.format->BytesPerPixel);

        if (SDL_BYTEORDER == SDL_BIG_ENDIAN)
        {
                p[0] = (px >> 16) & 0xff;
                p[1] = (px >> 8) & 0xff;
                p[2] = px & 0xff;
        }
        else
        {
                // Little endian
                p[0] = px & 0xff;
                p[1] = (px >> 8) & 0xff;
                p[2] = (px >> 16) & 0xff;
        }
}

static void put_px32(
        const SDL_Surface& surface,
        const P& px_pos,
        const Uint32 px)
{
        // p is the address to the pixel we want to set
        auto* const p =
                (Uint8*)surface.pixels +
                (px_pos.y * surface.pitch) +
                (px_pos.x * surface.format->BytesPerPixel);

        *(Uint32*)p = px;
}

// -----------------------------------------------------------------------------
// io
// -----------------------------------------------------------------------------
namespace io
{
Color read_px_on_surface(const SDL_Surface& surface, const P& px_pos)
{
        // 'p' is the address to the pixel we want to retrieve.
        Uint8* p =
                (Uint8*)surface.pixels +
                (px_pos.y * surface.pitch) +
                (px_pos.x * surface.format->BytesPerPixel);

        int v = 0;

        switch (surface.format->BytesPerPixel)
        {
        case 1:
                v = *p;
                break;

        case 2:
                v = *(Uint16*)p;
                break;

        case 3:
                if (SDL_BYTEORDER == SDL_BIG_ENDIAN)
                {
                        v = p[0] << 16 | p[1] << 8 | p[2];
                }
                else
                {
                        // Little endian
                        v = p[0] | p[1] << 8 | p[2] << 16;
                }
                break;

        case 4:
                v = *(uint32_t*)p;
                break;

        default:
                TRACE_ERROR_RELEASE
                        << "Unexpected bpp: "
                        << (int)surface.format->BytesPerPixel
                        << std::endl;

                PANIC;

                v = 0;

                break;
        }

        SDL_Color sdl_color;

        SDL_GetRGB(
                v,
                surface.format,
                &sdl_color.r,
                &sdl_color.g,
                &sdl_color.b);

        return {sdl_color};
}

void put_px_on_surface(
        SDL_Surface& surface,
        const P& px_pos,
        const Color& color)
{
        const int v =
                SDL_MapRGB(
                        surface.format,
                        color.r(),
                        color.g(),
                        color.b());

        switch (surface.format->BytesPerPixel)
        {
        case 1:
                put_px8(surface, px_pos, v);
                break;

        case 2:
                put_px16(surface, px_pos, v);
                break;

        case 3:
                put_px24(surface, px_pos, v);
                break;

        case 4:
                put_px32(surface, px_pos, v);
                break;

        default:
                TRACE_ERROR_RELEASE
                        << "Unexpected bpp: "
                        << (int)surface.format->BytesPerPixel
                        << std::endl;

                PANIC;
                break;
        }
}

}  // namespace io

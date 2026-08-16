// =============================================================================
// Copyright Werewolf Camp
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "io_display.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <ostream>
#include <vector>

#include "SDL_render.h"
#include "SDL_timer.h"
#include "colors.hpp"
#include "config.hpp"
#include "context_pins.hpp"
#include "debug.hpp"
#include "dpad.hpp"
#include "easing.hpp"
#include "io.hpp"
#include "io_internal.hpp"
#include "panel.hpp"
#include "state.hpp"
#include "viewport.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static SDL_Texture* s_display_textures[(size_t)io::Display::END] = {};

static P s_display_px_offsets[(size_t)io::Display::END] = {};

// The screen area each display covers, in logical pixels - its texture's
// origin and size. Recomputed whenever the panels move (see init_displays).
static R s_display_px_rects[(size_t)io::Display::END] = {};

// Sub-cell scroll of the map content (see set_map_scroll_px_offset). The map
// display texture is rendered with one map cell of overscan beyond each
// screen edge (the render target viewport is shifted by one cell), so that
// the composited map window always has content when sliding between cells.
static P s_map_scroll_px = {0, 0};

// The map texture reserves one cell for the camera follow plus this much
// for a shake, so that a shake still has room to move while the camera is
// at full lag - which is exactly when the player takes a hit
static P map_overscan_px()
{
        const int shake = io::max_map_shake_px();

        return {
                config::map_cell_px_w() + shake,
                config::map_cell_px_h() + shake};
}

// Shake offset of the map (see start_map_shake), added to the scroll offset
static P s_map_shake_px = {0, 0};

// Map magnification and the point it is centered on (see set_map_zoom)
static float s_map_zoom = 1.0f;
static P s_map_zoom_center_px = {0, 0};

// Radial gradient for the vignette, built once on first use (see
// set_map_vignette). Small - it is always stretched, and a soft ramp
// survives that with linear filtering.
static SDL_Texture* s_vignette_texture = nullptr;
static const int s_vignette_texture_px = 512;

// How much of the gradient's radius is fully clear before it starts to
// darken. Small, so the ramp spans nearly the whole radius and the darkness
// has no edge to it.
static const float s_vignette_clear_frac = 0.05f;

// Spread of the gradient at full open, as a fraction of the screen's width
// plus height - large enough that none of its darkening is on screen yet
static const float s_vignette_open_span = 1.6f;

// Where the vignette is closing to, and how far (see set_map_vignette)
static P s_vignette_center_px = {0, 0};
static float s_vignette_open_frac = 1.0f;

// Multiplied over the map as it is composited (see set_map_tint). NOT
// initialized from colors::white() - the named colors load from xml long
// after a file scope static is constructed, so that would capture an
// unloaded (black) color.
static Color s_map_tint = Color(255, 255, 255);

// How far behind its drawn framing the camera is (see offset_map_follow),
// in logical pixels. Floating point - rounding each step would stall the
// last pixels of the ease.
static float s_map_follow_px_x = 0.0f;
static float s_map_follow_px_y = 0.0f;

// Fraction of the remaining distance covered in one reference frame. The
// alpha is rescaled from the real elapsed time, so the follow takes the same
// time at any refresh rate.
static const float s_map_follow_lerp = 0.18f;
static const float s_map_follow_reference_frame_ms = 16.67f;

// An exponential never arrives; below this the camera has caught up
static const float s_map_follow_min_px = 0.5f;

// Zero means the camera has just gone behind and the clock has not started
static uint32_t s_map_follow_last_step_ms = 0;

// Whole cells of the lag the map display was last drawn with
static P s_map_follow_drawn_cells = {0, 0};

// Whole cells of lag beyond the one cell the composite can carry
static int follow_whole_cells(const float lag_px, const int cell_px)
{
        if (cell_px <= 0) {
                return 0;
        }

        const float cell = (float)cell_px;

        if (lag_px > cell) {
                return (int)((lag_px - cell) / cell) + 1;
        }

        if (lag_px < -cell) {
                return -((int)((-lag_px - cell) / cell) + 1);
        }

        return 0;
}

// Eases the camera toward its framing. Called once per present, from the
// composite, so it advances through anything that puts frames on screen.
static void step_map_follow()
{
        if (!io::is_map_follow_active()) {
                s_map_follow_px_x = 0.0f;
                s_map_follow_px_y = 0.0f;

                s_map_follow_last_step_ms = 0;

                return;
        }

        const uint32_t now_ms = SDL_GetTicks();

        if (s_map_follow_last_step_ms == 0) {
                // This present shows the player on its new cell with the map
                // not yet caught up - the motion starts from the next one
                s_map_follow_last_step_ms = now_ms;

                return;
        }

        const uint32_t elapsed_ms = now_ms - s_map_follow_last_step_ms;

        if (elapsed_ms == 0) {
                // Leave the clock, so the time is not lost
                return;
        }

        s_map_follow_last_step_ms = now_ms;

        const float alpha =
                1.0f -
                std::pow(
                        1.0f - s_map_follow_lerp,
                        (float)elapsed_ms / s_map_follow_reference_frame_ms);

        // A long stall saturates the alpha, putting the camera where it
        // should have been by now
        const float remaining = 1.0f - std::min(alpha, 1.0f);

        s_map_follow_px_x *= remaining;
        s_map_follow_px_y *= remaining;

        if (!io::is_map_follow_active()) {
                s_map_follow_px_x = 0.0f;
                s_map_follow_px_y = 0.0f;

                s_map_follow_last_step_ms = 0;
        }
}

// A soft radial ramp - transparent in the middle, opaque black at the rim.
// Stretched to whatever size the vignette needs, so one small texture
// serves every stage of the close.
static SDL_Texture* vignette_texture()
{
        if (s_vignette_texture) {
                return s_vignette_texture;
        }

        const int dims = s_vignette_texture_px;

        std::vector<uint32_t> pixels((size_t)(dims * dims));

        const float half = (float)dims / 2.0f;

        for (int y = 0; y < dims; ++y) {
                for (int x = 0; x < dims; ++x) {
                        const float dx = ((float)x + 0.5f - half) / half;
                        const float dy = ((float)y + 0.5f - half) / half;

                        const float dist = std::sqrt((dx * dx) + (dy * dy));

                        const float t =
                                std::clamp(
                                        (dist - s_vignette_clear_frac) /
                                                (1.0f - s_vignette_clear_frac),
                                        0.0f,
                                        1.0f);

                        // Smoothstep - soft at both ends, so the darkness
                        // arrives without an edge anywhere along the ramp
                        const float a = t * t * (3.0f - (2.0f * t));

                        const auto alpha = (uint32_t)std::lround(a * 255.0f);

                        pixels[(size_t)((y * dims) + x)] = (alpha << 24);
                }
        }

        s_vignette_texture =
                SDL_CreateTexture(
                        io::g_sdl_renderer,
                        SDL_PIXELFORMAT_ARGB8888,
                        SDL_TEXTUREACCESS_STATIC,
                        dims,
                        dims);

        if (!s_vignette_texture) {
                TRACE_ERROR_RELEASE
                        << "Failed to create vignette texture: "
                        << SDL_GetError()
                        << std::endl;

                return nullptr;
        }

        SDL_UpdateTexture(
                s_vignette_texture,
                nullptr,
                pixels.data(),
                dims * (int)sizeof(uint32_t));

        SDL_SetTextureBlendMode(s_vignette_texture, SDL_BLENDMODE_BLEND);

        SDL_SetTextureScaleMode(s_vignette_texture, SDL_ScaleModeLinear);

        return s_vignette_texture;
}

// Darkness closing on the vignette's center, drawn straight onto the window
// over the map's composited area (see set_map_vignette). Window pixels - the
// map's destination rectangle and the video scale, not logical coordinates.
static void draw_map_vignette(const SDL_Rect& map_rect, const int scale)
{
        if (s_vignette_open_frac >= 1.0f) {
                return;
        }

        auto* const texture = vignette_texture();

        if (!texture) {
                return;
        }

        // Fully open spreads the gradient far beyond the map, so only its
        // clear middle is on show; closing shrinks it onto the center point
        const float span =
                (float)(map_rect.w + map_rect.h) *
                s_vignette_open_span *
                s_vignette_open_frac;

        const int half = (int)std::lround(span / 2.0f);

        const int center_x = map_rect.x + (s_vignette_center_px.x * scale);
        const int center_y = map_rect.y + (s_vignette_center_px.y * scale);

        const SDL_Rect gradient {
                center_x - half,
                center_y - half,
                half * 2,
                half * 2};

        // Everything the gradient does not reach is past its rim, i.e.
        // solid black. Four bands rather than a full screen blend beneath
        // the gradient.
        SDL_SetRenderDrawColor(io::g_sdl_renderer, 0U, 0U, 0U, 255U);

        const int gradient_x1 = gradient.x + gradient.w;
        const int gradient_y1 = gradient.y + gradient.h;

        const int map_x1 = map_rect.x + map_rect.w;
        const int map_y1 = map_rect.y + map_rect.h;

        const int band_y0 = std::max(map_rect.y, gradient.y);
        const int band_y1 = std::min(map_y1, gradient_y1);

        const SDL_Rect bands[] {
                {map_rect.x, map_rect.y, map_rect.w, gradient.y - map_rect.y},
                {map_rect.x, gradient_y1, map_rect.w, map_y1 - gradient_y1},
                {map_rect.x, band_y0, gradient.x - map_rect.x, band_y1 - band_y0},
                {gradient_x1, band_y0, map_x1 - gradient_x1, band_y1 - band_y0}};

        for (const auto& band : bands) {
                if ((band.w > 0) && (band.h > 0)) {
                        SDL_RenderFillRect(io::g_sdl_renderer, &band);
                }
        }

        SDL_RenderCopy(io::g_sdl_renderer, texture, nullptr, &gradient);
}

// Where the composite samples the map texture: the manual pan, the camera
// follow and the shake together. The overscan is all the displacement there
// is drawn content for - beyond it the copy would sample outside the
// texture.
static P map_src_offset_px()
{
        const auto overscan = map_overscan_px();

        // Only what the drawn origin did not take on
        const P follow(
                (int)std::lround(
                        s_map_follow_px_x -
                        (float)(s_map_follow_drawn_cells.x *
                                config::map_cell_px_w())),
                (int)std::lround(
                        s_map_follow_px_y -
                        (float)(s_map_follow_drawn_cells.y *
                                config::map_cell_px_h())));

        return {
                std::clamp(
                        s_map_scroll_px.x + follow.x + s_map_shake_px.x,
                        -overscan.x,
                        overscan.x),
                std::clamp(
                        s_map_scroll_px.y + follow.y + s_map_shake_px.y,
                        -overscan.y,
                        overscan.y)};
}

// The display that draw calls are currently routed to. END means unknown
// (e.g. after compositing, when the render target is the window).
static io::Display s_current_display = io::Display::END;

// What has been drawn into each display since the last clear: whether
// anything at all, and the bounding rectangle of it (see
// mark_current_display_used)
static bool s_is_display_used[(size_t)io::Display::END] = {};

static R s_display_used_px_rect[(size_t)io::Display::END] = {};

// Set when the textures are (re)created - they hold garbage then, so the
// first clear must cover all of them regardless of the used flags
static bool s_must_clear_all_displays = true;

// The screen area a display can cover. Everything drawn to it must land
// inside this - the texture is exactly this big, so anything outside is
// clipped away rather than merely composited somewhere odd.
static R display_logical_px_rect(const io::Display display)
{
        switch (display) {
        case io::Display::map:
                // NOTE: The map panel spans the whole screen; the overscan
                // ring is added to the texture on top of this (see
                // init_displays)
                return io::panel_logical_px_rect(Panel::map);

        case io::Display::log:
                return io::panel_logical_px_rect(Panel::log);

        case io::Display::side:
                return io::panel_logical_px_rect(
                        Panel::map_gui_stats_border);

        case io::Display::dpad:
                return dpad::display_px_rect();

        case io::Display::bar: {
                auto rect = io::panel_logical_px_rect(Panel::action_bar);

                // The context pin row is anchored to the top of the bar's
                // BUTTONS, which can sit above the bar panel - and the pins
                // are aligned to whichever side the buttons are on, so the
                // display spans the full screen width.
                const auto screen =
                        io::panel_logical_px_rect(Panel::screen);

                rect.p0.y -=
                        context_pins::rows_above_bar() *
                        config::gui_cell_px_h();

                rect.p0.x = screen.p0.x;
                rect.p1.x = screen.p1.x;

                return rect;
        }

        case io::Display::screen:
        case io::Display::overlay:
        case io::Display::END:
                break;
        }

        return io::panel_logical_px_rect(Panel::screen);
}

// -----------------------------------------------------------------------------
// io
// -----------------------------------------------------------------------------
namespace io
{
Display display_for_panel(const Panel panel)
{
        switch (panel) {
        case Panel::map:
                return Display::map;

        case Panel::log:
                return Display::log;

        case Panel::map_gui_stats:
        case Panel::map_gui_stats_border:
                return Display::side;

        case Panel::action_bar:
                return Display::bar;

        default:
                return Display::screen;
        }
}

void set_display(const Display display)
{
        ASSERT(display != Display::END);

        if (display == s_current_display) {
                return;
        }

        auto* const texture = s_display_textures[(size_t)display];

        // The display textures may not be set up yet (e.g. when drawing an
        // "intro" screen during initialization) - then draw directly to the
        // window, as before the display separation.
        SDL_SetRenderTarget(g_sdl_renderer, texture);

        s_current_display = display;
}

P current_display_draw_px_offset()
{
        if (s_current_display == Display::END) {
                return {0, 0};
        }

        // Drawing is written in SCREEN coordinates; a display's texture only
        // covers its own part of the screen, so its origin is subtracted
        // here to land in texture space.
        //
        // The map additionally has its content shifted by one cell of
        // overscan into its (correspondingly larger) texture, so that there
        // is renderable space on every side of the map panel for sub-cell
        // scrolling.
        //
        // NOTE: A render target viewport can NOT be used for either of these
        // - SDL clips drawing to the viewport rectangle, which would discard
        // the ring above and left of the panel (negative coordinates).
        P offset = P(0, 0) - s_display_px_rects[(size_t)s_current_display].p0;

        if (s_current_display == Display::map) {
                offset = offset + map_overscan_px();
        }

        return offset;
}

void set_display_for_panel(const Panel panel)
{
        set_display(display_for_panel(panel));
}

void init_displays()
{
        TRACE_FUNC_BEGIN;

        cleanup_displays();

        for (size_t i = 0; i < (size_t)Display::END; ++i) {
                const auto rect = display_logical_px_rect((Display)i);

                s_display_px_rects[i] = rect;

                // The map display carries one cell of overscan on every side
                auto tex_px_dims = rect.dims();

                if ((Display)i == Display::map) {
                        tex_px_dims += map_overscan_px().scaled_up(2);
                }

                // A panel can be degenerate on a very small window - a
                // zero sized texture is a creation failure, and drawing
                // into it would simply be invisible anyway
                tex_px_dims.set(
                        std::max(1, tex_px_dims.x),
                        std::max(1, tex_px_dims.y));

                auto* const texture =
                        SDL_CreateTexture(
                                g_sdl_renderer,
                                SDL_PIXELFORMAT_RGBA8888,
                                SDL_TEXTUREACCESS_TARGET,
                                tex_px_dims.x,
                                tex_px_dims.y);

                if (!texture) {
                        TRACE_ERROR_RELEASE
                                << "Failed to create display texture: "
                                << SDL_GetError()
                                << std::endl;

                        PANIC;
                }

                // The map is the bottom layer, over a window that was just
                // cleared to black - so it can be copied straight down
                // instead of blended, saving a full screen read every frame.
                // Where the map texture is transparent it writes black,
                // which is what the cleared window shows anyway.
                SDL_SetTextureBlendMode(
                        texture,
                        ((Display)i == Display::map)
                                ? SDL_BLENDMODE_NONE
                                : SDL_BLENDMODE_BLEND);

                s_display_textures[i] = texture;
        }

        s_current_display = Display::END;

        s_must_clear_all_displays = true;

        clear_display_textures();

        TRACE_FUNC_END;
}

void cleanup_displays()
{
        if (s_vignette_texture) {
                SDL_DestroyTexture(s_vignette_texture);

                s_vignette_texture = nullptr;
        }

        for (auto*& texture : s_display_textures) {
                if (texture) {
                        SDL_DestroyTexture(texture);

                        texture = nullptr;
                }
        }

        s_current_display = Display::END;
}

void mark_current_display_used(const R& px_rect)
{
        if (s_current_display == Display::END) {
                return;
        }

        const size_t i = (size_t)s_current_display;

        if (!s_is_display_used[i]) {
                s_is_display_used[i] = true;

                s_display_used_px_rect[i] = px_rect;

                return;
        }

        R& used = s_display_used_px_rect[i];

        used.p0.x = std::min(used.p0.x, px_rect.p0.x);
        used.p0.y = std::min(used.p0.y, px_rect.p0.y);
        used.p1.x = std::max(used.p1.x, px_rect.p1.x);
        used.p1.y = std::max(used.p1.y, px_rect.p1.y);
}

void clear_display_texture(const Display display)
{
        ASSERT(display != Display::END);

        auto* const texture = s_display_textures[(size_t)display];

        if (!texture) {
                return;
        }

        SDL_SetRenderTarget(g_sdl_renderer, texture);

        // Clear to fully transparent (overwrite, do not blend)
        SDL_SetRenderDrawBlendMode(g_sdl_renderer, SDL_BLENDMODE_NONE);

        SDL_SetRenderDrawColor(g_sdl_renderer, 0U, 0U, 0U, 0U);

        SDL_RenderClear(g_sdl_renderer);

        SDL_SetRenderDrawBlendMode(g_sdl_renderer, SDL_BLENDMODE_BLEND);

        s_is_display_used[(size_t)display] = false;

        // The render target IS this display now - drawing continues here
        s_current_display = display;
}

void clear_display_textures()
{
        for (size_t i = 0; i < (size_t)Display::END; ++i) {
                auto* const texture = s_display_textures[i];

                if (!texture) {
                        continue;
                }

                // A display nothing was drawn to is still clear from last
                // time. Skipping it saves a render target bind, which is a
                // pipeline flush and (on tile based mobile GPUs) a full
                // resolve of a screen sized buffer.
                if (!s_must_clear_all_displays && !s_is_display_used[i]) {
                        continue;
                }

                SDL_SetRenderTarget(g_sdl_renderer, texture);

                // Clear to fully transparent (overwrite, do not blend)
                SDL_SetRenderDrawBlendMode(g_sdl_renderer, SDL_BLENDMODE_NONE);

                SDL_SetRenderDrawColor(g_sdl_renderer, 0U, 0U, 0U, 0U);

                SDL_RenderClear(g_sdl_renderer);
        }

        SDL_SetRenderDrawBlendMode(g_sdl_renderer, SDL_BLENDMODE_BLEND);

        for (bool& is_used : s_is_display_used) {
                is_used = false;
        }

        s_must_clear_all_displays = false;

        s_current_display = Display::END;

        // Route any subsequent "raw" pixel drawing somewhere sensible
        set_display(Display::screen);
}

void composite_display_textures()
{
        // The camera advances per present, not per pass of one loop
        step_map_follow();

        SDL_SetRenderTarget(g_sdl_renderer, nullptr);

        s_current_display = Display::END;

        SDL_SetRenderDrawColor(g_sdl_renderer, 0U, 0U, 0U, 0xFFU);

        SDL_RenderClear(g_sdl_renderer);

        const int scale = config::video_scale_factor();

        for (size_t i = 0; i < (size_t)Display::END; ++i) {
                auto* const texture = s_display_textures[i];

                if (!texture) {
                        continue;
                }

                // Nothing was drawn to this display - it is fully
                // transparent, so blending it over the window would cost a
                // screen sized read/write pass for no pixels at all. In the
                // menus that skips four of the five layers; in play it skips
                // the screen layer.
                if (!s_is_display_used[i]) {
                        continue;
                }

                const auto offset =
                        s_display_px_offsets[i]
                                .scaled_up(scale)
                                .with_offsets(g_rendering_px_offset);

                const R display_rect = s_display_px_rects[i];

                if ((Display)i == Display::map) {
                        // The map is copied as a region: a map-panel sized
                        // window into the overscanned content, shifted by the
                        // sub-cell scroll offset (map panning, the camera
                        // follow, and any shake)
                        const auto overscan = map_overscan_px();

                        const auto scroll = map_src_offset_px();

                        // Zooming samples a smaller region into the same
                        // destination. The centering point must land where it
                        // did unzoomed, which is what it is offset by here.
                        const int src_w =
                                (int)std::lround(
                                        (float)display_rect.w() / s_map_zoom);

                        const int src_h =
                                (int)std::lround(
                                        (float)display_rect.h() / s_map_zoom);

                        const int zoom_off_x =
                                s_map_zoom_center_px.x -
                                (int)std::lround(
                                        (float)s_map_zoom_center_px.x /
                                        s_map_zoom);

                        const int zoom_off_y =
                                s_map_zoom_center_px.y -
                                (int)std::lround(
                                        (float)s_map_zoom_center_px.y /
                                        s_map_zoom);

                        const SDL_Rect src_rect {
                                overscan.x + scroll.x + zoom_off_x,
                                overscan.y + scroll.y + zoom_off_y,
                                src_w,
                                src_h};

                        const SDL_Rect dst_rect {
                                offset.x + (display_rect.p0.x * scale),
                                offset.y + (display_rect.p0.y * scale),
                                display_rect.w() * scale,
                                display_rect.h() * scale};

                        // Set every frame, so a tint can never outlive the
                        // effect that asked for it
                        SDL_SetTextureColorMod(
                                texture,
                                s_map_tint.r(),
                                s_map_tint.g(),
                                s_map_tint.b());

                        SDL_RenderCopy(
                                g_sdl_renderer,
                                texture,
                                &src_rect,
                                &dst_rect);

                        // Straight onto the window, between the map and
                        // everything composited after it - so the log and
                        // the panels stay clear of the darkness
                        draw_map_vignette(dst_rect, scale);

                        continue;
                }

                P px_dims;

                SDL_QueryTexture(
                        texture,
                        nullptr,
                        nullptr,
                        &px_dims.x,
                        &px_dims.y);

                // Only the part of the display that actually has content -
                // the log, the side stats panel and the action bar each own
                // a screen sized texture but cover a fraction of it, and
                // blending the empty remainder was costing several times the
                // screen in fill rate every frame.
                R used = s_display_used_px_rect[i];

                used.p0.x = std::max(0, used.p0.x);
                used.p0.y = std::max(0, used.p0.y);
                used.p1.x = std::min(px_dims.x - 1, used.p1.x);
                used.p1.y = std::min(px_dims.y - 1, used.p1.y);

                if ((used.w() <= 0) || (used.h() <= 0)) {
                        continue;
                }

                const SDL_Rect src_rect {
                        used.p0.x,
                        used.p0.y,
                        used.w(),
                        used.h()};

                // NOTE: The used rectangle is in TEXTURE space, so the
                // display's own screen origin is added back here
                const SDL_Rect dst_rect {
                        offset.x + ((display_rect.p0.x + used.p0.x) * scale),
                        offset.y + ((display_rect.p0.y + used.p0.y) * scale),
                        used.w() * scale,
                        used.h() * scale};

                SDL_RenderCopy(g_sdl_renderer, texture, &src_rect, &dst_rect);
        }

}

void set_map_zoom(const float zoom, const P& center_px)
{
        s_map_zoom = std::max(1.0f, zoom);
        s_map_zoom_center_px = center_px;
}

void set_map_vignette(const P& center_px, const float open_frac)
{
        s_vignette_center_px = center_px;

        s_vignette_open_frac = std::clamp(open_frac, 0.0f, 1.0f);
}

void clear_map_vignette()
{
        s_vignette_open_frac = 1.0f;
}

void set_map_tint(const Color& color)
{
        s_map_tint = color;
}

void clear_map_tint()
{
        s_map_tint = Color(255, 255, 255);
}

void set_display_px_offset(const Display display, const P& px_offset)
{
        ASSERT(display != Display::END);

        s_display_px_offsets[(size_t)display] = px_offset;
}

void set_display_px_origin(const Display display, const P& px_pos)
{
        ASSERT(display != Display::END);

        R& rect = s_display_px_rects[(size_t)display];

        const P dims = rect.dims();

        rect.p0 = px_pos;
        rect.p1 = px_pos + dims - 1;
}

void reset_display_px_offsets()
{
        for (P& offset : s_display_px_offsets) {
                offset = {0, 0};
        }
}

P display_px_offset(const Display display)
{
        ASSERT(display != Display::END);

        return s_display_px_offsets[(size_t)display];
}

void set_map_scroll_px_offset(const P& px_offset)
{
        s_map_scroll_px = px_offset;
}

void offset_map_follow(const P& px_offset)
{
        const bool was_settled = !io::is_map_follow_active();

        s_map_follow_px_x += (float)px_offset.x;
        s_map_follow_px_y += (float)px_offset.y;

        if (was_settled) {
                // Clock starts at the next present - see step_map_follow
                s_map_follow_last_step_ms = 0;
        }
}

bool is_map_follow_active()
{
        return (std::fabs(s_map_follow_px_x) >= s_map_follow_min_px) ||
                (std::fabs(s_map_follow_px_y) >= s_map_follow_min_px);
}

P map_follow_whole_cells()
{
        return {
                follow_whole_cells(
                        s_map_follow_px_x,
                        config::map_cell_px_w()),
                follow_whole_cells(
                        s_map_follow_px_y,
                        config::map_cell_px_h())};
}

void set_map_follow_drawn_whole_cells(const P& cells)
{
        s_map_follow_drawn_cells = cells;
}

void cancel_map_follow()
{
        s_map_follow_px_x = 0.0f;
        s_map_follow_px_y = 0.0f;

        s_map_follow_last_step_ms = 0;
}

// -----------------------------------------------------------------------------
// Map shake (see start_map_shake)
// -----------------------------------------------------------------------------
static int s_shake_amplitude_px = 0;
static uint32_t s_shake_duration_ms = 0;
static uint32_t s_shake_start_ms = 0;
static bool s_is_shake_active = false;
static bool s_is_shake_running = false;

// The shake is stepped from tight loops (see step_map_animations) - it
// advances at most this often, so a shake costs a bounded number of
// composites
static const uint32_t s_shake_step_interval_ms = 12;

// One oscillation per this much duration - slow enough that several frames
// land within a cycle at 60 Hz, so the shake does not alias
static const float s_shake_ms_per_cycle = 60.0f;

// Minimum gap between frames presented for the camera follow (see
// step_map_animations)
static const uint32_t s_follow_frame_interval_ms = 15;

static uint32_t s_follow_frame_last_ms = 0;

static uint32_t s_shake_last_step_ms = 0;

int max_map_shake_px()
{
        return std::max(4, (config::map_cell_px_h() * 2) / 3);
}

void start_map_shake(const int amplitude_px, const uint32_t duration_ms)
{
        if ((amplitude_px <= 0) || (duration_ms == 0)) {
                return;
        }

        s_shake_amplitude_px = std::min(amplitude_px, max_map_shake_px());

        s_shake_duration_ms = duration_ms;
        s_is_shake_active = true;

        // NOTE: As with the camera follow, the clock starts at the first
        // step - not here, where the caller may still have drawing to do
        s_is_shake_running = false;
}

bool is_map_shake_active()
{
        return s_is_shake_active;
}

static bool step_map_shake()
{
        if (!s_is_shake_active) {
                return false;
        }

        const uint32_t now_ms = SDL_GetTicks();

        if (!s_is_shake_running) {
                s_is_shake_running = true;

                s_shake_start_ms = now_ms;
        }
        else if ((now_ms - s_shake_last_step_ms) < s_shake_step_interval_ms) {
                return false;
        }

        s_shake_last_step_ms = now_ms;

        const uint32_t elapsed_ms = now_ms - s_shake_start_ms;

        P new_offset(0, 0);

        if (elapsed_ms >= s_shake_duration_ms) {
                s_is_shake_active = false;
        }
        else {
                const float t =
                        (float)elapsed_ms / (float)s_shake_duration_ms;

                // Shaken hard at the impact, settling to nothing
                const float decay = (1.0f - t) * (1.0f - t);

                // Whole oscillations, so the shake ends where it started -
                // one fewer per axis, so it rattles instead of orbiting. A
                // count fixed regardless of duration aliases on short
                // shakes (see s_shake_ms_per_cycle).
                const float cycles_x =
                        std::max(
                                1.0f,
                                std::round(
                                        (float)s_shake_duration_ms /
                                        s_shake_ms_per_cycle));

                const float cycles_y = std::max(1.0f, cycles_x - 1.0f);

                const float two_pi = 2.0f * 3.14159265f;

                const float amplitude = (float)s_shake_amplitude_px * decay;

                new_offset.set(
                        (int)std::lround(
                                amplitude * std::sin(t * two_pi * cycles_x)),
                        (int)std::lround(
                                amplitude * std::sin(t * two_pi * cycles_y)));
        }

        if (new_offset == s_map_shake_px) {
                return false;
        }

        s_map_shake_px = new_offset;

        return true;
}

bool step_map_animations()
{
        // Stepped here as well as in the composite, so the camera is up to
        // date before the redraw decision below. The ease is time based, so
        // stepping twice in a pass is not double counted.
        step_map_follow();

        const bool did_step_shake = step_map_shake();

        const bool is_following = is_map_follow_active();

        // A frame at most, however tightly the calling loop spins - the
        // shake has its own interval, the follow would otherwise present as
        // fast as the CPU allows
        bool is_follow_frame_due = false;

        if (is_following) {
                const uint32_t now_ms = SDL_GetTicks();

                if ((now_ms - s_follow_frame_last_ms) >=
                    s_follow_frame_interval_ms) {
                        s_follow_frame_last_ms = now_ms;

                        is_follow_frame_due = true;
                }
        }

        if (!is_follow_frame_due && !did_step_shake) {
                return false;
        }

        // The composite carries at most one cell - past that the drawn view
        // origin has to move, which means redrawing the map. A state with no
        // map display has no camera either, so a refused redraw needs no
        // fallback.
        if (is_following && viewport::is_camera_redraw_needed()) {
                viewport::advance_camera();

                states::draw_map_display();
        }

        return true;
}

P window_px_to_logical_px(const P& window_px)
{
        const auto p = window_px - g_rendering_px_offset;

        return p.scaled_down(config::video_scale_factor());
}

R panel_logical_px_rect(const Panel panel)
{
        const auto area = panels::area(panel);

        const P p0 = gui_to_px_coords(area.p0);

        const P p1 = gui_to_px_coords(area.p1 + 1) - 1;

        return {p0, p1};
}

void run_side_panel_slide_animation()
{
        // Logical pixel positions before flipping the layout. The map spans
        // the whole screen, but its auto-centering is biased by which side
        // the stats panel is on - the flip shifts the view origin, and the
        // map display slides by the corresponding pixel delta. The log and
        // action bar slide together with their column.
        const auto old_view_origin = viewport::origin();

        const int old_log_x0 = panel_logical_px_rect(Panel::log).p0.x;

        const int old_bar_x0 =
                panel_logical_px_rect(Panel::action_bar).p0.x;

        const int old_side_x0 =
                panel_logical_px_rect(Panel::map_gui_stats_border).p0.x;

        const int old_dpad_x0 = dpad::origin_px().x;

        // A pad the player has placed by hand keeps the same spot relative
        // to its now-opposite anchor - and is put down first, since the
        // interface it is being arranged within is about to move
        dpad::exit_edit_mode();

        dpad::mirror_placement();

        // A manually panned map glides back to the player alongside the
        // slide animation
        viewport::end_pan();

        s_map_scroll_px = {0, 0};

        // The framing itself is moving here - the camera has nothing to
        // catch up to
        viewport::cut_camera();

        // Flip the layout, and redraw the game into the display textures at
        // the new positions (the animation below only offsets the compositing
        // of the displays, the drawn content is already final).
        config::set_side_panel_left(!config::is_side_panel_left());

        panels::init(sdl_window_gui_dims());

        // NOTE: The display textures are sized and positioned from the
        // panels, so moving a panel invalidates them - the side stats panel
        // and the log swap sides here.
        init_displays();

        states::draw();

        const auto new_side_rect =
                panel_logical_px_rect(Panel::map_gui_stats_border);

        // Start offsets making each display appear at its old position
        const int map_start =
                (viewport::origin().x - old_view_origin.x) *
                config::map_cell_px_w();

        const int log_start =
                old_log_x0 - panel_logical_px_rect(Panel::log).p0.x;

        const int bar_start =
                old_bar_x0 - panel_logical_px_rect(Panel::action_bar).p0.x;

        const int dpad_start = old_dpad_x0 - dpad::origin_px().x;

        const int side_start = old_side_x0 - new_side_rect.p0.x;

        const int screen_px_w = panel_px_w(Panel::screen);

        const int side_px_w = new_side_rect.w();

        const bool is_now_left = config::is_side_panel_left();

        // Offsets placing the side panel just outside the screen edges,
        // relative to its new position
        const int off_beyond_old_edge =
                is_now_left
                ? (screen_px_w - new_side_rect.p0.x)
                : -(new_side_rect.p0.x + side_px_w);

        const int off_beyond_new_edge =
                is_now_left
                ? -(new_side_rect.p0.x + side_px_w)
                : (screen_px_w - new_side_rect.p0.x);

        const uint32_t duration_ms = 210U;

        // Portion of the animation spent sliding out through the old edge
        const float exit_fraction = 0.4f;

        const uint32_t start_ms = SDL_GetTicks();

        while (true) {
                const uint32_t elapsed_ms = SDL_GetTicks() - start_ms;

                if (elapsed_ms >= duration_ms) {
                        break;
                }

                const float t = (float)elapsed_ms / (float)duration_ms;

                // The log and action bar displays slide to their new
                // positions with Cubic.out easing
                const float progress = ease::cubic_out(t);

                const float remaining = 1.0f - progress;

                set_display_px_offset(
                        Display::map,
                        {(int)std::lround((float)map_start * remaining), 0});

                set_display_px_offset(
                        Display::log,
                        {(int)std::lround((float)log_start * remaining), 0});

                set_display_px_offset(
                        Display::bar,
                        {(int)std::lround((float)bar_start * remaining), 0});

                set_display_px_offset(
                        Display::dpad,
                        {(int)std::lround((float)dpad_start * remaining), 0});

                int side_off;

                if (t < exit_fraction) {
                        // Slide out through the old screen edge, accelerating
                        const float u = t / exit_fraction;

                        const float p = u * u;

                        side_off =
                                side_start +
                                (int)std::lround(
                                        (float)(off_beyond_old_edge -
                                                side_start) *
                                        p);
                }
                else {
                        // Slide in from the new screen edge with Cubic.out
                        // easing
                        const float u =
                                (t - exit_fraction) / (1.0f - exit_fraction);

                        const float p = ease::cubic_out(u);

                        side_off =
                                (int)std::lround(
                                        (float)off_beyond_new_edge *
                                        (1.0f - p));
                }

                set_display_px_offset(Display::side, {side_off, 0});

                states::draw();

                update_screen();

                SDL_Delay(10U);
        }

        reset_display_px_offsets();

        states::draw();

        update_screen();
}

}  // namespace io

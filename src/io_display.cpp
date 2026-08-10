// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "io_display.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <ostream>

#include "SDL_render.h"
#include "SDL_timer.h"
#include "config.hpp"
#include "context_pins.hpp"
#include "debug.hpp"
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

static P map_overscan_px()
{
        return {config::map_cell_px_w(), config::map_cell_px_h()};
}

// Shake offset of the map (see start_map_shake), added to the scroll offset
static P s_map_shake_px = {0, 0};

// Where the composite samples the map texture: the camera offset and the
// shake together. The overscan is all the displacement there is drawn
// content for - beyond it the copy would sample outside the texture.
static P map_src_offset_px()
{
        const auto overscan = map_overscan_px();

        return {
                std::clamp(
                        s_map_scroll_px.x + s_map_shake_px.x,
                        -overscan.x,
                        overscan.x),
                std::clamp(
                        s_map_scroll_px.y + s_map_shake_px.y,
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

static float ease_cubic_out(const float t)
{
        const float u = 1.0f - t;

        return 1.0f - (u * u * u);
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
                        // follow tween, and any shake)
                        const auto overscan = map_overscan_px();

                        const auto scroll = map_src_offset_px();

                        const SDL_Rect src_rect {
                                overscan.x + scroll.x,
                                overscan.y + scroll.y,
                                display_rect.w(),
                                display_rect.h()};

                        const SDL_Rect dst_rect {
                                offset.x + (display_rect.p0.x * scale),
                                offset.y + (display_rect.p0.y * scale),
                                display_rect.w() * scale,
                                display_rect.h() * scale};

                        SDL_RenderCopy(
                                g_sdl_renderer,
                                texture,
                                &src_rect,
                                &dst_rect);

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

void set_display_px_offset(const Display display, const P& px_offset)
{
        ASSERT(display != Display::END);

        s_display_px_offsets[(size_t)display] = px_offset;
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

P map_scroll_px_offset()
{
        return s_map_scroll_px;
}

// Camera follow tween state (see start_map_follow_tween)
static P s_follow_tween_start_px(0, 0);
static uint32_t s_follow_tween_start_ms = 0;
static bool s_is_follow_tween_active = false;

// Whether the clock has been started (see step_map_follow_tween)
static bool s_is_follow_tween_running = false;

static const uint32_t s_follow_tween_duration_ms = 90;

void start_map_follow_tween(const P& px_offset)
{
        s_follow_tween_start_px = px_offset;
        s_is_follow_tween_active = true;

        // NOTE: The clock is NOT started here - see step_map_follow_tween
        s_is_follow_tween_running = false;

        // The content starts out visually where it was before the
        // viewport stepped, and eases to the new position
        set_map_scroll_px_offset(px_offset);
}

bool is_map_follow_tween_active()
{
        return s_is_follow_tween_active;
}

bool step_map_follow_tween()
{
        if (!s_is_follow_tween_active) {
                return false;
        }

        if (!s_is_follow_tween_running) {
                // The tween is STARTED where the camera moves - which is in
                // the middle of a state draw - but it can only be STEPPED
                // by the animation loop (see io::read_input). Timing it
                // from the start would spend the tween on everything in
                // between: the rest of that frame's drawing (the whole map
                // is redrawn there), its present, and the remaining turn
                // processing. On a slow device that is longer than the
                // tween itself, so the first step already found it expired,
                // jumped the offset to zero and the camera appeared to snap
                // - no intermediate frame was ever shown.
                //
                // So the clock starts at the first step instead. The frame
                // showing the start offset (the player on its new cell, the
                // map not yet caught up) is already on screen by then.
                s_is_follow_tween_running = true;

                s_follow_tween_start_ms = SDL_GetTicks();

                return false;
        }

        const uint32_t elapsed_ms =
                SDL_GetTicks() - s_follow_tween_start_ms;

        P new_offset(0, 0);

        if (elapsed_ms >= s_follow_tween_duration_ms) {
                s_is_follow_tween_active = false;
        }
        else {
                const float t =
                        (float)elapsed_ms /
                        (float)s_follow_tween_duration_ms;

                const float u = 1.0f - t;

                const float remaining = u * u * u;  // Cubic.out

                new_offset.set(
                        (int)std::lround(
                                (float)s_follow_tween_start_px.x *
                                remaining),
                        (int)std::lround(
                                (float)s_follow_tween_start_px.y *
                                remaining));
        }

        if (new_offset == s_map_scroll_px) {
                return false;
        }

        set_map_scroll_px_offset(new_offset);

        return true;
}

void cancel_map_follow_tween()
{
        if (!s_is_follow_tween_active) {
                return;
        }

        s_is_follow_tween_active = false;

        set_map_scroll_px_offset({0, 0});
}

// -----------------------------------------------------------------------------
// Map shake (see start_map_shake)
// -----------------------------------------------------------------------------
static int s_shake_amplitude_px = 0;
static uint32_t s_shake_duration_ms = 0;
static uint32_t s_shake_start_ms = 0;
static bool s_is_shake_active = false;
static bool s_is_shake_running = false;

// The shake is stepped from tight loops (io::sleep spins, and the input
// loop iterates every millisecond) - it advances at most this often, so
// that a shake costs a bounded number of composites
static const uint32_t s_shake_step_interval_ms = 12;

static uint32_t s_shake_last_step_ms = 0;

void start_map_shake(const int amplitude_px, const uint32_t duration_ms)
{
        if ((amplitude_px <= 0) || (duration_ms == 0)) {
                return;
        }

        s_shake_amplitude_px = amplitude_px;
        s_shake_duration_ms = duration_ms;
        s_is_shake_active = true;

        // NOTE: As with the follow tween, the clock starts at the first
        // step - not here, where the caller may still have drawing to do
        s_is_shake_running = false;
}

bool is_map_shake_active()
{
        return s_is_shake_active;
}

bool step_map_shake()
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

                // Shaken hard at the blast, settling to nothing
                const float decay = (1.0f - t) * (1.0f - t);

                const float amplitude = (float)s_shake_amplitude_px * decay;

                // Whole oscillations, so the shake ends where it started -
                // and a different count per axis, so that it rattles
                // instead of orbiting
                const float two_pi = 2.0f * 3.14159265f;

                new_offset.set(
                        (int)std::lround(
                                amplitude * std::sin(t * two_pi * 4.0f)),
                        (int)std::lround(
                                amplitude * std::sin(t * two_pi * 3.0f)));
        }

        if (new_offset == s_map_shake_px) {
                return false;
        }

        s_map_shake_px = new_offset;

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

        // A manually panned map snaps back to the player as part of the
        // slide animation
        viewport::end_pan();

        s_map_scroll_px = {0, 0};

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
                const float progress = ease_cubic_out(t);

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

                        const float p = ease_cubic_out(u);

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

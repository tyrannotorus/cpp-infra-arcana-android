// =============================================================================
// Copyright Werewolf Camp
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef IO_DISPLAY_HPP
#define IO_DISPLAY_HPP

#include <cstdint>

#include "colors.hpp"
#include "panel.hpp"
#include "pos.hpp"
#include "rect.hpp"

// -----------------------------------------------------------------------------
// Display separation
//
// All drawing is routed into one of a small number of offscreen "displays"
// (render target textures at logical, i.e. unscaled, resolution), which are
// then composited to the window in update_screen(). Video scaling and window
// centering offsets are applied only at composite time.
//
// This allows the map, the message log, and the side stats panel to be
// positioned and panned independently of each other - e.g. sliding the side
// panel to the other side of the screen, and scrolling the map under the
// overlays as the camera follows the player.
// -----------------------------------------------------------------------------
namespace io
{
// NOTE: The enum order is the composite order. The screen display (menus,
// popups) is drawn above the game displays, and the action bar is always on
// top of everything (state content may extend into the bar's rows, e.g. the
// main menu's copyright line at the bottom of the screen).
//
// Each display texture is sized to the screen area that display can cover
// (see display_logical_px_rect), NOT to the whole screen: ending a render
// pass writes the entire render target back to memory on the tile based
// GPUs mobile devices use, so a screen sized texture holding a few rows of
// log costs a screen sized write every frame.
enum class Display
{
        map,
        log,
        side,
        screen,

        // The optional movement pad (see dpad), under the action bar it
        // borrows its chrome from - the pad can be dragged anywhere on
        // screen, the bar included, and must never cover it.
        //
        // Its texture is sized to the LARGEST the pad can be scaled to
        // rather than to the screen, and the display's screen origin
        // travels with the pad instead (see set_display_px_origin) - a
        // screen sized texture would cost a screen sized clear and resolve
        // every frame to carry a pad.
        dpad,

        bar,

        // Over absolutely everything, the action bar included - for
        // fullscreen effects that must cover the interface too (the fade to
        // black, see fade.cpp). Screen sized, and skipped entirely by the
        // composite whenever nothing was drawn to it, which is almost always.
        overlay,

        END
};

Display display_for_panel(Panel panel);

// Redirects subsequent draw calls to a display's texture.
void set_display(Display display);

void set_display_for_panel(Panel panel);

// Creates (or recreates) the display textures. Must be called whenever the
// renderer or the screen panel dimensions change.
void init_displays();

void cleanup_displays();

// Clears the display textures to fully transparent.
//
// NOTE: Only the ones something was actually drawn to since the last clear -
// the others are still clear, and binding a render target costs a pipeline
// flush (and, on the tile based GPUs phones and tablets use, a resolve of
// the whole render target).
void clear_display_textures();

// Clears ONE display texture, and routes subsequent drawing to it. Used
// for a partial redraw: the other displays keep the content they hold
// (see states::draw_map_display).
void clear_display_texture(Display display);

// Records that something was drawn into the current display, covering the
// given rectangle in that display texture's own pixel space.
//
// The composite uses this to touch only the part of a display that has
// content, and to skip a display entirely when nothing was drawn to it.
// Without it, every frame blends five screen sized layers onto the window
// no matter how little is on them - which on a mobile GPU is pure wasted
// memory bandwidth, and was the single most expensive thing per frame.
void mark_current_display_used(const R& px_rect);

// Draws the display textures to the window back buffer (does not present).
void composite_display_textures();

// Composite offset in logical pixels (used for animating displays).
void set_display_px_offset(Display display, const P& px_offset);

// Moves a display's screen area, keeping the size its texture was created
// with. For a display whose content travels the screen (the movement pad),
// this is what places the texture - drawing itself stays in screen
// coordinates. NOTE: The content must still fit within the created size,
// which is the whole point of sizing that texture to the content's maximum.
void set_display_px_origin(Display display, const P& px_pos);

void reset_display_px_offsets();

P display_px_offset(Display display);

// Drawing offset for the current display in logical pixels: the map
// display's content is drawn shifted by one cell of overscan (so that the
// texture has renderable room on every side of the map panel); zero for
// all other displays. Applied by the central pixel drawing functions.
P current_display_draw_px_offset();

// Sub-cell scroll of the map display content, in logical pixels. This is
// the fractional part of manual map panning - whole cells shift the
// viewport (see viewport::pan), while this offset slides the composited map
// content smoothly between cell boundaries. The map display is rendered
// with one cell of overscan beyond each map panel edge, so the sliding
// edges always show content.
void set_map_scroll_px_offset(const P& px_offset);

// Puts the camera this far behind its drawn framing, in logical pixels, and
// eases it back to zero by a fraction of what is left per present.
//
// ADDS to the current offset, so the camera's screen position stays
// continuous. Unbounded - the drawn view origin takes on whatever exceeds
// the composite's one cell (see map_follow_whole_cells). The clock starts
// at the first present, not here.
void offset_map_follow(const P& px_offset);

// Whether the camera is still behind its framing
bool is_map_follow_active();

// Whole cells of the lag beyond the one cell the composite can carry. The
// drawn view origin must take these on (see viewport::show).
P map_follow_whole_cells();

// The whole-cell part the map display was last drawn with. The composite
// offsets by the rest.
void set_map_follow_drawn_whole_cells(const P& cells);

// Advances the camera and the shake, redrawing the map display when the
// camera crosses a cell. Returns whether to present.
bool step_map_animations();

// Magnifies the composited map about a point, in logical pixels within the
// map panel. 1.0 is unzoomed. Samples content already drawn, so it costs
// nothing but the copy - and cannot zoom OUT, where there is none.
void set_map_zoom(float zoom, const P& center_px);

// Multiplies the map by a color as it is composited, and nothing else on
// screen. No tint is pure 255,255,255 - NOT colors::white(), which is
// c0c0c0 and would darken the map.
void set_map_tint(const Color& color);

void clear_map_tint();

// Radial darkness closing on a point of the map, in logical pixels.
// open_frac 1.0 leaves the map clear, 0.0 blacks it out. Applied as the map
// is composited, so the log and the panels stay clear of it.
void set_map_vignette(const P& center_px, float open_frac);

void clear_map_vignette();

// Shakes the map for a moment, settling back to still. Only moves the
// window into the map texture, so it never costs a redraw. Amplitude is
// capped by max_map_shake_px, a new shake replaces the running one, and the
// clock starts at the first step. See screen_shake for what events use.
void start_map_shake(int amplitude_px, uint32_t duration_ms);

// The largest displacement a shake may use. The map texture reserves this
// beyond the camera's cell of overscan, so a shake is never clipped by a
// lagging camera; amplitudes are capped to it.
int max_map_shake_px();

bool is_map_shake_active();

// Camera home immediately, no easing. Call viewport::cut_to or
// viewport::cut_camera instead - viewport owns the whole-cell half.
void cancel_map_follow();

// Translates a window pixel position (e.g. a touch position) to logical
// screen pixels, i.e. the coordinate space of panels and display textures.
// The inverse of the composite transform, minus per-display offsets.
P window_px_to_logical_px(const P& window_px);

// Logical pixel rectangle fully covering a panel's cells (end-inclusive).
R panel_logical_px_rect(Panel panel);

// Slides the side stats panel to the other side of the screen with an
// animated tween (Cubic.out easing), repositioning the map and log displays
// accordingly. Persists the new layout in the config.
void run_side_panel_slide_animation();

}  // namespace io

#endif  // IO_DISPLAY_HPP

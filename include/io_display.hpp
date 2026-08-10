// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef IO_DISPLAY_HPP
#define IO_DISPLAY_HPP

#include <cstdint>

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
// positioned (and eventually scaled/panned) independently of each other -
// e.g. sliding the side panel to the other side of the screen on Android.
//
// On desktop, all composite offsets are zero and the result is pixel
// identical to drawing directly to the window.
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

void reset_display_px_offsets();

P display_px_offset(Display display);

// Drawing offset for the current display in logical pixels: the map
// display's content is drawn shifted by one cell of overscan (so that the
// texture has renderable room on every side of the map panel); zero for
// all other displays. Applied by the central pixel drawing functions.
P current_display_draw_px_offset();

// Sub-cell scroll of the map display content, in logical pixels. This is
// the fractional part of two finger map panning - whole cells shift the
// viewport (see viewport::pan), while this offset slides the composited map
// content smoothly between cell boundaries. The map display is rendered
// with one cell of overscan beyond each map panel edge, so the sliding
// edges always show content.
void set_map_scroll_px_offset(const P& px_offset);

P map_scroll_px_offset();

// A short tween of the map scroll offset from the given value back to
// zero, so that the camera SLIDES when the viewport steps while following
// the player, instead of snapping tile to tile (see viewport::show).
// Stepped from the input/render loop (non blocking). NOTE: The tween's
// clock starts at the first step, not here - see step_map_follow_tween.
void start_map_follow_tween(const P& px_offset);

// Advances the tween; returns whether the offset changed (the screen
// should then be re-composited). Stepping it costs a composite and a
// present - the map display is NOT redrawn, only the window into it moves.
bool step_map_follow_tween();

// Whether a camera follow tween is running. While it is, the render loop
// keeps to compositing: a full state redraw mid-tween is expensive enough
// on a slow device to eat the whole tween in one frame.
bool is_map_follow_tween_active();

// Shakes the map for a moment, settling back to still - an explosion going
// off (see explosion::run). Like the camera tween this only moves the
// window into the map texture, so it costs a composite per step and never
// a redraw, and it is stepped by whatever loop is running: io::sleep
// animates it through the blast's own delay, and the input loop finishes
// it. The amplitude is in logical pixels, and is capped by the map's one
// cell of overscan. NOTE: The clock starts at the first step, not here.
void start_map_shake(int amplitude_px, uint32_t duration_ms);

bool step_map_shake();

bool is_map_shake_active();

// E.g. manual panning takes over the scroll offset
void cancel_map_follow_tween();

// Translates a window pixel position (e.g. a touch position) to logical
// screen pixels, i.e. the coordinate space of panels and display textures.
// The inverse of the composite transform, minus per-display offsets.
P window_px_to_logical_px(const P& window_px);

// Logical pixel rectangle fully covering a panel's cells (end-inclusive).
R panel_logical_px_rect(Panel panel);

// Slides the side stats panel to the other side of the screen with an
// animated tween (Back.out easing), repositioning the map and log displays
// accordingly. Persists the new layout in the config.
void run_side_panel_slide_animation();

}  // namespace io

#endif  // IO_DISPLAY_HPP

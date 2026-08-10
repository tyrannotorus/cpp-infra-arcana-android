// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "scrollbar.hpp"

#include <algorithm>
#include <cstdint>

#include "colors.hpp"
#include "config.hpp"
#include "draw_box.hpp"
#include "io.hpp"
#include "io_display.hpp"
#include "panel.hpp"
#include "pos.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static int thumb_px_h(const int track_h, const int max_scroll_px)
{
        const int content_h = track_h + max_scroll_px;

        return std::max(
                config::gui_cell_px_h(),
                (track_h * track_h) / std::max(1, content_h));
}

static int thumb_px_y0(
        const R& track_px,
        const int thumb_h,
        const int scroll_px,
        const int max_scroll_px)
{
        const int span = track_px.h() - thumb_h;

        if ((span <= 0) || (max_scroll_px <= 0)) {
                return track_px.p0.y;
        }

        return track_px.p0.y + ((scroll_px * span) / max_scroll_px);
}

// Fade-to-black hint at the top or bottom edge of a content area,
// intuitively indicating that the content continues beyond the visible area
static void draw_fade(
        const R& content_px,
        const bool at_bottom,
        const int extra_h = 0)
{
        const int fade_h = (config::gui_cell_px_h() * 2) + extra_h;

        // NOTE: The steps are cut from the band by exact fractions, not by
        // a rounded step height - a small step height would otherwise
        // round DOWN and leave the rest of the band (the most opaque part,
        // right at the content edge) undrawn
        const int nr_steps = std::min(16, fade_h);

        if (nr_steps <= 0) {
                return;
        }

        for (int i = 0; i < nr_steps; ++i) {
                // More opaque towards the content edge
                const auto alpha = (uint8_t)((255 * (i + 1)) / nr_steps);

                // The step's offsets from the band's outer edge (the edge
                // the fade grows from)
                const int offset_lo = (fade_h * i) / nr_steps;
                const int offset_hi = ((fade_h * (i + 1)) / nr_steps) - 1;

                if (offset_hi < offset_lo) {
                        continue;
                }

                R step_rect;

                if (at_bottom) {
                        // Grows downwards, ending at the bottom edge
                        const int band_y0 = content_px.p1.y + 1 - fade_h;

                        step_rect = {
                                P(content_px.p0.x, band_y0 + offset_lo),
                                P(content_px.p1.x, band_y0 + offset_hi)};
                }
                else {
                        // Grows upwards, ending at the top edge
                        const int band_y1 = content_px.p0.y + fade_h - 1;

                        step_rect = {
                                P(content_px.p0.x, band_y1 - offset_hi),
                                P(content_px.p1.x, band_y1 - offset_lo)};
                }

                io::draw_rectangle_filled(
                        step_rect,
                        colors::black(),
                        alpha);
        }
}

// -----------------------------------------------------------------------------
// scrollbar
// -----------------------------------------------------------------------------
namespace scrollbar
{
static int track_w_px()
{
        return (config::gui_cell_px_w() * 3) / 2;
}

R track_px_rect(const R& content_px)
{
        // Right edge just inside the fullscreen border box line - a content
        // area spans the content column, so its bar belongs at the frame
        const int x1 =
                io::gui_to_px_coords(
                        P(panels::screen_box_area().p1.x, 0))
                        .x -
                1;

        return {
                P(x1 - track_w_px() + 1, content_px.p0.y),
                P(x1, content_px.p1.y)};
}

R track_px_rect(const Panel content_panel)
{
        return track_px_rect(io::panel_logical_px_rect(content_panel));
}

R grab_px_rect(const R& track_px)
{
        R r = track_px;

        r.p0.x -= (config::gui_cell_px_w() * 5) / 2;

        return r;
}

void draw(const R& track_px, const int scroll_px, const int max_scroll_px)
{
        if (max_scroll_px <= 0) {
                // Everything fits - there is nothing to scroll
                return;
        }

        // Styled like the action bar buttons: dark fill with a gray
        // outline, and the game's sepia accent for the interactive thumb
        io::draw_rectangle_filled(track_px, colors::extra_dark_gray());

        io::draw_rectangle(track_px, colors::dark_gray());

        const int thumb_h = thumb_px_h(track_px.h(), max_scroll_px);

        const int thumb_y0 =
                thumb_px_y0(track_px, thumb_h, scroll_px, max_scroll_px);

        const R thumb(
                P(track_px.p0.x + 2, thumb_y0),
                P(track_px.p1.x - 2, thumb_y0 + thumb_h - 1));

        io::draw_rectangle_filled(thumb, colors::sepia());
}

void draw_content_fades(
        const R& content_px,
        const int scroll_px,
        const int max_scroll_px,
        const int extra_bottom_fade_px)
{
        if (max_scroll_px <= 0) {
                return;
        }

        if (scroll_px < max_scroll_px) {
                draw_fade(content_px, true, extra_bottom_fade_px);
        }

        if (scroll_px > 0) {
                draw_fade(content_px, false);
        }
}

void Drag::begin(
        const R& track_px,
        const int touch_px_y,
        const int scroll_px,
        const int max_scroll_px)
{
        const int thumb_h = thumb_px_h(track_px.h(), max_scroll_px);

        const int thumb_y0 =
                thumb_px_y0(track_px, thumb_h, scroll_px, max_scroll_px);

        // The grip is where the finger landed RELATIVE to the thumb, and
        // it is kept whether or not the finger landed on the thumb at all.
        // Centering the thumb on a touch that missed it (the conventional
        // "jump to the touched position") makes the content leap the moment
        // you touch near either end of the track - so the bar never jumps
        // now: it scrolls from wherever it is, by however far the finger
        // moves.
        m_grab_offset_px = touch_px_y - thumb_y0;
}

int Drag::scroll_px_at(
        const R& track_px,
        const int touch_px_y,
        const int scroll_px,
        const int max_scroll_px) const
{
        const int thumb_h = thumb_px_h(track_px.h(), max_scroll_px);

        const int span = track_px.h() - thumb_h;

        if (span <= 0) {
                return scroll_px;
        }

        const int rel = touch_px_y - track_px.p0.y - m_grab_offset_px;

        return std::clamp((rel * max_scroll_px) / span, 0, max_scroll_px);
}

}  // namespace scrollbar

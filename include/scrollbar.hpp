// =============================================================================
// Copyright Werewolf Camp
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef SCROLLBAR_HPP
#define SCROLLBAR_HPP

#include "rect.hpp"

enum class Panel;

// -----------------------------------------------------------------------------
// A draggable scrollbar for any scrollable content area, and the
// fade-to-black hints drawn at the edges where content continues.
//
// The widget is stateless apart from the drag grip: the caller owns the
// scroll position and its maximum (in content pixels), and passes them in.
// A bar is only meaningful while there is something to scroll, i.e. while
// max_scroll_px > 0 - drawing and hit testing with 0 is a no-op.
// -----------------------------------------------------------------------------
namespace scrollbar
{
// The bar for a content area: a vertical track along the screen's right
// edge, flush against the fullscreen border box line, spanning the content
// area vertically. Takes either the content area itself (logical pixels),
// or the panel it occupies.
R track_px_rect(const R& content_px);
R track_px_rect(Panel content_panel);

// The track widened to the left, so that the bar is comfortable to grab
// with a finger. Content areas must reserve room for this (e.g. by
// wrapping their text narrower).
R grab_px_rect(const R& track_px);

void draw(const R& track_px, int scroll_px, int max_scroll_px);

// Fade-to-black hints at the top and/or bottom edge of the content area,
// shown at the edges where the content continues out of view. The bottom
// fade can be made taller, for content areas that keep something else in
// their bottom rows (e.g. the inventory's item action pins) - the text
// under it then reads as faded out rather than as cut off.
void draw_content_fades(
        const R& content_px,
        int scroll_px,
        int max_scroll_px,
        int extra_bottom_fade_px = 0);

// The grip taken on the thumb when a drag begins, held for the whole
// gesture so that the thumb never jumps out from under the finger. A grab
// on the bare track above or below the thumb has no such point - the thumb
// centers on the finger instead (the conventional jump to the touched
// position), and keeps that grip while dragging on.
class Drag
{
public:
        void begin(
                const R& track_px,
                int touch_px_y,
                int scroll_px,
                int max_scroll_px);

        // The scroll position for a finger at the given y, or the unchanged
        // scroll position if the content cannot be scrolled
        int scroll_px_at(
                const R& track_px,
                int touch_px_y,
                int scroll_px,
                int max_scroll_px) const;

private:
        int m_grab_offset_px {0};
};

}  // namespace scrollbar

#endif  // SCROLLBAR_HPP

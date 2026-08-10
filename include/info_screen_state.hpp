// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef INFO_SCREEN_STATE_HPP
#define INFO_SCREEN_STATE_HPP

#include <string>

#include "colors.hpp"
#include "rect.hpp"
#include "scrollbar.hpp"
#include "state.hpp"

enum class InfoScreenType
{
        scrolling,
        single_screen
};

// Base class for full screen text content (the manual, message history,
// character description, ...). Scrolling is pixel based: dragging the
// scrollbar scrolls the content, and keyboard scrolling moves by whole
// lines. The text itself is NOT a drag zone.
class InfoScreenState : public State
{
public:
        InfoScreenState() = default;

        void update() override;


        // Dragging the scrollbar scrolls the content - and is the only
        // gesture that does.
        bool try_begin_touch_drag(const P& logical_px) override;

        void on_touch_drag_move(const P& logical_px) override;

        void on_touch_drag_end() override;

protected:
        void draw_interface() const;

        // Draws the currently visible content lines (call from draw)
        void draw_scrollable_content() const;

        // Draws the scrollbar and the fade-to-black hints at the edges
        // where the content continues (call last from draw, so that the
        // fades do not dim the bar). Draws nothing when everything fits.
        void draw_scroll_affordances() const;

        virtual std::string title() const = 0;

        virtual InfoScreenType type() const = 0;

        virtual int get_lines_total() const = 0;

        // The scrolled content area (logical pixels). Defaults to the info
        // screen content panel - override for screens whose content sits
        // somewhere else (e.g. a text page, whose text is framed by its
        // divider rules).
        virtual R content_px_rect() const;

        // Confirm (a tap anywhere, see io_input) and escape (the [ x ]
        // border control, or the device back button). Both leave the
        // screen by default.
        virtual void on_confirmed();

        virtual void on_cancelled();

        // The content line at a line index (only called for indexes within
        // the total line count). Override for states using
        // draw_scrollable_content.
        virtual ColoredString content_line(int line_idx) const
        {
                (void)line_idx;

                return {};
        }

        // Scrolling state in content pixels (0 = top)
        int max_scroll_px() const;

        void set_scroll_px(int scroll_px);

        void scroll_to_bottom();

        int first_visible_line() const;

        int last_visible_line() const;

        int m_scroll_px {0};

private:
        scrollbar::Drag m_scrollbar_drag {};
        bool m_is_scrollbar_drag_active {false};
};

#endif  // INFO_SCREEN_STATE_HPP

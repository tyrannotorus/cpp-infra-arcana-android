// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef DESCR_COLUMN_HPP
#define DESCR_COLUMN_HPP

#include <string>
#include <vector>

#include "colors.hpp"
#include "scrollbar.hpp"

enum class Panel;
struct P;

// -----------------------------------------------------------------------------
// The description text beside a list - of the marked trait, spell domain,
// manual chapter, inventory item, ... Text that does not fit is scrolled
// with a draggable scrollbar at the screen frame, with fade-to-black hints
// at the edges where it continues. It NEVER scrolls by itself, and the
// text is not a drag zone: a gesture over it is a swipe like anywhere
// else, and navigates the list.
//
// The column fills its panel from the top, so that its first line is in
// line with the first row of the list beside it.
// -----------------------------------------------------------------------------
class DescrColumn
{
public:
        DescrColumn(const Panel panel) :
                m_panel(panel) {}

        // Text wrap width, leaving room for the scrollbar and its grab
        // zone
        int text_w() const;

        // Back to the top - call when another entry becomes marked
        void reset_scroll();

        // Rows at the bottom of the column kept for something else (the
        // inventory's item action pins). The text is not made shorter: it
        // scrolls PAST them, and the fade covering them is deepened, so
        // that whatever sits there always does so on faded out text.
        void set_reserved_bottom_rows(int nr_rows);

        // Each line is a sequence of colored segments (an empty line is an
        // empty vector)
        void draw(const std::vector<std::vector<ColoredString>>& lines);

        // Plain text in one color
        void draw(const std::vector<std::string>& lines, const Color& color);

        // Touch dragging - ONLY the scrollbar is a drag zone
        bool try_begin_drag(const P& logical_px);

        void on_drag_move(const P& logical_px);

        void on_drag_end();

private:
        void set_scroll_from_drag_y(int drag_px_y);

        const Panel m_panel;

        int m_scroll_px {0};
        int m_max_scroll_px {0};
        int m_reserved_bottom_rows {0};
        bool m_drag_active {false};

        scrollbar::Drag m_drag {};
};

#endif  // DESCR_COLUMN_HPP

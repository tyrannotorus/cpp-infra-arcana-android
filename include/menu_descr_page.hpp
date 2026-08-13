// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef MENU_DESCR_PAGE_HPP
#define MENU_DESCR_PAGE_HPP

#include <string>
#include <vector>

#include "colors.hpp"
#include "descr_column.hpp"
#include "menu_page.hpp"
#include "panel.hpp"

struct P;

// Appends a raw description string to a segmented line list, wrapped to the
// given width, with "{...}" color markup applied (compiled through the same
// TextCompiler as regular text drawing). Each line is a sequence of colored
// segments (an empty line is an empty vector).
void append_descr_text(
        std::vector<std::vector<ColoredString>>& lines,
        const std::string& raw_str,
        const Color& default_color,
        int w);

// The two column menu page: a selectable list in the left column, with
// description text of the marked entry in the right column (character
// creation, the manual, ...).
//
// Both columns are TOP ALIGNED, starting one row below the panel top, so
// that the first list entry and the first line of text sit on the same row
// with a bit of breathing room under the border box. The list is not framed
// by divider rules - with text beside it, the fullscreen border box is the
// only frame the page needs.
//
// The description scrolls with its own scrollbar; the text itself is not a
// drag zone (a gesture over it is a swipe like anywhere else, and navigates
// the list).
class MenuDescrPageState : public MenuPageState
{
public:
        // The default column pair suits a plain list; a page with a value
        // column needs wider ones (see the settings page)
        MenuDescrPageState(
                Panel list_panel = Panel::menu_descr_list,
                Panel descr_panel = Panel::menu_descr_text) :
                m_list_panel(list_panel),
                m_descr(descr_panel) {}

        bool try_begin_touch_drag(const P& logical_px) override;

        void on_touch_drag_move(const P& logical_px) override;

        void on_touch_drag_end() override;

protected:
        int list_x0(int block_w) const override;

        int list_y0(int nr_entries_shown) const override;

        int list_max_x1() const override;

        int list_h() const override;

        bool show_list_dividers() const override
        {
                return false;
        }

        // Draws the marked entry's description into the description
        // column, resetting the scroll when another entry becomes marked
        void draw_descr_lines(
                const std::vector<std::vector<ColoredString>>& lines);

        // Text wrap width for the description
        int descr_text_w() const;

private:
        const Panel m_list_panel;

        DescrColumn m_descr;

        int m_descr_last_marked_idx {-1};
};

#endif  // MENU_DESCR_PAGE_HPP

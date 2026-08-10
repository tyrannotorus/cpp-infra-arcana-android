// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "menu_page.hpp"

#include <algorithm>
#include <unordered_map>

#include "SDL_keycode.h"
#include "colors.hpp"
#include "common_text.hpp"
#include "config.hpp"
#include "debug.hpp"
#include "draw_box.hpp"
#include "io.hpp"
#include "io_display.hpp"
#include "panel.hpp"
#include "pos.hpp"
#include "rect.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------

// Horizontal gap between the label column and the value column
static const int s_value_column_gap = 2;

// How far the divider rules extend beyond the list block on each side
static const int s_divider_pad = 6;

// -----------------------------------------------------------------------------
// Global namespace
// -----------------------------------------------------------------------------
int menu_key_prefix_len(const std::string& label)
{
        if (label.empty() || (label[0] != '(')) {
                return 0;
        }

        for (size_t i = 0; i < label.size(); ++i) {
                if (label[i] == ')') {
                        return (int)i + 1;
                }
        }

        return 0;
}

void draw_menu_divider(const int x0, const int x1, const int y)
{
        io::set_display_for_panel(Panel::screen);

        const P cell_px_dims(
                config::gui_cell_px_w(),
                config::gui_cell_px_h());

        // Spans gui columns x0..x1 inclusive
        const auto p0 =
                P(x0, y)
                        .scaled_up(cell_px_dims)
                        .with_y_offset(cell_px_dims.y / 2);

        const auto p1 =
                P(x1 + 1, y)
                        .scaled_up(cell_px_dims)
                        .with_offsets(-1, cell_px_dims.y / 2);

        io::draw_rectangle({p0, p1}, colors::dark_gray());
}

// Marked entry of previously visited pages, keyed by page title - a page
// that is re-entered (a fresh instance) restores the last marked entry.
// Pages that stay on the state stack while a child is open keep their
// browser state anyway; this covers pages that are re-created.
static std::unordered_map<std::string, int> s_remembered_marked_y;

void forget_remembered_marked_entries()
{
        s_remembered_marked_y.clear();
}

// -----------------------------------------------------------------------------
// MenuPageState
// -----------------------------------------------------------------------------
int MenuPageState::list_h() const
{
        // Everything between the screen's border rules, minus the two
        // divider rules that frame the list. A longer list is shown a page
        // at a time (see MenuBrowser::update_range_shown).
        const R box = panels::screen_box_area();

        return std::max(1, (box.p1.y - box.p0.y - 1) - 2);
}

std::string MenuPageState::page_hint() const
{
        return common_text::g_menu_select_hint;
}

void MenuPageState::on_start()
{
        reset_browser();

        const auto it = s_remembered_marked_y.find(page_title());

        // A page that was visited before opens on the entry it was left on,
        // a page opened for the first time on its own default entry.
        // NOTE: set_y clamps to the current number of entries.
        m_browser.set_y(
                (it == std::end(s_remembered_marked_y))
                        ? default_marked_idx()
                        : it->second);
}

void MenuPageState::on_window_resized()
{
        reset_browser();
}

void MenuPageState::reset_browser()
{
        const int old_y = m_browser.y();

        m_browser.reset((int)page_entries().size(), list_h());

        if (use_left_right_keys()) {
                m_browser.enable_left_right_keys();
        }

        m_browser.set_y(old_y);
}

void MenuPageState::on_cancelled()
{
        states::pop();
}

Color MenuPageState::entry_color(const int idx, const bool is_marked) const
{
        (void)idx;

        return is_marked
                ? colors::menu_highlight()
                : colors::menu_dark();
}

int MenuPageState::list_x0(const int block_w) const
{
        return panels::center_x(Panel::screen) - (block_w / 2);
}

int MenuPageState::list_y0(const int nr_entries_shown) const
{
        return panels::center_y(Panel::screen) - (nr_entries_shown / 2);
}

int MenuPageState::list_max_x1() const
{
        return panels::x1(Panel::screen) - 1;
}

void MenuPageState::draw()
{
        const auto entries = page_entries();

        draw_box(panels::screen_box_area());

        const int screen_center_x = panels::center_x(Panel::screen);

        io::draw_text_center(
                " " + page_title() + " ",
                Panel::screen,
                {screen_center_x, panels::screen_box_area().p0.y},
                colors::title(),
                io::DrawBg::yes,
                colors::black(),
                true);  // Allow pixel-level adjustment

        const std::string hint = page_hint();

        if (!hint.empty()) {
                io::draw_text_center(
                        " " + hint + " ",
                        Panel::screen,
                        {screen_center_x, panels::screen_box_area().p1.y},
                        colors::title(),
                        io::DrawBg::yes,
                        colors::black(),
                        true);  // Allow pixel-level adjustment
        }

        m_drawn_list_y0 = -1;

        if (entries.empty()) {
                draw_page_content();

                return;
        }

        // Label/value column widths
        int label_w = 0;
        int value_w = 0;

        for (const auto& entry : entries) {
                label_w = std::max(label_w, (int)entry.label.size());
                value_w = std::max(value_w, (int)entry.value.size());
        }

        const int block_w =
                (value_w > 0)
                ? (label_w + s_value_column_gap + value_w)
                : label_w;

        const auto idx_range_shown = m_browser.range_shown();

        const int nr_shown = idx_range_shown.max - idx_range_shown.min + 1;

        const int x0 = list_x0(block_w);
        const int y0 = list_y0(nr_shown);

        // The list block: the labels padded out to the stylistic divider
        // width, centered on the labels. This is the width of the divider
        // rules (where they are drawn), of the row tap zone, and of the
        // scroll fades.
        const int row_w =
                std::clamp(
                        block_w + (2 * s_divider_pad),
                        g_divider_min_w,
                        g_divider_max_w);

        const int block_center_x = x0 + (block_w / 2);

        const int row_x0 =
                std::max(1, block_center_x - (row_w / 2));

        const int row_x1 =
                std::min(
                        list_max_x1(),
                        row_x0 + row_w - 1);

        if (show_list_dividers()) {
                draw_menu_divider(row_x0, row_x1, y0 - 1);
                draw_menu_divider(row_x0, row_x1, y0 + nr_shown);
        }

        // Entries
        for (int i = idx_range_shown.min; i <= idx_range_shown.max; ++i) {
                const auto& entry = entries[i];

                const int y = y0 + (i - idx_range_shown.min);

                const bool is_marked = m_browser.is_at_idx(i);

                const auto color = entry_color(i, is_marked);

                const int key_len = menu_key_prefix_len(entry.label);

                if (key_len > 0) {
                        const auto key_color =
                                is_marked
                                ? colors::menu_key_highlight()
                                : colors::menu_key_dark();

                        io::draw_text(
                                entry.label.substr(0, key_len),
                                Panel::screen,
                                {x0, y},
                                key_color);

                        io::draw_text(
                                entry.label.substr(key_len),
                                Panel::screen,
                                {x0 + key_len, y},
                                color);
                }
                else {
                        io::draw_text(
                                entry.label,
                                Panel::screen,
                                {x0, y},
                                color);
                }

                if (!entry.value.empty()) {
                        io::draw_text(
                                entry.value,
                                Panel::screen,
                                {x0 + label_w + s_value_column_gap, y},
                                color);
                }
        }

        // Where the list continues, its edge fades out - drawn after the
        // entries, so it dims the outermost ones. A side list sits in the
        // middle of the screen, nowhere near the frame, so a scrollbar
        // would be worse than none; the fade says "there is more" without
        // putting furniture there.
        if (m_browser.nr_items_tot() > nr_shown) {
                const int row_px_h = config::gui_cell_px_h();

                const R list_px(
                        io::gui_to_px_coords(P(row_x0, y0)),
                        io::gui_to_px_coords(
                                P(row_x1 + 1, y0 + nr_shown)) -
                                1);

                scrollbar::draw_content_fades(
                        list_px,
                        idx_range_shown.min * row_px_h,
                        (m_browser.nr_items_tot() - nr_shown) * row_px_h);
        }

        // Record the list geometry for tap mapping
        m_drawn_list_y0 = y0;
        m_drawn_row_x0 = row_x0;
        m_drawn_row_x1 = row_x1;

        // Remember the marked entry, so a later re-entry of this page
        // restores it
        s_remembered_marked_y[page_title()] = m_browser.y();

        draw_page_content();
}

void MenuPageState::update()
{
        const auto input = io::read_input();

        if (handle_custom_input(input)) {
                return;
        }

        if (m_browser.nr_items_tot() == 0) {
                if ((input.key == SDLK_ESCAPE) || (input.key == SDLK_SPACE)) {
                        on_cancelled();
                }

                return;
        }

        m_browser.set_selection_audio_enabled(
                entry_plays_selection_audio(m_browser.y()));

        const MenuAction action =
                m_browser.read(input, MenuInputMode::scrolling);

        switch (action) {
        case MenuAction::selected:
                on_entry_selected(m_browser.y());
                break;

        case MenuAction::left:
                on_entry_left(m_browser.y());
                break;

        case MenuAction::right:
                on_entry_right(m_browser.y());
                break;

        case MenuAction::esc:
        case MenuAction::space:
                on_cancelled();
                break;

        default:
                break;
        }
}

bool MenuPageState::try_tap(const P& logical_px)
{
        // Tapping a list row marks it; the tap is then deliberately
        // reported as NOT handled, so the input layer synthesizes a
        // confirm key (see io_input.cpp), which selects the marked entry
        // through the normal update path (a tap handler must not pop
        // states itself). Taps that miss the rows are also NOT handled -
        // tapping anywhere selects the currently marked entry (swipes can
        // never count as taps, see the max-travel classification in
        // io_input.cpp).

        if (m_drawn_list_y0 < 0) {
                return false;
        }

        const P gui_pos(
                logical_px.x / config::gui_cell_px_w(),
                logical_px.y / config::gui_cell_px_h());

        if ((gui_pos.x < m_drawn_row_x0) ||
            (gui_pos.x > m_drawn_row_x1)) {
                return false;
        }

        const auto idx_range_shown = m_browser.range_shown();

        const int idx =
                idx_range_shown.min + (gui_pos.y - m_drawn_list_y0);

        if ((idx < idx_range_shown.min) || (idx > idx_range_shown.max)) {
                return false;
        }

        m_browser.set_y(idx);

        return false;
}

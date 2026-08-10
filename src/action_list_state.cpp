// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "action_list_state.hpp"

#include <algorithm>
#include <vector>

#include "colors.hpp"
#include "config.hpp"
#include "game_time.hpp"
#include "io.hpp"
#include "io_display.hpp"
#include "io_internal.hpp"
#include "panel.hpp"
#include "pos.hpp"
#include "scrollbar.hpp"

// -----------------------------------------------------------------------------
// ActionListState
// -----------------------------------------------------------------------------
void ActionListState::on_window_resized()
{
        m_descr.reset_scroll();
}

bool ActionListState::try_begin_touch_drag(const P& logical_px)
{
        // Only the description SCROLLBAR is a drag zone - the text and the
        // list are operated by swiping/tapping like everywhere else
        return m_descr.try_begin_drag(logical_px);
}

void ActionListState::on_touch_drag_move(const P& logical_px)
{
        m_descr.on_drag_move(logical_px);
}

void ActionListState::on_touch_drag_end()
{
        m_descr.on_drag_end();
}

int ActionListState::descr_text_w() const
{
        return m_descr.text_w();
}

void ActionListState::draw_list_fades() const
{
        const Range idx_range_shown = m_browser.range_shown();

        const int nr_shown = idx_range_shown.max - idx_range_shown.min + 1;

        if (m_browser.nr_items_tot() <= nr_shown) {
                // The whole list is shown
                return;
        }

        const int row_px_h = config::gui_cell_px_h();

        scrollbar::draw_content_fades(
                io::panel_logical_px_rect(Panel::inventory_menu),
                idx_range_shown.min * row_px_h,
                (m_browser.nr_items_tot() - nr_shown) * row_px_h);
}

int ActionListState::layout_action_pins(std::vector<ActionPin>& pins) const
{
        if (pins.empty()) {
                return 0;
        }

        const int cell_w = config::gui_cell_px_w();
        const int cell_h = config::gui_cell_px_h();

        const R panel_px = io::panel_logical_px_rect(Panel::inventory_descr);

        // Right aligned within the TEXT width, so that the pins stay clear
        // of the scrollbar
        const int x1 = panel_px.p0.x + (descr_text_w() * cell_w) - 1;

        const int gap_px = cell_w;

        auto pin_px_w = [cell_w](const ActionPin& pin) {
                // "[ label ]"
                return ((int)pin.label.size() + 4) * cell_w;
        };

        // Group the pins into rows, in order, wrapping when the next one
        // does not fit
        std::vector<std::vector<size_t>> rows(1);

        int row_w = 0;

        for (size_t i = 0; i < pins.size(); ++i) {
                const int w = pin_px_w(pins[i]);

                const int new_row_w =
                        rows.back().empty() ? w : (row_w + gap_px + w);

                if (!rows.back().empty() &&
                    (new_row_w > (x1 - panel_px.p0.x + 1))) {
                        rows.emplace_back();

                        row_w = w;
                }
                else {
                        row_w = new_row_w;
                }

                rows.back().push_back(i);
        }

        const int nr_rows = (int)rows.size();

        // The last row sits at the bottom of the column, earlier rows
        // above it. Within a row the pins run left to right, in order, the
        // row as a whole right aligned.
        for (int row = 0; row < nr_rows; ++row) {
                int tot_w = 0;

                for (const size_t i : rows[row]) {
                        tot_w += pin_px_w(pins[i]);
                }

                tot_w += ((int)rows[row].size() - 1) * gap_px;

                const int row_y1 =
                        panel_px.p1.y - ((nr_rows - 1 - row) * cell_h);

                int x = x1 - tot_w + 1;

                for (const size_t i : rows[row]) {
                        const int w = pin_px_w(pins[i]);

                        pins[i].px_rect = {
                                P(x, row_y1 - cell_h + 1),
                                P(x + w - 1, row_y1)};

                        x += w + gap_px;
                }
        }

        return nr_rows;
}

void ActionListState::prepare_action_pins()
{
        // The pins are laid out BEFORE the description is drawn, so that
        // the text knows how many of its bottom rows they take
        m_drawn_pins = marked_entry_actions();

        m_descr.set_reserved_bottom_rows(layout_action_pins(m_drawn_pins));
}

void ActionListState::draw_action_pins() const
{
        if (m_drawn_pins.empty()) {
                return;
        }

        // The rows the pins sit on are blacked out: the description text
        // is faded into them (see DescrColumn), and this is where the fade
        // ends - text must not show BETWEEN the pins
        int band_y0 = m_drawn_pins.front().px_rect.p0.y;

        for (const auto& pin : m_drawn_pins) {
                band_y0 = std::min(band_y0, pin.px_rect.p0.y);
        }

        const R panel_px = io::panel_logical_px_rect(Panel::inventory_descr);

        io::draw_rectangle_filled(
                {P(panel_px.p0.x, band_y0), panel_px.p1},
                colors::black());

        for (const auto& pin : m_drawn_pins) {
                if (pin.px_rect.p0.x < 0) {
                        continue;
                }

                io::draw_text_at_px(
                        "[ " + pin.label + " ]",
                        pin.px_rect.p0,
                        colors::menu_highlight(),
                        io::DrawBg::no,
                        colors::black());
        }
}

bool ActionListState::try_tap(const P& logical_px)
{
        // A pin?
        for (const auto& pin : m_drawn_pins) {
                if (pin.px_rect.p0.x < 0) {
                        continue;
                }

                // The hit area is expanded a little, for easier tapping
                R hit = pin.px_rect;

                hit.p0 = hit.p0.with_offsets(
                        -config::gui_cell_px_w() / 2,
                        -config::gui_cell_px_h() / 2);

                hit.p1 = hit.p1.with_offsets(
                        config::gui_cell_px_w() / 2,
                        config::gui_cell_px_h() / 2);

                if (hit.is_pos_inside(logical_px)) {
                        // Run from update(), not from here - see
                        // m_pending_action. The tap is deliberately
                        // reported as NOT handled, so that the input layer
                        // synthesizes a key for it and input reading ends
                        // (the key itself is discarded).
                        m_has_pending_action = true;
                        m_pending_action = pin.id;

                        return false;
                }
        }

        // A row? On a screen WITH pins, tapping one only marks it - the
        // marked thing is engaged through the pins, so that nothing ever
        // happens to it by accident. Without pins the tap marks the row
        // and is reported as unhandled, so that the synthesized confirm
        // key selects it (the plain menu behaviour).
        const R menu_px = io::panel_logical_px_rect(Panel::inventory_menu);

        if (menu_px.is_pos_inside(logical_px)) {
                const int row =
                        (logical_px.y - menu_px.p0.y) /
                        config::gui_cell_px_h();

                const Range idx_range_shown = m_browser.range_shown();

                const int idx = idx_range_shown.min + row;

                if ((idx >= idx_range_shown.min) &&
                    (idx <= idx_range_shown.max)) {
                        m_browser.set_y(idx);
                }
        }

        return has_action_pins();
}

bool ActionListState::handle_pending_action()
{
        if (!m_has_pending_action) {
                return false;
        }

        // A pin was tapped - the key that ended the input reading is not
        // a menu command, it is discarded
        const int action_id = m_pending_action;

        m_has_pending_action = false;

        run_action(action_id);

        // NOTE: This object may now be deleted!
        return true;
}

void ActionListState::note_action_started()
{
        m_tick_count_at_action = game_time::tick_count();
}

bool ActionListState::did_action_spend_turn() const
{
        return (m_tick_count_at_action >= 0) &&
                (game_time::tick_count() != m_tick_count_at_action);
}

void ActionListState::close_if_action_spent_time()
{
        if (!did_action_spend_turn()) {
                // The action was cancelled or refused - the screen stays
                // open, on the same entry (the list may still have
                // changed, e.g. an item was identified)
                on_list_changed();

                return;
        }

        if (!states::is_current_state(this)) {
                // The action opened a screen of its own on top of this one
                // (a throw being aimed, identifying an item, gaining a
                // trait, ...) - nothing can be told yet, and asking again
                // is left to pop_if_action_spent_turn, once this screen is
                // back in control
                return;
        }

        states::pop();

        // NOTE: This object is now deleted!
}

bool ActionListState::pop_if_action_spent_turn()
{
        // An action started here was carried out in a screen of its own (a
        // throw that was aimed out on the map), and that screen is gone -
        // this is where the question left open by close_if_action_spent_time
        // gets its answer. See the header for why it is asked here rather
        // than in on_resume, and why it must be before the input read.
        if (did_action_spend_turn()) {
                states::pop();

                // NOTE: This object is now deleted

                return true;
        }

        if (is_drawing_disabled()) {
                // The action was called off (a throw cancelled at its
                // marker) and this screen is in charge again.
                //
                // NOTE: Drawing stays paused all the way until HERE,
                // rather than resuming when the marker pops: the throw is
                // performed after that pop, so resuming any earlier paints
                // this screen over the map while the item is still in the
                // air - and in the case above the screen is closing
                // anyway, having never been seen again.
                enable_drawing();

                // Redrawn at once: the input read that follows BLOCKS, so
                // the map would otherwise be left on screen until the
                // player taps something
                states::draw();

                io::update_screen();
        }

        return false;
}

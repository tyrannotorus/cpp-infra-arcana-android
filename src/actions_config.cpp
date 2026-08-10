// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "actions_config.hpp"

#include <algorithm>

#include "SDL_keycode.h"
#include "action_bar.hpp"
#include "colors.hpp"
#include "common_text.hpp"
#include "config.hpp"
#include "draw_box.hpp"
#include "io.hpp"
#include "io_display.hpp"
#include "io_icons.hpp"
#include "io_internal.hpp"
#include "panel.hpp"
#include "pos.hpp"
#include "rect.hpp"
#include "scrollbar.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------

// Left column (in cells) reserved for the drag handle, then the action icon
static const int s_handle_w_cells = 3;
static const int s_icon_w_cells = 3;

static R content_px_rect()
{
        return io::panel_logical_px_rect(Panel::info_screen_content);
}

// -----------------------------------------------------------------------------
// ActionsConfigState
// -----------------------------------------------------------------------------
StateId ActionsConfigState::id() const
{
        return StateId::actions_config;
}

int ActionsConfigState::row_px_h() const
{
        // Two cells tall - a comfortable touch target
        return config::gui_cell_px_h() * 2;
}

int ActionsConfigState::max_scroll_px() const
{
        const int content_h =
                (int)action_bar::current_order().size() * row_px_h();

        return std::max(0, content_h - content_px_rect().h());
}

int ActionsConfigState::row_at(const P& logical_px) const
{
        const auto content = content_px_rect();

        if (!content.is_pos_inside(logical_px)) {
                return -1;
        }

        const int idx =
                (logical_px.y - content.p0.y + m_scroll_px) / row_px_h();

        if (idx >= (int)action_bar::current_order().size()) {
                return -1;
        }

        return idx;
}

void ActionsConfigState::draw()
{
        draw_box(panels::screen_box_area());

        const int screen_center_x = panels::center_x(Panel::screen);

        io::draw_text_center(
                " Actions ",
                Panel::screen,
                {screen_center_x, panels::screen_box_area().p0.y},
                colors::title());

        io::draw_text_center(
                " tap to show/hide, drag handle to reorder ",
                Panel::screen,
                {screen_center_x, panels::screen_box_area().p1.y},
                colors::title());

        const auto& order = action_bar::current_order();

        const auto content = content_px_rect();

        const int row_h = row_px_h();

        const int cell_w = config::gui_cell_px_w();

        io::set_clip_rect_to_panel(Panel::info_screen_content);

        for (size_t i = 0; i < order.size(); ++i) {
                const auto* const def = action_bar::action_def(order[i]);

                if (!def) {
                        continue;
                }

                const bool is_dragged = ((int)i == m_dragged_idx);

                int y0 = content.p0.y + ((int)i * row_h) - m_scroll_px;

                if (is_dragged) {
                        // The dragged row follows the finger
                        y0 = m_drag_px_y - (row_h / 2);
                }

                if (((y0 + row_h) < content.p0.y) ||
                    (y0 > content.p1.y)) {
                        continue;
                }

                const bool is_enabled =
                        action_bar::is_action_enabled(order[i]);

                const auto text_color =
                        is_dragged
                        ? colors::menu_highlight()
                        : (is_enabled
                                   ? colors::text()
                                   : colors::dark_gray());

                const int center_y = y0 + (row_h / 2);

                const int icon_size = (row_h * 3) / 4;

                // Drag handle
                io::draw_icon(
                        "drag_indicator",
                        {content.p0.x + ((s_handle_w_cells * cell_w) / 2),
                         center_y},
                        icon_size,
                        is_dragged
                                ? colors::menu_highlight()
                                : colors::dark_gray());

                // Action icon
                io::draw_icon(
                        def->icon,
                        {content.p0.x +
                                 (s_handle_w_cells * cell_w) +
                                 ((s_icon_w_cells * cell_w) / 2),
                         center_y},
                        icon_size,
                        text_color);

                // Label
                io::draw_text_at_px(
                        def->label,
                        {content.p0.x +
                                 ((s_handle_w_cells + s_icon_w_cells + 1) *
                                  cell_w),
                         center_y - (config::gui_cell_px_h() / 2)},
                        text_color,
                        io::DrawBg::no,
                        colors::black());

                // Checkbox
                io::draw_text_at_px(
                        is_enabled ? "[x]" : "[ ]",
                        {content.p1.x - (4 * cell_w),
                         center_y - (config::gui_cell_px_h() / 2)},
                        text_color,
                        io::DrawBg::no,
                        colors::black());
        }

        io::disable_clip_rect();

        scrollbar::draw_content_fades(content, m_scroll_px, max_scroll_px());

        // Drawn last, so the fades do not dim it
        scrollbar::draw(
                scrollbar::track_px_rect(Panel::info_screen_content),
                m_scroll_px,
                max_scroll_px());
}

bool ActionsConfigState::try_tap(const P& logical_px)
{
        const int idx = row_at(logical_px);

        if (idx < 0) {
                return false;
        }

        const auto& id = action_bar::current_order()[idx];

        action_bar::set_action_enabled(
                id,
                !action_bar::is_action_enabled(id));

        action_bar::save_to_config();

        return true;
}

bool ActionsConfigState::try_begin_touch_drag(const P& logical_px)
{
        // The scrollbar first - it overlays the right edge of the rows
        if (max_scroll_px() > 0) {
                const R track =
                        scrollbar::track_px_rect(Panel::info_screen_content);

                if (scrollbar::grab_px_rect(track)
                            .is_pos_inside(logical_px)) {
                        m_is_scrollbar_drag_active = true;

                        m_scrollbar_drag.begin(
                                track,
                                logical_px.y,
                                m_scroll_px,
                                max_scroll_px());

                        return true;
                }
        }

        const int idx = row_at(logical_px);

        if (idx < 0) {
                return false;
        }

        // Only the drag handle column starts a drag (dragging elsewhere
        // scrolls the list)
        const auto content = content_px_rect();

        const int handle_x1 =
                content.p0.x +
                (s_handle_w_cells * config::gui_cell_px_w());

        if (logical_px.x > handle_x1) {
                return false;
        }

        m_dragged_idx = idx;
        m_drag_px_y = logical_px.y;

        return true;
}

void ActionsConfigState::on_touch_drag_move(const P& logical_px)
{
        if (m_is_scrollbar_drag_active) {
                m_scroll_px =
                        m_scrollbar_drag.scroll_px_at(
                                scrollbar::track_px_rect(
                                        Panel::info_screen_content),
                                logical_px.y,
                                m_scroll_px,
                                max_scroll_px());

                return;
        }

        if (m_dragged_idx < 0) {
                return;
        }

        m_drag_px_y = logical_px.y;

        const auto content = content_px_rect();

        const int target =
                std::clamp(
                        (logical_px.y - content.p0.y + m_scroll_px) /
                                row_px_h(),
                        0,
                        (int)action_bar::current_order().size() - 1);

        if (target != m_dragged_idx) {
                action_bar::move_action(
                        action_bar::current_order()[m_dragged_idx],
                        target);

                m_dragged_idx = target;
        }
}

void ActionsConfigState::on_touch_drag_end()
{
        if (m_is_scrollbar_drag_active) {
                m_is_scrollbar_drag_active = false;

                return;
        }

        m_dragged_idx = -1;

        action_bar::save_to_config();
}

void ActionsConfigState::on_popped()
{
        action_bar::save_to_config();
}

void ActionsConfigState::update()
{
        const auto input = io::read_input();

        switch (input.key) {
        case SDLK_SPACE:
        case SDLK_ESCAPE:
        case SDLK_RETURN: {
                states::pop();
        } break;

        default:
                break;
        }
}

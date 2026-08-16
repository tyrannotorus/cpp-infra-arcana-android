// =============================================================================
// Copyright Werewolf Camp
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef ACTIONS_CONFIG_HPP
#define ACTIONS_CONFIG_HPP

#include <string>

#include "scrollbar.hpp"
#include "state.hpp"

struct P;

// -----------------------------------------------------------------------------
// Configuration screen for the touch action bar: every action is listed
// with its icon and name; tapping a row shows/hides the action on the bar,
// and dragging a row by its handle reorders the bar. The list scrolls with
// its scrollbar - the rows themselves are not a scroll zone, since a drag
// over them means reordering. Drawn in the game's own menu style.
// -----------------------------------------------------------------------------
class ActionsConfigState : public State
{
public:
        ActionsConfigState() = default;

        void draw() override;

        void update() override;

        void on_popped() override;

        StateId id() const override;


        bool try_tap(const P& logical_px) override;

        bool try_begin_touch_drag(const P& logical_px) override;

        void on_touch_drag_move(const P& logical_px) override;

        void on_touch_drag_end() override;

private:
        int row_px_h() const;

        int max_scroll_px() const;

        // Content row index at a logical pixel position (negative if none)
        int row_at(const P& logical_px) const;

        int m_scroll_px {0};

        scrollbar::Drag m_scrollbar_drag {};
        bool m_is_scrollbar_drag_active {false};

        // Index (within the action order) of the row being dragged,
        // negative when no drag is active
        int m_dragged_idx {-1};

        // Current finger y (logical pixels) while dragging
        int m_drag_px_y {0};
};

#endif  // ACTIONS_CONFIG_HPP

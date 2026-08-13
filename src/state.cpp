// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "state.hpp"

#include <algorithm>
#include <iterator>
#include <utility>
#include <vector>

#include "action_bar.hpp"
#include "context_pins.hpp"
#include "debug.hpp"
#include "dpad.hpp"
#include "draw_box.hpp"
#include "io.hpp"
#include "io_display.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static std::vector<std::unique_ptr<State>> s_current_states;

bool State::has_close_button() const
{
        return screen_has_close_button(id());
}

// The lowest state that is currently drawn: everything from here up is
// drawn, everything below it is covered. Shared by the drawing and the
// graphics cycling passes, which must agree on it exactly.
static std::vector<std::unique_ptr<State>>::iterator first_drawn_state()
{
        auto it = std::end(s_current_states);

        while (it != std::begin(s_current_states)) {
                --it;

                const auto& state_ptr = *it;

                // If not drawn overlayed, this state is the bottom layer
                // (but only if it has been started and drawing is not paused
                // for it).
                if (!state_ptr->draw_overlayed() &&
                    state_ptr->has_started() &&
                    !state_ptr->is_drawing_disabled()) {
                        break;
                }
        }

        return it;
}

static void run_state_iteration()
{
        const State* const current_state = states::current_state();

        states::start();

        if ((states::current_state()) != current_state) {
                // The state that was the current state before running "start"
                // has been removed, or another state has been pushed on it. In
                // either case rerun the iteration from start (otherwise "draw"
                // and "update" will run for a different state than the one that
                // was started).
                return;
        }

        states::draw();
        io::update_screen();

        states::update();
}

//-----------------------------------------------------------------------------
// states
//-----------------------------------------------------------------------------
namespace states
{
void init()
{
        TRACE_FUNC_BEGIN;

        cleanup();

        TRACE_FUNC_END;
}

void cleanup()
{
        TRACE_FUNC_BEGIN;

        s_current_states.resize(0);

        TRACE_FUNC_END;
}

void run()
{
        TRACE_FUNC_BEGIN;

        while (!is_empty()) {
                run_state_iteration();
        }

        TRACE_FUNC_END;
}

void run_until_state_done(std::unique_ptr<State> state)
{
        TRACE_FUNC_BEGIN;

        State* state_ptr = state.get();

        push(std::move(state));

        while (contains_state(state_ptr)) {
                run_state_iteration();
        }

        TRACE_FUNC_END;
}

void start()
{
        while (!is_empty() && !s_current_states.back()->has_started()) {
                auto& state = s_current_states.back();

                state->set_started();

                // NOTE: This may cause states to be pushed/popped - do not use
                // the "state" pointer beyond this call!
                state->on_start();
        }
}

void cycle_graphics(const io::GraphicsCycle cycle)
{
        if (is_empty()) {
                return;
        }

        // Cycle graphics in every state from the bottom drawn layer onward.
        for (auto cycle_from = first_drawn_state();
             cycle_from != std::end(s_current_states);
             ++cycle_from) {
                const auto& state_ptr = *cycle_from;

                // Do NOT cycle graphics in states which are not yet started
                // (they may need to set up menus etc in their start function,
                // and expect the chance to do so before cycling is called).
                // Also do not cycle graphics if drawing is disabled.

                if (state_ptr->has_started() &&
                    !state_ptr->is_drawing_disabled()) {
                        state_ptr->cycle_graphics(cycle);
                }
        }
}

void draw()
{
        if (is_empty()) {
                return;
        }

        io::clear_screen();

        // Draw every state from the bottom drawn layer onward.
        for (auto draw_from = first_drawn_state();
             draw_from != std::end(s_current_states);
             ++draw_from) {
                const auto& state_ptr = *draw_from;

                // Do NOT draw states which are not yet started (they may need
                // to set up menus etc in their start function, and expect the
                // chance to do so before drawing is called). Also do not draw
                // if drawing is disabled.

                if (state_ptr->has_started() &&
                    !state_ptr->is_drawing_disabled()) {
                        state_ptr->draw();
                }
        }

        // The touch action bar is always drawn on top of every state, and
        // the context pins and the movement pad with it - they are an
        // overlay of whatever is being played, not content of a screen (see
        // context_pins, dpad)
        dpad::draw();

        action_bar::draw();

        context_pins::draw();
}

bool draw_map_display()
{
        if (is_empty()) {
                return false;
        }

        const auto first = first_drawn_state();

        // Checked BEFORE anything is cleared - a partial redraw is all or
        // nothing, and the caller falls back to a full draw
        for (auto it = first; it != std::end(s_current_states); ++it) {
                const auto& state_ptr = *it;

                if (!state_ptr->has_started() ||
                    state_ptr->is_drawing_disabled()) {
                        continue;
                }

                if (!state_ptr->has_map_display_draw()) {
                        return false;
                }
        }

        io::clear_display_texture(io::Display::map);

        for (auto it = first; it != std::end(s_current_states); ++it) {
                const auto& state_ptr = *it;

                if (state_ptr->has_started() &&
                    !state_ptr->is_drawing_disabled()) {
                        state_ptr->draw_map_display();
                }
        }

        // NOTE: The action bar and the context pins are NOT redrawn - they
        // live on the bar display, which still holds them.

        return true;
}

void on_window_resized()
{
        for (auto& state : s_current_states) {
                state->on_window_resized();
        }
}

void update()
{
        if (is_empty()) {
                return;
        }

        s_current_states.back()->update();
}

void push(std::unique_ptr<State> state)
{
        TRACE_FUNC_BEGIN;

        // Pause the current state
        if (!is_empty()) {
                s_current_states.back()->on_pause();
        }

        s_current_states.push_back(std::move(state));

        s_current_states.back()->on_pushed();

        io::clear_input();

        TRACE_FUNC_END;
}

void pop()
{
        TRACE_FUNC_BEGIN;

        if (is_empty()) {
                TRACE_FUNC_END;

                return;
        }

        s_current_states.back()->on_popped();

        s_current_states.pop_back();

        if (!is_empty()) {
                s_current_states.back()->on_resume();
        }

        io::clear_input();

        TRACE_FUNC_END;
}

void pop_all()
{
        TRACE_FUNC_BEGIN;

        while (!is_empty()) {
                s_current_states.back()->on_popped();

                s_current_states.pop_back();
        }

        TRACE_FUNC_END;
}

bool contains_state(const StateId id)
{
        return (
                std::any_of(
                        std::cbegin(s_current_states),
                        std::cend(s_current_states),
                        [id](const auto& state) {
                                return state->id() == id;
                        }));
}

bool contains_state(const State* const state)
{
        return (
                std::any_of(
                        std::cbegin(s_current_states),
                        std::cend(s_current_states),
                        [state](const auto& state_found) {
                                return state_found.get() == state;
                        }));
}

void pop_until(const StateId id)
{
        TRACE_FUNC_BEGIN;

        if (is_empty() || !contains_state(id)) {
                ASSERT(false);

                return;
        }

        while (s_current_states.back().get()->id() != id) {
                pop();
        }

        TRACE_FUNC_END;
}

bool is_current_state(const State* const state)
{
        if (is_empty()) {
                return false;
        }

        return state == s_current_states.back().get();
}

State* current_state()
{
        if (is_empty()) {
                return nullptr;
        }

        return s_current_states.back().get();
}

bool is_empty()
{
        return s_current_states.empty();
}

}  // namespace states

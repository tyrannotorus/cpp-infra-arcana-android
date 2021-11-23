// =============================================================================
// Copyright 2011-2021 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef STATE_HPP
#define STATE_HPP

#include <memory>

namespace io
{
enum class GraphicsCycle;
}  // namespace io

enum class StateId
{
        browse_highscore_entry,
        browse_spells,
        config,
        game,
        highscore,
        inventory,
        main_menu,
        manual,
        manual_page,
        marker,
        message_history,
        new_game,
        new_level,
        pick_background,
        pick_background_occultist,
        pick_name,
        pick_trait,
        player_character_descr,
        popup,
        postmortem_info,
        remove_trait,
        view_actor,
        view_minimap,
        win_game,  // TODO: This should just be a popup
};

class State
{
public:
        virtual ~State() = default;

        // Executed immediately when the state is pushed.
        virtual void on_pushed() {}

        // Executed the first time that the state becomes the current state.
        // Sometimes multiple states may be pushed in a sequence, and one of the
        // later states may want to perform actions only when it actually
        // becomes the current state.
        virtual void on_start() {}

        // Executed immediately when the state is popped.
        // This should only be used for cleanup, do not push or pop other states
        // from this call (this is not supported).
        virtual void on_popped() {}

        // This is called continuously, and can be used for cycling what to draw
        // (e.g. animating the color of a monster, or scrolling text, etc).
        virtual void cycle_graphics(const io::GraphicsCycle cycle)
        {
                (void)cycle;
        }

        virtual void draw() {}

        // If true, this state is drawn overlayed on the state(s) below. This
        // can be used for example to draw a marker state on top of the map.
        virtual bool draw_overlayed() const
        {
                return false;
        }

        virtual void on_window_resized() {}

        // Read input, process game logic etc.
        virtual void update() {}

        // All states above have been popped
        virtual void on_resume() {}

        // Another state is pushed on top
        virtual void on_pause() {}

        bool has_started() const
        {
                return m_has_started;
        }

        void set_started()
        {
                m_has_started = true;
        }

        virtual StateId id() const = 0;

private:
        bool m_has_started {false};
};

namespace states
{
void init();

void cleanup();

void run();

void run_until_state_done(std::unique_ptr<State> state);

void start();

void cycle_graphics(io::GraphicsCycle cycle);

void draw();

void on_window_resized();

void update();

void push(std::unique_ptr<State> state);

void pop();

void pop_all();

bool is_empty();

bool is_current_state(const State* state);

void pop_until(StateId id);

bool contains_state(StateId id);

bool contains_state(const State* state);

}  // namespace states

#endif  // STATE_HPP

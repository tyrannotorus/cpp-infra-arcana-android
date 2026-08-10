// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef STATE_HPP
#define STATE_HPP

#include <memory>

struct P;

namespace io
{
enum class GraphicsCycle;
}  // namespace io

enum class StateId
{
        actions_config,
        alpha_notice,
        browse_highscore_entry,
        browse_spells,
        credits,
        game,
        game_menu,
        game_over_summary,
        highscore,
        hint,
        intro_story,
        inventory,
        main_menu,
        manual,
        marker,
        message_history,
        new_game,
        new_level,
        options,
        options_submenu,
        pick_background,
        pick_background_occultist,
        pick_name,
        pick_trait,
        player_character_descr,
        popup,
        query_number,
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

        // Whether everything this state draws that can CHANGE from tile
        // animation lives on the map display, so that it can be refreshed
        // without redrawing the rest of the interface (see
        // states::draw_map_display).
        virtual bool has_map_display_draw() const
        {
                return false;
        }

        // Redraws only this state's map display content. Only called when
        // has_map_display_draw() is true.
        virtual void draw_map_display() {}

        // If true, this state is drawn overlayed on the state(s) below. This
        // can be used for example to draw a marker state on top of the map.
        virtual bool draw_overlayed() const
        {
                return false;
        }

        virtual void on_window_resized() {}

        // Read input, process game logic etc.
        virtual void update() {}

        // The map view was panned by dragging (e.g. the free look marker
        // state syncs its marker to the view center on this)
        virtual void on_map_panned() {}

        // A tap at a position (logical screen pixels). Returns true if the
        // state handled the tap directly (e.g. toggling the tapped row) -
        // then no key is synthesized for it.
        virtual bool try_tap(const P& logical_px)
        {
                (void)logical_px;

                return false;
        }

        // Direct touch dragging from an explicit HANDLE (a drag handle, a
        // scrollbar) - the ONLY kind of drag zone there is. Content areas
        // are never draggable: over them a gesture is a swipe, and text is
        // scrolled with its scrollbar. If this returns true for the
        // touch start position, subsequent finger movement is reported via
        // on_touch_drag_move, and release via on_touch_drag_end - and the
        // gesture is not interpreted as scrolling or swiping.
        virtual bool try_begin_touch_drag(const P& logical_px)
        {
                (void)logical_px;

                return false;
        }

        virtual void on_touch_drag_move(const P& logical_px)
        {
                (void)logical_px;
        }

        virtual void on_touch_drag_end() {}

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

        bool is_drawing_disabled() const
        {
                return m_is_drawing_disabled;
        }

        void disable_drawing()
        {
                m_is_drawing_disabled = true;
        }

        void enable_drawing()
        {
                m_is_drawing_disabled = false;
        }

        virtual StateId id() const = 0;

        // Whether this screen's fullscreen border carries the [ x ] close
        // control, and whether tapping there does anything. Answered from
        // the state's id by default (see screen_has_close_button) -
        // override only where ONE instance has to differ from the rest of
        // its kind, e.g. the first page of the winning ending, which is
        // not something to back out of.
        virtual bool has_close_button() const;

private:
        bool m_has_started {false};
        bool m_is_drawing_disabled {false};
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

// Redraws ONLY the map display, for animation that changes nothing else -
// tile graphics cycling and flash animations. GameState::cycle_graphics
// touches terrain and actors and nothing more, so the side stats panel,
// the message log and the action bar still hold exactly what they held
// last frame; their textures are left alone.
//
// Returns false if any currently drawn state has content that cannot be
// refreshed this way - NOTHING is drawn then, and the caller must fall
// back to a full draw().
bool draw_map_display();

void on_window_resized();

void update();

void push(std::unique_ptr<State> state);

void pop();

void pop_all();

bool is_empty();

bool is_current_state(const State* state);

State* current_state();

void pop_until(StateId id);

bool contains_state(StateId id);

bool contains_state(const State* state);

}  // namespace states

#endif  // STATE_HPP

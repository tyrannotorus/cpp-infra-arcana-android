// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef ACTION_LIST_STATE_HPP
#define ACTION_LIST_STATE_HPP

#include <string>
#include <vector>

#include "browser.hpp"
#include "descr_column.hpp"
#include "panel.hpp"
#include "rect.hpp"
#include "state.hpp"

struct P;

// -----------------------------------------------------------------------------
// The standard "pick a thing and do something with it" screen: a browsable
// list on one side, the marked entry's description - scrollable, with a
// draggable scrollbar - beside it, and the actions available for that entry
// as tappable [ pins ] in the corner of the description.
//
// The inventory, the throw list and the spell list are all this screen.
// What differs between them is only what the list holds, what the
// description says, and which actions the pins offer.
//
// The interaction rule they share, and the reason the pins exist: a tap on
// a row only MARKS it. Nothing is ever done to the marked thing except
// through a pin, so no item is dropped and no spell is cast by a stray
// finger. (A screen that offers no pins keeps the plain menu behaviour -
// a tap marks AND selects.)
// -----------------------------------------------------------------------------

struct ActionPin
{
        // Identifies the action to the screen that offered it. Each screen
        // has its own set of actions and its own enum for them (see
        // ItemActionId, SpellActionId) - this is that enum cast to int,
        // and it is handed straight back to run_action.
        int id {0};

        std::string label {};

        // Hit area in logical screen pixels, recorded when drawn
        R px_rect {-1, -1, -1, -1};
};

class ActionListState : public State
{
public:
        ActionListState() = default;

        ~ActionListState() override = default;

        void on_window_resized() override;

        // The description column's scrollbar is the only drag zone
        bool try_begin_touch_drag(const P& logical_px) override;

        void on_touch_drag_move(const P& logical_px) override;

        void on_touch_drag_end() override;

protected:
        // --- What the screen is made of ---

        // Whether this screen offers pins at all (see the interaction rule
        // above)
        virtual bool has_action_pins() const
        {
                return false;
        }

        // The actions for the marked entry, primary first and the one not
        // wanted by accident last
        virtual std::vector<ActionPin> marked_entry_actions() const
        {
                return {};
        }

        // Carries out a pin's action. NOTE: May delete this object (the
        // action may close the screen) - do not touch any members after
        // calling it.
        virtual void run_action(int action_id) = 0;

        // Called after an action that did NOT spend the turn - the screen
        // stays open, but its list may need re-making (an item was
        // identified, a stack was split by lighting one, ...)
        virtual void on_list_changed() {}

        // --- Drawing ---

        // Fade-to-black hints at the list edges where it continues past
        // the shown page (the list is browsed by swiping - there is no
        // scrollbar on it, see MenuPageState)
        void draw_list_fades() const;

        // Call from draw(), BEFORE the description is drawn (the text
        // needs to know how many of its bottom rows the pins take)
        void prepare_action_pins();

        // Call at the END of draw() - the pins sit on top of the faded out
        // end of the description text
        void draw_action_pins() const;

        // Text wrap width of the description column
        int descr_text_w() const;

        // --- Input ---

        // Call next in update(), after reading input: runs a pin tapped
        // since the last update. Returns true if this object may have been
        // deleted - just return from update() then.
        bool handle_pending_action();

        // --- Running an action, and closing when it spends the turn ---
        //
        // These screens close when something done from them moves the
        // world on, and stay open when it does not. That is ONE question -
        // "has the turn been spent since this action started" - asked at
        // two different moments, because an action may finish either
        // within its own call (equipping, dropping) or much later, in a
        // screen of its own (a throw, aimed out on the map).
        //
        // So: bracket every action with note_action_started() and
        // close_if_action_spent_time(). When the answer cannot be known
        // yet, the latter simply leaves it to pop_if_action_spent_turn(),
        // which asks again at the top of the next update.

        // Opens the bracket: records when the action began
        void note_action_started();

        // Closes the bracket: closes the screen if the action spent game
        // time, and leaves it to the next update if that cannot be told
        // yet (the action opened a screen of its own).
        // NOTE: May delete this object.
        void close_if_action_spent_time();

        // Call at the VERY TOP of update(), before reading input. Settles
        // what became of an action this screen started out on the map (a
        // throw that was aimed): the screen closes if the turn was spent,
        // and otherwise takes back over, drawing again. Returns true if
        // this object was deleted - just return from update() then.
        //
        // NOTE: Both of those orderings are load bearing. This cannot be
        // settled in on_resume, because the throw is performed AFTER its
        // marker pops - nothing has happened yet at that point, and this
        // screen drawing again there would paint over the map while the
        // item is still in the air. And it cannot wait until after the
        // input read, because that read BLOCKS: the screen would come
        // back up and sit there waiting for a tap, having already spent
        // the player's turn.
        bool pop_if_action_spent_turn();

        bool try_tap(const P& logical_px) final;

        MenuBrowser m_browser;

        DescrColumn m_descr {Panel::inventory_descr};

private:
        // Lays the pins out in the bottom right corner of the description
        // column, right aligned, and records their hit areas. Returns the
        // number of rows they occupy.
        int layout_action_pins(std::vector<ActionPin>& pins) const;

        // The one question the closing rules are built on: has the world
        // moved on since the action started (see note_action_started)
        bool did_action_spend_turn() const;

        // The pins as last drawn, for hit testing
        std::vector<ActionPin> m_drawn_pins {};

        // A tapped pin, run from update(). NOTE: The action must NOT be
        // run from the tap handler itself - that runs inside input
        // reading, and a state pushed from there would not start until
        // this screen's update returns (see io::read_input).
        bool m_has_pending_action {false};
        int m_pending_action {0};

        // Tick count when an action was last started from here. An action
        // that ends up spending the turn closes this screen, even when it
        // was carried out in a screen of its own (a throw being aimed).
        // Negative = no action started yet.
        int m_tick_count_at_action {-1};
};

#endif  // ACTION_LIST_STATE_HPP

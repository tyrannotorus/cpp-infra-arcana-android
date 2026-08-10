// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef GAME_HPP
#define GAME_HPP

#include <string>
#include <utility>
#include <vector>

#include "global.hpp"
#include "io.hpp"
#include "state.hpp"
#include "text_page.hpp"
#include "time.hpp"

namespace actor
{
class Actor;
}  // namespace actor

struct HistoryEvent
{
        HistoryEvent() = default;

        HistoryEvent(std::string history_msg, const int turn_nr) :
                msg(std::move(history_msg)),
                turn(turn_nr) {}

        std::string msg {};
        int turn {0};
};

namespace game
{
void init();

void save();
void load();

int clvl();
int xp_pct();
int xp_accumulated();
IsWin is_win();
void set_is_win();
TimeData start_time();

void player_discover_monster(actor::Actor& actor);
int mon_shock_lvl_to_xp(MonShockLvl shock_lvl);

void on_mon_killed(actor::Actor& actor);

void set_start_time_to_now();

void incr_player_xp(int xp_gained, Verbose verbose = Verbose::yes);

void decr_player_xp(int xp_lost);

// This function has no side effects except for incrementing the clvl value
void incr_clvl_number();

void add_history_event(const std::string& msg);

const std::vector<HistoryEvent>& history();

}  // namespace game

// -----------------------------------------------------------------------------
// Game state
// -----------------------------------------------------------------------------
class GameState : public State
{
public:
        GameState(GameEntryMode entry_mode) :

                m_entry_mode(entry_mode)
        {}

        void on_start() override;

        void cycle_graphics(io::GraphicsCycle cycle) override;

        void draw() override;

        bool has_map_display_draw() const override;

        void draw_map_display() override;

        void update() override;

        void on_map_panned() override;

        void on_resume() override;

        StateId id() const override;

private:
        void query_quit();

        const GameEntryMode m_entry_mode;
};

// -----------------------------------------------------------------------------
// Win game state
// -----------------------------------------------------------------------------
// The winning ending, read one section at a time - the same page the
// opening story is told on (see IntroStoryState), and its bookend.
//
// Tapping continues to the next section, and the [ x ] control steps back
// to the one before. The FIRST section has no [ x ]: the ending is not
// something to back out of, and there is nothing behind it to back out to
// (the game is over - see Trapezohedron::pre_pickup_hook). Continuing past
// the last section fades out and leaves the page, revealing the game
// summary underneath.
class WinGameState : public TextPageState
{
public:
        WinGameState() = default;

        StateId id() const override;

        bool has_close_button() const override;

protected:
        std::string page_title() const override;

        std::string page_text() const override;

        void on_confirmed() override;

        void on_cancelled() override;

private:
        size_t m_section_idx {0};
};

#endif  // GAME_HPP

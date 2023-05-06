// =============================================================================
// Copyright 2011-2023 Martin Törnqvist <m.tornq@gmail.com>
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

        void update() override;

        StateId id() const override;

private:
        void query_quit();

        const GameEntryMode m_entry_mode;
};

// -----------------------------------------------------------------------------
// Win game state
// -----------------------------------------------------------------------------
class WinGameState : public State
{
public:
        WinGameState() = default;

        void draw() override;

        void update() override;

        StateId id() const override;
};

#endif  // GAME_HPP

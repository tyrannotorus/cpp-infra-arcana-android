// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef GAME_SUMMARY_DATA_HPP
#define GAME_SUMMARY_DATA_HPP

#include <string>
#include <vector>

#include "colors.hpp"
#include "game.hpp"
#include "highscore.hpp"
#include "msg_log.hpp"
#include "player_bon.hpp"
#include "property_handler.hpp"

class InsSympt;

namespace game_summary_data
{
struct TraitData
{
        std::string name {};
        std::string descr {};
};

struct GameSummaryData
{
        HighscoreEntry highscore {};

        std::string player_name {};
        int xp {0};
        int clvl {0};
        int dlvl {0};
        int turns {0};
        int insanity {0};
        std::string background_title {};
        int nr_kills_tot {0};
        std::vector<std::string> unique_monsters_killed {};
        std::vector<const InsSympt*> insanity_symptons {};
        std::vector<TraitData> current_traits {};
        std::vector<player_bon::TraitLogEntry> trait_log {};
        std::vector<HistoryEvent> player_history {};
        std::vector<Msg> msg_history {};
        std::vector<prop::PropListEntry> properties {};
        std::vector<ColoredString> potion_knowledge {};
        std::vector<ColoredString> scroll_knowledge {};
};

// Collects data from the current game session, to be used for presenting a
// player character description or a game over summary. The data collected here
// shall be complete for these purposes, there shall be no need to fetch data
// from any other source (such as global variables).
GameSummaryData collect();

}  // namespace game_summary_data

#endif  // GAME_SUMMARY_DATA_HPP

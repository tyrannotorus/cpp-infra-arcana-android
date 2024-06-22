// =============================================================================
// Copyright 2011-2024 Martin Törnqvist <m.tornq@gmail.com>
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
struct ItemKnowledgeData
{
        std::string name {};
        Color item_color;
        bool is_identified {false};
};

struct TraitData
{
        std::string name {};
        std::string descr {};
};

struct InventoryItemData
{
        std::string slot_name {};
        std::string item_name {};
        bool is_identified {};
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
        int current_shock {0};
        int total_shock {0};
        int total_shock_from_src[(size_t)ShockSrc::END];
        // Each spell domain is a separate "shock source", this represents the
        // aggregated shock from casting spells (for convenient presentation):
        int total_shock_from_casting_spells {0};
        std::string background_title {};
        int nr_kills_tot {0};
        std::vector<std::string> unique_monsters_killed {};
        std::vector<const InsSympt*> insanity_symptons {};
        std::vector<TraitData> current_traits {};
        std::vector<player_bon::TraitLogEntry> trait_log {};
        std::vector<HistoryEvent> player_history {};
        std::vector<Msg> msg_history {};
        std::vector<prop::PropListEntry> properties {};
        std::vector<std::vector<ItemKnowledgeData>> item_knowledge {};
        std::vector<InventoryItemData> inventory {};
};

// Collects data from the current game session, to be used for presenting a
// player character description or a game over summary. The data collected here
// shall be complete for these purposes, there shall be no need to fetch data
// from any other source (such as global variables).
GameSummaryData collect();

}  // namespace game_summary_data

#endif  // GAME_SUMMARY_DATA_HPP

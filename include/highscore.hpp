// =============================================================================
// Copyright 2011-2021 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef HIGHSCORE_HPP
#define HIGHSCORE_HPP

#include <string>
#include <vector>

#include "browser.hpp"
#include "global.hpp"
#include "info_screen_state.hpp"
#include "player_bon.hpp"
#include "state.hpp"

struct HighscoreEntry
{
        int calculate_score() const;

        std::string game_summary_file_path {};
        std::string date {};
        std::string name {};
        int xp {0};
        int lvl {0};
        int dlvl {0};
        int turn_count {0};
        int ins {0};
        IsWin is_win {IsWin::no};
        Bg bg {Bg::END};
        OccultistDomain player_occultist_domain {OccultistDomain::END};
        bool is_latest_entry {false};
};

namespace highscore
{
void init();
void cleanup();

// NOTE: All this does is construct a HighscoreEntry object, populated with
// highscore info based on the current game - it has no side effects
HighscoreEntry make_entry_from_current_game_data(
        const std::string& game_summary_file_path,
        IsWin is_win);

void append_entry_to_highscores_file(HighscoreEntry& entry);

std::vector<HighscoreEntry> entries_sorted();

}  // namespace highscore

class BrowseHighscore : public State
{
public:
        BrowseHighscore() = default;

        void on_start() override;

        void on_window_resized() override;

        void draw() override;

        void update() override;

        bool draw_overlayed() const override
        {
                // If there are no entries, we draw an overlayed popup
                return m_entries.empty();
        }

        StateId id() const override;

private:
        std::vector<HighscoreEntry> m_entries;

        MenuBrowser m_browser;
};

class BrowseHighscoreEntry : public InfoScreenState
{
public:
        BrowseHighscoreEntry(std::string file_path);

        void on_start() override;

        void on_window_resized() override;

        void draw() override;

        void update() override;

        StateId id() const override;

private:
        std::string title() const override
        {
                return "Game summary";
        }

        InfoScreenType type() const override
        {
                return InfoScreenType::scrolling;
        }

        void read_file();

        const std::string m_file_path;

        std::vector<std::string> m_lines;

        int m_top_idx;
};

#endif  // HIGHSCORE_HPP

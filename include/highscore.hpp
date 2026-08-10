// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
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
// Constructs a HighscoreEntry data object based on data from the current game
// session.
HighscoreEntry make_entry_from_current_session(
        const std::string& game_summary_file_path = "");

void append_entry_to_highscores_file(HighscoreEntry& entry);

std::vector<HighscoreEntry> entries_sorted();

}  // namespace highscore

enum class IsAfterGameOver
{
        no,
        yes
};

// The high score table. Shown after dying, where it is the last thing
// between the player and the title screen (a tap continues), and from the
// main menu's graveyard, where entries are browsed and opened.
class BrowseHighscore : public State
{
public:
        BrowseHighscore() = default;

        BrowseHighscore(const IsAfterGameOver is_after_game_over) :
                m_is_after_game_over(is_after_game_over) {}

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

        const IsAfterGameOver m_is_after_game_over {IsAfterGameOver::no};
};

class BrowseHighscoreEntry : public InfoScreenState
{
public:
        BrowseHighscoreEntry(std::string file_path);

        void on_start() override;

        void on_window_resized() override;

        void draw() override;

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

        ColoredString content_line(int line_idx) const override;

        void read_file();

        const std::string m_file_path;

        std::vector<std::string> m_lines;

        int get_lines_total() const override
        {
                return m_lines.size();
        }
};

#endif  // HIGHSCORE_HPP

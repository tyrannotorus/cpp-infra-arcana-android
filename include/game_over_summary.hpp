// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef GAME_OVER_SUMMARY_HPP
#define GAME_OVER_SUMMARY_HPP

#include <string>
#include <vector>

#include "colors.hpp"
#include "info_screen_state.hpp"
#include "state.hpp"

namespace game_summary_data
{
struct GameSummaryData;
}  // namespace game_summary_data

// This is a class for presenting a game over summary, it can present this in
// the following formats:
//
// * As its own game state in a separate screen.
// * Written to a text file.
// * Copied to the clipboard.
//
class GameOverSummary : public InfoScreenState
{
public:
        // NOTE: A setup function must be called before the state runs.

        void setup(const game_summary_data::GameSummaryData& data);

        // Used for lines read from a text file (showing game over summary for
        // previous characters).
        void setup(std::vector<ColoredString> lines);

        void dump_to_file(const std::string& path) const;

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

        std::vector<ColoredString> m_lines {};

        int get_lines_total() const override
        {
                return m_lines.size();
        }
};

#endif  // GAME_OVER_SUMMARY_HPP

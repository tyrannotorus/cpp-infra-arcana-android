// =============================================================================
// Copyright 2011-2023 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef CHARACTER_DESCR_HPP
#define CHARACTER_DESCR_HPP

#include <string>
#include <vector>

#include "colors.hpp"
#include "info_screen_state.hpp"
#include "state.hpp"

namespace game_summary_data
{
struct GameSummaryData;
}  // namespace game_summary_data

// This is a class for presenting a "player character description", it can
// present this in the following formats:
//
// * As its own game state in a separate screen.
// * Copied to the clipboard.
//
class CharacterDescr : public InfoScreenState
{
public:
        CharacterDescr() :
                m_top_idx(0) {}

        // NOTE: This must be called before the state runs.
        void setup(const game_summary_data::GameSummaryData& data);

        void draw() override;

        void update() override;

        StateId id() const override;

private:
        std::string title() const override
        {
                return "Character description";
        }

        InfoScreenType type() const override
        {
                return InfoScreenType::scrolling;
        }

        std::vector<ColoredString> m_lines;

        int m_top_idx;
};

#endif  // CHARACTER_DESCR_HPP

// =============================================================================
// Copyright 2011-2024 Martin Törnqvist <m.tornq@gmail.com>
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

class CharacterDescr : public InfoScreenState
{
public:
        CharacterDescr() = default;

        // NOTE: This must be called before the state runs.
        void setup(const game_summary_data::GameSummaryData& data);

        void draw() override;

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

        int get_lines_total() const override
        {
                return m_lines.size();
        }
};

#endif  // CHARACTER_DESCR_HPP

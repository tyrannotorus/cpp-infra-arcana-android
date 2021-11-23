// =============================================================================
// Copyright 2011-2021 Martin Törnqvist <m.tornq@gmail.com>
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

class CharacterDescr : public InfoScreenState
{
public:
        CharacterDescr() :

                m_top_idx(0)
        {}

        void on_start() override;

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

// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef POSTMORTEM_HPP
#define POSTMORTEM_HPP

#include <string>
#include <utility>
#include <vector>

#include "colors.hpp"
#include "info_screen_state.hpp"
#include "state.hpp"

class PostmortemInfo : public InfoScreenState
{
public:
        PostmortemInfo(std::vector<ColoredString> lines) :
                m_lines(std::move(lines)) {}

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

        const std::vector<ColoredString> m_lines {};
        int m_top_idx {0};
};

#endif  // POSTMORTEM_HPP

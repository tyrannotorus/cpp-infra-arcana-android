// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef CREDITS_HPP
#define CREDITS_HPP

#include "info_screen_state.hpp"

// The credits page, reachable from the title screen
class CreditsState : public InfoScreenState
{
public:
        void draw() override;

        StateId id() const override
        {
                return StateId::credits;
        }

protected:
        std::string title() const override;

        InfoScreenType type() const override;

        int get_lines_total() const override;
};

#endif  // CREDITS_HPP

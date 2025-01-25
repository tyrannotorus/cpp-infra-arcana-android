// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef INFO_SCREEN_STATE_HPP
#define INFO_SCREEN_STATE_HPP

#include <string>

#include "state.hpp"

enum class InfoScreenType
{
        scrolling,
        single_screen
};

class InfoScreenState : public State
{
public:
        InfoScreenState() = default;

        void update() override;

protected:
        void draw_interface() const;

        virtual std::string title() const = 0;

        virtual InfoScreenType type() const = 0;

        virtual int get_lines_total() const = 0;

        int m_top_idx {0};
};

#endif  // INFO_SCREEN_STATE_HPP

// =============================================================================
// Copyright 2011-2024 Martin Törnqvist <m.tornq@gmail.com>
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

protected:
        void draw_interface() const;

        virtual std::string title() const = 0;

        virtual InfoScreenType type() const = 0;
};

#endif  // INFO_SCREEN_STATE_HPP

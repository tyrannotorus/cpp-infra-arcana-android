// =============================================================================
// Copyright 2011-2021 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef MAIN_MENU_HPP
#define MAIN_MENU_HPP

#include "browser.hpp"
#include "state.hpp"

class MainMenuState : public State
{
public:
        MainMenuState();

        ~MainMenuState();

        void draw() override;

        void update() override;

        void on_start() override;

        void on_resume() override;

        StateId id() const override;

private:
        MenuBrowser m_browser;
};

#endif  // MAIN_MENU_HPP

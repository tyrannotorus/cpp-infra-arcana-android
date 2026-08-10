// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef MAIN_MENU_HPP
#define MAIN_MENU_HPP

#include "browser.hpp"
#include "state.hpp"

// Shown on top of the title screen at boot, before anything else: this is
// an alpha build, not the finished game. Sets expectations BEFORE first
// impressions form - the difference between "this sucks" and "this alpha
// is promising". Dismissed by any input, fading to black into the title.
class AlphaNoticeState : public State
{
public:
        void draw() override;

        void update() override;

        StateId id() const override
        {
                return StateId::alpha_notice;
        }
};

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

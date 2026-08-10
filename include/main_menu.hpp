// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef MAIN_MENU_HPP
#define MAIN_MENU_HPP

#include "browser.hpp"
#include "state.hpp"
#include "text_page.hpp"

// Shown on top of the title screen at boot, before anything else: this is
// an alpha build, not the finished game. Sets expectations BEFORE first
// impressions form - the difference between "this sucks" and "this alpha
// is promising". Dismissed by any input, fading to black into the title.
class AlphaNoticeState : public TextPageState
{
public:
        StateId id() const override
        {
                return StateId::alpha_notice;
        }

protected:
        std::string page_title() const override;

        std::string page_text() const override;

        void on_confirmed() override;

        void on_cancelled() override;
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

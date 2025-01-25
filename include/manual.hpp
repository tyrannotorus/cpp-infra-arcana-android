// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef MANUAL_HPP
#define MANUAL_HPP

#include <algorithm>
#include <string>
#include <vector>

#include "browser.hpp"
#include "info_screen_state.hpp"
#include "state.hpp"

struct ManualPage
{
        std::string title {};
        std::vector<std::string> lines {};
};

class BrowseManual : public State
{
public:
        BrowseManual() = default;

        void on_start() override;

        void draw() override;

        void on_window_resized() override;

        void update() override;

        StateId id() const override;

private:
        MenuBrowser m_browser;

        std::vector<std::string> m_raw_lines;

        std::vector<ManualPage> m_pages;
};

class BrowseManualPage : public InfoScreenState
{
public:
        BrowseManualPage(const ManualPage& page) :
                m_page(page) {}

        void draw() override;

        StateId id() const override;

private:
        std::string title() const override;

        InfoScreenType type() const override
        {
                return InfoScreenType::scrolling;
        }

        const ManualPage& m_page;

        int get_lines_total() const override
        {
                return m_page.lines.size();
        }
};

#endif  // MANUAL_HPP

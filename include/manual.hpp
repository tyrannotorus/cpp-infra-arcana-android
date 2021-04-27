// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef MANUAL_HPP
#define MANUAL_HPP

#include <string>
#include <vector>
#include <algorithm>

#include "browser.hpp"
#include "info_screen_state.hpp"
#include "state.hpp"

struct ManualPage
{
        std::string title = "";

        std::vector<std::string> lines = {};
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

                m_page(page),
                m_top_idx(0)
        {}

        void draw() override;

        void update() override;

        StateId id() const override;

private:
        std::string title() const override;

        InfoScreenType type() const override
        {
                return InfoScreenType::scrolling;
        }

        const ManualPage& m_page;

        int m_top_idx;
};

#endif  // MANUAL_HPP

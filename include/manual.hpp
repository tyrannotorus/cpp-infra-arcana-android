// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef MANUAL_HPP
#define MANUAL_HPP

#include <string>
#include <vector>

#include "menu_descr_page.hpp"
#include "menu_page.hpp"
#include "state.hpp"

struct ManualPage
{
        std::string title {};
        std::vector<std::string> lines {};
};

// The manual - a standard two column description page: the chapter list on
// the left, the text of the marked chapter beside it (scrolled with its
// scrollbar).
class BrowseManual : public MenuDescrPageState
{
public:
        BrowseManual() = default;

        void on_start() override;

        StateId id() const override;

protected:
        std::string page_title() const override;

        std::string page_hint() const override;

        std::vector<MenuPageEntry> page_entries() const override;

        void on_entry_selected(int idx) override;

        void draw_page_content() override;

private:
        std::vector<ManualPage> m_pages {};
};

#endif  // MANUAL_HPP

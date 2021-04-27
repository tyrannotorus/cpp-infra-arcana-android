// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "manual.hpp"

#include <ext/alloc_traits.h>
#include <stddef.h>
#include <fstream>
#include <vector>
#include <memory>
#include <utility>

#include "common_text.hpp"
#include "debug.hpp"
#include "draw_box.hpp"
#include "io.hpp"
#include "panel.hpp"
#include "rect.hpp"
#include "text_format.hpp"
#include "SDL_keycode.h"
#include "colors.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static std::vector<std::string> read_manual_file()
{
        std::vector<std::string> lines;

        std::ifstream file("manual.txt");

        if (!file.is_open())
        {
                TRACE_ERROR_RELEASE
                        << "Could not open manual file"
                        << std::endl;

                PANIC;
        }

        std::string current_line;

        while (getline(file, current_line))
        {
                lines.push_back(current_line);
        }

        file.close();

        return lines;
}

static std::vector<std::string> format_lines(
        std::vector<std::string>& raw_lines)
{
        std::vector<std::string> formatted_lines;

        for (auto& raw_line : raw_lines)
        {
                // Format the line if it does not start with a space
                const bool should_format_line =
                        !raw_line.empty() &&
                        raw_line[0] != ' ';

                if (should_format_line)
                {
                        const auto split_line = text_format::split(
                                raw_line,
                                panels::w(Panel::info_screen_content));

                        for (const auto& line : split_line)
                        {
                                formatted_lines.push_back(line);
                        }
                }
                else
                {
                        // Do not format line
                        formatted_lines.push_back(raw_line);
                }
        }

        return formatted_lines;
}

static std::vector<ManualPage> init_pages(
        const std::vector<std::string>& formatted_lines)
{
        std::vector<ManualPage> pages;

        ManualPage current_page;

        const std::string delim(80, '-');

        // Sort the parsed lines into different pages
        for (size_t line_idx = 0;
             line_idx < formatted_lines.size();
             ++line_idx)
        {
                if (formatted_lines[line_idx] == delim)
                {
                        if (!current_page.lines.empty())
                        {
                                pages.push_back(current_page);

                                current_page.lines.clear();
                        }

                        // Skip first delimiter
                        ++line_idx;

                        // The title is printed on this line
                        current_page.title = formatted_lines[line_idx];

                        // Skip second delimiter
                        line_idx += 2;
                }

                current_page.lines.push_back(formatted_lines[line_idx]);
        }

        return pages;
}

// -----------------------------------------------------------------------------
// Browse manual
// -----------------------------------------------------------------------------
StateId BrowseManual::id() const
{
        return StateId::manual;
}

void BrowseManual::on_start()
{
        m_raw_lines = read_manual_file();

        const auto formatted_lines = format_lines(m_raw_lines);

        m_pages = init_pages(formatted_lines);

        m_browser.reset(m_pages.size());
}

void BrowseManual::draw()
{
        draw_box(panels::area(Panel::screen));

        io::draw_text_center(
                " Browsing manual ",
                Panel::screen,
                {panels::center_x(Panel::screen), 0},
                colors::title(),
                io::DrawBg::yes,
                colors::black(),
                true);  // Allow pixel-level adjustment

        io::draw_text_center(
                " " + common_text::g_screen_exit_hint + " ",
                Panel::screen,
                {panels::center_x(Panel::screen), panels::y1(Panel::screen)},
                colors::title(),
                io::DrawBg::yes,
                colors::black(),
                true);  // Allow pixel-level adjustment

        const int nr_pages = m_pages.size();

        const int labels_y0 = 1;

        for (int idx = 0; idx < (int)nr_pages; ++idx)
        {
                const bool is_marked = m_browser.y() == idx;

                const int y = labels_y0 + idx;

                auto str =
                        std::string("(") +
                        m_browser.menu_keys()[idx] +
                        std::string(")");

                auto color =
                        is_marked
                        ? colors::menu_key_highlight()
                        : colors::menu_key_dark();

                io::draw_text(
                        str,
                        Panel::screen,
                        {1, y},
                        color);

                const auto& page = m_pages[idx];

                color =
                        is_marked
                        ? colors::menu_highlight()
                        : colors::menu_dark();

                io::draw_text(
                        page.title,
                        Panel::screen,
                        {5, y},
                        color);
        }
}

void BrowseManual::on_window_resized()
{
        // const auto formatted_lines = format_lines(raw_lines_);

        // pages_ = init_pages(formatted_lines);
}

void BrowseManual::update()
{
        const auto input = io::get();

        const MenuAction action =
                m_browser.read(
                        input,
                        MenuInputMode::scrolling_and_letters);

        switch (action)
        {
        case MenuAction::selected:
        {
                const auto& page = m_pages[m_browser.y()];

                std::unique_ptr<State> browse_page(
                        new BrowseManualPage(page));

                states::push(std::move(browse_page));
        }
        break;

        case MenuAction::esc:
        case MenuAction::space:
        {
                states::pop();
        }
        break;

        default:
                break;
        }
}

// -----------------------------------------------------------------------------
// Manual page
// -----------------------------------------------------------------------------
StateId BrowseManualPage::id() const
{
        return StateId::manual_page;
}

void BrowseManualPage::draw()
{
        draw_interface();

        const int nr_lines_tot = m_page.lines.size();

        const int btm_nr =
                std::min(
                        m_top_idx + panels::h(Panel::info_screen_content) - 1,
                        nr_lines_tot - 1);

        int screen_y = 1;

        for (int i = m_top_idx; i <= btm_nr; ++i)
        {
                io::draw_text(
                        m_page.lines[i],
                        Panel::screen,
                        {panels::x0(Panel::info_screen_content), screen_y},
                        colors::text());

                ++screen_y;
        }
}

void BrowseManualPage::update()
{
        const int line_jump = 3;

        const int nr_lines_tot = m_page.lines.size();

        const auto input = io::get();

        switch (input.key)
        {
        case SDLK_KP_2:
        case SDLK_DOWN:
        {
                m_top_idx += line_jump;

                const int panel_h = panels::h(Panel::info_screen_content);

                if (nr_lines_tot <= panel_h)
                {
                        m_top_idx = 0;
                }
                else
                {
                        m_top_idx = std::min(
                                nr_lines_tot - panel_h,
                                m_top_idx);
                }
        }
        break;

        case SDLK_KP_8:
        case SDLK_UP:
        {
                m_top_idx = std::max(0, m_top_idx - line_jump);
        }
        break;

        case SDLK_SPACE:
        case SDLK_ESCAPE:
        {
                // Exit screen
                states::pop();
        }
        break;

        default:
        {
        }
        break;
        }
}

std::string BrowseManualPage::title() const
{
        return m_page.title;
}

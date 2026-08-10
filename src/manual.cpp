// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "manual.hpp"

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <vector>

#include "colors.hpp"
#include "debug.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static std::vector<std::string> read_manual_file()
{
        std::vector<std::string> lines;

        std::ifstream file("manual.txt");

        if (!file.is_open()) {
                TRACE_ERROR_RELEASE
                        << "Could not open manual file"
                        << std::endl;

                PANIC;
        }

        std::string current_line;

        while (getline(file, current_line)) {
                lines.push_back(current_line);
        }

        file.close();

        return lines;
}

// Sorts the raw file lines into chapters. A chapter header is a delimiter
// rule, the chapter title, and another delimiter rule.
// NOTE: The lines are kept RAW (unwrapped) - they are wrapped to the
// description column when drawn, so that the layout follows the panel.
static std::vector<ManualPage> init_pages(
        const std::vector<std::string>& raw_lines)
{
        std::vector<ManualPage> pages;

        ManualPage current_page;

        const std::string delim(80, '-');

        for (size_t line_idx = 0; line_idx < raw_lines.size(); ++line_idx) {
                if (raw_lines[line_idx] == delim) {
                        if (!current_page.lines.empty()) {
                                pages.push_back(current_page);

                                current_page.lines.clear();
                        }

                        // Skip first delimiter
                        ++line_idx;

                        // The title is printed on this line
                        current_page.title = raw_lines[line_idx];

                        // Skip second delimiter
                        line_idx += 2;

                        if (line_idx >= raw_lines.size()) {
                                break;
                        }
                }

                current_page.lines.push_back(raw_lines[line_idx]);
        }

        // NOTE: The last chapter is not followed by a delimiter - without
        // this it would be dropped
        if (!current_page.lines.empty()) {
                pages.push_back(current_page);
        }

        return pages;
}

// The column that continuation lines of a wrapped manual line are indented
// to. Body text just starts at the left edge, but a line that starts with
// spaces is a row of one of the manual's command tables
// ("   x         DESCRIPTION") - such a row stays readable only if its
// continuation lines line up with the description column.
static int hanging_indent(const std::string& line, const int w)
{
        const size_t text_start = line.find_first_not_of(' ');

        if ((text_start == std::string::npos) || (text_start == 0)) {
                return 0;
        }

        // The gap between the table's key column and its text column
        const size_t gap = line.find("  ", text_start);

        const size_t indent =
                (gap == std::string::npos)
                ? text_start
                : line.find_first_not_of(' ', gap);

        if (indent == std::string::npos) {
                return (int)text_start;
        }

        // Never indent so far that hardly any text fits on a line
        return std::min((int)indent, w / 2);
}

// Appends one raw manual line, wrapped to the given width (see
// hanging_indent). The manual is plain text - no color markup.
static void append_manual_line(
        std::vector<std::vector<ColoredString>>& lines,
        const std::string& raw_line,
        const int w)
{
        if (raw_line.empty() || (w <= 0)) {
                lines.emplace_back();

                return;
        }

        const std::string indent(hanging_indent(raw_line, w), ' ');

        std::string rest = raw_line;

        while ((int)rest.size() > w) {
                // Break at the last space that fits, or hard break a word
                // too long for the column
                size_t brk = rest.rfind(' ', w);

                if ((brk == std::string::npos) ||
                    ((int)brk <= (int)indent.size())) {
                        brk = (size_t)w;
                }

                lines.push_back({{rest.substr(0, brk), colors::text()}});

                const size_t next = rest.find_first_not_of(' ', brk);

                if (next == std::string::npos) {
                        return;
                }

                rest = indent + rest.substr(next);
        }

        lines.push_back({{rest, colors::text()}});
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
        m_pages = init_pages(read_manual_file());

        // NOTE: After the pages are set up, so that the entry list exists
        MenuPageState::on_start();
}

std::string BrowseManual::page_title() const
{
        return "Tome of Wisdom";
}

std::string BrowseManual::page_hint() const
{
        // Chapters are read beside the list - there is nothing to select,
        // and the screen closes via the [ x ] control
        return "";
}

std::vector<MenuPageEntry> BrowseManual::page_entries() const
{
        std::vector<MenuPageEntry> entries;

        entries.reserve(m_pages.size());

        for (const auto& page : m_pages) {
                entries.push_back({page.title, ""});
        }

        return entries;
}

void BrowseManual::on_entry_selected(const int idx)
{
        // The marked chapter is already shown beside the list - marking a
        // chapter IS reading it
        (void)idx;
}

void BrowseManual::draw_page_content()
{
        if (m_pages.empty()) {
                return;
        }

        const auto& page = m_pages[m_browser.y()];

        std::vector<std::vector<ColoredString>> lines;

        for (const auto& raw_line : page.lines) {
                append_manual_line(lines, raw_line, descr_text_w());
        }

        draw_descr_lines(lines);
}

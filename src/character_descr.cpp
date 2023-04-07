// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "character_descr.hpp"

#include <algorithm>
#include <iterator>

#include "SDL_keycode.h"
#include "game.hpp"
#include "game_summary_data.hpp"
#include "insanity.hpp"
#include "io.hpp"
#include "panel.hpp"
#include "property_handler.hpp"
#include "text_format.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static Color color_heading()
{
        return colors::menu_highlight();
}

static Color color_text_dark()
{
        return colors::gray();
}

static int max_descr_w()
{
        return panels::w(Panel::info_screen_content);
}

static void add_properties_descr(
        const game_summary_data::GameSummaryData& data,
        std::vector<ColoredString>& lines)
{
        lines.emplace_back("Current status effects", color_heading());

        if (data.properties.empty()) {
                lines.emplace_back("None", colors::text());
                lines.emplace_back("", colors::text());
        }
        else {
                for (const prop::PropListEntry& prop : data.properties) {
                        const ColoredString& title = prop.title;

                        lines.emplace_back(title.str, title.color);

                        const std::vector<std::string> descr_formatted =
                                text_format::split(prop.descr, max_descr_w());

                        for (const std::string& descr_line : descr_formatted) {
                                lines.emplace_back(descr_line, color_text_dark());
                        }

                        lines.emplace_back("", colors::text());
                }
        }
}

static void add_insanity_descr(
        const game_summary_data::GameSummaryData& data,
        std::vector<ColoredString>& lines)
{
        lines.emplace_back("Mental disorders", color_heading());

        if (data.insanity_symptons.empty()) {
                lines.emplace_back("None", colors::text());
        }
        else {
                for (const InsSympt* const sympt : data.insanity_symptons) {
                        const std::string sympt_descr = sympt->char_descr_msg();

                        if (!sympt_descr.empty()) {
                                lines.emplace_back(sympt_descr, colors::text());
                        }
                }
        }

        lines.emplace_back("", colors::text());
}

static void add_potion_descr(
        const game_summary_data::GameSummaryData& data,
        std::vector<ColoredString>& lines)
{
        lines.emplace_back("Potion knowledge", color_heading());

        std::vector<ColoredString> potion_knowledge = data.potion_knowledge;

        if (data.potion_knowledge.empty()) {
                lines.emplace_back("No known potions", colors::text());
        }
        else {
                std::sort(
                        std::begin(potion_knowledge),
                        std::end(potion_knowledge),
                        [](const ColoredString& e1, const ColoredString& e2) {
                                return e1.str < e2.str;
                        });

                for (const ColoredString& e : potion_knowledge) {
                        lines.push_back(e);
                }
        }

        lines.emplace_back("", colors::text());
}

static void add_scroll_descr(
        const game_summary_data::GameSummaryData& data,
        std::vector<ColoredString>& lines)
{
        lines.emplace_back("Manuscript knowledge", color_heading());

        std::vector<ColoredString> scroll_knowledge = data.scroll_knowledge;

        if (data.scroll_knowledge.empty()) {
                lines.emplace_back("No known manuscripts", colors::text());
        }
        else {
                std::sort(
                        std::begin(scroll_knowledge),
                        std::end(scroll_knowledge),
                        [](const ColoredString& e1,
                           const ColoredString& e2) {
                                return e1.str < e2.str;
                        });

                for (const ColoredString& e : scroll_knowledge) {
                        lines.push_back(e);
                }
        }

        lines.emplace_back("", colors::text());
}

static void add_traits_descr(
        const game_summary_data::GameSummaryData& data,
        std::vector<ColoredString>& lines)
{
        lines.emplace_back("Traits gained", color_heading());

        for (const game_summary_data::TraitData& trait : data.current_traits) {
                lines.emplace_back(trait.name, colors::text());

                const std::vector<std::string> descr_lines =
                        text_format::split(trait.descr, max_descr_w());

                for (const std::string& descr_line : descr_lines) {
                        lines.emplace_back(descr_line, color_text_dark());
                }

                lines.emplace_back("", colors::text());
        }
}

static void add_history_descr(
        const game_summary_data::GameSummaryData& data,
        std::vector<ColoredString>& lines)
{
        lines.emplace_back("History of " + data.player_name, color_heading());

        int longest_turn_w = 0;

        for (const HistoryEvent& event : data.player_history) {
                const int turn_w = (int)std::to_string(event.turn).size();

                longest_turn_w = std::max(turn_w, longest_turn_w);
        }

        for (const HistoryEvent& event : data.player_history) {
                std::string ev_str = std::to_string(event.turn);

                const int turn_w = (int)ev_str.size();

                ev_str.append(longest_turn_w - turn_w, ' ');

                ev_str += " " + event.msg;

                lines.emplace_back(ev_str, colors::text());
        }

        lines.emplace_back("", colors::text());
}

// -----------------------------------------------------------------------------
// Character description
// -----------------------------------------------------------------------------
void CharacterDescr::setup(const game_summary_data::GameSummaryData& data)
{
        m_lines.clear();

        add_properties_descr(data, m_lines);
        add_insanity_descr(data, m_lines);
        add_potion_descr(data, m_lines);
        add_scroll_descr(data, m_lines);
        add_traits_descr(data, m_lines);
        add_history_descr(data, m_lines);
}

void CharacterDescr::dump_to_clipboard() const
{
        // TODO: Implement.

        // TODO: Perhaps this should only dump an abbreviated version of the
        // lines (to fit in a Discord message (2000 character limit).
}

StateId CharacterDescr::id() const
{
        return StateId::player_character_descr;
}

void CharacterDescr::draw()
{
        draw_interface();

        int y = 0;

        const int nr_lines_tot = (int)m_lines.size();

        int btm_nr = std::min(
                m_top_idx + panels::h(Panel::info_screen_content) - 1,
                nr_lines_tot - 1);

        for (int i = m_top_idx; i <= btm_nr; ++i) {
                const ColoredString& line = m_lines[i];

                io::draw_text(
                        line.str,
                        Panel::info_screen_content,
                        {0, y},
                        line.color);

                ++y;
        }
}

void CharacterDescr::update()
{
        const int line_jump = 3;
        const int nr_lines_tot = m_lines.size();

        const auto input = io::read_input();

        switch (input.key) {
        case SDLK_KP_2:
        case SDLK_DOWN: {
                m_top_idx += line_jump;

                const int panel_h = panels::h(Panel::info_screen_content);

                if (nr_lines_tot <= panel_h) {
                        m_top_idx = 0;
                }
                else {
                        m_top_idx = std::min(
                                nr_lines_tot - panel_h,
                                m_top_idx);
                }
        } break;

        case SDLK_KP_8:
        case SDLK_UP: {
                m_top_idx = std::max(0, m_top_idx - line_jump);
        } break;

        case SDLK_SPACE:
        case SDLK_ESCAPE: {
                // Exit screen
                states::pop();
        } break;

        default:
                break;
        }
}

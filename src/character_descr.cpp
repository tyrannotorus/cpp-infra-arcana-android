// =============================================================================
// Copyright 2011-2023 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "character_descr.hpp"

#include <algorithm>
#include <iterator>

#include "SDL_clipboard.h"
#include "SDL_keycode.h"
#include "game.hpp"
#include "game_summary_data.hpp"
#include "insanity.hpp"
#include "io.hpp"
#include "panel.hpp"
#include "property_handler.hpp"
#include "spells.hpp"
#include "text_format.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static const Color& s_color_heading = colors::menu_highlight();
static const Color& s_color_text_dark = colors::gray();
static const std::string s_indent = "  ";

static int max_descr_w()
{
        return panels::w(Panel::info_screen_content) - (int)s_indent.size();
}

static void add_properties_descr(
        const game_summary_data::GameSummaryData& data,
        std::vector<ColoredString>& lines)
{
        lines.emplace_back("Current status effects", s_color_heading);

        if (data.properties.empty()) {
                lines.emplace_back(s_indent + "None", colors::text());
                lines.emplace_back("", colors::text());
        }
        else {
                for (const prop::PropListEntry& prop : data.properties) {
                        const ColoredString& title = prop.title;

                        lines.emplace_back(s_indent + title.str, title.color);

                        const std::vector<std::string> descr_formatted =
                                text_format::split(prop.descr, max_descr_w());

                        for (const std::string& descr_line : descr_formatted) {
                                lines.emplace_back(s_indent + descr_line, s_color_text_dark);
                        }

                        lines.emplace_back("", colors::text());
                }
        }
}

static int shock_from_src(
        const game_summary_data::GameSummaryData& data,
        const ShockSrc src)
{
        return data.total_shock_from_src[(size_t)src];
}

static std::string to_pct_str_padded(const int value, const int padding)
{
        return text_format::pad_after(std::to_string(value) + "%", padding);
}

static void add_insanity_descr(
        const game_summary_data::GameSummaryData& data,
        std::vector<ColoredString>& lines)
{
        lines.emplace_back("Sanity of mind", s_color_heading);

        lines.emplace_back(
                s_indent + std::to_string(data.insanity) + "% insane",
                colors::text());

        if (data.insanity_symptons.empty()) {
                lines.emplace_back(s_indent + "No specific mental disorders", colors::text());
        }
        else {
                for (const InsSympt* const sympt : data.insanity_symptons) {
                        const std::string sympt_descr = sympt->char_descr_msg();

                        if (!sympt_descr.empty()) {
                                lines.emplace_back(s_indent + sympt_descr, colors::text());
                        }
                }
        }

        lines.emplace_back(
                (
                        s_indent +
                        "Current shock level (including temporary sources) is " +
                        std::to_string(data.current_shock) +
                        "% "),
                colors::text());

        lines.emplace_back(
                (s_indent +
                 "Total shock received is " +
                 std::to_string(data.total_shock) +
                 "%, derived from (rounded values):"),
                colors::text());

        const int padding = 10;

        lines.emplace_back(
                (s_indent +
                 s_indent +
                 to_pct_str_padded(shock_from_src(data, ShockSrc::time), padding) +
                 "from the passing of time"),
                colors::text());

        lines.emplace_back(
                (s_indent +
                 s_indent +
                 to_pct_str_padded(shock_from_src(data, ShockSrc::see_mon), padding) +
                 "from observing creatures"),
                colors::text());

        lines.emplace_back(
                (s_indent +
                 s_indent +
                 to_pct_str_padded(shock_from_src(data, ShockSrc::take_damage), padding) +
                 "from being harmed"),
                colors::text());

        lines.emplace_back(
                (s_indent +
                 s_indent +
                 to_pct_str_padded(shock_from_src(data, ShockSrc::use_strange_item), padding) +
                 "from using items"),
                colors::text());

        lines.emplace_back(
                (s_indent +
                 s_indent +
                 to_pct_str_padded(data.total_shock_from_casting_spells, padding) +
                 "from casting learned spells"),
                colors::text());

        lines.emplace_back(
                (s_indent +
                 s_indent +
                 to_pct_str_padded(shock_from_src(data, ShockSrc::misc), padding) +
                 "from other sources"),
                colors::text());

        lines.emplace_back("", colors::text());
}

static void add_item_knowledge_descr(
        const game_summary_data::GameSummaryData& data,
        std::vector<ColoredString>& lines)
{
        lines.emplace_back("Item knowledge", s_color_heading);

        std::vector<std::vector<game_summary_data::ItemKnowledgeData>> item_knowledge =
                data.item_knowledge;

        for (const std::vector<game_summary_data::ItemKnowledgeData>& items : item_knowledge) {
                for (const game_summary_data::ItemKnowledgeData& e : items) {
                        ColoredString line;

                        line.str = e.name;
                        line.color = e.item_color;

                        if (!e.is_identified) {
                                line.str = s_indent + text_format::pad_after("(?)", 10) + line.str;
                                line.color = s_color_text_dark;
                        }

                        line.str = s_indent + line.str;

                        lines.push_back(line);
                }

                lines.emplace_back("", colors::text());
        }
}

static void add_traits_descr(
        const game_summary_data::GameSummaryData& data,
        std::vector<ColoredString>& lines)
{
        lines.emplace_back("Traits gained", s_color_heading);

        for (const game_summary_data::TraitData& trait : data.current_traits) {
                lines.emplace_back(s_indent + trait.name, colors::text());

                const std::vector<std::string> descr_lines =
                        text_format::split(trait.descr, max_descr_w());

                for (const std::string& descr_line : descr_lines) {
                        lines.emplace_back(s_indent + descr_line, s_color_text_dark);
                }

                lines.emplace_back("", colors::text());
        }
}

static void add_history_descr(
        const game_summary_data::GameSummaryData& data,
        std::vector<ColoredString>& lines)
{
        lines.emplace_back("History of " + data.player_name, s_color_heading);

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

                lines.emplace_back(s_indent + ev_str, colors::text());
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
        add_item_knowledge_descr(data, m_lines);
        add_traits_descr(data, m_lines);
        add_history_descr(data, m_lines);
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

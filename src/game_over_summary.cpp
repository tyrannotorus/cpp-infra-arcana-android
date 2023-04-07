// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "game_over_summary.hpp"

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <iterator>
#include <utility>

#include "SDL_keycode.h"
#include "common_text.hpp"
#include "draw_box.hpp"
#include "game.hpp"
#include "game_summary_data.hpp"
#include "highscore.hpp"
#include "insanity.hpp"
#include "io.hpp"
#include "msg_log.hpp"
#include "panel.hpp"
#include "player_bon.hpp"
#include "rect.hpp"
#include "text_format.hpp"

// -----------------------------------------------------------------------------
// GameOverSummary
// -----------------------------------------------------------------------------
void GameOverSummary::setup(const game_summary_data::GameSummaryData& data)
{
        const Color& color_heading = colors::menu_highlight();
        const Color& color_info = colors::white();

        const std::string name = data.highscore.name;

        m_lines.emplace_back(name + " (" + data.background_title + ")", color_heading);

        if (data.dlvl == 0) {
                m_lines.emplace_back(
                        "Died before entering the dungeon",
                        color_info);
        }
        else {
                m_lines.emplace_back(
                        "Explored to dungeon level " + std::to_string(data.dlvl),
                        color_info);
        }

        m_lines.emplace_back(
                "Spent " + std::to_string(data.turns) + " turns",
                color_info);

        m_lines.emplace_back(
                "Was " + std::to_string(data.insanity) + "% insane",
                color_info);

        m_lines.emplace_back(
                "Killed " + std::to_string(data.nr_kills_tot) + " monsters",
                color_info);

        m_lines.emplace_back(
                "Gained " + std::to_string(data.xp) + " experience points",
                color_info);

        const int score = data.highscore.calculate_score();

        m_lines.emplace_back(
                "Gained a score of " + std::to_string(score),
                color_info);

        if (!data.insanity_symptons.empty()) {
                for (const InsSympt* const sympt : data.insanity_symptons) {
                        const std::string sympt_descr = sympt->game_over_summary_msg();

                        if (!sympt_descr.empty()) {
                                m_lines.emplace_back(sympt_descr, color_info);
                        }
                }
        }

        m_lines.emplace_back("", color_info);

        m_lines.emplace_back(
                "Traits gained (at character level)",
                color_heading);

        if (data.trait_log.empty()) {
                m_lines.emplace_back("None", color_info);
        }
        else {
                bool has_double_digit =
                        std::find_if(
                                std::begin(data.trait_log),
                                std::end(data.trait_log),
                                [](const auto& e) {
                                        return e.clvl >= 10;
                                }) != std::end(data.trait_log);

                for (const player_bon::TraitLogEntry& e : data.trait_log) {
                        std::string clvl_str = std::to_string(e.clvl);

                        if (has_double_digit) {
                                clvl_str = text_format::pad_before(std::to_string(e.clvl), 2);
                        }

                        const std::string title = player_bon::trait_title(e.trait_id);
                        const std::string removed_str = e.is_removal ? " - REMOVED" : "";
                        const std::string str = clvl_str + " " + title + removed_str;

                        m_lines.emplace_back(str, color_info);
                }
        }

        m_lines.emplace_back("", color_info);

        m_lines.emplace_back("Unique monsters killed", color_heading);

        if (data.unique_monsters_killed.empty()) {
                m_lines.emplace_back("None", color_info);
        }
        else {
                for (const std::string& monster_name : data.unique_monsters_killed) {
                        m_lines.emplace_back(monster_name, color_info);
                }
        }

        m_lines.emplace_back("", color_info);

        m_lines.emplace_back("History of " + data.player_name, color_heading);

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

                m_lines.emplace_back(ev_str, color_info);
        }

        m_lines.emplace_back("", color_info);

        m_lines.emplace_back("Last messages", color_heading);

        const int max_nr_messages_to_show = 20;

        int history_start_idx =
                std::max(0, (int)data.msg_history.size() - max_nr_messages_to_show);

        for (size_t history_idx = history_start_idx;
             history_idx < data.msg_history.size();
             ++history_idx) {
                const Msg& msg = data.msg_history[history_idx];

                m_lines.emplace_back(msg.text_with_repeats(), color_info);
        }

        m_lines.emplace_back("", color_info);
}

void GameOverSummary::setup(std::vector<ColoredString> lines)
{
        m_lines = std::move(lines);
}

void GameOverSummary::dump_to_file(const std::string& path) const
{
        std::ofstream file;

        file.open(path.c_str(), std::ios::trunc);

        for (const ColoredString& line : m_lines) {
                file << line.str << std::endl;
        }

        file.close();
}

void GameOverSummary::dump_to_clipboard() const
{
        // TODO: Implement.

        // TODO: Perhaps this should only dump an abbreviated version of the
        // lines (to fit in a Discord message (2000 character limit)).
}

StateId GameOverSummary::id() const
{
        return StateId::game_over_summary;
}

void GameOverSummary::draw()
{
        io::clear_screen();

        draw_box(panels::area(Panel::screen));

        const int screen_center_x = panels::center_x(Panel::screen);

        io::draw_text_center(
                " " + title() + " ",
                Panel::screen,
                {screen_center_x, 0},
                colors::title());

        const std::string command_info =
                common_text::g_scroll_hint +
                " " +
                common_text::g_game_over_summary_exit_hint;

        io::draw_text_center(
                " " + command_info + " ",
                Panel::screen,
                {screen_center_x, panels::y1(Panel::screen)},
                colors::title());

        const int nr_lines = (int)m_lines.size();

        int y = 0;

        const int panel_h = panels::h(Panel::info_screen_content);

        for (int i = m_top_idx;
             (i < nr_lines) && ((i - m_top_idx) < panel_h);
             ++i) {
                const ColoredString& line = m_lines[i];

                io::draw_text(line.str, Panel::info_screen_content, {0, y}, line.color);

                ++y;
        }
}

void GameOverSummary::update()
{
        const int line_jump = 3;

        const int nr_lines = (int)m_lines.size();

        const io::InputData input = io::read_input();

        switch (input.key) {
        case SDLK_DOWN:
        case SDLK_KP_2: {
                m_top_idx += line_jump;

                const int panel_h = panels::h(Panel::info_screen_content);

                if (nr_lines <= panel_h) {
                        m_top_idx = 0;
                }
                else {
                        m_top_idx = std::min(nr_lines - panel_h, m_top_idx);
                }
        } break;

        case SDLK_UP:
        case SDLK_KP_8: {
                m_top_idx = std::max(0, m_top_idx - line_jump);
        } break;

        case SDLK_SPACE:
        case SDLK_ESCAPE: {
                // Exit screen
                states::pop();
        } break;
        }
}

// =============================================================================
// Copyright 2011-2021 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "game_over.hpp"

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "actor_data.hpp"
#include "actor_player.hpp"
#include "colors.hpp"
#include "game.hpp"
#include "highscore.hpp"
#include "init.hpp"
#include "insanity.hpp"
#include "map.hpp"
#include "msg_log.hpp"
#include "paths.hpp"
#include "player_bon.hpp"
#include "postmortem.hpp"
#include "state.hpp"
#include "text_format.hpp"
#include "time.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static void make_memorial_file(
        const std::vector<ColoredString>& lines,
        const std::string& path)
{
        // Write memorial file
        std::ofstream file;

        file.open(path.c_str(), std::ios::trunc);

        // Add info lines to file
        for (const auto& line : lines)
        {
                file << line.str << std::endl;
        }

        file.close();
}

static std::vector<ColoredString> make_game_summary_lines(
        const HighscoreEntry& highscore_entry)
{
        std::vector<ColoredString> lines;

        const auto color_heading = colors::menu_highlight();
        const auto color_info = colors::white();

        std::vector<std::string> unique_killed_names;

        int nr_kills_tot_all_mon = 0;

        for (const auto& d : actor::g_data)
        {
                if ((d.id != actor::Id::player) && (d.nr_kills > 0))
                {
                        nr_kills_tot_all_mon += d.nr_kills;

                        if (d.is_unique)
                        {
                                unique_killed_names.push_back(d.name_a);
                        }
                }
        }

        const std::string name = highscore_entry.name;

        std::string bg_title;

        if (highscore_entry.bg == Bg::occultist)
        {
                const auto domain = highscore_entry.player_occultist_domain;

                bg_title = player_bon::occultist_profession_title(domain);
        }
        else
        {
                bg_title = player_bon::bg_title(highscore_entry.bg);
        }

        lines.emplace_back(
                name + " (" + bg_title + ")",
                color_heading);

        const int dlvl = highscore_entry.dlvl;

        if (dlvl == 0)
        {
                lines.emplace_back(
                        "Died before entering the dungeon",
                        color_info);
        }
        else
        {
                // DLVL is at least 1
                lines.emplace_back(
                        "Explored to dungeon level " + std::to_string(dlvl),
                        color_info);
        }

        const int turn_count = highscore_entry.turn_count;

        lines.emplace_back(
                "Spent " + std::to_string(turn_count) + " turns",
                color_info);

        const int ins = highscore_entry.ins;

        lines.emplace_back(
                "Was " + std::to_string(ins) + "% insane",
                color_info);

        lines.emplace_back(
                "Killed " + std::to_string(nr_kills_tot_all_mon) + " monsters",
                color_info);

        const int xp = highscore_entry.xp;

        lines.emplace_back(
                "Gained " + std::to_string(xp) + " experience points",
                color_info);

        const int score = highscore_entry.calculate_score();

        lines.emplace_back(
                "Gained a score of " + std::to_string(score),
                color_info);

        const std::vector<const InsSympt*> sympts =
                insanity::active_sympts();

        if (!sympts.empty())
        {
                for (const InsSympt* const sympt : sympts)
                {
                        const std::string sympt_descr = sympt->postmortem_msg();

                        if (!sympt_descr.empty())
                        {
                                lines.emplace_back(sympt_descr, color_info);
                        }
                }
        }

        lines.emplace_back("", color_info);

        lines.emplace_back(
                "Traits gained (at character level)",
                color_heading);

        const auto trait_log = player_bon::trait_log();

        if (trait_log.empty())
        {
                lines.emplace_back("None", color_info);
        }
        else
        {
                bool is_double_digit =
                        std::find_if(
                                std::begin(trait_log),
                                std::end(trait_log),
                                [](const auto& e) {
                                        return e.clvl >= 10;
                                }) != std::end(trait_log);

                for (const auto& e : trait_log)
                {
                        std::string clvl_str = std::to_string(e.clvl);

                        if (is_double_digit)
                        {
                                clvl_str =
                                        text_format::pad_before(
                                                std::to_string(e.clvl),
                                                2);
                        }

                        const std::string title =
                                player_bon::trait_title(e.trait_id);

                        const std::string removed_str =
                                e.is_removal
                                ? " - REMOVED"
                                : "";

                        const std::string str =
                                clvl_str +
                                " " +
                                title +
                                removed_str;

                        lines.emplace_back(str, color_info);
                }
        }

        lines.emplace_back("", color_info);

        lines.emplace_back(
                "Unique monsters killed",
                color_heading);

        if (unique_killed_names.empty())
        {
                lines.emplace_back("None", color_info);
        }
        else
        {
                for (std::string& monster_name : unique_killed_names)
                {
                        lines.emplace_back(monster_name, color_info);
                }
        }

        lines.emplace_back("", color_info);

        lines.emplace_back(
                "History of " + map::g_player->name_the(),
                color_heading);

        const std::vector<HistoryEvent>& events =
                game::history();

        int longest_turn_w = 0;

        for (const auto& event : events)
        {
                const int turn_w = std::to_string(event.turn).size();

                longest_turn_w = std::max(turn_w, longest_turn_w);
        }

        for (const auto& event : events)
        {
                std::string ev_str = std::to_string(event.turn);

                const int turn_w = ev_str.size();

                ev_str.append(longest_turn_w - turn_w, ' ');

                ev_str += " " + event.msg;

                lines.emplace_back(ev_str, color_info);
        }

        lines.emplace_back("", color_info);

        lines.emplace_back(
                "Last messages",
                color_heading);

        const auto& msg_history = msg_log::history();

        const int max_nr_messages_to_show = 20;

        int history_start_idx = std::max(
                0,
                (int)msg_history.size() - max_nr_messages_to_show);

        for (size_t history_idx = history_start_idx;
             history_idx < msg_history.size();
             ++history_idx)
        {
                const auto& msg = msg_history[history_idx];

                lines.emplace_back(msg.text_with_repeats(), color_info);
        }

        lines.emplace_back("", color_info);

        return lines;
}

// -----------------------------------------------------------------------------
// Public
// -----------------------------------------------------------------------------
void on_game_over(const IsWin is_win)
{
        const auto game_summary_time_stamp =
                game::start_time().time_str(TimeType::second, false);

        const auto game_summary_filename =
                map::g_player->name_a() +
                "_" +
                game_summary_time_stamp +
                ".txt";

        const auto game_summary_file_path =
                paths::user_dir() +
                game_summary_filename;

        auto highscore_entry =
                highscore::make_entry_from_current_game_data(
                        game_summary_file_path,
                        is_win);

        highscore::append_entry_to_highscores_file(highscore_entry);

        const auto game_summary_lines =
                make_game_summary_lines(
                        highscore_entry);

        // Dump the lines to a memorial file
        make_memorial_file(
                game_summary_lines,
                game_summary_file_path);

        // From now on the session data is not needed anymore
        init::cleanup_session();

        // Show game summary first, then highscores
        states::push(
                std::make_unique<BrowseHighscore>());

        states::push(
                std::make_unique<PostmortemInfo>(
                        game_summary_lines));
}

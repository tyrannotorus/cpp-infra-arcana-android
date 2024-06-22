// =============================================================================
// Copyright 2011-2024 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "highscore.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <memory>
#include <utility>
#include <vector>

#include "SDL_keycode.h"
#include "actor.hpp"
#include "colors.hpp"
#include "common_text.hpp"
#include "debug.hpp"
#include "draw_box.hpp"
#include "game.hpp"
#include "game_time.hpp"
#include "io.hpp"
#include "map.hpp"
#include "misc.hpp"
#include "panel.hpp"
#include "paths.hpp"
#include "popup.hpp"
#include "random.hpp"
#include "rect.hpp"
#include "time.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static int max_nr_entries_on_screen()
{
        return panels::h(Panel::info_screen_content) - 1;
}

static void sort_entries(std::vector<HighscoreEntry>& entries)
{
        std::sort(
                std::begin(entries),
                std::end(entries),
                [](const HighscoreEntry& e1, const HighscoreEntry& e2) {
                        return e1.calculate_score() > e2.calculate_score();
                });
}

static void write_file(std::vector<HighscoreEntry>& entries)
{
        std::ofstream f;

        f.open(paths::highscores_file_path(), std::ios::trunc);

        for (const auto& entry : entries) {
                const std::string win_str =
                        (entry.is_win == IsWin::yes) ? "1" : "0";

                f << entry.game_summary_file_path << std::endl;
                f << win_str << std::endl;
                f << entry.date << std::endl;
                f << entry.name << std::endl;
                f << entry.xp << std::endl;
                f << entry.lvl << std::endl;
                f << entry.dlvl << std::endl;
                f << entry.turn_count << std::endl;
                f << entry.ins << std::endl;
                f << (int)entry.bg << std::endl;
                f << (int)entry.player_occultist_domain << std::endl;
                f << entry.is_latest_entry << std::endl;
        }
}

static std::vector<HighscoreEntry> read_highscores_file()
{
        TRACE_FUNC_BEGIN;

        std::vector<HighscoreEntry> entries;

        std::ifstream file;

        file.open(paths::highscores_file_path());

        if (!file.is_open()) {
                return entries;
        }

        std::string line;

        while (getline(file, line)) {
                HighscoreEntry e;

                e.game_summary_file_path = line;

                getline(file, line);

                e.is_win =
                        (line[0] == '1')
                        ? IsWin::yes
                        : IsWin::no;

                getline(file, line);
                e.date = line;

                getline(file, line);
                e.name = line;

                getline(file, line);
                e.xp = to_int(line);

                getline(file, line);
                e.lvl = to_int(line);

                getline(file, line);
                e.dlvl = to_int(line);

                getline(file, line);
                e.turn_count = to_int(line);

                getline(file, line);
                e.ins = to_int(line);

                getline(file, line);
                e.bg = (Bg)to_int(line);

                getline(file, line);
                e.player_occultist_domain = (OccultistDomain)to_int(line);

                getline(file, line);
                e.is_latest_entry = to_int(line);

                entries.push_back(e);
        }

        file.close();

        TRACE_FUNC_END;

        return entries;
}

// -----------------------------------------------------------------------------
// Highscore entry
// -----------------------------------------------------------------------------
int HighscoreEntry::calculate_score() const
{
        const auto xp_db = (double)xp;
        const auto dlvl_db = (double)dlvl;
        const auto dlvl_last_db = (double)g_dlvl_last;

        // HACK: Flagellants tend to spend a lot more turns (especially since
        // they are forced to wait for a turn every time they walk). Here this
        // is compensated by adjusting the turn count when calculating score.
        //
        // Preferably the score model should be changed instead to something
        // like counting how many actions the player performs (then no special
        // case is needed for Flagellants).
        int turns_adjusted = turn_count;

        if (bg == Bg::flagellant) {
                turns_adjusted = (turns_adjusted * 55) / 100;
        }

        const auto turns_db = (double)turns_adjusted;

        const auto ins_db = (double)ins;
        const bool win = (is_win == IsWin::yes);

        auto calc_turns_factor = [](const double nr_turns_db) {
                return std::max(1.0, 3.0 - (nr_turns_db / 10000.0));
        };

        auto calc_sanity_factor = [](const double nr_ins_db) {
                return 2.0 - (nr_ins_db / 100.0);
        };

        const double xp_factor = 1.0 + xp_db;
        const double dlvl_factor = 1.0 + (dlvl_db / dlvl_last_db);
        const double turns_factor = calc_turns_factor(turns_db);
        const double turns_factor_win = win ? calc_turns_factor(0.0) : 1.0;
        const double sanity_factor = calc_sanity_factor(ins_db);
        const double sanity_factor_win = win ? calc_sanity_factor(0.0) : 1.0;

        const double result_db =
                xp_factor *
                dlvl_factor *
                turns_factor *
                turns_factor_win *
                sanity_factor *
                sanity_factor_win;

        const int result = (int)result_db;

        return result;
}

// -----------------------------------------------------------------------------
// Highscore
// -----------------------------------------------------------------------------
namespace highscore
{
HighscoreEntry make_entry_from_current_session(
        const std::string& game_summary_file_path)
{
        HighscoreEntry e;

        e.game_summary_file_path = game_summary_file_path;
        e.date = current_time().time_str(TimeType::day, true);
        e.name = map::g_player->name_a();
        e.xp = game::xp_accumulated();
        e.lvl = game::clvl();
        e.dlvl = map::g_dlvl;
        e.turn_count = game_time::turn_nr();
        e.ins = map::g_player->insanity();
        e.is_win = game::is_win();
        e.bg = player_bon::bg();
        e.player_occultist_domain = player_bon::occultist_domain();

        return e;
}

void append_entry_to_highscores_file(HighscoreEntry& entry)
{
        TRACE_FUNC_BEGIN;

        std::vector<HighscoreEntry> entries = entries_sorted();

        std::for_each(
                std::begin(entries),
                std::end(entries),
                [](auto& e) { e.is_latest_entry = false; });

        entry.is_latest_entry = true;

        entries.push_back(entry);

        sort_entries(entries);

        write_file(entries);

        TRACE_FUNC_END;
}

std::vector<HighscoreEntry> entries_sorted()
{
        auto entries = read_highscores_file();

        if (!entries.empty()) {
                sort_entries(entries);
        }

        return entries;
}

}  // namespace highscore

// -----------------------------------------------------------------------------
// Browse highscore
// -----------------------------------------------------------------------------
StateId BrowseHighscore::id() const
{
        return StateId::highscore;
}

void BrowseHighscore::on_start()
{
        m_entries = read_highscores_file();

        sort_entries(m_entries);

        m_browser.reset(m_entries.size(), max_nr_entries_on_screen());
}

void BrowseHighscore::on_window_resized()
{
        m_browser.reset(m_entries.size(), max_nr_entries_on_screen());
}

void BrowseHighscore::draw()
{
        if (m_entries.empty()) {
                return;
        }

        draw_box(panels::area(Panel::screen));

        io::draw_text_center(
                " Browsing high scores ",
                Panel::screen,
                {panels::center_x(Panel::screen), 0},
                colors::title(),
                io::DrawBg::yes,
                colors::black(),
                true);  // Allow pixel-level adjustment

        io::draw_text_center(
                std::string(
                        " [select] to view game summary " +
                        common_text::g_screen_exit_hint +
                        " "),
                Panel::screen,
                {panels::center_x(Panel::screen), panels::y1(Panel::screen)},
                colors::title(),
                io::DrawBg::yes,
                colors::black(),
                true);  // Allow pixel-level adjustment

        const Color& label_clr = colors::white();

        const int x_date = 1;
        const int x_name = x_date + 12;
        const int x_bg = x_name + g_player_name_max_len + 1;
        const int x_lvl = x_bg + 13;
        const int x_dlvl = x_lvl + 7;
        const int x_turns = x_dlvl + 7;
        const int x_ins = x_turns + 7;
        const int x_win = x_ins + 6;
        const int x_score = x_win + 5;

        const std::vector<std::pair<std::string, int>> labels {
                {"Level", x_lvl},
                {"Depth", x_dlvl},
                {"Turns", x_turns},
                {"Ins", x_ins},
                {"Win", x_win},
                {"Score", x_score}};

        for (const auto& label : labels) {
                io::draw_text(
                        label.first,
                        Panel::info_screen_content,
                        {label.second, 0},
                        label_clr);
        }

        int y = 1;

        const Range idx_range_shown = m_browser.range_shown();

        for (int i = idx_range_shown.min; i <= idx_range_shown.max; ++i) {
                const auto& entry = m_entries[i];

                const auto& date = entry.date;
                const auto& name = entry.name;

                std::string bg_title;

                if (entry.bg == Bg::occultist) {
                        bg_title = player_bon::occultist_profession_title(
                                entry.player_occultist_domain);
                }
                else {
                        bg_title = player_bon::bg_title(entry.bg);
                }

                const auto lvl = std::to_string(entry.lvl);
                const auto dlvl = std::to_string(entry.dlvl);
                const auto turns = std::to_string(entry.turn_count);
                const auto ins = std::to_string(entry.ins);
                const auto* const win = (entry.is_win == IsWin::yes) ? "Yes" : "No";
                const auto score = std::to_string(entry.calculate_score());

                const bool is_marked = m_browser.is_at_idx(i);

                Color color;

                if (entry.is_latest_entry) {
                        color =
                                is_marked
                                ? colors::light_green()
                                : colors::green();
                }
                else {
                        color =
                                is_marked
                                ? colors::menu_highlight()
                                : colors::menu_dark();
                }

                const auto panel = Panel::info_screen_content;

                io::draw_text(date, panel, {x_date, y}, color);
                io::draw_text(name, panel, {x_name, y}, color);
                io::draw_text(bg_title, panel, {x_bg, y}, color);
                io::draw_text(lvl, panel, {x_lvl, y}, color);
                io::draw_text(dlvl, panel, {x_dlvl, y}, color);
                io::draw_text(turns, panel, {x_turns, y}, color);
                io::draw_text(ins + "%", panel, {x_ins, y}, color);
                io::draw_text(win, panel, {x_win, y}, color);
                io::draw_text(score, panel, {x_score, y}, color);

                ++y;
        }

        // Draw "more" labels
        if (!m_browser.is_on_top_page()) {
                io::draw_text(
                        "(More - Page Up)",
                        Panel::screen,
                        {0, 1},
                        colors::light_white());
        }

        if (!m_browser.is_on_btm_page()) {
                io::draw_text(
                        "(More - Page Down)",
                        Panel::screen,
                        {0, panels::y1(Panel::screen)},
                        colors::light_white());
        }
}

void BrowseHighscore::update()
{
        if (m_entries.empty()) {
                popup::Popup(popup::AddToMsgHistory::no)
                        .set_msg("No high score entries found.")
                        .run();

                // Exit screen
                states::pop();

                return;
        }

        const auto input = io::read_input();

        const MenuAction action =
                m_browser.read(
                        input,
                        MenuInputMode::scrolling);

        switch (action) {
        case MenuAction::selected: {
                const int browser_y = m_browser.y();

                ASSERT(browser_y < (int)m_entries.size());

                const auto& entry_marked = m_entries[browser_y];

                const std::string file_path = entry_marked.game_summary_file_path;

                states::push(std::make_unique<BrowseHighscoreEntry>(file_path));
        } break;

        case MenuAction::space:
        case MenuAction::esc: {
                // Exit screen
                states::pop();
        } break;

        default:
                break;
        }
}

// -----------------------------------------------------------------------------
// Browse highscore entry game summary file
// -----------------------------------------------------------------------------
BrowseHighscoreEntry::BrowseHighscoreEntry(std::string file_path) :

        m_file_path(std::move(file_path)),

        m_top_idx(0)
{}

StateId BrowseHighscoreEntry::id() const
{
        return StateId::browse_highscore_entry;
}

void BrowseHighscoreEntry::on_start()
{
        read_file();
}

void BrowseHighscoreEntry::on_window_resized()
{
        m_top_idx = 0;
}

void BrowseHighscoreEntry::draw()
{
        draw_interface();

        const int nr_lines_tot = m_lines.size();

        int btm_nr =
                std::min(
                        m_top_idx + panels::h(Panel::info_screen_content) - 1,
                        nr_lines_tot - 1);

        int y = 0;

        for (int i = m_top_idx; i <= btm_nr; ++i) {
                io::draw_text(
                        m_lines[i],
                        Panel::info_screen_content,
                        {0, y},
                        colors::text());

                ++y;
        }
}

void BrowseHighscoreEntry::update()
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
        {
        } break;
        }
}

void BrowseHighscoreEntry::read_file()
{
        m_lines.clear();

        std::ifstream file(m_file_path);

        if (!file.is_open()) {
                popup::Popup(popup::AddToMsgHistory::no)
                        .set_title("Game summary file could not be opened")
                        .set_msg("Path: \"" + m_file_path + "\"")
                        .run();

                states::pop();

                return;
        }

        std::string current_line;

        std::vector<std::string> formatted;

        while (getline(file, current_line)) {
                m_lines.push_back(current_line);
        }

        file.close();
}

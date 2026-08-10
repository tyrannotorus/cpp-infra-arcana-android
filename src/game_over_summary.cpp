// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
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
// Private
// -----------------------------------------------------------------------------
static const Color& s_color_heading = colors::menu_highlight();
static const Color& s_color_info = colors::text();
static const std::string s_indent = "  ";

static void add_player_summary_descr(
        const game_summary_data::GameSummaryData& data,
        std::vector<ColoredString>& lines)
{
        const std::string name = data.highscore.name;

        lines.emplace_back(name + " (" + data.background_title + ")", s_color_heading);

        if (data.dlvl == 0) {
                lines.emplace_back(
                        s_indent + "Died before entering the dungeon",
                        s_color_info);
        }
        else {
                lines.emplace_back(
                        s_indent + "Explored to dungeon level " + std::to_string(data.dlvl),
                        s_color_info);
        }

        lines.emplace_back(
                s_indent + "Spent " + std::to_string(data.turns) + " turns",
                s_color_info);

        lines.emplace_back(
                s_indent + "Was " + std::to_string(data.insanity) + "% insane",
                s_color_info);

        lines.emplace_back(
                s_indent + "Killed " + std::to_string(data.nr_kills_tot) + " monsters",
                s_color_info);

        lines.emplace_back(
                s_indent + "Gained " + std::to_string(data.xp) + " experience points",
                s_color_info);

        const int score = data.highscore.calculate_score();

        lines.emplace_back(
                s_indent + "Gained a score of " + std::to_string(score),
                s_color_info);

        if (!data.insanity_symptons.empty()) {
                for (const InsSympt* const sympt : data.insanity_symptons) {
                        const std::string sympt_descr = sympt->game_over_summary_msg();

                        if (!sympt_descr.empty()) {
                                lines.emplace_back(s_indent + sympt_descr, s_color_info);
                        }
                }
        }

        lines.emplace_back("", s_color_info);
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

static void add_total_shock_received_descr(
        const game_summary_data::GameSummaryData& data,
        std::vector<ColoredString>& lines)
{
        lines.emplace_back(
                ("Total shock received is " +
                 std::to_string(data.total_shock) +
                 "%, derived from (rounded values):"),
                s_color_heading);

        const int padding = 10;

        lines.emplace_back(
                (s_indent +
                 to_pct_str_padded(shock_from_src(data, ShockSrc::time), padding) +
                 "from the passing of time"),
                colors::text());

        lines.emplace_back(
                (s_indent +
                 to_pct_str_padded(shock_from_src(data, ShockSrc::see_mon), padding) +
                 "from observing creatures"),
                colors::text());

        lines.emplace_back(
                (s_indent +
                 to_pct_str_padded(shock_from_src(data, ShockSrc::take_damage), padding) +
                 "from being harmed"),
                colors::text());

        lines.emplace_back(
                (s_indent +
                 to_pct_str_padded(shock_from_src(data, ShockSrc::use_strange_item), padding) +
                 "from using items"),
                colors::text());

        lines.emplace_back(
                (s_indent +
                 to_pct_str_padded(data.total_shock_from_casting_spells, padding) +
                 "from casting learned spells"),
                colors::text());

        lines.emplace_back(
                (s_indent +
                 to_pct_str_padded(shock_from_src(data, ShockSrc::misc), padding) +
                 "from other sources"),
                colors::text());

        lines.emplace_back("", colors::text());
}

static void add_traits_descr(
        const game_summary_data::GameSummaryData& data,
        std::vector<ColoredString>& lines)
{
        lines.emplace_back(
                "Traits gained (at character level)",
                s_color_heading);

        if (data.trait_log.empty()) {
                lines.emplace_back(s_indent + "None", s_color_info);
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

                        lines.emplace_back(s_indent + str, s_color_info);
                }
        }

        lines.emplace_back("", s_color_info);
}

static void add_unique_monsters_killed_descr(
        const game_summary_data::GameSummaryData& data,
        std::vector<ColoredString>& lines)
{
        lines.emplace_back("Unique monsters killed", s_color_heading);

        if (data.unique_monsters_killed.empty()) {
                lines.emplace_back(s_indent + "None", s_color_info);
        }
        else {
                for (const std::string& monster_name : data.unique_monsters_killed) {
                        lines.emplace_back(s_indent + monster_name, s_color_info);
                }
        }

        lines.emplace_back("", s_color_info);
}

static Color get_inventory_item_color(const game_summary_data::InventoryItemData& item)
{
        return (
                (item.item_name.empty() || item.is_identified)
                        ? colors::text()
                        : colors::magenta());
}

static void add_inventory_descr(
        const game_summary_data::GameSummaryData& data,
        std::vector<ColoredString>& lines)
{
        lines.emplace_back("Inventory", s_color_heading);

        if (data.inventory.empty()) {
                lines.emplace_back(s_indent + "Empty", colors::text());
        }
        else {
                for (const game_summary_data::InventoryItemData& item : data.inventory) {
                        const Color color = get_inventory_item_color(item);

                        if (item.slot_name.empty()) {
                                // Is "backpack" item.
                                lines.emplace_back(s_indent + item.item_name, color);
                        }
                        else {
                                // Is slot item (wielded, armor etc),
                                std::string str = item.slot_name;

                                str = text_format::pad_after(str, 9, ' ');

                                str += item.item_name.empty() ? "<empty>" : item.item_name;

                                lines.emplace_back(s_indent + str, color);
                        }
                }
        }

        lines.emplace_back("", colors::text());
}

static void add_player_history(
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

                lines.emplace_back(s_indent + ev_str, s_color_info);
        }

        lines.emplace_back("", s_color_info);
}

static void add_last_messages(
        const game_summary_data::GameSummaryData& data,
        std::vector<ColoredString>& lines)
{
        lines.emplace_back("Last messages", s_color_heading);

        const int max_nr_messages_to_show = 20;

        int history_start_idx =
                std::max(
                        0,
                        (int)data.msg_history.size() - max_nr_messages_to_show);

        for (size_t history_idx = history_start_idx;
             history_idx < data.msg_history.size();
             ++history_idx) {
                const Msg& msg = data.msg_history[history_idx];

                lines.emplace_back(s_indent + msg.text_with_repeats(), s_color_info);
        }

        lines.emplace_back("", s_color_info);
}

// -----------------------------------------------------------------------------
// GameOverSummary
// -----------------------------------------------------------------------------
void GameOverSummary::setup(const game_summary_data::GameSummaryData& data)
{
        add_player_summary_descr(data, m_lines);
        add_total_shock_received_descr(data, m_lines);
        add_unique_monsters_killed_descr(data, m_lines);
        add_last_messages(data, m_lines);
        add_inventory_descr(data, m_lines);
        // add_item_knowledge_descr(data, m_lines);
        add_traits_descr(data, m_lines);
        add_player_history(data, m_lines);
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

StateId GameOverSummary::id() const
{
        return StateId::game_over_summary;
}

void GameOverSummary::draw()
{
        io::clear_screen();

        draw_box(panels::screen_box_area());

        const int screen_center_x = panels::center_x(Panel::screen);

        io::draw_text_center(
                " " + title() + " ",
                Panel::screen,
                {screen_center_x, panels::screen_box_area().p0.y},
                colors::title());

        io::draw_text_center(
                " " + common_text::g_confirm_hint + " ",
                Panel::screen,
                {screen_center_x, panels::screen_box_area().p1.y},
                colors::title());

        draw_scrollable_content();
}

ColoredString GameOverSummary::content_line(const int line_idx) const
{
        return m_lines[line_idx];
}

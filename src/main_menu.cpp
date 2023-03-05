// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "main_menu.hpp"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "audio.hpp"
#include "audio_data.hpp"
#include "colors.hpp"
#include "config.hpp"
#include "create_character.hpp"
#include "draw_box.hpp"
#include "game.hpp"
#include "global.hpp"
#include "highscore.hpp"
#include "init.hpp"
#include "io.hpp"
#include "manual.hpp"
#include "messages.hpp"
#include "panel.hpp"
#include "popup.hpp"
#include "pos.hpp"
#include "rect.hpp"
#include "saving.hpp"
#include "text_format.hpp"
#include "version.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static std::string s_git_sha1_str;

static std::string s_current_quote;

static bool query_overwrite_savefile()
{
        int choice = 0;

        popup::Popup(popup::AddToMsgHistory::no)
                .set_title("A saved game exists")
                .set_msg("Start a new game?")
                .setup_menu_mode(
                        {"(Y)es", "(N)o"},
                        {'y', 'n'},
                        &choice)
                .run();

        return (choice == 0);
}

// -----------------------------------------------------------------------------
// Main menu state
// -----------------------------------------------------------------------------
MainMenuState::MainMenuState() :
        m_browser(MenuBrowser(6))
{
        m_browser.set_custom_menu_keys({'n', 'r', 't', 'o', 'g', 'e'});
}

MainMenuState::~MainMenuState() = default;

StateId MainMenuState::id() const
{
        return StateId::main_menu;
}

void MainMenuState::draw()
{
        if (config::is_tiles_mode()) {
                draw_box(panels::area(Panel::screen));
        }

        if (config::is_tiles_mode()) {
                io::draw_logo();
        }
        else {
                // Text mode
                io::draw_text_center(
                        "I n f r a   A r c a n a",
                        Panel::screen,
                        {panels::center_x(Panel::screen),
                         (panels::h(Panel::screen) * 3) / 12},
                        colors::light_white());
        }

        if (config::is_gj_mode()) {
                io::draw_text(
                        "### GJ MODE ENABLED ###",
                        Panel::screen,
                        {1, 1},
                        colors::yellow());
        }
#ifndef NDEBUG
        else {
                io::draw_text(
                        "### DEBUG ###",
                        Panel::screen,
                        {1, 1},
                        colors::yellow());

                io::draw_text(
                        "###  MODE ###",
                        Panel::screen,
                        {1, 2},
                        colors::yellow());
        }
#endif  // NDEBUG

        const std::vector<std::string> labels = {
                "(N)ew journey",
                "(R)esurrect",
                "(T)ome of Wisdom",
                "(O)ptions",
                "(G)raveyard",
                "(E)scape to reality"};

        const P screen_dims = panels::dims(Panel::screen);

        P menu_pos((screen_dims.x * 13) / 20, screen_dims.y / 2);

        P pos = menu_pos;

        for (size_t i = 0; i < labels.size(); ++i) {
                const std::string label = labels[i];

                const bool is_marked = m_browser.is_at_idx((int)i);

                auto str = label.substr(0, 3);

                auto color =
                        is_marked
                        ? colors::menu_key_highlight()
                        : colors::menu_key_dark();

                io::draw_text(str, Panel::screen, pos, color);

                str = label.substr(3, std::string::npos);

                color =
                        is_marked
                        ? colors::menu_highlight()
                        : colors::menu_dark();

                io::draw_text(str, Panel::screen, pos.with_x_offset(3), color);

                // ++pos.x;
                ++pos.y;
        }

        const Color quote_clr = colors::gray_brown().shaded(40);

        std::vector<std::string> quote_lines;

        int quote_w = 45;

        // Decrease quote width until we find a width that doesn't leave a
        // "tiny" string on the last line (looks very ugly),
        while (quote_w != 0) {
                quote_lines =
                        text_format::split(
                                "\"" + s_current_quote + "\"",
                                quote_w);

                const size_t min_str_w_last_line = 20;

                const std::string& last_line = quote_lines.back();

                // Is the length of the current last line at least as long as
                // the minimum required?
                if (last_line.length() >= min_str_w_last_line) {
                        break;
                }

                --quote_w;
        }

        if (quote_w > 0) {
                int quote_y = 0;

                if (quote_lines.size() < (labels.size() - 1)) {
                        quote_y = menu_pos.y + 1;
                }
                else if (quote_lines.size() > (labels.size() + 1)) {
                        quote_y = menu_pos.y - 1;
                }
                else {
                        // Number of quote lines is within +/- 1 difference from
                        // number of main menu labels
                        quote_y = menu_pos.y;
                }

                pos.set(
                        std::max((quote_w / 2) + 2, (screen_dims.x * 3) / 10),
                        quote_y);

                pos.y =
                        std::min(
                                screen_dims.y - (int)quote_lines.size() - 1,
                                pos.y);

                for (const std::string& line : quote_lines) {
                        io::draw_text_center(
                                line,
                                Panel::screen,
                                pos,
                                quote_clr);

                        ++pos.y;
                }
        }

        std::string build_str = version_info::g_version_str + " (";

        if (!s_git_sha1_str.empty()) {
                build_str += s_git_sha1_str + ", ";
        }

        build_str += version_info::g_date_str + ")";

        io::draw_text_right(
                " " + build_str + " ",
                Panel::screen,
                {panels::x1(Panel::screen) - 1, 0},
                colors::gray());

        io::draw_text_center(
                std::string(
                        " " +
                        version_info::g_copyright_str +
                        ", " +
                        version_info::g_license_str +
                        " "),
                Panel::screen,
                {panels::center_x(Panel::screen), panels::y1(Panel::screen)},
                colors::gray());

}  // draw

void MainMenuState::update()
{
        auto action = MenuAction::selected;

        if (config::is_stress_test()) {
                // Stress-test mode, we just want to run everything
                // automatically without requiring manual input.
                action = MenuAction::selected;
        }
        else {
                const auto input = io::read_input();

                action =
                        m_browser.read(
                                input,
                                MenuInputMode::scrolling_and_letters,
                                ForceAutoSelect::yes);
        }

        switch (action) {
        case MenuAction::selected:
                switch (m_browser.y()) {
                case 0: {
                        if (!config::is_bot_playing()) {
                                if (saving::is_save_available()) {
                                        const bool should_proceed =
                                                query_overwrite_savefile();

                                        if (!should_proceed) {
                                                return;
                                        }
                                }
                        }

                        audio::fade_out_music();

                        init::init_session();

                        states::push(std::make_unique<NewGameState>());
                } break;

                case 1: {
                        // Load game
                        if (saving::is_save_available()) {
                                audio::fade_out_music();

                                init::init_session();

                                saving::load_game();

                                auto game_state = std::make_unique<GameState>(
                                        GameEntryMode::load_game);

                                states::push(std::move(game_state));
                        }
                        else {
                                // No save available
                                popup::Popup(popup::AddToMsgHistory::no)
                                        .set_msg("No saved game found")
                                        .run();
                        }
                } break;

                case 2: {
                        // Manual
                        states::push(std::make_unique<BrowseManual>());
                } break;

                case 3: {
                        // Options
                        states::push(std::make_unique<OptionsState>());
                } break;

                case 4: {
                        // Highscores
                        states::push(std::make_unique<BrowseHighscore>());
                } break;

                case 5: {
                        // Exit
                        states::pop();
                } break;

                }  // switch
                break;

        default:
                break;

        }  // switch
}  // update

void MainMenuState::on_start()
{
        const auto sha1_result = version_info::read_git_sha1_str_from_file();

        s_git_sha1_str = sha1_result.value_or("");

        s_current_quote = messages::get_random_menu_quote();

        // Do not play the music in debug mode (it gets extremely repetitive).
#ifdef NDEBUG
        audio::play_music(audio::MusId::cthulhiana_madness);
#endif
}

void MainMenuState::on_resume()
{
        // Do not play the music in debug mode (it gets extremely repetitive).
#ifdef NDEBUG
        audio::play_music(audio::MusId::cthulhiana_madness);
#endif
}

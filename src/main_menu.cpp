// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "main_menu.hpp"

#include "menu_page.hpp"

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
#include "common_text.hpp"
#include "config.hpp"
#include "create_character.hpp"
#include "credits.hpp"
#include "draw_box.hpp"
#include "fade.hpp"
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
static std::string s_current_quote;

static bool query_overwrite_savefile()
{
        int choice = 0;

        popup::Popup(popup::AddToMsgHistory::no)
                .set_title("A saved game exists")
                .set_msg("Start a new game?")
                .setup_menu_mode(
                        {"(Y)es", "(N)o"},
                        &choice)
                .run();

        return (choice == 0);
}

// -----------------------------------------------------------------------------
// Alpha notice state
// -----------------------------------------------------------------------------
std::string AlphaNoticeState::page_title() const
{
        return "Developer Note";
}

std::string AlphaNoticeState::page_text() const
{
        const std::string line_1 =
                "This is an early alpha of the Android port of Infra Arcana.";

        const std::string line_2 =
                "It is perfectly playable, but expect bugs or unrefinement.";

        const std::string signature = "- Love, Werewolf Camp";

        // Right-aligned by padding with non-breaking spaces (plain spaces
        // can be dropped as wrap points; the base page has no alignment
        // support), in the frame's title color
        std::string text = line_1 + "\n" + line_2 + "\n\n";

        const size_t block_w = std::max(line_1.size(), line_2.size());

        // Pulled in a touch from the right edge
        const size_t right_indent = 6;

        for (size_t i = signature.size() + right_indent; i < block_w; ++i) {
                text += "{_}";
        }

        text += "{GUICOLOR_TITLE}" + signature;

        return text;
}

void AlphaNoticeState::on_confirmed()
{
        // Acknowledged - a beat of black, then the title screen covers it
        fade::to_black(1500);

        states::pop();
}

void AlphaNoticeState::on_cancelled()
{
        // The device back button acknowledges too - a boot notice has
        // nothing to cancel back to
        on_confirmed();
}

// -----------------------------------------------------------------------------
// Main menu state
// -----------------------------------------------------------------------------
MainMenuState::MainMenuState() :
        m_browser(MenuBrowser(7))
{
        m_browser.set_custom_menu_keys({'n', 'r', 't', 'o', 'g', 'c', 'e'});
}

MainMenuState::~MainMenuState() = default;

StateId MainMenuState::id() const
{
        return StateId::main_menu;
}

void MainMenuState::draw()
{
        if (config::is_tiles_mode()) {
                draw_box(panels::screen_box_area());
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
                "(S)ettings",
                "(G)raveyard",
                "(C)redits",
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

        // The version (core version + port build increment), inset by the
        // standard border content inset (same as the [ x ] control) - the
        // copyright and license footer lives on the credits page
        io::draw_text_right(
                " " + version_info::g_build_version_str + " ",
                Panel::screen,
                {panels::screen_box_area().p1.x - g_screen_border_content_inset,
                 panels::screen_box_area().p0.y},
                colors::gray());

        // The standard select guidance footer
        io::draw_text_center(
                " " + common_text::g_menu_select_hint + " ",
                Panel::screen,
                {panels::center_x(Panel::screen), panels::screen_box_area().p1.y},
                colors::title(),
                io::DrawBg::yes,
                colors::black(),
                true);  // Allow pixel-level adjustment

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

                        // A new game starts with no menu history: pages must
                        // not open on the previous game's picks (the marked
                        // entry is remembered per page title, see
                        // MenuPageState). NOTE: This is the NEW GAME entry
                        // point only - stepping back during character
                        // creation restarts the session too, and must keep
                        // its memory to return to the entry you picked.
                        forget_remembered_marked_entries();

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
                        // Settings
                        states::push(std::make_unique<SettingsState>());
                } break;

                case 4: {
                        // Highscores
                        states::push(std::make_unique<BrowseHighscore>());
                } break;

                case 5: {
                        // Credits
                        states::push(std::make_unique<CreditsState>());
                } break;

                case 6: {
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

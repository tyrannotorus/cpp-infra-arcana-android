// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "query.hpp"

#include <memory>
#include <string>

#include "SDL_keycode.h"
#include "colors.hpp"
#include "config.hpp"
#include "context_pins.hpp"
#include "game_commands.hpp"
#include "io.hpp"
#include "msg_log.hpp"
#include "popup.hpp"
#include "state.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static bool s_is_inited = false;

// -----------------------------------------------------------------------------
// query
// -----------------------------------------------------------------------------
namespace query
{
void init()
{
        s_is_inited = true;
}

void cleanup()
{
        s_is_inited = false;
}

BinaryAnswer yes_or_no(
        std::optional<SpecialChoice> special_choice,
        const AllowSpaceCancel allow_space_cancel)
{
        if (!s_is_inited || config::is_bot_playing()) {
                return BinaryAnswer::yes;
        }

        // Show tappable answer pins above the action bar (tapping a pin
        // sends the corresponding key)
        msg_log::set_waiting_query(true);

        context_pins::add("yes", 'y', colors::menu_highlight());
        context_pins::add("no", 'n', colors::menu_dark());

        if (special_choice) {
                context_pins::add(
                        special_choice->label,
                        special_choice->key,
                        colors::menu_dark());
        }

        states::draw();
        io::update_screen();

        io::InputData input;

        while (true) {
                input = io::read_input();

                const bool is_special_key_pressed =
                        special_choice.has_value() &&
                        (input.key == special_choice->key);

                const bool is_canceled_with_space =
                        (input.key == SDLK_SPACE) &&
                        (allow_space_cancel == AllowSpaceCancel::yes);

                if ((input.key == 'y') ||
                    (input.key == 'n') ||
                    (input.key == SDLK_ESCAPE) ||
                    is_canceled_with_space ||
                    is_special_key_pressed) {
                        break;
                }
        }

        msg_log::set_waiting_query(false);
        context_pins::clear();

        if (special_choice.has_value() &&
            (input.key == special_choice->key)) {
                return BinaryAnswer::special;
        }
        else if (input.key == 'y') {
                return BinaryAnswer::yes;
        }
        else {
                return BinaryAnswer::no;
        }
}

io::InputData letter(const bool accept_enter)
{
        io::InputData input;

        if (!s_is_inited || config::is_bot_playing()) {
                input.key = 'a';

                return input;
        }

        states::draw();
        io::update_screen();

        while (true) {
                input = io::read_input();

                if ((accept_enter && (input.key == SDLK_RETURN)) ||
                    (input.key == SDLK_ESCAPE) ||
                    (input.key == SDLK_SPACE) ||
                    ((input.key >= 'a') && (input.key <= 'z')) ||
                    ((input.key >= 'A') && (input.key <= 'Z'))) {
                        return input;
                }
        }

        // Unreachable
        return input;
}

int number(
        const QueryNumberConfig& config,
        const std::string& title,
        const std::string& msg)
{
        if (!s_is_inited || config::is_bot_playing()) {
                return 0;
        }

        int result = 0;

        auto popup = std::make_unique<popup::Popup>(popup::AddToMsgHistory::no);

        popup->set_title(title);
        popup->set_msg(msg);

        popup->setup_number_query_mode(config, &result);

        popup->run();

        return result;
}

void wait_for_msg_more()
{
        if (!s_is_inited || config::is_bot_playing()) {
                return;
        }

        states::draw();
        io::update_screen();

        // Only some keys confirm more prompts - a tap sends one of them (see
        // io_input), so this is what every confirmation goes through
        while (true) {
                const auto input = io::read_input();

                if ((input.key == SDLK_SPACE) ||
                    (input.key == SDLK_ESCAPE) ||
                    (input.key == SDLK_RETURN) ||
                    (input.key == SDLK_TAB)
#ifndef NDEBUG
                    ||
                    // Cheat key for descending
                    (input.key == SDLK_F2) ||
                    // Cheat key for teleporting
                    (input.key == SDLK_F7)
#endif  // NDEBUG
                ) {
                        break;
                }
        }
}

void wait_for_confirm()
{
        if (!s_is_inited || config::is_bot_playing()) {
                return;
        }

        states::draw();
        io::update_screen();

        while (true) {
                const auto input = io::read_input();

                if ((input.key == SDLK_SPACE) ||
                    (input.key == SDLK_ESCAPE) ||
                    (input.key == SDLK_RETURN)) {
                        break;
                }
        }
}

Dir dir(const AllowCenter allow_center)
{
        if (!s_is_inited || config::is_bot_playing()) {
                return Dir::END;
        }

        // The direction is answered by swiping - offer a tappable cancel
        // pin (there is deliberately no escape action bar button)
        msg_log::set_waiting_query(true);

        context_pins::add(
                "cancel",
                (char)SDLK_ESCAPE,
                colors::menu_dark());

        states::draw();
        io::update_screen();

        const auto cleanup = []() {
                msg_log::set_waiting_query(false);
                context_pins::clear();
        };

        while (true) {
                const auto input = io::read_input();

                const auto game_cmd = game_commands::to_cmd(input);

                switch (game_cmd) {
                case GameCmd::right:
                        cleanup();
                        return Dir::right;

                case GameCmd::down:
                        cleanup();
                        return Dir::down;

                case GameCmd::left:
                        cleanup();
                        return Dir::left;

                case GameCmd::up:
                        cleanup();
                        return Dir::up;

                case GameCmd::down_right:
                        cleanup();
                        return Dir::down_right;

                case GameCmd::up_right:
                        cleanup();
                        return Dir::up_right;

                case GameCmd::down_left:
                        cleanup();
                        return Dir::down_left;

                case GameCmd::up_left:
                        cleanup();
                        return Dir::up_left;

                case GameCmd::wait:
                        if (allow_center == AllowCenter::yes) {
                                cleanup();
                                return Dir::center;
                        }
                        break;

                default:
                        break;
                }

                if ((input.key == SDLK_SPACE) || (input.key == SDLK_ESCAPE)) {
                        cleanup();
                        return Dir::END;
                }
        }

        // Unreachable
        return Dir::END;
}

}  // namespace query

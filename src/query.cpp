// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "query.hpp"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "SDL_keycode.h"
#include "colors.hpp"
#include "config.hpp"
#include "game_commands.hpp"
#include "io.hpp"
#include "panel.hpp"
#include "popup.hpp"
#include "pos.hpp"
#include "random.hpp"

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

void wait_for_key_press()
{
        if (s_is_inited && !config::is_bot_playing()) {
                io::update_screen();

                io::read_input();
        }
}

BinaryAnswer yes_or_no(
        std::optional<char> key_for_special_event,
        const AllowSpaceCancel allow_space_cancel)
{
        if (!s_is_inited || config::is_bot_playing()) {
                return BinaryAnswer::yes;
        }

        io::update_screen();

        io::InputData input;

        while (true) {
                input = io::read_input();

                const bool is_special_key_pressed =
                        key_for_special_event.has_value() &&
                        (input.key == key_for_special_event.value());

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

        if (key_for_special_event.has_value() &&
            (input.key == key_for_special_event.value())) {
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

        io::update_screen();

        // Determine criteria for confirming more prompt (decided by config)
        if (config::is_any_key_confirm_more()) {
                wait_for_key_press();
        }
        else {
                // Only some keys confirm more prompts
                while (true) {
                        const auto input = io::read_input();

                        if ((input.key == SDLK_SPACE) ||
                            (input.key == SDLK_ESCAPE) ||
                            (input.key == SDLK_RETURN) ||
                            (input.key == SDLK_TAB)) {
                                break;
                        }
                }
        }
}

void wait_for_confirm()
{
        if (!s_is_inited || config::is_bot_playing()) {
                return;
        }

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

        io::update_screen();

        while (true) {
                const auto input = io::read_input();

                const auto game_cmd = game_commands::to_cmd(input);

                switch (game_cmd) {
                case GameCmd::right:
                        return Dir::right;

                case GameCmd::down:
                        return Dir::down;

                case GameCmd::left:
                        return Dir::left;

                case GameCmd::up:
                        return Dir::up;

                case GameCmd::down_right:
                        return Dir::down_right;

                case GameCmd::up_right:
                        return Dir::up_right;

                case GameCmd::down_left:
                        return Dir::down_left;

                case GameCmd::up_left:
                        return Dir::up_left;

                case GameCmd::wait:
                        if (allow_center == AllowCenter::yes) {
                                return Dir::center;
                        }
                        break;

                default:
                        break;
                }

                if ((input.key == SDLK_SPACE) || (input.key == SDLK_ESCAPE)) {
                        return Dir::END;
                }
        }

        // Unreachable
        return Dir::END;
}

}  // namespace query

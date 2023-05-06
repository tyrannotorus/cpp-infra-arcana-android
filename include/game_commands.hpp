// =============================================================================
// Copyright 2011-2023 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef GAME_COMMANDS_HPP
#define GAME_COMMANDS_HPP

namespace io
{
struct InputData;
}  // namespace io

enum class GameCmd
{
        undefined,

        none,

        right,
        down,
        left,
        up,
        up_right,
        down_right,
        down_left,
        up_left,
        auto_move_right,
        auto_move_down,
        auto_move_left,
        auto_move_up,
        auto_move_up_right,
        auto_move_down_right,
        auto_move_down_left,
        auto_move_up_left,
        wait,
        wait_long,
        reload,
        kick,
        close,
        unload,
        fire,
        get,
        inventory,
        apply_item,
        drop_item,
        swap_weapon,
        throw_item,
        toggle_lantern,
        look,
        auto_melee,
        cast_spell,
        make_noise,
        disarm,
        char_descr,
        minimap,
        msg_history,
        manual,
        options,
        game_menu,
        quit,

// Debug commands
#ifndef NDEBUG
        debug_f2,
        debug_f3,
        debug_f4,
        debug_f5,
        debug_f6,
        debug_f7,
        debug_f8,
        debug_f9,
        debug_f10,
        debug_shift_f2,
        debug_shift_f3,
        debug_shift_f4,
#endif  // NDEBUG
};

namespace game_commands
{
// NOTE: This is a pure function, except for reading the options
GameCmd to_cmd(const io::InputData& input);

void handle(GameCmd cmd);

}  // namespace game_commands

#endif  // GAME_COMMANDS_HPP

// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef GAME_COMMANDS_HPP
#define GAME_COMMANDS_HPP

#include <string>

namespace io
{
struct InputData;
}  // namespace io

namespace item
{
class Wpn;
}  // namespace item

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
        kick_at_look_pin,
        close,
        unload,
        fire,
        toggle_aim,
        get,
        inventory,
        swap_weapon,
        throw_item,
        toggle_throw,
        toggle_lantern,
        use_medical_bag,
        look,
        auto_interact,
        cast_spell,
        make_noise,
        disarm,
        char_descr,
        minimap,
        msg_history,
        manual,
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
        debug_shift_f5,
        debug_shift_f6,
        debug_shift_f7,
#endif  // NDEBUG
};

namespace game_commands
{
// NOTE: This is a pure function, except for reading the options
GameCmd to_cmd(const io::InputData& input);

void handle(GameCmd cmd);

// Why the wielded ranged weapon cannot be fired right now - out of ammo,
// on fire, a Mi-go gun that would drain more health than is left - or an
// empty string when it can be.
//
// NOTE: Targeting is a MODE, and it is engaged whether or not the shot is
// possible: a gun that has run dry is exactly when the [ reload ] and
// [ swap weapon ] pins inside that mode are wanted, and refusing to open
// it would leave a player with an empty gun no way to reach either. So
// the aim marker opens regardless, and asks this to decide whether to
// offer [ fire ], what to say instead, and how to answer a fire command
// it cannot obey.
std::string ranged_wpn_unfireable_reason(const item::Wpn& wpn);

char fire_key();
char throw_key();
char view_key();
char get_key();

char unload_key();
char close_key();
char disarm_key();

// The [ swap ] and [ reload ] pins of the aim marker (see marker.cpp)
char swap_weapon_key();
char reload_key();

// Key of the contextual [ kick ] look-pin button (see
// GameState::on_map_panned). It is deliberately NOT the kick key: the
// kick command itself - and its action bar button - keeps asking for a
// direction, while this one kicks the pinned cell directly. No keyboard
// key produces it; the button carries the keycode (see
// context_pins::add).
int kick_at_look_pin_key();

// Keys of the action bar's target and throw buttons. They are
// deliberately NOT the fire and throw keys: a bar button TOGGLES its
// targeting mode - engaging it, and dropping out of it when tapped again
// while it is engaged - while the plain keys (the keyboard, and the
// [ fire ] / [ throw ] context pins) always mean "loose it at the
// reticle". No keyboard key produces these.
int toggle_aim_key();
int toggle_throw_key();

}  // namespace game_commands

#endif  // GAME_COMMANDS_HPP

// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "game_commands.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <memory>
#include <ostream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "SDL_keycode.h"
#include "actions_config.hpp"
#include "actor.hpp"
#include "actor_data.hpp"
#include "actor_factory.hpp"
#include "actor_move.hpp"
#include "actor_player_state.hpp"
#include "actor_see.hpp"
#include "array2.hpp"
#include "attack.hpp"
#include "audio_data.hpp"
#include "auto_interact.hpp"
#include "bash.hpp"
#include "character_descr.hpp"
#include "close.hpp"
#include "colors.hpp"
#include "common_text.hpp"
#include "config.hpp"
#include "debug.hpp"
#include "direction.hpp"
#include "disarm.hpp"
#include "explosion.hpp"
#include "game.hpp"
#include "game_summary_data.hpp"
#include "game_time.hpp"
#include "gfx.hpp"
#include "global.hpp"
#include "init.hpp"
#include "inventory.hpp"
#include "inventory_handling.hpp"
#include "io.hpp"
#include "item.hpp"
#include "item_data.hpp"
#include "item_explosive.hpp"
#include "item_factory.hpp"
#include "item_weapon.hpp"
#include "knockback.hpp"
#include "manual.hpp"
#include "map.hpp"
#include "map_travel.hpp"
#include "mapgen.hpp"
#include "marker.hpp"
#include "minimap.hpp"
#include "msg_log.hpp"
#include "pickup.hpp"
#include "player_bon.hpp"
#include "player_spells.hpp"
#include "popup.hpp"
#include "pos.hpp"
#include "property.hpp"
#include "property_data.hpp"
#include "property_factory.hpp"
#include "property_handler.hpp"
#include "query.hpp"
#include "random.hpp"
#include "rect.hpp"
#include "reload.hpp"
#include "saving.hpp"
#include "sound.hpp"
#include "state.hpp"
#include "teleport.hpp"
#include "terrain.hpp"
#include "terrain_data.hpp"
#include "terrain_door.hpp"
#include "terrain_factory.hpp"
#include "terrain_trap.hpp"
#include "text_format.hpp"
#include "view_actor_descr.hpp"
#include "viewport.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
// Opens the detailed description of a visible monster at the look "pin"
// (the view's centering point while the map is manually panned, see
// GameState::on_map_panned). The view key is also sent by tapping the
// [ describe ] context pin.
static void handle_view_command()
{
        if (!viewport::is_pan_active()) {
                // The view is centered on the player - there is no pinned
                // cell to describe
                return;
        }

        const P pin_pos = viewport::center_map_pos();

        if (!map::is_pos_inside_map(pin_pos)) {
                return;
        }

        actor::Actor* const actor = map::living_actor_at(pin_pos);

        if (actor &&
            !actor::is_player(actor) &&
            actor::can_player_see_actor(*actor)) {
                msg_log::clear();

                states::push(std::make_unique<ViewActorDescr>(*actor));
        }
}

// Kicks the cell at the look "pin" directly, with no direction query -
// the contextual [ kick ] button shown while the map is panned onto an
// adjacent cell (see GameState::on_map_panned). The plain kick command
// keeps its direction query, for the keyboard and the action bar button.
static void handle_kick_at_look_pin_command()
{
        if (!viewport::is_pan_active()) {
                return;
        }

        const P pin_pos = viewport::center_map_pos();

        if (!map::is_pos_inside_map(pin_pos) ||
            !pin_pos.is_adjacent(map::g_player->m_pos) ||
            (pin_pos == map::g_player->m_pos)) {
                return;
        }

        msg_log::clear();

        bash::run_at(pin_pos);
}

static void query_quit()
{
        int choice = 0;

        const std::string msg =
                "Save and highscore will not be kept "
                "(use stairs to save the game).";

        popup::Popup(popup::AddToMsgHistory::no)
                .set_title("Quit the current game?")
                .set_msg(msg)
                .setup_menu_mode(
                        {"(N)o", "(Y)es"},
                        &choice)
                .run();

        if (choice == 1) {
                // Choosing to quit the game deletes the save
                saving::erase_save();

                states::pop_until(StateId::main_menu);
        }
}

static bool allow_player_fire_mi_go_weapon(const int hp_drained)
{
        const bool has_enough_hp = (map::g_player->m_hp > hp_drained);

        if (has_enough_hp) {
                return true;
        }
        else {
                // Not enough HP - allow firing the gun if player has the
                // Prolonged Life trait and enough fervor instead.
                const bool has_prolonged_life = player_bon::has_trait(Trait::prolonged_life);

                const int hp = map::g_player->m_hp;
                const int fervor = actor::player_state::g_exorcist_fervor;

                const bool has_enough_hp_and_fervor = ((hp + fervor) > hp_drained);

                return (has_prolonged_life && has_enough_hp_and_fervor);
        }
}

static void handle_fire_command_melee(item::Wpn& wpn)
{
        states::push(std::make_unique<AimingMeleeWpn>(map::g_player->m_pos, wpn));
}

static void handle_fire_command_firearm(item::Wpn& wpn)
{
        const bool is_empty =
                (wpn.m_ammo_loaded <= 0) &&
                !wpn.data().ranged.has_infinite_ammo;

        if (is_empty && config::is_ranged_wpn_auto_reload()) {
                // The player asked for the fire command to load the gun
                // for them
                const int tick_count_before = game_time::tick_count();

                reload::try_reload(*map::g_player, &wpn);

                if (game_time::tick_count() != tick_count_before) {
                        // Loading it (or fumbling the attempt) WAS this
                        // command's action, and it cost the turn - aiming
                        // comes on the next one
                        return;
                }

                // There was nothing to load it with. Fall through and
                // engage the targeting anyway - see below.
        }

        // NOTE: The marker opens whether or not the gun can be fired (see
        // game_commands::ranged_wpn_unfireable_reason). Targeting is a
        // MODE, and a gun that has run dry is exactly when the [ reload ]
        // and [ swap weapon ] pins inside that mode are wanted - refusing
        // to open it here, as this used to, left a player holding an empty
        // gun no way to reach either. The marker says why it cannot be
        // fired, and offers no [ fire ] pin.
        states::push(std::make_unique<Aiming>(map::g_player->m_pos, wpn));
}

static void handle_fire_command()
{
        item::Wpn* weapon = nullptr;

        item::Item* wielded_item = map::g_player->m_inv.item_in_slot(SlotId::wpn);

        if (wielded_item) {
                weapon = static_cast<item::Wpn*>(wielded_item);
        }
        else {
                weapon = &map::g_player->unarmed_wpn();
        }

        if (!weapon) {
                ASSERT(false);

                return;
        }

        // TODO: Pressing "F" could perhaps force an aimed melee attack for
        // ranged weapons (i.e. buttstroke).

        if (weapon->data().ranged.is_ranged_wpn) {
                handle_fire_command_firearm(*weapon);
        }
        else {
                handle_fire_command_melee(*weapon);
        }
}

static void handle_activate_item_shortcut_command(const item::Id item_id)
{
        item::Item* item = nullptr;

        for (auto* const found_item : map::g_player->m_inv.m_backpack) {
                if (found_item->id() == item_id) {
                        item = found_item;

                        break;
                }
        }

        if (item) {
                item->activate(map::g_player);
        }
        else {
                std::unique_ptr<item::Item> tmp_item(item::make(item_id));

                const std::string name =
                        tmp_item->name(
                                ItemNameType::a,
                                ItemNameInfo::none,
                                ItemNameAttackInfo::none);

                msg_log::add("I am not carrying " + name + ".");
        }
}

// The in-game menu (the hamburger button) - a standard fullscreen menu
// page. Screens opened from it (options submenus, the actions config, the
// manual) stack on top: closing such a screen (the [ x ] control / back
// button) falls back to this page, which keeps its state like any other
// menu page. The [ x ] on THIS page returns to the game.
class GameMenuState : public MenuPageState
{
public:
        StateId id() const override
        {
                return StateId::game_menu;
        }

protected:
        std::string page_title() const override
        {
                return "Menu";
        }

        std::vector<MenuPageEntry> page_entries() const override
        {
                // NOTE: The letter prefixes are visual flavour only -
                // there is no keyboard to type them on, entries are engaged
                // by tapping. Duplicates ("A", "V") are therefore fine.
                return {
                        {"(A)ctions", ""},
                        {"(C)haracter description", ""},
                        {"(M)essage history", ""},
                        {"(V)iew Minimap", ""},
                        {"(V)ideo", ""},
                        {"(A)udio", ""},
                        {"(I)nput", ""},
                        {"(G)ameplay", ""},
                        {"(T)ome of Wisdom", ""},
                        {"(Q)uit", ""},
                };
        }

        void on_entry_selected(const int idx) override
        {
                switch (idx) {
                case 0: {
                        // Action bar configuration
                        states::push(
                                std::make_unique<ActionsConfigState>());
                } break;

                case 1: {
                        // Character description - it has no action bar
                        // button, this is where it lives
                        const game_summary_data::GameSummaryData game_data =
                                game_summary_data::collect();

                        auto character_descr =
                                std::make_unique<CharacterDescr>();

                        character_descr->setup(game_data);

                        states::push(std::move(character_descr));
                } break;

                case 2: {
                        // Message history - it has no action bar button,
                        // this is where it lives
                        states::push(std::make_unique<MsgHistoryState>());
                } break;

                case 3: {
                        // The minimap DOES keep its action bar button (an
                        // optional one, like every bar action) - it is here
                        // as well, for players who turn that button off
                        states::push(std::make_unique<ViewMinimap>());
                } break;

                case 4:
                case 5:
                case 6:
                case 7: {
                        // An options submenu (Video / Audio / Input /
                        // Gameplay)
                        const auto submenu =
                                (config::OptionSubmenuType)(idx - 4);

                        states::push(
                                std::make_unique<OptionsSubmenuState>(
                                        submenu));
                } break;

                case 8: {
                        // Help (the manual)
                        states::push(std::make_unique<BrowseManual>());
                } break;

                case 9: {
                        // NOTE: An accepted quit pops all states down to
                        // the main menu - this state is destroyed, so
                        // nothing must be done here afterwards. A declined
                        // quit simply stays on this page.
                        query_quit();
                } break;

                default:
                        break;
                }
        }
};

static void handle_game_menu_command()
{
        states::push(std::make_unique<GameMenuState>());
}

static void handle_swap_weapon_command()
{
        auto* const wielded = map::g_player->m_inv.item_in_slot(SlotId::wpn);
        auto* const alt = map::g_player->m_inv.item_in_slot(SlotId::wpn_alt);

        if (!wielded && !alt) {
                // No wielded weapon and no alt weapon
                msg_log::add("I have neither a wielded nor a prepared weapon.");

                return;
        }

        // Player is wielding a weapon and/or have an alt weapon.

        std::string alt_name;

        if (alt) {
                alt_name =
                        alt->name(
                                ItemNameType::a,
                                ItemNameInfo::yes,
                                ItemNameAttackInfo::none);
        }

        // War veteran swaps instantly
        const bool is_instant = player_bon::bg() == Bg::war_vet;

        const std::string swift_str = is_instant ? "swiftly " : "";

        if (wielded && alt) {
                msg_log::add(
                        "I " +
                        swift_str +
                        "swap to " +
                        alt_name +
                        ".");
        }
        else if (wielded && !alt) {
                const std::string name = wielded->name(ItemNameType::plain);

                msg_log::add(
                        "I " +
                        swift_str +
                        "put away my " +
                        name +
                        ".");
        }
        else {
                // No weapon wielded.
                msg_log::add(
                        "I " +
                        swift_str +
                        "wield " +
                        alt_name +
                        ".");
        }

        map::g_player->m_inv.swap_wielded_and_prepared();

        if (!is_instant) {
                game_time::tick();
        }
}

static void handle_auto_move_command(const Dir dir)
{
        bool is_allowed = true;

        std::string prevent_msg;

        if (actor::is_player_seeing_burning_terrain()) {
                is_allowed = false;
                prevent_msg = common_text::g_fire_prevent_cmd;
        }
        else if (!actor::seen_foes(*map::g_player).empty()) {
                is_allowed = false;
                prevent_msg = common_text::g_mon_prevent_cmd;
        }
        else if (!map::g_player->m_properties.allow_see()) {
                is_allowed = false;
                prevent_msg = "Not while blind.";
        }
        else if (map::g_player->m_properties.has(prop::Id::poisoned)) {
                is_allowed = false;
                prevent_msg = "Not while poisoned.";
        }
        else if (map::g_player->m_properties.has(prop::Id::confused)) {
                is_allowed = false;
                prevent_msg = "Not while confused.";
        }
        else if (map::g_player->m_properties.has(prop::Id::infected)) {
                is_allowed = false;
                prevent_msg = "Not while infected.";
        }

        if (!is_allowed) {
                msg_log::add(
                        prevent_msg,
                        colors::text(),
                        MsgInterruptPlayer::no,
                        MorePromptOnMsg::no,
                        CopyToMsgHistory::no);

                return;
        }

        map::g_player->set_auto_move(dir);
}

static GameCmd to_cmd_default(const io::InputData& input)
{
        // When running on windows, with numlock enabled, each numpad key press
        // yields two input events - one for the keypad key, and one as a number
        // key. This seems to be due to a bug in SDL (or maybe it's just how it
        // works on windows). As a workaround, we skip numerical keys here.
        if (input.key >= '0' && input.key <= '9') {
                return GameCmd::none;
        }

        switch (input.key) {
        case SDLK_KP_6:
                if (input.is_shift_held) {
                        return GameCmd::auto_move_right;
                }
                else {
                        return GameCmd::right;
                }

        case SDLK_KP_4:
                if (input.is_shift_held) {
                        return GameCmd::auto_move_left;
                }
                else {
                        return GameCmd::left;
                }

        case SDLK_KP_2:
                if (input.is_shift_held) {
                        return GameCmd::auto_move_down;
                }
                else {
                        return GameCmd::down;
                }

        case SDLK_KP_8:
                if (input.is_shift_held) {
                        return GameCmd::auto_move_up;
                }
                else {
                        return GameCmd::up;
                }

        case SDLK_KP_9:
                if (input.is_shift_held) {
                        return GameCmd::auto_move_up_right;
                }
                else {
                        return GameCmd::up_right;
                }

        case SDLK_KP_3:
                if (input.is_shift_held) {
                        return GameCmd::auto_move_down_right;
                }
                else {
                        return GameCmd::down_right;
                }

        case SDLK_KP_1:
                if (input.is_shift_held) {
                        return GameCmd::auto_move_down_left;
                }
                else {
                        return GameCmd::down_left;
                }

        case SDLK_KP_7:
                if (input.is_shift_held) {
                        return GameCmd::auto_move_up_left;
                }
                else {
                        return GameCmd::up_left;
                }

        case SDLK_KP_5:
        case '.':
                return GameCmd::wait;

        case SDLK_RIGHT:
                if (input.is_shift_held) {
                        return GameCmd::up_right;
                }
                else if (input.is_ctrl_held) {
                        return GameCmd::down_right;
                }
                else {
                        return GameCmd::right;
                }

        case SDLK_LEFT:
                if (input.is_shift_held) {
                        return GameCmd::up_left;
                }
                else if (input.is_ctrl_held) {
                        return GameCmd::down_left;
                }
                else {
                        return GameCmd::left;
                }

        case SDLK_DOWN:
                return GameCmd::down;

        case SDLK_UP:
                return GameCmd::up;

        case 's':
                return GameCmd::wait_long;

        case '?':
        case SDLK_F1:
                return GameCmd::manual;

        case 'r':
                return GameCmd::reload;

        case 'k':
        case 'w':
                return GameCmd::kick;

        case SDLK_F13:
                // Sent only by the contextual [ kick ] look-pin button
                return GameCmd::kick_at_look_pin;

        case 'c':
                return GameCmd::close;

        case 'u':
        case 'G':
                return GameCmd::unload;

        case 'f':
                return GameCmd::fire;

        case SDLK_F14:
                // Sent only by the action bar's target button
                return GameCmd::toggle_aim;

        case SDLK_F15:
                // Sent only by the action bar's throw button
                return GameCmd::toggle_throw;

        case 'g':
        case ',':
                return GameCmd::get;

        case 'i':
                return GameCmd::inventory;

        case 'z':
                return GameCmd::swap_weapon;

        case 't':
                return GameCmd::throw_item;

        case 'b':
                return GameCmd::use_medical_bag;

        case 'l':
        case 'e':
                return GameCmd::toggle_lantern;

        case 'v':
                return GameCmd::look;

        case SDLK_TAB:
                return GameCmd::auto_interact;

        case 'x':
                return GameCmd::cast_spell;

        case 'C':
        case '@':
                return GameCmd::char_descr;

        case SDLK_KP_0:
        case 'm':
                return GameCmd::minimap;

        case 'h':
        case 'M':
                return GameCmd::msg_history;

        case 'n':
        case 'o':
                return GameCmd::make_noise;

        case 'p':
                return GameCmd::disarm;

        case SDLK_ESCAPE:
                return GameCmd::game_menu;

        case 'Q':
                return GameCmd::quit;

                // Some cheat commands enabled in debug builds
#ifndef NDEBUG
        case SDLK_F2:
                if (input.is_shift_held) {
                        return GameCmd::debug_shift_f2;
                }
                else {
                        return GameCmd::debug_f2;
                }

        case SDLK_F3:
                if (input.is_shift_held) {
                        return GameCmd::debug_shift_f3;
                }
                else {
                        return GameCmd::debug_f3;
                }

        case SDLK_F4:
                if (input.is_shift_held) {
                        return GameCmd::debug_shift_f4;
                }
                else {
                        return GameCmd::debug_f4;
                }

        case SDLK_F5:
                if (input.is_shift_held) {
                        return GameCmd::debug_shift_f5;
                }
                else {
                        return GameCmd::debug_f5;
                }

        case SDLK_F6:
                if (input.is_shift_held) {
                        return GameCmd::debug_shift_f6;
                }
                else {
                        return GameCmd::debug_f6;
                }

        case SDLK_F7:
                if (input.is_shift_held) {
                        return GameCmd::debug_shift_f7;
                }
                else {
                        return GameCmd::debug_f7;
                }

        case SDLK_F8:
                return GameCmd::debug_f8;

        case SDLK_F9:
                return GameCmd::debug_f9;

        case SDLK_F10:
                return GameCmd::debug_f10;
#endif  // NDEBUG

        default:
                // Undefined command
                break;

        }  // switch

        return GameCmd::undefined;
}  // to_cmd_default

static GameCmd to_cmd_vi(const io::InputData& input)
{
        // Overriden keys for vi-mode
        switch (input.key) {
        case 'l':
                return GameCmd::right;

        case 'L':
                return GameCmd::auto_move_right;

        case 'j':
                return GameCmd::down;

        case 'J':
                return GameCmd::auto_move_down;

        case 'h':
                return GameCmd::left;

        case 'H':
                return GameCmd::auto_move_left;

        case 'k':
                return GameCmd::up;

        case 'K':
                return GameCmd::auto_move_up;

        case 'u':
                return GameCmd::up_right;

        case 'U':
                return GameCmd::auto_move_up_right;

        case 'n':
                return GameCmd::down_right;

        case 'N':
                return GameCmd::auto_move_down_right;

        case 'b':
                return GameCmd::down_left;

        case 'B':
                return GameCmd::auto_move_down_left;

        case 'y':
                return GameCmd::up_left;

        case 'Y':
                return GameCmd::auto_move_up_left;

        default:
                break;
        }

        // Input not overriden, delegate to default keys
        return to_cmd_default(input);

}  // to_cmd_vi

// -----------------------------------------------------------------------------
// game_commands
// -----------------------------------------------------------------------------
namespace game_commands
{
GameCmd to_cmd(const io::InputData& input)
{
        switch (config::input_mode()) {
        case InputMode::standard:
                return to_cmd_default(input);

        case InputMode::vi_keys:
                return to_cmd_vi(input);

        case InputMode::END:
                break;
        }

        PANIC;

        return (GameCmd)0;
}

void handle(const GameCmd cmd)
{
        // Input that maps to no command is INERT - it does not answer, and
        // it does not disturb what is on screen.
        //
        // NOTE: This is not the desktop case of a mistyped command. Every
        // tap that no control claimed arrives here (a tap synthesizes the
        // confirm key, which is no game command - see io_input), so a
        // finger landing an inch off a pin used to be met with "Unknown
        // command." and a [ help ] pin, and the status messages it was
        // reading - along with their context pins - were wiped to make
        // room for that. Both are gone: a mis-tap now leaves the screen
        // exactly as it was, which is also what keeps drag-to-look alive
        // through one (see the look-pin block at the end of this
        // function).
        if ((cmd == GameCmd::undefined) || (cmd == GameCmd::none)) {
                return;
        }

        msg_log::clear();

        switch (cmd) {
        case GameCmd::undefined:
        case GameCmd::none:
                break;

        case GameCmd::right: {
                actor::do_move_action(*map::g_player, Dir::right);
        } break;

        case GameCmd::down: {
                actor::do_move_action(*map::g_player, Dir::down);
        } break;

        case GameCmd::left: {
                actor::do_move_action(*map::g_player, Dir::left);
        } break;

        case GameCmd::up: {
                actor::do_move_action(*map::g_player, Dir::up);
        } break;

        case GameCmd::up_right: {
                actor::do_move_action(*map::g_player, Dir::up_right);
        } break;

        case GameCmd::down_right: {
                actor::do_move_action(*map::g_player, Dir::down_right);
        } break;

        case GameCmd::down_left: {
                actor::do_move_action(*map::g_player, Dir::down_left);
        } break;

        case GameCmd::up_left: {
                actor::do_move_action(*map::g_player, Dir::up_left);
        } break;

        case GameCmd::wait: {
                if (player_bon::has_trait(Trait::steady_aimer)) {
                        auto* const aiming =
                                prop::make(prop::Id::aiming);

                        aiming->set_duration(1);

                        map::g_player->m_properties.apply(aiming);
                }

                actor::do_move_action(*map::g_player, Dir::center);
        } break;

        case GameCmd::wait_long: {
                bool is_allowed = true;
                std::string prevent_msg;
                if (actor::is_player_seeing_burning_terrain()) {
                        is_allowed = false;
                        prevent_msg = common_text::g_fire_prevent_cmd;
                }
                else if (!actor::seen_foes(*map::g_player).empty()) {
                        is_allowed = false;
                        prevent_msg = common_text::g_mon_prevent_cmd;
                }
                else if (map::g_player->shock_tot() >= 100) {
                        is_allowed = false;
                        prevent_msg = common_text::g_shock_prevent_cmd;
                }
                else if (map::g_player->m_properties.has(prop::Id::infected)) {
                        is_allowed = false;
                        prevent_msg = "Not while infected.";
                }

                if (is_allowed) {
                        // NOTE: We should not print any "wait" message here,
                        // since it would look weird in some cases - e.g. when
                        // the waiting is immediately interrupted by a message
                        // from rearranging pistol magazines.

                        // NOTE: A 'long wait' merely performs "move" into the
                        // center position a number of turns (i.e. the same as
                        // pressing 'wait')
                        const int turns_to_apply = 5;

                        actor::player_state::g_wait_turns_left = (turns_to_apply - 1);

                        game_time::tick();
                }
                else {
                        // Not allowed to long-wait
                        msg_log::add(
                                prevent_msg,
                                colors::text(),
                                MsgInterruptPlayer::no,
                                MorePromptOnMsg::no,
                                CopyToMsgHistory::no);
                }
        } break;

        case GameCmd::manual: {
                states::push(std::make_unique<BrowseManual>());
        } break;

        case GameCmd::reload: {
                auto* const wpn =
                        map::g_player->m_inv.item_in_slot(SlotId::wpn);

                reload::try_reload(*map::g_player, wpn);
        } break;

        case GameCmd::kick: {
                bash::run();
        } break;

        case GameCmd::kick_at_look_pin: {
                handle_kick_at_look_pin_command();
        } break;

        case GameCmd::close: {
                close::player_try_close_or_jam();
        } break;

        case GameCmd::unload: {
                item_pickup::try_unload_or_pick();
        } break;

        case GameCmd::fire:
        case GameCmd::toggle_aim: {
                // Reaching here means targeting is NOT engaged (an open
                // marker would have handled the toggle itself, see
                // MarkerState::update) - so both engage it
                handle_fire_command();
        } break;

        case GameCmd::get: {
                item_pickup::try_pick();
        } break;

        case GameCmd::inventory: {
                states::push(std::make_unique<BrowseInv>());
        } break;

        case GameCmd::swap_weapon: {
                handle_swap_weapon_command();
        } break;

        case GameCmd::auto_move_right: {
                handle_auto_move_command(Dir::right);
        } break;

        case GameCmd::auto_move_down: {
                handle_auto_move_command(Dir::down);
        } break;

        case GameCmd::auto_move_left: {
                handle_auto_move_command(Dir::left);
        } break;

        case GameCmd::auto_move_up: {
                handle_auto_move_command(Dir::up);
        } break;

        case GameCmd::auto_move_up_right: {
                handle_auto_move_command(Dir::up_right);
        } break;

        case GameCmd::auto_move_down_right: {
                handle_auto_move_command(Dir::down_right);
        } break;

        case GameCmd::auto_move_down_left: {
                handle_auto_move_command(Dir::down_left);
        } break;

        case GameCmd::auto_move_up_left: {
                handle_auto_move_command(Dir::up_left);
        } break;

        case GameCmd::throw_item:
        case GameCmd::toggle_throw: {
                // Reaching here means no throw is being aimed (an open
                // marker would have handled the toggle itself, see
                // MarkerState::update) - so both start one.
                //
                // NOTE: A LIT explosive is carried like any other item now,
                // and is thrown from this list like any other item (see
                // SelectThrow)
                const bool is_allowed =
                        map::g_player->m_properties
                                .allow_attack_ranged(Verbose::yes);

                if (is_allowed) {
                        states::push(std::make_unique<SelectThrow>());
                }
        } break;

        case GameCmd::use_medical_bag: {
                handle_activate_item_shortcut_command(item::Id::medical_bag);
        } break;

        case GameCmd::toggle_lantern: {
                handle_activate_item_shortcut_command(item::Id::lantern);
        } break;

        case GameCmd::look: {
                // Looking around IS dragging the map (the center pin, see
                // GameState::on_map_panned) - there is no look mode to
                // enter. The view key instead describes a visible monster
                // at the pin. The Viewing marker state remains only for
                // flows where the game view is otherwise unreachable
                // (surveying the map from the trait pick info menu).
                handle_view_command();
        } break;

        case GameCmd::auto_interact: {
                auto_interact::run();
        } break;

        case GameCmd::cast_spell: {
                states::push(std::make_unique<BrowseSpell>());
        } break;

        case GameCmd::char_descr: {
                // Collect data from the game session.
                const game_summary_data::GameSummaryData game_data =
                        game_summary_data::collect();

                // Create the object that can present the data.
                auto character_descr = std::make_unique<CharacterDescr>();
                character_descr->setup(game_data);

                states::push(std::move(character_descr));
        } break;

        case GameCmd::minimap: {
                states::push(std::make_unique<ViewMinimap>());
        } break;

        case GameCmd::msg_history: {
                states::push(std::make_unique<MsgHistoryState>());
        } break;

        case GameCmd::make_noise: {
                if (player_bon::bg() == Bg::ghoul) {
                        msg_log::add("I let out a chilling howl.");
                }
                else {
                        msg_log::add("I make some noise.");
                }

                Snd snd(
                        "",
                        audio::SfxId::END,
                        IgnoreMsgIfOriginSeen::yes,
                        map::g_player->m_pos,
                        map::g_player,
                        SndVol::low,
                        AlertsMon::yes);

                snd_emit::run(snd);

                game_time::tick();
        } break;

        case GameCmd::disarm: {
                disarm::player_disarm();
        } break;

        case GameCmd::game_menu: {
                handle_game_menu_command();
        } break;

        case GameCmd::quit: {
                query_quit();
        } break;

        // Some cheat commands enabled in debug builds
#ifndef NDEBUG
        case GameCmd::debug_f2: {
                map_travel::go_to_nxt();
        } break;

        case GameCmd::debug_shift_f2: {
                // TODO: It would be more convenient to query for a string instead, so that the
                // monster ID string could be entered directly, instead of an index number (perhaps
                // with partial matches allowed).

                std::string msg = "Listing all properties (IDX    NAME):";

                for (size_t i = 0; i < (size_t)prop::Id::END; ++i) {
                        msg +=
                                "\n" +
                                std::to_string(i) +
                                "    " +
                                prop::g_data[i].name;
                }

                TRACE << msg << std::endl;

                const std::string query_str = "Apply property";

                query::QueryNumberConfig query_config;

                query_config.allowed_range = {0, (int)prop::Id::END};
                query_config.cancel_returns_default = false;

                const int id_to_apply = query::number(query_config, query_str);

                if (id_to_apply == -1) {
                        return;
                }

                prop::Prop* const prop = prop::make((prop::Id)id_to_apply);

                prop->set_duration(std::max(10, prop->nr_turns_left()));

                map::g_player->m_properties.apply(prop);

        } break;

        case GameCmd::debug_f3: {
                game::incr_player_xp(100, Verbose::no);
        } break;

        case GameCmd::debug_shift_f3: {
                const std::vector<terrain::DoorType> door_types = {
                        terrain::DoorType::wood,
                        terrain::DoorType::gate,
                        terrain::DoorType::metal};

                const std::vector<terrain::DoorSpawnState> door_states = {
                        terrain::DoorSpawnState::closed,
                        terrain::DoorSpawnState::stuck,
                        terrain::DoorSpawnState::warded,
                        terrain::DoorSpawnState::secret,
                        terrain::DoorSpawnState::secret_and_stuck,
                        terrain::DoorSpawnState::open,
                };

                P pos = map::g_player->m_pos.with_x_offset(1);

                for (const terrain::DoorType door_type : door_types) {
                        for (const terrain::DoorSpawnState door_state : door_states) {
                                if (door_type == terrain::DoorType::gate) {
                                        // Gates shall never be secret or warded.
                                        switch (door_state) {
                                        case terrain::DoorSpawnState::secret:
                                        case terrain::DoorSpawnState::secret_and_stuck:
                                        case terrain::DoorSpawnState::warded:
                                                continue;

                                        default:
                                                break;
                                        }
                                }

                                if (door_type == terrain::DoorType::metal) {
                                        // Metal doors shall never be stuck or warded.
                                        switch (door_state) {
                                        case terrain::DoorSpawnState::stuck:
                                        case terrain::DoorSpawnState::secret_and_stuck:
                                        case terrain::DoorSpawnState::warded:
                                                continue;

                                        default:
                                                break;
                                        }
                                }

                                auto* door =
                                        static_cast<terrain::Door*>(
                                                terrain::make(terrain::Id::door, pos));

                                if (door_type != terrain::DoorType::gate) {
                                        door->set_mimic_terrain(
                                                terrain::make(terrain::Id::wall, pos));
                                }

                                door->init_type_and_state(door_type, door_state);

                                map::update_terrain(door);

                                map::update_vision();

                                pos.x += 1;
                        }
                }

        } break;

        case GameCmd::debug_f4: {
                if (init::g_is_cheat_vision_enabled) {
                        for (const P& p : map::rect().positions()) {
                                map::g_seen.at(p) = false;

                                map::clear_player_memory_at(p);
                        }

                        init::g_is_cheat_vision_enabled = false;
                }
                else {
                        // Cheat vision was not enabled
                        init::g_is_cheat_vision_enabled = true;
                }

                actor::update_player_fov();
        } break;

        case GameCmd::debug_shift_f4: {
                // Spawn some of the lootable/usable terrain objects.

                const std::vector<terrain::Id> terrain_ids = {
                        terrain::Id::fountain,
                        terrain::Id::monolith,
                        terrain::Id::mirror,
                        terrain::Id::gong,
                        terrain::Id::chest,
                        terrain::Id::tomb,
                        terrain::Id::cocoon,
                        terrain::Id::alchemist_bench,
                        terrain::Id::cabinet,
                        terrain::Id::pillar,
                        terrain::Id::urn,
                        terrain::Id::petroglyph,
                        terrain::Id::pylon,
                };

                int dx = 1;

                for (const terrain::Id id : terrain_ids) {
                        terrain::Terrain* const terrain =
                                terrain::make(
                                        id,
                                        map::g_player->m_pos.with_x_offset(dx));

                        // Set pillars to inscribed (most interesting type).
                        if (id == terrain::Id::pillar) {
                                static_cast<terrain::Pillar*>(terrain)
                                        ->set_inscribed();
                        }

                        // Set urns to inscribed (most interesting type).
                        if (id == terrain::Id::urn) {
                                static_cast<terrain::Urn*>(terrain)
                                        ->set_inscribed();
                        }

                        map::update_terrain(terrain);

                        ++dx;
                }
        } break;

        case GameCmd::debug_f5: {
                map::g_player->incr_shock(50.0, ShockSrc::misc);
        } break;

        case GameCmd::debug_shift_f5: {
                for (actor::Actor* const actor : game_time::g_actors) {
                        if (!actor::is_player(actor) && actor::is_alive(*actor)) {
                                knockback::run(
                                        *actor,
                                        actor->m_pos.with_x_offset(-1),
                                        knockback::KnockbackSource::other,
                                        Verbose::yes);
                        }
                }
        } break;

        case GameCmd::debug_f6: {
                for (size_t i = 0; i < (size_t)item::Id::END; ++i) {
                        const item::ItemData& item_data = item::g_data[i];

                        if (!item_data.is_intr && (item_data.tile != gfx::TileId::END)) {
                                item::make_item_on_floor((item::Id)i, map::g_player->m_pos);
                        }
                }
        } break;

        case GameCmd::debug_shift_f6: {
                mapgen::put_inscribed_terrain();
        } break;

        case GameCmd::debug_f7: {
                map::g_player->m_properties.apply(prop::make(prop::Id::r_conf));

                teleport(*map::g_player);
        } break;

        case GameCmd::debug_shift_f7: {
                // Collect melee weapon stats.

                const int nr_iterations = 800;

                int tot_dmg_per_item[(size_t)item::Id::END] {};

                // Perform attacks.

                for (size_t idx = 0; idx < (size_t)item::Id::END; ++idx) {
                        const item::ItemData& d = item::g_data[idx];

                        if (!d.melee.is_melee_wpn || d.is_intr || !d.allow_spawn) {
                                continue;
                        }

                        map::g_player->m_inv.remove_item_in_slot(SlotId::wpn, true);

                        item::Item* wpn = item::make((item::Id)idx);

                        wpn->set_melee_plus(2);

                        map::g_player->m_inv.put_in_slot(SlotId::wpn, wpn, Verbose::no);

                        for (int iteration = 0; iteration < nr_iterations; ++iteration) {
                                const P& player_pos = map::g_player->m_pos;
                                const P mon_pos = player_pos.with_x_offset(1);

                                actor::Actor* const actor =
                                        actor::make(
                                                "MON_GREEN_SPIDER",
                                                mon_pos);

                                actor->become_aware_player(actor::AwareSource::other);

                                actor::restore_hp(
                                        *actor,
                                        9999,
                                        actor::AllowRestoreAboveMax::yes,
                                        Verbose::no);

                                game_time::g_allow_tick = true;

                                const int hp_before = actor->m_hp;

                                attack::melee(
                                        map::g_player,
                                        player_pos,
                                        mon_pos,
                                        *static_cast<item::Wpn*>(wpn));

                                tot_dmg_per_item[idx] += hp_before - actor->m_hp;

                                actor->m_state = ActorState::destroyed;

                                msg_log::clear();

                                io::sleep(1U);
                        }

                        game_time::erase_all_destroyed_actors();

                        map::g_player->m_inv.remove_item_in_slot(SlotId::wpn, true);
                }

                // Collect and present data.

                std::vector<std::tuple<std::string, double, double>> result;

                for (size_t idx = 0; idx < (size_t)item::Id::END; ++idx) {
                        const int dmg = tot_dmg_per_item[idx];

                        if (dmg > 0) {
                                const item::ItemData& d = item::g_data[idx];

                                const std::string name =
                                        text_format::pad_after(
                                                d.base_name.names[0],
                                                30);

                                const double avg_dmg = (double)dmg / (double)nr_iterations;

                                const double avg_dmg_per_weight_unit = avg_dmg / (double)d.weight;

                                result.push_back({name, avg_dmg, avg_dmg_per_weight_unit});
                        }
                }

                std::sort(
                        std::begin(result),
                        std::end(result),
                        [](const auto& v1, const auto& v2) {
                                // Sort by average damage.
                                return std::get<1>(v1) > std::get<1>(v2);
                        });

                std::string str = "\nAverage damage done per item id (per weight unit):";

                for (const auto& r : result) {
                        const auto [name, avg_dmg, avg_dmg_per_weight] = r;

                        str +=
                                "\n" +
                                name +
                                ": " +
                                std::to_string(avg_dmg) +
                                " (" +
                                std::to_string(avg_dmg_per_weight) +
                                ")";
                }

                TRACE << str << std::endl;
        } break;

        case GameCmd::debug_f8: {
                P p = map::g_player->m_pos;

                for (int id_idx = 0; id_idx < (int)terrain::TrapId::END; ++id_idx) {
                        if ((terrain::TrapId)id_idx == terrain::TrapId::END_MECHANICAL) {
                                continue;
                        }

                        ++p.x;

                        if (!map::g_terrain.at(p)->can_have_trap()) {
                                continue;
                        }

                        auto* const trap =
                                static_cast<terrain::Trap*>(
                                        terrain::make(terrain::Id::trap, p));

                        trap->set_mimic_terrain(terrain::make(terrain::Id::floor, p));

                        if (trap->try_init_type((terrain::TrapId)id_idx)) {
                                map::update_terrain(trap);
                        }
                        else {
                                delete trap;
                        }
                }
        } break;

        case GameCmd::debug_f9: {
                // TODO: It would be more convenient to query for a string instead, so that the
                // monster ID string could be entered directly, instead of an index number (perhaps
                // with partial matches allowed).

                std::string msg = "Listing all monsters (IDX    ID):";

                std::vector<std::string> mon_ids;
                mon_ids.reserve(actor::g_data.size());

                for (const auto& it : actor::g_data) {
                        mon_ids.push_back(it.first);
                }

                std::sort(std::begin(mon_ids), std::end(mon_ids));

                int default_idx = 0;

                for (size_t i = 0; i < mon_ids.size(); ++i) {
                        msg +=
                                "\n" +
                                std::to_string(i) +
                                "    " +
                                mon_ids[i];

                        if (mon_ids[i] == "MON_ZOMBIE") {
                                // Use this as default value for the query.
                                default_idx = (int)i;
                        }
                }

                TRACE << msg << std::endl;

                std::string query_str = "Summon monster id";

                query::QueryNumberConfig query_config;

                query_config.allowed_range = {1, (int)actor::g_data.size() - 1};
                query_config.default_value = default_idx;
                query_config.cancel_returns_default = false;

                const int idx_to_spawn = query::number(query_config, query_str);

                if (idx_to_spawn == -1) {
                        return;
                }

                states::draw();
                io::update_screen();

                query_str = "How many?";

                query_config.allowed_range = {1, 999};
                query_config.default_value = 1;
                query_config.cancel_returns_default = false;

                const int nr_to_spawn =
                        query::number(
                                query_config,
                                query_str);

                if (nr_to_spawn == -1) {
                        return;
                }

                const std::string mon_id = mon_ids[idx_to_spawn];

                actor::MonSpawnResult spawned =
                        actor::spawn(
                                map::g_player->m_pos.with_x_offset(2),
                                {(size_t)nr_to_spawn, mon_id});

                for (actor::Actor* const actor : spawned.monsters) {
                        actor::spawn_starting_allies(*actor);
                }
        } break;

        case GameCmd::debug_f10: {
                for (item::Item* const item : map::g_player->m_inv.m_backpack) {
                        item->identify(Verbose::yes);
                }
        } break;

#endif  // NDEBUG

        }  // switch

        // Drag-to-look is a transient mode: engaging any action ends it,
        // and the camera snaps back to the player. The exception is
        // describing the pinned cell ([ describe ] / the view key), which
        // keeps the pin, so that closing the description returns to the
        // cell that was being looked at.
        //
        // NOTE: This runs AFTER the command - the close/jam and kick
        // handlers read the pin (see close::player_try_close_or_jam and
        // handle_kick_at_look_pin_command), so it must still be there
        // while they run. Unmapped input never gets this far at all (it
        // returns at the top of this function), so a stray tap leaves the
        // look alone.
        if (!viewport::is_pan_active()) {
                // Not looking around - the camera is following the player
                // as usual, and must NOT be forced to center (that would
                // re-center the view on every single command)
                return;
        }

        switch (cmd) {
        case GameCmd::look:
        case GameCmd::none:
        case GameCmd::undefined:
                break;

        case GameCmd::throw_item:
        case GameCmd::toggle_throw:
                // Throwing while looking around runs THROUGH the pin: the
                // item selection screen must be able to return to it
                // (cancelled), and the throw marker takes over the pinned
                // cell (see MarkerState::on_start) instead of starting on
                // the player. The marker ends the pan when it is popped.
                break;

        case GameCmd::fire:
        case GameCmd::toggle_aim:
                // Aiming while looking around likewise takes over the
                // pinned cell rather than auto-targeting somewhere else -
                // the reticle appears exactly where the player was already
                // looking. NOTE: The aim marker is only PUSHED here, it
                // starts later (see states::start), so the pan has to
                // survive this block for it to find the pin. If the
                // command did not open a marker at all (no ammo loaded,
                // firing would be lethal, ...), the look ends as usual.
                if (states::contains_state(StateId::marker)) {
                        break;
                }

                viewport::end_pan_and_center_on_player();
                break;

        default:
                viewport::end_pan_and_center_on_player();
                break;
        }

}  // handle

char fire_key()
{
        return 'f';
}

char throw_key()
{
        return 't';
}

char view_key()
{
        return 'v';
}

char get_key()
{
        return 'g';
}

char unload_key()
{
        return 'u';
}

char close_key()
{
        return 'c';
}

char disarm_key()
{
        return 'p';
}

char swap_weapon_key()
{
        return 'z';
}

char reload_key()
{
        return 'r';
}

int kick_at_look_pin_key()
{
        return SDLK_F13;
}

int toggle_aim_key()
{
        return SDLK_F14;
}

int toggle_throw_key()
{
        return SDLK_F15;
}

std::string ranged_wpn_unfireable_reason(const item::Wpn& wpn)
{
        if (!map::g_player->m_properties.allow_attack_ranged(Verbose::no)) {
                // NOTE: Burning is the only thing that stops the player
                // from shooting (see Burning::allow_attack_ranged) - if
                // some other property ever does, its reason belongs here
                // as well
                return common_text::g_burning_prevent_attack_ranged;
        }

        const item::ItemData& d = wpn.data();

        if ((wpn.m_ammo_loaded <= 0) && !d.ranged.has_infinite_ammo) {
                return "There is no ammo loaded.";
        }

        int mi_go_wpn_hp_drain = 0;

        switch (d.id) {
        case item::Id::electric_gun:
                mi_go_wpn_hp_drain = g_electric_gun_hp_drained;
                break;

        case item::Id::morphic_blaster:
                mi_go_wpn_hp_drain = g_morphic_blaster_hp_drained;
                break;

        default:
                break;
        }

        if ((mi_go_wpn_hp_drain > 0) &&
            !allow_player_fire_mi_go_weapon(mi_go_wpn_hp_drain)) {
                return "Firing the gun now would destroy me.";
        }

        return {};
}

}  // namespace game_commands

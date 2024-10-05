// =============================================================================
// Copyright 2011-2024 Martin Törnqvist <m.tornq@gmail.com>
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
#include "actor.hpp"
#include "actor_data.hpp"
#include "actor_factory.hpp"
#include "actor_move.hpp"
#include "actor_player_state.hpp"
#include "actor_see.hpp"
#include "array2.hpp"
#include "audio_data.hpp"
#include "auto_melee.hpp"
#include "bash.hpp"
#include "character_descr.hpp"
#include "close.hpp"
#include "colors.hpp"
#include "common_text.hpp"
#include "config.hpp"
#include "debug.hpp"
#include "direction.hpp"
#include "disarm.hpp"
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

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
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
                        {'n', 'y'},
                        popup::MenuModeShowCancelHint::no,
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
                // Prolonged Life trait and enough SP instead.
                const bool has_prolonged_life = player_bon::has_trait(Trait::prolonged_life);

                const int hp = map::g_player->m_hp;
                const int sp = map::g_player->m_sp;

                const bool has_enough_hp_and_sp = ((hp + sp - 1) > hp_drained);

                return (has_prolonged_life && has_enough_hp_and_sp);
        }
}

static void handle_fire_command_melee(item::Wpn& wpn)
{
        states::push(std::make_unique<AimingMeleeWpn>(map::g_player->m_pos, wpn));
}

static void handle_fire_command_firearm(item::Wpn& wpn)
{
        if (!map::g_player->m_properties.allow_attack_ranged(Verbose::yes)) {
                return;
        }

        const item::ItemData& item_data = wpn.data();

        if ((wpn.m_ammo_loaded <= 0) &&
            !item_data.ranged.has_infinite_ammo) {
                // Not enough ammo loaded - auto reload?
                if (config::is_ranged_wpn_auto_reload()) {
                        reload::try_reload(*map::g_player, &wpn);
                }
                else {
                        msg_log::add("There is no ammo loaded.");
                }

                return;
        }

        bool is_mi_go_wpn = false;
        int mi_go_wpn_hp_drain = 0;

        switch (item_data.id) {
        case item::Id::electric_gun:
                is_mi_go_wpn = true;
                mi_go_wpn_hp_drain = g_electric_gun_hp_drained;
                break;

        case item::Id::morphic_blaster:
                is_mi_go_wpn = true;
                mi_go_wpn_hp_drain = g_morphic_blaster_hp_drained;
                break;

        default:
                break;
        }

        if (is_mi_go_wpn && !allow_player_fire_mi_go_weapon(mi_go_wpn_hp_drain)) {
                msg_log::add("Firing the gun now would destroy me.");

                return;
        }

        // OK - we can fire the gun.
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

static void handle_game_menu_command()
{
        const auto choices = std::vector<std::string> {
                "(T)ome of Wisdom",
                "(O)ptions",
                "(Q)uit",
        };

        int choice = 0;

        popup::Popup(popup::AddToMsgHistory::no)
                .setup_menu_mode(
                        choices,
                        {'t', 'o', 'q'},
                        popup::MenuModeShowCancelHint::yes,
                        &choice)
                .run();

        if (choice == 0) {
                // Manual
                states::push(std::make_unique<BrowseManual>());
        }
        else if (choice == 1) {
                // Options
                states::push(std::make_unique<OptionsState>());
        }
        else if (choice == 2) {
                // Quit
                query_quit();
        }
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

        case '=':
                return GameCmd::options;

        case 'r':
                return GameCmd::reload;

        case 'k':
        case 'w':
                return GameCmd::kick;

        case 'c':
                return GameCmd::close;

        case 'u':
        case 'G':
                return GameCmd::unload;

        case 'f':
                return GameCmd::fire;

        case 'g':
        case ',':
                return GameCmd::get;

        case 'i':
                return GameCmd::inventory;

        case 'a':
                return GameCmd::apply_item;

        case 'd':
                return GameCmd::drop_item;

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
                return GameCmd::auto_melee;

        case 'x':
                return GameCmd::cast_spell;

        case 'C':
        case '@':
                return GameCmd::char_descr;

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
                return GameCmd::debug_f7;

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
        if (cmd != GameCmd::none) {
                msg_log::clear();
        }

        switch (cmd) {
        case GameCmd::undefined: {
                msg_log::add(
                        "Press [?] for help.",
                        colors::light_white(),
                        MsgInterruptPlayer::no,
                        MorePromptOnMsg::no,
                        CopyToMsgHistory::no);
        } break;

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

        case GameCmd::options: {
                states::push(std::make_unique<OptionsState>());
        } break;

        case GameCmd::reload: {
                auto* const wpn =
                        map::g_player->m_inv.item_in_slot(SlotId::wpn);

                reload::try_reload(*map::g_player, wpn);
        } break;

        case GameCmd::kick: {
                bash::run();
        } break;

        case GameCmd::close: {
                close::player_try_close_or_jam();
        } break;

        case GameCmd::unload: {
                item_pickup::try_unload_or_pick();
        } break;

        case GameCmd::fire: {
                handle_fire_command();
        } break;

        case GameCmd::get: {
                item_pickup::try_pick();
        } break;

        case GameCmd::inventory: {
                states::push(std::make_unique<BrowseInv>());
        } break;

        case GameCmd::apply_item: {
                states::push(std::make_unique<Apply>());
        } break;

        case GameCmd::drop_item: {
                states::push(std::make_unique<Drop>());
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

        case GameCmd::throw_item: {
                const item::Item* explosive = actor::player_state::g_active_explosive.get();

                if (explosive) {
                        states::push(
                                std::make_unique<ThrowingExplosive>(
                                        map::g_player->m_pos, *explosive));
                }
                else {
                        // Not holding explosive - run throwing attack instead
                        const bool is_allowed =
                                map::g_player->m_properties
                                        .allow_attack_ranged(Verbose::yes);

                        if (is_allowed) {
                                states::push(std::make_unique<SelectThrow>());
                        }
                }
        } break;

        case GameCmd::use_medical_bag: {
                handle_activate_item_shortcut_command(item::Id::medical_bag);
        } break;

        case GameCmd::toggle_lantern: {
                handle_activate_item_shortcut_command(item::Id::lantern);
        } break;

        case GameCmd::look: {
                states::push(std::make_unique<Viewing>(map::g_player->m_pos));
        } break;

        case GameCmd::auto_melee: {
                auto_melee::run();
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
                // TODO: It would be more convenient to query for a string
                // instead, so that the monster ID string could be entered
                // directly, instead of an index number (perhaps with partial
                // matches allowed).

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

                map::g_player->m_properties.apply(
                        prop::make(
                                (prop::Id)id_to_apply));

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
                knockback::run(
                        *map::g_player,
                        map::g_player->m_pos.with_y_offset(1),
                        knockback::KnockbackSource::other,
                        Verbose::no);
        } break;

        case GameCmd::debug_shift_f6: {
                mapgen::put_inscribed_terrain();
        } break;

        case GameCmd::debug_f6: {
                for (size_t i = 0; i < (size_t)item::Id::END; ++i) {
                        const item::ItemData& item_data = item::g_data[i];

                        if (!item_data.is_intr && (item_data.tile != gfx::TileId::END)) {
                                item::make_item_on_floor((item::Id)i, map::g_player->m_pos);
                        }
                }
        } break;

        case GameCmd::debug_f7: {
                map::g_player->m_properties.apply(prop::make(prop::Id::r_conf));

                teleport(*map::g_player);
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
                // TODO: It would be more convenient to query for a string
                // instead, so that the monster ID string could be entered
                // directly, instead of an index number (perhaps with partial
                // matches allowed).

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
                                {(size_t)nr_to_spawn, mon_id},
                                map::rect());

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

}  // handle

}  // namespace game_commands

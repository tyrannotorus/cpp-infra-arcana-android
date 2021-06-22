// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "game_commands.hpp"

#include <cstddef>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "SDL_keycode.h"
#include "actor_data.hpp"
#include "actor_factory.hpp"
#include "actor_move.hpp"
#include "actor_player.hpp"
#include "actor_see.hpp"
#include "array2.hpp"
#include "audio_data.hpp"
#include "character_descr.hpp"
#include "close.hpp"
#include "colors.hpp"
#include "common_text.hpp"
#include "config.hpp"
#include "debug.hpp"
#include "direction.hpp"
#include "disarm.hpp"
#include "game.hpp"
#include "game_time.hpp"
#include "global.hpp"
#include "init.hpp"
#include "inventory.hpp"
#include "inventory_handling.hpp"
#include "io.hpp"
#include "item.hpp"
#include "item_data.hpp"
#include "item_factory.hpp"
#include "manual.hpp"
#include "map.hpp"
#include "map_travel.hpp"
#include "marker.hpp"
#include "minimap.hpp"
#include "msg_log.hpp"
#include "panel.hpp"
#include "pickup.hpp"
#include "player_bon.hpp"
#include "player_spells.hpp"
#include "popup.hpp"
#include "property.hpp"
#include "property_data.hpp"
#include "property_factory.hpp"
#include "property_handler.hpp"
#include "query.hpp"
#include "reload.hpp"
#include "saving.hpp"
#include "sound.hpp"
#include "spells.hpp"
#include "state.hpp"
#include "teleport.hpp"
#include "wham.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static void query_quit()
{
        int choice = 0;

        popup::Popup(popup::AddToMsgHistory::no)
                .set_title("Quit the current game?")
                .set_msg("Save and highscore are not kept.")
                .set_menu(
                        {"(Y)es", "(N)o"},
                        {'y', 'n'},
                        &choice)
                .run();

        if (choice == 0)
        {
                // Choosing to quit the game deletes the save
                saving::erase_save();

                states::pop();

                init::cleanup_session();
        }
}

static bool allow_player_fire_mi_go_gun()
{
        const bool has_enough_hp =
                (map::g_player->m_hp > g_mi_go_gun_hp_drained);

        if (has_enough_hp)
        {
                return true;
        }
        else
        {
                // Not enough HP - allow firing the gun if player has the
                // Prolonged Life trait and enough SP instead
                const bool has_prolonged_life =
                        player_bon::has_trait(Trait::prolonged_life);

                const int hp = map::g_player->m_hp;
                const int sp = map::g_player->m_sp;

                const bool has_enough_hp_and_sp =
                        ((hp + sp - 1) > g_mi_go_gun_hp_drained);

                return (has_prolonged_life && has_enough_hp_and_sp);
        }
}

static void handle_fire_command()
{
        if (!map::g_player->m_properties.allow_attack_ranged(Verbose::yes))
        {
                return;
        }

        auto* const item = map::g_player->m_inv.item_in_slot(SlotId::wpn);

        if (!item)
        {
                msg_log::add("I am not wielding a weapon.");

                return;
        }

        const auto& item_data = item->data();

        if (!item_data.ranged.is_ranged_wpn)
        {
                msg_log::add("I am not wielding a firearm.");

                return;
        }

        auto* wpn = static_cast<item::Wpn*>(item);

        if ((wpn->m_ammo_loaded <= 0) &&
            !item_data.ranged.has_infinite_ammo)
        {
                // Not enough ammo loaded - auto reload?
                if (config::is_ranged_wpn_auto_reload())
                {
                        reload::try_reload(*map::g_player, item);
                }
                else
                {
                        msg_log::add("There is no ammo loaded.");
                }

                return;
        }

        if ((wpn->data().id == item::Id::mi_go_gun) &&
            !allow_player_fire_mi_go_gun())
        {
                msg_log::add("Firing the gun now would destroy me.");

                return;
        }

        // OK - we can fire the gun.
        states::push(
                std::make_unique<Aiming>(
                        map::g_player->m_pos,
                        *wpn));
}

static GameCmd to_cmd_default(const InputData& input)
{
        // When running on windows, with numlock enabled, each numpad key press
        // yields two input events - one for the keypad key, and one as a number
        // key. This seems to be due to a bug in SDL (or maybe it's just how it
        // works on windows). As a workaround, we skip numerical keys here.
        if (input.key >= '0' && input.key <= '9')
        {
                return GameCmd::none;
        }

        switch (input.key)
        {
        case SDLK_KP_6:
                if (input.is_shift_held)
                {
                        return GameCmd::auto_move_right;
                }
                else
                {
                        return GameCmd::right;
                }

        case SDLK_KP_4:
                if (input.is_shift_held)
                {
                        return GameCmd::auto_move_left;
                }
                else
                {
                        return GameCmd::left;
                }

        case SDLK_KP_2:
                if (input.is_shift_held)
                {
                        return GameCmd::auto_move_down;
                }
                else
                {
                        return GameCmd::down;
                }

        case SDLK_KP_8:
                if (input.is_shift_held)
                {
                        return GameCmd::auto_move_up;
                }
                else
                {
                        return GameCmd::up;
                }

        case SDLK_KP_9:
                if (input.is_shift_held)
                {
                        return GameCmd::auto_move_up_right;
                }
                else
                {
                        return GameCmd::up_right;
                }

        case SDLK_KP_3:
                if (input.is_shift_held)
                {
                        return GameCmd::auto_move_down_right;
                }
                else
                {
                        return GameCmd::down_right;
                }

        case SDLK_KP_1:
                if (input.is_shift_held)
                {
                        return GameCmd::auto_move_down_left;
                }
                else
                {
                        return GameCmd::down_left;
                }

        case SDLK_KP_7:
                if (input.is_shift_held)
                {
                        return GameCmd::auto_move_up_left;
                }
                else
                {
                        return GameCmd::up_left;
                }

        case SDLK_KP_5:
        case '.':
                return GameCmd::wait;

        case SDLK_RIGHT:
                if (input.is_shift_held)
                {
                        return GameCmd::up_right;
                }
                else if (input.is_ctrl_held)
                {
                        return GameCmd::down_right;
                }
                else
                {
                        return GameCmd::right;
                }

        case SDLK_LEFT:
                if (input.is_shift_held)
                {
                        return GameCmd::up_left;
                }
                else if (input.is_ctrl_held)
                {
                        return GameCmd::down_left;
                }
                else
                {
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
                return GameCmd::debug_f2;

        case SDLK_F3:
                return GameCmd::debug_f3;

        case SDLK_F4:
                return GameCmd::debug_f4;

        case SDLK_F5:
                return GameCmd::debug_f5;

        case SDLK_F6:
                return GameCmd::debug_f6;

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

static GameCmd to_cmd_vi(const InputData& input)
{
        // Overriden keys for vi-mode
        switch (input.key)
        {
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
GameCmd to_cmd(const InputData& input)
{
        switch (config::input_mode())
        {
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
        if (cmd != GameCmd::none)
        {
                msg_log::clear();
        }

        switch (cmd)
        {
        case GameCmd::undefined:
        {
                msg_log::add(
                        "Press [?] for help.",
                        colors::light_white(),
                        MsgInterruptPlayer::no,
                        MorePromptOnMsg::no,
                        CopyToMsgHistory::no);
        }
        break;

        case GameCmd::none:
                break;

        case GameCmd::right:
        {
                actor::move(*map::g_player, Dir::right);
        }
        break;

        case GameCmd::down:
        {
                actor::move(*map::g_player, Dir::down);
        }
        break;

        case GameCmd::left:
        {
                actor::move(*map::g_player, Dir::left);
        }
        break;

        case GameCmd::up:
        {
                actor::move(*map::g_player, Dir::up);
        }
        break;

        case GameCmd::up_right:
        {
                actor::move(*map::g_player, Dir::up_right);
        }
        break;

        case GameCmd::down_right:
        {
                actor::move(*map::g_player, Dir::down_right);
        }
        break;

        case GameCmd::down_left:
        {
                actor::move(*map::g_player, Dir::down_left);
        }
        break;

        case GameCmd::up_left:
        {
                actor::move(*map::g_player, Dir::up_left);
        }
        break;

        case GameCmd::wait:
        {
                if (player_bon::has_trait(Trait::steady_aimer))
                {
                        Prop* aiming = new PropAiming();

                        aiming->set_duration(1);

                        map::g_player->m_properties.apply(aiming);
                }

                actor::move(*map::g_player, Dir::center);
        }
        break;

        case GameCmd::wait_long:
        {
                bool is_allowed = true;
                std::string prevent_msg;
                if (actor::is_player_seeing_burning_terrain())
                {
                        is_allowed = false;
                        prevent_msg = common_text::g_fire_prevent_cmd;
                }
                else if (!actor::seen_foes(*map::g_player).empty())
                {
                        is_allowed = false;
                        prevent_msg = common_text::g_mon_prevent_cmd;
                }
                else if (map::g_player->shock_tot() >= 100)
                {
                        is_allowed = false;
                        prevent_msg = common_text::g_shock_prevent_cmd;
                }
                else if (map::g_player->m_properties.has(PropId::infected))
                {
                        is_allowed = false;
                        prevent_msg = "Not while infected.";
                }

                if (is_allowed)
                {
                        // NOTE: We should not print any "wait" message here,
                        // since it would look weird in some cases - e.g. when
                        // the waiting is immediately interrupted by a message
                        // from rearranging pistol magazines.

                        // NOTE: A 'long wait' merely performs "move" into the
                        // center position a number of turns (i.e. the same as
                        // pressing 'wait')
                        const int turns_to_apply = 5;

                        map::g_player->m_wait_turns_left = turns_to_apply - 1;

                        game_time::tick();
                }
                else
                {
                        // Not allowed to long-wait
                        msg_log::add(
                                prevent_msg,
                                colors::text(),
                                MsgInterruptPlayer::no,
                                MorePromptOnMsg::no,
                                CopyToMsgHistory::no);
                }
        }
        break;

        case GameCmd::manual:
        {
                states::push(std::make_unique<BrowseManual>());
        }
        break;

        case GameCmd::options:
        {
                states::push(std::make_unique<ConfigState>());
        }
        break;

        case GameCmd::reload:
        {
                auto* const wpn =
                        map::g_player->m_inv.item_in_slot(SlotId::wpn);

                reload::try_reload(*map::g_player, wpn);
        }
        break;

        case GameCmd::kick:
        {
                wham::run();
        }
        break;

        case GameCmd::close:
        {
                close::player_try_close_or_jam();
        }
        break;

        case GameCmd::unload:
        {
                item_pickup::try_unload_or_pick();
        }
        break;

        case GameCmd::fire:
        {
                handle_fire_command();
        }
        break;

        case GameCmd::get:
        {
                item_pickup::try_pick();
        }
        break;

        case GameCmd::inventory:
        {
                states::push(std::make_unique<BrowseInv>());
        }
        break;

        case GameCmd::apply_item:
        {
                states::push(std::make_unique<Apply>());
        }
        break;

        case GameCmd::drop_item:
        {
                states::push(std::make_unique<Drop>());
        }
        break;

        case GameCmd::swap_weapon:
        {
                auto* const wielded = map::g_player->m_inv.item_in_slot(SlotId::wpn);

                auto* const alt = map::g_player->m_inv.item_in_slot(SlotId::wpn_alt);

                if (wielded || alt)
                {
                        const std::string alt_name_a =
                                alt
                                ? alt->name(ItemRefType::a)
                                : "";

                        // War veteran swaps instantly
                        const bool is_instant = player_bon::bg() == Bg::war_vet;

                        const std::string swift_str =
                                is_instant
                                ? "swiftly "
                                : "";

                        if (wielded)
                        {
                                if (alt)
                                {
                                        msg_log::add(
                                                "I " +
                                                swift_str +
                                                "swap to " +
                                                alt_name_a +
                                                ".");
                                }
                                else
                                {
                                        // No current alt weapon
                                        const std::string name =
                                                wielded->name(ItemRefType::plain);

                                        msg_log::add(
                                                "I " +
                                                swift_str +
                                                "put away my " +
                                                name +
                                                ".");
                                }
                        }
                        else
                        {
                                // No current wielded item
                                msg_log::add("I " + swift_str + "wield " + alt_name_a + ".");
                        }

                        map::g_player->m_inv.swap_wielded_and_prepared();

                        if (!is_instant)
                        {
                                game_time::tick();
                        }
                }
                else
                {
                        // No wielded weapon and no alt weapon
                        msg_log::add("I have neither a wielded nor a prepared weapon.");
                }
        }
        break;

        case GameCmd::auto_move_right:
        case GameCmd::auto_move_down:
        case GameCmd::auto_move_left:
        case GameCmd::auto_move_up:
        case GameCmd::auto_move_up_right:
        case GameCmd::auto_move_down_right:
        case GameCmd::auto_move_down_left:
        case GameCmd::auto_move_up_left:
        {
                bool is_allowed = true;
                std::string prevent_msg;
                if (actor::is_player_seeing_burning_terrain())
                {
                        is_allowed = false;
                        prevent_msg = common_text::g_fire_prevent_cmd;
                }
                else if (!actor::seen_foes(*map::g_player).empty())
                {
                        is_allowed = false;
                        prevent_msg = common_text::g_mon_prevent_cmd;
                }
                else if (!map::g_player->m_properties.allow_see())
                {
                        is_allowed = false;
                        prevent_msg = "Not while blind.";
                }
                else if (map::g_player->m_properties.has(PropId::poisoned))
                {
                        is_allowed = false;
                        prevent_msg = "Not while poisoned.";
                }
                else if (map::g_player->m_properties.has(PropId::confused))
                {
                        is_allowed = false;
                        prevent_msg = "Not while confused.";
                }
                else if (map::g_player->m_properties.has(PropId::infected))
                {
                        is_allowed = false;
                        prevent_msg = "Not while infected.";
                }

                if (is_allowed)
                {
                        auto dir = (Dir)0;

                        switch (cmd)
                        {
                        case GameCmd::auto_move_right:
                                dir = Dir::right;
                                break;

                        case GameCmd::auto_move_down:
                                dir = Dir::down;
                                break;

                        case GameCmd::auto_move_left:
                                dir = Dir::left;
                                break;

                        case GameCmd::auto_move_up:
                                dir = Dir::up;
                                break;

                        case GameCmd::auto_move_up_right:
                                dir = Dir::up_right;
                                break;

                        case GameCmd::auto_move_down_right:
                                dir = Dir::down_right;
                                break;

                        case GameCmd::auto_move_down_left:
                                dir = Dir::down_left;
                                break;

                        case GameCmd::auto_move_up_left:
                                dir = Dir::up_left;
                                break;

                        default:
                                ASSERT(false);
                                dir = Dir::right;
                                break;
                        }

                        map::g_player->set_auto_move(dir);
                }
                else
                {
                        // Not allowed to use auto-move
                        msg_log::add(
                                prevent_msg,
                                colors::text(),
                                MsgInterruptPlayer::no,
                                MorePromptOnMsg::no,
                                CopyToMsgHistory::no);
                }
        }
        break;

        case GameCmd::throw_item:
        {
                const auto* explosive = map::g_player->m_active_explosive;

                if (explosive)
                {
                        states::push(
                                std::make_unique<ThrowingExplosive>(
                                        map::g_player->m_pos, *explosive));
                }
                else
                {
                        // Not holding explosive - run throwing attack instead
                        const bool is_allowed =
                                map::g_player->m_properties
                                        .allow_attack_ranged(Verbose::yes);

                        if (is_allowed)
                        {
                                states::push(std::make_unique<SelectThrow>());
                        }
                }
        }
        break;

        case GameCmd::look:
        {
                if (map::g_player->m_properties.allow_see())
                {
                        states::push(
                                std::make_unique<Viewing>(
                                        map::g_player->m_pos));
                }
                else
                {
                        // Cannot see
                        msg_log::add("I cannot see.");
                }
        }
        break;

        case GameCmd::auto_melee:
        {
                map::g_player->auto_melee();
        }
        break;

        case GameCmd::cast_spell:
        {
                states::push(std::make_unique<BrowseSpell>());
        }
        break;

        case GameCmd::char_descr:
        {
                states::push(std::make_unique<CharacterDescr>());
        }
        break;

        case GameCmd::minimap:
        {
                states::push(std::make_unique<ViewMinimap>());
        }
        break;

        case GameCmd::msg_history:
        {
                states::push(std::make_unique<MsgHistoryState>());
        }
        break;

        case GameCmd::make_noise:
        {
                if (player_bon::bg() == Bg::ghoul)
                {
                        msg_log::add("I let out a chilling howl.");
                }
                else
                {
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
        }
        break;

        case GameCmd::disarm:
        {
                disarm::player_disarm();
        }
        break;

        case GameCmd::game_menu:
        {
                const auto choices = std::vector<std::string> {
                        "(T)ome of Wisdom",
                        "(O)ptions",
                        "(Q)uit",
                        "(C)ancel",
                };

                int choice = 0;

                popup::Popup(popup::AddToMsgHistory::no)
                        .set_menu(
                                choices,
                                {'t', 'o', 'q', 'c'},
                                &choice)
                        .run();

                if (choice == 0)
                {
                        // Manual
                        states::push(std::make_unique<BrowseManual>());
                }
                else if (choice == 1)
                {
                        // Options
                        states::push(std::make_unique<ConfigState>());
                }
                else if ((choice == 2) || (choice == -1))
                {
                        // Quit
                        query_quit();
                }
        }
        break;

        case GameCmd::quit:
        {
                query_quit();
        }
        break;

        // Some cheat commands enabled in debug builds
#ifndef NDEBUG
        case GameCmd::debug_f2:
        {
                map::g_player->m_properties.end_prop(PropId::swimming);

                map_travel::go_to_nxt();
        }
        break;

        case GameCmd::debug_f3:
        {
                game::incr_player_xp(100, Verbose::no);
        }
        break;

        case GameCmd::debug_f4:
        {
                if (init::g_is_cheat_vision_enabled)
                {
                        const size_t nr_positions = map::nr_positions();
                        for (size_t i = 0; i < nr_positions; ++i)
                        {
                                map::g_seen.at(i) = false;
                                map::g_explored.at(i) = false;
                        }

                        init::g_is_cheat_vision_enabled = false;
                }
                else
                {
                        // Cheat vision was not enabled
                        init::g_is_cheat_vision_enabled = true;
                }

                map::g_player->update_fov();
        }
        break;

        case GameCmd::debug_f5:
        {
                map::g_player->incr_shock(50, ShockSrc::misc);
        }
        break;

        case GameCmd::debug_f6:
        {
                item::make_item_on_floor(
                        item::Id::gas_mask,
                        map::g_player->m_pos);

                item::make_item_on_floor(
                        item::Id::incinerator,
                        map::g_player->m_pos);

                item::make_item_on_floor(
                        item::Id::mi_go_gun,
                        map::g_player->m_pos);

                item::make_item_on_floor(
                        item::Id::machine_gun,
                        map::g_player->m_pos);

                for (size_t i = 0; i < (size_t)item::Id::END; ++i)
                {
                        const auto& item_data = item::g_data[i];

                        if ((item_data.value != item::Value::normal) &&
                            !item_data.is_intr)
                        {
                                item::make_item_on_floor(
                                        (item::Id)i,
                                        map::g_player->m_pos);
                        }
                }
        }
        break;

        case GameCmd::debug_f7:
        {
                map::g_player->m_properties.apply(
                        property_factory::make(
                                PropId::r_conf));

                teleport(*map::g_player);
        }
        break;

        case GameCmd::debug_f8:
        {
                auto* const prop = property_factory::make(PropId::poisoned);
                prop->set_duration(20);
                map::g_player->m_properties.apply(prop);
        }
        break;

        case GameCmd::debug_f9:
        {
                const std::string query_str = "Summon monster id:";

                io::draw_text(
                        query_str,
                        Panel::screen,
                        {0, 0},
                        colors::yellow());

                const int idx =
                        query::number(
                                {(int)query_str.size(), 0},
                                colors::light_white(),
                                {1, (int)actor::Id::END - 1},
                                (int)actor::Id::zombie,
                                false);

                if (idx != -1)
                {
                        const auto mon_id = (actor::Id)idx;

                        actor::spawn(
                                map::g_player->m_pos,
                                {mon_id},
                                map::rect());
                }
        }
        break;

        case GameCmd::debug_f10:
        {
                std::string msg = "Listing all monsters (ID#  Name):";

                for (const auto& d : actor::g_data)
                {
                        msg +=
                                "\n" +
                                std::to_string((size_t)d.id) +
                                "  " +
                                d.name_a;
                }

                TRACE << msg << std::endl;
        }

#endif  // NDEBUG

        }  // switch

}  // handle

}  // namespace game_commands

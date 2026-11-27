// =============================================================================
// Copyright Werewolf Camp
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "run_mode.hpp"

#include <string>

#include "actor.hpp"
#include "actor_player_state.hpp"
#include "actor_see.hpp"
#include "colors.hpp"
#include "common_text.hpp"
#include "direction.hpp"
#include "map.hpp"
#include "msg_log.hpp"
#include "property_data.hpp"
#include "property_handler.hpp"
#include "text_format.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static void print_notice(const std::string& msg)
{
        msg_log::add(
                msg,
                colors::text(),
                MsgInterruptPlayer::no,
                MorePromptOnMsg::no,
                CopyToMsgHistory::no);
}

static void stop_running(const std::string& reason)
{
        actor::player_state::g_is_run_mode = false;

        print_notice(
                "I stop running - " +
                text_format::first_to_lower(reason));
}

// -----------------------------------------------------------------------------
// run_mode
// -----------------------------------------------------------------------------
namespace run_mode
{
bool is_running()
{
        return actor::player_state::g_is_run_mode;
}

void toggle()
{
        if (is_running()) {
                actor::player_state::g_is_run_mode = false;

                return;
        }

        const std::string reason = refusal_reason();

        if (!reason.empty()) {
                print_notice(reason);

                return;
        }

        actor::player_state::g_is_run_mode = true;
}

std::string refusal_reason()
{
        const actor::Actor& player = *map::g_player;

        if (actor::is_player_seeing_burning_terrain()) {
                return common_text::g_fire_prevent_cmd;
        }

        if (!actor::seen_foes(player).empty()) {
                return common_text::g_mon_prevent_cmd;
        }

        if (!player.m_properties.allow_see()) {
                return "Not while blind.";
        }

        if (player.m_properties.has(prop::Id::poisoned)) {
                return "Not while poisoned.";
        }

        if (player.m_properties.has(prop::Id::confused)) {
                return "Not while confused.";
        }

        if (player.m_properties.has(prop::Id::infected)) {
                return "Not while infected.";
        }

        return "";
}

bool try_run(const Dir dir)
{
        if (!is_running()) {
                return false;
        }

        const std::string reason = refusal_reason();

        if (!reason.empty()) {
                stop_running(reason);

                return false;
        }

        map::g_player->set_auto_move(dir);

        return true;
}

void on_player_turn()
{
        if (!is_running()) {
                return;
        }

        const std::string reason = refusal_reason();

        if (!reason.empty()) {
                stop_running(reason);
        }
}

}  // namespace run_mode

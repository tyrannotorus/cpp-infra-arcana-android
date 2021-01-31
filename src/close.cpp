// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "close.hpp"

#include "actor.hpp"
#include "actor_player.hpp"
#include "common_text.hpp"
#include "io.hpp"
#include "map.hpp"
#include "msg_log.hpp"
#include "property_handler.hpp"
#include "query.hpp"
#include "terrain_door.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static void player_try_close_or_jam_terrain(terrain::Terrain* const terrain)
{
        if (terrain->id() != terrain::Id::door)
        {
                const bool player_can_see =
                        map::g_player->m_properties.allow_see();

                if (player_can_see)
                {
                        msg_log::add(
                                "I see nothing there to close or jam shut.");
                }
                else
                {
                        // Player cannot see
                        msg_log::add(
                                "I find nothing there to close or jam shut.");
                }

                return;
        }

        // This is a door

        auto* const door = static_cast<terrain::Door*>(terrain);

        if (door->is_open())
        {
                // Door is open, try to close it
                door->actor_try_close(*map::g_player);
        }
        else
        {
                // Door is closed - try to jam it
                const bool has_spike =
                        map::g_player->m_inv.has_item_in_backpack(
                                item::Id::iron_spike);

                if (has_spike)
                {
                        const bool did_spike_door =
                                door->actor_try_jam(*map::g_player);

                        if (did_spike_door)
                        {
                                map::g_player->m_inv.decr_item_type_in_backpack(
                                        item::Id::iron_spike);

                                const int nr_spikes_left =
                                        map::g_player->m_inv
                                                .item_stack_size_in_backpack(
                                                        item::Id::iron_spike);

                                if (nr_spikes_left == 0)
                                {
                                        msg_log::add(
                                                "I have no iron spikes left.");
                                }
                                else
                                {
                                        // Has spikes left
                                        msg_log::add(
                                                "I have " +
                                                std::to_string(nr_spikes_left) +
                                                " iron spikes left.");
                                }
                        }
                }
                else
                {
                        // Has no spikes to jam with
                        msg_log::add("I have nothing to jam the door with.");
                }
        }
}  // player_try_close_or_jam_terrain

// -----------------------------------------------------------------------------
// close
// -----------------------------------------------------------------------------
namespace close
{
void player_try_close_or_jam()
{
        msg_log::clear();

        msg_log::add(
                "Which direction? " + common_text::g_cancel_hint,
                colors::light_white(),
                MsgInterruptPlayer::no,
                MorePromptOnMsg::no,
                CopyToMsgHistory::no);

        const auto input_dir = query::dir(AllowCenter::no);

        msg_log::clear();

        if ((input_dir != Dir::END) && (input_dir != Dir::center))
        {
                // Valid direction
                const auto p =
                        map::g_player->m_pos +
                        dir_utils::offset(input_dir);

                player_try_close_or_jam_terrain(map::g_terrain.at(p));
        }
}

}  // namespace close

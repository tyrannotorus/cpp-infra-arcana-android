// =============================================================================
// Copyright 2011-2024 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "close.hpp"

#include <string>

#include "actor.hpp"
#include "array2.hpp"
#include "colors.hpp"
#include "common_text.hpp"
#include "direction.hpp"
#include "inventory.hpp"
#include "item_data.hpp"
#include "map.hpp"
#include "msg_log.hpp"
#include "pos.hpp"
#include "query.hpp"
#include "terrain.hpp"
#include "terrain_data.hpp"
#include "terrain_door.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static void player_try_close_or_jam_terrain(terrain::Terrain* const terrain)
{
        const bool can_see_terrain = map::g_seen.at(terrain->pos());

        if (terrain->id() != terrain::Id::door) {
                if (can_see_terrain) {
                        msg_log::add(
                                "I see nothing there to close or jam shut.");
                }
                else {
                        msg_log::add(
                                "I find nothing there to close or jam shut.");
                }

                return;
        }

        // This is a door

        auto* const door = static_cast<terrain::Door*>(terrain);

        if (door->is_open()) {
                door->actor_try_close(*map::g_player);

                return;
        }

        // Door is closed - try to jam it

        if ((door->type() == terrain::DoorType::metal) ||
            door->is_warded()) {
                if (can_see_terrain) {
                        msg_log::add("This door cannot be jammed.");
                }
                else {
                        msg_log::add("I find nothing there to close or jam shut.");
                }

                return;
        }

        const bool has_spike =
                map::g_player->m_inv.has_item_in_backpack(
                        item::Id::iron_spike);

        if (!has_spike) {
                msg_log::add("I have nothing to jam the door with.");

                return;
        }

        // Player has spikes

        const bool did_spike_door = door->actor_try_jam(*map::g_player);

        if (!did_spike_door) {
                return;
        }

        // Player did spike the door

        map::g_player->m_inv.decr_item_type_in_backpack(item::Id::iron_spike);

        const int nr_spikes_left =
                map::g_player->m_inv
                        .item_stack_size_in_backpack(
                                item::Id::iron_spike);

        if (nr_spikes_left == 0) {
                msg_log::add("I have no iron spikes left.");
        }
        else {
                msg_log::add(
                        "I have " +
                        std::to_string(nr_spikes_left) +
                        " iron spikes left.");
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

        const std::string hint =
                common_text::g_direction_query +
                " " +
                common_text::g_cancel_hint;

        msg_log::add(
                hint,
                colors::light_white(),
                MsgInterruptPlayer::no,
                MorePromptOnMsg::no,
                CopyToMsgHistory::no);

        const auto input_dir = query::dir(AllowCenter::no);

        msg_log::clear();

        if ((input_dir != Dir::END) && (input_dir != Dir::center)) {
                // Valid direction
                const auto p =
                        map::g_player->m_pos +
                        dir_utils::offset(input_dir);

                player_try_close_or_jam_terrain(map::g_terrain.at(p));
        }
}

}  // namespace close

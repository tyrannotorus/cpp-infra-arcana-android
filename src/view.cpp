// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "view.hpp"

#include <string>
#include <vector>

#include "actor.hpp"
#include "actor_data.hpp"
#include "actor_see.hpp"
#include "array2.hpp"
#include "colors.hpp"
#include "debug.hpp"
#include "game_time.hpp"
#include "global.hpp"
#include "item.hpp"
#include "map.hpp"
#include "msg_log.hpp"
#include "pos.hpp"
#include "terrain.hpp"
#include "text_format.hpp"

// -----------------------------------------------------------------------------
// private
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// View
// -----------------------------------------------------------------------------
namespace view
{
void print_location_info_msgs(const P& pos)
{
        bool is_cell_seen = false;

        if (map::is_pos_inside_map(pos))
        {
                is_cell_seen = map::g_seen.at(pos);
        }

        if (is_cell_seen)
        {
                // Describe terrain
                const auto* const terrain = map::g_terrain.at(pos);

                std::string str = terrain->name(Article::a);
                str = text_format::first_to_upper(terrain->name(Article::a));

                msg_log::add(
                        str + ".",
                        colors::text(),
                        MsgInterruptPlayer::no,
                        MorePromptOnMsg::no,
                        CopyToMsgHistory::no);

                // Describe mobile terrains
                for (auto* mob : game_time::g_mobs)
                {
                        if (mob->pos() == pos)
                        {
                                str = mob->name(Article::a);

                                str = text_format::first_to_upper(str);

                                msg_log::add(
                                        str + ".",
                                        colors::text(),
                                        MsgInterruptPlayer::no,
                                        MorePromptOnMsg::no,
                                        CopyToMsgHistory::no);
                        }
                }

                // Describe darkness
                if (map::g_dark.at(pos) && !map::g_light.at(pos))
                {
                        msg_log::add(
                                "It is very dark here.",
                                colors::text(),
                                MsgInterruptPlayer::no,
                                MorePromptOnMsg::no,
                                CopyToMsgHistory::no);
                }

                // Describe item
                const auto* item = map::g_items.at(pos);

                if (item)
                {
                        str =
                                item->name(
                                        ItemRefType::plural,
                                        ItemRefInf::yes,
                                        ItemRefAttInf::wpn_main_att_mode);

                        str = text_format::first_to_upper(str);

                        msg_log::add(
                                str + ".",
                                colors::text(),
                                MsgInterruptPlayer::no,
                                MorePromptOnMsg::no,
                                CopyToMsgHistory::no);
                }

                // Describe dead actors
                for (auto* actor : game_time::g_actors)
                {
                        if (actor->is_corpse() && actor->m_pos == pos)
                        {
                                ASSERT(!actor->m_data->corpse_name_a.empty());

                                str = text_format::first_to_upper(
                                        actor->m_data->corpse_name_a);

                                msg_log::add(
                                        str + ".",
                                        colors::text(),
                                        MsgInterruptPlayer::no,
                                        MorePromptOnMsg::no,
                                        CopyToMsgHistory::no);
                        }
                }
        }

        print_living_actor_info_msg(pos);

        if (!is_cell_seen)
        {
                msg_log::add(
                        "I have no vision here.",
                        colors::text(),
                        MsgInterruptPlayer::no,
                        MorePromptOnMsg::no,
                        CopyToMsgHistory::no);
        }
}

void print_living_actor_info_msg(const P& pos)
{
        auto* actor = map::first_actor_at_pos(pos);

        if (!actor ||
            actor->is_player() ||
            !actor->is_alive())
        {
                return;
        }

        if (actor::can_player_see_actor(*actor))
        {
                const std::string str =
                        text_format::first_to_upper(
                                actor->name_a());

                msg_log::add(
                        str + ".",
                        colors::text(),
                        MsgInterruptPlayer::no,
                        MorePromptOnMsg::no,
                        CopyToMsgHistory::no);
        }
        else
        {
                // Cannot see actor
                if (actor->is_player_aware_of_me())
                {
                        msg_log::add(
                                "There is a creature here.",
                                colors::text(),
                                MsgInterruptPlayer::no,
                                MorePromptOnMsg::no,
                                CopyToMsgHistory::no);
                }
        }
}

}  // namespace view

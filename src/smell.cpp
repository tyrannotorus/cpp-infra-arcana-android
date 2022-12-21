// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "smell.hpp"

#include <algorithm>
#include <cstddef>
#include <vector>

#include "actor.hpp"
#include "actor_data.hpp"
#include "actor_see.hpp"
#include "array2.hpp"
#include "colors.hpp"
#include "debug.hpp"
#include "direction.hpp"
#include "global.hpp"
#include "inventory.hpp"
#include "item_data.hpp"
#include "map.hpp"
#include "map_parsing.hpp"
#include "msg_log.hpp"
#include "player_bon.hpp"
#include "pos.hpp"
#include "property_data.hpp"
#include "property_handler.hpp"
#include "random.hpp"
#include "saving.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static int s_strength_decay_per_turn = 1;
static int s_strength_decay_on_spread = 3;
const static int s_msg_countdown_reset_value = 40;
static int s_msg_countdown = 0;

static void decay(smell::Smell& smell, const int decay_value)
{
        smell.strength_pct -= decay_value;

        if (smell.strength_pct <= 0) {
                smell = {};
        }
}

static void decay_smells_for_new_turn()
{
        const size_t nr_positions = map::nr_positions();

        for (size_t i = 0; i < nr_positions; ++i) {
                auto& smell = map::g_smell.at(i);

                if (smell.msg_ptr) {
                        decay(smell, s_strength_decay_per_turn);
                }
        }
}

static void spread_smell_from_pos(const P& p, const Array2<bool>& blocked)
{
        auto& src_smell = map::g_smell.at(p);

        for (const auto& d : dir_utils::g_dir_list) {
                const auto p_adj = p + d;

                if (!map::is_pos_inside_outer_walls(p_adj)) {
                        continue;
                }

                if (blocked.at(p_adj)) {
                        continue;
                }

                auto& dst_smell = map::g_smell_spread.at(p_adj);

                if (src_smell.strength_pct <= dst_smell.strength_pct) {
                        continue;
                }

                dst_smell = src_smell;

                decay(dst_smell, s_strength_decay_on_spread);
        }
}

static void update_smell_spread_map(const Array2<bool>& blocked)
{
        const int x0 = 1;
        const int x1 = map::w() - 2;
        const int y0 = 1;
        const int y1 = map::h() - 2;

        for (int x = x0; x <= x1; ++x) {
                for (int y = y0; y <= y1; ++y) {
                        spread_smell_from_pos({x, y}, blocked);
                }
        }
}

static bool is_wearing_item_preventing_detecting_smell()
{
        const auto& inv = map::g_player->m_inv;

        const bool is_prevented =
                (inv.has_item_in_slot(SlotId::body, item::Id::armor_asb_suit) ||
                 inv.has_item_in_slot(SlotId::head, item::Id::gas_mask));

        return is_prevented;
}

static bool is_seeing_mon_with_smell_msg()
{
        const auto seen_actors = actor::seen_actors(*map::g_player);

        return std::any_of(
                std::cbegin(seen_actors),
                std::cend(seen_actors),
                [](const auto* const actor) {
                        return !actor->m_data->smell_msg.empty();
                });
}

// -----------------------------------------------------------------------------
// smell
// -----------------------------------------------------------------------------
namespace smell
{
void init()
{
        s_msg_countdown = 0;
}

void save()
{
        saving::put_int(s_msg_countdown);
}

void load()
{
        s_msg_countdown = saving::get_int();
}

void on_std_turn()
{
        if (s_msg_countdown > 0) {
                --s_msg_countdown;
        }

        decay_smells_for_new_turn();

        // NOTE: We spread smell on the smell spread map, then the smell spread
        // map map is copied back to the smell map. This way, we can simply
        // iterate over the map and let each smell get a chance to spread
        // without affecting other smells.

        Array2<bool> blocks_projectiles(map::dims());

        // Re-using projectile blocking should be good enough.
        // TODO: Or allow smell to spread through closed doors?
        map_parsers::BlocksProjectiles().run(blocks_projectiles, map::rect());

        map::g_smell_spread = map::g_smell;
        update_smell_spread_map(blocks_projectiles);
        map::g_smell = map::g_smell_spread;
}

void put_smell_for_mon(const actor::Actor& mon)
{
        const std::string* const msg_ptr = &mon.m_data->smell_msg;

        if (msg_ptr->empty() || !mon.is_alive()) {
                return;
        }

        const bool allow_corpse_smell =
                mon.m_properties.has(PropId::corpse_rises);

        if ((mon.m_state == ActorState::corpse) && !allow_corpse_smell) {
                return;
        }

        // Ignore smells from ghoul monsters if player is ghoul
        if ((mon.id() == "MON_GHOUL") && player_bon::is_bg(Bg::ghoul)) {
                return;
        }

        Smell smell;

        smell.msg_ptr = msg_ptr;

        // TODO: Allow different strength for different monsters?
        smell.strength_pct = 30;

        auto& current_smell = map::g_smell.at(mon.m_pos);

        if (smell.strength_pct > current_smell.strength_pct) {
                current_smell = smell;
        }
}

void on_player_turn_start()
{
        if (is_seeing_mon_with_smell_msg()) {
                // A smelly monster is currently seen, reset the smell message
                // countdown (we don't want to print smell messages after just
                // seeing a monster).
                s_msg_countdown = s_msg_countdown_reset_value;
                return;
        }

        if (s_msg_countdown > 0) {
                return;
        }

        const auto& smell = map::g_smell.at(map::g_player->m_pos);

        if (!smell.msg_ptr) {
                return;
        }

        if (is_wearing_item_preventing_detecting_smell()) {
                return;
        }

        ASSERT((smell.strength_pct > 0) && (smell.strength_pct <= 100));

        if (!rnd::percent(smell.strength_pct)) {
                return;
        }

        msg_log::add(
                *smell.msg_ptr,
                colors::text(),
                MsgInterruptPlayer::no,
                MorePromptOnMsg::yes);

        s_msg_countdown = s_msg_countdown_reset_value;
}

}  // namespace smell

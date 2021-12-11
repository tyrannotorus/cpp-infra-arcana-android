// =============================================================================
// Copyright 2011-2021 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "throwing.hpp"

#include <cstddef>
#include <ostream>
#include <string>
#include <vector>

#include "ability_values.hpp"
#include "actor.hpp"
#include "actor_data.hpp"
#include "actor_hit.hpp"
#include "actor_mon.hpp"
#include "actor_player.hpp"
#include "actor_see.hpp"
#include "array2.hpp"
#include "attack_data.hpp"
#include "audio_data.hpp"
#include "colors.hpp"
#include "config.hpp"
#include "debug.hpp"
#include "dmg_range.hpp"
#include "drop.hpp"
#include "game_time.hpp"
#include "global.hpp"
#include "io.hpp"
#include "item.hpp"
#include "item_data.hpp"
#include "item_potion.hpp"
#include "line_calc.hpp"
#include "map.hpp"
#include "misc.hpp"
#include "msg_log.hpp"
#include "panel.hpp"
#include "pos.hpp"
#include "property_data.hpp"
#include "property_handler.hpp"
#include "random.hpp"
#include "sound.hpp"
#include "state.hpp"
#include "terrain.hpp"
#include "terrain_data.hpp"
#include "text_format.hpp"
#include "viewport.hpp"

namespace throwing
{
void player_throw_lit_explosive(const P& aim_cell)
{
        ASSERT(map::g_player->m_active_explosive);

        auto* const explosive = map::g_player->m_active_explosive;

        const int max_range = explosive->data().ranged.max_range;

        auto path =
                line_calc::calc_new_line(
                        map::g_player->m_pos,
                        aim_cell,
                        true,
                        max_range,
                        false);

        // Remove cells after blocked cells
        for (size_t i = 1; i < path.size(); ++i)
        {
                const auto p = path[i];

                const auto* t = map::g_terrain.at(p);

                if (!t->is_projectile_passable())
                {
                        path.resize(i);
                        break;
                }
        }

        const P end_pos =
                path.empty()
                ? P()
                : path.back();

        msg_log::add(explosive->str_on_player_throw());

        // Render
        if (path.size() > 1)
        {
                const auto color = explosive->ignited_projectile_color();

                for (const P& p : path)
                {
                        states::draw();

                        if (map::g_seen.at(p) &&
                            viewport::is_in_view(p))
                        {
                                io::draw_symbol(
                                        explosive->tile(),
                                        explosive->character(),
                                        Panel::map,
                                        viewport::to_view_pos(p),
                                        color);

                                io::update_screen();

                                io::sleep(
                                        config::delay_projectile_draw());
                        }
                }
        }

        const auto f_id = map::g_terrain.at(end_pos)->id();

        if (f_id != terrain::Id::chasm)
        {
                explosive->on_thrown_ignited_landing(end_pos);
        }

        delete explosive;

        map::g_player->m_active_explosive = nullptr;

        // Attacking ends cloaking and sanctuary
        map::g_player->m_properties.end_prop(PropId::cloaked);
        map::g_player->m_properties.end_prop(PropId::sanctuary);

        game_time::tick();
}

void throw_item(
        actor::Actor& actor_throwing,
        const P& tgt_pos,
        item::Item& item_thrown)
{
        TRACE_FUNC_BEGIN;

        ThrowAttData att_data(
                &actor_throwing,
                actor_throwing.m_pos,
                tgt_pos,
                actor_throwing.m_pos,
                item_thrown);

        TRACE << "Calculating throwing path" << std::endl;

        const auto path =
                line_calc::calc_new_line(
                        actor_throwing.m_pos,
                        tgt_pos,
                        false,
                        999,
                        false);

        TRACE << "Throwing path size: " << path.size() << std::endl;

        const auto& item_thrown_data = item_thrown.data();

        const std::string item_name_a = item_thrown.name(ItemNameType::a);

        if (actor::is_player(&actor_throwing))
        {
                msg_log::clear();

                msg_log::add("I throw " + item_name_a + ".");
        }
        else
        {
                // Monster throwing
                const auto& p = path.front();

                if (map::g_seen.at(p))
                {
                        const std::string name_the =
                                text_format::first_to_upper(
                                        actor_throwing.name_the());

                        msg_log::add(
                                name_the +
                                " throws " +
                                item_name_a +
                                ".");
                }
        }

        states::draw();

        bool is_actor_hit = false;

        const Color item_color = item_thrown.color();

        int break_item_one_in_n = -1;

        P pos(-1, -1);

        P drop_pos(-1, -1);

        for (size_t path_idx = 1; path_idx < path.size(); ++path_idx)
        {
                states::draw();

                // Have we gone out of range?
                {
                        const int max_range =
                                item_thrown.data().ranged.max_range;

                        const P current_pos = path[path_idx];

                        if (king_dist(path[0], current_pos) > max_range)
                        {
                                break;
                        }
                }

                pos = path[path_idx];

                drop_pos = pos;

                auto* const actor_here = map::first_actor_at_pos(pos);

                if (actor_here &&
                    ((pos == tgt_pos) ||
                     (actor_here->m_data->actor_size >=
                      actor::Size::humanoid)))
                {
                        att_data =
                                ThrowAttData(
                                        &actor_throwing,
                                        actor_throwing.m_pos,
                                        tgt_pos,
                                        pos,
                                        item_thrown);

                        const auto att_result =
                                ability_roll::roll(att_data.hit_chance_tot);

                        const int dmg = att_data.dmg_range.roll();

                        if (att_result >= ActionResult::success)
                        {
                                const bool is_potion =
                                        item_thrown_data.type ==
                                        ItemType::potion;

                                const bool player_see_cell =
                                        map::g_seen.at(pos);

                                if (player_see_cell)
                                {
                                        const Color hit_color =
                                                is_potion
                                                ? item_color
                                                : colors::light_red();

                                        io::draw_blast_at_cells(
                                                {pos},
                                                hit_color);
                                }

                                static_cast<actor::Mon*>(actor_here)
                                        ->make_player_aware_of_me();

                                Snd snd(
                                        "A creature is hit.",
                                        audio::SfxId::hit_small,
                                        IgnoreMsgIfOriginSeen::yes,
                                        pos,
                                        nullptr,
                                        SndVol::low,
                                        AlertsMon::no);

                                snd.run();

                                if (player_see_cell)
                                {
                                        const std::string defender_name =
                                                actor::can_player_see_actor(
                                                        *actor_here)
                                                ? text_format::first_to_upper(
                                                          actor_here->name_the())
                                                : "An unseen creature";

                                        msg_log::add(
                                                (defender_name +
                                                 " is hit."),
                                                colors::msg_good());
                                }

                                if (dmg > 0)
                                {
                                        actor::hit(
                                                *actor_here,
                                                dmg,
                                                item_thrown_data.ranged.dmg_type,
                                                AllowWound::yes);

                                        static_cast<actor::Mon*>(actor_here)
                                                ->become_aware_player(
                                                        actor::AwareSource::attacked);
                                }

                                item_thrown.on_ranged_hit(*actor_here);

                                is_actor_hit = true;

                                // TODO: Couldn't the potion handle this itself
                                // via "on_ranged_hit" called above? It would be
                                // good to make the throwing code more generic,
                                // it should not know about potions!
                                if (is_potion)
                                {
                                        if (actor_here->m_state == ActorState::alive)
                                        {
                                                // Apply potion effects
                                                auto* const potion =
                                                        static_cast<potion::Potion*>(
                                                                &item_thrown);

                                                potion->on_collide(pos, actor_here);

                                                // Attacking ends cloaking and sanctuary
                                                actor_throwing.m_properties.end_prop(PropId::cloaked);
                                                actor_throwing.m_properties.end_prop(PropId::sanctuary);
                                        }

                                        delete &item_thrown;

                                        game_time::tick();

                                        TRACE_FUNC_END;

                                        return;
                                }

                                const bool always_break_on_throw =
                                        item_thrown_data.ranged
                                                .always_break_on_throw;

                                const bool is_throwing_wpn =
                                        item_thrown_data.type ==
                                        ItemType::throwing_wpn;

                                if (!always_break_on_throw && is_throwing_wpn)
                                {
                                        break_item_one_in_n = 4;
                                }

                                break;
                        }
                }  // if actor hit

                const auto* terrain_here = map::g_terrain.at(pos);

                if (!terrain_here->is_projectile_passable())
                {
                        // Drop item before the wall, not on the wall
                        drop_pos = path[path_idx - 1];

                        break;
                }

                if (map::g_seen.at(pos) &&
                    viewport::is_in_view(pos))
                {
                        io::draw_symbol(
                                item_thrown.tile(),
                                item_thrown.character(),
                                Panel::map,
                                viewport::to_view_pos(pos),
                                item_color);

                        io::update_screen();

                        io::sleep(config::delay_projectile_draw());
                }

                if ((pos == tgt_pos) &&
                    (att_data.aim_lvl == actor::Size::floor))
                {
                        break;
                }
        }  // path loop

        // If potion, collide it on the landscape
        if (item_thrown_data.type == ItemType::potion)
        {
                const Color hit_color = item_color;

                io::draw_blast_at_seen_cells({pos}, hit_color);

                auto* const potion = static_cast<potion::Potion*>(&item_thrown);

                potion->on_collide(pos, nullptr);

                delete &item_thrown;

                // Attacking ends cloaking and sanctuary
                actor_throwing.m_properties.end_prop(PropId::cloaked);
                actor_throwing.m_properties.end_prop(PropId::sanctuary);

                game_time::tick();

                TRACE_FUNC_END;

                return;
        }

        // Set a collision sound effect (this may not necessarily get executed)
        const AlertsMon alerts =
                actor::is_player(&actor_throwing)
                ? AlertsMon::yes
                : AlertsMon::no;

        Snd snd(item_thrown_data.land_on_hard_snd_msg,
                item_thrown_data.land_on_hard_sfx,
                IgnoreMsgIfOriginSeen::yes,
                drop_pos,
                nullptr,
                SndVol::low,
                alerts);

        if (item_thrown.data().ranged.always_break_on_throw ||
            ((break_item_one_in_n != -1) &&
             rnd::one_in(break_item_one_in_n)))
        {
                delete &item_thrown;
        }
        else
        {
                // Not destroyed
                item_drop::drop_item_on_map(drop_pos, item_thrown);
        }

        auto is_noisy_matl = [](const Matl matl) {
                bool is_noisy = false;

                switch (matl)
                {
                case Matl::empty:
                        is_noisy = false;
                        break;

                case Matl::stone:
                        is_noisy = true;
                        break;

                case Matl::metal:
                        is_noisy = true;
                        break;

                case Matl::plant:
                        is_noisy = false;
                        break;

                case Matl::wood:
                        is_noisy = true;
                        break;

                case Matl::cloth:
                        is_noisy = false;
                        break;

                case Matl::fluid:
                        is_noisy = false;
                        break;
                }

                return is_noisy;
        };

        if (!is_actor_hit)
        {
                const Matl matl_at_last_pos =
                        map::g_terrain.at(pos)->matl();

                const Matl matl_at_drop_pos =
                        map::g_terrain.at(drop_pos)->matl();

                if (is_noisy_matl(matl_at_last_pos) ||
                    is_noisy_matl(matl_at_drop_pos))
                {
                        // OK, run the sound that we set up earlier
                        snd.run();
                }
        }

        // Attacking ends cloaking and sanctuary
        actor_throwing.m_properties.end_prop(PropId::cloaked);
        actor_throwing.m_properties.end_prop(PropId::sanctuary);

        game_time::tick();

        TRACE_FUNC_END;
}

}  // namespace throwing

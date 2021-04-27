// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "terrain_event.hpp"

#include <stddef.h>
#include <algorithm>
#include <iterator>
#include <utility>

#include "actor_factory.hpp"
#include "actor_mon.hpp"
#include "actor_player.hpp"
#include "game_time.hpp"
#include "io.hpp"
#include "map.hpp"
#include "misc.hpp"
#include "msg_log.hpp"
#include "popup.hpp"
#include "property.hpp"
#include "property_handler.hpp"
#include "terrain.hpp"
#include "actor.hpp"
#include "actor_data.hpp"
#include "debug.hpp"
#include "direction.hpp"
#include "fov.hpp"
#include "insanity.hpp"

namespace terrain
{
// -----------------------------------------------------------------------------
// Wall crumble
// -----------------------------------------------------------------------------
EventWallCrumble::EventWallCrumble(
        const P& p,
        std::vector<P>& walls,
        std::vector<P>& inner) :
        Event(p),
        m_wall_cells(walls),
        m_inner_cells(inner) {}

void EventWallCrumble::on_new_turn()
{
        if (!is_pos_adj(map::g_player->m_pos, m_pos, true))
        {
                return;
        }

        auto is_wall = [](const P& p) {
                const auto id = map::g_terrain.at(p)->id();

                return (id == terrain::Id::wall) ||
                        (id == terrain::Id::rubble_high);
        };

        // Check that it still makes sense to run the crumbling
        auto has_only_walls = [is_wall](const std::vector<P>& positions) {
                for (const P& p : positions)
                {
                        if (!is_wall(p))
                        {
                                return false;
                        }
                }

                return true;
        };

        const bool edge_ok = has_only_walls(m_wall_cells);
        const bool inner_ok = has_only_walls(m_inner_cells);

        if (!edge_ok || !inner_ok)
        {
                // This area is (no longer) covered by walls (perhaps walls have
                // been destroyed by an explosion for example), remove this
                // crumble event
                game_time::erase_mob(this, true);

                return;
        }

        const bool event_is_on_wall = is_wall(m_pos);

        ASSERT(event_is_on_wall);

        // Release mode robustness
        if (!event_is_on_wall)
        {
                game_time::erase_mob(this, true);

                return;
        }

        const bool event_is_on_edge =
                std::find(
                        std::begin(m_wall_cells),
                        std::end(m_wall_cells),
                        m_pos) !=
                std::end(m_wall_cells);

        ASSERT(event_is_on_edge);

        // Release mode robustness
        if (!event_is_on_edge)
        {
                game_time::erase_mob(this, true);

                return;
        }

        // OK, everything seems to be in a good state, go!

        if (map::g_player->m_properties.allow_see())
        {
                msg_log::add(
                        "Suddenly, the walls collapse!",
                        colors::msg_note(),
                        MsgInterruptPlayer::no,
                        MorePromptOnMsg::yes);
        }

        bool should_make_dark = false;

        // Check if any cell adjacent to the destroyed walls is dark
        for (const P& p : m_wall_cells)
        {
                for (const P& d : dir_utils::g_dir_list_w_center)
                {
                        const P p_adj(p + d);

                        if (!map::is_pos_inside_outer_walls(p_adj))
                        {
                                continue;
                        }

                        if (map::g_dark.at(p_adj))
                        {
                                should_make_dark = true;

                                break;
                        }
                }

                if (should_make_dark)
                {
                        break;
                }
        }

        // Destroy the outer walls
        for (const P& p : m_wall_cells)
        {
                if (!map::is_pos_inside_outer_walls(p))
                {
                        continue;
                }

                if (should_make_dark)
                {
                        map::g_dark.at(p) = true;
                }

                auto* const t = map::g_terrain.at(p);

                t->hit(DmgType::pure, nullptr);
        }

        // Destroy the inner walls
        for (const P& p : m_inner_cells)
        {
                if (should_make_dark)
                {
                        map::g_dark.at(p) = true;
                }

                auto* const t = map::g_terrain.at(p);

                t->hit(DmgType::pure, nullptr);

                if (rnd::one_in(8))
                {
                        map::make_gore(p);
                        map::make_blood(p);
                }
        }

        // Spawn monsters

        // Actor id, and corresponding maximum number of monsters allowed
        std::vector<std::pair<actor::Id, size_t>> spawn_bucket;

        if (map::g_dlvl <= g_dlvl_last_early_game)
        {
                spawn_bucket.emplace_back(actor::Id::rat, 24);
                spawn_bucket.emplace_back(actor::Id::rat_thing, 16);
        }

        spawn_bucket.emplace_back(actor::Id::zombie, 4);
        spawn_bucket.emplace_back(actor::Id::bloated_zombie, 1);

        const auto spawn_data = rnd::element(spawn_bucket);

        const auto actor_id = spawn_data.first;

        const auto nr_mon_limit_except_adj_to_entry = spawn_data.second;

        rnd::shuffle(m_inner_cells);

        std::vector<actor::Mon*> mon_spawned;

        for (const P& p : m_inner_cells)
        {
                if ((mon_spawned.size() < nr_mon_limit_except_adj_to_entry) ||
                    is_pos_adj(p, m_pos, false))
                {
                        auto* const actor = actor::make(actor_id, p);

                        auto* const mon = static_cast<actor::Mon*>(actor);

                        mon_spawned.push_back(mon);
                }
        }

        map::update_vision();

        // Make the monsters aware of the player
        for (auto* const mon : mon_spawned)
        {
                mon->become_aware_player(actor::AwareSource::other);
        }

        map::g_player->incr_shock(ShockLvl::terrifying, ShockSrc::see_mon);

        game_time::erase_mob(this, true);
}

// -----------------------------------------------------------------------------
// Snake emerge
// -----------------------------------------------------------------------------
EventSnakeEmerge::EventSnakeEmerge() :
        Event(P(-1, -1)) {}

bool EventSnakeEmerge::try_find_p()
{
        const auto blocked = blocked_cells(map::rect());

        auto p_bucket = to_vec(blocked, false, blocked.rect());

        if (p_bucket.empty())
        {
                return false;
        }

        rnd::shuffle(p_bucket);

        for (const P& p : p_bucket)
        {
                const R r = allowed_emerge_rect(p);

                const auto emerge_bucket = emerge_p_bucket(p, blocked, r);

                if ((int)emerge_bucket.size() >= m_min_nr_snakes)
                {
                        m_pos = p;

                        return true;
                }
        }

        return false;
}

R EventSnakeEmerge::allowed_emerge_rect(const P& p) const
{
        const int max_d = allowed_emerge_dist_range.max;

        const int x0 = std::max(1, p.x - max_d);
        const int y0 = std::max(1, p.y - max_d);
        const int x1 = std::min(map::w() - 2, p.x + max_d);
        const int y1 = std::min(map::h() - 2, p.y + max_d);

        return R(x0, y0, x1, y1);
}

bool EventSnakeEmerge::is_ok_terrain_at(const P& p) const
{
        ASSERT(map::is_pos_inside_map(p));

        const auto id = map::g_terrain.at(p)->id();

        return (
                (id == terrain::Id::floor) ||
                (id == terrain::Id::rubble_low));
}

std::vector<P> EventSnakeEmerge::emerge_p_bucket(
        const P& p,
        const Array2<bool>& blocked,
        const R& allowed_area) const
{
        if (!allowed_area.is_pos_inside(p))
        {
                ASSERT(false);

                return {};
        }

        FovMap fov_map;
        fov_map.hard_blocked = &blocked;
        fov_map.light = &map::g_light;
        fov_map.dark = &map::g_dark;

        const auto fov = fov::run(p, fov_map);

        std::vector<P> result;

        result.reserve(allowed_area.w() * allowed_area.h());

        for (int x = allowed_area.p0.x; x <= allowed_area.p1.x; ++x)
        {
                for (int y = allowed_area.p0.y; y <= allowed_area.p1.y; ++y)
                {
                        const P tgt_p(x, y);

                        const int min_d = allowed_emerge_dist_range.min;

                        if (!blocked.at(x, y) &&
                            !fov.at(x, y).is_blocked_hard &&
                            king_dist(p, tgt_p) >= min_d)
                        {
                                result.push_back(tgt_p);
                        }
                }
        }

        return result;
}

Array2<bool> EventSnakeEmerge::blocked_cells(const R& r) const
{
        Array2<bool> result(map::dims());

        for (int x = r.p0.x; x <= r.p1.x; ++x)
        {
                for (int y = r.p0.y; y <= r.p1.y; ++y)
                {
                        const P p(x, y);

                        result.at(p) = !is_ok_terrain_at(p);
                }
        }

        for (auto* const actor : game_time::g_actors)
        {
                const P& p = actor->m_pos;

                result.at(p) = true;
        }

        return result;
}

void EventSnakeEmerge::on_new_turn()
{
        if (map::g_player->m_pos != m_pos)
        {
                return;
        }

        const R r = allowed_emerge_rect(m_pos);

        const auto blocked = blocked_cells(r);

        auto tgt_bucket = emerge_p_bucket(m_pos, blocked, r);

        if ((int)tgt_bucket.size() < m_min_nr_snakes)
        {
                // Not possible to spawn at least minimum number
                return;
        }

        int max_nr_snakes = m_min_nr_snakes + (map::g_dlvl / 4);

        // Cap max number of snakes to the size of the target bucket

        // NOTE: The target bucket is at least as big as the minimum number
        max_nr_snakes = std::min(max_nr_snakes, int(tgt_bucket.size()));

        rnd::shuffle(tgt_bucket);

        std::vector<actor::Id> id_bucket;

        for (const auto& d : actor::g_data)
        {
                if (d.is_snake)
                {
                        id_bucket.push_back(d.id);
                }
        }

        const size_t idx = rnd::range(0, (int)id_bucket.size() - 1);

        const auto id = id_bucket[idx];

        const int nr_summoned = rnd::range(m_min_nr_snakes, max_nr_snakes);

        std::vector<P> seen_tgt_positions;

        for (int i = 0; i < nr_summoned; ++i)
        {
                ASSERT(i < (int)tgt_bucket.size());

                const P& p(tgt_bucket[i]);

                if (map::g_seen.at(p))
                {
                        seen_tgt_positions.push_back(p);
                }
        }

        if (!seen_tgt_positions.empty())
        {
                msg_log::add(
                        {"Suddenly, vicious snakes slither up from cracks in "
                         "the floor!"},
                        colors::msg_note(),
                        MsgInterruptPlayer::yes,
                        MorePromptOnMsg::yes);

                io::draw_blast_at_cells(seen_tgt_positions, colors::magenta());

                ShockLvl shock_lvl = ShockLvl::unsettling;

                if (insanity::has_sympt(InsSymptId::phobia_reptile_and_amph))
                {
                        shock_lvl = ShockLvl::terrifying;
                }

                map::g_player->incr_shock(shock_lvl, ShockSrc::see_mon);
        }

        for (int i = 0; i < nr_summoned; ++i)
        {
                const P& p(tgt_bucket[i]);

                auto* const actor = actor::make(id, p);

                auto* prop = new PropWaiting();

                prop->set_duration(2);

                actor->m_properties.apply(prop);

                static_cast<actor::Mon*>(actor)
                        ->become_aware_player(
                                actor::AwareSource::other);
        }

        game_time::erase_mob(this, true);
}

// -----------------------------------------------------------------------------
// Rats in the walls discovery
// -----------------------------------------------------------------------------
EventRatsInTheWallsDiscovery::EventRatsInTheWallsDiscovery(
        const P& terrain_pos) :
        Event(terrain_pos) {}

void EventRatsInTheWallsDiscovery::on_new_turn()
{
        // Run the event if player is at the event position or to the right of
        // it. If it's the latter case, it means the player somehow bypassed it
        // (e.g. teleport or dynamite), it should not be possible to use this as
        // a "cheat" to avoid the shock.
        if ((map::g_player->m_pos == m_pos) ||
            (map::g_player->m_pos.x > m_pos.x))
        {
                map::update_vision();

                const auto* const msg =
                        "Before me lies a twilit grotto of enormous height. "
                        "An insane tangle of human bones extends for yards "
                        "like a foamy sea - invariably in postures of demoniac "
                        "frenzy, either fighting off some menace or clutching "
                        "other forms with cannibal intent.";

                popup::Popup(popup::AddToMsgHistory::yes)
                        .set_title("A gruesome discovery...")
                        .set_msg(msg)
                        .run();

                map::g_player->incr_shock(
                        ShockLvl::mind_shattering,
                        ShockSrc::misc);

                for (auto* const actor : game_time::g_actors)
                {
                        if (actor->is_player())
                        {
                                continue;
                        }

                        actor->m_ai_state.is_roaming_allowed =
                                MonRoamingAllowed::yes;
                }

                game_time::erase_mob(this, true);
        }
}

}  // namespace terrain

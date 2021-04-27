// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include <algorithm>
#include <vector>

#include "mapgen.hpp"
#include "actor.hpp"
#include "actor_player.hpp"
#include "game_time.hpp"
#include "map_parsing.hpp"
#include "misc.hpp"
#include "terrain_pylon.hpp"
#include "array2.hpp"
#include "global.hpp"
#include "map.hpp"
#include "pos.hpp"
#include "random.hpp"

namespace mapgen
{
void make_pylons()
{
        // Never make Pylons late game (theme)
        if (map::g_dlvl >= g_dlvl_first_late_game)
        {
                return;
        }

        // Determine number of Pylons to place, by a weighted choice
        std::vector<int> nr_weights = {
                20,  // 0 pylon(s)
                5,  // 1 -
                1,  // 2 -
        };

        const int nr_pylons = rnd::weighted_choice(nr_weights);

        Array2<bool> blocked(map::dims());

        map_parsers::IsNotFloorLike()
                .run(blocked, blocked.rect());

        // Block player cell before expanding the blocked cells
        blocked.at(map::g_player->m_pos) = true;

        // Expand the blocked cells to block around them as well
        blocked = map_parsers::expand(blocked, 2);

        for (auto* const actor : game_time::g_actors)
        {
                blocked.at(actor->m_pos) = true;
        }

        for (int i = 0; i < nr_pylons; ++i)
        {
                // Store non-blocked (false) cells in a vector
                const auto p_bucket = to_vec(blocked, false, blocked.rect());

                if (p_bucket.empty())
                {
                        // No position available to place a Pylon - give up
                        return;
                }

                const auto pylon_p = rnd::element(p_bucket);

                // Do not try this position again, regardless if we place this
                // pylon or not
                blocked.at(pylon_p) = true;

                // OK, valid positions found - place Pylon
                auto* const pylon = new terrain::Pylon(pylon_p);

                map::put(pylon);

                // Don't place other pylons too near
                {
                        const int min_dist_between = 8;

                        const int x0 = std::max(
                                0,
                                pylon_p.x - min_dist_between);

                        const int y0 = std::max(
                                0,
                                pylon_p.y - min_dist_between);

                        const int x1 = std::min(
                                map::w() - 1,
                                pylon_p.x + min_dist_between);

                        const int y1 = std::min(
                                map::h() - 1,
                                pylon_p.y + min_dist_between);

                        for (int x = x0; x <= x1; ++x)
                        {
                                for (int y = y0; y <= y1; ++y)
                                {
                                        blocked.at(x, y) = true;
                                }
                        }
                }
        }  // Pylons loop
}  // make_pylons

}  // namespace mapgen

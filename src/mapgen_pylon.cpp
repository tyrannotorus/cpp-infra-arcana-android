// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include <algorithm>
#include <vector>

#include "actor.hpp"
#include "actor_player.hpp"
#include "array2.hpp"
#include "game_time.hpp"
#include "global.hpp"
#include "map.hpp"
#include "map_parsing.hpp"
#include "mapgen.hpp"
#include "misc.hpp"
#include "pos.hpp"
#include "random.hpp"
#include "terrain_pylon.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static int roll_nr_pylons_to_make()
{
        // Weight for 0 pylons, 1 pylon, etc.
        std::vector<int> nr_weights = {20, 5, 1};

        return rnd::weighted_choice(nr_weights);
}

static void set_area_blocked(
        const P& pos,
        const int dist,
        Array2<bool>& blocked)
{
        const int x0 = std::max(0, pos.x - dist);
        const int y0 = std::max(0, pos.y - dist);
        const int x1 = std::min(map::w() - 1, pos.x + dist);
        const int y1 = std::min(map::h() - 1, pos.y + dist);

        for (int x = x0; x <= x1; ++x)
        {
                for (int y = y0; y <= y1; ++y)
                {
                        blocked.at(x, y) = true;
                }
        }
}

// -----------------------------------------------------------------------------
// mapgen
// -----------------------------------------------------------------------------
namespace mapgen
{
void make_pylons()
{
        // Never make Pylons late game (theme)
        if (map::g_dlvl >= g_dlvl_first_late_game)
        {
                return;
        }

        Array2<bool> blocked(map::dims());

        map_parsers::IsNotFloorLike().run(blocked, blocked.rect());

        // Block player cell before expanding the blocked cells
        blocked.at(map::g_player->m_pos) = true;

        // Expand the blocked cells to block around them as well
        blocked = map_parsers::expand(blocked, 2);

        for (auto* const actor : game_time::g_actors)
        {
                blocked.at(actor->m_pos) = true;
        }

        const int nr_pylons = roll_nr_pylons_to_make();

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

                auto* const pylon = new terrain::Pylon(pylon_p);

                map::put(pylon);

                // Don't place other pylons too near
                const int min_dist = 8;

                set_area_blocked(pylon_p, min_dist, blocked);
        }
}

}  // namespace mapgen

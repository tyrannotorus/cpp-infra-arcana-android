// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <vector>

#include "actor.hpp"
#include "array2.hpp"
#include "debug.hpp"
#include "direction.hpp"
#include "game_time.hpp"
#include "global.hpp"
#include "map.hpp"
#include "map_parsing.hpp"
#include "mapgen.hpp"
#include "misc.hpp"
#include "pos.hpp"
#include "random.hpp"
#include "rect.hpp"
#include "terrain_factory.hpp"
#include "terrain_monolith.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static void block_positions_with_actors(Array2<bool>& blocked)
{
        for (actor::Actor* const actor : game_time::g_actors) {
                blocked.at(actor->m_pos) = true;
        }
}

static void block_area_around_player(Array2<bool>& blocked)
{
        const P& player_p = map::g_player->m_pos;

        const int r = g_fov_radi_int;

        const R fov_r(
                std::max(0, player_p.x - r),
                std::max(0, player_p.y - r),
                std::min(map::w() - 1, player_p.x + r),
                std::min(map::h() - 1, player_p.y + r));

        for (int x = fov_r.p0.x; x <= fov_r.p1.x; ++x) {
                for (int y = fov_r.p0.y; y <= fov_r.p1.y; ++y) {
                        blocked.at(x, y) = true;
                }
        }
}

static void remove_weighted_pos_at(
        std::vector<P>& positions,
        std::vector<int>& weights,
        const P& pos)
{
        for (size_t idx = 0; idx < positions.size(); ++idx) {
                if (positions[idx] == pos) {
                        positions.erase(std::cbegin(positions) + (int)idx);
                        weights.erase(std::cbegin(weights) + (int)idx);
                }
        }
}

static void place_terrains(const terrain::Id id, const int nr_to_spawn)
{
        Array2<bool> blocked(map::dims());

        map_parsers::IsNotFloorLike().run(blocked, blocked.rect());

        blocked = map_parsers::expand(blocked, blocked.rect());

        block_positions_with_actors(blocked);

        block_area_around_player(blocked);

        std::vector<P> weighted_positions;
        std::vector<int> weights;

        mapgen::make_explore_spawn_weights(blocked, weighted_positions, weights);

        for (int idx = 0; idx < nr_to_spawn; ++idx) {
                // Store non-blocked (false) cells in a vector.
                const std::vector<P> pos_bucket = to_vec(blocked, false, blocked.rect());

                if (pos_bucket.empty()) {
                        // Unable to place terrain.
                        return;
                }

                const int spawn_p_idx = rnd::weighted_choice(weights);

                const P p = weighted_positions[spawn_p_idx];

                map::set_terrain(terrain::make(id, p));

                // Block this position and all adjacent positions.
                for (const P& d : dir_utils::g_cardinal_list_w_center) {
                        const P p_adj = p + d;

                        blocked.at(p_adj) = true;

                        remove_weighted_pos_at(weighted_positions, weights, p_adj);
                }

                ASSERT(weights.size() == weighted_positions.size());
        }
}

// -----------------------------------------------------------------------------
// mapgen
// -----------------------------------------------------------------------------
namespace mapgen
{
void make_monoliths()
{
        std::vector<int> nr_weights = {
                50,  // 0 monolith(s)
                50,  // 1 -
                1,   // 2 -
        };

        const int nr_to_spawn = rnd::weighted_choice(nr_weights);

        place_terrains(terrain::Id::monolith, nr_to_spawn);
}

void make_mirrors()
{
        std::vector<int> nr_weights = {
                2,   // 0 mirrors(s)
                16,  // 1 -
                2,   // 2 -
                1,   // 3 -
        };

        const int nr_to_spawn = rnd::weighted_choice(nr_weights);

        place_terrains(terrain::Id::mirror, nr_to_spawn);
}

}  // namespace mapgen

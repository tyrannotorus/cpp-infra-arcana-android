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

namespace mapgen
{
void make_monoliths()
{
        // Determine number of Monoliths to place, by a weighted choice
        std::vector<int> nr_weights = {
                50,  // 0 monolith(s)
                50,  // 1 -
                1,  // 2 -
        };

        const int nr_monoliths = rnd::weighted_choice(nr_weights);

        Array2<bool> blocked(map::dims());

        map_parsers::IsNotFloorLike()
                .run(blocked, blocked.rect());

        blocked = map_parsers::expand(blocked, blocked.rect());

        for (auto* const actor : game_time::g_actors) {
                blocked.at(actor->m_pos) = true;
        }

        // Block the area around the player
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

        std::vector<P> spawn_weight_positions;

        std::vector<int> spawn_weights;

        mapgen::make_explore_spawn_weights(
                blocked,
                spawn_weight_positions,
                spawn_weights);

        for (int monolith_idx = 0; monolith_idx < nr_monoliths; ++monolith_idx) {
                // Store non-blocked (false) cells in a vector
                const auto p_bucket = to_vec(blocked, false, blocked.rect());

                if (p_bucket.empty()) {
                        // Unable to place Monolith
                        return;
                }

                const auto spawn_p_idx = rnd::weighted_choice(spawn_weights);

                const auto p = spawn_weight_positions[spawn_p_idx];

                map::set_terrain(terrain::make(terrain::Id::monolith, p));

                // Block this position and all adjacent positions
                for (const P& d : dir_utils::g_cardinal_list_w_center) {
                        const P p_adj(p + d);

                        blocked.at(p_adj) = true;

                        for (size_t spawn_weight_idx = 0;
                             spawn_weight_idx < spawn_weight_positions.size();
                             ++spawn_weight_idx) {
                                if (spawn_weight_positions[spawn_weight_idx] !=
                                    p_adj) {
                                        continue;
                                }

                                spawn_weight_positions.erase(
                                        std::begin(spawn_weight_positions) +
                                        spawn_weight_idx);

                                spawn_weights.erase(
                                        std::begin(spawn_weights) +
                                        spawn_weight_idx);
                        }
                }

                ASSERT(spawn_weights.size() == spawn_weight_positions.size());
        }
}

}  // namespace mapgen

// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include <algorithm>
#include <iterator>
#include <ostream>
#include <vector>

#include "actor.hpp"
#include "array2.hpp"
#include "debug.hpp"
#include "flood.hpp"
#include "map.hpp"
#include "map_parsing.hpp"
#include "mapgen.hpp"
#include "misc.hpp"
#include "pos.hpp"
#include "random.hpp"
#include "terrain_data.hpp"
#include "terrain_factory.hpp"

// -----------------------------------------------------------------------------
// mapgen
// -----------------------------------------------------------------------------
namespace mapgen
{
P make_stairs_at_random_pos()
{
        TRACE_FUNC_BEGIN;

        const auto allowed_cells = allowed_stair_cells();

        auto pos_bucket = to_vec(allowed_cells, true, allowed_cells.rect());

        const auto nr_ok_cells = (int)pos_bucket.size();

        const int min_nr_ok_cells_req = 3;

        if (nr_ok_cells < min_nr_ok_cells_req) {
                TRACE << "Nr available cells to place stairs too low "
                      << "(" << nr_ok_cells << "), discarding map"
                      << std::endl;

                g_is_map_valid = false;

                return {-1, -1};
        }

        TRACE << "Sorting the allowed cells vector "
              << "(" << pos_bucket.size() << " cells)" << std::endl;

        Array2<bool> blocks_player(map::dims());

        map_parsers::BlocksWalking(ParseActors::no)
                .run(blocks_player, blocks_player.rect());

        const std::vector<terrain::Id> free_terrains = {
                terrain::Id::door,
        };

        for (int x = 0; x < blocks_player.w(); ++x) {
                for (int y = 0; y < blocks_player.h(); ++y) {
                        const P p(x, y);

                        if (map_parsers::IsAnyOfTerrains(free_terrains).run(p)) {
                                blocks_player.at(p) = false;
                        }
                }
        }

        const auto flood = floodfill(map::g_player->m_pos, blocks_player);

        std::sort(
                std::begin(pos_bucket),
                std::end(pos_bucket),
                [flood](const P& p1, const P& p2) {
                        // If any of the two positions are unreached by the
                        // flood, assume this is furthest - otherwise compare
                        // which position is nearest on the flood
                        if (flood.at(p1) == 0) {
                                return false;
                        }
                        else if (flood.at(p2) == 0) {
                                return true;
                        }
                        else {
                                return flood.at(p1) < flood.at(p2);
                        }
                });

        TRACE << "Picking one of the furthest cells" << std::endl;

        const int cell_idx_range_size = std::max(1, nr_ok_cells / 5);

        const int cell_idx = nr_ok_cells - rnd::range(1, cell_idx_range_size);

        if ((cell_idx < 0) ||
            (cell_idx > (int)pos_bucket.size())) {
                ASSERT(false);

                g_is_map_valid = false;

                return {-1, -1};
        }

        const P stairs_pos(pos_bucket[cell_idx]);

        TRACE << "Spawning stairs at chosen cell" << std::endl;

        map::set_terrain(terrain::make(terrain::Id::stairs, stairs_pos));

        TRACE_FUNC_END;

        return stairs_pos;
}

}  // namespace mapgen

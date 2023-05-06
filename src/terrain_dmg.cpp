// =============================================================================
// Copyright 2011-2023 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "terrain_dmg.hpp"

#include <vector>

#include "array2.hpp"
#include "direction.hpp"
#include "item_data.hpp"
#include "item_factory.hpp"
#include "map.hpp"
#include "pos.hpp"
#include "random.hpp"
#include "terrain.hpp"
#include "terrain_data.hpp"
#include "terrain_factory.hpp"

// // --------------------------------------------------------------------------
// // Private
// // --------------------------------------------------------------------------

// // --------------------------------------------------------------------------
// // terrain
// // --------------------------------------------------------------------------
namespace terrain
{
void destr_all_adj_doors(const P p)
{
        for (const P& d : dir_utils::g_cardinal_list) {
                const auto p_adj = p + d;

                if (!map::is_pos_inside_map(p_adj)) {
                        continue;
                }

                if (map::g_terrain.at(p_adj)->id() !=
                    terrain::Id::door) {
                        continue;
                }

                map::update_terrain(
                        terrain::make(
                                terrain::Id::rubble_low,
                                p_adj));
        }
}

void destr_stone_wall(const P p)
{
        map::update_terrain(terrain::make(terrain::Id::rubble_low, p));

        if (rnd::one_in(4)) {
                item::make_item_on_floor(item::Id::rock, p);
        }
}

}  // namespace terrain

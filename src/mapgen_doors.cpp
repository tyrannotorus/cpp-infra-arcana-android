// =============================================================================
// Copyright 2011-2024 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include <ostream>
#include <vector>

#include "array2.hpp"
#include "debug.hpp"
#include "map.hpp"
#include "map_parsing.hpp"
#include "mapgen.hpp"
#include "pos.hpp"
#include "random.hpp"
#include "terrain.hpp"
#include "terrain_data.hpp"
#include "terrain_door.hpp"
#include "terrain_factory.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static bool is_wall(const P& p)
{
        return map::g_terrain.at(p)->id() == terrain::Id::wall;
}

static void try_make_door(const P& p)
{
        // Do not allow placing doors adjacent to these terrains
        std::vector<terrain::Id> forbidden_adj_terrains = {
                terrain::Id::door,
                terrain::Id::liquid};

        const map_parsers::AnyAdjIsAnyOfTerrains parser(forbidden_adj_terrains);

        if (parser.run(p)) {
                return;
        }

        bool is_good_ver = true;
        bool is_good_hor = true;

        for (int d = -1; d <= 1; d++) {
                if (is_wall({p.x + d, p.y})) {
                        is_good_hor = false;
                }

                if (is_wall({p.x, p.y + d})) {
                        is_good_ver = false;
                }

                if (d != 0) {
                        if (!is_wall({p.x, p.y + d})) {
                                is_good_hor = false;
                        }

                        if (!is_wall({p.x + d, p.y})) {
                                is_good_ver = false;
                        }
                }
        }

        if (is_good_hor || is_good_ver) {
                terrain::Door* door = nullptr;

                const WeightedItems<terrain::DoorType> type_weights = {
                        {
                                terrain::DoorType::wood,
                                terrain::DoorType::gate,
                                terrain::DoorType::metal,
                        },
                        {
                                8,
                                2,
                                1,
                        },
                };

                const terrain::DoorType door_type = type_weights.roll();

                switch (door_type) {
                case terrain::DoorType::wood:
                case terrain::DoorType::metal: {
                        terrain::Terrain* const mimic = terrain::make(terrain::Id::wall, p);

                        door =
                                static_cast<terrain::Door*>(
                                        terrain::make(terrain::Id::door, p));

                        door->set_mimic_terrain(mimic);

                        door->init_type_and_state(door_type);
                } break;

                case terrain::DoorType::gate: {
                        door =
                                static_cast<terrain::Door*>(
                                        terrain::make(terrain::Id::door, p));

                        door->init_type_and_state(door_type);
                } break;
                }

                map::set_terrain(door);
        }
}

// -----------------------------------------------------------------------------
// mapgen
// -----------------------------------------------------------------------------
namespace mapgen
{
void make_doors()
{
        TRACE << "Placing doors" << std::endl;

        for (int x = 0; x < map::w(); ++x) {
                for (int y = 0; y < map::h(); ++y) {
                        if (g_door_proposals.at(x, y) && rnd::fraction(3, 5)) {
                                try_make_door(P(x, y));
                        }
                }
        }
}

}  // namespace mapgen

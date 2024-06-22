// =============================================================================
// Copyright 2011-2024 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "mapgen.hpp"

#include <algorithm>
#include <iterator>
#include <ostream>
#include <string>
#include <vector>

#include "actor.hpp"
#include "array2.hpp"
#include "debug.hpp"
#include "game_time.hpp"
#include "global.hpp"
#include "map.hpp"
#include "map_parsing.hpp"
#include "misc.hpp"
#include "pos.hpp"
#include "random.hpp"
#include "terrain.hpp"
#include "terrain_data.hpp"
#include "terrain_factory.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
using TerrainVectors = std::vector<std::vector<terrain::Terrain*>>;

static TerrainVectors find_terrains_can_inscribe()
{
        TerrainVectors result;

        for (terrain::Terrain* const terrain : map::g_terrain) {
                if (terrain->allow_inscribe()) {
                        const terrain::Id id = terrain->id();

                        auto it =
                                std::find_if(
                                        std::begin(result),
                                        std::end(result),
                                        [id](std::vector<terrain::Terrain*>& v) {
                                                return v[0]->id() == id;
                                        });

                        if (it == std::end(result)) {
                                // Terrain id did not previously exist in any list. Add the terrain
                                // to a new list.
                                result.push_back({terrain});
                        }
                        else {
                                // Terrain id already exists, add the terrain to existing list.
                                it->push_back(terrain);
                        }
                }
        }

        return result;
}

static int try_inscribe_existing_terrain(const int nr_to_try)
{
        TerrainVectors terrains = find_terrains_can_inscribe();

        TRACE << "Found '" << terrains.size() << "' types of terrain to inscribe" << std::endl;

        if (terrains.empty()) {
                return 0;
        }

        int nr_inscribed = 0;

        for (int i = 0; i < nr_to_try; ++i) {
                const int type_idx = rnd::range(0, (int)terrains.size() - 1);

                std::vector<terrain::Terrain*>& terrains_of_type = terrains[type_idx];

                const int terrain_idx = rnd::range(0, (int)terrains_of_type.size() - 1);

                terrain::Terrain* const terrain = terrains_of_type[terrain_idx];

                ASSERT(terrain->allow_inscribe());

                TRACE << "Inscribing terrain '" << terrain->name(Article::a) << "'" << std::endl;

                terrain->set_inscribed();

                ++nr_inscribed;

                terrains_of_type.erase(std::begin(terrains_of_type) + terrain_idx);

                if (terrains_of_type.empty()) {
                        terrains.erase(std::begin(terrains) + type_idx);
                }

                if (terrains.empty()) {
                        break;
                }
        }

        return nr_inscribed;
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

        for (int x = x0; x <= x1; ++x) {
                for (int y = y0; y <= y1; ++y) {
                        blocked.at(x, y) = true;
                }
        }
}

static int try_put_new_inscribed_terrain(const int nr_to_try)
{
        // Early/mid game: Always place urns.
        // Late game: Mostly place petroglyphs, and sometimes urns.
        const terrain::Id terrain_id =
                ((map::g_dlvl <= g_dlvl_last_mid_game) || rnd::one_in(4))
                ? terrain::Id::urn
                : terrain::Id::petroglyph;

        Array2<bool> blocked(map::dims());

        map_parsers::IsNotFloorLike().run(blocked, blocked.rect());

        // Block player cell before expanding the blocked cells
        blocked.at(map::g_player->m_pos) = true;

        // Expand the blocked cells to block around them as well
        blocked = map_parsers::expand(blocked, 1);

        for (auto* const actor : game_time::g_actors) {
                blocked.at(actor->m_pos) = true;
        }

        int nr_placed = 0;

        for (int i = 0; i < nr_to_try; ++i) {
                // Store non-blocked (false) cells in a vector
                const auto pos_bucket = to_vec(blocked, false, blocked.rect());

                if (pos_bucket.empty()) {
                        // No position available to place a Pylon - give up
                        return nr_placed;
                }

                const P& pos = rnd::element(pos_bucket);

                terrain::Terrain* const terrain = terrain::make(terrain_id, pos);

                map::set_terrain(terrain);

                TRACE << "Placed terrain '" << terrain->name(Article::a) << "'" << std::endl;

                terrain->set_inscribed();

                ++nr_placed;

                // Don't place other inscribed terrains too near.
                const int min_dist = 2;

                set_area_blocked(pos, min_dist, blocked);
        }

        return nr_placed;
}

// -----------------------------------------------------------------------------
// mapgen
// -----------------------------------------------------------------------------
namespace mapgen
{
void put_inscribed_terrain()
{
        int nr_inscribed_terrains_to_try = rnd::range(0, 3);

        TRACE
                << "Attempting to include '"
                << nr_inscribed_terrains_to_try
                << "' number of inscribed terrains on the map"
                << std::endl;

        if (nr_inscribed_terrains_to_try == 0) {
                return;
        }

        // Prioritize inscribing existing terrain. Second, if there was not enough existing terrain
        // to inscribe, try to put new inscribed terrain instead.

        const int nr_existing_inscribed =
                try_inscribe_existing_terrain(nr_inscribed_terrains_to_try);

        nr_inscribed_terrains_to_try -= nr_existing_inscribed;

        TRACE
                << "Inscribed '"
                << nr_existing_inscribed
                << "' existing terrains ('"
                << nr_inscribed_terrains_to_try
                << "' left to try)"
                << std::endl;

        if (nr_inscribed_terrains_to_try == 0) {
                // We managed to inscribe enough existing terrain.
                return;
        }

        const int nr_new_placed = try_put_new_inscribed_terrain(nr_inscribed_terrains_to_try);

        TRACE
                << "Placed '"
                << nr_new_placed
                << "' new inscribed terrains "
                << std::endl;
}
}  // namespace mapgen

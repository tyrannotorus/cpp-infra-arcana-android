// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "test_utils.hpp"

#include "actor.hpp"
#include "actor_move.hpp"
#include "config.hpp"
#include "init.hpp"
#include "map.hpp"
#include "pos.hpp"
#include "query.hpp"
#include "random.hpp"
#include "rect.hpp"
#include "terrain.hpp"
#include "terrain_factory.hpp"

static void put_floor_and_walls_on_map()
{
        for (int x = 0; x < map::w(); ++x) {
                for (int y = 0; y < map::h(); ++y) {
                        const P p(x, y);

                        terrain::Terrain* t = nullptr;

                        if (map::is_pos_inside_outer_walls(p)) {
                                t = terrain::make(terrain::Id::floor, p);
                        }
                        else {
                                t = terrain::make(terrain::Id::wall, p);
                        }

                        map::set_terrain(t);
                }
        }
}

namespace test_utils
{
void init_all()
{
        rnd::seed();

        init::init_io();
        init::init_game();
        init::init_session();

        // To return default answers (e.g. "yes" from yes/no questions)
        query::cleanup();

        map::reset({100, 100});

        put_floor_and_walls_on_map();

        map::g_player->m_pos = map::rect().center();
}

void cleanup_all()
{
        init::cleanup_session();
        init::cleanup_game();
        init::cleanup_io();
}

}  // namespace test_utils

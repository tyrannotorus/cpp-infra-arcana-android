// =============================================================================
// Copyright 2011-2021 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "test_utils.hpp"

#include "actor_player.hpp"
#include "config.hpp"
#include "init.hpp"
#include "map.hpp"
#include "pos.hpp"
#include "query.hpp"
#include "random.hpp"
#include "rect.hpp"
#include "terrain.hpp"

static void put_floor_and_walls_on_map()
{
        for (int x = 0; x < map::w(); ++x)
        {
                for (int y = 0; y < map::h(); ++y)
                {
                        const bool is_on_edge =
                                (x == 0) ||
                                (y == 0) ||
                                (x == (map::w() - 1)) ||
                                (y == (map::h() - 1));

                        if (is_on_edge)
                        {
                                map::put(new terrain::Wall({x, y}));
                        }
                        else
                        {
                                map::put(new terrain::Floor({x, y}));
                        }
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

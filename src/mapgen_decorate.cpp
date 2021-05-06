// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include <vector>

#include "array2.hpp"
#include "direction.hpp"
#include "global.hpp"
#include "map.hpp"
#include "map_parsing.hpp"
#include "mapgen.hpp"
#include "pos.hpp"
#include "random.hpp"
#include "terrain.hpp"
#include "terrain_data.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static void decorate_walls()
{
        for (int x = 0; x < map::w(); ++x)
        {
                for (int y = 0; y < map::h(); ++y)
                {
                        auto* const terrain = map::g_terrain.at(x, y);

                        if (terrain->id() != terrain::Id::wall)
                        {
                                continue;
                        }

                        auto* const wall =
                                static_cast<terrain::Wall*>(terrain);

                        if (rnd::one_in(10))
                        {
                                map::put(new terrain::RubbleHigh(P(x, y)));
                        }
                        else
                        {
                                wall->set_rnd_common_wall();

                                if (rnd::one_in(40))
                                {
                                        wall->set_moss_grown();
                                }
                        }
                }
        }
}

static bool is_cave_floor(const P& p)
{
        const auto& t = *map::g_terrain.at(p);

        // TODO: Consider traps mimicking cave floor

        if (t.id() == terrain::Id::floor)
        {
                const auto* floor = static_cast<const terrain::Floor*>(&t);

                if (floor->m_type == terrain::FloorType::cave)
                {
                        return true;
                }
        }

        return false;
}

static bool should_convert_wall_to_cave_early_game(const P& p)
{
        return map_parsers::AllAdjIsTerrain(terrain::Id::wall).run(p);
}

static bool should_convert_wall_to_cave_mid_game(const P& p)
{
        const std::vector<terrain::Id> terrains_for_cave = {
                terrain::Id::bones,
                terrain::Id::bush,
                terrain::Id::chasm,
                terrain::Id::cocoon,
                terrain::Id::grass,
                terrain::Id::liquid_deep,
                terrain::Id::liquid_shallow,
                terrain::Id::rubble_high,
                terrain::Id::rubble_low,
                terrain::Id::stalagmite,
                terrain::Id::tree,
                terrain::Id::vines,
                terrain::Id::wall};

        const bool all_adj_are_terrains_for_cave =
                map_parsers::AllAdjIsAnyOfTerrains(terrains_for_cave)
                        .run(p);

        bool is_adj_to_cave_floor = false;

        for (const P& d : dir_utils::g_dir_list)
        {
                const P p_adj(p + d);

                if (!map::is_pos_inside_map(p_adj))
                {
                        continue;
                }

                if (is_cave_floor(p_adj))
                {
                        is_adj_to_cave_floor = true;

                        break;
                }
        }

        return all_adj_are_terrains_for_cave || is_adj_to_cave_floor;
}

static bool should_convert_wall_to_cave(const P& p)
{
        if (map::g_dlvl <= g_dlvl_last_early_game)
        {
                return should_convert_wall_to_cave_early_game(p);
        }
        else if (map::g_dlvl <= g_dlvl_last_mid_game)
        {
                return should_convert_wall_to_cave_mid_game(p);
        }
        else
        {
                return true;
        }
}

static void convert_walls_to_cave()
{
        for (int x = 0; x < map::w(); ++x)
        {
                for (int y = 0; y < map::h(); ++y)
                {
                        const P p(x, y);

                        auto& t = *map::g_terrain.at(p);

                        if ((t.id() != terrain::Id::wall) ||
                            !should_convert_wall_to_cave(p))
                        {
                                continue;
                        }

                        auto* const wall = static_cast<terrain::Wall*>(&t);

                        wall->m_type = terrain::WallType::cave;
                }
        }
}

static void put_a_bunch_of_vines_at(const P& p)
{
        for (const auto& d : dir_utils::g_dir_list_w_center)
        {
                const auto adj_p = p + d;

                if (!map::is_pos_inside_outer_walls(adj_p))
                {
                        continue;
                }

                const auto& id = map::g_terrain.at(adj_p)->id();
                const bool is_floor = (id == terrain::Id::floor);

                if (is_floor && ((p == adj_p) || rnd::one_in(3)))
                {
                        map::put(new terrain::Vines(adj_p));
                }
        }
}

static void decorate_floor_at(const P& p)
{
        if (map::g_terrain.at(p)->id() != terrain::Id::floor)
        {
                return;
        }

        if (rnd::one_in(100))
        {
                map::put(new terrain::RubbleLow(p));
        }

        if (rnd::one_in(150))
        {
                put_a_bunch_of_vines_at(p);
        }
}

static void decorate_floor()
{
        for (int x = 1; x < map::w() - 1; ++x)
        {
                for (int y = 1; y < map::h() - 1; ++y)
                {
                        const P pos(x, y);

                        decorate_floor_at(pos);
                }
        }
}

static void decorate_door_proposals()
{
        const auto dims = mapgen::g_door_proposals.dims();

        for (int x = 0; x < dims.x; ++x)
        {
                for (int y = 0; y < dims.y; ++y)
                {
                        const P p(x, y);

                        const auto id = map::g_terrain.at(p)->id();

                        if (!mapgen::g_door_proposals.at(p) ||
                            (id == terrain::Id::door))
                        {
                                continue;
                        }

                        if (rnd::one_in(30))
                        {
                                put_a_bunch_of_vines_at(p);
                        }
                }
        }
}

static void try_make_grate_at(const P& pos, const Array2<bool>& blocked)
{
        const int convert_to_grate_one_in_n = 6;

        if (!rnd::one_in(convert_to_grate_one_in_n))
        {
                return;
        }

        if (!mapgen::allow_make_grate_at(pos, blocked))
        {
                return;
        }

        map::put(new terrain::Grate(pos));
}

static void make_grates()
{
        Array2<bool> blocked(map::dims());

        map_parsers::BlocksWalking(ParseActors::no)
                .run(blocked, blocked.rect());

        for (int x = 1; x < map::w() - 1; ++x)
        {
                for (int y = 1; y < map::h() - 1; ++y)
                {
                        try_make_grate_at({x, y}, blocked);
                }
        }
}

// -----------------------------------------------------------------------------
// mapgen
// -----------------------------------------------------------------------------
namespace mapgen
{
void decorate()
{
        decorate_floor();

        decorate_walls();

        decorate_door_proposals();

        convert_walls_to_cave();

        make_grates();
}

bool allow_make_grate_at(const P& pos, const Array2<bool>& blocked)
{
        const auto t_id = map::g_terrain.at(pos)->id();
        const bool is_wall = (t_id == terrain::Id::wall);

        if (!is_wall)
        {
                return false;
        }

        const P adj_hor_1 = pos.with_x_offset(-1);
        const P adj_hor_2 = pos.with_x_offset(1);
        const P adj_ver_1 = pos.with_y_offset(1);
        const P adj_ver_2 = pos.with_y_offset(-1);

        const bool is_free_hor_1 = !blocked.at(adj_hor_1);
        const bool is_free_hor_2 = !blocked.at(adj_hor_2);
        const bool is_free_ver_1 = !blocked.at(adj_ver_1);
        const bool is_free_ver_2 = !blocked.at(adj_ver_2);

        const bool is_blocked_hor =
                !is_free_hor_1 &&
                !is_free_hor_2;

        const bool is_free_hor =
                is_free_hor_1 &&
                is_free_hor_2;

        const bool is_blocked_ver =
                !is_free_ver_1 &&
                !is_free_ver_2;

        const bool is_free_ver =
                is_free_ver_1 &&
                is_free_ver_2;

        const bool is_allowed =
                (is_blocked_hor && is_free_ver) ||
                (is_free_hor && is_blocked_ver);

        return is_allowed;
}

}  // namespace mapgen

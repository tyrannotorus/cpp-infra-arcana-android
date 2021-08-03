// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "map_parsing.hpp"

#include <algorithm>
#include <climits>
#include <cstddef>
#include <iterator>

#include "actor.hpp"
#include "array2.hpp"
#include "colors.hpp"
#include "debug.hpp"
#include "direction.hpp"
#include "flood.hpp"
#include "game_time.hpp"
#include "gfx.hpp"
#include "init.hpp"
#include "map.hpp"
#include "misc.hpp"
#include "panel.hpp"
#include "random.hpp"
#include "rect.hpp"
#include "state.hpp"
#include "terrain.hpp"

#ifndef NDEBUG
#include "io.hpp"
#include "viewport.hpp"
#endif  // NDEBUG

namespace map_parsers
{
// -----------------------------------------------------------------------------
// Base class
// -----------------------------------------------------------------------------
void MapParser::run(
        Array2<bool>& out,
        const R& area_to_parse_cells,
        const MapParseMode write_rule)
{
        ASSERT(m_parse_terrain == ParseTerrain::yes ||
               m_parse_mobs == ParseMobs::yes ||
               m_parse_actors == ParseActors::yes);

        const bool allow_write_false =
                write_rule == MapParseMode::overwrite;

        if (m_parse_terrain == ParseTerrain::yes)
        {
                for (int x = area_to_parse_cells.p0.x;
                     x <= area_to_parse_cells.p1.x;
                     ++x)
                {
                        for (int y = area_to_parse_cells.p0.y;
                             y <= area_to_parse_cells.p1.y;
                             ++y)
                        {
                                const auto& t = *map::g_terrain.at(x, y);

                                const bool is_match = parse_terrain(t, {x, y});

                                if (is_match || allow_write_false)
                                {
                                        out.at(x, y) = is_match;
                                }
                        }
                }
        }

        if (m_parse_mobs == ParseMobs::yes)
        {
                for (auto* mob : game_time::g_mobs)
                {
                        const P& p = mob->pos();

                        if (!area_to_parse_cells.is_pos_inside(p))
                        {
                                continue;
                        }

                        const bool is_match = parse_mob(*mob);

                        if (!is_match && !allow_write_false)
                        {
                                continue;
                        }

                        bool& v = out.at(p);

                        if (!v)
                        {
                                v = is_match;
                        }
                }
        }

        if (m_parse_actors == ParseActors::yes)
        {
                for (auto* actor : game_time::g_actors)
                {
                        const P& p = actor->m_pos;

                        if (!area_to_parse_cells.is_pos_inside(p))
                        {
                                continue;
                        }

                        const bool is_match = parse_actor(*actor);

                        if (!is_match && !allow_write_false)
                        {
                                continue;
                        }

                        bool& v = out.at(p);

                        if (!v)
                        {
                                v = is_match;
                        }
                }
        }

}  // run

bool MapParser::run(const P& pos) const
{
        ASSERT(m_parse_terrain == ParseTerrain::yes ||
               m_parse_mobs == ParseMobs::yes ||
               m_parse_actors == ParseActors::yes);

        bool r = false;

        if (m_parse_terrain == ParseTerrain::yes)
        {
                const auto& t = *map::g_terrain.at(pos);

                const bool is_match = parse_terrain(t, pos);

                if (is_match)
                {
                        r = true;
                }
        }

        if (m_parse_mobs == ParseMobs::yes)
        {
                for (auto* mob : game_time::g_mobs)
                {
                        const auto& mob_p = mob->pos();

                        if (mob_p != pos)
                        {
                                continue;
                        }

                        const bool is_match = parse_mob(*mob);

                        if (is_match)
                        {
                                r = true;
                                break;
                        }
                }
        }

        if (m_parse_actors == ParseActors::yes)
        {
                for (auto* actor : game_time::g_actors)
                {
                        const auto& actor_pos = actor->m_pos;

                        if (actor_pos != pos)
                        {
                                continue;
                        }

                        const bool is_match = parse_actor(*actor);

                        if (is_match)
                        {
                                r = true;
                                break;
                        }
                }
        }

        return r;

}  // cell

// -----------------------------------------------------------------------------
// Map parsers
// -----------------------------------------------------------------------------
bool BlocksLos::parse_terrain(
        const terrain::Terrain& t,
        const P& pos) const
{
        return (
                !map::is_pos_inside_outer_walls(pos) ||
                !t.is_los_passable());
}

bool BlocksLos::parse_mob(const terrain::Terrain& f) const
{
        return !f.is_los_passable();
}

bool BlocksWalking::parse_terrain(
        const terrain::Terrain& t,
        const P& pos) const
{
        return (
                !map::is_pos_inside_outer_walls(pos) ||
                !t.is_walkable());
}

bool BlocksWalking::parse_mob(const terrain::Terrain& f) const
{
        return !f.is_walkable();
}

bool BlocksWalking::parse_actor(const actor::Actor& a) const
{
        return a.is_alive();
}

bool BlocksActor::parse_terrain(
        const terrain::Terrain& t,
        const P& pos) const
{
        return (
                !map::is_pos_inside_outer_walls(pos) ||
                !t.can_move(m_actor));
}

bool BlocksActor::parse_mob(const terrain::Terrain& f) const
{
        return !f.can_move(m_actor);
}

bool BlocksActor::parse_actor(const actor::Actor& a) const
{
        return a.is_alive();
}

bool BlocksProjectiles::parse_terrain(
        const terrain::Terrain& t,
        const P& pos) const
{
        return (
                !map::is_pos_inside_outer_walls(pos) ||
                !t.is_projectile_passable());
}

bool BlocksProjectiles::parse_mob(const terrain::Terrain& f) const
{
        return !f.is_projectile_passable();
}

bool BlocksSound::parse_terrain(
        const terrain::Terrain& t,
        const P& pos) const
{
        return (
                !map::is_pos_inside_outer_walls(pos) ||
                !t.is_sound_passable());
}

bool BlocksSound::parse_mob(const terrain::Terrain& f) const
{
        return !f.is_sound_passable();
}

bool LivingActorsAdjToPos::parse_actor(const actor::Actor& a) const
{
        if (!a.is_alive())
        {
                return false;
        }

        return is_pos_adj(m_pos, a.m_pos, true);
}

bool BlocksTraps::parse_terrain(
        const terrain::Terrain& t,
        const P& pos) const
{
        return (
                !map::is_pos_inside_outer_walls(pos) ||
                !t.can_have_trap());
}

bool BlocksItems::parse_terrain(
        const terrain::Terrain& t,
        const P& pos) const
{
        return (
                !map::is_pos_inside_outer_walls(pos) ||
                !t.can_have_item());
}

bool BlocksItems::parse_mob(const terrain::Terrain& f) const
{
        return !f.can_have_item();
}

bool IsFloorLike::parse_terrain(
        const terrain::Terrain& t,
        const P& pos) const
{
        return (
                map::is_pos_inside_outer_walls(pos) &&
                t.is_floor_like());
}

bool IsNotFloorLike::parse_terrain(
        const terrain::Terrain& t,
        const P& pos) const
{
        return (
                !map::is_pos_inside_outer_walls(pos) ||
                !t.is_floor_like());
}

bool IsNotTerrain::parse_terrain(
        const terrain::Terrain& t,
        const P& pos) const
{
        (void)pos;

        return t.id() != m_terrain;
}

bool IsAnyOfTerrains::parse_terrain(
        const terrain::Terrain& t,
        const P& pos) const
{
        (void)pos;

        return (
                std::any_of(
                        std::cbegin(m_terrains),
                        std::cend(m_terrains),
                        [&t](const auto search_id) {
                                return search_id == t.id();
                        }));
}

bool AnyAdjIsAnyOfTerrains::parse_terrain(
        const terrain::Terrain& t,
        const P& pos) const
{
        (void)t;

        if (!map::is_pos_inside_outer_walls(pos))
        {
                return false;
        }

        for (const auto& d : dir_utils::g_dir_list_w_center)
        {
                const auto id_here = map::g_terrain.at(pos + d)->id();

                const auto search_result =
                        std::find(
                                std::begin(m_terrains),
                                std::end(m_terrains),
                                id_here);

                if (search_result != std::end(m_terrains))
                {
                        return true;
                }
        }

        return false;
}

bool AllAdjIsTerrain::parse_terrain(
        const terrain::Terrain& t,
        const P& pos) const
{
        (void)t;

        if (!map::is_pos_inside_outer_walls(pos))
        {
                return false;
        }

        for (const auto& d : dir_utils::g_dir_list_w_center)
        {
                if (map::g_terrain.at(pos + d)->id() != m_terrain)
                {
                        return false;
                }
        }

        return true;
}

bool AllAdjIsAnyOfTerrains::parse_terrain(
        const terrain::Terrain& t,
        const P& pos) const
{
        (void)t;

        if (!map::is_pos_inside_outer_walls(pos))
        {
                return false;
        }

        for (const auto& d : dir_utils::g_dir_list_w_center)
        {
                const auto current_id = map::g_terrain.at(pos + d)->id();

                bool is_match = false;

                for (auto search_id : m_terrains)
                {
                        if (search_id == current_id)
                        {
                                is_match = true;

                                break;
                        }
                }

                if (!is_match)
                {
                        return false;
                }
        }

        return true;
}

bool AllAdjIsNotTerrain::parse_terrain(
        const terrain::Terrain& t,
        const P& pos) const
{
        (void)t;

        if (pos.x <= 0 ||
            pos.x >= map::w() - 1 ||
            pos.y <= 0 ||
            pos.y >= map::h() - 1)
        {
                return false;
        }

        for (const auto& d : dir_utils::g_dir_list_w_center)
        {
                if (map::g_terrain.at(pos + d)->id() == m_terrain)
                {
                        return false;
                }
        }

        return true;
}

bool AllAdjIsNoneOfTerrains::parse_terrain(
        const terrain::Terrain& t,
        const P& pos) const
{
        (void)t;

        if (pos.x <= 0 ||
            pos.x >= map::w() - 1 ||
            pos.y <= 0 ||
            pos.y >= map::h() - 1)
        {
                return false;
        }

        for (const auto& d : dir_utils::g_dir_list_w_center)
        {
                const auto current_id = map::g_terrain.at(pos + d)->id();

                for (auto search_id : m_terrains)
                {
                        if (search_id == current_id)
                        {
                                return false;
                        }
                }
        }

        return true;
}

// -----------------------------------------------------------------------------
// Various utility algorithms
// -----------------------------------------------------------------------------
Array2<bool> cells_within_dist_of_others(
        const Array2<bool>& in,
        const Range& dist_interval)
{
        const P dims = in.dims();

        Array2<bool> result(dims);

        for (int x_outer = 0; x_outer < dims.x; x_outer++)
        {
                for (int y_outer = 0; y_outer < dims.y; y_outer++)
                {
                        if (result.at(x_outer, y_outer))
                        {
                                continue;
                        }

                        for (int d = dist_interval.min;
                             d <= dist_interval.max;
                             d++)
                        {
                                P p0(std::max(0, x_outer - d),
                                     std::max(0, y_outer - d));

                                P p1(std::min(dims.x - 1, x_outer + d),
                                     std::min(dims.y - 1, y_outer + d));

                                for (int x = p0.x; x <= p1.x; ++x)
                                {
                                        if (!in.at(x, p0.y) && !in.at(x, p1.y))
                                        {
                                                continue;
                                        }

                                        result.at(x_outer, y_outer) = true;
                                        break;
                                }

                                for (int y = p0.y; y <= p1.y; ++y)
                                {
                                        if (!in.at(p0.x, y) && !in.at(p1.x, y))
                                        {
                                                continue;
                                        }

                                        result.at(x_outer, y_outer) = true;
                                        break;
                                }
                        }  // distance loop
                }  // outer y loop
        }  // outer x loop

        return result;

}  // cells_within_dist_of_others

void append(Array2<bool>& base, const Array2<bool>& append)
{
        const size_t nr_positions = map::nr_positions();
        for (size_t i = 0; i < nr_positions; ++i)
        {
                if (append.at(i))
                {
                        base.at(i) = true;
                }
        }
}

Array2<bool> expand(const Array2<bool>& in, const R& area_allowed_to_modify)
{
        const P dims = in.dims();

        Array2<bool> result(dims);

        const int x0 = std::max(
                0,
                area_allowed_to_modify.p0.x);

        const int y0 = std::max(
                0,
                area_allowed_to_modify.p0.y);

        const int x1 = std::min(
                dims.x - 1,
                area_allowed_to_modify.p1.x);

        const int y1 = std::min(
                dims.y - 1,
                area_allowed_to_modify.p1.y);

        for (int x = x0; x <= x1; ++x)
        {
                for (int y = y0; y <= y1; ++y)
                {
                        result.at(x, y) = false;

                        // Search all cells adjacent to the current position for
                        // any cell which is "true" in the input arry.
                        const int cmp_x0 = std::max(x - 1, 0);
                        const int cmp_y0 = std::max(y - 1, 0);
                        const int cmp_x1 = std::min(x + 1, dims.x - 1);
                        const int cmp_y1 = std::min(y + 1, dims.y - 1);

                        for (int cmp_x = cmp_x0;
                             cmp_x <= cmp_x1;
                             ++cmp_x)
                        {
                                bool is_found = false;

                                for (int cmp_y = cmp_y0;
                                     cmp_y <= cmp_y1;
                                     ++cmp_y)
                                {
                                        if (in.at(cmp_x, cmp_y))
                                        {
                                                result.at(x, y) = true;

                                                is_found = true;

                                                break;
                                        }
                                }  // Compare y loop

                                if (is_found)
                                {
                                        break;
                                }
                        }  // Compare x loop
                }  // y loop
        }  // x loop

        return result;

}  // expand

Array2<bool> expand(const Array2<bool>& in, const int dist)
{
        const P dims = in.dims();

        Array2<bool> result(dims);

        for (int x = 0; x < dims.x; ++x)
        {
                for (int y = 0; y < dims.y; ++y)
                {
                        result.at(x, y) = false;

                        const int x0 = x - dist;
                        const int y0 = y - dist;
                        const int x1 = x + dist;
                        const int y1 = y + dist;

                        const int cmp_x0 = x0 < 0 ? 0 : x0;
                        const int cmp_y0 = y0 < 0 ? 0 : y0;
                        const int cmp_x1 = x1 > dims.x - 1 ? dims.x - 1 : x1;
                        const int cmp_y1 = y1 > dims.y - 1 ? dims.y - 1 : y1;

                        for (int cmp_y = cmp_y0;
                             cmp_y <= cmp_y1;
                             ++cmp_y)
                        {
                                bool is_found = false;

                                for (int cmp_x = cmp_x0;
                                     cmp_x <= cmp_x1;
                                     ++cmp_x)
                                {
                                        if (!in.at(cmp_x, cmp_y))
                                        {
                                                continue;
                                        }

                                        is_found = result.at(x, y) = true;

                                        break;
                                }

                                if (is_found)
                                {
                                        break;
                                }
                        }
                }
        }

        return result;

}  // expand

bool is_map_connected(const Array2<bool>& blocked)
{
        const auto dims = blocked.dims();

        const int x0 = 1;
        const int y0 = 1;
        const int x1 = (dims.x - 2);
        const int y1 = (dims.y - 2);

        // Find a free position to search from.
        P origin(-1, -1);

        for (int x = x0; x <= x1; ++x)
        {
                for (int y = y0; y <= y1; ++y)
                {
                        if (!blocked.at(x, y))
                        {
                                origin.set(x, y);
                                break;
                        }
                }

                if (origin.x != -1)
                {
                        break;
                }
        }

        ASSERT(map::is_pos_inside_outer_walls(origin));

        const auto flood =
                floodfill(
                        origin,
                        blocked,
                        INT_MAX,
                        {-1, -1},
                        true);

        // Check if there is any free position not reached by the flood - if so,
        // the map is not connected.
        for (int x = x0; x <= x1; ++x)
        {
                for (int y = y0; y <= y1; ++y)
                {
                        const P p(x, y);

                        if ((p != origin) &&
                            !blocked.at(p) &&
                            (flood.at(p) == 0))
                        {
                                return false;
                        }
                }
        }

        return true;

}  // is_map_connected

}  // namespace map_parsers

// -----------------------------------------------------------------------------
// Is closer to pos
// -----------------------------------------------------------------------------
bool IsCloserToPos::operator()(const P& p1, const P& p2) const
{
        const int king_dist1 = king_dist(m_pos.x, m_pos.y, p1.x, p1.y);
        const int king_dist2 = king_dist(m_pos.x, m_pos.y, p2.x, p2.y);

        return king_dist1 < king_dist2;
}

// -----------------------------------------------------------------------------
// Is further from pos
// -----------------------------------------------------------------------------
bool IsFurtherFromPos::operator()(const P& p1, const P& p2) const
{
        const int king_dist1 = king_dist(m_pos.x, m_pos.y, p1.x, p1.y);
        const int king_dist2 = king_dist(m_pos.x, m_pos.y, p2.x, p2.y);

        return king_dist1 > king_dist2;
}

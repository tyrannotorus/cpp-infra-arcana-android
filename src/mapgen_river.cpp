// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include <ostream>
#include <vector>

#include "array2.hpp"
#include "debug.hpp"
#include "global.hpp"
#include "map.hpp"
#include "mapgen.hpp"
#include "misc.hpp"
#include "pos.hpp"
#include "random.hpp"
#include "rect.hpp"
#include "room.hpp"
#include "terrain.hpp"
#include "terrain_factory.hpp"

namespace mapgen
{
void reserve_river(Array2<Region>& regions)
{
        TRACE_FUNC_BEGIN;

        R room_rect;

        Region* river_region = nullptr;

        const int reserved_padding = 2;

        auto init_room_rect = [&](
                                      int& len0,
                                      int& len1,
                                      int& breadth0,
                                      int& breadth1,
                                      const P& reg0,
                                      const P& reg2) {
                const R regions_tot_rect(
                        regions.at(reg0.x, reg0.y).r.p0,
                        regions.at(reg2.x, reg2.y).r.p1);

                room_rect = regions_tot_rect;

                river_region = &regions.at(reg0.x, reg0.y);

                const int c = (breadth1 + breadth0) / 2;

                breadth0 = c - reserved_padding;

                breadth1 = c + reserved_padding;

                ASSERT(is_area_inside(room_rect, regions_tot_rect, true));

                --len0;  // Extend room rectangle to map edge

                ++len1;
        };

        const Axis axis = rnd::coin_toss() ? Axis::hor : Axis::ver;

        if (axis == Axis::hor) {
                // Horizontal
                init_room_rect(
                        room_rect.p0.x,
                        room_rect.p1.x,
                        room_rect.p0.y,
                        room_rect.p1.y,
                        P(0, 1),
                        P(2, 1));
        }
        else {
                // Vertical
                init_room_rect(
                        room_rect.p0.y,
                        room_rect.p1.y,
                        room_rect.p0.x,
                        room_rect.p1.x,
                        P(1, 0),
                        P(1, 2));
        }

        room::Room* const room = room::make(room::RoomType::river, room_rect);

        auto* const river_room = static_cast<room::RiverRoom*>(room);

        river_room->m_axis = axis;

        river_region->main_room = room;

        river_region->is_free = false;

        if (axis == Axis::hor) {
                // Horizontal
                regions.at(1, 1) = regions.at(2, 1) = *river_region;
        }
        else {
                // Vertical
                regions.at(1, 1) = regions.at(1, 2) = *river_region;
        }

        map::g_room_list.push_back(room);

        auto make = [&](const int x0, const int x1, const int y0, const int y1) {
                for (int x = x0; x <= x1; ++x) {
                        for (int y = y0; y <= y1; ++y) {
                                // Just put floor for now, river terrain will be
                                // placed later
                                auto* const t =
                                        terrain::make(
                                                terrain::Id::floor,
                                                {x, y});

                                map::set_terrain(t);

                                map::g_room_map.at(x, y) = room;
                        }
                }
        };

        if (axis == Axis::hor) {
                // Horizontal
                make(room_rect.p0.x + 1,
                     room_rect.p1.x - 1,
                     room_rect.p0.y,
                     room_rect.p1.y);
        }
        else {
                // Vertical
                make(room_rect.p0.x,
                     room_rect.p1.x,
                     room_rect.p0.y + 1,
                     room_rect.p1.y - 1);
        }

        TRACE_FUNC_END;
}

}  // namespace mapgen

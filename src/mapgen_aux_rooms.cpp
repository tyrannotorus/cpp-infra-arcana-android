// =============================================================================
// Copyright 2011-2024 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include <ostream>
#include <vector>

#include "array2.hpp"
#include "debug.hpp"
#include "game_time.hpp"
#include "global.hpp"
#include "map.hpp"
#include "map_parsing.hpp"
#include "mapgen.hpp"
#include "pos.hpp"
#include "random.hpp"
#include "rect.hpp"
#include "room.hpp"
#include "terrain.hpp"
#include "terrain_event.hpp"
#include "terrain_factory.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static void make_crumble_room(const R& room_area_incl_walls, const P& event_pos)
{
        std::vector<P> wall_cells;
        std::vector<P> inner_cells;

        const R& a = room_area_incl_walls;  // Abbreviation

        for (int x = a.p0.x; x <= a.p1.x; ++x) {
                for (int y = a.p0.y; y <= a.p1.y; ++y) {
                        const P p(x, y);

                        if (x == a.p0.x ||
                            x == a.p1.x ||
                            y == a.p0.y ||
                            y == a.p1.y) {
                                wall_cells.push_back(p);
                        }
                        else {
                                // Is inner cell
                                inner_cells.push_back(p);
                        }

                        // Fill the room with walls (so we don't have an
                        // inaccessible empty room)
                        auto* const terrain =
                                terrain::make(terrain::Id::wall, p);

                        map::set_terrain(terrain);
                }
        }

        auto* const event =
                static_cast<terrain::EventWallCrumble*>(
                        terrain::make(
                                terrain::Id::event_wall_crumble,
                                event_pos));

        event->set_inner_positions(inner_cells);
        event->set_wall_positions(wall_cells);

        game_time::add_mob(event);

}  // make_crumble_room

// NOTE: The positions and size can be outside map (e.g. negative positions).
// This function just returns false in that case.
static bool try_make_aux_room(
        const P& p,
        const P& d,
        Array2<bool>& blocked,
        const P& door_p)
{
        const R aux_rect(p, p + d - 1);

        const R aux_rect_with_border(aux_rect.p0 - 1, aux_rect.p1 + 1);

        ASSERT(aux_rect_with_border.is_pos_inside(door_p));

        if (map::is_area_inside_map(aux_rect_with_border)) {
                // Check if area is blocked
                for (int x = aux_rect_with_border.p0.x;
                     x <= aux_rect_with_border.p1.x;
                     ++x) {
                        for (int y = aux_rect_with_border.p0.y;
                             y <= aux_rect_with_border.p1.y;
                             ++y) {
                                if (blocked.at(x, y)) {
                                        // Can't build here, bye...
                                        return false;
                                }
                        }
                }

                for (int x = aux_rect.p0.x; x <= aux_rect.p1.x; ++x) {
                        for (int y = aux_rect.p0.y; y <= aux_rect.p1.y; ++y) {
                                blocked.at(x, y) = true;

                                ASSERT(!map::g_room_map.at(x, y));
                        }
                }

                // Make a "crumble room"?
                if (rnd::one_in(30)) {
                        room::Room* const room =
                                room::make(
                                        room::RoomType::crumble_room,
                                        aux_rect);

                        mapgen::register_room(*room);

                        make_crumble_room(aux_rect_with_border, door_p);
                }
                else {
                        // Not "crumble room"
                        mapgen::make_room(aux_rect, IsSubRoom::no);
                }

                return true;
        }

        return false;

}  // try_make_aux_room

namespace mapgen
{
void make_aux_rooms(Array2<Region>& regions)
{
        TRACE_FUNC_BEGIN;

        const int nr_tries_per_side = 20;

        auto rnd_aux_room_dim = []() {
                const Range range(2, 7);

                return P(range.roll(), range.roll());
        };

        Array2<bool> floor_cells(map::dims());

        // Get blocked cells
        map_parsers::BlocksWalking(ParseActors::no)
                .run(floor_cells, floor_cells.rect());

        // TODO: It would be better with a parse predicate that checks for free
        // cells immediately

        // Flip the values so that we get free cells
        for (auto& is_floor : floor_cells) {
                is_floor = !is_floor;
        }

        for (int region_x = 0; region_x < 3; region_x++) {
                for (int region_y = 0; region_y < 3; region_y++) {
                        const Region& region = regions.at(region_x, region_y);

                        if (region.main_room) {
                                room::Room& main_r = *region.main_room;

                                // Right
                                if (rnd::fraction(3, 4)) {
                                        for (int i = 0; i < nr_tries_per_side; ++i) {
                                                const P con_p(
                                                        main_r.m_r.p1.x + 1,
                                                        rnd::range(
                                                                main_r.m_r.p0.y + 1,
                                                                main_r.m_r.p1.y - 1));

                                                const P aux_d(rnd_aux_room_dim());

                                                const P aux_p(
                                                        con_p.x + 1,
                                                        rnd::range(
                                                                con_p.y - aux_d.y + 1,
                                                                con_p.y));

                                                if (floor_cells.at(con_p.x - 1, con_p.y)) {
                                                        const bool did_place_room =
                                                                try_make_aux_room(
                                                                        aux_p,
                                                                        aux_d,
                                                                        floor_cells,
                                                                        con_p);

                                                        if (did_place_room) {
                                                                TRACE_VERBOSE << "Aux room placed right"
                                                                              << std::endl;
                                                                break;
                                                        }
                                                }
                                        }
                                }

                                // Up
                                if (rnd::fraction(3, 4)) {
                                        for (int i = 0; i < nr_tries_per_side; ++i) {
                                                const P con_p(
                                                        rnd::range(
                                                                main_r.m_r.p0.x + 1,
                                                                main_r.m_r.p1.x - 1),
                                                        main_r.m_r.p0.y - 1);

                                                const P aux_d(rnd_aux_room_dim());

                                                const P aux_p(rnd::range(con_p.x - aux_d.x + 1, con_p.x), con_p.y - 1);

                                                if (floor_cells.at(con_p.x, con_p.y + 1)) {
                                                        const bool did_place_room =
                                                                try_make_aux_room(aux_p, aux_d, floor_cells, con_p);

                                                        if (did_place_room) {
                                                                TRACE_VERBOSE << "Aux room placed up"
                                                                              << std::endl;
                                                                break;
                                                        }
                                                }
                                        }
                                }

                                // Left
                                if (rnd::fraction(3, 4)) {
                                        for (int i = 0; i < nr_tries_per_side; ++i) {
                                                const P con_p(
                                                        main_r.m_r.p0.x - 1,
                                                        rnd::range(
                                                                main_r.m_r.p0.y + 1,
                                                                main_r.m_r.p1.y - 1));

                                                const P aux_d(rnd_aux_room_dim());

                                                const P aux_p(con_p.x - 1, rnd::range(con_p.y - aux_d.y + 1, con_p.y));

                                                if (floor_cells.at(con_p.x + 1, con_p.y)) {
                                                        const bool did_place_room =
                                                                try_make_aux_room(aux_p, aux_d, floor_cells, con_p);

                                                        if (did_place_room) {
                                                                TRACE_VERBOSE << "Aux room placed left"
                                                                              << std::endl;
                                                                break;
                                                        }
                                                }
                                        }
                                }

                                // Down
                                if (rnd::fraction(3, 4)) {
                                        for (int i = 0; i < nr_tries_per_side; ++i) {
                                                const P con_p(
                                                        rnd::range(
                                                                main_r.m_r.p0.x + 1,
                                                                main_r.m_r.p1.x - 1),
                                                        main_r.m_r.p1.y + 1);

                                                const P aux_d(rnd_aux_room_dim());

                                                const P aux_p(rnd::range(con_p.x - aux_d.x + 1, con_p.x), con_p.y + 1);

                                                if (floor_cells.at(con_p.x, con_p.y - 1)) {
                                                        const bool did_place_room =
                                                                try_make_aux_room(aux_p, aux_d, floor_cells, con_p);

                                                        if (did_place_room) {
                                                                TRACE_VERBOSE << "Aux room placed down"
                                                                              << std::endl;
                                                                break;
                                                        }
                                                }
                                        }
                                }
                        }
                }  // Region y loop
        }          // Region x loop

        TRACE_FUNC_END;

}  // make_aux_rooms

}  // namespace mapgen

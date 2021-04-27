// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include <ostream>

#include "mapgen.hpp"
#include "debug.hpp"
#include "room.hpp"
#include "terrain.hpp"
#include "terrain_door.hpp"
#include "array2.hpp"
#include "global.hpp"
#include "map.hpp"
#include "map_templates.hpp"
#include "pos.hpp"
#include "random.hpp"
#include "rect.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static void put_templ_terrains(
        const Array2<char>& templ,
        const P& p0)
{
        const P dims(templ.dims());

        for (int templ_x = 0; templ_x < dims.x; ++templ_x)
        {
                for (int templ_y = 0; templ_y < dims.y; ++templ_y)
                {
                        const P templ_p(templ_x, templ_y);

                        const P p(p0 + templ_p);

                        const char c = templ.at(templ_p);

                        bool is_room_cell = true;

                        switch (c)
                        {
                        case '.':
                        {
                                map::put(new terrain::Floor(p));
                        }
                        break;

                        case '#':
                        {
                                map::put(new terrain::Wall(p));

                                is_room_cell = false;
                        }
                        break;

                        case '-':
                        {
                                map::put(new terrain::Altar(p));
                        }
                        break;

                        case '~':
                        {
                                auto* liquid = new terrain::LiquidShallow(p);

                                liquid->m_type = LiquidType::water;

                                map::put(liquid);
                        }
                        break;

                        case '0':
                        {
                                map::put(new terrain::Brazier(p));
                        }
                        break;

                        case 'P':
                        {
                                map::put(new terrain::Statue(p));
                        }
                        break;

                        case '+':
                        {
                                auto* mimic = new terrain::Wall(p);

                                map::put(
                                        new terrain::Door(
                                                p,
                                                mimic,
                                                terrain::DoorType::wood));
                        }
                        break;

                        case 'x':
                        {
                                map::put(
                                        new terrain::Door(
                                                p,
                                                nullptr,
                                                terrain::DoorType::gate));
                        }
                        break;

                        case '=':
                        {
                                map::put(new terrain::Grate(p));
                        }
                        break;

                        case '"':
                        {
                                map::put(new terrain::Vines(p));
                        }
                        break;

                        case '*':
                        {
                                map::put(new terrain::Chains(p));
                        }
                        break;

                        // (Space)
                        case ' ':
                        {
                                // Do nothing

                                is_room_cell = false;
                        }
                        break;

                        default:
                        {
                                TRACE << "Illegal template character \""
                                      << c
                                      << "\""
                                      << " (at template pos "
                                      << templ_x
                                      << ", "
                                      << templ_y
                                      << ")"
                                      << std::endl;

                                // Release mode robustness: invalidate the map
                                mapgen::g_is_map_valid = false;

                                ASSERT(false);

                                return;
                        }
                        break;

                        }  // switch

                        if (!is_room_cell)
                        {
                                map::g_room_map.at(p) = nullptr;
                        }
                }  // y loop
        }  // x loop
}

static Room* make_template_room(const RoomTempl& templ, Region& region)
{
        const P dims(templ.symbols.dims());

        // Random position inside the region
        const P p0(region.r.p0.x + rnd::range(0, region.r.w() - dims.x), region.r.p0.y + rnd::range(0, region.r.h() - dims.y));

        const P p1(p0.x + dims.x - 1, p0.y + dims.y - 1);

        const R r(p0, p1);

        auto* room = new TemplateRoom(r, templ.type);

        mapgen::register_room(*room);

        // Place terrains on the map based on the template

        // NOTE: This must be done AFTER "register_room", since it may remove
        // some of its cells from the global room map (e.g. untouched cells)
        put_templ_terrains(templ.symbols, p0);

        region.main_room = room;
        region.is_free = false;

        return room;

}  // make_template_room

// -----------------------------------------------------------------------------
// mapgen
// -----------------------------------------------------------------------------
namespace mapgen
{
Room* make_room(Region& region)
{
        ASSERT(!region.main_room);

        ASSERT(region.is_free);

        const int templ_room_one_in_n = 7;

        // Make a templated room?
        if ((map::g_dlvl <= g_dlvl_last_mid_game) &&
            rnd::one_in(templ_room_one_in_n))
        {
                const P max_dims(region.r.dims());

                const auto* templ = map_templates::random_room_templ(max_dims);

                if (templ)
                {
                        const auto& symbols = templ->symbols;

                        if ((symbols.dims().x > max_dims.x) ||
                            (symbols.dims().y > max_dims.y))
                        {
                                ASSERT(false);
                        }
                        else
                        {
                                Room* const room =
                                        make_template_room(*templ, region);

                                map_templates::on_base_room_template_placed(
                                        *templ);

                                return room;
                        }
                }

                // Failed to make a templated room - fine, make a normal
                // procedural room
        }

        // Make a procedural room

        const R room_rect = region.rnd_room_rect();

        auto* room = room_factory::make_random_room(room_rect, IsSubRoom::no);

        register_room(*room);

        make_floor(*room);

        region.main_room = room;
        region.is_free = false;

        return room;
}

Room* make_room(const R& r, const IsSubRoom is_sub_room)
{
        auto* room = room_factory::make_random_room(r, is_sub_room);

        register_room(*room);

        make_floor(*room);

        return room;
}

}  // namespace mapgen

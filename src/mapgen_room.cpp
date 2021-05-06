// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include <ostream>

#include "array2.hpp"
#include "debug.hpp"
#include "global.hpp"
#include "map.hpp"
#include "map_templates.hpp"
#include "mapgen.hpp"
#include "pos.hpp"
#include "random.hpp"
#include "rect.hpp"
#include "room.hpp"
#include "terrain.hpp"
#include "terrain_door.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static void put_templ_symbol_at(const P& p, const char c)
{
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

        // Space
        case ' ':
        {
                // Do nothing
        }
        break;

        default:
        {
                TRACE
                        << "Illegal template character \""
                        << c
                        << "\""
                        << std::endl;

                // Release mode robustness: invalidate the map
                mapgen::g_is_map_valid = false;

                ASSERT(false);

                return;
        }
        break;

        }  // switch
}

static bool is_symbol_room_cell(const char c)
{
        switch (c)
        {
        case '#':
        case ' ':
                return false;

        default:
                return true;
        }
}

static void put_templ_terrains(
        const Array2<char>& templ,
        const P& p0)
{
        const bool generate_optional_walls = rnd::coin_toss();

        const P dims(templ.dims());

        for (int templ_x = 0; templ_x < dims.x; ++templ_x)
        {
                for (int templ_y = 0; templ_y < dims.y; ++templ_y)
                {
                        const P templ_p(templ_x, templ_y);

                        const auto p = p0 + templ_p;

                        char c = templ.at(templ_p);

                        if (c == '?')
                        {
                                c = generate_optional_walls ? '#' : '.';
                        }

                        put_templ_symbol_at(p, c);

                        if (!is_symbol_room_cell(c))
                        {
                                map::g_room_map.at(p) = nullptr;
                        }
                }
        }
}

static Room* make_template_room(const RoomTempl& templ, Region& region)
{
        const P dims(templ.symbols.dims());

        // Random position inside the region
        const P p0(
                region.r.p0.x + rnd::range(0, region.r.w() - dims.x),
                region.r.p0.y + rnd::range(0, region.r.h() - dims.y));

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

static Room* try_make_template_room(Region& region)
{
        const P max_dims(region.r.dims());

        const auto* templ = map_templates::random_room_templ(max_dims);

        if (!templ)
        {
                return nullptr;
        }

        const auto& symbols = templ->symbols;

        if ((symbols.dims().x > max_dims.x) ||
            (symbols.dims().y > max_dims.y))
        {
                ASSERT(false);

                return nullptr;
        }

        auto* const room = make_template_room(*templ, region);

        map_templates::on_base_room_template_placed(*templ);

        return room;
}

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
                auto* const room = try_make_template_room(region);

                if (room)
                {
                        return room;
                }

                // Fine, make a normal procedural room instead...
        }

        // Make a procedural room

        const auto room_rect = region.rnd_room_rect();

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

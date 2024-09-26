// =============================================================================
// Copyright 2011-2024 Martin Törnqvist <m.tornq@gmail.com>
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
#include "terrain_factory.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static void put_templ_symbol_at(
        const P& p,
        const char c,
        const LiquidType liquid_type)
{
        switch (c) {
        case '.': {
                map::set_terrain(terrain::make(terrain::Id::floor, p));
        } break;

        case '#': {
                map::set_terrain(terrain::make(terrain::Id::wall, p));
        } break;

        case '-': {
                map::set_terrain(terrain::make(terrain::Id::altar, p));
        } break;

        case '~': {
                terrain::Terrain* const t = terrain::make(terrain::Id::liquid, p);

                static_cast<terrain::Liquid*>(t)->m_type = liquid_type;

                map::set_terrain(t);
        } break;

        case '0': {
                map::set_terrain(terrain::make(terrain::Id::brazier, p));
        } break;

        case 'P': {
                map::set_terrain(terrain::make(terrain::Id::statue, p));
        } break;

        case '+': {
                auto* mimic = terrain::make(terrain::Id::wall, p);

                auto* const t =
                        static_cast<terrain::Door*>(
                                terrain::make(terrain::Id::door, p));

                t->set_mimic_terrain(mimic);
                t->init_type_and_state(terrain::DoorType::wood);

                map::set_terrain(t);
        } break;

        case 'x': {
                auto* const t =
                        static_cast<terrain::Door*>(
                                terrain::make(terrain::Id::door, p));

                t->init_type_and_state(terrain::DoorType::gate);

                map::set_terrain(t);
        } break;

        case '=': {
                map::set_terrain(terrain::make(terrain::Id::grate, p));
        } break;

        case '"': {
                map::set_terrain(terrain::make(terrain::Id::vines, p));
        } break;

        case '*': {
                map::set_terrain(terrain::make(terrain::Id::chains, p));
        } break;

        // Space
        case ' ': {
                // Do nothing
        } break;

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
        } break;

        }  // switch
}

static bool is_symbol_room_cell(const char c)
{
        switch (c) {
        case '#':
        case ' ':
                return false;

        default:
                return true;
        }
}

static void put_templ_terrains(const RoomTempl& templ, const P& p0)
{
        const bool generate_optional_walls = rnd::coin_toss();

        const P dims(templ.symbols.dims());

        auto liquid_type = LiquidType::water;

        if (templ.type == room::RoomType::damp) {
                liquid_type = rnd::coin_toss() ? LiquidType::water : LiquidType::mud;
        }

        for (int templ_x = 0; templ_x < dims.x; ++templ_x) {
                for (int templ_y = 0; templ_y < dims.y; ++templ_y) {
                        const P templ_p(templ_x, templ_y);

                        const auto p = p0 + templ_p;

                        char c = templ.symbols.at(templ_p);

                        if (c == '?') {
                                c = generate_optional_walls ? '#' : '.';
                        }

                        put_templ_symbol_at(p, c, liquid_type);

                        if (!is_symbol_room_cell(c)) {
                                map::g_room_map.at(p) = nullptr;
                        }
                }
        }
}

static room::Room* make_template_room(const RoomTempl& templ, Region& region)
{
        const P dims(templ.symbols.dims());

        // Random position inside the region
        const P p0(
                region.r.p0.x + rnd::range(0, region.r.w() - dims.x),
                region.r.p0.y + rnd::range(0, region.r.h() - dims.y));

        const P p1(p0.x + dims.x - 1, p0.y + dims.y - 1);

        const R r(p0, p1);

        room::Room* room = new room::TemplateRoom(r, templ.type);

        mapgen::register_room(*room);

        // Place terrains on the map based on the template.

        // NOTE: This must be done AFTER "register_room", since it may remove
        // some of its cells from the global room map (e.g. untouched cells).
        put_templ_terrains(templ, p0);

        region.main_room = room;
        region.is_free = false;

        return room;

}  // make_template_room

static room::Room* try_make_template_room(Region& region)
{
        const P max_dims(region.r.dims());

        const auto* templ = map_templates::random_room_templ(max_dims);

        if (!templ) {
                return nullptr;
        }

        const auto& symbols = templ->symbols;

        if ((symbols.dims().x > max_dims.x) ||
            (symbols.dims().y > max_dims.y)) {
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
room::Room* make_room(Region& region)
{
        ASSERT(!region.main_room);

        ASSERT(region.is_free);

        const int templ_room_one_in_n = 7;

        // Make a templated room?
        if ((map::g_dlvl <= g_dlvl_last_mid_game) &&
            rnd::one_in(templ_room_one_in_n)) {
                auto* const room = try_make_template_room(region);

                if (room) {
                        return room;
                }

                // Fine, make a normal procedural room instead...
        }

        // Make a procedural room

        const auto room_rect = region.rnd_room_rect();

        auto* room = room::make_random_room(room_rect, IsSubRoom::no);

        register_room(*room);

        make_floor(*room);

        region.main_room = room;
        region.is_free = false;

        return room;
}

room::Room* make_room(const R& r, const IsSubRoom is_sub_room)
{
        auto* room = room::make_random_room(r, is_sub_room);

        register_room(*room);

        make_floor(*room);

        return room;
}

}  // namespace mapgen

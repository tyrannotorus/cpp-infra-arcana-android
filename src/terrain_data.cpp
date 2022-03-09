// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "terrain_data.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>

#include "actor.hpp"
#include "debug.hpp"
#include "property_data.hpp"
#include "property_handler.hpp"
#include "terrain.hpp"
#include "terrain_door.hpp"
#include "terrain_event.hpp"
#include "terrain_gong.hpp"
#include "terrain_mob.hpp"
#include "terrain_monolith.hpp"
#include "terrain_pylon.hpp"
#include "terrain_trap.hpp"

struct P;

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static terrain::TerrainData s_data[(size_t)terrain::Id::END];

static void reset_data(terrain::TerrainData& d)
{
        d = terrain::TerrainData();
}

static void add_to_list_and_reset(terrain::TerrainData& d)
{
        s_data[(size_t)d.id] = d;

        reset_data(d);
}

static void init_data_list()
{
        terrain::TerrainData d;
        reset_data(d);

        d.id = terrain::Id::floor;
        d.make_obj = [](const P& p) {
                return new terrain::Floor(p);
        };
        d.character = '.';
        d.tile = gfx::TileId::floor;
        d.move_rules.is_walkable = true;
        d.is_floor_like = true;
        d.matl_type = Matl::stone;
        d.can_have_gore = true;
        d.can_have_trap = true;
        add_to_list_and_reset(d);

        d.id = terrain::Id::bridge;
        d.make_obj = [](const P& p) {
                return new terrain::Bridge(p);
        };
        d.move_rules.is_walkable = true;
        d.matl_type = Matl::wood;
        add_to_list_and_reset(d);

        d.id = terrain::Id::wall;
        d.make_obj = [](const P& p) {
                return new terrain::Wall(p);
        };
        d.character = '#';
        d.tile = gfx::TileId::wall_top;
        d.move_rules.props_allow_move.push_back(PropId::ethereal);
        d.move_rules.props_allow_move.push_back(PropId::burrowing);
        d.is_sound_passable = false;
        d.is_projectile_passable = false;
        d.is_los_passable = false;
        d.is_smoke_passable = false;
        d.can_have_gore = false;
        d.can_have_corpse = false;
        d.can_have_item = false;
        d.matl_type = Matl::stone;
        add_to_list_and_reset(d);

        d.id = terrain::Id::tree;
        d.make_obj = [](const P& p) {
                return new terrain::Tree(p);
        };
        d.character = '|';
        d.tile = gfx::TileId::tree;
        d.move_rules.props_allow_move.push_back(PropId::ethereal);
        d.is_sound_passable = false;
        d.is_projectile_passable = false;
        d.is_los_passable = false;
        d.can_have_blood = false;
        d.can_have_gore = false;
        d.can_have_corpse = false;
        d.can_have_item = false;
        d.matl_type = Matl::wood;
        d.shock_when_adjacent = 1;
        add_to_list_and_reset(d);

        d.id = terrain::Id::grass;
        d.make_obj = [](const P& p) {
                return new terrain::Grass(p);
        };
        d.character = '.';
        d.tile = gfx::TileId::floor;
        d.move_rules.is_walkable = true;
        d.is_floor_like = true;
        d.matl_type = Matl::plant;
        d.can_have_gore = true;
        d.can_have_trap = true;
        add_to_list_and_reset(d);

        d.id = terrain::Id::bush;
        d.make_obj = [](const P& p) {
                return new terrain::Bush(p);
        };
        d.character = '"';
        d.tile = gfx::TileId::bush;
        d.move_rules.is_walkable = true;
        d.is_los_passable = false;
        d.matl_type = Matl::plant;
        add_to_list_and_reset(d);

        d.id = terrain::Id::vines;
        d.make_obj = [](const P& p) {
                return new terrain::Vines(p);
        };
        d.character = '"';
        d.tile = gfx::TileId::vines;
        d.move_rules.is_walkable = true;
        d.is_los_passable = false;
        d.can_have_blood = false;
        d.can_have_gore = false;
        d.matl_type = Matl::plant;
        d.auto_spawn_placement = terrain::TerrainPlacement::either;
        add_to_list_and_reset(d);

        d.id = terrain::Id::chains;
        d.make_obj = [](const P& p) {
                return new terrain::Chains(p);
        };
        d.character = '"';
        d.tile = gfx::TileId::chains;
        d.move_rules.is_walkable = true;
        d.is_los_passable = true;
        d.is_projectile_passable = true;
        d.can_have_blood = true;
        d.matl_type = Matl::metal;
        d.auto_spawn_placement = terrain::TerrainPlacement::either;
        add_to_list_and_reset(d);

        d.id = terrain::Id::grate;
        d.make_obj = [](const P& p) {
                return new terrain::Grate(p);
        };
        d.character = '#';
        d.tile = gfx::TileId::grate;
        d.move_rules.props_allow_move.push_back(PropId::ethereal);
        d.move_rules.props_allow_move.push_back(PropId::burrowing);
        d.move_rules.props_allow_move.push_back(PropId::ooze);
        d.move_rules.props_allow_move.push_back(PropId::small_crawling);
        d.is_los_passable = true;
        d.can_have_blood = false;  // Looks weird
        d.can_have_gore = false;
        d.can_have_corpse = false;
        d.can_have_item = false;
        d.matl_type = Matl::metal;
        add_to_list_and_reset(d);

        d.id = terrain::Id::stairs;
        d.make_obj = [](const P& p) {
                return new terrain::Stairs(p);
        };
        d.character = '>';
        d.tile = gfx::TileId::stairs_down;
        d.can_have_blood = false;
        d.can_have_gore = false;
        d.can_have_corpse = false;
        d.can_have_item = false;
        d.matl_type = Matl::stone;
        add_to_list_and_reset(d);

        d.id = terrain::Id::monolith;
        d.make_obj = [](const P& p) {
                return new terrain::Monolith(p);
        };
        d.character = '|';
        d.tile = gfx::TileId::monolith;
        d.is_projectile_passable = false;
        d.is_los_passable = false;
        d.can_have_blood = false;  // We don't want to mess with the color
        d.can_have_gore = false;
        d.can_have_corpse = false;
        d.can_have_item = false;
        d.shock_when_adjacent = 10;
        d.matl_type = Matl::stone;
        add_to_list_and_reset(d);

        d.id = terrain::Id::pylon;
        d.make_obj = [](const P& p) {
                return new terrain::Pylon(p);
        };
        d.character = '|';
        d.tile = gfx::TileId::END;  // This is set elsewhere
        d.is_projectile_passable = false;
        d.is_los_passable = true;
        d.can_have_blood = false;  // We don't want to mess with the color
        d.can_have_gore = false;
        d.can_have_corpse = false;
        d.can_have_item = false;
        d.shock_when_adjacent = 10;
        d.matl_type = Matl::metal;
        add_to_list_and_reset(d);

        d.id = terrain::Id::lever;
        d.make_obj = [](const P& p) {
                return new terrain::Lever(p);
        };
        d.character = '%';
        d.tile = gfx::TileId::lever_left;
        d.can_have_blood = false;
        d.can_have_gore = false;
        d.can_have_corpse = false;
        d.can_have_item = false;
        d.matl_type = Matl::metal;
        add_to_list_and_reset(d);

        d.id = terrain::Id::brazier;
        d.make_obj = [](const P& p) {
                return new terrain::Brazier(p);
        };
        d.character = '0';
        d.tile = gfx::TileId::brazier;
        d.can_have_blood = false;
        d.can_have_gore = false;
        d.can_have_corpse = false;
        d.can_have_item = false;
        d.matl_type = Matl::metal;
        d.auto_spawn_placement = terrain::TerrainPlacement::away_from_walls;
        add_to_list_and_reset(d);

        d.id = terrain::Id::liquid;
        d.make_obj = [](const P& p) {
                return new terrain::Liquid(p);
        };
        d.character = '~';
        d.tile = gfx::TileId::water;
        d.move_rules.is_walkable = true;
        d.can_have_blood = false;
        d.can_have_gore = false;
        d.matl_type = Matl::fluid;
        add_to_list_and_reset(d);

        d.id = terrain::Id::chasm;
        d.make_obj = [](const P& p) {
                return new terrain::Chasm(p);
        };
        d.character = '.';
        d.tile = gfx::TileId::floor;
        d.move_rules.props_allow_move.push_back(PropId::ethereal);
        d.move_rules.props_allow_move.push_back(PropId::flying);
        d.can_have_item = false;
        d.can_have_blood = false;
        d.can_have_gore = false;
        d.can_have_corpse = false;
        d.msg_on_player_blocked = "A chasm lies in my way.";
        d.msg_on_player_blocked_blind =
                "I realize I am standing on the edge of a chasm.";
        d.matl_type = Matl::empty;
        d.shock_when_adjacent = 3;
        add_to_list_and_reset(d);

        d.id = terrain::Id::gravestone;
        d.make_obj = [](const P& p) {
                return new terrain::GraveStone(p);
        };
        d.character = ']';
        d.tile = gfx::TileId::grave_stone;
        d.move_rules.props_allow_move.push_back(PropId::ethereal);
        d.move_rules.props_allow_move.push_back(PropId::flying);
        d.can_have_blood = false;
        d.can_have_gore = false;
        d.can_have_corpse = false;
        d.can_have_item = false;
        d.shock_when_adjacent = 2;
        d.matl_type = Matl::stone;
        add_to_list_and_reset(d);

        d.id = terrain::Id::church_bench;
        d.make_obj = [](const P& p) {
                return new terrain::ChurchBench(p);
        };
        d.character = '[';
        d.tile = gfx::TileId::church_bench;
        d.move_rules.props_allow_move.push_back(PropId::ethereal);
        d.move_rules.props_allow_move.push_back(PropId::flying);
        d.move_rules.props_allow_move.push_back(PropId::ooze);
        d.is_projectile_passable = false;
        d.is_los_passable = false;
        d.can_have_blood = false;
        d.can_have_gore = false;
        d.can_have_corpse = false;
        d.can_have_item = false;
        d.matl_type = Matl::wood;
        add_to_list_and_reset(d);

        d.id = terrain::Id::carpet;
        d.make_obj = [](const P& p) {
                return new terrain::Carpet(p);
        };
        d.character = '.';
        d.tile = gfx::TileId::floor;
        d.move_rules.is_walkable = true;
        d.is_floor_like = true;
        d.can_have_blood = true;
        d.can_have_gore = true;
        d.can_have_trap = true;
        d.matl_type = Matl::cloth;
        add_to_list_and_reset(d);

        d.id = terrain::Id::rubble_high;
        d.make_obj = [](const P& p) {
                return new terrain::RubbleHigh(p);
        };
        d.character = ';';
        d.tile = gfx::TileId::rubble_high;
        d.move_rules.props_allow_move.push_back(PropId::ethereal);
        d.move_rules.props_allow_move.push_back(PropId::ooze);
        d.move_rules.props_allow_move.push_back(PropId::burrowing);
        d.move_rules.props_allow_move.push_back(PropId::small_crawling);
        d.is_projectile_passable = false;
        d.is_los_passable = false;
        d.is_smoke_passable = false;
        d.can_have_blood = false;
        d.can_have_gore = false;
        d.can_have_corpse = false;
        d.can_have_item = false;
        d.matl_type = Matl::stone;
        add_to_list_and_reset(d);

        d.id = terrain::Id::rubble_low;
        d.make_obj = [](const P& p) {
                return new terrain::RubbleLow(p);
        };
        d.character = ',';
        d.tile = gfx::TileId::rubble_low;
        d.move_rules.is_walkable = true;
        d.is_floor_like = true;
        d.can_have_trap = false;
        d.matl_type = Matl::stone;
        d.auto_spawn_placement = terrain::TerrainPlacement::either;
        add_to_list_and_reset(d);

        d.id = terrain::Id::bones;
        d.make_obj = [](const P& p) {
                return new terrain::Bones(p);
        };
        d.character = '&';
        d.tile = gfx::TileId::corpse2;
        d.move_rules.is_walkable = true;
        d.is_floor_like = true;
        d.can_have_trap = true;
        d.matl_type = Matl::stone;
        d.auto_spawn_placement = terrain::TerrainPlacement::either;
        add_to_list_and_reset(d);

        d.id = terrain::Id::statue;
        d.make_obj = [](const P& p) {
                return new terrain::Statue(p);
        };
        d.character = '|';
        d.tile = gfx::TileId::witch_or_warlock;
        d.is_projectile_passable = false;
        d.is_los_passable = true;
        d.can_have_blood = false;
        d.can_have_gore = false;
        d.can_have_corpse = false;
        d.can_have_item = false;
        d.matl_type = Matl::stone;
        d.auto_spawn_placement = terrain::TerrainPlacement::away_from_walls;
        add_to_list_and_reset(d);

        d.id = terrain::Id::cocoon;
        d.make_obj = [](const P& p) {
                return new terrain::Cocoon(p);
        };
        d.character = '8';
        d.tile = gfx::TileId::cocoon_closed;
        d.is_projectile_passable = true;
        d.is_los_passable = false;
        d.can_have_blood = false;
        d.can_have_gore = false;
        d.can_have_corpse = false;
        d.can_have_item = false;
        d.shock_when_adjacent = 3;
        d.matl_type = Matl::cloth;
        d.auto_spawn_placement = terrain::TerrainPlacement::either;
        add_to_list_and_reset(d);

        d.id = terrain::Id::chest;
        d.make_obj = [](const P& p) {
                return new terrain::Chest(p);
        };
        d.character = '7';
        d.tile = gfx::TileId::chest_closed;
        d.can_have_blood = false;
        d.can_have_gore = false;
        d.can_have_corpse = false;
        d.can_have_item = false;
        d.auto_spawn_placement = terrain::TerrainPlacement::adj_to_walls;
        add_to_list_and_reset(d);

        d.id = terrain::Id::cabinet;
        d.make_obj = [](const P& p) {
                return new terrain::Cabinet(p);
        };
        d.character = '7';
        d.tile = gfx::TileId::cabinet_closed;
        d.is_projectile_passable = false;
        d.is_los_passable = false;
        d.can_have_blood = false;
        d.can_have_gore = false;
        d.can_have_corpse = false;
        d.can_have_item = false;
        d.matl_type = Matl::wood;
        d.auto_spawn_placement = terrain::TerrainPlacement::adj_to_walls;
        add_to_list_and_reset(d);

        d.id = terrain::Id::bookshelf;
        d.make_obj = [](const P& p) {
                return new terrain::Bookshelf(p);
        };
        d.character = '7';
        d.tile = gfx::TileId::bookshelf_full;
        d.is_projectile_passable = false;
        d.is_los_passable = false;
        d.can_have_blood = false;
        d.can_have_gore = false;
        d.can_have_corpse = false;
        d.can_have_item = false;
        d.matl_type = Matl::wood;
        d.auto_spawn_placement = terrain::TerrainPlacement::adj_to_walls;
        add_to_list_and_reset(d);

        d.id = terrain::Id::alchemist_bench;
        d.make_obj = [](const P& p) {
                return new terrain::AlchemistBench(p);
        };
        d.character = '7';
        d.tile = gfx::TileId::alchemist_bench_full;
        d.is_projectile_passable = false;
        d.is_los_passable = true;
        d.can_have_blood = false;
        d.can_have_gore = false;
        d.can_have_corpse = false;
        d.can_have_item = false;
        d.matl_type = Matl::wood;
        d.auto_spawn_placement = terrain::TerrainPlacement::either;
        add_to_list_and_reset(d);

        d.id = terrain::Id::fountain;
        d.make_obj = [](const P& p) {
                return new terrain::Fountain(p);
        };
        d.character = '1';
        d.tile = gfx::TileId::fountain;
        d.is_projectile_passable = false;
        d.is_los_passable = false;
        d.can_have_blood = false;
        d.can_have_gore = false;
        d.can_have_corpse = false;
        d.can_have_item = false;
        d.matl_type = Matl::stone;
        d.auto_spawn_placement = terrain::TerrainPlacement::away_from_walls;
        add_to_list_and_reset(d);

        d.id = terrain::Id::stalagmite;
        d.make_obj = [](const P& p) {
                return new terrain::Stalagmite(p);
        };
        d.character = ':';
        d.tile = gfx::TileId::stalagmite;
        d.is_projectile_passable = false;
        d.is_los_passable = false;
        d.can_have_blood = true;
        d.can_have_gore = false;
        d.can_have_corpse = false;
        d.can_have_item = false;
        d.matl_type = Matl::stone;
        d.auto_spawn_placement = terrain::TerrainPlacement::either;
        add_to_list_and_reset(d);

        d.id = terrain::Id::altar;
        d.make_obj = [](const P& p) {
                return new terrain::Altar(p);
        };
        d.character = '_';
        d.tile = gfx::TileId::altar;
        d.can_have_blood = false;
        d.can_have_gore = false;
        d.can_have_corpse = false;
        d.can_have_item = false;
        d.shock_when_adjacent = 10;
        d.matl_type = Matl::stone;
        d.auto_spawn_placement = terrain::TerrainPlacement::either;
        add_to_list_and_reset(d);

        d.id = terrain::Id::gong;
        d.make_obj = [](const P& p) {
                return new terrain::Gong(p);
        };
        d.character = '_';
        d.tile = gfx::TileId::gong;
        d.is_los_passable = true;
        d.is_projectile_passable = true;
        d.can_have_blood = false;
        d.can_have_gore = false;
        d.can_have_corpse = false;
        d.can_have_item = false;
        d.shock_when_adjacent = 5;
        d.matl_type = Matl::metal;
        d.auto_spawn_placement = terrain::TerrainPlacement::either;
        add_to_list_and_reset(d);

        d.id = terrain::Id::tomb;
        d.make_obj = [](const P& p) {
                return new terrain::Tomb(p);
        };
        d.character = '7';
        d.tile = gfx::TileId::tomb_closed;
        d.move_rules.props_allow_move.push_back(PropId::ethereal);
        d.move_rules.props_allow_move.push_back(PropId::flying);
        d.can_have_blood = false;
        d.can_have_gore = false;
        d.can_have_corpse = false;
        d.can_have_item = false;
        d.shock_when_adjacent = 10;
        d.matl_type = Matl::stone;
        d.auto_spawn_placement = terrain::TerrainPlacement::either;
        add_to_list_and_reset(d);

        d.id = terrain::Id::door;
        d.make_obj = [](const P& p) {
                return new terrain::Door(p);
        };
        d.can_have_blood = false;
        d.can_have_gore = false;
        d.can_have_corpse = false;
        d.can_have_trap = false;
        d.can_have_item = false;
        add_to_list_and_reset(d);

        d.id = terrain::Id::trap;
        d.make_obj = [](const P& p) {
                return new terrain::Trap(p);
        };
        d.move_rules.is_walkable = true;
        add_to_list_and_reset(d);

        // ---------------------------------------------------------------------
        // Mobile terrain
        // ---------------------------------------------------------------------
        d.id = terrain::Id::lit_dynamite;
        d.make_obj = [](const P& p) {
                return new terrain::LitDynamite(p);
        };
        d.character = '/';
        d.tile = gfx::TileId::dynamite_lit;
        d.move_rules.is_walkable = true;
        add_to_list_and_reset(d);

        d.id = terrain::Id::lit_flare;
        d.make_obj = [](const P& p) {
                return new terrain::LitFlare(p);
        };
        d.character = '/';
        d.tile = gfx::TileId::flare_lit;
        d.move_rules.is_walkable = true;
        add_to_list_and_reset(d);

        d.id = terrain::Id::smoke;
        d.make_obj = [](const P& p) {
                return new terrain::Smoke(p);
        };
        d.character = '*';
        d.tile = gfx::TileId::smoke;
        d.move_rules.is_walkable = true;
        d.is_los_passable = false;
        add_to_list_and_reset(d);

        d.id = terrain::Id::force_field;
        d.make_obj = [](const P& p) {
                return new terrain::ForceField(p);
        };
        d.character = '#';
        d.tile = gfx::TileId::square_checkered;
        d.move_rules.reset();
        d.is_sound_passable = false;
        d.is_projectile_passable = false;
        d.is_los_passable = false;
        d.is_smoke_passable = false;
        d.can_have_blood = false;
        d.can_have_gore = false;
        d.can_have_item = false;
        d.matl_type = Matl::metal;
        add_to_list_and_reset(d);

        d.id = terrain::Id::event_wall_crumble;
        d.make_obj = [](const P& p) {
                return new terrain::EventWallCrumble(p);
        };
        d.move_rules.is_walkable = true;
        add_to_list_and_reset(d);

        d.id = terrain::Id::event_snake_emerge;
        d.make_obj = [](const P& p) {
                return new terrain::EventSnakeEmerge(p);
        };
        d.move_rules.is_walkable = true;
        add_to_list_and_reset(d);

        d.id = terrain::Id::event_rat_cave_discovery;
        d.make_obj = [](const P& p) {
                return new terrain::EventRatsInTheWallsDiscovery(p);
        };
        d.move_rules.is_walkable = true;
        add_to_list_and_reset(d);
}

// -----------------------------------------------------------------------------
// terrain
// -----------------------------------------------------------------------------
namespace terrain
{
bool MoveRules::can_move(const actor::Actor& actor) const
{
        if (is_walkable)
        {
                return true;
        }

        // This terrain blocks walking, check if any property overrides this
        // (e.g. flying)

        const auto match = std::find_if(
                std::begin(props_allow_move),
                std::end(props_allow_move),
                [&actor](const PropId id) {
                        return actor.m_properties.has(id);
                });

        return match != std::end(props_allow_move);
}

void init()
{
        TRACE_FUNC_BEGIN;

        init_data_list();

        TRACE_FUNC_END;
}

const TerrainData& data(const Id id)
{
        ASSERT(id != terrain::Id::END);

        return s_data[int(id)];
}

}  // namespace terrain

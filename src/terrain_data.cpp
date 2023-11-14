// =============================================================================
// Copyright 2011-2023 Martin Törnqvist <m.tornq@gmail.com>
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
#include "terrain_mirror.hpp"
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
        d = {};
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
        d.character = '.';
        d.tile = gfx::TileId::floor;
        d.move_rules.is_walkable = true;
        d.is_floor_like = true;
        d.material_type = Material::stone;
        d.can_have_gore = true;
        d.can_have_trap = true;
        add_to_list_and_reset(d);

        d.id = terrain::Id::bridge;
        d.move_rules.is_walkable = true;
        d.material_type = Material::wood;
        add_to_list_and_reset(d);

        d.id = terrain::Id::wall;
        d.character = '#';  // NOTE: A filled rectangle may be used instead.
        d.tile = gfx::TileId::wall_top;
        d.move_rules.props_allow_move.push_back(prop::Id::ethereal);
        d.move_rules.props_allow_move.push_back(prop::Id::burrowing);
        d.is_sound_passable = false;
        d.is_projectile_passable = false;
        d.is_los_passable = false;
        d.is_smoke_passable = false;
        d.can_have_gore = false;
        d.can_have_corpse = false;
        d.can_have_item = false;
        d.material_type = Material::stone;
        // If walls are spawned automatically in rooms (e.g. to create a nice
        // pattern), do not spawn walls next to other walls.
        d.auto_spawn_placement = terrain::TerrainPlacement::away_from_walls;
        add_to_list_and_reset(d);

        d.id = terrain::Id::tree;
        d.character = '|';
        d.tile = gfx::TileId::tree;
        d.move_rules.props_allow_move.push_back(prop::Id::ethereal);
        d.is_sound_passable = false;
        d.is_projectile_passable = false;
        d.is_los_passable = false;
        d.can_have_blood = false;
        d.can_have_gore = false;
        d.can_have_corpse = false;
        d.can_have_item = false;
        d.material_type = Material::wood;
        d.shock_when_adjacent = 1;
        add_to_list_and_reset(d);

        d.id = terrain::Id::grass;
        d.character = '.';
        d.tile = gfx::TileId::floor;
        d.move_rules.is_walkable = true;
        d.is_floor_like = true;
        d.material_type = Material::plant;
        d.can_have_gore = true;
        d.can_have_trap = true;
        add_to_list_and_reset(d);

        d.id = terrain::Id::bush;
        d.character = '"';
        d.tile = gfx::TileId::bush;
        d.move_rules.is_walkable = true;
        d.is_los_passable = false;
        d.material_type = Material::plant;
        add_to_list_and_reset(d);

        d.id = terrain::Id::vines;
        d.character = '"';
        d.tile = gfx::TileId::vines;
        d.move_rules.is_walkable = true;
        d.is_los_passable = false;
        d.can_have_blood = false;
        d.can_have_gore = false;
        d.material_type = Material::plant;
        d.auto_spawn_placement = terrain::TerrainPlacement::either;
        add_to_list_and_reset(d);

        d.id = terrain::Id::chains;
        d.character = '"';
        d.tile = gfx::TileId::chains;
        d.move_rules.is_walkable = true;
        d.is_los_passable = true;
        d.is_projectile_passable = true;
        d.can_have_blood = true;
        d.material_type = Material::metal;
        d.auto_spawn_placement = terrain::TerrainPlacement::either;
        add_to_list_and_reset(d);

        d.id = terrain::Id::grate;
        d.character = ':';  // NOTE: '#' may be automatically used instead.
        d.tile = gfx::TileId::grate;
        d.move_rules.props_allow_move.push_back(prop::Id::ethereal);
        d.move_rules.props_allow_move.push_back(prop::Id::burrowing);
        d.move_rules.props_allow_move.push_back(prop::Id::ooze);
        d.move_rules.props_allow_move.push_back(prop::Id::small_crawling);
        d.move_rules.props_allow_move.push_back(prop::Id::tiny_flying);
        d.is_los_passable = true;
        d.can_have_blood = false;  // Looks weird
        d.can_have_gore = false;
        d.can_have_corpse = false;
        d.can_have_item = false;
        d.material_type = Material::metal;
        add_to_list_and_reset(d);

        d.id = terrain::Id::stairs;
        d.character = '>';
        d.tile = gfx::TileId::stairs_down;
        d.can_have_blood = false;
        d.can_have_gore = false;
        d.can_have_corpse = false;
        d.can_have_item = false;
        d.material_type = Material::stone;
        add_to_list_and_reset(d);

        d.id = terrain::Id::monolith;
        d.character = '|';
        d.tile = gfx::TileId::monolith;
        d.is_projectile_passable = false;
        d.is_los_passable = false;
        d.can_have_blood = false;  // We don't want to mess with the color
        d.can_have_gore = false;
        d.can_have_corpse = false;
        d.can_have_item = false;
        d.shock_when_adjacent = 10;
        d.material_type = Material::stone;
        add_to_list_and_reset(d);

        d.id = terrain::Id::mirror;
        d.character = '|';
        d.tile = gfx::TileId::mirror;
        d.is_projectile_passable = false;
        d.is_los_passable = false;
        d.can_have_blood = false;  // We don't want to mess with the color
        d.can_have_gore = false;
        d.can_have_corpse = false;
        d.can_have_item = false;
        d.shock_when_adjacent = 5;
        d.material_type = Material::metal;  // Close enough.
        add_to_list_and_reset(d);

        d.id = terrain::Id::pylon;
        d.character = '|';
        d.tile = gfx::TileId::END;  // This is set elsewhere
        d.is_projectile_passable = false;
        d.is_los_passable = true;
        d.can_have_blood = false;  // We don't want to mess with the color
        d.can_have_gore = false;
        d.can_have_corpse = false;
        d.can_have_item = false;
        d.shock_when_adjacent = 10;
        d.material_type = Material::metal;
        add_to_list_and_reset(d);

        d.id = terrain::Id::crystal_key;
        d.character = '%';
        d.tile = gfx::TileId::END;  // This is set elsewhere
        d.can_have_blood = false;
        d.can_have_gore = false;
        d.can_have_corpse = false;
        d.can_have_item = false;
        d.material_type = Material::stone;  // Whatever...
        d.shock_when_adjacent = 5;
        add_to_list_and_reset(d);

        d.id = terrain::Id::brazier;
        d.character = '0';
        d.tile = gfx::TileId::brazier;
        d.can_have_blood = false;
        d.can_have_gore = false;
        d.can_have_corpse = false;
        d.can_have_item = false;
        d.material_type = Material::metal;
        d.auto_spawn_placement = terrain::TerrainPlacement::away_from_walls;
        add_to_list_and_reset(d);

        d.id = terrain::Id::liquid;
        d.character = '~';
        d.tile = gfx::TileId::water;
        d.move_rules.is_walkable = true;
        d.can_have_blood = false;
        d.can_have_gore = false;
        d.material_type = Material::fluid;
        add_to_list_and_reset(d);

        d.id = terrain::Id::chasm;
        d.character = '.';
        d.tile = gfx::TileId::square_checkered;
        d.move_rules.props_allow_move.push_back(prop::Id::ethereal);
        d.move_rules.props_allow_move.push_back(prop::Id::flying);
        d.move_rules.props_allow_move.push_back(prop::Id::tiny_flying);
        d.can_have_item = false;
        d.can_have_blood = false;
        d.can_have_gore = false;
        d.can_have_corpse = false;
        d.msg_on_player_blocked =
                "A chasm lies in my way.";
        d.msg_on_player_blocked_blind =
                "I realize I am standing on the edge of a chasm.";
        d.material_type = Material::empty;
        d.shock_when_adjacent = 3;
        add_to_list_and_reset(d);

        d.id = terrain::Id::gravestone;
        d.character = ']';
        d.tile = gfx::TileId::grave_stone;
        d.move_rules.props_allow_move.push_back(prop::Id::ethereal);
        d.move_rules.props_allow_move.push_back(prop::Id::flying);
        d.move_rules.props_allow_move.push_back(prop::Id::tiny_flying);
        d.can_have_blood = false;
        d.can_have_gore = false;
        d.can_have_corpse = false;
        d.can_have_item = false;
        d.shock_when_adjacent = 2;
        d.material_type = Material::stone;
        add_to_list_and_reset(d);

        d.id = terrain::Id::church_bench;
        d.character = '[';
        d.tile = gfx::TileId::church_bench;
        d.move_rules.props_allow_move.push_back(prop::Id::ethereal);
        d.move_rules.props_allow_move.push_back(prop::Id::flying);
        d.move_rules.props_allow_move.push_back(prop::Id::tiny_flying);
        d.move_rules.props_allow_move.push_back(prop::Id::ooze);
        d.is_projectile_passable = false;
        d.is_los_passable = false;
        d.can_have_blood = false;
        d.can_have_gore = false;
        d.can_have_corpse = false;
        d.can_have_item = false;
        d.material_type = Material::wood;
        add_to_list_and_reset(d);

        d.id = terrain::Id::carpet;
        d.character = '.';
        d.tile = gfx::TileId::floor;
        d.move_rules.is_walkable = true;
        d.is_floor_like = true;
        d.can_have_blood = true;
        d.can_have_gore = true;
        d.can_have_trap = true;
        d.material_type = Material::cloth;
        add_to_list_and_reset(d);

        d.id = terrain::Id::rubble_high;
        d.character = ';';
        d.tile = gfx::TileId::rubble_high;
        d.move_rules.props_allow_move.push_back(prop::Id::ethereal);
        d.move_rules.props_allow_move.push_back(prop::Id::ooze);
        d.move_rules.props_allow_move.push_back(prop::Id::burrowing);
        d.move_rules.props_allow_move.push_back(prop::Id::small_crawling);
        d.move_rules.props_allow_move.push_back(prop::Id::tiny_flying);
        d.is_projectile_passable = false;
        d.is_los_passable = false;
        d.is_smoke_passable = false;
        d.can_have_blood = false;
        d.can_have_gore = false;
        d.can_have_corpse = false;
        d.can_have_item = false;
        d.material_type = Material::stone;
        add_to_list_and_reset(d);

        d.id = terrain::Id::rubble_low;
        d.character = ',';
        d.tile = gfx::TileId::rubble_low;
        d.move_rules.is_walkable = true;
        d.is_floor_like = true;
        d.can_have_trap = false;
        d.material_type = Material::stone;
        d.auto_spawn_placement = terrain::TerrainPlacement::either;
        add_to_list_and_reset(d);

        d.id = terrain::Id::bones;
        d.character = '&';
        d.tile = gfx::TileId::corpse2;
        d.move_rules.is_walkable = true;
        d.is_floor_like = true;
        d.can_have_trap = true;
        d.material_type = Material::stone;
        d.auto_spawn_placement = terrain::TerrainPlacement::either;
        add_to_list_and_reset(d);

        d.id = terrain::Id::statue;
        d.character = '|';
        d.tile = gfx::TileId::witch_or_warlock;
        d.is_projectile_passable = false;
        d.is_los_passable = true;
        d.can_have_blood = false;
        d.can_have_gore = false;
        d.can_have_corpse = false;
        d.can_have_item = false;
        d.material_type = Material::stone;
        d.auto_spawn_placement = terrain::TerrainPlacement::away_from_walls;
        add_to_list_and_reset(d);

        d.id = terrain::Id::cocoon;
        d.character = '8';
        d.tile = gfx::TileId::cocoon_closed;
        d.is_projectile_passable = true;
        d.is_los_passable = false;
        d.can_have_blood = false;
        d.can_have_gore = false;
        d.can_have_corpse = false;
        d.can_have_item = false;
        d.shock_when_adjacent = 3;
        d.material_type = Material::cloth;
        d.auto_spawn_placement = terrain::TerrainPlacement::either;
        add_to_list_and_reset(d);

        d.id = terrain::Id::chest;
        d.character = '7';
        d.tile = gfx::TileId::chest_closed;
        d.can_have_blood = false;
        d.can_have_gore = false;
        d.can_have_corpse = false;
        d.can_have_item = false;
        d.auto_spawn_placement = terrain::TerrainPlacement::adj_to_walls;
        add_to_list_and_reset(d);

        d.id = terrain::Id::cabinet;
        d.character = '7';
        d.tile = gfx::TileId::cabinet_closed;
        d.is_projectile_passable = false;
        d.is_los_passable = false;
        d.can_have_blood = false;
        d.can_have_gore = false;
        d.can_have_corpse = false;
        d.can_have_item = false;
        d.material_type = Material::wood;
        d.auto_spawn_placement = terrain::TerrainPlacement::adj_to_walls;
        add_to_list_and_reset(d);

        d.id = terrain::Id::bookshelf;
        d.character = '7';
        d.tile = gfx::TileId::bookshelf_full;
        d.is_projectile_passable = false;
        d.is_los_passable = false;
        d.can_have_blood = false;
        d.can_have_gore = false;
        d.can_have_corpse = false;
        d.can_have_item = false;
        d.material_type = Material::wood;
        d.auto_spawn_placement = terrain::TerrainPlacement::adj_to_walls;
        add_to_list_and_reset(d);

        d.id = terrain::Id::alchemist_bench;
        d.character = '7';
        d.tile = gfx::TileId::alchemist_bench_full;
        d.is_projectile_passable = false;
        d.is_los_passable = true;
        d.can_have_blood = false;
        d.can_have_gore = false;
        d.can_have_corpse = false;
        d.can_have_item = false;
        d.material_type = Material::wood;
        d.auto_spawn_placement = terrain::TerrainPlacement::either;
        add_to_list_and_reset(d);

        d.id = terrain::Id::fountain;
        d.character = '1';
        d.tile = gfx::TileId::fountain;
        d.is_projectile_passable = false;
        d.is_los_passable = false;
        d.can_have_blood = false;
        d.can_have_gore = false;
        d.can_have_corpse = false;
        d.can_have_item = false;
        d.material_type = Material::stone;
        d.auto_spawn_placement = terrain::TerrainPlacement::away_from_walls;
        add_to_list_and_reset(d);

        d.id = terrain::Id::stalagmite;
        d.character = ':';
        d.tile = gfx::TileId::stalagmite;
        d.is_projectile_passable = false;
        d.is_los_passable = false;
        d.can_have_blood = true;
        d.can_have_gore = false;
        d.can_have_corpse = false;
        d.can_have_item = false;
        d.material_type = Material::stone;
        d.auto_spawn_placement = terrain::TerrainPlacement::either;
        add_to_list_and_reset(d);

        d.id = terrain::Id::altar;
        d.character = '_';
        d.tile = gfx::TileId::altar;
        d.can_have_blood = false;
        d.can_have_gore = false;
        d.can_have_corpse = false;
        d.can_have_item = false;
        d.shock_when_adjacent = 10;
        d.material_type = Material::stone;
        d.auto_spawn_placement = terrain::TerrainPlacement::either;
        add_to_list_and_reset(d);

        d.id = terrain::Id::gong;
        d.character = '_';
        d.tile = gfx::TileId::gong;
        d.is_los_passable = true;
        d.is_projectile_passable = true;
        d.can_have_blood = false;
        d.can_have_gore = false;
        d.can_have_corpse = false;
        d.can_have_item = false;
        d.shock_when_adjacent = 5;
        d.material_type = Material::metal;
        d.auto_spawn_placement = terrain::TerrainPlacement::either;
        add_to_list_and_reset(d);

        d.id = terrain::Id::tomb;
        d.character = '7';
        d.tile = gfx::TileId::tomb_closed;
        d.move_rules.props_allow_move.push_back(prop::Id::ethereal);
        d.move_rules.props_allow_move.push_back(prop::Id::flying);
        d.move_rules.props_allow_move.push_back(prop::Id::tiny_flying);
        d.can_have_blood = false;
        d.can_have_gore = false;
        d.can_have_corpse = false;
        d.can_have_item = false;
        d.shock_when_adjacent = 10;
        d.material_type = Material::stone;
        d.auto_spawn_placement = terrain::TerrainPlacement::either;
        add_to_list_and_reset(d);

        d.id = terrain::Id::door;
        d.can_have_blood = false;
        d.can_have_gore = false;
        d.can_have_corpse = false;
        d.can_have_trap = false;
        d.can_have_item = false;
        add_to_list_and_reset(d);

        d.id = terrain::Id::trap;
        d.move_rules.is_walkable = true;
        add_to_list_and_reset(d);

        // ---------------------------------------------------------------------
        // Mobile terrain
        // ---------------------------------------------------------------------
        d.id = terrain::Id::lit_dynamite;
        d.character = '/';
        d.tile = gfx::TileId::dynamite_lit;
        d.move_rules.is_walkable = true;
        add_to_list_and_reset(d);

        d.id = terrain::Id::lit_flare;
        d.character = '/';
        d.tile = gfx::TileId::flare_lit;
        d.move_rules.is_walkable = true;
        add_to_list_and_reset(d);

        d.id = terrain::Id::smoke;
        d.character = '*';
        d.tile = gfx::TileId::smoke;
        d.move_rules.is_walkable = true;
        d.is_los_passable = false;
        add_to_list_and_reset(d);

        d.id = terrain::Id::force_field;
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
        d.material_type = Material::metal;
        add_to_list_and_reset(d);

        d.id = terrain::Id::event_wall_crumble;
        d.move_rules.is_walkable = true;
        add_to_list_and_reset(d);

        d.id = terrain::Id::event_snake_emerge;
        d.move_rules.is_walkable = true;
        add_to_list_and_reset(d);

        d.id = terrain::Id::event_spawn_monsters_delayed;
        d.move_rules.is_walkable = true;
        add_to_list_and_reset(d);

        d.id = terrain::Id::event_rat_cave_discovery;
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
        if (is_walkable) {
                return true;
        }

        // This terrain blocks walking, check if any property overrides this
        // (e.g. flying)

        const auto match =
                std::find_if(
                        std::begin(props_allow_move),
                        std::end(props_allow_move),
                        [&actor](const prop::Id id) {
                                return actor.m_properties.has(id);
                        });

        return match != std::end(props_allow_move);
}

bool MoveRules::is_property_allowing_move(const prop::Id id) const
{
        return (
                std::find(
                        std::begin(props_allow_move),
                        std::end(props_allow_move),
                        id) != std::end(props_allow_move));
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

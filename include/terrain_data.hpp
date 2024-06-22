// =============================================================================
// Copyright 2011-2023 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef TERRAIN_DATA_HPP
#define TERRAIN_DATA_HPP

#include <functional>
#include <string>
#include <vector>

#include "gfx.hpp"
#include "global.hpp"
#include "property_data.hpp"

struct P;

namespace actor
{
class Actor;
}  // namespace actor

namespace terrain
{
class Terrain;

enum class Id
{
        floor,
        bridge,
        wall,
        pillar,
        petroglyph,
        tree,
        grass,
        bush,
        vines,
        chains,
        grate,
        stairs,
        crystal_key,
        brazier,
        gravestone,
        tomb,
        church_bench,
        altar,
        gong,
        carpet,
        rubble_high,
        rubble_low,
        bones,
        statue,
        urn,
        cocoon,
        chest,
        cabinet,
        bookshelf,
        alchemist_bench,
        fountain,
        monolith,
        mirror,
        pylon,
        stalagmite,
        chasm,
        liquid,
        door,
        lit_dynamite,
        lit_flare,
        trap,
        smoke,
        force_field,
        event_wall_crumble,
        event_snake_emerge,
        event_spawn_monsters_delayed,
        event_rat_cave_discovery,

        END
};

enum class TerrainPlacement
{
        adj_to_walls,
        away_from_walls,
        either
};

struct MoveRules
{
        void reset()
        {
                is_walkable = false;
                props_allow_move.clear();
        }

        bool can_move(const actor::Actor& actor) const;

        // Is this given property allowing movement into this terrain, when it
        // normally wouldn't be?
        bool is_property_allowing_move(prop::Id id) const;

        bool is_walkable {false};
        std::vector<prop::Id> props_allow_move {};
};

struct TerrainData
{
        Id id {Id::END};
        char character {'x'};
        gfx::TileId tile {gfx::TileId::END};
        MoveRules move_rules {};
        bool is_sound_passable {true};
        bool is_projectile_passable {true};
        bool is_los_passable {true};
        bool is_smoke_passable {true};
        bool is_floor_like {false};
        bool can_have_blood {true};
        bool can_have_gore {false};
        bool can_have_corpse {true};
        bool can_have_trap {false};
        bool can_have_item {true};
        Material material_type {Material::stone};
        std::string msg_on_player_blocked {"The way is blocked."};
        std::string msg_on_player_blocked_blind {"I bump into something."};
        int shock_when_adjacent {0};
        TerrainPlacement auto_spawn_placement {TerrainPlacement::either};
};

void init();

const TerrainData& data(Id id);

}  // namespace terrain

#endif  // TERRAIN_DATA_HPP

// =============================================================================
// Copyright 2011-2024 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef MAP_HPP
#define MAP_HPP

#include <cstddef>
#include <vector>

#include "array2.hpp"
#include "colors.hpp"
#include "gfx.hpp"
#include "global.hpp"
#include "pos.hpp"
#include "rect.hpp"
#include "room.hpp"

namespace smell
{
struct Smell;
}  // namespace smell

struct LosResult;

namespace item
{
class Item;
enum class Id;
}  // namespace item

namespace actor
{
class Actor;
}  // namespace actor

namespace terrain
{
class Terrain;
enum class Id;
}  // namespace terrain

namespace room
{
class Room;
}  // namespace room

struct ChokePointData
{
        ChokePointData() = default;
        ChokePointData(const ChokePointData& other);
        ChokePointData& operator=(const ChokePointData& other);

        P p {};

        // These shall only ever have a value of 0 or 1 (or -1 when undefined)
        int player_side {-1};
        int stairs_side {-1};

        std::vector<P> sides[2] {};
};

namespace map
{
enum class MinimapSymbol
{
        rectangle_filled,
        rectangle_filled_small,
        rectangle_edge,
};

struct MinimapAppearance
{
        Color color {};
        std::string legend_text;
        MinimapSymbol symbol {MinimapSymbol::rectangle_filled};
};

struct PlayerMemoryAppearance
{
        bool is_defined() const
        {
                return tile != gfx::TileId::END;
        }

        gfx::TileId tile {gfx::TileId::END};
        char character {0};
        std::string name;
        Color color {};

        MinimapAppearance minimap {};
};

struct PlayerMemoryTerrain
{
        PlayerMemoryAppearance appearance {};
        terrain::Id id {(terrain::Id)0};
        bool blocks_walking {false};
};

struct PlayerMemoryItem
{
        PlayerMemoryAppearance appearance {};
        item::Id id {(item::Id)0};
};

extern Array2<bool> g_seen;
extern Array2<LosResult> g_los;
extern Array2<bool> g_light;
extern Array2<bool> g_dark;
extern Array2<smell::Smell> g_smell;
extern Array2<smell::Smell> g_smell_spread;
extern Array2<item::Item*> g_items;
extern Array2<PlayerMemoryItem> g_item_memory;
extern Array2<terrain::Terrain*> g_terrain;
extern Array2<PlayerMemoryTerrain> g_terrain_memory;

extern actor::Actor* g_player;

extern int g_dlvl;

extern Color g_wall_color;

// This vector is the room memory owner
extern std::vector<room::Room*> g_room_list;

// Helper array, for convenience and optimization
extern Array2<room::Room*> g_room_map;

// NOTE: This data is only intended to be used for the purpose of map generation
// (and placing items etc), it is NOT updated while playing the map.
extern std::vector<ChokePointData> g_chokepoint_data;

void init();
void cleanup();
void save();
void load();
void reset(const P& dims);

int w();
int h();
const P& dims();
R rect();
std::vector<P> positions();
size_t nr_positions();

// Updates light map, player fov (etc).
void update_vision();

void update_player_memory();

// Sets a new terrain object and updates map information (e.g. which positions
// are blocked). This should always be used when changing terrain while a map is
// played (e.g. on terrain destruction).
void update_terrain(terrain::Terrain* terrain);

// This merely sets a new terrain object. It should mainly be used during map
// generation.
void set_terrain(terrain::Terrain* terrain);

void memorize_terrain_at(const P& p);
void memorize_item_at(const P& p);

void clear_player_memory_at(const P& p);

void update_light_map();

void delete_and_remove_room_from_list(room::Room* room);

actor::Actor* living_actor_at(const P& pos);
actor::Actor* first_corpse_at(const P& pos);

terrain::Terrain* first_mob_at_pos(const P& pos);

actor::Actor* random_closest_actor(
        const P& c,
        const std::vector<actor::Actor*>& actors);

bool is_pos_inside_map(const P& pos);
bool is_pos_inside_outer_walls(const P& pos);
bool is_area_inside_map(const R& area);

}  // namespace map

#endif  // MAP_HPP

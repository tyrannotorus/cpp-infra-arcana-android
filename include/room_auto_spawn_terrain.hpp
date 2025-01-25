// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef ROOM_AUTO_SPAWN_TERRAIN_HPP
#define ROOM_AUTO_SPAWN_TERRAIN_HPP

#include <vector>

#include "pos.hpp"
#include "room.hpp"
#include "terrain_data.hpp"

namespace room
{
void place_auto_terrains(const Room& room);

P find_auto_terrain_placement(
        const std::vector<P>& adj_to_walls,
        const std::vector<P>& away_from_walls,
        terrain::Id id);

}  // namespace room

#endif  // ROOM_AUTO_SPAWN_TERRAIN_HPP

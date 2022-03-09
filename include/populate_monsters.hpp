// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef POPULATE_MONSTERS_HPP
#define POPULATE_MONSTERS_HPP

#include <vector>

#include "global.hpp"
#include "room.hpp"

namespace actor
{
enum class Id;
}  // namespace actor

struct P;

template <typename T>
class Array2;

namespace populate_mon
{
void make_group_at(
        actor::Id id,
        const std::vector<P>& sorted_free_cells,
        Array2<bool>* blocked_out,
        MonRoamingAllowed roaming_allowed);

std::vector<P> make_sorted_free_cells(
        const P& origin,
        const Array2<bool>& blocked);

// Unwalkable terrain or too close to the player
Array2<bool> forbidden_spawn_positions();

void spawn_for_repopulate_over_time();

void populate_std_lvl();

// Convenient function for special levels which should auto-spawn monsters
// instead of according to design (or in addition to hand-placed monsters)
void populate_lvl_as_room_types(const std::vector<RoomType>& room_types);

}  // namespace populate_mon

#endif  // POPULATE_MONSTERS_HPP

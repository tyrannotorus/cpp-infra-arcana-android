// =============================================================================
// Copyright 2011-2021 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef MAP_TRAVEL_HPP
#define MAP_TRAVEL_HPP



namespace map_travel
{
void init();

void save();
void load();

void try_use_down_stairs();

void go_to_nxt();

}  // namespace map_travel

#endif  // MAP_TRAVEL_HPP

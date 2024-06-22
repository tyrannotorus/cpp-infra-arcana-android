// =============================================================================
// Copyright 2011-2024 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef AI_HPP
#define AI_HPP

#include <vector>

#include "global.hpp"

template <typename T>
class Array2;

namespace actor
{
class Actor;
}  // namespace actor

struct P;

namespace ai
{
// -----------------------------------------------------------------------------
// Things that cost turns for the monster
// -----------------------------------------------------------------------------
namespace action
{
DidAction try_cast_random_spell(actor::Actor& mon);

DidAction handle_closed_blocking_door(actor::Actor& mon, std::vector<P>& path);

DidAction handle_inventory(actor::Actor& mon);

DidAction make_room_for_friend(actor::Actor& mon);

DidAction move_to_random_adj_cell(actor::Actor& mon);

DidAction move_to_target_simple(actor::Actor& mon);

DidAction step_path(actor::Actor& mon, const std::vector<P>& path);

DidAction step_to_lair_if_los(actor::Actor& mon, const P& lair_p);

}  // namespace action

// -----------------------------------------------------------------------------
// Information gathering
// -----------------------------------------------------------------------------
namespace info
{
bool look(actor::Actor& mon);

std::vector<P> find_path_to_lair_if_no_los(actor::Actor& mon, const P& lair_p);

std::vector<P> find_path_to_leader(actor::Actor& mon);

std::vector<P> find_path_to_target(actor::Actor& mon);

void set_special_blocked_cells(actor::Actor& mon, Array2<bool>& a);

}  // namespace info
}  // namespace ai

#endif  // AI_HPP

// =============================================================================
// Copyright 2011-2021 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef GAME_TIME_HPP
#define GAME_TIME_HPP

#include <vector>

struct P;

namespace actor
{
class Actor;
}  // namespace actor

namespace terrain
{
class Terrain;
}  // namespace terrain

namespace game_time
{
extern std::vector<actor::Actor*> g_actors;
extern std::vector<terrain::Terrain*> g_mobs;

extern bool g_is_magic_descend_nxt_std_turn;
extern bool g_is_player_acting;

// Used for checking if game ticked multiple times during the same action.
extern bool g_allow_tick;

void init();
void cleanup();

void save();
void load();

void add_actor(actor::Actor* actor);

void tick();

int turn_nr();

actor::Actor* current_actor();

std::vector<terrain::Terrain*> mobs_at_pos(const P& p);

void add_mob(terrain::Terrain* t);

void erase_mob(terrain::Terrain* f, bool destroy_object);

void erase_all_mobs();

void reset_current_actor_idx();

void update_light_map();

}  // namespace game_time

#endif  // GAME_TIME_HPP

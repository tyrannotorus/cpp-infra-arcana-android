// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
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
// TODO: These should be in the map namespace instead (however consider what
// functions in the map namespace such as init/reset should do and how they
// should be named):
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

void erase_all_destroyed_actors();

std::vector<terrain::Terrain*> mobs_at(const P& p);

void add_mob(terrain::Terrain* terrain);

void erase_mob(const terrain::Terrain* terrain, bool destroy_object);

void erase_all_mobs();

void reset_current_actor_idx();

}  // namespace game_time

#endif  // GAME_TIME_HPP

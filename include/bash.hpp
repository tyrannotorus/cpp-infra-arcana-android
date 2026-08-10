// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef BASH_HPP
#define BASH_HPP

struct P;

namespace item
{
class Item;
}  // namespace item

namespace actor
{
class Actor;
}  // namespace actor

namespace terrain
{
class Terrain;
}  // namespace terrain

namespace bash
{
// Query player for direction
void run();

// Bash whatever is at the given position (monster, corpse, terrain) with
// no direction query - the contextual [ kick ] look-pin button (see
// GameState::on_map_panned and GameCmd::kick_at_look_pin)
void run_at(const P& pos);

// Whether a kick could do anything to the terrain - the predicate behind
// the contextual [ kick ] look-pin button (see GameState::on_map_panned)
bool is_kickable_terrain(const terrain::Terrain& terrain);

void try_sprain_player();

void attack_terrain(const P& att_pos, const item::Item& wpn);

void attack_corpse(actor::Actor& mon, const item::Item& wpn);

// Finds a corpse to bash at the given position. If there are multiple corpses,
// the first corpse found with the "prioritize corpse bash" property is returned
// (otherwise the last corpse found is returned).
actor::Actor* get_corpse_to_bash_at(const P& pos);

// Print a message as if the player is attacking the unseen terrain, but no
// attack is actually performed. This is used when the player attacks unseen
// terrain that is not possible to attack with the wielded weapon or kick.
void do_fake_attack_on_unseen_terrain(const P& pos, const item::Item& wpn);

// Player kicks or strikes into open terrain, not hitting anything.
void attack_air();

bool is_allowed_use_wpn_on_terrain(
        const item::Item& wpn,
        const terrain::Terrain& terrain);

bool is_open_terrain(const terrain::Terrain& terrain);

}  // namespace bash

#endif  // BASH_HPP

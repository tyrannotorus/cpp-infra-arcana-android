// =============================================================================
// Copyright 2011-2023 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef ATTACK_HPP
#define ATTACK_HPP

#include "global.hpp"
#include "query.hpp"

namespace item
{
class Wpn;
}  // namespace item

namespace actor
{
class Actor;
}  // namespace actor

struct P;

namespace attack
{
void melee(
        actor::Actor* attacker,
        const P& origin,
        const P& aim_pos,
        item::Wpn& wpn);

DidAction ranged(
        actor::Actor* attacker,
        const P& origin,
        const P& aim_pos,
        item::Wpn& wpn);

void ranged_hit_chance(
        const actor::Actor& attacker,
        const actor::Actor& defender,
        const item::Wpn& wpn);

// TODO: Perhaps not the best place for this function, but it's used from
// several places.
BinaryAnswer query_player_attack_mon_with_ranged_wpn(
        const item::Wpn& wpn,
        const actor::Actor& mon);

}  // namespace attack

#endif  // ATTACK_HPP

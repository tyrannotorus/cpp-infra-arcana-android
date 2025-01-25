// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef ATTACK_INTERNAL_HPP
#define ATTACK_INTERNAL_HPP

#include <string>

#include "global.hpp"

struct ItemAttackProp;

namespace actor
{
class Actor;
}  // namespace actor

namespace terrain
{
class Terrain;
}  // namespace terrain

namespace attack
{
enum class HitSize
{
        small,
        medium,
        hard
};

HitSize relative_hit_size(int dmg, int wpn_max_dmg);

std::string hit_size_punctuation_str(HitSize hit_size);

void try_apply_attack_property_on_actor(
        const ItemAttackProp& att_prop,
        actor::Actor& actor,
        DmgType dmg_type);

}  // namespace attack

#endif  // ATTACK_INTERNAL_HPP

// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef TERRAIN_DMG_HPP
#define TERRAIN_DMG_HPP

struct P;

namespace terrain
{
// NOTE: Functions here here shall take a position by value, not by reference
// (since the original position will typically be a member variable for some
// terrain object that gets destroyed).

void destr_all_adj_doors(P p);

void destr_stone_wall(P p);

}  // namespace terrain

#endif

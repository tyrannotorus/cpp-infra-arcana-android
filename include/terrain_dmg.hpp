#ifndef TERRAIN_DMG_HPP
#define TERRAIN_DMG_HPP

struct P;
// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

namespace terrain
{
void destr_all_adj_doors(const P& p);

void destr_stone_wall(const P& p);

}  // namespace terrain

#endif

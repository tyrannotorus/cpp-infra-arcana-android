// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef TERRAIN_FACTORY_HPP
#define TERRAIN_FACTORY_HPP

struct P;

namespace terrain
{
class Terrain;
enum class Id;

Terrain* make(Id id, const P& pos);

}  // namespace terrain

#endif  // TERRAIN_FACTORY_HPP

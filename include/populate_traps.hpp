// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef POPULATE_TRAPS_HPP
#define POPULATE_TRAPS_HPP

namespace terrain
{
class Trap;

enum class TrapId;
}  // namespace terrain

struct P;

namespace populate_traps
{
void populate();

terrain::Trap* try_make_trap(terrain::TrapId id, const P& pos);

}  // namespace populate_traps

#endif  // POPULATE_TRAPS_HPP

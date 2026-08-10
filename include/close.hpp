// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef CLOSE_HPP
#define CLOSE_HPP

struct P;

namespace close
{
// Closes/jams via the look pin when it rests on an adjacent cell,
// otherwise asks for a direction
void player_try_close_or_jam();

// Closes or jams the terrain at the given (adjacent) map position
void player_try_close_or_jam_at(const P& pos);

}  // namespace close

#endif  // CLOSE_HPP

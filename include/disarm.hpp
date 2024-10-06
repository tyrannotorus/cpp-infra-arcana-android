// =============================================================================
// Copyright 2011-2024 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef DISARM_HPP
#define DISARM_HPP

struct P;

namespace disarm
{
void player_disarm();

void player_disarm_at_pos(const P& pos);

}  // namespace disarm

#endif

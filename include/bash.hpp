// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef BASH_HPP
#define BASH_HPP

struct P;

namespace bash
{
void try_sprain_player();

// Query player for direction
void run();

void bash_terrain_at_pos(const P& pos);

}  // namespace bash

#endif  // BASH_HPP

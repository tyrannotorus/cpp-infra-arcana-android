// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef DRAW_BLAST_HPP
#define DRAW_BLAST_HPP

#include <vector>

#include "colors.hpp"
#include "pos.hpp"

namespace actor
{
class Actor;
}  // namespace actor

void draw_blast_at_cells(
        const std::vector<P>& positions,
        const Color& color);

void draw_blast_at_seen_cells(
        const std::vector<P>& positions,
        const Color& color);

void draw_blast_at_seen_actors(
        const std::vector<actor::Actor*>& actors,
        const Color& color);

#endif  // DRAW_BLAST_HPP

// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef VIEWPORT_HPP
#define VIEWPORT_HPP

struct P;
struct R;

namespace viewport
{
enum class ForceCentering
{
        no,
        yes
};

R get_map_view_area();

// NOTE: This function does not necessarily center the map on the given position
// (unless force_centering is true). It only guarantees that the position will
// be visible in the viewport.
void show(P map_pos, ForceCentering force_centering);

bool is_in_view(P map_pos);

P to_view_pos(P map_pos);

P to_map_pos(P view_pos);

}  // namespace viewport

#endif  // VIEWPORT_HPP

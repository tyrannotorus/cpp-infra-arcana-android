// =============================================================================
// Copyright 2011-2023 Martin Törnqvist <m.tornq@gmail.com>
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
void show(
        const P& map_pos,
        ForceCentering force_centering = ForceCentering::no);

bool is_in_view(const P& map_pos);

P to_view_pos(const P& map_pos);

P to_map_pos(const P& view_pos);

}  // namespace viewport

#endif  // VIEWPORT_HPP

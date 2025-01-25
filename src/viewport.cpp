// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "viewport.hpp"

#include <algorithm>

#include "config.hpp"
#include "global.hpp"
#include "io.hpp"
#include "panel.hpp"
#include "pos.hpp"
#include "rect.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static P s_p0;

// Always move viewport if focused position is closer to the edge than this:
constexpr static int s_min_required_viewport_edge_dist = g_fov_radi_int + 1;

static P get_view_dims()
{
        const auto map_panel_gui_dims = panels::dims(Panel::map);

        return io::gui_to_map_coords(map_panel_gui_dims);
}

// -----------------------------------------------------------------------------
// viewport
// -----------------------------------------------------------------------------
namespace viewport
{
R get_map_view_area()
{
        auto view_dims = get_view_dims();

        const R map_area(s_p0, s_p0 + view_dims - 1);

        return map_area;
}

void show(const P& map_pos, const ForceCentering force_centering)
{
        const auto map_view_area = get_map_view_area();

        const auto smallest_dist_hor =
                std::min(
                        map_pos.x - map_view_area.p0.x,
                        map_view_area.p1.x - map_pos.x);

        const auto smallest_dist_ver =
                std::min(
                        map_pos.y - map_view_area.p0.y,
                        map_view_area.p1.y - map_pos.y);

        const auto view_dims = get_view_dims();

        const auto centered_pos = map_pos - view_dims.scaled_down(2);

        const P p0_before = s_p0;

        if (config::always_center_view_on_player() ||
            (force_centering == ForceCentering::yes)) {
                // Always center the view (both X/Y axis)
                s_p0 = centered_pos;
        }
        else {
                // Only center if needed (for X/Y axis separately)
                if (smallest_dist_hor < s_min_required_viewport_edge_dist) {
                        s_p0.x = centered_pos.x;
                }

                if (smallest_dist_ver < s_min_required_viewport_edge_dist) {
                        s_p0.y = centered_pos.y;
                }
        }

        if (s_p0 != p0_before) {
                io::clear_all_flash_animations();
        }
}

bool is_in_view(const P& map_pos)
{
        return get_map_view_area().is_pos_inside(map_pos);
}

P to_view_pos(const P& map_pos)
{
        return map_pos - s_p0;
}

P to_map_pos(const P& view_pos)
{
        return view_pos + s_p0;
}

}  // namespace viewport

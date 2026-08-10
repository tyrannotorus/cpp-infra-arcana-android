// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "draw_health_bars.hpp"

#include <algorithm>
#include <vector>

#include "actor.hpp"
#include "actor_see.hpp"
#include "colors.hpp"
#include "config.hpp"
#include "game_time.hpp"
#include "io.hpp"
#include "io_display.hpp"
#include "panel.hpp"
#include "pos.hpp"
#include "rect.hpp"
#include "viewport.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static int health_bar_length(const actor::Actor& actor)
{
        const int actor_hp = std::max(0, actor.m_hp);

        const int actor_hp_max = actor::max_hp(actor);

        if (actor_hp < actor_hp_max) {
                int hp_percent = (actor_hp * 100) / actor_hp_max;

                return ((config::map_cell_px_w() - 2) * hp_percent) / 100;
        }

        return -1;
}

static void draw_health_bar(const actor::Actor& actor)
{
        const int length = health_bar_length(actor);

        if (length < 0) {
                return;
        }

        const P map_pos = actor.m_pos.with_y_offset(1);

        if (!viewport::is_in_view(map_pos)) {
                return;
        }

        const P cell_dims(config::map_cell_px_w(), config::map_cell_px_h());

        const int w_green = length;
        const int w_bar_tot = cell_dims.x - 2;
        const int w_red = w_bar_tot - w_green;

        const auto view_pos = viewport::to_view_pos(map_pos);

        P px_pos = io::map_to_px_coords(Panel::map, view_pos);

        px_pos.y -= 2;

        const int x0_green = px_pos.x + 1;
        const int x0_red = x0_green + w_green;

        if (w_green > 0) {
                const P px_p0_green(x0_green, px_pos.y);

                const R px_rect_green(
                        px_p0_green,
                        px_p0_green + P(w_green, 2) - 1);

                io::draw_rectangle_filled(
                        px_rect_green,
                        colors::light_green());
        }

        if (w_red > 0) {
                const P px_p0_red(x0_red, px_pos.y);

                const R px_rect_red(
                        px_p0_red,
                        px_p0_red + P(w_red, 2) - 1);

                io::draw_rectangle_filled(
                        px_rect_red,
                        colors::light_red());
        }
}

// -----------------------------------------------------------------------------
// Public
// -----------------------------------------------------------------------------
void draw_health_bars()
{
        io::set_display_for_panel(Panel::map);

        for (auto* actor : game_time::g_actors) {
                if (actor::is_alive(*actor) && can_player_see_actor(*actor)) {
                        draw_health_bar(*actor);
                }
        }
}

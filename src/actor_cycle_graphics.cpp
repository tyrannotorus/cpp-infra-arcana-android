// =============================================================================
// Copyright 2011-2024 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "actor_cycle_graphics.hpp"

#include "actor.hpp"
#include "actor_player_state.hpp"
#include "colors.hpp"
#include "io.hpp"
#include "map.hpp"
#include "property_handler.hpp"
#include "random.hpp"

namespace actor
{
void cycle_graphics(Actor& actor, const io::GraphicsCycle cycle)
{
        if (actor::is_player(&actor) && (cycle == io::GraphicsCycle::fast)) {
                actor::player_state::g_lantern_color =
                        rnd::coin_toss()
                        ? colors::yellow().shaded(rnd::range(20, 40))
                        : colors::yellow();
        }

        actor.m_properties.cycle_graphics(cycle);
}

}  // namespace actor

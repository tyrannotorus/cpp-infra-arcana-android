// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "actor_cycle_graphics.hpp"

#include "actor.hpp"
#include "actor_player.hpp"
#include "colors.hpp"
#include "io.hpp"
#include "map.hpp"
#include "property_handler.hpp"
#include "random.hpp"

namespace actor
{
void cycle_graphics(Actor& actor, const io::GraphicsCycle cycle)
{
        if (actor::is_player(&actor) &&
            (cycle == io::GraphicsCycle::fast))
        {
                switch (rnd::range(0, 1))
                {
                case 0:
                        map::g_player->m_lantern_color =
                                colors::yellow().shaded(
                                        rnd::range(20, 40));
                        break;

                default:
                        map::g_player->m_lantern_color =
                                colors::yellow();
                        break;
                }
        }

        actor.m_properties.cycle_graphics(cycle);
}

}  // namespace actor

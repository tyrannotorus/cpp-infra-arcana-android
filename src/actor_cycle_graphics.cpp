// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "actor_cycle_graphics.hpp"

#include "actor.hpp"
#include "actor_player.hpp"
#include "colors.hpp"
#include "io.hpp"
#include "map.hpp"
#include "random.hpp"

namespace actor
{
class Actor;

void cycle_graphics(Actor& actor, const io::GraphicsCycle cycle)
{
        if (actor.is_player() && (cycle == io::GraphicsCycle::fast))
        {
                switch (rnd::range(0, 7))
                {
                case 0:
                case 1:
                        map::g_player->m_lantern_color =
                                colors::yellow().shaded(
                                        rnd::range(50, 75));
                        break;

                case 2:
                        map::g_player->m_lantern_color =
                                colors::yellow().tinted(
                                        rnd::range(0, 40));
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

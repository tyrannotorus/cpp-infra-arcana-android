// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include <cstdint>
#include <ostream>

#include "SDL_events.h"
#include "SDL_keyboard.h"
#include "SDL_keycode.h"
#include "SDL_timer.h"
#include "actor.hpp"
#include "config.hpp"
#include "debug.hpp"
#include "game_time.hpp"
#include "io.hpp"
#include "io_internal.hpp"
#include "pos.hpp"
#include "state.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static const size_t nr_graphics_cycle_types = (size_t)io::GraphicsCycle::END;
static std::uint32_t s_graphics_cycle_delay_ms[nr_graphics_cycle_types];
static std::uint32_t s_last_graphics_cycle_ms[nr_graphics_cycle_types];
static int s_graphics_cycle_nr[nr_graphics_cycle_types];

// -----------------------------------------------------------------------------
// io
// -----------------------------------------------------------------------------
namespace io
{
void init_animation()
{
        for (size_t i = 0; i < (size_t)GraphicsCycle::END; ++i)
        {
                auto& delay = s_graphics_cycle_delay_ms[i];

                const auto cycle = (GraphicsCycle)i;

                switch (cycle)
                {
                case GraphicsCycle::fast:
                        delay = 350;
                        break;

                case GraphicsCycle::slow:
                        delay = 800;
                        break;

                case GraphicsCycle::very_slow:
                        delay = 1400;
                        break;

                case GraphicsCycle::END:
                        ASSERT(false);
                        break;
                }

                s_last_graphics_cycle_ms[i] = 0;
                s_graphics_cycle_nr[i] = 0;
        }
}

bool step_graphics_cycling()
{
        bool did_step = false;

        const auto current_time_ms = SDL_GetTicks();

        for (size_t i = 0; i < (size_t)io::GraphicsCycle::END; ++i)
        {
                const auto d = current_time_ms - s_last_graphics_cycle_ms[i];

                if (d < s_graphics_cycle_delay_ms[i])
                {
                        continue;
                }

                s_last_graphics_cycle_ms[i] = current_time_ms;

                const auto cycle = (io::GraphicsCycle)i;

                states::cycle_graphics(cycle);

                ++s_graphics_cycle_nr[i];

                did_step = true;
        }

        return did_step;
}

int graphics_cycle_nr(const GraphicsCycle cycle_type)
{
        if (cycle_type == GraphicsCycle::END)
        {
                ASSERT(false);

                return 0;
        }

        return s_graphics_cycle_nr[(size_t)cycle_type];
}

}  // namespace io

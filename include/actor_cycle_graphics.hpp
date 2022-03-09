// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef ACTOR_CYCLE_GRAPHICS_HPP
#define ACTOR_CYCLE_GRAPHICS_HPP

#include "io.hpp"

namespace io
{
enum class GraphicsCycle;
}  // namespace io

namespace actor
{
class Actor;

void cycle_graphics(Actor& actor, io::GraphicsCycle cycle);

}  // namespace actor

#endif  // ACTOR_CYCLE_GRAPHICS_HPP

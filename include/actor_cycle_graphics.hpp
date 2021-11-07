// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

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

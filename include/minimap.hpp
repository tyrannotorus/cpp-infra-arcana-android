// =============================================================================
// Copyright 2011-2024 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef MINIMAP_HPP
#define MINIMAP_HPP

#include "state.hpp"

// -----------------------------------------------------------------------------
// ViewMinimap
// -----------------------------------------------------------------------------
class ViewMinimap : public State
{
public:
        ViewMinimap() = default;

        StateId id() const override
        {
                return StateId::view_minimap;
        }

        void draw() override;

        void update() override;
};

// -----------------------------------------------------------------------------
// minimap
// -----------------------------------------------------------------------------
namespace minimap
{
void clear();

void update();

}  // namespace minimap

#endif  // MINIMAP_HPP

// =============================================================================
// Copyright 2011-2023 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef HINTS_HPP
#define HINTS_HPP

namespace hints
{
enum class Id
{
        altars,
        fountains,
        destroying_corpses,
        unload_weapons,
        infected,
        overburdened,
        high_shock,
        temporary_and_permanent_shock,
        status_effects,
        kick_statue,
        kick_brazier,

        END
};

void init();

void save();

void load();

void display(Id id);

}  // namespace hints

#endif  // HINTS_HPP

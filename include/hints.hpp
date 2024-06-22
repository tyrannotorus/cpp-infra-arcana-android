// =============================================================================
// Copyright 2011-2024 Martin Törnqvist <m.tornq@gmail.com>
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
        destroying_corpses,
        fountains,
        high_shock,
        infected,
        kick_brazier,
        kick_statue,
        overburdened,
        status_effects,
        study_inscription,
        temporary_and_permanent_shock,
        unload_weapons,

        END
};

void init();

void save();

void load();

void display(Id id);

}  // namespace hints

#endif  // HINTS_HPP

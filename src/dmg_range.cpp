// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "dmg_range.hpp"

int DmgRange::roll() const
{
        return m_range.roll();
}

std::string DmgRange::str() const
{
        return m_range.str();
}

DmgRange DmgRange::scaled(const int k) const
{
        return {m_range.min * k, m_range.max * k};
}

DmgRange DmgRange::scaled_pct(const int pct) const
{
        int new_min = (m_range.min * pct) / 100;
        int new_max = (m_range.max * pct) / 100;

        new_min = std::max(new_min, 1);
        new_max = std::max(new_max, 1);

        return {new_min, new_max};
}

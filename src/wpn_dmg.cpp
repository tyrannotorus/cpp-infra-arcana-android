// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "wpn_dmg.hpp"

#include <algorithm>

int WpnDmg::roll_scaled(int scale_pct) const
{
        scale_pct = std::max(scale_pct, 0);

        int dmg = total_range().roll();

        dmg = (dmg * scale_pct) / 100;

        // If this weapon can potentially do damage before scaling, then always do at least 1 damage
        // after scaling (assuming there are no weapons that do 0-N damage, where N > 0).
        if (!is_zero_max_damage()) {
                dmg = std::max(dmg, 1);
        }

        return dmg;
}

std::string WpnDmg::str_total_range() const
{
        return total_range().str();
}

std::string WpnDmg::str_avg() const
{
        return total_range().str_avg();
}

std::string WpnDmg::str_plus() const
{
        if (m_plus == 0) {
                return "";
        }
        else if (m_plus > 0) {
                return "+" + std::to_string(m_plus);
        }
        else {
                return "-" + std::to_string(m_plus);
        }
}

Range WpnDmg::total_range() const
{
        return {m_min + m_plus, m_max + m_plus};
}

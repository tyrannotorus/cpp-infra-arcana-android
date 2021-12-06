// =============================================================================
// Copyright 2011-2021 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "wpn_dmg.hpp"
#include <string>

std::string WpnDmg::str() const
{
        std::string str = std::to_string(m_dmg);

        if (m_plus > 0)
        {
                str += "+" + std::to_string(m_plus);
        }
        else if (m_plus < 0)
        {
                str += "-" + std::to_string(m_plus);
        }

        return str;
}

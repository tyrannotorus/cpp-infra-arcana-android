// =============================================================================
// Copyright 2011-2023 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "wpn_dmg.hpp"

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

// =============================================================================
// Copyright 2011-2021 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef WPN_DMG_HPP
#define WPN_DMG_HPP

#include <string>

#include "random.hpp"

class WpnDmg
{
public:
        WpnDmg() = default;

        WpnDmg(const int dmg, const int plus = 0) :
                m_dmg(dmg),
                m_plus(plus)
        {
        }

        WpnDmg& operator=(const WpnDmg& other) = default;

        bool operator==(const WpnDmg& other) const
        {
                return ((m_dmg == other.m_dmg) && (m_plus == other.m_plus));
        }

        int dmg_tot() const
        {
                return m_dmg + m_plus;
        }

        int base_dmg() const
        {
                return m_dmg;
        }

        int plus() const
        {
                return m_plus;
        }

        std::string str() const;

        void incr_base_dmg(const int value)
        {
                m_dmg += value;
        }

        void set_plus(const int value)
        {
                m_plus = value;
        }

private:
        int m_dmg {0};
        int m_plus {0};
};

#endif  // DMG_RANGE_HPP

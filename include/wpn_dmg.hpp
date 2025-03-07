// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
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

        WpnDmg(const int min, const int max, const int plus = 0) :
                m_min(min),
                m_max(max),
                m_plus(plus)
        {
        }

        bool operator==(const WpnDmg& other) const
        {
                const bool min_eq = (m_min == other.m_min);
                const bool max_eq = (m_max == other.m_max);
                const bool plus_eq = (m_plus == other.m_plus);

                return min_eq && max_eq && plus_eq;
        }

        // NOTE: This shall be used when performing an actual attack.
        int roll_scaled(int scale_pct) const;

        std::string str_total_range() const;
        std::string str_avg() const;
        std::string str_plus() const;

        // Is the total max damage non-zero?
        bool is_zero_max_damage() const
        {
                return (total_range().max == 0);
        }

        int base_min() const
        {
                return m_min;
        }

        int base_max() const
        {
                return m_max;
        }

        void set_base_min(const int v)
        {
                m_min = v;
        }

        void set_base_max(const int v)
        {
                m_max = v;
        }

        int plus() const
        {
                return m_plus;
        }

        void set_plus(const int v)
        {
                m_plus = v;
        }

private:
        Range total_range() const;

        int m_min {0};
        int m_max {0};
        int m_plus {0};
};

#endif  // WPN_DMG_HPP

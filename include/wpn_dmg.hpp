// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef WPN_DMG_HPP
#define WPN_DMG_HPP

#include <algorithm>
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

        Range total_range() const
        {
                return {m_min + m_plus, m_max + m_plus};
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

        WpnDmg scaled_pct(int pct) const
        {
                // TODO: This is not a good method. Damage will often be scaled down more than
                // descriptions imply, since "plus" damage will be reduced and rounded down
                // individually. So for example with "-50% damage", 4-10 damage (3-9 +1) becomes 1-4
                // damage (1-4 +0).

                int new_min = m_min;

                if (m_min > 0) {
                        new_min = (m_min * pct) / 100;
                        new_min = std::max(new_min, 1);
                }

                int new_max = m_max;

                if (m_max > 0) {
                        new_max = (m_max * pct) / 100;

                        new_max = std::max(new_max, 1);
                }

                int new_plus = (m_plus * pct) / 100;

                return {new_min, new_max, new_plus};
        }

        std::string str_plus() const;

private:
        int m_min {0};
        int m_max {0};
        int m_plus {0};
};

#endif  // WPN_DMG_HPP

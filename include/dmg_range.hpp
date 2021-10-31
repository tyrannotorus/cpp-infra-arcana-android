// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef DMG_RANGE_HPP
#define DMG_RANGE_HPP

#include <string>

#include "random.hpp"

class DmgRange
{
public:
        DmgRange() = default;

        DmgRange(const int min, const int max) :
                m_range(min, max)
        {
        }

        DmgRange& operator=(const DmgRange& other) = default;

        bool operator==(const DmgRange& other) const
        {
                return m_range == other.m_range;
        }

        int min() const
        {
                return m_range.min;
        }

        int max() const
        {
                return m_range.max;
        }

        void set_min(const int value)
        {
                m_range.min = value;
        }

        void set_max(const int value)
        {
                m_range.max = value;
        }

        int roll() const;

        std::string str() const;

        Range range() const
        {
                return m_range;
        }

        void incr_dmg(const int value)
        {
                m_range.min += value;
                m_range.max += value;
        }

        DmgRange scaled(int k) const;

        DmgRange scaled_pct(int pct) const;

private:
        Range m_range {0, 0};
};

#endif  // DMG_RANGE_HPP

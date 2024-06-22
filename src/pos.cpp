// =============================================================================
// Copyright 2011-2024 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "pos.hpp"

#include "direction.hpp"

P::P(const Dir dir) :
        x(0),
        y(0)
{
        set(dir_utils::offset(dir));
}

P& P::operator=(const Dir dir)
{
        set(dir_utils::offset(dir));

        return *this;
}

P& P::operator+=(const Dir dir)
{
        *this += dir_utils::offset(dir);

        return *this;
}

P& P::operator-=(const Dir dir)
{
        const auto reversed_dir = dir_utils::reversed_dir(dir);

        *this += dir_utils::offset(reversed_dir);

        return *this;
}

P P::operator+(const Dir dir) const
{
        auto result = *this;

        result += dir;

        return result;
}

Dir P::to_dir() const
{
        return dir_utils::dir(*this);
}

P P::operator-(Dir dir) const
{
        auto result = *this;

        result -= dir;

        return result;
}

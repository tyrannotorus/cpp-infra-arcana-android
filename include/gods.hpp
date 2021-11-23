// =============================================================================
// Copyright 2011-2021 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef GODS_HPP
#define GODS_HPP

#include <string>

struct God
{
        std::string name;
        std::string descr;
};

namespace gods
{
const God& current_god();

void set_random_god();

}  // namespace gods

#endif  // GODS_HPP

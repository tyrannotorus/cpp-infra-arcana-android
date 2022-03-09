// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef PROPERTY_FACTORY_HPP
#define PROPERTY_FACTORY_HPP

#include "property_data.hpp"

class Prop;

namespace property_factory
{
Prop* make(PropId id);

}  // namespace property_factory

#endif  // PROPERTY_FACTORY_HPP

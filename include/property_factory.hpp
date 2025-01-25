// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef PROPERTY_FACTORY_HPP
#define PROPERTY_FACTORY_HPP

#include "property_data.hpp"

namespace prop
{
class Prop;

Prop* make(prop::Id id);

}  // namespace prop

#endif  // PROPERTY_FACTORY_HPP

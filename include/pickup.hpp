// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef PICKUP_HPP
#define PICKUP_HPP

namespace item
{
class Item;
class Wpn;
}  // namespace item

namespace item_pickup
{
// NOTE: The "try_" functions can always be called to check if something is
// there to be picked up or unloaded

void try_pick();

void try_unload_or_pick();

item::Item* unload_ranged_wpn(item::Wpn& wpn);

}  // namespace item_pickup

#endif

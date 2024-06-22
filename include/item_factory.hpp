// =============================================================================
// Copyright 2011-2024 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef ITEM_FACTORY_HPP
#define ITEM_FACTORY_HPP

struct P;

namespace item
{
class Item;

enum class Id;

item::Item* make(item::Id item_id, int nr_items = 1);

// TODO: Shouldn't this be a virtual function for the Item class? Something like
// "init_randomized()"?
void randomize_item_properties(item::Item& item);

item::Item* make_item_on_floor(item::Id item_id, const P& pos);

item::Item* copy_item(const item::Item& item_to_copy);

}  // namespace item

#endif  // ITEM_FACTORY_HPP

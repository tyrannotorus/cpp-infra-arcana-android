// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
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

// Whether the item the player is standing on is a firearm with ammo in it,
// i.e. whether try_unload_or_pick would unload it (instead of falling back
// to picking it up). This is the condition of the [ unload ] standing log
// button, and of the "unloading weapons" hint.
bool can_unload_item_at_player();

item::Item* unload_ranged_wpn(item::Wpn& wpn);

// The walking-on-an-item tile status message: prints the name of the item
// at the player's cell (or triggers its "found" message) and marks it
// discovered. Does nothing if the cell has no item. Also re-triggered
// when an item appears under the player WITHOUT a move (dropping, a
// thrown item landing at the player's feet, teleporting onto an item).
//
// NOTE: The [ pick up ] and [ unload ] context pins are NOT tied to
// these messages - they are "standing" buttons, derived from the game
// state every frame for as long as the player stands on an item (see
// msg_log). There are no "pick up" or "unload" action bar buttons - the
// standing buttons are THE touch path to picking things up and to
// emptying a firearm where it lies.
void print_item_at_player_msg();

}  // namespace item_pickup

#endif

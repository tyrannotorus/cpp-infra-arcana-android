// =============================================================================
// Copyright 2011-2024 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef RELOAD_HPP
#define RELOAD_HPP

namespace item
{
class Item;
}  // namespace item

namespace actor
{
class Actor;
}  // namespace actor

namespace reload
{
void try_reload(actor::Actor& actor, item::Item* item_to_reload);

void player_arrange_pistol_mags();

}  // namespace reload

#endif

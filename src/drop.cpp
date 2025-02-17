// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "drop.hpp"

#include <algorithm>
#include <iterator>
#include <ostream>
#include <string>
#include <vector>

#include "actor.hpp"
#include "actor_see.hpp"
#include "array2.hpp"
#include "colors.hpp"
#include "debug.hpp"
#include "inventory.hpp"
#include "item.hpp"
#include "item_data.hpp"
#include "item_factory.hpp"
#include "map.hpp"
#include "map_parsing.hpp"
#include "misc.hpp"
#include "msg_log.hpp"
#include "pos.hpp"
#include "terrain.hpp"
#include "terrain_data.hpp"
#include "text_format.hpp"

namespace item_drop
{
void drop_item_from_inv(
        actor::Actor& actor,
        const InvType inv_type,
        const size_t idx,
        const int nr_items_to_drop)
{
        item::Item* item_to_drop = nullptr;

        if (inv_type == InvType::slots) {
                ASSERT(idx != (size_t)SlotId::END);

                item_to_drop = actor.m_inv.m_slots[idx].item;
        }
        else {
                // Backpack item
                ASSERT(idx < actor.m_inv.m_backpack.size());

                item_to_drop = actor.m_inv.m_backpack[idx];
        }

        if (!item_to_drop) {
                return;
        }

        const item::ItemData& data = item_to_drop->data();

        const bool is_stackable = data.is_stackable;

        const int nr_items_before_drop = item_to_drop->m_nr_items;

        const bool is_whole_stack_dropped =
                !is_stackable ||
                (nr_items_to_drop == -1) ||
                (nr_items_to_drop >= nr_items_before_drop);

        std::string item_ref;

        item::Item* item_to_keep = nullptr;

        if (is_whole_stack_dropped) {
                item_ref = item_to_drop->name(ItemNameType::plural);

                item_to_drop = actor.m_inv.remove_item(item_to_drop, false);
        }
        else {
                // Only some items are dropped from a stack

                // Drop a copy of the selected item
                item_to_keep = item_to_drop;

                item_to_drop = item::copy_item(*item_to_keep);

                item_to_drop->m_nr_items = nr_items_to_drop;

                item_to_drop->on_removed_from_inv();

                item_ref = item_to_drop->name(ItemNameType::plural);

                item_to_keep->m_nr_items =
                        nr_items_before_drop - nr_items_to_drop;
        }

        if (!item_to_drop) {
                return;
        }

        // Print message
        if (actor::is_player(&actor)) {
                msg_log::add("I drop " + item_ref + ".");
        }
        else {
                // Monster is dropping item
                if (can_player_see_actor(actor)) {
                        const std::string mon_name_the =
                                text_format::first_to_upper(
                                        actor::name_the(actor));

                        msg_log::add(mon_name_the + " drops " + item_ref + ".");
                }
        }

        drop_item_on_map(actor.m_pos, *item_to_drop);
}

item::Item* drop_item_on_map(const P& intended_pos, item::Item& item)
{
        ASSERT(map::is_pos_inside_outer_walls(intended_pos));

        // If target cell is bottomless, just destroy the item
        const auto* const tgt_f = map::g_terrain.at(intended_pos);

        if (tgt_f->id() == terrain::Id::chasm) {
                delete &item;

                return nullptr;
        }

        // Make a vector of all cells on map with no blocking terrain
        Array2<bool> free_cell_array(map::dims());

        const size_t nr_positions = map::nr_positions();
        for (size_t i = 0; i < nr_positions; ++i) {
                auto* const t = map::g_terrain.at(i);

                free_cell_array.at(i) = t->can_have_item();
        }

        auto free_cells =
                to_vec(
                        free_cell_array,
                        true,
                        free_cell_array.rect());

        if (free_cells.empty()) {
                // No cells found were items could be placed - too bad!
                delete &item;

                return nullptr;
        }

        // Sort the vector according to distance to origin
        IsCloserToPos is_closer_to_origin(intended_pos);

        std::sort(
                std::begin(free_cells),
                std::end(free_cells),
                is_closer_to_origin);

        const bool is_stackable_type = item.data().is_stackable;

        int dist_searched_stackable = -1;

        // If the item is stackable, and there is a cell A in which the item can
        // be stacked, and a cell B which is empty, and A and B are of equal
        // distance to the origin, then we ALWAYS prefer cell A.
        // In other words, try to drop as near as possible, but prefer stacking.
        for (auto outer_it = std::begin(free_cells);
             outer_it != std::end(free_cells);
             ++outer_it) {
                const P& p = *outer_it;

                const int dist = king_dist(intended_pos, p);

                ASSERT(dist >= dist_searched_stackable);

                if (is_stackable_type &&
                    (dist > dist_searched_stackable)) {
                        // Search each cell with equal distance to the
                        // current distance
                        for (auto stack_it = outer_it;
                             stack_it != end(free_cells);
                             ++stack_it) {
                                const P& stack_p = *stack_it;

                                const int stack_dist =
                                        king_dist(intended_pos, stack_p);

                                ASSERT(stack_dist >= dist);

                                if (stack_dist > dist) {
                                        break;
                                }

                                auto* item_on_floor = map::g_items.at(stack_p);

                                if (item_on_floor &&
                                    item_on_floor->data().id == item.data().id) {
                                        item.m_nr_items += item_on_floor->m_nr_items;

                                        delete item_on_floor;

                                        map::g_items.at(stack_p) = &item;

                                        if (map::g_player->m_pos == stack_p) {
                                                item.discover();
                                        }

                                        return &item;
                                }
                        }  // Stack position loop

                        dist_searched_stackable = dist;
                }

                // Item has not been stacked at this distance

                if (!map::g_items.at(p)) {
                        // Alright, this cell is empty, let's put the item here
                        map::g_items.at(p) = &item;

                        if (map::g_player->m_pos == p) {
                                item.discover();
                        }

                        return &item;
                }
        }  // Free cells loop

        // All free cells occupied by other items (and stacking failed)
        // Too bad - goodbye item!
        delete &item;

        return nullptr;
}

}  // namespace item_drop

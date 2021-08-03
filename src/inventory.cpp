// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "inventory.hpp"

#include <algorithm>
#include <iterator>
#include <memory>
#include <ostream>
#include <vector>

#include "actor.hpp"
#include "actor_player.hpp"
#include "colors.hpp"
#include "debug.hpp"
#include "drop.hpp"
#include "item.hpp"
#include "item_data.hpp"
#include "item_factory.hpp"
#include "map.hpp"
#include "msg_log.hpp"
#include "saving.hpp"

Inventory::Inventory(actor::Actor* const owning_actor) :
        m_owning_actor(owning_actor)
{
        auto set_slot = [&](const SlotId id, const std::string& name) {
                m_slots[(size_t)id] = {id, name};
        };

        set_slot(SlotId::wpn, "Wielded");
        set_slot(SlotId::wpn_alt, "Prepared");
        set_slot(SlotId::body, "Body");
        set_slot(SlotId::head, "Head");
}

Inventory::~Inventory()
{
        for (size_t i = 0; i < (size_t)SlotId::END; ++i)
        {
                auto& slot = m_slots[i];

                delete slot.item;
        }

        for (auto* item : m_backpack)
        {
                delete item;
        }

        for (auto* item : m_intrinsics)
        {
                delete item;
        }
}

void Inventory::save() const
{
        for (const InvSlot& slot : m_slots)
        {
                auto* const item = slot.item;

                if (item)
                {
                        saving::put_int((int)item->id());
                        saving::put_int(item->m_nr_items);

                        item->save();
                }
                else
                {
                        // No item in this slot
                        saving::put_int((int)item::Id::END);
                }
        }

        saving::put_int(m_backpack.size());

        for (size_t i = 0; i < m_backpack.size(); ++i)
        {
                auto* const item = m_backpack[i];

                saving::put_int((int)item->id());
                saving::put_int(item->m_nr_items);

                item->save();
        }
}

void Inventory::load()
{
        for (InvSlot& slot : m_slots)
        {
                // Any previous item is destroyed
                auto* item = slot.item;

                delete item;

                slot.item = nullptr;

                const auto item_id = (item::Id)saving::get_int();

                if (item_id != item::Id::END)
                {
                        item = item::make(item_id);

                        item->m_nr_items = saving::get_int();

                        item->load();

                        slot.item = item;

                        ASSERT(m_owning_actor);

                        item->on_pickup(*m_owning_actor);

                        item->on_equip(Verbose::no);
                }
        }

        while (!m_backpack.empty())
        {
                remove_item_in_backpack_with_idx(0, true);
        }

        const int backpack_size = saving::get_int();

        for (int i = 0; i < backpack_size; ++i)
        {
                const auto id = (item::Id)saving::get_int();

                auto* item = item::make(id);

                item->m_nr_items = saving::get_int();

                item->load();

                m_backpack.push_back(item);

                ASSERT(m_owning_actor);

                item->on_pickup(*m_owning_actor);
        }
}

bool Inventory::has_item_in_backpack(const item::Id id) const
{
        auto it =
                std::find_if(
                        std::begin(m_backpack),
                        std::end(m_backpack),
                        [id](item::Item* item) {
                                return item->id() == id;
                        });

        return it != std::end(m_backpack);
}

int Inventory::item_stack_size_in_backpack(const item::Id id) const
{
        for (size_t i = 0; i < m_backpack.size(); ++i)
        {
                if (m_backpack[i]->data().id == id)
                {
                        if (m_backpack[i]->data().is_stackable)
                        {
                                return m_backpack[i]->m_nr_items;
                        }
                        else
                        {
                                // Not stackable
                                return 1;
                        }
                }
        }

        return 0;
}

bool Inventory::try_stack_in_backpack(item::Item* item)
{
        // If item stacks, see if there are other items of same type
        if (!item->data().is_stackable)
        {
                return false;
        }

        for (size_t i = 0; i < m_backpack.size(); ++i)
        {
                auto* const other = m_backpack[i];

                if (other->id() != item->id())
                {
                        continue;
                }

                // NOTE: We are keeping the new item and destroying old one (so
                // the parameter pointer is still valid)
                item->m_nr_items += other->m_nr_items;

                delete other;

                m_backpack[i] = item;

                if (m_owning_actor->is_player() &&
                    (map::g_player->m_last_thrown_item == other))
                {
                        map::g_player->m_last_thrown_item = item;
                }

                return true;
        }

        return false;
}

void Inventory::put_in_backpack(item::Item* item)
{
        ASSERT(!item->actor_carrying());

        bool is_stacked = try_stack_in_backpack(item);

        if (!is_stacked)
        {
                m_backpack.push_back(item);

                sort_backpack();
        }

        // NOTE: This may destroy the item (e.g. combining with another item)
        item->on_pickup(*m_owning_actor);
}

void Inventory::drop_all_non_intrinsic(const P& pos)
{
        TRACE_FUNC_BEGIN_VERBOSE;

        // Drop from slots
        for (InvSlot& slot : m_slots)
        {
                auto* const item = remove_item_in_slot(slot.id, false);

                if (item)
                {
                        item_drop::drop_item_on_map(pos, *item);
                }
        }

        // Drop from backpack
        while (!m_backpack.empty())
        {
                auto* const item = remove_item_in_backpack_with_idx(0, false);

                item_drop::drop_item_on_map(pos, *item);
        }

        TRACE_FUNC_END_VERBOSE;
}

bool Inventory::has_ammo_for_firearm_in_inventory() const
{
        auto* weapon = static_cast<item::Wpn*>(item_in_slot(SlotId::wpn));

        // If weapon found
        if (weapon)
        {
                // Should not happen
                ASSERT(!weapon->data().ranged.has_infinite_ammo);

                // If weapon is a firearm
                if (weapon->data().ranged.is_ranged_wpn)
                {
                        // Get weapon ammo type
                        const auto ammo_id =
                                weapon->data().ranged.ammo_item_id;

                        // Look for that ammo type in inventory
                        for (size_t i = 0; i < m_backpack.size(); ++i)
                        {
                                if (m_backpack[i]->data().id == ammo_id)
                                {
                                        return true;
                                }
                        }
                }
        }

        return false;
}

item::Item* Inventory::decr_item_in_slot(SlotId slot_id)
{
        auto& item = *item_in_slot(slot_id);

        auto delete_item = true;

        if (item.data().is_stackable)
        {
                --item.m_nr_items;

                delete_item = item.m_nr_items <= 0;
        }

        if (delete_item)
        {
                remove_item_in_slot(slot_id, true);

                return nullptr;
        }
        else
        {
                return &item;
        }
}

item::Item* Inventory::remove_item_in_slot(
        const SlotId slot_id,
        const bool delete_item)
{
        ASSERT(slot_id != SlotId::END);

        auto& slot = m_slots[(size_t)slot_id];

        auto* item = slot.item;

        if (!item)
        {
                return nullptr;
        }

        slot.item = nullptr;

        item->on_unequip();

        item->on_removed_from_inv();

        if (delete_item)
        {
                delete item;

                item = nullptr;
        }

        return item;
}

item::Item* Inventory::remove_item_in_backpack_with_idx(
        const size_t idx,
        const bool delete_item)
{
        ASSERT(idx < m_backpack.size());

        auto* item = m_backpack[idx];

        if (m_owning_actor->is_player() &&
            (item == map::g_player->m_last_thrown_item))
        {
                map::g_player->m_last_thrown_item = nullptr;
        }

        m_backpack.erase(std::begin(m_backpack) + idx);

        item->on_removed_from_inv();

        if (delete_item)
        {
                delete item;

                item = nullptr;
        }

        return item;
}

item::Item* Inventory::remove_item(
        item::Item* const item,
        const bool delete_item)
{
        item::Item* item_returned = nullptr;

        for (InvSlot& slot : m_slots)
        {
                if (slot.item == item)
                {
                        item_returned = remove_item_in_slot(
                                slot.id, delete_item);

                        return item_returned;
                }
        }

        for (size_t i = 0; i < m_backpack.size(); ++i)
        {
                if (m_backpack[i] == item)
                {
                        item_returned = remove_item_in_backpack_with_idx(
                                i, delete_item);

                        return item_returned;
                }
        }

        return item_returned;
}

item::Item* Inventory::remove_item_in_backpack_with_ptr(
        item::Item* const item,
        const bool delete_item)
{
        for (size_t i = 0; i < m_backpack.size(); ++i)
        {
                const auto* const current_item = m_backpack[i];

                if (current_item == item)
                {
                        return remove_item_in_backpack_with_idx(i, delete_item);
                }
        }

        TRACE << "Parameter item not in backpack" << std::endl;
        ASSERT(false);

        return nullptr;
}

item::Item* Inventory::decr_item_in_backpack(const size_t idx)
{
        auto& item = *m_backpack[idx];

        bool delete_item = true;

        if (item.data().is_stackable)
        {
                --item.m_nr_items;

                delete_item = item.m_nr_items <= 0;
        }

        if (delete_item)
        {
                remove_item_in_backpack_with_idx(idx, true);

                return nullptr;
        }
        else
        {
                return &item;
        }
}

void Inventory::decr_item_type_in_backpack(const item::Id id)
{
        for (size_t i = 0; i < m_backpack.size(); ++i)
        {
                if (m_backpack[i]->data().id == id)
                {
                        decr_item_in_backpack(i);
                }
        }
}

item::Item* Inventory::decr_item(item::Item* const item)
{
        for (InvSlot& slot : m_slots)
        {
                if (slot.item == item)
                {
                        auto* const item_after = decr_item_in_slot(slot.id);

                        return item_after;
                }
        }

        for (size_t i = 0; i < m_backpack.size(); ++i)
        {
                if (m_backpack[i] == item)
                {
                        auto* const item_after = decr_item_in_backpack(i);

                        return item_after;
                }
        }

        return nullptr;
}

size_t Inventory::move_from_slot_to_backpack(const SlotId id)
{
        ASSERT(id != SlotId::END);

        auto& slot = m_slots[(size_t)id];

        auto* const item = slot.item;

        if (item)
        {
                item->on_unequip();

                slot.item = nullptr;

                bool is_stacked = try_stack_in_backpack(item);

                if (!is_stacked)
                {
                        m_backpack.push_back(item);

                        sort_backpack();
                }
        }

        for (size_t i = 0; i < m_backpack.size(); ++i)
        {
                if (item == m_backpack[i])
                {
                        return i;
                }
        }

        ASSERT(false);

        return 0;
}

void Inventory::equip_backpack_item(
        const size_t backpack_idx,
        const SlotId slot_id)
{
        ASSERT(slot_id != SlotId::END);

        ASSERT(m_owning_actor);

        const bool backpack_slot_exists = backpack_idx < m_backpack.size();

        if (!backpack_slot_exists)
        {
                ASSERT(false);

                return;
        }

        auto* item = m_backpack[backpack_idx];

        m_backpack.erase(std::begin(m_backpack) + backpack_idx);

        equip(slot_id, item, Verbose::yes);

        sort_backpack();
}

void Inventory::equip_backpack_item(
        const item::Item* const item,
        const SlotId slot_id)
{
        for (size_t i = 0; i < m_backpack.size(); ++i)
        {
                if (m_backpack[i] == item)
                {
                        equip_backpack_item(i, slot_id);

                        return;
                }
        }

        ASSERT(false);
}

size_t Inventory::unequip_slot(const SlotId id)
{
        auto& slot = m_slots[(size_t)id];

        auto* item = slot.item;

        if (!item)
        {
                ASSERT(false);

                return 0;
        }

        const size_t item_backpack_idx = move_from_slot_to_backpack(slot.id);

        if (m_owning_actor->is_player())
        {
                print_unequip_message(slot.id, *item);
        }

        return item_backpack_idx;
}

void Inventory::swap_wielded_and_prepared()
{
        auto& slot1 = m_slots[(size_t)SlotId::wpn];
        auto& slot2 = m_slots[(size_t)SlotId::wpn_alt];

        auto* item1 = slot1.item;
        auto* item2 = slot2.item;

        slot1.item = item2;
        slot2.item = item1;
}

bool Inventory::has_item_in_slot(const SlotId id) const
{
        ASSERT(id != SlotId::END && "Illegal slot id");

        return m_slots[int(id)].item;
}

bool Inventory::has_item_in_slot(
        const SlotId slot_id,
        item::Id item_id) const
{
        if (!has_item_in_slot(slot_id))
        {
                return false;
        }

        auto* const item = item_in_slot(slot_id);

        return item->id() == item_id;
}

std::vector<item::Item*> Inventory::all_items() const
{
        std::vector<item::Item*> items = m_backpack;

        for (const auto& slot : m_slots)
        {
                if (slot.item)
                {
                        items.push_back(slot.item);
                }
        }

        return items;
}

item::Item* Inventory::item_in_backpack(const item::Id id) const
{
        auto it =
                std::find_if(
                        std::begin(m_backpack),
                        std::end(m_backpack),
                        [id](item::Item* item) {
                                return item->id() == id;
                        });

        if (it == std::end(m_backpack))
        {
                return nullptr;
        }

        return *it;
}

int Inventory::backpack_idx(const item::Id id) const
{
        for (size_t i = 0; i < m_backpack.size(); ++i)
        {
                if (m_backpack[i]->id() == id)
                {
                        return (int)i;
                }
        }

        return -1;
}

item::Item* Inventory::item_in_slot(SlotId id) const
{
        ASSERT(id != SlotId::END);

        return m_slots[(size_t)id].item;
}

item::Item* Inventory::intrinsic_in_element(int idx) const
{
        if (intrinsics_size() > idx)
        {
                return m_intrinsics[idx];
        }

        return nullptr;
}

void Inventory::put_in_intrinsics(item::Item* item)
{
        ASSERT(item->data().type == ItemType::melee_wpn_intr ||
               item->data().type == ItemType::ranged_wpn_intr);

        m_intrinsics.push_back(item);

        item->on_pickup(*m_owning_actor);
}

void Inventory::equip(
        const SlotId id,
        item::Item* const item,
        Verbose verbose)
{
        ASSERT(id != SlotId::END);

        InvSlot* slot = nullptr;

        for (InvSlot& current_slot : m_slots)
        {
                if (current_slot.id == id)
                {
                        slot = &current_slot;
                }
        }

        if (!slot)
        {
                ASSERT(false);

                return;
        }

        if (slot->item)
        {
                ASSERT(false);

                return;
        }

        slot->item = item;

        if (m_owning_actor->is_player() && (verbose == Verbose::yes))
        {
                print_equip_message(id, *item);
        }

        item->on_equip(verbose);
}

void Inventory::put_in_slot(
        const SlotId id,
        item::Item* item,
        Verbose verbose)
{
        item->on_pickup(*m_owning_actor);

        equip(id, item, verbose);
}

void Inventory::print_equip_message(
        const SlotId slot_id,
        const item::Item& item)
{
        const std::string name = item.name(ItemRefType::plural);

        std::string msg;

        switch (slot_id)
        {
        case SlotId::wpn:
                msg = "I am now wielding " + name + ".";
                break;

        case SlotId::wpn_alt:
                msg =
                        "I am now using " +
                        name +
                        " as a prepared weapon.";
                break;

        case SlotId::body:
                msg = "I am now wearing " + name + ".";
                break;

        case SlotId::head:
                msg = "I am now wearing " + name + ".";
                break;

        case SlotId::END:
        {
        }
        break;
        }

        msg_log::add(
                msg,
                colors::text(),
                MsgInterruptPlayer::no,
                MorePromptOnMsg::no);
}

void Inventory::print_unequip_message(
        const SlotId slot_id,
        const item::Item& item)
{
        // The message should be of the form "... my [item]" - we never
        // want the name to be "A [item]". Therefore we use plural form
        // for stacks, and plain form for single items.
        const auto item_ref_type =
                (item.m_nr_items > 1)
                ? ItemRefType::plural
                : ItemRefType::plain;

        const std::string name = item.name(item_ref_type);

        std::string msg;

        switch (slot_id)
        {
        case SlotId::wpn:
                msg = "I put away my " + name + ".";
                break;

        case SlotId::wpn_alt:
                msg = "I put away my " + name + ".";
                break;

        case SlotId::body:
                msg = "I have taken off my " + name + ".";
                break;

        case SlotId::head:
                msg = "I have taken off my " + name + ".";
                break;

        case SlotId::END:
                break;
        }

        msg_log::add(
                msg,
                colors::text(),
                MsgInterruptPlayer::no,
                MorePromptOnMsg::no);
}

int Inventory::total_item_weight() const
{
        int weight = 0;

        for (const auto& slot : m_slots)
        {
                if (slot.item)
                {
                        weight += slot.item->weight();
                }
        }

        for (auto* const item : m_backpack)
        {
                weight += item->weight();
        }

        return weight;
}

// Function for lexicographically comparing two items
struct LexicograhicalCompareItems
{
        bool operator()(
                const item::Item* const item1,
                const item::Item* const item2)
        {
                const std::string& item_name1 = item1->name(ItemRefType::plain);
                const std::string& item_name2 = item2->name(ItemRefType::plain);

                return lexicographical_compare(
                        item_name1.begin(),
                        item_name1.end(),
                        item_name2.begin(),
                        item_name2.end());
        }
};

void Inventory::sort_backpack()
{
        LexicograhicalCompareItems lex_cmp;

        // First, take out prioritized items
        std::vector<item::Item*> prio_items;

        for (auto it = std::begin(m_backpack); it != std::end(m_backpack);)
        {
                auto* const item = *it;

                if (item->data().is_prio_in_backpack_list)
                {
                        prio_items.push_back(item);

                        it = m_backpack.erase(it);
                }
                else
                {
                        // Item not prioritized
                        ++it;
                }
        }

        // Sort the prioritized items lexicographically
        if (!prio_items.empty())
        {
                std::sort(
                        std::begin(prio_items),
                        std::end(prio_items),
                        lex_cmp);
        }

        // Categorize the remaining items
        std::vector<std::vector<item::Item*>> sort_buffer;

        // TODO: Sort according to item type, instead of color?

        // Sort according to item interface color first
        for (auto* item : m_backpack)
        {
                bool is_added_to_buffer = false;

                // Check if item should be added to any existing color group
                for (auto& group : sort_buffer)
                {
                        const Color color_current_group =
                                group[0]->interface_color();

                        if (item->interface_color() == color_current_group)
                        {
                                group.push_back(item);

                                is_added_to_buffer = true;

                                break;
                        }
                }

                if (is_added_to_buffer)
                {
                        continue;
                }

                // Item is a new color, create a new color group
                std::vector<item::Item*> new_group;
                new_group.push_back(item);
                sort_buffer.push_back(new_group);
        }

        // Sort lexicographically secondarily
        for (auto& group : sort_buffer)
        {
                std::sort(std::begin(group), std::end(group), lex_cmp);
        }

        // Add the sorted items to the backpack
        // NOTE: prio_items may be empty
        m_backpack = prio_items;

        for (size_t i = 0; i < sort_buffer.size(); ++i)
        {
                for (size_t ii = 0; ii < sort_buffer[i].size(); ii++)
                {
                        m_backpack.push_back(sort_buffer[i][ii]);
                }
        }
}

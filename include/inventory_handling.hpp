// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef INVENTORY_HANDLING_HPP
#define INVENTORY_HANDLING_HPP

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "action_list_state.hpp"
#include "browser.hpp"
#include "colors.hpp"
#include "global.hpp"
#include "inventory.hpp"
#include "io.hpp"
#include "panel.hpp"
#include "rect.hpp"
#include "state.hpp"

namespace item
{
class Item;
}  // namespace item
struct P;

struct FilteredInvEntry
{
        // Index relatie to slot list or relative to backpack list
        size_t relative_idx {0};
        bool is_slot {false};
};

// What can be done with the marked inventory item. The inventory screen
// is not engaged by tapping its rows (a tap only marks) - everything is
// done through pins in the corner of the description column, so that
// nothing happens to an item by accident.
enum class ItemActionId
{
        equip,     // Wield / wear the marked backpack item
        equip_in,  // Fill the marked EMPTY slot (opens the item selection)
        unequip,   // Take off what is in the marked slot
        activate,  // Use / drink / read / light
        reload,
        throw_item,
        drop
};

class InvState : public ActionListState
{
public:
        InvState() = default;

        ~InvState() override = default;

        StateId id() const override;

protected:
        void draw_slot(
                SlotId id,
                int y,
                bool is_marked,
                ItemNameAttackInfo attack_info);

        void draw_backpack_item(
                size_t backpack_idx,
                int y,
                bool is_marked,
                ItemNameAttackInfo attack_info);

        // void draw_item_symbol(const item::Item& item, const P& p) const;

        void set_viewed_item(
                const item::Item* item,
                ItemNameAttackInfo attack_info);

        void draw_item_descr();

        // The marked entry, as an item and where it is kept
        struct MarkedItem
        {
                item::Item* item {nullptr};
                InvType inv_type {InvType::slots};

                // Slot id, or index into the backpack
                size_t idx {0};
        };

        virtual MarkedItem marked_item() const
        {
                return {};
        }

        // The actions offered for the marked entry, primary first and
        // [ drop ] last
        virtual std::vector<ActionPin> marked_item_actions() const
        {
                return {};
        }

        std::vector<ActionPin> marked_entry_actions() const final;

        void run_action(int action_id) final;

        // Whether lighting an explosive from this screen goes straight
        // into aiming the throw. Coming from the THROW screen says what
        // the player means to do with it; lighting it from the inventory
        // says nothing of the kind - they may want to carry it, or put it
        // down somewhere.
        virtual bool should_aim_after_lighting() const
        {
                return false;
        }

        // Runs a tapped pin. NOTE: May delete this object (the action may
        // spend the turn) - do not touch any members after calling it.
        void run_item_action(ItemActionId action_id);

        // Called after an action that did NOT spend the turn - the screen
        // stays open, but its list may need re-making (an item was
        // identified, a stack was split by lighting one, ...)
        void on_list_changed() final
        {
                on_inventory_changed();
        }

        virtual void on_inventory_changed() {}

private:
        std::vector<std::string> make_detailed_descr_lines() const;

        const item::Item* m_viewed_item {nullptr};
        ItemNameAttackInfo m_viewed_item_attack_info {(ItemNameAttackInfo)0};
};

// The inventory screen. Screens opened from it (equipping, the medical bag
// treatment popup, ...) are pushed ON TOP of it, so that closing one
// returns to the inventory exactly as it was left - same page, same marked
// entry. The inventory itself closes only when an action actually spends
// game time, since the world then has to move on.
class BrowseInv : public InvState
{
public:
        BrowseInv() = default;

        void on_start() override;

        void on_resume() override;

        void draw() override;

        void update() override;

        void disable_allow_inventory_actions()
        {
                m_allow_inv_action = false;
        }

private:
        bool has_action_pins() const override
        {
                // NOT while the game info screens borrow this screen, see
                // disable_allow_inventory_actions
                return m_allow_inv_action;
        }

        MarkedItem marked_item() const override;

        std::vector<ActionPin> marked_item_actions() const override;

        void on_inventory_changed() override;

        // Number of rows: every slot, then the backpack
        int list_size() const;

        // Re-makes the browser for the current inventory, keeping the
        // marked entry
        void sync_browser_to_inventory();

        void on_selected();

        void on_inventory_slot_selected(InvSlot& slot);

        void on_inventory_slot_with_item_selected(InvSlot& slot);

        void on_backpack_item_selected(size_t backpack_idx);

        bool m_allow_inv_action {true};
};

class Equip : public InvState
{
public:
        Equip(InvSlot& slot) :

                m_slot_to_equip(slot)
        {}

        void on_start() override;

        void draw() override;

        void update() override;

private:
        std::vector<FilteredInvEntry> m_filtered_backpack_indexes {};

        InvSlot& m_slot_to_equip;
};

// Picking something to throw. Lists everything throwable, and everything
// that can be LIT (explosives) - lighting one is what its pin does, and
// the lit item is then carried like anything else until it is thrown or
// put down (see item::Explosive).
class SelectThrow : public InvState
{
public:
        SelectThrow() = default;

        void on_start() override;

        void draw() override;

        void update() override;

private:
        bool has_action_pins() const override
        {
                return true;
        }

        MarkedItem marked_item() const override;

        std::vector<ActionPin> marked_item_actions() const override;

        bool should_aim_after_lighting() const override
        {
                return true;
        }

        void reserve_keys();

        std::vector<FilteredInvEntry> m_filtered_inv {};
};

class SelectIdentify : public InvState
{
public:
        SelectIdentify(std::vector<ItemType> item_types_allowed = {}) :
                m_item_types_allowed(std::move(item_types_allowed)) {}

        void on_start() override;

        void draw() override;

        void update() override;

private:
        const std::vector<ItemType> m_item_types_allowed;
        std::vector<FilteredInvEntry> m_filtered_inv {};
};

#endif  // INVENTORY_HANDLING_HPP

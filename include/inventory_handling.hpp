// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef INVENTORY_HANDLING_HPP
#define INVENTORY_HANDLING_HPP

#include <stddef.h>
#include <utility>
#include <vector>

#include "browser.hpp"
#include "inventory.hpp"
#include "state.hpp"
#include "global.hpp"

class Color;
namespace item {
class Item;
}  // namespace item
struct P;

enum class InvScr
{
        inv,
        equip,
        apply,
        none
};

struct FilteredInvEntry
{
        // Index relatie to slot list or relative to backpack list
        size_t relative_idx {0};
        bool is_slot {false};
};

class InvState : public State
{
public:
        InvState();

        virtual ~InvState() = default;

        StateId id() const override;

protected:
        void draw_slot(
                SlotId id,
                int y,
                char key,
                bool is_marked,
                ItemRefAttInf att_info) const;

        void draw_backpack_item(
                size_t backpack_idx,
                int y,
                char key,
                bool is_marked,
                ItemRefAttInf att_info) const;

        void activate(size_t backpack_idx);

        MenuBrowser m_browser;

        void draw_weight_pct_and_dots(
                P item_pos,
                size_t item_name_len,
                const item::Item& item,
                const Color& item_name_color_id,
                bool is_marked) const;

        // void draw_item_symbol(const item::Item& item, const P& p) const;

        void draw_detailed_item_descr(
                const item::Item* item,
                ItemRefAttInf att_inf) const;
};

class BrowseInv : public InvState
{
public:
        BrowseInv() = default;

        void on_start() override;

        void draw() override;

        void update() override;

private:
        void on_body_slot_item_selected() const;

        void on_equipable_backpack_item_selected(
                size_t backpack_idx) const;
};

class Apply : public InvState
{
public:
        Apply() = default;

        void on_start() override;

        void draw() override;

        void update() override;

private:
        std::vector<size_t> m_filtered_backpack_indexes {};
};

class Drop : public InvState
{
public:
        Drop() = default;

        void on_start() override;

        void draw() override;

        void update() override;
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
        std::vector<size_t> m_filtered_backpack_indexes {};

        InvSlot& m_slot_to_equip;
};

class SelectThrow : public InvState
{
public:
        SelectThrow() = default;

        void on_start() override;

        void draw() override;

        void update() override;

private:
        std::vector<FilteredInvEntry> m_filtered_inv {};
};

class SelectIdentify : public InvState
{
public:
        SelectIdentify(std::vector<ItemType> item_types_allowed = {}) :

                m_item_types_allowed(std::move(item_types_allowed))
        {}

        void on_start() override;

        void draw() override;

        void update() override;

private:
        const std::vector<ItemType> m_item_types_allowed;
        std::vector<FilteredInvEntry> m_filtered_inv {};
};

#endif  // INVENTORY_HANDLING_HPP

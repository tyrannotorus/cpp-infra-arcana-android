// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef ITEM_ARMOR_HPP
#define ITEM_ARMOR_HPP

#include <string>

#include "colors.hpp"
#include "global.hpp"
#include "item.hpp"

namespace item
{
struct ItemData;

class Armor : public Item
{
public:
        Armor(ItemData* item_data);

        ~Armor() = default;

        Color interface_color() const override
        {
                return colors::gray();
        }

        std::string name_info_str() const override;

        void hit(int dmg);
};

class ArmorAsbSuit : public Armor
{
public:
        ArmorAsbSuit(ItemData* const item_data) :
                Armor(item_data) {}

        ~ArmorAsbSuit() = default;

        void on_equip_hook(Verbose verbose) override;

private:
        void on_unequip_hook() override;
};

class ArmorMiGo : public Armor
{
public:
        ArmorMiGo(ItemData* const item_data) :
                Armor(item_data) {}

        ~ArmorMiGo() = default;

        void on_equip_hook(Verbose verbose) override;
};

}  // namespace item

#endif  // ITEM_ARMOR_HPP

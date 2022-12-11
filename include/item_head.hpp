// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef ITEM_HEAD_HPP
#define ITEM_HEAD_HPP

#include <string>

#include "colors.hpp"
#include "item.hpp"

class Inventory;

namespace item
{
struct ItemData;

class Headwear : public Item
{
public:
        Headwear(ItemData* item_data) :
                Item(item_data) {}

        Color interface_color() const override
        {
                return colors::brown();
        }
};

class GasMask : public Headwear
{
public:
        GasMask(ItemData* item_data) :
                Headwear(item_data),
                m_nr_turns_left(60) {}

        std::string name_info_str() const override;

        void decr_turns_left(Inventory& carrier_inv);

protected:
        int m_nr_turns_left;
};

class TortureCollar : public Headwear
{
public:
        TortureCollar(ItemData* item_data) :
                Headwear(item_data) {}
};

}  // namespace item

#endif  // ITEM_HEAD_HPP

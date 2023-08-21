// =============================================================================
// Copyright 2011-2023 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef ITEM_AMMO_HPP
#define ITEM_AMMO_HPP

#include <string>

#include "colors.hpp"
#include "item.hpp"

namespace item
{
struct ItemData;

class Ammo : public Item
{
public:
        Ammo(ItemData* const item_data) :
                Item(item_data) {}

        virtual ~Ammo() = default;

        Color interface_color() const override
        {
                return colors::white();
        }
};

class AmmoMag : public Ammo
{
public:
        AmmoMag(ItemData* item_data);

        ~AmmoMag() = default;

        std::string name_info_str(ItemNameIdentified id_type) const override;

        void set_full_ammo();

        void save_hook() const override;

        void load_hook() override;

        int m_ammo;
};

}  // namespace item

#endif  // ITEM_AMMO_HPP

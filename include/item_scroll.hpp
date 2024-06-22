// =============================================================================
// Copyright 2011-2023 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef ITEM_SCROLL_HPP
#define ITEM_SCROLL_HPP

#include <string>
#include <vector>

#include "colors.hpp"
#include "global.hpp"
#include "item.hpp"

namespace item
{
struct ItemData;
}  // namespace item

namespace actor
{
class Actor;
}  // namespace actor

class Spell;

namespace scroll
{
inline constexpr int g_low_spawn_chance = 10;
inline constexpr int g_high_spawn_chance = 40;

void init();

void save();
void load();

class Scroll : public item::Item
{
public:
        Scroll(item::ItemData* item_data);

        ~Scroll() = default;

        Color interface_color() const override
        {
                return colors::magenta();
        }

        std::string name_info_str(ItemNameIdentified id_type) const override;

        ConsumeItem activate(actor::Actor* actor) override;

        std::string real_name() const;

        std::string domain_str() const;

        std::vector<std::string> descr_hook() const override;

        ItemPrePickResult pre_pickup_hook() override;

        void identify(Verbose verbose) override;

        void reveal_domain() const;

        Spell* make_spell() const;
};

}  // namespace scroll

#endif  // ITEM_SCROLL_HPP

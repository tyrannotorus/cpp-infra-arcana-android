// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
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

        void save_hook() const override;

        void load_hook() override;

        Color interface_color() const override
        {
                return colors::magenta();
        }

        std::string name_info_str() const override;

        ConsumeItem activate(actor::Actor* actor) override;

        std::string real_name() const;

        std::vector<std::string> descr_hook() const override;

        void on_player_reached_new_dlvl_hook() final;

        void on_actor_turn_in_inv_hook(InvType inv_type) override;

        ItemPrePickResult pre_pickup_hook() override;

        void identify(Verbose verbose) override;

        Spell* make_spell() const;

private:
        int m_domain_feeling_dlvl_countdown;
        int m_domain_feeling_turn_countdown;
};

}  // namespace scroll

#endif  // ITEM_SCROLL_HPP

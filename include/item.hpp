// =============================================================================
// Copyright 2011-2023 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef ITEM_HPP
#define ITEM_HPP

#include <string>
#include <utility>
#include <vector>

#include "colors.hpp"
#include "gfx.hpp"
#include "global.hpp"
#include "item_curse.hpp"
#include "item_curse_ids.hpp"
#include "wpn_dmg.hpp"

struct ItemAttackProp;
struct P;

namespace actor
{
class Actor;
}  // namespace actor

namespace prop
{
class Prop;
}  // namespace prop

namespace item
{
enum class Id;
struct ItemData;

enum class ItemActivateRetType
{
        keep,
        destroyed
};

class Item
{
public:
        Item(ItemData* item_data);

        Item(Item& other) = delete;

        Item& operator=(const Item& other);

        virtual ~Item();

        Id id() const;

        void save();

        void load();

        ItemData& data() const;

        virtual Color color() const;

        char character() const;

        gfx::TileId tile() const;

        virtual LightSize light_size() const
        {
                return LightSize::none;
        }

        std::string name(
                ItemNameType name_type,
                ItemNameInfo info = ItemNameInfo::yes,
                ItemNameAttackInfo attack_info = ItemNameAttackInfo::none,
                ItemNameIdentified id_type = ItemNameIdentified::use_item_status) const;

        std::vector<std::string> descr() const;

        std::string hit_mod_str(
                ItemNameAttackInfo attack_info,
                AbbrevItemAttackInfo abbrev) const;

        std::string dmg_str(
                ItemNameAttackInfo attack_info,
                ItemNameDmg dmg_value) const;

        std::string plus_str(ItemNameAttackInfo attack_info) const;

        // E.g. "(Off)" for Lanterns, or "(4/7)" for Pistols
        virtual std::string name_info_str(
                const ItemNameIdentified id_type =
                        ItemNameIdentified::use_item_status) const
        {
                (void)id_type;

                return "";
        }

        virtual void identify(const Verbose verbose)
        {
                (void)verbose;
        }

        int weight() const;

        std::string weight_str() const;

        virtual ConsumeItem activate(actor::Actor* actor);

        virtual Color interface_color() const
        {
                return colors::dark_yellow();
        }

        void on_std_turn_in_inv(InvType inv_type);

        void on_actor_turn_in_inv(InvType inv_type);

        virtual ItemPrePickResult pre_pickup_hook()
        {
                return ItemPrePickResult::do_pickup;
        }

        void on_pickup(actor::Actor& actor);

        // "on_pickup()" should be called before this
        void on_equip(Verbose verbose);

        void on_unequip();

        // This is the opposite of "on_pickup()". If this is a wielded item,
        // "on_unequip()" should be called first.
        void on_removed_from_inv();

        // This is called for example when:
        // * Player walks into the same position as the item,
        // * The item is dropped into the same position as the player,
        // * The item is picked up,
        // * The item is found in an item container, but NOT picked up
        void discover();

        void on_player_reached_new_dlvl();

        virtual void on_projectile_blocked(const P& pos)
        {
                (void)pos;
        }

        virtual void on_melee_hit(actor::Actor& actor_hit, const int dmg)
        {
                (void)actor_hit;
                (void)dmg;
        }

        virtual void pre_ranged_attack()
        {
        }

        void set_base_melee_dmg(const WpnDmg& range)
        {
                m_base_melee_dmg = range;
        }

        void set_base_ranged_dmg(const WpnDmg& range)
        {
                m_base_ranged_dmg = range;
        }

        WpnDmg base_melee_dmg() const
        {
                return m_base_melee_dmg;
        }

        void set_melee_plus(const int plus)
        {
                m_base_melee_dmg.set_plus(plus);
        }

        // Calculated damage taking into account things like player traits.
        WpnDmg melee_dmg(const actor::Actor* attacker) const;
        WpnDmg ranged_dmg(const actor::Actor* attacker) const;
        WpnDmg thrown_dmg(const actor::Actor* attacker) const;

        ItemAttackProp& prop_applied_on_melee(const actor::Actor* attacker) const;
        ItemAttackProp& prop_applied_on_ranged(const actor::Actor* attacker) const;

        virtual void on_melee_kill(actor::Actor& actor_killed)
        {
                (void)actor_killed;
        }

        virtual void on_ranged_hit(actor::Actor& actor_hit)
        {
                (void)actor_hit;
        }

        void add_carrier_prop(prop::Prop* prop, Verbose verbose);

        void clear_carrier_props();

        virtual int hp_regen_change(const InvType inv_type) const
        {
                (void)inv_type;
                return 0;
        }

        actor::Actor* actor_carrying()
        {
                return m_actor_carrying;
        }

        void clear_actor_carrying()
        {
                m_actor_carrying = nullptr;
        }

        const std::vector<prop::Prop*>& carrier_props() const
        {
                return m_carrier_props;
        }

        virtual bool is_curse_allowed(item_curse::Id id) const
        {
                (void)id;

                return true;
        }

        bool is_cursed() const
        {
                return (m_curse.id() != item_curse::Id::END);
        }

        item_curse::Curse& current_curse()
        {
                return m_curse;
        }

        void set_curse(item_curse::Curse&& curse)
        {
                m_curse = std::move(curse);
        }

        void remove_curse()
        {
                m_curse = item_curse::Curse();
        }

        int armor_points() const;

        int durability() const
        {
                return m_durability;
        }

        void set_max_durability()
        {
                m_durability = 100;
        }

        int m_nr_items {1};

protected:
        virtual void save_hook() const {}

        virtual void load_hook() {}

        virtual std::vector<std::string> descr_hook() const;

        virtual void on_std_turn_in_inv_hook(const InvType inv_type)
        {
                (void)inv_type;
        }

        virtual void on_actor_turn_in_inv_hook(const InvType inv_type)
        {
                (void)inv_type;
        }

        virtual void on_pickup_hook() {}

        virtual void on_equip_hook(const Verbose verbose)
        {
                (void)verbose;
        }

        virtual void on_unequip_hook() {}

        virtual void on_removed_from_inv_hook() {}

        virtual void on_player_reached_new_dlvl_hook() {}

        virtual void specific_dmg_mod(
                WpnDmg& range,
                const actor::Actor* const actor) const
        {
                (void)range;
                (void)actor;
        }

        ItemAttackProp* prop_applied_intr_attack(const actor::Actor* attacker) const;

        ItemData* m_data;

        actor::Actor* m_actor_carrying {nullptr};

        // Base damage (not including actor properties, player traits, etc)
        WpnDmg m_base_melee_dmg;
        WpnDmg m_base_ranged_dmg;

        int m_durability;

private:
        // Properties to apply on owning actor (when e.g. wearing the item, or
        // just keeping it in the inventory).
        std::vector<prop::Prop*> m_carrier_props {};

        item_curse::Curse m_curse {};
};

}  // namespace item

#endif  // ITEM_HPP

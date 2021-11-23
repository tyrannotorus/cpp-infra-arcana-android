// =============================================================================
// Copyright 2011-2021 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef ITEM_HPP
#define ITEM_HPP

#include <string>
#include <utility>
#include <vector>

#include "colors.hpp"
#include "dmg_range.hpp"
#include "gfx.hpp"
#include "global.hpp"
#include "item_curse.hpp"
#include "item_curse_ids.hpp"

class Inventory;
struct ItemAttackProp;
struct P;

namespace actor
{
class Actor;
}  // namespace actor

class Prop;

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

        virtual LgtSize lgt_size() const
        {
                return LgtSize::none;
        }

        std::string name(
                ItemNameType name_type,
                ItemNameInfo info = ItemNameInfo::yes,
                ItemNameAttackInfo attack_info =
                        ItemNameAttackInfo::none) const;

        std::vector<std::string> descr() const;

        std::string hit_mod_str(
                ItemNameAttackInfo attack_info,
                AbbrevItemAttackInfo abbrev = AbbrevItemAttackInfo::no) const;

        std::string dmg_str(
                ItemNameAttackInfo attack_info,
                AbbrevItemAttackInfo abbrev = AbbrevItemAttackInfo::no) const;

        // E.g. "(Off)" for Lanterns, or "(4/7)" for Pistols
        virtual std::string name_info_str() const
        {
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

        // Called when:
        // * Player walks into the same cell as the item,
        // * The item is dropped into the same cell as the player,
        // * The item is picked up,
        // * The item is found in an item container, but not picked up
        void on_player_found();

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

        void set_base_melee_dmg(const DmgRange& range)
        {
                m_base_melee_dmg = range;
        }

        void set_base_ranged_dmg(const DmgRange& range)
        {
                m_base_ranged_dmg = range;
        }

        DmgRange base_melee_dmg() const
        {
                return m_base_melee_dmg;
        }

        void reset_base_melee_dmg();

        void incr_base_melee_damage(int value);

        // Calculated damage taking into account things like player traits.
        DmgRange melee_dmg(const actor::Actor* attacker) const;
        DmgRange ranged_dmg(const actor::Actor* attacker) const;
        DmgRange thrown_dmg(const actor::Actor* attacker) const;

        ItemAttackProp& prop_applied_on_melee(
                const actor::Actor* attacker) const;

        ItemAttackProp& prop_applied_on_ranged(
                const actor::Actor* attacker) const;

        virtual void on_melee_kill(actor::Actor& actor_killed)
        {
                (void)actor_killed;
        }

        virtual void on_ranged_hit(actor::Actor& actor_hit)
        {
                (void)actor_hit;
        }

        void add_carrier_prop(Prop* prop, Verbose verbose);

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

        const std::vector<Prop*>& carrier_props() const
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
                DmgRange& range,
                const actor::Actor* const actor) const
        {
                (void)range;
                (void)actor;
        }

        ItemAttackProp* prop_applied_intr_attack(
                const actor::Actor* attacker) const;

        ItemData* m_data;

        actor::Actor* m_actor_carrying {nullptr};

        // Base damage (not including actor properties, player traits, etc)
        DmgRange m_base_melee_dmg;
        DmgRange m_base_ranged_dmg;

private:
        // Properties to apply on owning actor (when e.g. wearing the item, or
        // just keeping it in the inventory)
        std::vector<Prop*> m_carrier_props {};

        item_curse::Curse m_curse {};
};

class Trapez : public Item
{
public:
        Trapez(ItemData* item_data);

        ItemPrePickResult pre_pickup_hook() override;
};

class Armor : public Item
{
public:
        Armor(ItemData* item_data);

        ~Armor() = default;

        void save_hook() const override;
        void load_hook() override;

        Color interface_color() const override
        {
                return colors::gray();
        }

        std::string name_info_str() const override;

        int armor_points() const;

        int durability() const
        {
                return m_dur;
        }

        void set_max_durability()
        {
                m_dur = 100;
        }

        bool is_destroyed() const
        {
                return armor_points() <= 0;
        }

        void hit(int dmg);

protected:
        int m_dur;
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

class Wpn : public Item
{
public:
        Wpn(ItemData* item_data);

        virtual ~Wpn() = default;

        Wpn& operator=(const Wpn& other) = delete;

        void save_hook() const override;
        void load_hook() override;

        Color color() const override;

        Color interface_color() const override
        {
                return colors::gray();
        }

        std::string name_info_str() const override;

        const ItemData& ammo_data()
        {
                return *m_ammo_data;
        }

        int m_ammo_loaded;

protected:
        ItemData* m_ammo_data;
};

class SpikedMace : public Wpn
{
public:
        SpikedMace(ItemData* const item_data) :
                Wpn(item_data) {}

private:
        void on_melee_hit(actor::Actor& actor_hit, int dmg) override;
};

class PlayerGhoulClaw : public Wpn
{
public:
        PlayerGhoulClaw(ItemData* const item_data) :
                Wpn(item_data) {}

private:
        void on_melee_hit(actor::Actor& actor_hit, int dmg) override;

        void on_melee_kill(actor::Actor& actor_killed) override;
};

class Incinerator : public Wpn
{
public:
        Incinerator(ItemData* item_data);
        ~Incinerator() = default;

        void on_projectile_blocked(const P& pos) override;
};

class MiGoGun : public Wpn
{
public:
        MiGoGun(ItemData* item_data);
        ~MiGoGun() = default;

        void pre_ranged_attack() override;

protected:
        void specific_dmg_mod(
                DmgRange& range,
                const actor::Actor* actor) const override;
};

class RavenPeck : public Wpn
{
public:
        RavenPeck(ItemData* const item_data) :
                Wpn(item_data) {}

        void on_melee_hit(actor::Actor& actor_hit, int dmg) override;
};

class VampiricBite : public Wpn
{
public:
        VampiricBite(ItemData* const item_data) :
                Wpn(item_data) {}

        void on_melee_hit(actor::Actor& actor_hit, int dmg) override;
};

class MindLeechSting : public Wpn
{
public:
        MindLeechSting(ItemData* const item_data) :
                Wpn(item_data) {}

        void on_melee_hit(actor::Actor& actor_hit, int dmg) override;
};

class DustEngulf : public Wpn
{
public:
        DustEngulf(ItemData* const item_data) :
                Wpn(item_data) {}

        void on_melee_hit(actor::Actor& actor_hit, int dmg) override;
};

class SnakeVenomSpit : public Wpn
{
public:
        SnakeVenomSpit(ItemData* const item_data) :
                Wpn(item_data) {}

        void on_ranged_hit(actor::Actor& actor_hit) override;
};

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

        std::string name_info_str() const override;

        void set_full_ammo();

        void save_hook() const override;

        void load_hook() override;

        int m_ammo;
};

enum class MedBagAction
{
        treat_wound,
        sanitize_infection,
        END
};

class MedicalBag : public Item
{
public:
        MedicalBag(ItemData* item_data);

        ~MedicalBag() = default;

        void save_hook() const override;

        void load_hook() override;

        Color interface_color() const override
        {
                return colors::green();
        }

        std::string name_info_str() const override;

        void on_pickup_hook() override;

        ConsumeItem activate(actor::Actor* actor) override;

        void continue_action();

        void interrupted(ForceInterruptActions is_forced);

        void finish_current_action();

        int m_nr_supplies;

protected:
        MedBagAction choose_action() const;

        int tot_suppl_for_action(MedBagAction action) const;

        int tot_turns_for_action() const;

        int m_nr_turns_left_action;

        MedBagAction m_current_action;
};

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

        void on_equip_hook(Verbose verbose) override;

        void on_unequip_hook() override;

        void decr_turns_left(Inventory& carrier_inv);

protected:
        int m_nr_turns_left;
};

class Explosive : public Item
{
public:
        virtual ~Explosive() = default;

        Explosive() = delete;

        ConsumeItem activate(actor::Actor* actor) final;

        Color interface_color() const final
        {
                return colors::light_red();
        }

        virtual void on_std_turn_player_hold_ignited() = 0;
        virtual void on_thrown_ignited_landing(const P& p) = 0;
        virtual void on_player_paralyzed() = 0;
        virtual Color ignited_projectile_color() const = 0;
        virtual std::string str_on_player_throw() const = 0;

protected:
        Explosive(ItemData* const item_data) :
                Item(item_data),
                m_fuse_turns(-1) {}

        virtual int std_fuse_turns() const = 0;
        virtual void on_player_ignite() const = 0;

        int m_fuse_turns;
};

class Dynamite : public Explosive
{
public:
        Dynamite(ItemData* const item_data) :
                Explosive(item_data) {}

        void on_thrown_ignited_landing(const P& p) override;
        void on_std_turn_player_hold_ignited() override;
        void on_player_paralyzed() override;

        Color ignited_projectile_color() const override
        {
                return colors::light_red();
        }

        std::string str_on_player_throw() const override
        {
                return "I throw a lit dynamite stick.";
        }

protected:
        int std_fuse_turns() const override
        {
                return 6;
        }

        void on_player_ignite() const override;
};

class Molotov : public Explosive
{
public:
        Molotov(ItemData* const item_data) :
                Explosive(item_data) {}

        void on_thrown_ignited_landing(const P& p) override;
        void on_std_turn_player_hold_ignited() override;
        void on_player_paralyzed() override;

        Color ignited_projectile_color() const override
        {
                return colors::yellow();
        }

        std::string str_on_player_throw() const override
        {
                return "I throw a lit Molotov Cocktail.";
        }

protected:
        int std_fuse_turns() const override
        {
                return 12;
        }
        void on_player_ignite() const override;
};

class Flare : public Explosive
{
public:
        Flare(ItemData* const item_data) :
                Explosive(item_data) {}

        void on_thrown_ignited_landing(const P& p) override;
        void on_std_turn_player_hold_ignited() override;
        void on_player_paralyzed() override;

        Color ignited_projectile_color() const override
        {
                return colors::yellow();
        }

        std::string str_on_player_throw() const override
        {
                return "I throw a lit flare.";
        }

protected:
        int std_fuse_turns() const override
        {
                return 200;
        }
        void on_player_ignite() const override;
};

class SmokeGrenade : public Explosive
{
public:
        SmokeGrenade(ItemData* const item_data) :
                Explosive(item_data) {}

        void on_thrown_ignited_landing(const P& p) override;
        void on_std_turn_player_hold_ignited() override;
        void on_player_paralyzed() override;

        Color ignited_projectile_color() const override;

        std::string str_on_player_throw() const override
        {
                return "I throw a smoke grenade.";
        }

protected:
        int std_fuse_turns() const override
        {
                return 12;
        }

        void on_player_ignite() const override;
};

}  // namespace item

#endif  // ITEM_HPP

// =============================================================================
// Copyright 2011-2023 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef ITEM_WEAPON_HPP
#define ITEM_WEAPON_HPP

#include <string>

#include "colors.hpp"
#include "global.hpp"
#include "item.hpp"

struct P;
class WpnDmg;

namespace actor
{
class Actor;
}  // namespace actor

namespace item
{
struct ItemData;

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

        std::string name_info_str(ItemNameIdentified id_type) const override;

        const ItemData& ammo_data()
        {
                return *m_ammo_data;
        }

        int m_ammo_loaded;

protected:
        ItemData* m_ammo_data;
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

class MorphicBlaster : public Wpn
{
public:
        MorphicBlaster(ItemData* item_data);
        ~MorphicBlaster() = default;

        void pre_ranged_attack() override;

        void on_projectile_blocked(const P& pos) override;
};

class ElectricGun : public Wpn
{
public:
        ElectricGun(ItemData* item_data);
        ~ElectricGun() = default;

        void pre_ranged_attack() override;

protected:
        void specific_dmg_mod(
                WpnDmg& range,
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

class WaterBreath : public Wpn
{
public:
        WaterBreath(ItemData* item_data);
        ~WaterBreath() = default;

        void on_projectile_blocked(const P& pos) override;
};

class PharaohStaff : public Wpn
{
public:
        PharaohStaff(ItemData* item_data);

        void on_std_turn_in_inv_hook(InvType inv_type) override;

private:
        void on_mon_see_player_carrying(actor::Actor& mon) const;

        void on_melee_hit(actor::Actor& actor_hit, int dmg) override;
};

class ShadowDagger : public Wpn
{
public:
        ShadowDagger(ItemData* item_data);

        void on_melee_hit(actor::Actor& actor_hit, int dmg) override;

        void on_ranged_hit(actor::Actor& actor_hit) override;

private:
        bool is_radiant_creature(const actor::Actor& actor) const;

        void hit_normal_creature(actor::Actor& actor) const;

        void hit_radiant_creature(actor::Actor& actor) const;
};

class ZombieDust : public Wpn
{
public:
        ZombieDust(ItemData* const item_data) :
                Wpn(item_data) {}

        void on_ranged_hit(actor::Actor& actor_hit) override;
};

}  // namespace item

#endif  // ITEM_WEAPON_HPP

// =============================================================================
// Copyright 2011-2023 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef ITEM_EXPLOSIVE_HPP
#define ITEM_EXPLOSIVE_HPP

#include <string>

#include "colors.hpp"
#include "global.hpp"
#include "item.hpp"

struct P;

namespace actor
{
class Actor;
}  // namespace actor

namespace item
{
struct ItemData;

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

#endif  // ITEM_EXPLOSIVE_HPP

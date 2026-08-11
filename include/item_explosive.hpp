// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef ITEM_EXPLOSIVE_HPP
#define ITEM_EXPLOSIVE_HPP

#include <string>
#include <vector>

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

// An explosive is carried unlit, and lighting one splits it off the stack
// as a LIT item of its own, kept in the backpack like anything else (it is
// held in hand really - the backpack is just where it is listed). Its fuse
// burns down every turn wherever it is, so it must be thrown or put down
// before it runs out. Any number can be burning at once.
class Explosive : public Item
{
public:
        virtual ~Explosive() = default;

        Explosive() = delete;

        // Lights one from the stack (see the class comment)
        ConsumeItem activate(actor::Actor* actor) final;

        Color interface_color() const final
        {
                // Orange rather than red - the same colour a throw is
                // aimed in, and easier on the eyes in a list
                return colors::orange();
        }

        bool is_lit() const
        {
                return m_fuse_turns >= 0;
        }

        int fuse_turns() const
        {
                return m_fuse_turns;
        }

        // A lit explosive is a thing of its own - it must never be stacked
        // with the unlit ones it came from, nor with another lit one (they
        // burn down separately)
        bool can_stack_with(const Item& other) const override;

        // Puts a lit explosive down at the player's position: deliberately
        // (the [ drop ] pin), or because it fell from a paralyzed hand
        virtual void drop_lit_at_player(bool is_deliberate) = 0;

        // One short line on the state of a lit explosive, put last in its
        // description
        virtual std::string str_when_lit() const
        {
                return "The fuse is lit.";
        }

        virtual void on_std_turn_player_hold_ignited() = 0;
        virtual void on_thrown_ignited_landing(const P& p) = 0;
        virtual Color ignited_projectile_color() const = 0;
        virtual std::string str_on_player_throw() const = 0;

        // Reaction of one unlit explosive of this type when fire or an
        // explosion washes over the cell it lies in. The item has already
        // been taken off the map when this runs - in a stack, each item
        // reacts separately, one after the other.
        virtual void on_triggered_on_ground(const P& p) const = 0;

protected:
        Explosive(ItemData* const item_data) :
                Item(item_data),
                m_fuse_turns(-1) {}

        virtual int std_fuse_turns() const = 0;
        virtual void on_player_ignite() const = 0;

        std::string name_info_str(ItemNameIdentified id_type) const override;

        void save_hook() const override;

        void load_hook() override;

        // Removes this explosive from the player's inventory and destroys
        // it. NOTE: This object is deleted - do not touch it afterwards!
        void destroy_self();

        int m_fuse_turns;
};

// The lit explosives the player is carrying, in backpack order
std::vector<Explosive*> player_lit_explosives();

// Fire or an explosion washing over a map cell sets off the explosives
// lying there: the unlit item stack, and any lit dynamite (a catalyst
// does not care whether the fuse is already burning)
void trigger_explosives_on_ground_at(const P& pos);

// Sets off explosives lying in burning cells, or sharing a cell with a
// burning flare. Runs once per standard turn, after terrains have acted.
void trigger_explosives_on_burning_cells();

class Dynamite : public Explosive
{
public:
        Dynamite(ItemData* const item_data) :
                Explosive(item_data) {}

        void on_thrown_ignited_landing(const P& p) override;
        void on_std_turn_player_hold_ignited() override;
        void drop_lit_at_player(bool is_deliberate) override;
        void on_triggered_on_ground(const P& p) const override;

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
        void drop_lit_at_player(bool is_deliberate) override;
        void on_triggered_on_ground(const P& p) const override;

        Color ignited_projectile_color() const override
        {
                return colors::yellow();
        }

        std::string str_on_player_throw() const override
        {
                return "I throw a lit Molotov Cocktail.";
        }

        std::string str_when_lit() const override
        {
                return "The rag is lit.";
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
        void drop_lit_at_player(bool is_deliberate) override;
        void on_triggered_on_ground(const P& p) const override;

        Color ignited_projectile_color() const override
        {
                return colors::yellow();
        }

        std::string str_on_player_throw() const override
        {
                return "I throw a lit flare.";
        }

        std::string str_when_lit() const override
        {
                return "The flare is lit.";
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
        void drop_lit_at_player(bool is_deliberate) override;
        void on_triggered_on_ground(const P& p) const override;

        Color ignited_projectile_color() const override;

        std::string str_on_player_throw() const override
        {
                return "I throw a smoke grenade.";
        }

        std::string str_when_lit() const override
        {
                return "The grenade is ignited.";
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

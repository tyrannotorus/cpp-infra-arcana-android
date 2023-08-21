// =============================================================================
// Copyright 2011-2023 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef ITEM_ROD_HPP
#define ITEM_ROD_HPP

#include <string>
#include <vector>

#include "colors.hpp"
#include "global.hpp"
#include "item.hpp"

namespace actor
{
class Actor;
}  // namespace actor

namespace item
{
struct ItemData;
}  // namespace item

namespace rod
{
struct RodLook
{
        std::string name_plain;
        std::string name_a;
        Color color;
};

void init();

void save();
void load();

class Rod : public item::Item
{
public:
        Rod(item::ItemData* const item_data) :
                Item(item_data),
                m_nr_charge_turns_left(0) {}

        virtual ~Rod() = default;

        void save_hook() const final;

        void load_hook() final;

        ConsumeItem activate(actor::Actor* actor) final;

        Color interface_color() const final
        {
                return colors::violet();
        }

        std::string name_info_str(ItemNameIdentified id_type) const final;

        void on_std_turn_in_inv_hook(InvType inv_type) final;

        std::vector<std::string> descr_hook() const final;

        void identify(Verbose verbose) final;

        virtual std::string real_name() const = 0;

protected:
        virtual std::string descr_identified() const = 0;

        virtual void run_effect() = 0;

        virtual int nr_turns_to_recharge() const
        {
                return 250;
        }

        void set_max_charge_turns_left();

private:
        int m_nr_charge_turns_left;
};

class Curing : public Rod
{
public:
        Curing(item::ItemData* const item_data) :
                Rod(item_data) {}

        ~Curing() = default;

        std::string real_name() const override
        {
                return "Curing";
        }

protected:
        std::string descr_identified() const override
        {
                return (
                        "When activated, this device restores 3 hit points, "
                        "and cures blindness, deafness, poisoning, "
                        "infections, disease, weakening, and life sapping.");
        }

        void run_effect() override;
};

class Opening : public Rod
{
public:
        Opening(item::ItemData* const item_data) :
                Rod(item_data) {}

        ~Opening() = default;

        std::string real_name() const override
        {
                return "Opening";
        }

protected:
        std::string descr_identified() const override
        {
                return "When activated, this device opens all locks, lids and "
                       "doors in the surrounding area (except heavy doors "
                       "operated externally by a switch).";
        }

        void run_effect() override;
};

class Bless : public Rod
{
public:
        Bless(item::ItemData* const item_data) :
                Rod(item_data) {}

        ~Bless() = default;

        std::string real_name() const override
        {
                return "Blessing";
        }

protected:
        std::string descr_identified() const override
        {
                return "When activated, this device bends reality in favor of "
                       "the user for a while.";
        }

        void run_effect() override;
};

class CloudMinds : public Rod
{
public:
        CloudMinds(item::ItemData* const item_data) :
                Rod(item_data) {}

        ~CloudMinds() = default;

        std::string real_name() const override
        {
                return "Cloud Minds";
        }

protected:
        std::string descr_identified() const override
        {
                return "When activated, this device clouds the memories of "
                       "all creatures in the area, causing them to forget "
                       "the presence of the user.";
        }

        int nr_turns_to_recharge() const override
        {
                return 90;
        }

        void run_effect() override;
};

class Shockwave : public Rod
{
public:
        Shockwave(item::ItemData* const item_data) :
                Rod(item_data) {}

        ~Shockwave() = default;

        std::string real_name() const override
        {
                return "Shockwave";
        }

protected:
        std::string descr_identified() const override
        {
                return "When activated, this device generates a shock wave "
                       "which violently pushes away any adjacent creatures "
                       "and destroys structures.";
        }

        void run_effect() override;
};

}  // namespace rod

#endif  // ITEM_ROD_HPP

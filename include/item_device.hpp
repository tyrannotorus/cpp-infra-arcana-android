// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef ITEM_DEVICE_HPP
#define ITEM_DEVICE_HPP

#include <string>
#include <vector>

#include "item.hpp"
#include "colors.hpp"
#include "global.hpp"

namespace actor {
class Actor;
}  // namespace actor
namespace item {
struct ItemData;
}  // namespace item

namespace device
{
class Device : public item::Item
{
public:
        Device(item::ItemData* item_data);

        virtual ~Device() = default;

        ConsumeItem activate(actor::Actor* actor) override = 0;

        Color interface_color() const final
        {
                return colors::cyan();
        }

        void on_std_turn_in_inv_hook(const InvType inv_type) override
        {
                (void)inv_type;
        }

        void identify(Verbose verbose) override;
};

class StrangeDevice : public Device
{
public:
        StrangeDevice(item::ItemData* item_data);

        std::vector<std::string> descr_hook() const final;

        ConsumeItem activate(actor::Actor* actor) override;

        std::string name_inf_str() const override;

        void save_hook() const override;
        void load_hook() override;

        Condition condition;

private:
        virtual std::string descr_identified() const = 0;

        virtual ConsumeItem run_effect() = 0;
};

class Blaster : public StrangeDevice
{
public:
        Blaster(item::ItemData* const item_data) :
                StrangeDevice(item_data) {}

        ~Blaster() override = default;

private:
        std::string descr_identified() const override
        {
                return "When activated, this device blasts all visible "
                       "enemies with infernal power.";
        }

        ConsumeItem run_effect() override;
};

class Rejuvenator : public StrangeDevice
{
public:
        Rejuvenator(item::ItemData* const item_data) :
                StrangeDevice(item_data) {}

        ~Rejuvenator() override = default;

private:
        std::string descr_identified() const override
        {
                return "When activated, this device heals all wounds and "
                       "physical maladies. The procedure is very painful and "
                       "invasive however, and causes great shock to the user.";
        }

        ConsumeItem run_effect() override;
};

class Translocator : public StrangeDevice
{
public:
        Translocator(item::ItemData* const item_data) :
                StrangeDevice(item_data) {}

        ~Translocator() override = default;

private:
        std::string descr_identified() const override
        {
                return "When activated, this device teleports all visible "
                       "enemies to different locations.";
        }

        ConsumeItem run_effect() override;
};

class SentryDrone : public StrangeDevice
{
public:
        SentryDrone(item::ItemData* const item_data) :
                StrangeDevice(item_data) {}

        ~SentryDrone() override = default;

private:
        std::string descr_identified() const override
        {
                return "When activated, this device will \"come alive\" and "
                       "guard the user.";
        }

        ConsumeItem run_effect() override;
};

class Deafening : public StrangeDevice
{
public:
        Deafening(item::ItemData* const item_data) :
                StrangeDevice(item_data) {}

        ~Deafening() override = default;

private:
        std::string descr_identified() const override
        {
                return "When activated, this device causes temporary deafness "
                       "in all creatures in a large area (on the whole map), "
                       "except for the user.";
        }

        ConsumeItem run_effect() override;
};

class ForceField : public StrangeDevice
{
public:
        ForceField(item::ItemData* const item_data) :
                StrangeDevice(item_data) {}

        ~ForceField() override = default;

private:
        std::string descr_identified() const override
        {
                return "When activated, this device constructs a temporary "
                       "opaque barrier around the user, blocking all "
                       "physical matter. The barrier can only be created in "
                       "empty spaces (i.e. not in spaces occupied by "
                       "creatures, walls, etc).";
        }

        ConsumeItem run_effect() override;
};

class Lantern : public Device
{
public:
        Lantern(item::ItemData* item_data);

        ~Lantern() override = default;

        std::string name_inf_str() const override;

        ConsumeItem activate(actor::Actor* actor) override;

        void on_std_turn_in_inv_hook(InvType inv_type) override;

        void on_pickup_hook() override;

        LgtSize lgt_size() const override;

        void save_hook() const override;
        void load_hook() override;

        int nr_turns_left;
        bool is_activated;

private:
        void toggle();
};

}  // namespace device

#endif  // ITEM_DEVICE_HPP

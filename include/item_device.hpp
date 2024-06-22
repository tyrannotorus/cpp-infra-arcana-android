// =============================================================================
// Copyright 2011-2024 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef ITEM_DEVICE_HPP
#define ITEM_DEVICE_HPP

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

namespace device
{
class Device : public item::Item
{
public:
        Device(item::ItemData* item_data);

        virtual ~Device() = default;

        void save_hook() const final;
        void load_hook() final;

        std::string name_info_str(ItemNameIdentified id_type) const final;

        std::vector<std::string> descr_hook() const final;

        Color interface_color() const final
        {
                return colors::cyan();
        }

        ConsumeItem activate(actor::Actor* actor) final;

        void identify(Verbose verbose) final;

        Condition m_condition;

protected:
        virtual std::string descr_identified() const = 0;

        virtual ConsumeItem run_effect() = 0;
};

class Blaster : public Device
{
public:
        Blaster(item::ItemData* const item_data) :
                Device(item_data) {}

        ~Blaster() override = default;

private:
        std::string descr_identified() const override;

        ConsumeItem run_effect() override;
};

class Rejuvenator : public Device
{
public:
        Rejuvenator(item::ItemData* const item_data) :
                Device(item_data) {}

        ~Rejuvenator() override = default;

private:
        std::string descr_identified() const override;

        ConsumeItem run_effect() override;
};

class Translocator : public Device
{
public:
        Translocator(item::ItemData* const item_data) :
                Device(item_data) {}

        ~Translocator() override = default;

private:
        std::string descr_identified() const override;

        ConsumeItem run_effect() override;
};

class SentryDrone : public Device
{
public:
        SentryDrone(item::ItemData* const item_data) :
                Device(item_data) {}

        ~SentryDrone() override = default;

private:
        std::string descr_identified() const override
        {
                return "When activated, this device will \"come alive\" and "
                       "guard the user.";
        }

        ConsumeItem run_effect() override;
};

class Deafening : public Device
{
public:
        Deafening(item::ItemData* const item_data) :
                Device(item_data) {}

        ~Deafening() override = default;

private:
        std::string descr_identified() const override;

        ConsumeItem run_effect() override;
};

class ForceField : public Device
{
public:
        ForceField(item::ItemData* const item_data) :
                Device(item_data) {}

        ~ForceField() override = default;

private:
        std::string descr_identified() const override;

        ConsumeItem run_effect() override;
};

}  // namespace device

#endif  // ITEM_DEVICE_HPP

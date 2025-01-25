// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
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
                Item(item_data) {}

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
        int m_nr_charge_turns_left {0};
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
        std::string descr_identified() const override;

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
        std::string descr_identified() const override;

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
        std::string descr_identified() const override;

        void run_effect() override;
};

class Deafening : public Rod
{
public:
        Deafening(item::ItemData* const item_data) :
                Rod(item_data) {}

        ~Deafening() = default;

        std::string real_name() const override
        {
                return "Deafening";
        }

protected:
        std::string descr_identified() const override;

        void run_effect() override;
};

class DoorCreation : public Rod
{
public:
        DoorCreation(item::ItemData* const item_data) :
                Rod(item_data) {}

        ~DoorCreation() = default;

        std::string real_name() const override
        {
                return "Gateways";
        }

protected:
        std::string descr_identified() const override;

        void run_effect() override;
};

class Unbinding : public Rod
{
public:
        Unbinding(item::ItemData* const item_data) :
                Rod(item_data) {}

        ~Unbinding() = default;

        std::string real_name() const override
        {
                return "Unbinding";
        }

protected:
        std::string descr_identified() const override;

        void run_effect() override;
};

class Mist : public Rod
{
public:
        Mist(item::ItemData* const item_data) :
                Rod(item_data) {}

        ~Mist() = default;

        std::string real_name() const override
        {
                return "Mist";
        }

protected:
        std::string descr_identified() const override;

        void run_effect() override;
};

class MiGoHypno : public Rod
{
public:
        MiGoHypno(item::ItemData* const item_data) :
                Rod(item_data) {}

        ~MiGoHypno() = default;

        std::string real_name() const override
        {
                return "Sleep";
        }

protected:
        std::string descr_identified() const override;

        void run_effect() override;
};

class Displacement : public Rod
{
public:
        Displacement(item::ItemData* const item_data) :
                Rod(item_data) {}

        ~Displacement() = default;

        std::string real_name() const override
        {
                return "Displacement";
        }

protected:
        std::string descr_identified() const override;

        void run_effect() override;
};

}  // namespace rod

#endif  // ITEM_ROD_HPP

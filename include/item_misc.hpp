// =============================================================================
// Copyright 2011-2024 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef ITEM_MISC_HPP
#define ITEM_MISC_HPP

#include <string>

#include "colors.hpp"
#include "global.hpp"
#include "item.hpp"
#include "item_curse_ids.hpp"
#include "random.hpp"
#include "sound.hpp"

namespace actor
{
class Actor;
}  // namespace actor

namespace item
{
struct ItemData;

enum class MedBagAction
{
        quick_patch_up,
        treat_wound,
        sanitize_infection,
        END
};

class Trapezohedron : public Item
{
public:
        Trapezohedron(ItemData* item_data);

        ItemPrePickResult pre_pickup_hook() override;
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

        std::string name_info_str(ItemNameIdentified id_type) const override;

        void on_pickup_hook() override;

        ConsumeItem activate(actor::Actor* actor) override;

        void continue_action();

        void interrupted(ForceInterruptActions is_forced);

        void finish_current_action();

        void randomize_nr_supplies();

        int nr_supplies() const
        {
                return m_nr_supplies;
        }

protected:
        // Two different ways of choosing action, depending on user settings:
        MedBagAction choose_action_auto() const;
        MedBagAction choose_action_manual() const;

        int tot_suppl_for_action(MedBagAction action) const;

        int tot_turns_for_action(MedBagAction action) const;

        void stop_action();

        static const int m_max_starting_supplies {24};
        static const int m_hp_restored_by_quick_patch_up {10};

        int m_nr_supplies {m_max_starting_supplies};
        int m_nr_turns_left_action {-1};
        MedBagAction m_current_action {MedBagAction::END};
};

class Lantern : public Item
{
public:
        Lantern(item::ItemData* item_data);

        ~Lantern() override = default;

        void save_hook() const override;

        void load_hook() override;

        std::string name_info_str(ItemNameIdentified id_type) const override;

        ConsumeItem activate(actor::Actor* actor) override;

        bool is_activated() const
        {
                return m_is_activated;
        }

        int nr_turns_left() const
        {
                return m_nr_turns_left;
        }

        void on_std_turn_in_inv_hook(InvType inv_type) override;

        void on_pickup_hook() override;

        LightSize light_size() const override;

        void randomize_duration();

private:
        void toggle();

        static const int m_max_turns_left {150};

        int m_nr_turns_left {m_max_turns_left};
        bool m_is_activated {false};
};

class ReflTalisman : public Item
{
public:
        ReflTalisman(ItemData* item_data);

        void on_pickup_hook() override;

        void on_removed_from_inv_hook() override;
};

class ResurrectTalisman : public Item
{
public:
        ResurrectTalisman(ItemData* item_data);

        bool is_curse_allowed(item_curse::Id id) const override
        {
                (void)id;

                // This item is destroyed on use, it's too messy to also
                // have to deactivate the curse
                return false;
        }
};

class TeleCtrlTalisman : public Item
{
public:
        TeleCtrlTalisman(ItemData* item_data);

        bool is_curse_allowed(item_curse::Id id) const override
        {
                return id != item_curse::Id::teleport;
        }

        void on_pickup_hook() override;

        void on_removed_from_inv_hook() override;
};

class HornOfMaliceHeard : public SndHeardEffect
{
public:
        HornOfMaliceHeard() = default;

        ~HornOfMaliceHeard() = default;

        void run(actor::Actor& actor) const override;
};

class HornOfMalice : public Item
{
public:
        HornOfMalice(ItemData* item_data);

        std::string name_info_str(ItemNameIdentified id_type) const override;

        void save_hook() const override;

        void load_hook() override;

        ConsumeItem activate(actor::Actor* actor) override;

private:
        int m_charges;
};

class HornOfBanishmentHeard : public SndHeardEffect
{
public:
        HornOfBanishmentHeard() = default;

        ~HornOfBanishmentHeard() = default;

        void run(actor::Actor& actor) const override;
};

class HornOfBanishment : public Item
{
public:
        HornOfBanishment(ItemData* item_data);

        std::string name_info_str(ItemNameIdentified id_type) const override;

        void save_hook() const override;

        void load_hook() override;

        ConsumeItem activate(actor::Actor* actor) override;

private:
        int m_charges;
};

class HolySymbol : public Item
{
public:
        HolySymbol(ItemData* item_data);

        ConsumeItem activate(actor::Actor* actor) override;

        void on_std_turn_in_inv_hook(InvType inv_type) override;

        std::string name_info_str(ItemNameIdentified id_type) const override;

        void save_hook() const override;

        void load_hook() override;

private:
        void run_effect();

        Range nr_turns_to_recharge() const;

        int m_nr_charge_turns_left {0};
        bool m_has_failed_attempt {false};
};

class Clockwork : public Item
{
public:
        Clockwork(ItemData* item_data);

        ConsumeItem activate(actor::Actor* actor) override;

        std::string name_info_str(ItemNameIdentified id_type) const override;

        void save_hook() const override;

        void load_hook() override;

private:
        int m_charges;
};

class OrbOfLife : public Item
{
public:
        OrbOfLife(ItemData* item_data);

        void on_pickup_hook() override;

        void on_removed_from_inv_hook() override;
};

class Necronomicon : public Item
{
public:
        Necronomicon(ItemData* item_data);

        ItemPrePickResult pre_pickup_hook() override;

        void on_std_turn_in_inv_hook(InvType inv_type) override;

        void on_pickup_hook() override;
};

class WitchEye : public Item
{
public:
        WitchEye(ItemData* const item_data) :
                Item(item_data) {}

        ConsumeItem activate(actor::Actor* actor) override;

        // TODO: Consider interface color for "Other Curiosities". Should they
        // all share the same color?
        // Color interface_color() const final
        // {
        //         return colors::light_red();
        // }
};

class BoneCharm : public Item
{
public:
        BoneCharm(ItemData* const item_data) :
                Item(item_data) {}

        ConsumeItem activate(actor::Actor* actor) override;
};

// class FlaskOfDamning : public Item
// {
// public:
//         FlaskOfDamning(ItemData* const item_data) :
//                 Item(item_data) {}

//         ConsumeItem activate(actor::Actor* actor);
// };

// class ObsidianCharm : public Item
// {
// public:
//         ObsidianCharm(ItemData* const item_data) :
//                 Item(item_data) {}

//         ConsumeItem activate(actor::Actor* actor);
// };

class FluctuatingMaterial : public Item
{
public:
        FluctuatingMaterial(ItemData* const item_data) :
                Item(item_data) {}

        ConsumeItem activate(actor::Actor* actor) override;
};

// class BatWingSalve : public Item
// {
// public:
//         BatWingSalve(ItemData* const item_data) :
//                 Item(item_data) {}

//         ConsumeItem activate(actor::Actor* actor);
// };

class AstralOpium : public Item
{
public:
        AstralOpium(ItemData* const item_data) :
                Item(item_data) {}

        ConsumeItem activate(actor::Actor* actor) override;
};

}  // namespace item

#endif  // ITEM_MISC_HPP

// =============================================================================
// Copyright 2011-2023 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef ITEM_POTION_HPP
#define ITEM_POTION_HPP

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
struct P;

namespace potion
{
enum class PotionAlignment
{
        good,
        bad
};

void init();

void save();
void load();

class Potion : public item::Item
{
public:
        Potion(item::ItemData* item_data);

        virtual ~Potion() = default;

        Color interface_color() const final
        {
                return colors::light_blue();
        }

        std::string name_info_str(ItemNameIdentified id_type) const final;

        ConsumeItem activate(actor::Actor* actor) final;

        std::vector<std::string> descr_hook() const final;

        void on_collide(const P& pos, actor::Actor* actor);

        void identify(Verbose verbose) final;

        void reveal_alignment() const;

        virtual std::string real_name() const = 0;

        virtual PotionAlignment alignment() const = 0;

protected:
        virtual std::string descr_identified() const = 0;

        virtual void collide_hook(const P& pos, actor::Actor* actor) = 0;

        virtual void quaff_impl(actor::Actor& actor) = 0;

private:
        std::string alignment_str() const;
};

class Vitality : public Potion
{
public:
        Vitality(item::ItemData* const item_data) :
                Potion(item_data) {}
        ~Vitality() = default;

        void quaff_impl(actor::Actor& actor) override;

        std::string real_name() const override
        {
                return "Vitality";
        }

private:
        std::string descr_identified() const override
        {
                return (
                        "This elixir fully restores all hit points, heals all "
                        "wounds, and cures blindness, deafness, poisoning, "
                        "infections, disease, weakening, and life sapping. "
                        "Also, for some duration after consuming the potion, "
                        "+1 extra hit point is healed per turn, and there is "
                        "10% chance per turn to heal one wound.");
        }

        PotionAlignment alignment() const override
        {
                return PotionAlignment::good;
        }

        void collide_hook(const P& pos, actor::Actor* actor) override;
};

class Spirit : public Potion
{
public:
        Spirit(item::ItemData* const item_data) :
                Potion(item_data) {}
        ~Spirit() = default;

        void quaff_impl(actor::Actor& actor) override;

        std::string real_name() const override
        {
                return "Spirit";
        }

private:
        std::string descr_identified() const override
        {
                return "Restores the spirit, and cures spirit sapping.";
        }

        PotionAlignment alignment() const override
        {
                return PotionAlignment::good;
        }

        void collide_hook(const P& pos, actor::Actor* actor) override;
};

class Blindness : public Potion
{
public:
        Blindness(item::ItemData* const item_data) :
                Potion(item_data) {}
        ~Blindness() = default;

        void quaff_impl(actor::Actor& actor) override;

        std::string real_name() const override
        {
                return "Blindness";
        }

private:
        std::string descr_identified() const override
        {
                return "Causes temporary loss of vision.";
        }

        PotionAlignment alignment() const override
        {
                return PotionAlignment::bad;
        }

        void collide_hook(const P& pos, actor::Actor* actor) override;
};

class Paral : public Potion
{
public:
        Paral(item::ItemData* const item_data) :
                Potion(item_data) {}
        ~Paral() = default;

        void quaff_impl(actor::Actor& actor) override;

        std::string real_name() const override
        {
                return "Paralyzation";
        }

private:
        std::string descr_identified() const override
        {
                return "Causes paralysis.";
        }

        PotionAlignment alignment() const override
        {
                return PotionAlignment::bad;
        }

        void collide_hook(const P& pos, actor::Actor* actor) override;
};

class Disease : public Potion
{
public:
        Disease(item::ItemData* const item_data) :
                Potion(item_data) {}
        ~Disease() = default;

        void quaff_impl(actor::Actor& actor) override;

        std::string real_name() const override
        {
                return "Disease";
        }

private:
        std::string descr_identified() const override
        {
                return "This foul liquid causes a horrible disease.";
        }

        PotionAlignment alignment() const override
        {
                return PotionAlignment::bad;
        }

        void collide_hook(const P& pos, actor::Actor* const actor) override
        {
                (void)pos;
                (void)actor;
        }
};

class Conf : public Potion
{
public:
        Conf(item::ItemData* const item_data) :
                Potion(item_data) {}
        ~Conf() = default;
        void quaff_impl(actor::Actor& actor) override;

        std::string real_name() const override
        {
                return "Confusion";
        }

private:
        std::string descr_identified() const override
        {
                return "Causes confusion.";
        }

        PotionAlignment alignment() const override
        {
                return PotionAlignment::bad;
        }

        void collide_hook(const P& pos, actor::Actor* actor) override;
};

class Fortitude : public Potion
{
public:
        Fortitude(item::ItemData* const item_data) :
                Potion(item_data) {}

        ~Fortitude() = default;

        void quaff_impl(actor::Actor& actor) override;

        std::string real_name() const override
        {
                return "Fortitude";
        }

private:
        std::string descr_identified() const override
        {
                return (
                        "Gives the consumer complete peace and clarity of "
                        "mind, and cures mind sapping.");
        }

        PotionAlignment alignment() const override
        {
                return PotionAlignment::good;
        }

        void collide_hook(const P& pos, actor::Actor* actor) override;
};

class Poison : public Potion
{
public:
        Poison(item::ItemData* const item_data) :
                Potion(item_data) {}
        ~Poison() = default;

        void quaff_impl(actor::Actor& actor) override;

        std::string real_name() const override
        {
                return "Poison";
        }

private:
        std::string descr_identified() const override
        {
                return "A deadly brew.";
        }

        PotionAlignment alignment() const override
        {
                return PotionAlignment::bad;
        }

        void collide_hook(const P& pos, actor::Actor* actor) override;
};

class Insight : public Potion
{
public:
        Insight(item::ItemData* const item_data) :
                Potion(item_data) {}
        ~Insight() = default;

        void quaff_impl(actor::Actor& actor) override;

        std::string real_name() const override
        {
                return "Insight";
        }

private:
        std::string descr_identified() const override
        {
                return (
                        "This strange concoction causes a sudden flash of "
                        "intuition.");
        }

        PotionAlignment alignment() const override
        {
                return PotionAlignment::good;
        }

        void collide_hook(const P& pos, actor::Actor* const actor) override
        {
                (void)pos;
                (void)actor;
        }
};

class RFire : public Potion
{
public:
        RFire(item::ItemData* const item_data) :
                Potion(item_data) {}
        ~RFire() = default;

        void quaff_impl(actor::Actor& actor) override;

        std::string real_name() const override
        {
                return "Fire Resistance";
        }

private:
        std::string descr_identified() const override
        {
                return "Protects the consumer from fire.";
        }

        PotionAlignment alignment() const override
        {
                return PotionAlignment::good;
        }

        void collide_hook(const P& pos, actor::Actor* actor) override;
};

class Curing : public Potion
{
public:
        Curing(item::ItemData* const item_data) :
                Potion(item_data) {}
        ~Curing() = default;

        void quaff_impl(actor::Actor& actor) override;

        std::string real_name() const override
        {
                return "Curing";
        }

private:
        std::string descr_identified() const override
        {
                return (
                        "Restores 3 hit points, and cures blindness, deafness, "
                        "poisoning, infections, disease, weakening, and "
                        "life sapping.");
        }

        PotionAlignment alignment() const override
        {
                return PotionAlignment::good;
        }

        void collide_hook(const P& pos, actor::Actor* actor) override;
};

class RElec : public Potion
{
public:
        RElec(item::ItemData* const item_data) :
                Potion(item_data) {}
        ~RElec() = default;

        void quaff_impl(actor::Actor& actor) override;

        std::string real_name() const override
        {
                return "Insulation";
        }

private:
        std::string descr_identified() const override
        {
                return "Protects the consumer from electricity.";
        }

        PotionAlignment alignment() const override
        {
                return PotionAlignment::good;
        }

        void collide_hook(const P& pos, actor::Actor* actor) override;
};

class Descent : public Potion
{
public:
        Descent(item::ItemData* const item_data) :
                Potion(item_data) {}
        ~Descent() = default;

        void quaff_impl(actor::Actor& actor) override;

        std::string real_name() const override
        {
                return "Descent";
        }

private:
        std::string descr_identified() const override
        {
                return (
                        "A bizarre liquid that causes the consumer to "
                        "dematerialize and sink through the ground.");
        }

        // TODO: Not sure about the alignment for this one...
        PotionAlignment alignment() const override
        {
                return PotionAlignment::good;
        }

        void collide_hook(const P& pos, actor::Actor* const actor) override
        {
                (void)pos;
                (void)actor;
        }
};

}  // namespace potion

#endif  // ITEM_POTION_HPP

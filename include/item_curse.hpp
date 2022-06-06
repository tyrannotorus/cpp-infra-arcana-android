// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef ITEM_CURSE_HPP
#define ITEM_CURSE_HPP

#include <memory>
#include <string>
#include <vector>

#include "item_curse_ids.hpp"

namespace item
{
class Item;
}  // namespace item

namespace item_curse
{
class CurseImpl;

class Curse
{
public:
        Curse() = default;

        Curse(Curse&& other);

        Curse(std::unique_ptr<CurseImpl> curse_impl);

        Curse& operator=(Curse&& other);

        Curse& operator=(Curse&) = delete;

        Id id() const;

        void save() const;

        void load();

        bool is_active() const
        {
                if (!m_curse_impl) {
                        return false;
                }

                return m_turn_countdown == 0;
        }

        void on_new_turn(const item::Item& item);

        void on_player_reached_new_dlvl();

        void on_item_picked_up(const item::Item& item);

        void on_item_dropped();

        void on_curse_end();

        int affect_weight(int weight) const;

        std::string descr() const;

private:
        void print_trigger_msg(const item::Item& item) const;

        void print_warning_msg(const item::Item& item) const;

        int m_dlvl_countdown {-1};
        int m_turn_countdown {-1};

        int m_warning_dlvl_countdown {-1};
        int m_warning_turn_countdown {-1};

        std::unique_ptr<CurseImpl> m_curse_impl {nullptr};
};

void init();

void save();

void load();

Curse try_make_random_free_curse(const item::Item& item);

class CurseImpl
{
public:
        virtual ~CurseImpl() = default;

        virtual Id id() const = 0;

        virtual void save() const {}

        virtual void load() {}

        virtual void on_new_turn_active(const item::Item& item)
        {
                (void)item;
        }

        virtual void on_start(const item::Item& item)
        {
                (void)item;
        }

        virtual void on_stop() {}

        virtual int affect_weight(const int weight)
        {
                return weight;
        }

        virtual std::string descr() const = 0;

        // NOTE: This is only necessary to implement if the curse message itself
        // does not print a message (e.g. a curse lowering the player's HP will
        // trigger a message about this, so it does not need a specific message)
        virtual std::string curse_msg(const item::Item& item) const
        {
                (void)item;

                return "";
        }
};

class HitChancePenalty : public CurseImpl
{
public:
        Id id() const override
        {
                return Id::hit_chance_penalty;
        }

        void on_start(const item::Item& item) override;

        void on_stop() override;

        std::string descr() const override;
};

class IncreasedShock : public CurseImpl
{
public:
        Id id() const override
        {
                return Id::increased_shock;
        }

        void on_start(const item::Item& item) override;

        void on_stop() override;

        std::string descr() const override;
};

class Heavy : public CurseImpl
{
public:
        Id id() const override
        {
                return Id::heavy;
        }

        int affect_weight(int weight) override;

        std::string descr() const override;

        std::string curse_msg(const item::Item& item) const override;
};

class Shriek : public CurseImpl
{
public:
        Shriek();

        Id id() const override
        {
                return Id::shriek;
        }

        void on_start(const item::Item& item) override;

        void on_new_turn_active(const item::Item& item) override;

        std::string descr() const override;

private:
        void shriek(const item::Item& item) const;

        std::vector<std::string> m_words {};
};

class Teleport : public CurseImpl
{
public:
        Id id() const override
        {
                return Id::teleport;
        }

        void on_start(const item::Item& item) override;

        void on_new_turn_active(const item::Item& item) override;

        std::string descr() const override;

private:
        void teleport(const item::Item& item) const;
};

class Summon : public CurseImpl
{
public:
        Id id() const override
        {
                return Id::summon;
        }

        void on_new_turn_active(const item::Item& item) override;

        std::string curse_msg(const item::Item& item) const override;

        std::string descr() const override;

private:
        void summon(const item::Item& item) const;
};

class Fire : public CurseImpl
{
public:
        Id id() const override
        {
                return Id::fire;
        }

        void on_start(const item::Item& item) override;

        void on_new_turn_active(const item::Item& item) override;

        std::string descr() const override;

private:
        void run_fire(const item::Item& item) const;
};

class CannotRead : public CurseImpl
{
public:
        Id id() const override
        {
                return Id::cannot_read;
        }

        void on_start(const item::Item& item) override;

        void on_stop() override;

        std::string descr() const override;
};

class LightSensitive : public CurseImpl
{
public:
        Id id() const override
        {
                return Id::light_sensitive;
        }

        void on_start(const item::Item& item) override;

        void on_stop() override;

        std::string descr() const override;
};

}  // namespace item_curse

#endif  // ITEM_CURSE_HPP

// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef ACTOR_MON_HPP
#define ACTOR_MON_HPP

#include <memory>
#include <string>
#include <vector>

#include "actor.hpp"
#include "global.hpp"
#include "item.hpp"
#include "sound.hpp"
#include "actor_data.hpp"
#include "audio_data.hpp"
#include "colors.hpp"
#include "gfx.hpp"
#include "spells.hpp"

struct P;

struct AiAttData
{
        item::Wpn* wpn {nullptr};
        bool is_melee {false};
};

struct AiAvailAttacksData
{
        std::vector<item::Wpn*> weapons = {};
        bool should_reload = false;
        bool is_melee = false;
};

namespace actor
{
enum class AwareSource
{
        seeing,
        attacked,
        spell_victim,
        other
};

std::string get_cultist_phrase();

std::string get_cultist_aware_msg_seen(const Actor& actor);

std::string get_cultist_aware_msg_hidden();

class Mon : public Actor
{
public:
        virtual ~Mon();

        virtual DidAction on_act()
        {
                return DidAction::no;
        }

        bool is_sneaking() const;

        Color color() const override;

        SpellSkill spell_skill(SpellId id) const override;

        AiAvailAttacksData avail_attacks(Actor& defender) const;

        AiAttData choose_attack(const AiAvailAttacksData& avail_attacks) const;

        DidAction try_attack(Actor& defender);

        void hear_sound(const Snd& snd);

        void become_aware_player(AwareSource source, int factor = 1);

        void become_wary_player();

        void set_player_aware_of_me(int duration_factor = 1);

        std::vector<Actor*> foes_aware_of() const;

        std::string aware_msg_mon_seen() const;

        std::string aware_msg_mon_hidden() const;

        virtual audio::SfxId aware_sfx_mon_seen() const
        {
                return m_data->aware_sfx_mon_seen;
        }

        virtual audio::SfxId aware_sfx_mon_hidden() const
        {
                return m_data->aware_sfx_mon_hidden;
        }

        void speak_phrase(AlertsMon alerts_others);

        bool is_leader_of(const Actor* actor) const override;
        bool is_actor_my_leader(const Actor* actor) const override;

        void add_spell(SpellSkill skill, Spell* spell);

protected:
        void print_player_see_mon_become_aware_msg() const;

        void print_player_see_mon_become_wary_msg() const;

        bool is_friend_blocking_ranged_attack(const P& target_pos) const;

        item::Wpn* avail_wielded_melee() const;
        item::Wpn* avail_wielded_ranged() const;
        std::vector<item::Wpn*> avail_intr_melee() const;
        std::vector<item::Wpn*> avail_intr_ranged() const;

        bool should_reload(const item::Wpn& wpn) const;

        int nr_mon_in_group() const;
};

class Ape : public Mon
{
private:
        // TODO: This should be a property
        DidAction on_act() override;

        int m_frenzy_cooldown {0};
};

class Khephren : public Mon
{
private:
        // TODO: This should be a property
        DidAction on_act() override;

        bool m_has_summoned_locusts {false};
};

class StrangeColor : public Mon
{
public:
        StrangeColor() = default;

        ~StrangeColor() = default;

        // TODO: This should be a property
        Color color() const override;
};

class SpectralWpn : public Mon
{
public:
        SpectralWpn() = default;

        ~SpectralWpn() = default;

        void on_death() override;

        std::string name_the() const override;

        std::string name_a() const override;

        char character() const override;

        gfx::TileId tile() const override;

        std::string descr() const override;

private:
        std::unique_ptr<item::Item> m_discarded_item {};
};

}  // namespace actor

#endif  // ACTOR_MON_HPP

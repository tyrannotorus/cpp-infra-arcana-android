// =============================================================================
// Copyright 2011-2024 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef SPELLS_HPP
#define SPELLS_HPP

#include <memory>
#include <string>
#include <vector>

#include "global.hpp"
#include "random.hpp"

class Spell;
struct P;

namespace actor
{
class Actor;
}  // namespace actor

namespace terrain
{
enum class DidOpen;
enum class DidClose;
}  // namespace terrain

namespace item
{
class Item;
}  // namespace item

enum class SpellId
{
        // Available for player and monsters
        aura_of_decay,
        darkbolt,
        aza_gaze,
        enfeeble,
        heal,
        pestilence,
        slow,
        haste,
        spell_shield,
        teleport,
        terrify,

        // Player only
        spectral_weapons,
        bless,
        premonition,
        erudition,
        identify,
        light,
        cataclysm,
        control_object,
        resistance,
        invis,
        see_invis,
        transmut,
        thorns,
        sacrifice_life,
        blood_tempering,
        crimson_passage,

        // Exorcist background
        cleansing_fire,
        sanctuary,
        purge,

        // Ghoul background
        frenzy,

        // Flagellant background
        shed_impurity,

        // Monsters only
        curse,
        force_bolt,
        burn,
        deafen,
        disease,
        knockback,
        mi_go_hypno,
        summon,
        summon_tentacles,
        heal_others,

        END
};

enum class SpellDomain
{
        clairvoyance,
        enchantment,
        invocation,
        transmutation,
        blood,

        END
};

enum class SpellSkill
{
        basic,
        expert,
        master,
        transcendent
};

enum class SpellSrc
{
        learned,
        manuscript,
        item
};

enum class SpellShock
{
        none,
        mild,
        disturbing,
        severe
};

// Does the spell cost spirit or hit points to cast?
enum class SpellCostType
{
        spirit,
        hit_points
};

namespace spells
{
Spell* make(SpellId spell_id);

SpellId str_to_spell_id(const std::string& str);

std::string spell_domain_title(SpellDomain domain);

SpellSkill str_to_spell_skill_id(const std::string& str);

std::string skill_to_str(SpellSkill skill);

ShockSrc spell_domain_to_shock_type(SpellDomain domain);

terrain::DidOpen run_opening_spell_effect_at(
        const P& pos,
        SpellSkill skill);

terrain::DidClose run_close_spell_effect_at(
        const P& pos,
        SpellSkill skill);

}  // namespace spells

class Spell
{
public:
        Spell() = default;

        virtual ~Spell() = default;

        void cast(
                actor::Actor* caster,
                SpellSkill skill,
                SpellSrc spell_src,
                const std::vector<actor::Actor*>& seen_targets) const;

        virtual bool allow_mon_cast_now(
                const actor::Actor& mon,
                const std::vector<actor::Actor*>& seen_targets) const
        {
                (void)mon;
                (void)seen_targets;

                return false;
        }

        virtual int mon_cooldown() const
        {
                return 3;
        }

        virtual bool player_can_learn() const = 0;

        virtual std::string name() const = 0;

        virtual SpellId id() const = 0;

        virtual SpellDomain domain() const = 0;

        // Casting a memorized tenebrous spell disables it (i.e. single use,
        // until it it re-enabled).
        virtual bool is_tenebrous() const
        {
                return false;
        }

        virtual bool can_be_improved_with_skill() const
        {
                return true;
        }

        std::vector<std::string> descr(
                SpellSkill skill,
                SpellSrc spell_src) const;

        std::string domain_descr() const;

        virtual std::vector<std::string> descr_specific(
                SpellSkill skill) const = 0;

        Range cost_range(
                SpellSkill skill,
                const actor::Actor* caster = nullptr) const;

        virtual SpellCostType cost_type() const
        {
                return SpellCostType::spirit;
        }

        int shock_value() const;

        virtual SpellShock shock_type() const = 0;

        virtual void run_effect(
                actor::Actor* caster,
                SpellSkill skill,
                const std::vector<actor::Actor*>& seen_targets) const = 0;

protected:
        virtual int base_max_cost(
                SpellSkill skill,
                const actor::Actor* caster) const = 0;

        virtual bool is_noisy(SpellSkill skill) const = 0;

        void on_resist(actor::Actor& target) const;

        bool m_is_disabled {false};
};

class SpellCurse : public Spell
{
public:
        SpellCurse() = default;

        bool allow_mon_cast_now(
                const actor::Actor& mon,
                const std::vector<actor::Actor*>& seen_targets) const override;

        int mon_cooldown() const override;

        std::string name() const override
        {
                return "Curse";
        }

        SpellId id() const override
        {
                return SpellId::curse;
        }

        SpellDomain domain() const override
        {
                return SpellDomain::enchantment;
        }

        SpellShock shock_type() const override
        {
                return SpellShock::mild;
        }

        bool player_can_learn() const override
        {
                return false;
        }

        std::vector<std::string> descr_specific(
                SpellSkill skill) const override
        {
                (void)skill;
                return {};
        }

        void run_effect(
                actor::Actor* caster,
                SpellSkill skill,
                const std::vector<actor::Actor*>& seen_targets) const override;

protected:
        int base_max_cost(
                SpellSkill skill,
                const actor::Actor* caster) const override;

        bool is_noisy(const SpellSkill skill) const override
        {
                (void)skill;

                return true;
        }
};

class SpellEnfeeble : public Spell
{
public:
        SpellEnfeeble() = default;

        bool allow_mon_cast_now(
                const actor::Actor& mon,
                const std::vector<actor::Actor*>& seen_targets) const override;

        int mon_cooldown() const override;

        std::string name() const override
        {
                return "Enfeeble";
        }

        SpellId id() const override
        {
                return SpellId::enfeeble;
        }

        SpellDomain domain() const override
        {
                return SpellDomain::enchantment;
        }

        SpellShock shock_type() const override
        {
                return SpellShock::mild;
        }

        bool player_can_learn() const override
        {
                return true;
        }

        std::vector<std::string> descr_specific(
                SpellSkill skill) const override;

        void run_effect(
                actor::Actor* caster,
                SpellSkill skill,
                const std::vector<actor::Actor*>& seen_targets) const override;

protected:
        Range duration_range(SpellSkill skill) const;

        int base_max_cost(
                SpellSkill skill,
                const actor::Actor* caster) const override;

        bool is_noisy(const SpellSkill skill) const override
        {
                (void)skill;

                return true;
        }
};

class SpellSlow : public Spell
{
public:
        SpellSlow() = default;

        bool allow_mon_cast_now(
                const actor::Actor& mon,
                const std::vector<actor::Actor*>& seen_targets) const override;

        int mon_cooldown() const override;

        std::string name() const override
        {
                return "Slow";
        }

        SpellId id() const override
        {
                return SpellId::slow;
        }

        SpellDomain domain() const override
        {
                return SpellDomain::enchantment;
        }

        SpellShock shock_type() const override
        {
                return SpellShock::mild;
        }

        bool player_can_learn() const override
        {
                return true;
        }

        std::vector<std::string> descr_specific(
                SpellSkill skill) const override;

        void run_effect(
                actor::Actor* caster,
                SpellSkill skill,
                const std::vector<actor::Actor*>& seen_targets) const override;

protected:
        Range duration_range(SpellSkill skill) const;

        int base_max_cost(
                SpellSkill skill,
                const actor::Actor* caster) const override;

        bool is_noisy(const SpellSkill skill) const override
        {
                (void)skill;

                return true;
        }
};

class SpellTerrify : public Spell
{
public:
        SpellTerrify() = default;

        bool allow_mon_cast_now(
                const actor::Actor& mon,
                const std::vector<actor::Actor*>& seen_targets) const override;

        int mon_cooldown() const override;

        std::string name() const override
        {
                return "Terrify";
        }

        SpellId id() const override
        {
                return SpellId::terrify;
        }

        SpellDomain domain() const override
        {
                return SpellDomain::enchantment;
        }

        SpellShock shock_type() const override
        {
                return SpellShock::disturbing;
        }

        bool player_can_learn() const override
        {
                return true;
        }

        std::vector<std::string> descr_specific(
                SpellSkill skill) const override;

        void run_effect(
                actor::Actor* caster,
                SpellSkill skill,
                const std::vector<actor::Actor*>& seen_targets) const override;

protected:
        int base_max_cost(
                SpellSkill skill,
                const actor::Actor* caster) const override;

        bool is_noisy(const SpellSkill skill) const override
        {
                (void)skill;

                return true;
        }

        Range duration_range(SpellSkill skill) const;
};

class SpellAuraOfDecay : public Spell
{
public:
        SpellAuraOfDecay() = default;

        bool allow_mon_cast_now(
                const actor::Actor& mon,
                const std::vector<actor::Actor*>& seen_targets) const override;

        int mon_cooldown() const override;

        bool player_can_learn() const override
        {
                return true;
        }

        std::string name() const override
        {
                return "Aura of Decay";
        }

        SpellId id() const override
        {
                return SpellId::aura_of_decay;
        }

        SpellDomain domain() const override
        {
                return SpellDomain::invocation;
        }

        SpellShock shock_type() const override
        {
                return SpellShock::disturbing;
        }

        std::vector<std::string> descr_specific(
                SpellSkill skill) const override;

        void run_effect(
                actor::Actor* caster,
                SpellSkill skill,
                const std::vector<actor::Actor*>& seen_targets) const override;

private:
        int base_max_cost(
                SpellSkill skill,
                const actor::Actor* caster) const override;

        bool is_noisy(const SpellSkill skill) const override
        {
                (void)skill;

                return true;
        }

        Range dmg_range(SpellSkill skill) const;

        Range duration_range(SpellSkill skill) const;
};

class BoltImpl
{
public:
        virtual ~BoltImpl() = default;

        virtual Range damage(
                SpellSkill skill,
                const actor::Actor& caster) const = 0;

        virtual void on_hit(
                actor::Actor& actor_hit,
                actor::Actor& caster,
                SpellSkill skill) const = 0;

        virtual std::string hit_msg_ending() const = 0;

        virtual int mon_cooldown() const = 0;

        virtual bool player_can_learn() const = 0;

        virtual std::string name() const = 0;

        virtual SpellId id() const = 0;

        virtual std::vector<std::string> descr_specific(
                SpellSkill skill) const = 0;

        virtual int base_max_cost(
                SpellSkill skill,
                const actor::Actor* caster) const = 0;
};

class ForceBolt : public BoltImpl
{
public:
        ForceBolt() = default;

        Range damage(
                SpellSkill skill,
                const actor::Actor& caster) const override;

        void on_hit(
                actor::Actor& actor_hit,
                actor::Actor& caster,
                const SpellSkill skill) const override
        {
                (void)actor_hit;
                (void)caster;
                (void)skill;
        }

        std::string hit_msg_ending() const override
        {
                return "struck by a bolt!";
        }

        int mon_cooldown() const override
        {
                return 3;
        }

        bool player_can_learn() const override
        {
                return false;
        }

        std::string name() const override
        {
                return "Force Bolt";
        }

        SpellId id() const override
        {
                return SpellId::force_bolt;
        }

        std::vector<std::string> descr_specific(
                SpellSkill skill) const override;

        int base_max_cost(
                const SpellSkill skill,
                const actor::Actor* const caster) const override
        {
                (void)skill;
                (void)caster;

                return 2;
        }
};

class Darkbolt : public BoltImpl
{
public:
        Darkbolt() = default;

        Range damage(
                SpellSkill skill,
                const actor::Actor& caster) const override;

        void on_hit(
                actor::Actor& actor_hit,
                actor::Actor& caster,
                SpellSkill skill) const override;

        std::string hit_msg_ending() const override
        {
                return "struck by a blast!";
        }

        int mon_cooldown() const override
        {
                return 5;
        }

        bool player_can_learn() const override
        {
                return true;
        }

        std::string name() const override
        {
                return "Darkbolt";
        }

        SpellId id() const override
        {
                return SpellId::darkbolt;
        }

        std::vector<std::string> descr_specific(
                SpellSkill skill) const override;

        int base_max_cost(
                const SpellSkill skill,
                const actor::Actor* const caster) const override
        {
                (void)skill;
                (void)caster;

                return 4;
        }
};

class SpellBolt : public Spell
{
public:
        SpellBolt(BoltImpl* impl) :

                m_impl(impl)
        {}

        bool allow_mon_cast_now(
                const actor::Actor& mon,
                const std::vector<actor::Actor*>& seen_targets) const override;

        int mon_cooldown() const override
        {
                return m_impl->mon_cooldown();
        }

        bool player_can_learn() const override
        {
                return m_impl->player_can_learn();
        }

        std::string name() const override
        {
                return m_impl->name();
        }

        SpellId id() const override
        {
                return m_impl->id();
        }

        SpellDomain domain() const override
        {
                return SpellDomain::invocation;
        }

        SpellShock shock_type() const override
        {
                return SpellShock::mild;
        }

        std::vector<std::string> descr_specific(
                const SpellSkill skill) const override
        {
                return m_impl->descr_specific(skill);
        }

        void run_effect(
                actor::Actor* caster,
                SpellSkill skill,
                const std::vector<actor::Actor*>& seen_targets) const override;

private:
        int base_max_cost(
                const SpellSkill skill,
                const actor::Actor* const caster) const override
        {
                (void)caster;

                return m_impl->base_max_cost(skill, caster);
        }

        bool is_noisy(const SpellSkill skill) const override
        {
                (void)skill;

                return true;
        }

        void draw_projectile_travel(
                const actor::Actor& caster,
                const actor::Actor& target) const;

        std::unique_ptr<BoltImpl> m_impl;
};

class SpellAzaGaze : public Spell
{
public:
        SpellAzaGaze() = default;

        bool allow_mon_cast_now(
                const actor::Actor& mon,
                const std::vector<actor::Actor*>& seen_targets) const override;

        int mon_cooldown() const override
        {
                return 6;
        }

        bool player_can_learn() const override
        {
                return true;
        }

        std::string name() const override
        {
                return "Azathoth's Gaze";
        }

        SpellId id() const override
        {
                return SpellId::aza_gaze;
        }

        SpellDomain domain() const override
        {
                return SpellDomain::invocation;
        }

        SpellShock shock_type() const override
        {
                return SpellShock::disturbing;
        }

        std::vector<std::string> descr_specific(
                SpellSkill skill) const override;

        void run_effect(
                actor::Actor* caster,
                SpellSkill skill,
                const std::vector<actor::Actor*>& seen_targets) const override;

private:
        int base_max_cost(
                const SpellSkill skill,
                const actor::Actor* const caster) const override
        {
                (void)skill;
                (void)caster;

                return 8;
        }

        bool is_noisy(const SpellSkill skill) const override
        {
                (void)skill;

                return true;
        }

        Range dmg_range(SpellSkill skill) const;

        Range faint_duration_range(SpellSkill skill) const;

        Range conflict_duration_range(SpellSkill skill) const;

        void run_effect_on_target(
                actor::Actor* caster,
                actor::Actor& target,
                SpellSkill skill) const;

        void do_damage_on_target(
                actor::Actor& target,
                SpellSkill skill,
                actor::Actor* caster) const;

        void apply_properties_on_target(
                actor::Actor& target,
                SpellSkill skill) const;
};

class SpellCataclysm : public Spell
{
public:
        SpellCataclysm() = default;

        bool allow_mon_cast_now(
                const actor::Actor& mon,
                const std::vector<actor::Actor*>& seen_targets) const override;

        bool player_can_learn() const override
        {
                return true;
        }

        std::string name() const override
        {
                return "Cataclysm";
        }

        SpellId id() const override
        {
                return SpellId::cataclysm;
        }

        SpellDomain domain() const override
        {
                return SpellDomain::invocation;
        }

        SpellShock shock_type() const override
        {
                return SpellShock::disturbing;
        }

        std::vector<std::string> descr_specific(
                SpellSkill skill) const override;

        void run_effect(
                actor::Actor* caster,
                SpellSkill skill,
                const std::vector<actor::Actor*>& seen_targets) const override;

private:
        int destruction_radi(SpellSkill skill) const;

        int nr_explosions(SpellSkill skill) const;

        int nr_destruction_sweeps(SpellSkill skill) const;

        int base_max_cost(
                SpellSkill skill,
                const actor::Actor* caster) const override;

        bool is_noisy(const SpellSkill skill) const override
        {
                (void)skill;

                return true;
        }
};

class SpellPestilence : public Spell
{
public:
        SpellPestilence() = default;

        bool allow_mon_cast_now(
                const actor::Actor& mon,
                const std::vector<actor::Actor*>& seen_targets) const override;

        int mon_cooldown() const override
        {
                return 21;
        }

        bool player_can_learn() const override
        {
                return true;
        }

        std::string name() const override
        {
                return "Pestilence";
        }

        SpellId id() const override
        {
                return SpellId::pestilence;
        }

        SpellDomain domain() const override
        {
                return SpellDomain::transmutation;
        }

        SpellShock shock_type() const override
        {
                return SpellShock::disturbing;
        }

        std::vector<std::string> descr_specific(
                SpellSkill skill) const override;

        void run_effect(
                actor::Actor* caster,
                SpellSkill skill,
                const std::vector<actor::Actor*>& seen_targets) const override;

private:
        int nr_rats_summoned(SpellSkill skill) const;

        int base_max_cost(
                const SpellSkill skill,
                const actor::Actor* const caster) const override
        {
                (void)skill;
                (void)caster;

                return 7;
        }

        bool is_noisy(const SpellSkill skill) const override
        {
                (void)skill;

                return true;
        }

        void on_rat_summoned(actor::Actor* mon, SpellSkill skill) const;
};

class SpellSpectralWeapons : public Spell
{
public:
        SpellSpectralWeapons() = default;

        bool player_can_learn() const override
        {
                return true;
        }

        std::string name() const override
        {
                return "Spectral Weapons";
        }

        SpellId id() const override
        {
                return SpellId::spectral_weapons;
        }

        SpellDomain domain() const override
        {
                return SpellDomain::transmutation;
        }

        SpellShock shock_type() const override
        {
                return SpellShock::mild;
        }

        std::vector<std::string> descr_specific(
                SpellSkill skill) const override;

        void run_effect(
                actor::Actor* caster,
                SpellSkill skill,
                const std::vector<actor::Actor*>& seen_targets) const override;

private:
        int base_max_cost(
                const SpellSkill skill,
                const actor::Actor* const caster) const override
        {
                (void)skill;
                (void)caster;

                return 6;
        }

        bool is_noisy(const SpellSkill skill) const override
        {
                (void)skill;

                return true;
        }

        Range duration_range(SpellSkill skill) const;

        void on_mon_summoned(
                item::Item* item,
                actor::Actor* mon,
                SpellSkill skill) const;

        int max_nr_weapons(SpellSkill skill) const;
};

class SpellControlObject : public Spell
{
public:
        SpellControlObject() = default;

        bool player_can_learn() const override
        {
                return true;
        }

        std::string name() const override
        {
                return "Control Object";
        }

        SpellId id() const override
        {
                return SpellId::control_object;
        }

        SpellDomain domain() const override
        {
                return SpellDomain::transmutation;
        }

        SpellShock shock_type() const override
        {
                return SpellShock::mild;
        }

        std::vector<std::string> descr_specific(
                SpellSkill skill) const override;

        void run_effect(
                actor::Actor* caster,
                SpellSkill skill,
                const std::vector<actor::Actor*>& seen_targets) const override;

private:
        int base_max_cost(
                SpellSkill skill,
                const actor::Actor* caster) const override;

        int max_dist(SpellSkill skill) const;

        bool is_noisy(SpellSkill skill) const override;
};

class SpellCleansingFire : public Spell
{
public:
        SpellCleansingFire() = default;

        bool player_can_learn() const override
        {
                return true;
        }

        std::string name() const override
        {
                return "Cleansing Fire";
        }

        SpellId id() const override
        {
                return SpellId::cleansing_fire;
        }

        SpellDomain domain() const override
        {
                return SpellDomain::END;
        }

        bool can_be_improved_with_skill() const override
        {
                return true;
        }

        SpellShock shock_type() const override
        {
                return SpellShock::disturbing;
        }

        std::vector<std::string> descr_specific(
                SpellSkill skill) const override;

        void run_effect(
                actor::Actor* caster,
                SpellSkill skill,
                const std::vector<actor::Actor*>& seen_targets) const override;

private:
        int base_max_cost(
                const SpellSkill skill,
                const actor::Actor* const caster) const override
        {
                (void)skill;
                (void)caster;

                return 7;
        }

        bool is_noisy(const SpellSkill skill) const override
        {
                (void)skill;

                return true;
        }

        Range burn_duration_range() const;
};

class SpellSanctuary : public Spell
{
public:
        SpellSanctuary() = default;

        bool player_can_learn() const override
        {
                return true;
        }

        std::string name() const override
        {
                return "Sanctuary";
        }

        SpellId id() const override
        {
                return SpellId::sanctuary;
        }

        SpellDomain domain() const override
        {
                return SpellDomain::END;
        }

        bool can_be_improved_with_skill() const override
        {
                return true;
        }

        SpellShock shock_type() const override
        {
                return SpellShock::mild;
        }

        std::vector<std::string> descr_specific(
                SpellSkill skill) const override;

        void run_effect(
                actor::Actor* caster,
                SpellSkill skill,
                const std::vector<actor::Actor*>& seen_targets) const override;

private:
        int base_max_cost(
                const SpellSkill skill,
                const actor::Actor* const caster) const override
        {
                (void)skill;
                (void)caster;

                return 5;
        }

        bool is_noisy(const SpellSkill skill) const override
        {
                (void)skill;

                return false;
        }

        Range duration(SpellSkill skill) const;
};

class SpellPurge : public Spell
{
public:
        SpellPurge() = default;

        bool player_can_learn() const override
        {
                return true;
        }

        std::string name() const override
        {
                return "Purge";
        }

        SpellId id() const override
        {
                return SpellId::purge;
        }

        SpellDomain domain() const override
        {
                return SpellDomain::END;
        }

        bool can_be_improved_with_skill() const override
        {
                return false;
        }

        SpellShock shock_type() const override
        {
                return SpellShock::mild;
        }

        std::vector<std::string> descr_specific(
                SpellSkill skill) const override;

        void run_effect(
                actor::Actor* caster,
                SpellSkill skill,
                const std::vector<actor::Actor*>& seen_targets) const override;

private:
        int base_max_cost(
                const SpellSkill skill,
                const actor::Actor* const caster) const override
        {
                (void)skill;
                (void)caster;

                return 4;
        }

        bool is_noisy(const SpellSkill skill) const override
        {
                (void)skill;

                return true;
        }

        Range dmg_range() const;

        Range fear_duration_range() const;
};

class SpellFrenzy : public Spell
{
public:
        SpellFrenzy() = default;

        bool player_can_learn() const override
        {
                return true;
        }

        std::string name() const override
        {
                return "Incite Frenzy";
        }

        SpellId id() const override
        {
                return SpellId::frenzy;
        }

        SpellDomain domain() const override
        {
                return SpellDomain::END;
        }

        bool can_be_improved_with_skill() const override
        {
                return false;
        }

        SpellShock shock_type() const override
        {
                return SpellShock::mild;
        }

        std::vector<std::string> descr_specific(
                SpellSkill skill) const override;

        void run_effect(
                actor::Actor* caster,
                SpellSkill skill,
                const std::vector<actor::Actor*>& seen_targets) const override;

private:
        int base_max_cost(
                const SpellSkill skill,
                const actor::Actor* const caster) const override
        {
                (void)skill;
                (void)caster;

                return 0;
        }

        bool is_noisy(const SpellSkill skill) const override
        {
                (void)skill;

                return false;
        }
};

class SpellBless : public Spell
{
public:
        SpellBless() = default;

        bool player_can_learn() const override
        {
                return true;
        }

        std::string name() const override
        {
                return "Bless";
        }

        SpellId id() const override
        {
                return SpellId::bless;
        }

        SpellDomain domain() const override
        {
                // NOTE: This could perhaps be considered an enchantment spell,
                // but the way the spell description is phrased, it sounds a lot
                // more like transmutation.
                return SpellDomain::transmutation;
        }

        SpellShock shock_type() const override
        {
                return SpellShock::mild;
        }

        std::vector<std::string> descr_specific(
                SpellSkill skill) const override;

        void run_effect(
                actor::Actor* caster,
                SpellSkill skill,
                const std::vector<actor::Actor*>& seen_targets) const override;

        Range duration_range(SpellSkill skill) const;

private:
        int base_max_cost(
                SpellSkill skill,
                const actor::Actor* caster) const override;

        bool is_noisy(const SpellSkill skill) const override
        {
                (void)skill;

                return false;
        }
};

class SpellTransmut : public Spell
{
public:
        SpellTransmut() = default;

        bool player_can_learn() const override
        {
                return true;
        }

        std::string name() const override
        {
                return "Transmutation";
        }

        SpellId id() const override
        {
                return SpellId::transmut;
        }

        SpellDomain domain() const override
        {
                return SpellDomain::transmutation;
        }

        bool is_tenebrous() const override
        {
                return true;
        }

        SpellShock shock_type() const override
        {
                return SpellShock::mild;
        }

        std::vector<std::string> descr_specific(
                SpellSkill skill) const override;

        void run_effect(
                actor::Actor* caster,
                SpellSkill skill,
                const std::vector<actor::Actor*>& seen_targets) const override;

private:
        int skill_bon(SpellSkill skill) const;

        int chance_scroll(SpellSkill skill) const;

        int chance_potion(SpellSkill skill) const;

        int chance_weapon(SpellSkill skill, int plus) const;

        int base_max_cost(
                const SpellSkill skill,
                const actor::Actor* const caster) const override
        {
                (void)skill;
                (void)caster;

                return 4;
        }

        bool is_noisy(const SpellSkill skill) const override
        {
                (void)skill;

                return false;
        }
};

class SpellLight : public Spell
{
public:
        SpellLight() = default;
        bool player_can_learn() const override
        {
                return true;
        }

        std::string name() const override
        {
                return "Light";
        }

        SpellId id() const override
        {
                return SpellId::light;
        }

        SpellDomain domain() const override
        {
                return SpellDomain::transmutation;
        }

        SpellShock shock_type() const override
        {
                return SpellShock::mild;
        }

        std::vector<std::string> descr_specific(
                SpellSkill skill) const override;

        void run_effect(
                actor::Actor* caster,
                SpellSkill skill,
                const std::vector<actor::Actor*>& seen_targets) const override;

private:
        Range light_duration_range(SpellSkill skill) const;

        Range blind_duration_range(SpellSkill skill) const;

        Range burning_duration_range() const;

        int base_max_cost(
                SpellSkill skill,
                const actor::Actor* const caster) const override
        {
                (void)skill;
                (void)caster;

                return 5;
        }

        bool is_noisy(const SpellSkill skill) const override
        {
                (void)skill;

                return false;
        }
};

class SpellKnockBack : public Spell
{
public:
        SpellKnockBack() = default;

        bool allow_mon_cast_now(
                const actor::Actor& mon,
                const std::vector<actor::Actor*>& seen_targets) const override;

        int mon_cooldown() const override
        {
                return 5;
        }

        bool player_can_learn() const override
        {
                return false;
        }

        std::string name() const override
        {
                return "Knockback";
        }

        SpellId id() const override
        {
                return SpellId::knockback;
        }

        SpellDomain domain() const override
        {
                return SpellDomain::END;
        }

        SpellShock shock_type() const override
        {
                return SpellShock::mild;
        }

        std::vector<std::string> descr_specific(
                const SpellSkill skill) const override
        {
                (void)skill;

                return {};
        }

        void run_effect(
                actor::Actor* caster,
                SpellSkill skill,
                const std::vector<actor::Actor*>& seen_targets) const override;

private:
        int base_max_cost(
                const SpellSkill skill,
                const actor::Actor* const caster) const override
        {
                (void)skill;
                (void)caster;

                return 8;
        }

        bool is_noisy(const SpellSkill skill) const override
        {
                (void)skill;

                return true;
        }
};

class SpellTeleport : public Spell
{
public:
        SpellTeleport() = default;

        bool allow_mon_cast_now(
                const actor::Actor& mon,
                const std::vector<actor::Actor*>& seen_targets) const override;

        int mon_cooldown() const override
        {
                return 20;
        }

        bool player_can_learn() const override
        {
                return true;
        }

        std::string name() const override
        {
                return "Teleport";
        }

        SpellId id() const override
        {
                return SpellId::teleport;
        }

        SpellDomain domain() const override
        {
                return SpellDomain::transmutation;
        }

        SpellShock shock_type() const override
        {
                return SpellShock::disturbing;
        }

        std::vector<std::string> descr_specific(
                SpellSkill skill) const override;

        void run_effect(
                actor::Actor* caster,
                SpellSkill skill,
                const std::vector<actor::Actor*>& seen_targets) const override;

private:
        int invis_duration(SpellSkill skill) const;

        int base_max_cost(
                const SpellSkill skill,
                const actor::Actor* const caster) const override
        {
                (void)skill;
                (void)caster;

                return 8;
        }

        bool is_noisy(const SpellSkill skill) const override
        {
                (void)skill;

                return true;
        }

        int max_dist(SpellSkill skill) const;
};

class SpellInvis : public Spell
{
public:
        SpellInvis() = default;

        bool player_can_learn() const override
        {
                return true;
        }

        std::string name() const override
        {
                return "Invisibility";
        }

        SpellId id() const override
        {
                return SpellId::invis;
        }

        SpellDomain domain() const override
        {
                return SpellDomain::enchantment;
        }

        bool is_tenebrous() const override
        {
                return true;
        }

        SpellShock shock_type() const override
        {
                return SpellShock::mild;
        }

        std::vector<std::string> descr_specific(
                SpellSkill skill) const override;

        void run_effect(
                actor::Actor* caster,
                SpellSkill skill,
                const std::vector<actor::Actor*>& seen_targets) const override;

private:
        Range duration_range(SpellSkill skill) const;

        int base_max_cost(
                SpellSkill skill,
                const actor::Actor* caster) const override;

        bool is_noisy(SpellSkill skill) const override;
};

class SpellSeeInvis : public Spell
{
public:
        SpellSeeInvis() = default;

        bool allow_mon_cast_now(
                const actor::Actor& mon,
                const std::vector<actor::Actor*>& seen_targets) const override;

        int mon_cooldown() const override
        {
                return 30;
        }

        bool player_can_learn() const override
        {
                return true;
        }

        std::string name() const override
        {
                return "See Invisible";
        }

        SpellId id() const override
        {
                return SpellId::see_invis;
        }

        SpellDomain domain() const override
        {
                return SpellDomain::clairvoyance;
        }

        SpellShock shock_type() const override
        {
                return SpellShock::mild;
        }

        std::vector<std::string> descr_specific(
                SpellSkill skill) const override;

        void run_effect(
                actor::Actor* caster,
                SpellSkill skill,
                const std::vector<actor::Actor*>& seen_targets) const override;

private:
        Range duration_range(SpellSkill skill) const;

        int base_max_cost(
                SpellSkill skill,
                const actor::Actor* caster) const override;

        bool is_noisy(const SpellSkill skill) const override
        {
                (void)skill;

                return true;
        }
};

class SpellSpellShield : public Spell
{
public:
        SpellSpellShield() = default;

        bool allow_mon_cast_now(
                const actor::Actor& mon,
                const std::vector<actor::Actor*>& seen_targets) const override;

        int mon_cooldown() const override
        {
                return 3;
        }

        bool player_can_learn() const override
        {
                return true;
        }

        std::string name() const override
        {
                return "Spell Shield";
        }

        SpellId id() const override
        {
                return SpellId::spell_shield;
        }

        SpellDomain domain() const override
        {
                return SpellDomain::enchantment;
        }

        SpellShock shock_type() const override
        {
                return SpellShock::mild;
        }

        std::vector<std::string> descr_specific(
                SpellSkill skill) const override;

        void run_effect(
                actor::Actor* caster,
                SpellSkill skill,
                const std::vector<actor::Actor*>& seen_targets) const override;

private:
        int base_max_cost(
                SpellSkill skill,
                const actor::Actor* caster) const override;

        bool is_noisy(const SpellSkill skill) const override
        {
                (void)skill;

                return true;
        }
};

class SpellHaste : public Spell
{
public:
        SpellHaste() = default;

        bool allow_mon_cast_now(
                const actor::Actor& mon,
                const std::vector<actor::Actor*>& seen_targets) const override;

        int mon_cooldown() const override;

        bool player_can_learn() const override
        {
                return true;
        }

        std::string name() const override
        {
                return "Haste";
        }

        SpellId id() const override
        {
                return SpellId::haste;
        }

        SpellDomain domain() const override
        {
                return SpellDomain::transmutation;
        }

        SpellShock shock_type() const override
        {
                return SpellShock::mild;
        }

        std::vector<std::string> descr_specific(
                SpellSkill skill) const override;

        void run_effect(
                actor::Actor* caster,
                SpellSkill skill,
                const std::vector<actor::Actor*>& seen_targets) const override;

private:
        Range duration_range(SpellSkill skill) const;

        int base_max_cost(
                SpellSkill skill,
                const actor::Actor* caster) const override;

        bool is_noisy(const SpellSkill skill) const override
        {
                (void)skill;

                return false;
        }
};

class SpellPremonition : public Spell
{
public:
        SpellPremonition() = default;

        bool player_can_learn() const override
        {
                return true;
        }

        std::string name() const override
        {
                return "Premonition";
        }

        SpellId id() const override
        {
                return SpellId::premonition;
        }

        SpellDomain domain() const override
        {
                return SpellDomain::clairvoyance;
        }

        SpellShock shock_type() const override
        {
                return SpellShock::mild;
        }

        std::vector<std::string> descr_specific(
                SpellSkill skill) const override;

        void run_effect(
                actor::Actor* caster,
                SpellSkill skill,
                const std::vector<actor::Actor*>& seen_targets) const override;

private:
        Range duration_range(SpellSkill skill) const;

        int base_max_cost(
                SpellSkill skill,
                const actor::Actor* caster) const override;

        bool is_noisy(const SpellSkill skill) const override
        {
                (void)skill;

                return false;
        }
};

class SpellErudition : public Spell
{
public:
        SpellErudition() = default;

        bool player_can_learn() const override
        {
                return true;
        }

        std::string name() const override
        {
                return "Erudition";
        }

        SpellId id() const override
        {
                return SpellId::erudition;
        }

        SpellDomain domain() const override
        {
                return SpellDomain::clairvoyance;
        }

        SpellShock shock_type() const override
        {
                return SpellShock::disturbing;
        }

        std::vector<std::string> descr_specific(
                SpellSkill skill) const override;

        void run_effect(
                actor::Actor* caster,
                SpellSkill skill,
                const std::vector<actor::Actor*>& seen_targets) const override;

private:
        int base_max_cost(
                SpellSkill skill,
                const actor::Actor* caster) const override;

        bool is_noisy(const SpellSkill skill) const override
        {
                (void)skill;

                return false;
        }

        Range get_duration_range(SpellSkill skill) const;
};

class SpellIdentify : public Spell
{
public:
        SpellIdentify() = default;

        bool player_can_learn() const override
        {
                return true;
        }

        std::string name() const override
        {
                return "Identify";
        }

        SpellId id() const override
        {
                return SpellId::identify;
        }

        SpellDomain domain() const override
        {
                return SpellDomain::clairvoyance;
        }

        bool is_tenebrous() const override
        {
                return true;
        }

        SpellShock shock_type() const override
        {
                return SpellShock::disturbing;
        }

        std::vector<std::string> descr_specific(
                SpellSkill skill) const override;

        void run_effect(
                actor::Actor* caster,
                SpellSkill skill,
                const std::vector<actor::Actor*>& seen_targets) const override;

private:
        int base_max_cost(
                SpellSkill skill,
                const actor::Actor* caster) const override;

        bool is_noisy(const SpellSkill skill) const override
        {
                (void)skill;

                return false;
        }
};

class SpellResistance : public Spell
{
public:
        SpellResistance() = default;

        bool allow_mon_cast_now(
                const actor::Actor& mon,
                const std::vector<actor::Actor*>& seen_targets) const override;

        int mon_cooldown() const override
        {
                return 20;
        }

        bool player_can_learn() const override
        {
                return true;
        }

        std::string name() const override
        {
                return "Resistance";
        }

        SpellId id() const override
        {
                return SpellId::resistance;
        }

        SpellDomain domain() const override
        {
                return SpellDomain::enchantment;
        }

        SpellShock shock_type() const override
        {
                return SpellShock::mild;
        }

        std::vector<std::string> descr_specific(
                SpellSkill skill) const override;

        void run_effect(
                actor::Actor* caster,
                SpellSkill skill,
                const std::vector<actor::Actor*>& seen_targets) const override;

private:
        Range duration_range(SpellSkill skill) const;

        int base_max_cost(
                const SpellSkill skill,
                const actor::Actor* const caster) const override
        {
                (void)skill;
                (void)caster;

                return 7;
        }

        bool is_noisy(const SpellSkill skill) const override
        {
                (void)skill;

                return true;
        }
};

class SpellBloodTempering : public Spell
{
public:
        SpellBloodTempering() = default;

        bool player_can_learn() const override
        {
                return true;
        }

        std::string name() const override
        {
                return "Blood Tempering";
        }

        SpellId id() const override
        {
                return SpellId::blood_tempering;
        }

        SpellDomain domain() const override
        {
                return SpellDomain::blood;
        }

        bool is_tenebrous() const override
        {
                return true;
        }

        SpellCostType cost_type() const override
        {
                return SpellCostType::hit_points;
        }

        SpellShock shock_type() const override
        {
                return SpellShock::disturbing;
        }

        std::vector<std::string> descr_specific(
                SpellSkill skill) const override;

        void run_effect(
                actor::Actor* caster,
                SpellSkill skill,
                const std::vector<actor::Actor*>& seen_targets) const override;

private:
        Range duration_range(SpellSkill skill) const;

        int base_max_cost(
                SpellSkill skill,
                const actor::Actor* caster) const override;

        bool is_noisy(const SpellSkill skill) const override
        {
                (void)skill;

                return true;
        }
};

class SpellThorns : public Spell
{
public:
        SpellThorns() = default;

        bool player_can_learn() const override
        {
                return true;
        }

        std::string name() const override
        {
                return "Thorns";
        }

        SpellId id() const override
        {
                return SpellId::thorns;
        }

        SpellDomain domain() const override
        {
                return SpellDomain::blood;
        }

        SpellCostType cost_type() const override
        {
                return SpellCostType::hit_points;
        }

        SpellShock shock_type() const override
        {
                return SpellShock::mild;
        }

        std::vector<std::string> descr_specific(
                SpellSkill skill) const override;

        void run_effect(
                actor::Actor* caster,
                SpellSkill skill,
                const std::vector<actor::Actor*>& seen_targets) const override;

private:
        Range duration_range(SpellSkill skill) const;
        Range dmg_range(SpellSkill skill) const;

        int base_max_cost(
                const SpellSkill skill,
                const actor::Actor* const caster) const override
        {
                (void)skill;
                (void)caster;

                return 4;
        }

        bool is_noisy(const SpellSkill skill) const override
        {
                (void)skill;

                return true;
        }
};

class SpellCrimsonPassage : public Spell
{
public:
        SpellCrimsonPassage() = default;

        bool player_can_learn() const override
        {
                return true;
        }

        std::string name() const override
        {
                return "Crimson Passage";
        }

        SpellId id() const override
        {
                return SpellId::crimson_passage;
        }

        SpellDomain domain() const override
        {
                return SpellDomain::blood;
        }

        SpellCostType cost_type() const override
        {
                return SpellCostType::hit_points;
        }

        SpellShock shock_type() const override;

        std::vector<std::string> descr_specific(
                SpellSkill skill) const override;

        void run_effect(
                actor::Actor* caster,
                SpellSkill skill,
                const std::vector<actor::Actor*>& seen_targets) const override;

private:
        int nr_steps_allowed(SpellSkill skill) const;

        int base_max_cost(
                SpellSkill skill,
                const actor::Actor* caster) const override;

        bool is_noisy(SpellSkill skill) const override;
};

class SpellSacrificeLife : public Spell
{
public:
        SpellSacrificeLife() = default;

        bool player_can_learn() const override
        {
                return true;
        }

        std::string name() const override
        {
                return "Sacrifice Life";
        }

        SpellId id() const override
        {
                return SpellId::sacrifice_life;
        }

        SpellDomain domain() const override
        {
                return SpellDomain::blood;
        }

        bool is_tenebrous() const override
        {
                return true;
        }

        SpellShock shock_type() const override
        {
                return SpellShock::severe;
        }

        std::vector<std::string> descr_specific(
                SpellSkill skill) const override;

        void run_effect(
                actor::Actor* caster,
                SpellSkill skill,
                const std::vector<actor::Actor*>& seen_targets) const override;

private:
        int nr_sp_per_hp(SpellSkill skill) const;

        int base_max_cost(
                const SpellSkill skill,
                const actor::Actor* const caster) const override
        {
                (void)skill;
                (void)caster;

                return 0;
        }

        bool is_noisy(const SpellSkill skill) const override
        {
                (void)skill;

                return true;
        }
};

class SpellShedImpurity : public Spell
{
public:
        SpellShedImpurity() = default;

        bool player_can_learn() const override
        {
                return true;
        }

        std::string name() const override
        {
                return "Shed Impurity";
        }

        SpellId id() const override
        {
                return SpellId::shed_impurity;
        }

        SpellDomain domain() const override
        {
                return SpellDomain::blood;
        }

        SpellShock shock_type() const override
        {
                return SpellShock::mild;
        }

        std::vector<std::string> descr_specific(
                SpellSkill skill) const override;

        void run_effect(
                actor::Actor* caster,
                SpellSkill skill,
                const std::vector<actor::Actor*>& seen_targets) const override;

private:
        int get_min_hp_removed_for_bonus_effects() const;

        int get_moribund_hp_limit() const;

        int calc_nr_hp_removed(const actor::Actor* caster) const;

        int base_max_cost(
                const SpellSkill skill,
                const actor::Actor* const caster) const override
        {
                (void)skill;
                (void)caster;

                return 0;
        }

        bool is_noisy(const SpellSkill skill) const override
        {
                (void)skill;

                return false;
        }
};

class SpellDisease : public Spell
{
public:
        SpellDisease() = default;

        bool allow_mon_cast_now(
                const actor::Actor& mon,
                const std::vector<actor::Actor*>& seen_targets) const override;

        int mon_cooldown() const override
        {
                return 10;
        }

        bool player_can_learn() const override
        {
                return false;
        }

        std::string name() const override
        {
                return "Disease";
        }

        SpellId id() const override
        {
                return SpellId::disease;
        }

        SpellDomain domain() const override
        {
                return SpellDomain::END;
        }

        SpellShock shock_type() const override
        {
                return SpellShock::disturbing;
        }

        std::vector<std::string> descr_specific(
                const SpellSkill skill) const override
        {
                (void)skill;

                return {};
        }

        void run_effect(
                actor::Actor* caster,
                SpellSkill skill,
                const std::vector<actor::Actor*>& seen_targets) const override;

private:
        int base_max_cost(
                const SpellSkill skill,
                const actor::Actor* const caster) const override
        {
                (void)skill;
                (void)caster;

                return 7;
        }

        bool is_noisy(const SpellSkill skill) const override
        {
                (void)skill;

                return true;
        }
};

class SpellSummonMon : public Spell
{
public:
        SpellSummonMon() = default;

        bool allow_mon_cast_now(
                const actor::Actor& mon,
                const std::vector<actor::Actor*>& seen_targets) const override;

        int mon_cooldown() const override
        {
                return 8;
        }

        bool player_can_learn() const override
        {
                return false;
        }

        std::string name() const override
        {
                return "";
        }

        SpellId id() const override
        {
                return SpellId::summon;
        }

        SpellDomain domain() const override
        {
                return SpellDomain::END;
        }

        SpellShock shock_type() const override
        {
                return SpellShock::disturbing;
        }

        std::vector<std::string> descr_specific(
                const SpellSkill skill) const override
        {
                (void)skill;

                return {};
        }

        void run_effect(
                actor::Actor* caster,
                SpellSkill skill,
                const std::vector<actor::Actor*>& seen_targets) const override;

private:
        int base_max_cost(
                const SpellSkill skill,
                const actor::Actor* const caster) const override
        {
                (void)skill;
                (void)caster;

                return 6;
        }

        bool is_noisy(const SpellSkill skill) const override
        {
                (void)skill;

                return true;
        }

        Range get_allowed_mon_dlvl_range(SpellSkill skill) const;

        std::vector<std::string> make_summon_bucket(
                const Range& dlvl_range) const;

        void summon(const std::string& id, actor::Actor* caster) const;
};

class SpellSummonTentacles : public Spell
{
public:
        SpellSummonTentacles() = default;

        bool allow_mon_cast_now(
                const actor::Actor& mon,
                const std::vector<actor::Actor*>& seen_targets) const override;

        int mon_cooldown() const override
        {
                return 3;
        }

        bool player_can_learn() const override
        {
                return false;
        }

        std::string name() const override
        {
                return "";
        }

        SpellId id() const override
        {
                return SpellId::summon_tentacles;
        }

        SpellDomain domain() const override
        {
                return SpellDomain::END;
        }

        SpellShock shock_type() const override
        {
                return SpellShock::disturbing;
        }

        std::vector<std::string> descr_specific(
                const SpellSkill skill) const override
        {
                (void)skill;

                return {};
        }

        void run_effect(
                actor::Actor* caster,
                SpellSkill skill,
                const std::vector<actor::Actor*>& seen_targets) const override;

private:
        int base_max_cost(
                const SpellSkill skill,
                const actor::Actor* const caster) const override
        {
                (void)skill;
                (void)caster;

                return 6;
        }

        bool is_noisy(const SpellSkill skill) const override
        {
                (void)skill;

                return false;
        }
};

class SpellHeal : public Spell
{
public:
        SpellHeal() = default;

        bool allow_mon_cast_now(
                const actor::Actor& mon,
                const std::vector<actor::Actor*>& seen_targets) const override;

        int mon_cooldown() const override
        {
                return 6;
        }

        bool player_can_learn() const override
        {
                return true;
        }

        std::string name() const override
        {
                return "Healing";
        }

        SpellId id() const override
        {
                return SpellId::heal;
        }

        SpellDomain domain() const override
        {
                return SpellDomain::enchantment;
        }

        SpellShock shock_type() const override
        {
                return SpellShock::mild;
        }

        std::vector<std::string> descr_specific(
                SpellSkill skill) const override;

        void run_effect(
                actor::Actor* caster,
                SpellSkill skill,
                const std::vector<actor::Actor*>& seen_targets) const override;

private:
        int nr_hp_restored(SpellSkill skill) const;

        int base_max_cost(
                SpellSkill skill,
                const actor::Actor* caster) const override;

        bool is_noisy(const SpellSkill skill) const override
        {
                (void)skill;

                return true;
        }

        Range regen_duration() const;
};

class SpellMiGoHypno : public Spell
{
public:
        SpellMiGoHypno() = default;

        bool allow_mon_cast_now(
                const actor::Actor& mon,
                const std::vector<actor::Actor*>& seen_targets) const override;

        int mon_cooldown() const override
        {
                return 5;
        }

        bool player_can_learn() const override
        {
                return false;
        }

        std::string name() const override
        {
                return "MiGo Hypnosis";
        }

        SpellId id() const override
        {
                return SpellId::mi_go_hypno;
        }

        SpellDomain domain() const override
        {
                return SpellDomain::END;
        }

        SpellShock shock_type() const override
        {
                return SpellShock::mild;
        }

        std::vector<std::string> descr_specific(
                const SpellSkill skill) const override
        {
                (void)skill;

                return {};
        }

        void run_effect(
                actor::Actor* caster,
                SpellSkill skill,
                const std::vector<actor::Actor*>& seen_targets) const override;

private:
        int base_max_cost(
                const SpellSkill skill,
                const actor::Actor* const caster) const override
        {
                (void)skill;
                (void)caster;

                return 7;
        }

        bool is_noisy(const SpellSkill skill) const override
        {
                (void)skill;

                return true;
        }
};

class SpellBurn : public Spell
{
public:
        SpellBurn() = default;

        bool allow_mon_cast_now(
                const actor::Actor& mon,
                const std::vector<actor::Actor*>& seen_targets) const override;

        int mon_cooldown() const override
        {
                return 9;
        }

        bool player_can_learn() const override
        {
                return false;
        }

        std::string name() const override
        {
                return "Immolation";
        }

        SpellId id() const override
        {
                return SpellId::burn;
        }

        SpellDomain domain() const override
        {
                return SpellDomain::END;
        }

        SpellShock shock_type() const override
        {
                return SpellShock::disturbing;
        }

        std::vector<std::string> descr_specific(
                const SpellSkill skill) const override
        {
                (void)skill;

                return {};
        }

        void run_effect(
                actor::Actor* caster,
                SpellSkill skill,
                const std::vector<actor::Actor*>& seen_targets) const override;

private:
        int base_max_cost(
                const SpellSkill skill,
                const actor::Actor* const caster) const override
        {
                (void)skill;
                (void)caster;

                return 7;
        }

        bool is_noisy(const SpellSkill skill) const override
        {
                (void)skill;

                return true;
        }
};

class SpellDeafen : public Spell
{
public:
        SpellDeafen() = default;

        bool allow_mon_cast_now(
                const actor::Actor& mon,
                const std::vector<actor::Actor*>& seen_targets) const override;

        int mon_cooldown() const override
        {
                return 5;
        }

        bool player_can_learn() const override
        {
                return false;
        }

        std::string name() const override
        {
                return "Deafen";
        }

        SpellId id() const override
        {
                return SpellId::deafen;
        }

        SpellDomain domain() const override
        {
                return SpellDomain::END;
        }

        SpellShock shock_type() const override
        {
                return SpellShock::mild;
        }

        std::vector<std::string> descr_specific(
                const SpellSkill skill) const override
        {
                (void)skill;

                return {};
        }

        void run_effect(
                actor::Actor* caster,
                SpellSkill skill,
                const std::vector<actor::Actor*>& seen_targets) const override;

private:
        int base_max_cost(
                const SpellSkill skill,
                const actor::Actor* const caster) const override
        {
                (void)skill;
                (void)caster;

                return 4;
        }

        bool is_noisy(const SpellSkill skill) const override
        {
                (void)skill;

                return true;
        }
};

class SpellHealOthers : public Spell
{
public:
        SpellHealOthers() = default;

        bool allow_mon_cast_now(
                const actor::Actor& mon,
                const std::vector<actor::Actor*>& seen_targets) const override;

        int mon_cooldown() const override;

        std::string name() const override
        {
                return "Heal Others";
        }

        SpellId id() const override
        {
                return SpellId::heal_others;
        }

        SpellDomain domain() const override
        {
                return SpellDomain::enchantment;
        }

        SpellShock shock_type() const override
        {
                return SpellShock::mild;
        }

        bool player_can_learn() const override
        {
                return false;
        }

        std::vector<std::string> descr_specific(
                SpellSkill skill) const override
        {
                (void)skill;
                return {};
        }

        void run_effect(
                actor::Actor* caster,
                SpellSkill skill,
                const std::vector<actor::Actor*>& seen_targets) const override;

protected:
        std::vector<actor::Actor*> find_possible_actors_to_heal(
                const actor::Actor* caster) const;

        actor::Actor* find_random_actor_to_heal(const actor::Actor* caster) const;

        int base_max_cost(
                SpellSkill skill,
                const actor::Actor* caster) const override;

        bool is_noisy(const SpellSkill skill) const override
        {
                (void)skill;

                return true;
        }
};

#endif  // SPELLS_HPP

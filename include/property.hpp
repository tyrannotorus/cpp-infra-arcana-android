// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef PROPERTY_HPP
#define PROPERTY_HPP

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ability_values.hpp"
#include "colors.hpp"
#include "gfx.hpp"
#include "global.hpp"
#include "item.hpp"
#include "property_data.hpp"
#include "random.hpp"

namespace actor
{
class Actor;
}  // namespace actor

enum class Dir;

namespace actor
{
struct ActorData;
}  // namespace actor

struct P;

// -----------------------------------------------------------------------------
// Support types
// -----------------------------------------------------------------------------
enum class PropSrc
{
        // Properties applied by potions, spells, etc, or "natural" properties
        // for monsters (e.g. flying), or player properties gained by traits
        intr,

        // Properties applied by items carried in inventory
        inv,

        END
};

enum class PropDurationMode
{
        standard,
        specific,
        indefinite
};

struct DmgResistData
{
        bool is_resisted {false};
        std::string msg_resist_player {};
        // Not including monster name, e.g. " seems unaffected"
        std::string msg_resist_mon {};
};

enum class PropEnded
{
        no,
        yes
};

struct PropActResult
{
        DidAction did_action {DidAction::no};
        PropEnded prop_ended {PropEnded::no};
};

// TODO: There should be a property namespace, and the "Prop" prefix should be
// removed from the property classes.

extern const std::string g_property_ending_suffix;

// -----------------------------------------------------------------------------
// Property base class
// -----------------------------------------------------------------------------
class Prop
{
public:
        Prop(PropId id);

        virtual ~Prop() = default;

        PropId id() const
        {
                return m_id;
        }

        virtual void save() const {}

        virtual void load() {}

        int nr_turns_left() const
        {
                return m_nr_turns_left;
        }

        int nr_turns_active() const
        {
                return m_nr_turns_active;
        }

        void set_duration(int nr_turns);

        void set_indefinite()
        {
                m_duration_mode = PropDurationMode::indefinite;

                m_nr_turns_left = -1;
        }

        PropDurationMode duration_mode() const
        {
                return m_duration_mode;
        }

        PropSrc src() const
        {
                return m_src;
        }

        virtual bool is_finished() const
        {
                return m_nr_turns_left == 0;
        }

        virtual PropAlignment alignment() const
        {
                return m_data.alignment;
        }

        virtual void cycle_graphics() {}

        virtual std::optional<Color> override_property_color() const
        {
                return {};
        }

        virtual bool allow_display_turns() const
        {
                return m_data.allow_display_turns;
        }

        virtual std::string name() const
        {
                return m_data.name;
        }

        virtual std::string name_short() const
        {
                return m_data.name_short;
        }

        std::string descr() const
        {
                return m_data.descr;
        }

        virtual std::string msg_end_player() const
        {
                return m_data.msg_end_player;
        }

        virtual bool should_update_vision_on_toggled() const
        {
                return m_data.update_vision_on_toggled;
        }

        virtual bool allow_see() const
        {
                return true;
        }

        virtual bool allow_move() const
        {
                return true;
        }

        virtual bool allow_act() const
        {
                return true;
        }

        virtual PropEnded on_hit()
        {
                return PropEnded::no;
        }

        virtual void on_placed() {}

        virtual PropEnded on_actor_turn()
        {
                return PropEnded::no;
        }

        virtual void on_std_turn() {}

        virtual PropActResult on_act()
        {
                return {};
        }

        virtual void on_player_see() {}

        virtual void on_applied() {}

        virtual void on_end() {}

        virtual void on_more(const Prop& new_prop)
        {
                (void)new_prop;
        }

        virtual void on_death() {}

        virtual void on_new_dlvl() {}

        virtual void on_destroyed_alive() {}

        virtual void on_destroyed_corpse() {}

        virtual int affect_max_hp(const int hp_max) const
        {
                return hp_max;
        }

        virtual int affect_max_spi(const int spi_max) const
        {
                return spi_max;
        }

        virtual int player_extra_min_shock() const
        {
                return 0;
        }

        virtual std::optional<std::string> override_actor_name_the() const
        {
                return {};
        }

        virtual std::optional<std::string> override_actor_name_a() const
        {
                return {};
        }

        virtual std::optional<gfx::TileId> override_actor_tile() const
        {
                return {};
        }

        virtual std::optional<char> override_actor_character() const
        {
                return {};
        }

        virtual std::optional<std::string> override_actor_descr() const
        {
                return {};
        }

        virtual std::optional<Color> override_actor_color() const
        {
                return {};
        }

        virtual bool allow_attack_melee(const Verbose verbose) const
        {
                (void)verbose;
                return true;
        }

        virtual bool allow_attack_ranged(const Verbose verbose) const
        {
                (void)verbose;
                return true;
        }

        virtual bool allow_speak(const Verbose verbose) const
        {
                (void)verbose;
                return true;
        }

        virtual bool allow_pray(const Verbose verbose) const
        {
                (void)verbose;
                return true;
        }

        virtual bool allow_eat(const Verbose verbose) const
        {
                (void)verbose;
                return true;
        }

        virtual bool allow_read_absolute(const Verbose verbose) const
        {
                (void)verbose;
                return true;
        }

        virtual bool allow_read_chance(const Verbose verbose) const
        {
                (void)verbose;
                return true;
        }

        virtual bool allow_cast_intr_spell_absolute(
                const Verbose verbose) const
        {
                (void)verbose;
                return true;
        }

        virtual bool allow_cast_intr_spell_chance(
                const Verbose verbose) const
        {
                (void)verbose;
                return true;
        }

        virtual int ability_mod(const AbilityId ability) const
        {
                (void)ability;
                return 0;
        }

        virtual PropEnded affect_move_dir(Dir& dir)
        {
                (void)dir;

                return PropEnded::no;
        }

        virtual bool allow_move_dir(Dir dir)
        {
                (void)dir;

                return true;
        }

        virtual bool is_resisting_other_prop(const PropId prop_id) const
        {
                (void)prop_id;
                return false;
        }

        virtual DmgResistData is_resisting_dmg(const DmgType dmg_type) const
        {
                (void)dmg_type;

                return {};
        }

protected:
        friend class PropHandler;

        const PropId m_id;
        const PropData& m_data;

        int m_nr_turns_left;
        int m_nr_dlvls_left;

        int m_nr_turns_active {0};

        PropDurationMode m_duration_mode {PropDurationMode::standard};

        actor::Actor* m_owner {nullptr};
        PropSrc m_src {PropSrc::END};
        const item::Item* m_item_applying {nullptr};
};

// -----------------------------------------------------------------------------
// Specific properties
// -----------------------------------------------------------------------------
class PropTerrified : public Prop
{
public:
        PropTerrified() :
                Prop(PropId::terrified) {}

        int ability_mod(const AbilityId ability) const override
        {
                switch (ability)
                {
                case AbilityId::dodging:
                        return 20;

                case AbilityId::ranged:
                        return -20;

                default:
                        return 0;
                }
        }

        bool allow_attack_melee(Verbose verbose) const override;

        bool allow_attack_ranged(Verbose verbose) const override;

        void on_applied() override;
};

class PropInfected : public Prop
{
public:
        PropInfected() :
                Prop(PropId::infected) {}

        std::optional<Color> override_property_color() const override
        {
                return colors::orange();
        }

        PropEnded on_actor_turn() override;

        void on_applied() override;

private:
        bool has_warned {false};
};

class PropDiseased : public Prop
{
public:
        PropDiseased() :
                Prop(PropId::diseased) {}

        int affect_max_hp(int hp_max) const override;

        bool is_resisting_other_prop(PropId prop_id) const override;

        void on_applied() override;
};

class PropDescend : public Prop
{
public:
        PropDescend() :
                Prop(PropId::descend) {}

        PropEnded on_actor_turn() override;
};

class PropBurrowing : public Prop
{
public:
        PropBurrowing() :
                Prop(PropId::burrowing) {}

        PropEnded on_actor_turn() override;
};

class PropZuulPossessPriest : public Prop
{
public:
        PropZuulPossessPriest() :
                Prop(PropId::zuul_possess_priest) {}

        void on_placed() override;
};

class PropPossessedByZuul : public Prop
{
public:
        PropPossessedByZuul() :
                Prop(PropId::possessed_by_zuul) {}

        void on_death() override;

        int affect_max_hp(const int hp_max) const override
        {
                return hp_max * 2;
        }
};

class PropShapeshifts : public Prop
{
public:
        PropShapeshifts() :
                Prop(PropId::shapeshifts) {}

        void on_placed() override;

        void on_std_turn() override;

        void on_death() override;

private:
        void shapeshift(Verbose verbose) const;

        int m_countdown {-1};
};

class PropZealotStop : public Prop
{
public:
        PropZealotStop() :
                Prop(PropId::zealot_stop) {}

        PropEnded affect_move_dir(Dir& dir) override;
};

class PropPoisoned : public Prop
{
public:
        PropPoisoned() :
                Prop(PropId::poisoned) {}

        PropEnded on_actor_turn() override;
};

class PropAiming : public Prop
{
public:
        PropAiming() :
                Prop(PropId::aiming) {}

        int ability_mod(const AbilityId ability) const override
        {
                if (ability == AbilityId::ranged)
                {
                        return 10;
                }
                else
                {
                        return 0;
                }
        }

        PropEnded on_hit() override;
};

class PropBlind : public Prop
{
public:
        PropBlind() :
                Prop(PropId::blind) {}

        bool should_update_vision_on_toggled() const override;

        bool allow_read_absolute(Verbose verbose) const override;

        bool allow_see() const override
        {
                return false;
        }

        int ability_mod(const AbilityId ability) const override
        {
                switch (ability)
                {
                case AbilityId::searching:
                        return -9999;

                case AbilityId::ranged:
                        return -20;

                case AbilityId::melee:
                        return -20;

                case AbilityId::dodging:
                        return -50;

                default:
                        return 0;
                }
        }
};

class PropRecloaks : public Prop
{
public:
        PropRecloaks() :
                Prop(PropId::recloaks) {}

        PropActResult on_act() override;
};

class PropSeeInvis : public Prop
{
public:
        PropSeeInvis() :
                Prop(PropId::see_invis) {}

        void on_applied() override;

        bool is_resisting_other_prop(PropId prop_id) const override;
};

class PropBlessed : public Prop
{
public:
        PropBlessed() :
                Prop(PropId::blessed) {}

        void on_applied() override;

        void on_more(const Prop& new_prop) override;

        int ability_mod(AbilityId ability) const override;
};

class PropCursed : public Prop
{
public:
        PropCursed() :
                Prop(PropId::cursed) {}

        bool allow_read_chance(
                Verbose verbose) const override;

        bool allow_cast_intr_spell_chance(
                Verbose verbose) const override;

        int ability_mod(AbilityId ability) const override;

        void on_applied() override;

        void on_more(const Prop& new_prop) override;
};

class PropDoomed : public Prop
{
public:
        PropDoomed() :
                Prop(PropId::doomed) {}

        bool allow_read_chance(
                Verbose verbose) const override;

        bool allow_cast_intr_spell_chance(
                Verbose verbose) const override;

        int ability_mod(AbilityId ability) const override;

        void on_applied() override;

        void on_more(const Prop& new_prop) override;
};

class PropPremonition : public Prop
{
public:
        PropPremonition() :
                Prop(PropId::premonition) {}

        int ability_mod(const AbilityId ability) const override
        {
                (void)ability;

                if (ability == AbilityId::dodging)
                {
                        return 75;
                }
                else
                {
                        return 0;
                }
        }
};

class PropMagicSearching : public Prop
{
public:
        PropMagicSearching() :
                Prop(PropId::magic_searching) {}

        void save() const override;

        void load() override;

        PropEnded on_actor_turn() override;

        void set_range(const int range)
        {
                m_range = range;
        }

        void set_allow_reveal_items()
        {
                m_allow_reveal_items = true;
        }

        void set_allow_reveal_creatures()
        {
                m_allow_reveal_creatures = true;
        }

private:
        int m_range {1};
        bool m_allow_reveal_items {false};
        bool m_allow_reveal_creatures {false};
};

class PropEntangled : public Prop
{
public:
        PropEntangled() :
                Prop(PropId::entangled) {}

        void on_applied() override;

        PropEnded affect_move_dir(Dir& dir) override;

private:
        bool try_player_end_with_machete();
};

class PropBurning : public Prop
{
public:
        PropBurning() :
                Prop(PropId::burning) {}

        PropEnded on_actor_turn() override;

        bool allow_read_chance(
                Verbose verbose) const override;

        bool allow_cast_intr_spell_chance(
                Verbose verbose) const override;

        bool allow_pray(Verbose verbose) const override;

        bool allow_attack_ranged(Verbose verbose) const override;

        int ability_mod(const AbilityId ability) const override
        {
                (void)ability;
                return -30;
        }

        std::optional<Color> override_actor_color() const override;
};

class PropConfused : public Prop
{
public:
        PropConfused() :
                Prop(PropId::confused) {}

        PropEnded affect_move_dir(Dir& dir) override;

        bool allow_attack_melee(Verbose verbose) const override;
        bool allow_attack_ranged(Verbose verbose) const override;
        bool allow_read_absolute(Verbose verbose) const override;
        bool allow_cast_intr_spell_absolute(Verbose verbose) const override;
        bool allow_pray(Verbose verbose) const override;
};

class PropHallucinating : public Prop
{
public:
        PropHallucinating() :
                Prop(PropId::hallucinating) {}

        void on_applied() override;

        void on_std_turn() override;

        void on_end() override;

private:
        void apply_fake_actor_data() const;

        void clear_fake_actor_data() const;

        void create_fake_stairs() const;

        void clear_all_fake_stairs() const;

        std::vector<const actor::ActorData*> get_allowed_fake_mon_data() const;
};

class PropAstralOpiumAddict : public Prop
{
public:
        PropAstralOpiumAddict() :
                Prop(PropId::astral_opium_addiction) {}

        void save() const override;

        void load() override;

        std::string name_short() const override;

        void on_applied() override;

        void on_std_turn() override;

        void on_new_dlvl() override;

        void on_more(const Prop& new_prop) override;

        int player_extra_min_shock() const override;

private:
        bool is_active() const;

        void reset_penalty_countdown();

        int m_shock_lvl {0};
        int m_nr_dlvls_to_penalty {-1};
        int m_nr_turns_to_penalty {-1};
};

class PropNailed : public Prop
{
public:
        PropNailed() :
                Prop(PropId::nailed) {}

        std::string name_short() const override
        {
                return "Nailed(" + std::to_string(m_nr_spikes) + ")";
        }

        PropEnded affect_move_dir(Dir& dir) override;

        void on_more(const Prop& new_prop) override
        {
                (void)new_prop;

                ++m_nr_spikes;
        }

        int ability_mod(AbilityId ability) const override;

        bool is_finished() const override
        {
                return m_nr_spikes <= 0;
        }

private:
        int m_nr_spikes {1};
};

class PropWound : public Prop
{
public:
        PropWound() :
                Prop(PropId::wound) {}

        void save() const override;

        void load() override;

        std::string msg_end_player() const override;

        std::string name_short() const override
        {
                return "Wounded(" + std::to_string(m_nr_wounds) + ")";
        }

        int ability_mod(AbilityId ability) const override;

        void on_more(const Prop& new_prop) override;

        PropEnded on_actor_turn() override;

        bool is_finished() const override
        {
                return m_nr_wounds <= 0;
        }

        int affect_max_hp(int hp_max) const override;

        int nr_wounds() const
        {
                return m_nr_wounds;
        }

        void heal_one_wound();

private:
        void print_one_wound_healed_msg() const;

        std::string get_one_wound_heal_str() const;
        std::string get_all_wounds_heal_str() const;

        int m_nr_wounds {1};
};

class PropHpSap : public Prop
{
public:
        PropHpSap();

        void save() const override;

        void load() override;

        std::string name_short() const override
        {
                return "Life Sapped(" + std::to_string(m_nr_drained) + ")";
        }

        void on_more(const Prop& new_prop) override;

        int affect_max_hp(int hp_max) const override;

private:
        int m_nr_drained;
};

class PropSpiSap : public Prop
{
public:
        PropSpiSap();

        void save() const override;

        void load() override;

        std::string name_short() const override
        {
                return "Spirit Sapped(" + std::to_string(m_nr_drained) + ")";
        }

        void on_more(const Prop& new_prop) override;

        int affect_max_spi(int spi_max) const override;

private:
        int m_nr_drained;
};

class PropMindSap : public Prop
{
public:
        PropMindSap();

        void save() const override;

        void load() override;

        std::string name_short() const override
        {
                return "Mind Sapped(" + std::to_string(m_nr_drained) + "%)";
        }

        void on_more(const Prop& new_prop) override;

        int player_extra_min_shock() const override;

private:
        int m_nr_drained;
};

class PropWaiting : public Prop
{
public:
        PropWaiting() :
                Prop(PropId::waiting) {}

        bool allow_move() const override
        {
                return false;
        }

        bool allow_act() const override
        {
                return false;
        }

        bool allow_attack_melee(const Verbose verbose) const override
        {
                (void)verbose;
                return false;
        }

        bool allow_attack_ranged(const Verbose verbose) const override
        {
                (void)verbose;
                return false;
        }
};

class PropDelayedByLiquid : public Prop
{
public:
        PropDelayedByLiquid() :
                Prop(PropId::delayed_by_liquid) {}

        bool allow_move() const override
        {
                return false;
        }

        bool allow_act() const override
        {
                return false;
        }

        bool allow_attack_melee(const Verbose verbose) const override
        {
                (void)verbose;
                return false;
        }

        bool allow_attack_ranged(const Verbose verbose) const override
        {
                (void)verbose;
                return false;
        }
};

class PropDisabledAttack : public Prop
{
public:
        PropDisabledAttack() :
                Prop(PropId::disabled_attack) {}

        bool allow_attack_ranged(const Verbose verbose) const override
        {
                (void)verbose;
                return false;
        }

        bool allow_attack_melee(const Verbose verbose) const override
        {
                (void)verbose;
                return false;
        }
};

class PropDisabledMelee : public Prop
{
public:
        PropDisabledMelee() :
                Prop(PropId::disabled_melee) {}

        bool allow_attack_melee(const Verbose verbose) const override
        {
                (void)verbose;
                return false;
        }
};

class PropDisabledRanged : public Prop
{
public:
        PropDisabledRanged() :
                Prop(PropId::disabled_ranged) {}

        bool allow_attack_ranged(const Verbose verbose) const override
        {
                (void)verbose;
                return false;
        }
};

class PropParalyzed : public Prop
{
public:
        PropParalyzed() :
                Prop(PropId::paralyzed) {}

        void on_applied() override;

        int ability_mod(const AbilityId ability) const override
        {
                if (ability == AbilityId::dodging)
                {
                        return -999;
                }
                else
                {
                        return 0;
                }
        }

        bool allow_act() const override
        {
                return false;
        }

        bool allow_attack_ranged(const Verbose verbose) const override
        {
                (void)verbose;
                return false;
        }

        bool allow_attack_melee(const Verbose verbose) const override
        {
                (void)verbose;
                return false;
        }
};

class PropFainted : public Prop
{
public:
        PropFainted() :
                Prop(PropId::fainted) {}

        bool should_update_vision_on_toggled() const override;

        int ability_mod(const AbilityId ability) const override
        {
                if (ability == AbilityId::dodging)
                {
                        return -999;
                }
                else
                {
                        return 0;
                }
        }

        bool allow_act() const override
        {
                return false;
        }

        bool allow_see() const override
        {
                return false;
        }

        bool allow_attack_ranged(const Verbose verbose) const override
        {
                (void)verbose;
                return false;
        }

        bool allow_attack_melee(const Verbose verbose) const override
        {
                (void)verbose;
                return false;
        }

        PropEnded on_hit() override;
};

class PropSlowed : public Prop
{
public:
        PropSlowed() :
                Prop(PropId::slowed) {}

        void on_applied() override;
};

class PropHasted : public Prop
{
public:
        PropHasted() :
                Prop(PropId::hasted) {}

        void on_applied() override;
};

class PropExtraHasted : public Prop
{
public:
        PropExtraHasted() :
                Prop(PropId::extra_hasted) {}

        void on_applied() override;
};

class PropSummoned : public Prop
{
public:
        PropSummoned() :
                Prop(PropId::summoned) {}

        void on_end() override;
};

class PropFrenzied : public Prop
{
public:
        PropFrenzied() :
                Prop(PropId::frenzied) {}

        void on_applied() override;
        void on_end() override;

        bool allow_move_dir(Dir dir) override;

        bool allow_read_absolute(Verbose verbose) const override;
        bool allow_cast_intr_spell_absolute(Verbose verbose) const override;
        bool allow_pray(Verbose verbose) const override;

        bool is_resisting_other_prop(PropId prop_id) const override;

        int ability_mod(AbilityId ability) const override;
};

class PropRShock : public Prop
{
public:
        PropRShock() :
                Prop(PropId::r_shock) {}

        void on_applied() override;
};

class PropRAcid : public Prop
{
public:
        PropRAcid() :
                Prop(PropId::r_acid) {}

        DmgResistData is_resisting_dmg(DmgType dmg_type) const override;
};

class PropRConf : public Prop
{
public:
        PropRConf() :
                Prop(PropId::r_conf) {}

        void on_applied() override;

        bool is_resisting_other_prop(PropId prop_id) const override;
};

class PropRElec : public Prop
{
public:
        PropRElec() :
                Prop(PropId::r_elec) {}

        DmgResistData is_resisting_dmg(DmgType dmg_type) const override;
};

class PropRFear : public Prop
{
public:
        PropRFear() :
                Prop(PropId::r_fear) {}

        void on_applied() override;

        bool is_resisting_other_prop(PropId prop_id) const override;
};

class PropRSlow : public Prop
{
public:
        PropRSlow() :
                Prop(PropId::r_slow) {}

        void on_applied() override;

        bool is_resisting_other_prop(PropId prop_id) const override;
};

class PropRPhys : public Prop
{
public:
        PropRPhys() :
                Prop(PropId::r_phys) {}

        void on_applied() override;

        bool is_resisting_other_prop(PropId prop_id) const override;

        DmgResistData is_resisting_dmg(DmgType dmg_type) const override;
};

class PropRFire : public Prop
{
public:
        PropRFire() :
                Prop(PropId::r_fire) {}

        void on_applied() override;

        bool is_resisting_other_prop(PropId prop_id) const override;

        DmgResistData is_resisting_dmg(DmgType dmg_type) const override;
};

class PropRPoison : public Prop
{
public:
        PropRPoison() :
                Prop(PropId::r_poison) {}

        void on_applied() override;

        bool is_resisting_other_prop(PropId prop_id) const override;
};

class PropRSleep : public Prop
{
public:
        PropRSleep() :
                Prop(PropId::r_sleep) {}

        void on_applied() override;

        bool is_resisting_other_prop(PropId prop_id) const override;
};

class PropRDisease : public Prop
{
public:
        PropRDisease() :
                Prop(PropId::r_disease) {}

        void on_applied() override;

        bool is_resisting_other_prop(PropId prop_id) const override;
};

class PropRBlind : public Prop
{
public:
        PropRBlind() :
                Prop(PropId::r_blind) {}

        void on_applied() override;

        bool is_resisting_other_prop(PropId prop_id) const override;
};

class PropRPara : public Prop
{
public:
        PropRPara() :
                Prop(PropId::r_para) {}

        void on_applied() override;

        bool is_resisting_other_prop(PropId prop_id) const override;
};

class PropRBreath : public Prop
{
public:
        PropRBreath() :
                Prop(PropId::r_breath) {}
};

class PropLgtSens : public Prop
{
public:
        PropLgtSens() :
                Prop(PropId::light_sensitive) {}

        int get_extra_damage() const
        {
                return m_extra_dmg;
        }

        void raise_extra_damage_to(int dmg);

private:
        int m_extra_dmg {0};
};

class PropVortex : public Prop
{
public:
        PropVortex() :
                Prop(PropId::vortex) {}

        PropActResult on_act() override;

private:
        int m_cooldown {0};
};

class PropExplodesOnDeath : public Prop
{
public:
        PropExplodesOnDeath() :
                Prop(PropId::explodes_on_death) {}

        void on_death() override;
};

class PropSplitsOnDeath : public Prop
{
public:
        PropSplitsOnDeath() :
                Prop(PropId::splits_on_death) {}

        void on_death() override;

        bool prevent_std_death_msg() const
        {
                return m_prevent_std_death_msg;
        }

private:
        bool m_prevent_std_death_msg {true};
};

class PropCorpseEater : public Prop
{
public:
        PropCorpseEater() :
                Prop(PropId::corpse_eater) {}

        PropActResult on_act() override;
};

class PropTeleports : public Prop
{
public:
        PropTeleports() :
                Prop(PropId::teleports) {}

        PropActResult on_act() override;
};

class PropTeleportsAway : public Prop
{
public:
        PropTeleportsAway() :
                Prop(PropId::teleports_away) {}

        PropActResult on_act() override;
};

class PropAlwaysAware : public Prop
{
public:
        PropAlwaysAware() :
                Prop(PropId::always_aware) {}

        void on_std_turn() override;
};

class PropCorruptsEnvColor : public Prop
{
public:
        PropCorruptsEnvColor() :
                Prop(PropId::corrupts_env_color) {}

        void cycle_graphics() override;

        std::optional<Color> override_actor_color() const override;

        PropActResult on_act() override;

private:
        Color m_color {255, 255, 255};
};

class PropAltersEnv : public Prop
{
public:
        PropAltersEnv() :
                Prop(PropId::alters_env) {}

        void on_std_turn() override;
};

class PropRegenerating : public Prop
{
public:
        PropRegenerating() :
                Prop(PropId::regenerating) {}

        void on_std_turn() override;
};

class PropCorpseRises : public Prop
{
public:
        PropCorpseRises() :
                Prop(PropId::corpse_rises),
                m_has_risen(false),
                m_nr_turns_until_allow_rise(2) {}

        PropActResult on_act() override;

        void on_death() override;

private:
        bool m_has_risen;
        int m_nr_turns_until_allow_rise;
};

class PropSpawnsZombiePartsOnDestroyed : public Prop
{
public:
        PropSpawnsZombiePartsOnDestroyed() :
                Prop(PropId::spawns_zombie_parts_on_destroyed) {}

        void on_destroyed_alive() override;
        void on_destroyed_corpse() override;

private:
        void try_spawn_zombie_parts() const;

        void try_spawn_zombie_dust() const;

        bool is_allowed_to_spawn_parts_here() const;
};

class PropBreeds : public Prop
{
public:
        PropBreeds() :
                Prop(PropId::breeds) {}

        void on_std_turn() override;
};

class PropVomitsOoze : public Prop
{
public:
        PropVomitsOoze() :
                Prop(PropId::vomits_ooze) {}

        void on_std_turn() override;

private:
        bool m_has_triggered_before {false};
};

class PropConfusesAdjacent : public Prop
{
public:
        PropConfusesAdjacent() :
                Prop(PropId::confuses_adjacent) {}

        void on_std_turn() override;
};

class PropFrenzyPlayerOnSeen : public Prop
{
public:
        PropFrenzyPlayerOnSeen() :
                Prop(PropId::frenzy_player_on_seen) {}

        void on_player_see() override;
};

class PropAuraOfDecay : public Prop
{
public:
        PropAuraOfDecay() :
                Prop(PropId::aura_of_decay) {}

        void save() const override;

        void load() override;

        void on_std_turn() override;

        void set_dmg_range(const Range& range)
        {
                m_dmg_range = range;
        }

        void set_allow_instant_kill()
        {
                m_allow_instant_kill = true;
        }

private:
        int range() const;

        void run_effect_on_actors() const;

        void run_effect_on_env() const;

        void run_effect_on_env_at(const P& p) const;

        void print_msg_actor_hit(const actor::Actor& actor) const;

        Range m_dmg_range {1, 1};

        bool m_allow_instant_kill {false};
};

class PropMajorClaphamSummon : public Prop
{
public:
        PropMajorClaphamSummon() :
                Prop(PropId::major_clapham_summon) {}

        PropActResult on_act() override;
};

class PropAlliesPlayerGhoul : public Prop
{
public:
        PropAlliesPlayerGhoul() :
                Prop(PropId::allies_ghoul_player) {}

        PropActResult on_act() override;
};

class PropHitChancePenaltyCurse : public Prop
{
public:
        PropHitChancePenaltyCurse() :
                Prop(PropId::hit_chance_penalty_curse) {}

        int ability_mod(const AbilityId ability) const override
        {
                switch (ability)
                {
                case AbilityId::melee:
                case AbilityId::ranged:
                        return -10;

                default:
                        return 0;
                }
        }
};

class PropIncreasedShockCurse : public Prop
{
public:
        PropIncreasedShockCurse() :
                Prop(PropId::increased_shock_curse) {}

        int player_extra_min_shock() const override
        {
                return 10;
        }
};

class PropCannotReadCurse : public Prop
{
public:
        PropCannotReadCurse() :
                Prop(PropId::cannot_read_curse) {}

        bool allow_read_absolute(Verbose verbose) const override;
};

class PropErudition : public Prop
{
public:
        PropErudition() :
                Prop(PropId::erudition) {}

        bool should_end_on_spell_cast() const
        {
                return m_end_on_spell_cast;
        }

        void disable_end_on_spell_cast()
        {
                m_end_on_spell_cast = false;
        }

private:
        bool m_end_on_spell_cast {true};
};

class PropFrenziesSelf : public Prop
{
public:
        PropFrenziesSelf() :
                Prop(PropId::frenzies_self) {}

        PropActResult on_act() override;

private:
        int m_cooldown {0};
};

class PropSummonsLocusts : public Prop
{
public:
        PropSummonsLocusts() :
                Prop(PropId::summons_locusts) {}

        PropActResult on_act() override;

private:
        bool m_has_summoned {false};
};

class PropSpectralWpn : public Prop
{
public:
        PropSpectralWpn();

        void on_death() override;

        std::optional<std::string> override_actor_name_the() const override;

        std::optional<std::string> override_actor_name_a() const override;

        std::optional<gfx::TileId> override_actor_tile() const override;

        std::optional<char> override_actor_character() const override;

        std::optional<std::string> override_actor_descr() const override;

private:
        std::unique_ptr<item::Item> m_discarded_item {};

        std::string get_weapon_name() const;
};

#endif  // PROPERTY_HPP

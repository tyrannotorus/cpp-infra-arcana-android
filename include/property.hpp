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

namespace prop
{
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

extern const std::string g_property_ending_suffix;

void run_alter_env_effect(const P& origin, int change_pos_one_in_n = 6);

class Prop
{
public:
        Prop(Id id);

        virtual ~Prop() = default;

        Id id() const
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

        virtual PropEnded on_hit(
                const int dmg,
                const DmgType dmg_type,
                actor::Actor* const attacker)
        {
                (void)dmg;
                (void)dmg_type;
                (void)attacker;

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

        virtual int player_temporary_shock_change() const
        {
                return 0;
        }

        virtual int player_extra_min_shock() const
        {
                return 0;
        }

        virtual int armor_points() const
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

        virtual void on_melee_attack() {}

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

        virtual PropEnded on_moved_non_center_dir()
        {
                return PropEnded::no;
        }

        virtual bool is_resisting_other_prop(const Id prop_id) const
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

        const Id m_id;
        const PropData& m_data;

        int m_nr_turns_left;
        int m_nr_dlvls_left;

        int m_nr_turns_active {0};

        PropDurationMode m_duration_mode {PropDurationMode::standard};

        actor::Actor* m_owner {nullptr};
        PropSrc m_src {PropSrc::END};
        const item::Item* m_item_applying {nullptr};
};

class Terrified : public Prop
{
public:
        Terrified() :
                Prop(Id::terrified) {}

        int ability_mod(const AbilityId ability) const override
        {
                switch (ability) {
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

class Infected : public Prop
{
public:
        Infected() :
                Prop(Id::infected) {}

        std::optional<Color> override_property_color() const override
        {
                return colors::orange();
        }

        PropEnded on_actor_turn() override;

        void on_applied() override;

private:
        bool has_warned {false};
};

class Diseased : public Prop
{
public:
        Diseased() :
                Prop(Id::diseased) {}

        int affect_max_hp(int hp_max) const override;

        bool is_resisting_other_prop(Id prop_id) const override;

        void on_applied() override;
};

class Descend : public Prop
{
public:
        Descend() :
                Prop(Id::descend) {}

        PropEnded on_actor_turn() override;
};

class Burrowing : public Prop
{
public:
        Burrowing() :
                Prop(Id::burrowing) {}

        PropEnded on_actor_turn() override;
};

class ZuulPossessPriest : public Prop
{
public:
        ZuulPossessPriest() :
                Prop(Id::zuul_possess_priest) {}

        void on_placed() override;
};

class PossessedByZuul : public Prop
{
public:
        PossessedByZuul() :
                Prop(Id::possessed_by_zuul) {}

        void on_death() override;

        int affect_max_hp(const int hp_max) const override
        {
                return hp_max * 2;
        }
};

class Shapeshifts : public Prop
{
public:
        Shapeshifts() :
                Prop(Id::shapeshifts) {}

        void on_placed() override;

        void on_std_turn() override;

        void on_death() override;

private:
        void shapeshift(Verbose verbose) const;

        int m_countdown {-1};
};

class ZealotStop : public Prop
{
public:
        ZealotStop() :
                Prop(Id::zealot_stop) {}

        PropEnded affect_move_dir(Dir& dir) override;
};

class Poisoned : public Prop
{
public:
        Poisoned() :
                Prop(Id::poisoned) {}

        PropEnded on_actor_turn() override;
};

class Aiming : public Prop
{
public:
        Aiming() :
                Prop(Id::aiming) {}

        int ability_mod(const AbilityId ability) const override
        {
                if (ability == AbilityId::ranged) {
                        return 10;
                }
                else {
                        return 0;
                }
        }

        PropEnded on_hit(
                int dmg,
                DmgType dmg_type,
                actor::Actor* attacker) override;
};

class Blind : public Prop
{
public:
        Blind() :
                Prop(Id::blind) {}

        bool should_update_vision_on_toggled() const override;

        bool allow_read_absolute(Verbose verbose) const override;

        bool allow_see() const override
        {
                return false;
        }

        int ability_mod(const AbilityId ability) const override
        {
                switch (ability) {
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

class Recloaks : public Prop
{
public:
        Recloaks() :
                Prop(Id::recloaks) {}

        PropActResult on_act() override;
};

class SeeInvis : public Prop
{
public:
        SeeInvis() :
                Prop(Id::see_invis) {}

        void on_applied() override;

        bool is_resisting_other_prop(Id prop_id) const override;
};

class Blessed : public Prop
{
public:
        Blessed() :
                Prop(Id::blessed) {}

        void on_applied() override;

        void on_more(const Prop& new_prop) override;

        int ability_mod(AbilityId ability) const override;
};

class Cursed : public Prop
{
public:
        Cursed() :
                Prop(Id::cursed) {}

        int ability_mod(AbilityId ability) const override;

        void on_applied() override;

        void on_more(const Prop& new_prop) override;
};

class Doomed : public Prop
{
public:
        Doomed() :
                Prop(Id::doomed) {}

        bool allow_read_chance(
                Verbose verbose) const override;

        bool allow_cast_intr_spell_chance(
                Verbose verbose) const override;

        int ability_mod(AbilityId ability) const override;

        void on_applied() override;

        void on_more(const Prop& new_prop) override;
};

class Premonition : public Prop
{
public:
        Premonition() :
                Prop(Id::premonition) {}

        int ability_mod(const AbilityId ability) const override
        {
                (void)ability;

                if (ability == AbilityId::dodging) {
                        return 75;
                }
                else {
                        return 0;
                }
        }
};

class MagicSearching : public Prop
{
public:
        MagicSearching() :
                Prop(Id::magic_searching) {}

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

class Entangled : public Prop
{
public:
        Entangled() :
                Prop(Id::entangled) {}

        void on_applied() override;

        PropEnded affect_move_dir(Dir& dir) override;

private:
        bool try_player_end_with_machete();
};

class Burning : public Prop
{
public:
        Burning() :
                Prop(Id::burning) {}

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

class Confused : public Prop
{
public:
        Confused() :
                Prop(Id::confused) {}

        PropEnded affect_move_dir(Dir& dir) override;

        bool allow_attack_melee(Verbose verbose) const override;
        bool allow_attack_ranged(Verbose verbose) const override;
        bool allow_read_absolute(Verbose verbose) const override;
        bool allow_cast_intr_spell_absolute(Verbose verbose) const override;
        bool allow_pray(Verbose verbose) const override;
};

class Hallucinating : public Prop
{
public:
        Hallucinating() :
                Prop(Id::hallucinating) {}

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

class AstralOpiumAddict : public Prop
{
public:
        AstralOpiumAddict() :
                Prop(Id::astral_opium_addiction) {}

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

class Nailed : public Prop
{
public:
        Nailed() :
                Prop(Id::nailed) {}

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

class Wound : public Prop
{
public:
        Wound() :
                Prop(Id::wound) {}

        void save() const override;

        void load() override;

        std::string msg_end_player() const override;

        std::string name_short() const override;

        int ability_mod(AbilityId ability) const override;

        void on_more(const Prop& new_prop) override;

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

class Moribund : public Prop
{
public:
        Moribund() :
                Prop(Id::moribund) {}

        std::string name() const override;

        std::string name_short() const override;

        int ability_mod(AbilityId ability) const override;

        int armor_points() const override;

        bool has_bonus() const;
};

class HpSap : public Prop
{
public:
        HpSap();

        void save() const override;

        void load() override;

        std::string name_short() const override
        {
                return "Life Sapped(" + std::to_string(m_nr_drained) + ")";
        }

        void on_more(const Prop& new_prop) override;

        int affect_max_hp(int hp_max) const override;

        void set_nr_drained(int value);

private:
        int m_nr_drained;
};

class SpiSap : public Prop
{
public:
        SpiSap();

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

class MindSap : public Prop
{
public:
        MindSap();

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

class Waiting : public Prop
{
public:
        Waiting() :
                Prop(Id::waiting) {}

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

class DelayedByLiquid : public Prop
{
public:
        DelayedByLiquid() :
                Prop(Id::delayed_by_liquid) {}

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

class DisabledAttack : public Prop
{
public:
        DisabledAttack() :
                Prop(Id::disabled_attack) {}

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

class DisabledMelee : public Prop
{
public:
        DisabledMelee() :
                Prop(Id::disabled_melee) {}

        bool allow_attack_melee(const Verbose verbose) const override
        {
                (void)verbose;
                return false;
        }
};

class DisabledRanged : public Prop
{
public:
        DisabledRanged() :
                Prop(Id::disabled_ranged) {}

        bool allow_attack_ranged(const Verbose verbose) const override
        {
                (void)verbose;
                return false;
        }
};

class MeleeCooldown : public Prop
{
public:
        MeleeCooldown() :
                Prop(Id::melee_cooldown) {}

        void on_melee_attack() override;
};

class Paralyzed : public Prop
{
public:
        Paralyzed() :
                Prop(Id::paralyzed) {}

        void on_applied() override;

        int ability_mod(const AbilityId ability) const override
        {
                if (ability == AbilityId::dodging) {
                        return -999;
                }
                else {
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

class Fainted : public Prop
{
public:
        Fainted() :
                Prop(Id::fainted) {}

        bool should_update_vision_on_toggled() const override;

        int ability_mod(const AbilityId ability) const override
        {
                if (ability == AbilityId::dodging) {
                        return -999;
                }
                else {
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

        PropEnded on_hit(
                int dmg,
                DmgType dmg_type,
                actor::Actor* attacker) override;
};

class Slowed : public Prop
{
public:
        Slowed() :
                Prop(Id::slowed) {}

        void on_applied() override;
};

class Hasted : public Prop
{
public:
        Hasted() :
                Prop(Id::hasted) {}

        void on_applied() override;
};

class ExtraHasted : public Prop
{
public:
        ExtraHasted() :
                Prop(Id::extra_hasted) {}

        void on_applied() override;
};

class Summoned : public Prop
{
public:
        Summoned() :
                Prop(Id::summoned) {}

        void on_end() override;
};

class Frenzied : public Prop
{
public:
        Frenzied() :
                Prop(Id::frenzied) {}

        void on_applied() override;
        void on_end() override;

        bool allow_move_dir(Dir dir) override;

        bool allow_read_absolute(Verbose verbose) const override;
        bool allow_cast_intr_spell_absolute(Verbose verbose) const override;
        bool allow_pray(Verbose verbose) const override;

        bool is_resisting_other_prop(Id prop_id) const override;

        int ability_mod(AbilityId ability) const override;
};

class RShock : public Prop
{
public:
        RShock() :
                Prop(Id::r_shock) {}

        void on_applied() override;
};

class RAcid : public Prop
{
public:
        RAcid() :
                Prop(Id::r_acid) {}

        DmgResistData is_resisting_dmg(DmgType dmg_type) const override;
};

class RConf : public Prop
{
public:
        RConf() :
                Prop(Id::r_conf) {}

        void on_applied() override;

        bool is_resisting_other_prop(Id prop_id) const override;
};

class RElec : public Prop
{
public:
        RElec() :
                Prop(Id::r_elec) {}

        DmgResistData is_resisting_dmg(DmgType dmg_type) const override;
};

class RFear : public Prop
{
public:
        RFear() :
                Prop(Id::r_fear) {}

        void on_applied() override;

        bool is_resisting_other_prop(Id prop_id) const override;
};

class RSlow : public Prop
{
public:
        RSlow() :
                Prop(Id::r_slow) {}

        void on_applied() override;

        bool is_resisting_other_prop(Id prop_id) const override;
};

class RPhys : public Prop
{
public:
        RPhys() :
                Prop(Id::r_phys) {}

        void on_applied() override;

        bool is_resisting_other_prop(Id prop_id) const override;

        DmgResistData is_resisting_dmg(DmgType dmg_type) const override;
};

class RFire : public Prop
{
public:
        RFire() :
                Prop(Id::r_fire) {}

        void on_applied() override;

        bool is_resisting_other_prop(Id prop_id) const override;

        DmgResistData is_resisting_dmg(DmgType dmg_type) const override;
};

class RPoison : public Prop
{
public:
        RPoison() :
                Prop(Id::r_poison) {}

        void on_applied() override;

        bool is_resisting_other_prop(Id prop_id) const override;
};

class RSleep : public Prop
{
public:
        RSleep() :
                Prop(Id::r_sleep) {}

        void on_applied() override;

        bool is_resisting_other_prop(Id prop_id) const override;
};

class RDisease : public Prop
{
public:
        RDisease() :
                Prop(Id::r_disease) {}

        void on_applied() override;

        bool is_resisting_other_prop(Id prop_id) const override;
};

class RBlind : public Prop
{
public:
        RBlind() :
                Prop(Id::r_blind) {}

        void on_applied() override;

        bool is_resisting_other_prop(Id prop_id) const override;
};

class RPara : public Prop
{
public:
        RPara() :
                Prop(Id::r_para) {}

        void on_applied() override;

        bool is_resisting_other_prop(Id prop_id) const override;
};

class RBreath : public Prop
{
public:
        RBreath() :
                Prop(Id::r_breath) {}
};

class LgtSens : public Prop
{
public:
        LgtSens() :
                Prop(Id::light_sensitive) {}

        int get_extra_damage() const
        {
                return m_extra_dmg;
        }

        void raise_extra_damage_to(int dmg);

private:
        int m_extra_dmg {0};
};

class Vortex : public Prop
{
public:
        Vortex() :
                Prop(Id::vortex) {}

        PropActResult on_act() override;

private:
        int m_cooldown {0};
};

class ExplodesOnDeath : public Prop
{
public:
        ExplodesOnDeath() :
                Prop(Id::explodes_on_death) {}

        void on_death() override;
};

class SplitsOnDeath : public Prop
{
public:
        SplitsOnDeath() :
                Prop(Id::splits_on_death) {}

        void on_death() override;

        bool prevent_std_death_msg() const
        {
                return m_prevent_std_death_msg;
        }

private:
        bool m_prevent_std_death_msg {true};
};

class OthersTerrifiedOnDeath : public Prop
{
public:
        OthersTerrifiedOnDeath() :
                Prop(Id::others_terrified_on_death) {}

        void on_death() override;
};

class CorpseEater : public Prop
{
public:
        CorpseEater() :
                Prop(Id::corpse_eater) {}

        PropActResult on_act() override;
};

class Teleports : public Prop
{
public:
        Teleports() :
                Prop(Id::teleports) {}

        PropActResult on_act() override;
};

class TeleportsAway : public Prop
{
public:
        TeleportsAway() :
                Prop(Id::teleports_away) {}

        PropActResult on_act() override;
};

class AlwaysAware : public Prop
{
public:
        AlwaysAware() :
                Prop(Id::always_aware) {}

        void on_std_turn() override;
};

class CorruptsEnvColor : public Prop
{
public:
        CorruptsEnvColor() :
                Prop(Id::corrupts_env_color) {}

        void cycle_graphics() override;

        std::optional<Color> override_actor_color() const override;

        PropActResult on_act() override;

private:
        Color m_color {255, 255, 255};
};

class AltersEnv : public Prop
{
public:
        AltersEnv() :
                Prop(Id::alters_env) {}

        void on_std_turn() override;
};

class Regenerating : public Prop
{
public:
        Regenerating() :
                Prop(Id::regenerating) {}

        void on_std_turn() override;
};

class CorpseRises : public Prop
{
public:
        CorpseRises() :
                Prop(Id::corpse_rises),
                m_has_risen(false),
                m_nr_turns_until_allow_rise(2) {}

        PropActResult on_act() override;

        void on_death() override;

private:
        bool m_has_risen;
        int m_nr_turns_until_allow_rise;
};

class SpawnsZombiePartsOnDestroyed : public Prop
{
public:
        SpawnsZombiePartsOnDestroyed() :
                Prop(Id::spawns_zombie_parts_on_destroyed) {}

        void on_destroyed_alive() override;
        void on_destroyed_corpse() override;

private:
        void try_spawn_zombie_parts() const;

        void try_spawn_zombie_dust() const;

        bool is_allowed_to_spawn_parts_here() const;
};

class Breeds : public Prop
{
public:
        Breeds() :
                Prop(Id::breeds) {}

        void on_std_turn() override;
};

class VomitsOoze : public Prop
{
public:
        VomitsOoze() :
                Prop(Id::vomits_ooze) {}

        void on_std_turn() override;

private:
        bool m_has_triggered_before {false};
};

class ConfusesAdjacent : public Prop
{
public:
        ConfusesAdjacent() :
                Prop(Id::confuses_adjacent) {}

        void on_std_turn() override;
};

class FrenzyPlayerOnSeen : public Prop
{
public:
        FrenzyPlayerOnSeen() :
                Prop(Id::frenzy_player_on_seen) {}

        void on_player_see() override;
};

class AuraOfDecay : public Prop
{
public:
        AuraOfDecay() :
                Prop(Id::aura_of_decay) {}

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

class MajorClaphamSummon : public Prop
{
public:
        MajorClaphamSummon() :
                Prop(Id::major_clapham_summon) {}

        PropActResult on_act() override;
};

class AlliesPlayerGhoul : public Prop
{
public:
        AlliesPlayerGhoul() :
                Prop(Id::allies_ghoul_player) {}

        PropActResult on_act() override;
};

class HitChancePenaltyCurse : public Prop
{
public:
        HitChancePenaltyCurse() :
                Prop(Id::hit_chance_penalty_curse) {}

        int ability_mod(const AbilityId ability) const override
        {
                switch (ability) {
                case AbilityId::melee:
                case AbilityId::ranged:
                        return -10;

                default:
                        return 0;
                }
        }
};

class IncreasedShockCurse : public Prop
{
public:
        IncreasedShockCurse() :
                Prop(Id::increased_shock_curse) {}

        int player_extra_min_shock() const override
        {
                return 10;
        }
};

class CannotReadCurse : public Prop
{
public:
        CannotReadCurse() :
                Prop(Id::cannot_read_curse) {}

        bool allow_read_absolute(Verbose verbose) const override;
};

class Erudition : public Prop
{
public:
        Erudition() :
                Prop(Id::erudition) {}

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

class FrenziesSelf : public Prop
{
public:
        FrenziesSelf() :
                Prop(Id::frenzies_self) {}

        PropActResult on_act() override;

private:
        int m_cooldown {0};
};

class FrenziesFollowers : public Prop
{
public:
        FrenziesFollowers() :
                Prop(Id::frenzies_followers) {}

        PropActResult on_act() override;

private:
        int m_cooldown {0};
};

class SummonsLocusts : public Prop
{
public:
        SummonsLocusts() :
                Prop(Id::summons_locusts) {}

        PropActResult on_act() override;

private:
        bool m_has_summoned {false};
};

class SpectralWpn : public Prop
{
public:
        SpectralWpn();

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

class Thorns : public Prop
{
public:
        Thorns() :
                Prop(Id::thorns) {}

        void save() const override;
        void load() override;

        PropEnded on_hit(
                int dmg,
                DmgType dmg_type,
                actor::Actor* attacker) override;

        void set_dmg(const int dmg)
        {
                m_dmg = dmg;
        }

private:
        void hit_actor(actor::Actor& target);

        void print_msg_player_retaliate_mon(const actor::Actor& target) const;
        void print_msg_mon_retaliate_player() const;
        void print_msg_mon_retaliate_mon(const actor::Actor& target) const;

        int m_dmg {1};
};

class Sanctuary : public Prop
{
public:
        Sanctuary() :
                Prop(Id::sanctuary) {}

        PropEnded on_moved_non_center_dir() override;
};

class CrimsonPassage : public Prop
{
public:
        CrimsonPassage() :
                Prop(Id::crimson_passage) {}

        void save() const override;
        void load() override;

        PropEnded on_moved_non_center_dir() override;

        void set_nr_steps_allowed(const int nr_steps)
        {
                m_nr_steps_allowed = nr_steps;
        }

private:
        int m_nr_steps_allowed {1};
        int m_nr_steps_taken {0};
};

}  // namespace prop

#endif  // PROPERTY_HPP

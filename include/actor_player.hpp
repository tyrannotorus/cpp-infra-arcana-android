// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef ACTOR_PLAYER_HPP
#define ACTOR_PLAYER_HPP

#include "actor.hpp"
#include "colors.hpp"
#include "global.hpp"
#include "direction.hpp"
#include "spells.hpp"

class Snd;
template <typename T> class Array2;

namespace item
{
class Explosive;
class Item;
class MedicalBag;
class Wpn;
}  // namespace item

enum class Phobia
{
        rat,
        spider,
        dog,
        undead,
        open_place,
        cramped_place,
        deep_places,
        dark,
        END
};

enum class Obsession
{
        sadism,
        masochism,
        END
};

enum class ShockSrc
{
        see_mon,
        use_strange_item,
        cast_intr_spell,
        time,
        misc,
        END
};

namespace actor
{
class Player : public Actor
{
public:
        Player();
        ~Player();

        void save() const;
        void load();

        void update_fov();

        void update_mon_awareness();

        Color color() const override;

        SpellSkill spell_skill(SpellId id) const override;

        void hear_sound(
                const Snd& snd,
                bool is_origin_seen_by_player,
                Dir dir_to_origin,
                int percent_audible_distance);

        void incr_shock(ShockLvl shock_lvl, ShockSrc shock_src);

        void incr_shock(double shock, ShockSrc shock_src);

        double shock_lvl_to_value(ShockLvl shock_lvl) const;

        void restore_shock(
                int amount_restored,
                bool is_temp_shock_restored);

        int shock_tot() const;

        int ins() const;

        void auto_melee();

        item::Wpn& unarmed_wpn() const;

        void set_unarmed_wpn(item::Wpn* wpn)
        {
                m_unarmed_wpn = wpn;
        }

        void kick_mon(Actor& defender);
        void hand_att(Actor& defender);

        void add_light_hook(Array2<bool>& light_map) const override;

        void on_log_msg_printed();  // Aborts e.g. searching and quick move
        void interrupt_actions();  // Aborts e.g. healing

        int enc_percent() const;

        int carry_weight_lmt() const;

        void set_auto_move(Dir dir);

        bool is_leader_of(const Actor* actor) const override;
        bool is_actor_my_leader(const Actor* actor) const override;

        void on_new_dlvl_reached();

        void update_tmp_shock();

        void add_shock_from_seen_monsters();

        void incr_insanity();

        bool is_busy() const;

        // Randomly prints a message such as "I sense an object of great power
        // here" if there is a major treasure on the map (on the floor or in a
        // container), and the player is a Rogue
        void item_feeling();

        // Randomly prints a message such as "A chill runs down my spine" if
        // there are unique monsters on the map, and the player is a Rogue
        void mon_feeling();

        item::MedicalBag* m_active_medical_bag {nullptr};
        int m_equip_armor_countdown {0};
        int m_remove_armor_countdown {0};
        bool m_is_dropping_armor_from_body_slot {false};
        item::Item* m_item_equipping {nullptr};
        item::Explosive* m_active_explosive {nullptr};
        item::Item* m_last_thrown_item {nullptr};
        Actor* m_tgt {nullptr};
        int m_wait_turns_left {-1};
        int m_ins {0};
        double m_shock {0.0};
        double m_shock_tmp {0.0};
        int m_nr_turns_until_ins {-1};
        Dir m_auto_move_dir {Dir::END};
        bool m_has_taken_auto_move_step {false};
        int m_nr_turns_until_rspell {-1};
        item::Wpn* m_unarmed_wpn {nullptr};

private:
        int shock_resistance(ShockSrc shock_src) const;

        double shock_taken_after_mods(
                double base_shock,
                ShockSrc shock_src) const;

        void on_hit(
                int dmg,
                DmgType dmg_type,
                AllowWound allow_wound) override;

        void fov_hack();
};

}  // namespace actor

#endif  // ACTOR_PLAYER_HPP

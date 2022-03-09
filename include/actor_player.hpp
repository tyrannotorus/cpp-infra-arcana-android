// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef ACTOR_PLAYER_HPP
#define ACTOR_PLAYER_HPP

#include "actor.hpp"
#include "colors.hpp"
#include "direction.hpp"
#include "global.hpp"
#include "spells.hpp"

class Snd;
template <typename T>
class Array2;

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

        void incr_shock(double shock, ShockSrc shock_src);

        void restore_shock(int amount_restored, bool is_temp_shock_restored);

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

        // Only interrupts repeated commands like waiting.
        void on_log_msg_printed();

        // Aborts e.g. healing. "is_forced" controlls if querying is allowed
        // (for example if the player is seeing a monster, the game shall query
        // the player to continue, but if the player is knocked back the healing
        // should just be aborted).
        void interrupt_actions(ForceInterruptActions is_forced);

        int enc_percent() const;

        int carry_weight_lmt() const;

        void set_auto_move(Dir dir);

        void on_new_dlvl_reached();

        void update_tmp_shock();

        void add_shock_from_seen_monsters();

        void incr_insanity();

        bool is_busy() const;

        // Is the player busy with something that would result in a query on
        // interruption? ("Do you want to continue?")
        bool is_busy_queryable_action() const;

        // Randomly prints a message such as "I sense an object of great power
        // here" if there is a major treasure on the map (on the floor or in a
        // container), and the player is a Rogue
        void item_feeling();

        // Randomly prints a message such as "A chill runs down my spine" if
        // there are unique monsters on the map, and the player is a Rogue
        void mon_feeling();

        // Inventory and item handling state
        item::MedicalBag* m_active_medical_bag {nullptr};
        int m_equip_armor_countdown {0};
        int m_remove_armor_countdown {0};
        bool m_is_dropping_armor_from_body_slot {false};
        item::Item* m_item_equipping {nullptr};
        item::Explosive* m_active_explosive {nullptr};
        item::Item* m_last_thrown_item {nullptr};
        item::Wpn* m_unarmed_wpn {nullptr};

        // Player target
        Actor* m_tgt {nullptr};

        // "Five turn waiting" (long wait command) state
        int m_wait_turns_left {-1};

        // Auto-move command state
        Dir m_auto_move_dir {Dir::END};
        bool m_has_taken_auto_move_step {false};

        // Shock and insanity state
        int m_ins {0};
        double m_shock {0.0};
        double m_shock_tmp {0.0};
        int m_nr_turns_until_ins {-1};

        // Cooldowns to regain effects
        int m_nr_turns_until_r_spell {-1};
        int m_nr_turns_until_meditative_focused {-1};

        // Current color for lantern flickering effect
        Color m_lantern_color {};

        // State for keeping track of if a monster should be warned about
        // ("[...] is in my view!").
        Actor* seen_mon_to_warn_about {nullptr};
        bool allow_print_mon_warning {false};

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

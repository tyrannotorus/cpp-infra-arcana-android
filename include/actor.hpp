// =============================================================================
// Copyright 2011-2021 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef ACTOR_HPP
#define ACTOR_HPP

#include <string>
#include <vector>

#include "ability_values.hpp"
#include "actor_data.hpp"
#include "colors.hpp"
#include "direction.hpp"
#include "gfx.hpp"
#include "global.hpp"
#include "inventory.hpp"
#include "pos.hpp"
#include "property_handler.hpp"
#include "spells.hpp"

template <typename T>
class Array2;

namespace actor
{
class Actor;
class Mon;

struct AiState
{
        Actor* target {nullptr};
        bool is_target_seen {false};
        MonRoamingAllowed is_roaming_allowed {MonRoamingAllowed::yes};
        P spawn_pos {};
        Dir last_dir_moved {Dir::center};

        // AI creatures pauses every second step while not aware or wary, this
        // tracks the state of the pausing
        bool is_waiting {false};
};

struct AwareState
{
        int wary_counter {0};
        int aware_counter {0};
        int player_aware_of_me_counter {0};
        bool is_msg_mon_in_view_printed {false};
        bool is_player_feeling_msg_allowed {true};
};

struct MonSpell
{
        Spell* spell {nullptr};
        SpellSkill skill {(SpellSkill)0};
        int cooldown {-1};
};

int max_hp(const Actor& actor);

int max_sp(const Actor& actor);

bool is_player(const Actor* actor);

void init_actor(Actor& actor, const P& pos_, ActorData& data);

void print_aware_invis_mon_msg(const Mon& mon);

void make_player_aware_mon(Actor& actor);
void make_player_aware_seen_monsters();

class Actor
{
public:
        virtual ~Actor();

        Id id() const
        {
                return m_data->id;
        }

        virtual Color color() const = 0;
        gfx::TileId tile() const;
        char character() const;
        std::string name_the() const;
        std::string name_a() const;
        std::string descr() const;

        bool is_leader_of(const Actor* actor) const;
        bool is_actor_my_leader(const Actor* actor) const;
        bool is_in_same_group_as(const Actor* actor) const;

        int ability(
                AbilityId id,
                bool is_affected_by_props) const;

        bool restore_hp(
                int hp_restored,
                bool is_allowed_above_max = false,
                Verbose verbose = Verbose::yes);

        bool restore_sp(
                int spi_restored,
                bool is_allowed_above_max = false,
                Verbose verbose = Verbose::yes);

        void change_max_hp(
                int change,
                Verbose verbose = Verbose::yes);

        void change_max_sp(
                int change,
                Verbose verbose = Verbose::yes);

        int armor_points() const;

        void add_light(Array2<bool>& light_map) const;

        virtual void add_light_hook(Array2<bool>& light) const
        {
                (void)light;
        }

        bool is_alive() const
        {
                return m_state == ActorState::alive;
        }

        bool is_corpse() const
        {
                return m_state == ActorState::corpse;
        }

        std::string death_msg() const;

        virtual void on_hit(
                const int dmg,
                const DmgType dmg_type,
                const AllowWound allow_wound)
        {
                (void)dmg;
                (void)dmg_type;
                (void)allow_wound;
        }

        virtual SpellSkill spell_skill(SpellId id) const = 0;

        bool is_aware_of_player() const
        {
                return m_mon_aware_state.aware_counter > 0;
        }

        bool is_wary_of_player() const
        {
                return m_mon_aware_state.wary_counter > 0;
        }

        bool is_player_aware_of_me() const
        {
                return m_mon_aware_state.player_aware_of_me_counter > 0;
        }

        P m_pos {};
        ActorState m_state {ActorState::alive};
        int m_hp {-1};
        int m_base_max_hp {-1};
        int m_sp {-1};
        int m_base_max_sp {-1};
        PropHandler m_properties {this};
        Inventory m_inv {this};
        ActorData* m_data {nullptr};
        int m_delay {0};
        P m_opening_door_pos {-1, -1};

        // Monster specific data
        AiState m_ai_state {};
        AwareState m_mon_aware_state {};
        bool m_give_hit_chance_penalty_vs_player {true};
        Actor* m_leader {nullptr};
        std::vector<MonSpell> m_mon_spells {};
        const ActorData* m_mimic_data {nullptr};  // Hallucination

protected:
        // Damages worn armor, and returns damage after armor absorbs damage
        int hit_armor(int dmg);
};

}  // namespace actor

#endif  // ACTOR_HPP

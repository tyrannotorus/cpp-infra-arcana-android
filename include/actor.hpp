// =============================================================================
// Copyright 2011-2024 Martin Törnqvist <m.tornq@gmail.com>
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
#include "sound.hpp"
#include "spells.hpp"

template <typename T>
class Array2;

namespace item
{
class Wpn;
}  // namespace item

namespace actor
{
class Actor;

struct AiState
{
        Actor* target {nullptr};
        bool is_target_seen {false};
        MonRoamingAllowed is_roaming_allowed {MonRoamingAllowed::yes};
        P spawn_pos {};
        Dir last_dir_moved {Dir::center};

        // AI creatures pauses every second step while not aware or wary, this
        // tracks the state of the pausing.
        bool is_waiting_unaware {false};

        // Some monster types will skip a turn upon seeing the player (to avoid
        // unfair instant deaths by allowing the player to "shoot first").
        //
        // NOTE: This is different from monsters pausing when DETECTING the
        // player by looking (and BECOMING aware), which can also cause them to
        // skip a turn. The waiting handled here is supposed to make the monster
        // wait when seeing the player, while they are ALREADY aware.
        //
        bool do_wait_on_player_seen_aware {false};

        // This is used for controlling actor speed - the monster should have
        // normal speed while waiting.
        bool is_waiting_on_player_seen_aware {false};
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

enum class AwareSource
{
        seeing,
        heard_sound,
        attacked,
        spell_victim,
        other,
};

// Allow filling HP/SP above max limit?
enum class AllowRestoreAboveMax
{
        no,
        yes,
};

// -----------------------------------------------------------------------------
// Common functions
// -----------------------------------------------------------------------------
void init_actor(Actor& actor, const P& pos, ActorData& data);

std::string id(const actor::Actor& actor);
Color color(const Actor& actor);
gfx::TileId tile(const Actor& actor);
char character(const Actor& actor);
std::string name_the(const Actor& actor);
std::string name_a(const Actor& actor);
std::string descr(const Actor& actor);

int max_hp(const Actor& actor);
int max_sp(const Actor& actor);

bool is_player(const Actor* actor);

void add_light(const Actor& actor, Array2<bool>& light_map);

SpellSkill spell_skill(const Actor& actor, SpellId id);

int ability(
        const Actor& actor,
        AbilityId id,
        AbilityAffectedByProperties affected_by_props = AbilityAffectedByProperties::yes);

bool restore_hp(
        Actor& actor,
        int hp_restored,
        AllowRestoreAboveMax allow_above_max = AllowRestoreAboveMax::no,
        Verbose verbose = Verbose::yes);

bool restore_sp(
        Actor& actor,
        int sp_restored,
        AllowRestoreAboveMax allow_above_max = AllowRestoreAboveMax::no,
        Verbose verbose = Verbose::yes);

void change_max_hp(
        Actor& actor,
        int change,
        Verbose verbose = Verbose::yes);

void change_max_sp(
        Actor& actor,
        int change,
        Verbose verbose = Verbose::yes);

bool is_alive(const actor::Actor& actor);

bool is_corpse(const actor::Actor& actor);

std::string death_msg(const actor::Actor& actor);

int armor_points(const actor::Actor& actor);

// NOTE: If the actors are the same actor, they are considered to be in the same
// group (the actor is in the same group as itself).
bool is_in_same_group(const Actor* actor_1, const Actor* actor_2);

std::vector<Actor*> other_actors_in_same_group(const actor::Actor* actor);
int nr_other_actors_in_same_group(const actor::Actor* actor);

// -----------------------------------------------------------------------------
// Player specific functions
// -----------------------------------------------------------------------------
void make_player_aware_mon(Actor& actor);
void make_player_aware_seen_monsters();

void print_player_aware_invis_mon_msg(const Actor& mon);

void update_player_fov();

int player_exorcist_max_fervor();

bool restore_exorcist_fervor(
        int fervor_restored,
        Verbose verbose = Verbose::yes);

// -----------------------------------------------------------------------------
// Monster specific functions
// -----------------------------------------------------------------------------
bool is_aware_of_player(const actor::Actor& actor);
bool is_wary_of_player(const actor::Actor& actor);
bool is_player_aware_of_me(const actor::Actor& actor);

std::string get_cultist_phrase();
std::string get_cultist_aware_msg_seen(const Actor& actor);
std::string get_cultist_aware_msg_hidden();

// -----------------------------------------------------------------------------
// Actor
// -----------------------------------------------------------------------------
class Actor
{
public:
        virtual ~Actor();

        // NOTE: These functions should be member functions, since otherwise the
        // parameters would be very confusing and there would be a high risk of
        // mistakes. I.e. it would look something like this:
        //
        //     bool is_leader(const Actor& follower, const Actor* const leader)
        //
        // And the call would look like this:
        //
        //     is_leader(actor, other_actor)
        //
        bool is_leader_of(const Actor* actor) const;
        bool is_actor_my_leader(const Actor* actor) const;

        // ==================================================
        // Player specific public functions
        // TODO: These should be free functions instead, refactor.
        // ==================================================
        void save() const;
        void load();
        void update_mon_awareness() const;
        void incr_shock(double shock, ShockSrc shock_src);
        void restore_shock(int amount_restored, bool is_temp_shock_restored);
        int shock_tot() const;
        int insanity() const;
        item::Wpn& unarmed_wpn() const;
        item::Wpn* make_kick_wpn(const Actor& mon_kicked) const;
        void set_unarmed_wpn(item::Wpn* wpn) const;
        void kick_mon(Actor& defender);
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
        void mon_feeling() const;

        // ==================================================
        // Monster specific public functions
        // TODO: These should be free functions instead, refactor.
        // ==================================================
        bool is_sneaking() const;
        AiAvailAttacksData avail_attacks(Actor& defender) const;
        AiAttData choose_attack(const AiAvailAttacksData& avail_attacks) const;
        DidAction try_attack(Actor& defender);
        int nr_turns_to_be_aware(int factor = 1) const;
        void become_aware_player(AwareSource source, int factor = 1);
        void become_wary_player();
        void make_player_aware_of_me(int duration_factor = 1);
        std::vector<Actor*> foes_aware_of() const;
        std::string aware_msg_mon_seen() const;
        std::string aware_msg_mon_hidden() const;
        void speak_phrase(AlertsMon alerts_others);
        void add_spell(SpellSkill skill, Spell* spell);
        bool has_ai(actor::AiId id) const;

        // Common creature state (for player and monsters)
        P m_pos {};
        ActorState m_state {ActorState::alive};
        int m_hp {-1};
        int m_base_max_hp {-1};
        int m_sp {-1};
        int m_base_max_sp {-1};
        prop::PropHandler m_properties {this};
        Inventory m_inv {this};
        ActorData* m_data {nullptr};
        int m_delay {0};
        P m_opening_door_pos {-1, -1};

        // Monster specific state
        //
        // NOTE: Player specific state is in "actor_player_state.hpp".
        //
        AiState m_ai_state {};
        AwareState m_mon_aware_state {};
        Actor* m_leader {nullptr};
        std::vector<MonSpell> m_mon_spells {};
        const ActorData* m_mimic_data {nullptr};  // Hallucination

private:
        // ==================================================
        // Player specific private functions
        // TODO: These should be free functions instead, refactor.
        // ==================================================
        int shock_resistance(ShockSrc shock_src) const;
        double shock_taken_after_mods(double base_shock, ShockSrc shock_src) const;
        double increased_tmp_chock_on_blind() const;
        double increased_tmp_shock_from_dark() const;
        double reduced_tmp_shock_from_light() const;
        double increased_tmp_shock_from_adjacent_terrain() const;
        void fov_hack() const;

        // ==================================================
        // Monster specific private functions
        // TODO: These should be free functions instead, refactor.
        // ==================================================
        void print_player_see_mon_become_aware_msg() const;
        void print_player_see_mon_become_wary_msg() const;
        bool is_ranged_attack_blocked(const P& target_pos) const;
        item::Wpn* avail_wielded_melee() const;
        item::Wpn* avail_wielded_ranged() const;
        std::vector<item::Wpn*> avail_intr_melee() const;
        std::vector<item::Wpn*> avail_intr_ranged() const;
        bool should_reload(const item::Wpn& wpn) const;
        int nr_mon_in_group() const;

        // Damages worn armor, and returns damage after armor absorbs damage
        int hit_armor(int dmg);
};

}  // namespace actor

#endif  // ACTOR_HPP

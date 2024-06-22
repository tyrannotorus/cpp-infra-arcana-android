// =============================================================================
// Copyright 2011-2024 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef PROPERTY_DATA_HPP
#define PROPERTY_DATA_HPP

#include <cstddef>
#include <string>

#include "random.hpp"

namespace prop
{
// NOTE: When updating this, also update the string -> id map
enum class Id
{
        r_phys,
        r_fire,
        r_poison,
        r_elec,
        r_acid,
        r_sleep,
        r_fear,
        r_slow,
        r_conf,
        r_breath,
        r_disease,
        r_shock,
        // NOTE: The purpose of this is only to prevent blindness for "eyeless"
        // monsters (e.g. constructs such as animated weapons), and is only
        // intended as a natural property - not for e.g. gas masks.
        r_blind,
        r_para,  // Mostly intended as a natural property for monsters
        r_spell,
        light_sensitive,
        blind,
        deaf,
        fainted,
        burning,
        radiant_self,
        radiant_adjacent,
        radiant_fov,
        invis,
        cloaked,
        recloaks,
        see_invis,
        darkvision,
        poisoned,
        paralyzed,
        delayed_by_liquid,  // Delayed for a turn due to bumping liquid terrain
        terrified,
        confused,
        hallucinating,
        stunned,
        slowed,
        hasted,
        extra_hasted,
        infected,
        diseased,
        weakened,
        frenzied,
        blessed,
        cursed,
        doomed,
        premonition,
        erudition,
        magic_searching,
        entangled,
        stuck,
        tele_ctrl,
        spell_reflect,
        conflict,
        vortex,  // Vortex monsters pulling the player
        explodes_on_death,
        splits_on_death,
        corpse_eater,
        teleports,
        teleports_away,
        always_aware,
        corrupts_env_color,  // "Strange color" monster corrupting the area
        alters_env,
        regenerating,
        corpse_rises,
        spawns_zombie_parts_on_destroyed,
        breeds,
        vomits_ooze,            // Gla'Suu
        confuses_adjacent,      // "Strange color" confusing player when seen
        frenzy_player_on_seen,  // Ghastly Light
        aura_of_decay,          // Damages adjacent hostile creatures
        reduced_pierce_dmg,     // E.g. worm masses
        short_hearing_range,
        frenzies_self,       // E.g. Apes
        frenzies_followers,  // E.g. Pickman
        summons_locusts,     // Khephren ability
        // Monster with this property terrifies other monsters with this
        // property on death ("cowardly monsters", e.g. Troglodytes).
        others_terrified_on_death,

        // Properties describing the actors body or method of moving around.
        // These affect which terrain types the actor can move through, but
        // may have other effects as well.
        flying,
        tiny_flying,  // E.g. Locusts
        ethereal,
        ooze,
        small_crawling,
        burrowing,

        // Properties mostly used for AI control.
        waiting,  // Prevent acting - also used for player
        disabled_attack,
        disabled_melee,
        disabled_ranged,
        melee_cooldown,  // After a melee attack, cannot attack again for a while.

        // Properties for supporting specific game mechanics (NOT intended to be
        // used in a general way).
        descend,
        zuul_possess_priest,
        possessed_by_zuul,
        shapeshifts,  // For the Shapeshifter monster
        zealot_stop,  // The Zealot pauses and "gropes about"
        major_clapham_summon,
        allies_ghoul_player,
        spectral_wpn,
        aiming,
        nailed,
        wound,
        summoned,
        hp_sap,
        spi_sap,
        mind_sap,
        hit_chance_penalty_curse,
        increased_shock_curse,
        cannot_read_curse,
        light_sensitive_curse,  // This is just a copy of light_sensitive
        disabled_hp_regen,
        sanctuary,
        astral_opium_addiction,
        meditative_focused,  // From Meditative trait
        flagellant,          // Used for applying moribund when hit
        moribund,            // Flagellant low health bonuses
        thorns,              // From the Thorns spell
        crimson_passage,     // From the Crimson Passage spell

        END
};

enum class PropAlignment
{
        good,
        bad,
        neutral
};

struct PropData
{
        Id id {Id::END};
        Range std_rnd_turns {10, 10};
        Range std_rnd_dlvls {0, 0};
        std::string name {};
        std::string name_short {};
        std::string descr {};
        std::string msg_start_player {};
        std::string msg_start_mon {};
        std::string msg_end_player {};
        std::string msg_end_mon {};
        std::string msg_res_player {};
        std::string msg_res_mon {};
        std::string historic_msg_start_permanent {};
        std::string historic_msg_end_permanent {};
        bool allow_display_turns {true};
        bool update_vision_on_toggled {false};
        bool force_interrupt_player_on_start {false};
        bool is_preventable_by_player_trait {false};
        bool allow_test_on_bot {false};
        PropAlignment alignment {PropAlignment::neutral};
};

extern PropData g_data[(size_t)Id::END];

void init();

Id str_to_prop_id(const std::string& str);

std::string descr(Id id);

}  // namespace prop

#endif  // PROPERTY_DATA_HPP

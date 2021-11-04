// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef ACTOR_DATA_HPP
#define ACTOR_DATA_HPP

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "ability_values.hpp"
#include "audio_data.hpp"
#include "colors.hpp"
#include "gfx.hpp"
#include "global.hpp"
#include "item_att_property.hpp"
#include "item_data.hpp"
#include "property_data.hpp"
#include "random.hpp"
#include "room.hpp"
#include "spells.hpp"

namespace actor
{
enum class Id
{
        player,
        zombie,
        bloated_zombie,
        major_clapham_lee,
        dean_halsey,
        crawling_intestines,
        crawling_hand,
        thing,
        floating_skull,
        cultist,
        zealot,
        bog_tcher,
        keziah_mason,
        brown_jenkin,
        cultist_priest,
        cultist_wizard,
        cultist_arch_wizard,
        green_spider,
        white_spider,
        red_spider,
        shadow_spider,
        leng_spider,
        leng_doomweaver,
        leng_matriarch,
        rat,
        transcendent_rat,
        rat_thing,
        pit_viper,
        spitting_cobra,
        black_mamba,
        fire_hound,
        energy_hound,
        zuul,
        ghost,
        wraith,
        mi_go,
        mi_go_commander,
        flying_polyp,
        greater_polyp,
        mind_leech,
        ghoul,
        shadow,
        invis_stalker,
        wolf,
        void_traveler,
        elder_void_traveler,
        raven,
        giant_bat,
        vampire_bat,
        abaxu,  // Unique bat
        byakhee,
        giant_mantis,
        locust,
        mummy,
        croc_head_mummy,
        khephren,
        nitokris,
        deep_one,
        niduza,
        ape,
        worm_mass,
        mind_worm,
        dust_vortex,
        fire_vortex,
        energy_vortex,
        ooze_putrid,
        ooze_lurking,
        ooze_poison,
        glasuu,
        strange_color,
        ghastly_light,
        chthonian,
        hunting_horror,
        sentry_drone,
        spectral_wpn,
        mold,
        mold_halluc,
        gas_spore,
        tentacles,
        warping_aberrance,
        death_fiend,
        khaga_offspring,
        khaga,
        shapeshifter,
        the_high_priest,
        high_priest_guard_war_vet,
        high_priest_guard_rogue,
        high_priest_guard_ghoul,

        END
};

enum class MonGroupSize
{
        alone,
        few,
        pack,
        swarm
};

// Each actor data entry has a list of this struct, this is used for choosing
// group sizes when spawning monsters. The size of the group spawned is
// determined by a weighted random choice (so that a certain monster could for
// example usually spawn alone, but on some rare occasions spawn in big groups).
struct MonGroupSpawnRule
{
        MonGroupSize group_size {MonGroupSize::alone};
        int weight {1};
        int required_dlvl {0};
};

struct ActorItemSetData
{
        item::ItemSetId item_set_id {(item::ItemSetId)0};
        int pct_chance_to_spawn {100};
        Range nr_spawned_range {1, 1};
};

struct IntrAttData
{
        IntrAttData() = default;

        ~IntrAttData() = default;

        item::Id item_id {item::Id::END};
        int dmg {0};
        ItemAttackProp prop_applied {};
};

struct ActorSpellData
{
        SpellId spell_id {SpellId::END};
        SpellSkill spell_skill {SpellSkill::basic};
        int pct_chance_to_know {100};
};

struct StartingAllyEntry
{
        Id id {(Id)0};
        Range nr {1, 1};
};

enum class Speed
{
        slow,
        normal,
        fast,
        very_fast,
};

enum class Size
{
        floor,
        humanoid,
        giant
};

enum class AiId
{
        looks,
        avoids_blocking_friend,
        attacks,
        paths_to_target_when_aware,
        moves_to_target_when_los,
        moves_to_lair,
        moves_to_leader,
        moves_randomly_when_unaware,
        END
};

struct ActorData
{
        ActorData()
        {
                reset();
        }

        void reset();

        Id id;
        std::string name_a;
        std::string name_the;
        std::string corpse_name_a;
        std::string corpse_name_the;
        gfx::TileId tile;
        char character;
        Color color;
        std::vector<MonGroupSpawnRule> group_sizes;
        int hp;
        int spi;
        std::vector<ActorItemSetData> item_sets;
        std::vector<std::shared_ptr<IntrAttData>> intr_attacks;
        std::vector<ActorSpellData> spells;
        Speed speed;
        AbilityValues ability_values;
        bool natural_props[(size_t)PropId::END];
        bool ai[(size_t)AiId::END];
        int nr_turns_aware;
        int ranged_cooldown_turns;
        int spawn_min_dlvl, spawn_max_dlvl;
        int spawn_weight;
        Size actor_size;
        bool allow_wielded_wpn_descr;
        bool allow_speed_descr;
        int nr_kills;
        bool has_player_seen;
        bool can_open_doors;
        bool can_bash_doors;
        // NOTE: Knockback may also be prevented by other soucres, e.g. if the
        // monster is ethereal
        bool prevent_knockback;
        int nr_left_allowed_to_spawn;
        bool is_unique;
        bool is_auto_spawn_allowed;
        std::string descr;
        std::string smell_msg;
        std::string wary_msg;
        std::string aware_msg_mon_seen;
        std::string aware_msg_mon_hidden;
        bool use_cultist_aware_msg_mon_seen;
        bool use_cultist_aware_msg_mon_hidden;
        audio::SfxId aware_sfx_mon_seen;
        audio::SfxId aware_sfx_mon_hidden;
        std::string spell_msg;
        std::string death_msg_override;
        int erratic_move_pct;
        MonShockLvl mon_shock_lvl;
        bool is_humanoid;
        bool is_rat;
        bool is_canine;
        bool is_spider;
        bool is_undead;
        bool is_ghost;
        bool is_ghoul;
        bool is_snake;
        bool is_reptile;
        bool is_amphibian;
        bool can_be_summoned_by_mon;
        bool can_be_shapeshifted_into;
        bool can_bleed;
        bool can_leave_corpse;
        bool prio_corpse_bash;
        std::vector<RoomType> native_rooms;
        std::vector<StartingAllyEntry> starting_allies;
};

extern ActorData g_data[(size_t)Id::END];

void init();

void save();
void load();

}  // namespace actor

#endif  // ACTOR_DATA_HPP

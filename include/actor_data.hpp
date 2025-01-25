// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef ACTOR_DATA_HPP
#define ACTOR_DATA_HPP

#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>
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
        std::string id {};
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

        std::string id;
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
        bool natural_props[(size_t)prop::Id::END];
        bool ai[(size_t)AiId::END];
        int nr_turns_aware;
        int ranged_cooldown_turns;
        bool is_pausing_on_player_seen;
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
        std::vector<room::RoomType> native_rooms;
        std::vector<StartingAllyEntry> starting_allies;
};

extern std::unordered_map<std::string, ActorData> g_data;

void init();

void save();
void load();

}  // namespace actor

#endif  // ACTOR_DATA_HPP

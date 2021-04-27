// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef ITEM_DATA_HPP
#define ITEM_DATA_HPP

#include <stddef.h>
#include <string>
#include <utility>
#include <vector>
#include <algorithm>

#include "ability_values.hpp"
#include "dmg_range.hpp"
#include "item_att_property.hpp"
#include "room.hpp"
#include "spells.hpp"
#include "terrain_data.hpp"
#include "audio_data.hpp"
#include "colors.hpp"
#include "gfx.hpp"
#include "global.hpp"
#include "random.hpp"

enum class SndVol;

namespace item
{
enum class Id
{
        trapez,

        // Melee weapons and thrown weapons
        rock,
        iron_spike,
        dagger,
        hatchet,
        club,
        hammer,
        machete,
        axe,
        spiked_mace,
        pitch_fork,
        sledge_hammer,
        thr_knife,

        // Ranged weapons, ammo
        sawed_off,
        pump_shotgun,
        machine_gun,
        incinerator,
        spike_gun,
        shotgun_shell,
        drum_of_bullets,
        incinerator_ammo,
        revolver,
        revolver_bullet,
        pistol,
        pistol_mag,
        rifle,
        rifle_bullet,
        flare_gun,
        mi_go_gun,

        // Trap weapons
        trap_dart,
        trap_dart_poison,
        trap_spear,
        trap_spear_poison,

        // Explosives
        dynamite,
        flare,
        molotov,
        smoke_grenade,

        // Player attacks
        player_kick,
        player_stomp,
        player_punch,
        player_ghoul_claw,

        // Intrinsic attacks for monsters
        // NOTE: There is a string -> id map for these entries
        intr_kick,
        intr_bite,
        intr_claw,
        intr_strike,
        intr_punch,
        intr_acid_spit,
        intr_snake_venom_spit,
        intr_fire_breath,
        intr_energy_breath,
        intr_raven_peck,
        intr_vampiric_bite,
        intr_strangle,
        intr_ghost_touch,
        intr_sting,
        intr_mind_leech_sting,
        intr_spear_thrust,
        intr_net_throw,
        intr_maul,
        intr_pus_spew,
        intr_acid_touch,
        intr_dust_engulf,
        intr_fire_engulf,
        intr_energy_engulf,
        intr_spores,
        intr_web_bola,

        // Armor
        armor_leather_jacket,
        armor_iron_suit,
        armor_flak_jacket,
        armor_asb_suit,
        armor_mi_go,

        gas_mask,

        // Scrolls
        // NOTE: There is no scroll for the identify spell - this is on purpose
        scroll_aura_of_decay,
        scroll_aza_wrath,
        scroll_bless,
        scroll_darkbolt,
        scroll_enfeeble,
        scroll_heal,
        scroll_light,
        scroll_mayhem,
        scroll_control_object,
        scroll_pest,
        scroll_premonition,
        scroll_erudition,
        scroll_res,
        scroll_see_invis,
        scroll_slow,
        scroll_haste,
        scroll_spectral_wpns,
        scroll_spell_shield,
        scroll_summon_mon,
        scroll_telep,
        scroll_terrify,
        scroll_transmut,

        // Potions
        potion_blindness,
        potion_conf,
        potion_curing,
        potion_descent,
        potion_fortitude,
        potion_insight,
        potion_invis,  // TODO: Should be called "Potion of Cloaking"
        potion_paralyze,
        potion_poison,
        potion_r_elec,
        potion_r_fire,
        potion_spirit,
        potion_vitality,

        // Strange Devices
        device_blaster,
        device_deafening,
        device_force_field,
        device_rejuvenator,
        device_sentry_drone,
        device_translocator,

        lantern,

        // Rods
        rod_curing,
        rod_opening,
        rod_bless,
        rod_cloud_minds,
        rod_shockwave,

        // Medical bag
        medical_bag,

        // Artifacts
        holy_symbol,
        clockwork,
        horn_of_banishment,
        horn_of_malice,
        orb_of_life,
        pharaoh_staff,
        refl_talisman,
        resurrect_talisman,
        spirit_dagger,
        tele_ctrl_talisman,

        // Other Curiosities
        zombie_dust,
        witches_eye,
        // flask_of_damning,
        // obsidian_charm,
        fluctuating_material,
        // bat_wing_salve,
        astral_opium,

        END
};

enum class ItemSetId
{
        minor_treasure,
        rare_treasure,
        supreme_treasure,
        firearm,
        spike_gun,
        witches_eye,
        fluctuating_material,
        zealot_spiked_mace,
        priest_dagger,
        mi_go_gun,
        mi_go_armor,
        high_priest_guard_war_vet,
        high_priest_guard_rogue,
};

enum class Value
{
        normal,
        minor_treasure,
        rare_treasure,
        supreme_treasure
};

enum Weight
{
        none = 0,
        extra_light = 1,  // E.g. ammo
        light = 10,  // E.g. dynamite, daggers
        medium = 50,  // E.g. most firearms
        heavy = 100,  // E.g. heavy armor, heavy weapons
};

struct ItemName
{
        ItemName(
                const std::string& name,
                const std::string& name_pl,
                const std::string& name_a)
        {
                names[(size_t)ItemRefType::plain] = name;
                names[(size_t)ItemRefType::plural] = name_pl;
                names[(size_t)ItemRefType::a] = name_a;
        }

        ItemName()
        {
                for (size_t i = 0; i < (size_t)ItemRefType::END; ++i)
                {
                        names[i] = "";
                }
        }

        std::string names[(size_t)ItemRefType::END];
};

struct ItemAttMsgs
{
        ItemAttMsgs() :
                player(""),
                other("") {}

        ItemAttMsgs(std::string player_, std::string other_) :
                player(std::move(player_)),
                other(std::move(other_)) {}

        std::string player, other;
};

struct ContainerSpawnRule
{
        ContainerSpawnRule(
                terrain::Id container_terrain_id,
                int pct_chance_to_incl_terrain) :
                terrain_id(container_terrain_id),
                pct_chance_to_incl(pct_chance_to_incl_terrain) {}

        terrain::Id terrain_id = terrain::Id::END;
        int pct_chance_to_incl = 0;
};

struct MeleeData
{
        MeleeData();

        bool is_melee_wpn;
        // NOTE: The "plus" field is ignored in the melee damage data,
        // melee weapons have individual plus damages per class instance
        DmgRange dmg;
        int hit_chance_mod;
        bool is_noisy;
        ItemAttMsgs att_msgs;
        ItemAttProp prop_applied;
        DmgType dmg_type;
        bool knocks_back;
        bool att_corpse;
        bool att_terrain;
        audio::SfxId hit_small_sfx;
        audio::SfxId hit_medium_sfx;
        audio::SfxId hit_hard_sfx;
        audio::SfxId miss_sfx;
};

struct RangedData
{
        RangedData();

        bool is_ranged_wpn;
        bool is_throwable_wpn;
        bool is_machine_gun;
        bool is_shotgun;
        // NOTE: This should be set on ranged weapons AND magazines
        int max_ammo;
        // NOTE: "Pure" melee weapons should not set this value - they
        // do throw damage based on their melee damage instead
        DmgRange dmg;
        int hit_chance_mod;
        int throw_hit_chance_mod;
        bool always_break_on_throw;
        Range effective_range;
        int max_range;
        bool knocks_back;
        Id ammo_item_id;
        DmgType dmg_type;
        bool has_infinite_ammo;
        char projectile_character;
        gfx::TileId projectile_tile;
        Color projectile_color;
        bool projectile_leaves_trail;
        ItemAttMsgs att_msgs;
        std::string snd_msg;
        SndVol snd_vol;
        bool makes_ricochet_snd;
        audio::SfxId att_sfx;
        audio::SfxId reload_sfx;
        ItemAttProp prop_applied;
};

struct ArmorData
{
        ArmorData();

        int armor_points;
        double dmg_to_durability_factor;
};

struct ItemData
{
public:
        ItemData();

        Id id;
        ItemType type;
        bool is_intr;
        bool has_std_activate;  // E.g. potions and scrolls
        bool is_prio_in_backpack_list;  // E.g. Medical Bag
        Value value;
        int weight;
        bool is_unique;
        bool allow_spawn;
        Range spawn_std_range;
        int max_stack_at_spawn;
        int chance_to_incl_in_spawn_list;
        bool is_stackable;
        bool is_identified;
        bool is_alignment_known;  // Used for Potions
        bool is_spell_domain_known;  // Used for Scrolls
        bool is_tried;
        bool is_found;  // Was seen on map or in inventory
        int xp_on_found;
        ItemName base_name;
        ItemName base_name_un_id;
        std::vector<std::string> base_descr;
        char character;
        Color color;
        gfx::TileId tile;
        AttMode main_att_mode;
        SpellId spell_cast_from_scroll;
        std::string land_on_hard_snd_msg;
        audio::SfxId land_on_hard_sfx;

        std::vector<RoomType> native_rooms;
        std::vector<terrain::Id> native_containers;

        int ability_mods_while_equipped[(size_t)AbilityId::END];

        bool allow_display_dmg;

        MeleeData melee;

        RangedData ranged;

        ArmorData armor;
};

extern ItemData g_data[(size_t)Id::END];

void init();
void cleanup();

void save();
void load();

ItemSetId str_to_item_set_id(const std::string& str);

Id str_to_intr_item_id(const std::string& str);

}  // namespace item

#endif  // ITEM_DATA_HPP

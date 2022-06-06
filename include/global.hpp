// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef GLOBAL_HPP
#define GLOBAL_HPP

#include <cstdint>
#include <cstdlib>

// -----------------------------------------------------------------------------
// Data
// -----------------------------------------------------------------------------
inline constexpr size_t g_player_name_max_len = 14;

// NOTE:
// Early = dlvl 1  - 9
// Mid   = dlvl 10 - 19
// Late  = dlvl 20 - 30
inline constexpr int g_dlvl_last_early_game = 9;
inline constexpr int g_dlvl_first_mid_game = g_dlvl_last_early_game + 1;
inline constexpr int g_dlvl_last_mid_game = 19;
inline constexpr int g_dlvl_first_late_game = g_dlvl_last_mid_game + 1;
inline constexpr int g_dlvl_last = 30;

inline constexpr int g_dlvl_harder_traps = 6;

inline constexpr uint32_t g_ms_delay_player_unable_act = 7;
inline constexpr uint32_t g_min_ms_between_same_sfx = 60;

inline constexpr int g_fov_radi_int = 6;
inline constexpr int g_fov_w_int = (g_fov_radi_int * 2) + 1;
inline constexpr double g_fov_radi_db = (double)g_fov_radi_int;

inline constexpr int g_dynamite_fuse_turns = 5;
inline constexpr int g_expl_std_radi = 2;

inline constexpr int g_enc_immobile_lvl = 125;

inline constexpr int g_nr_mg_projectiles = 5;

inline constexpr int g_mi_go_gun_hp_drained = 3;
inline constexpr int g_mi_go_gun_regen_disabled_min_turns = 7;
inline constexpr int g_mi_go_gun_regen_disabled_max_turns = 12;

inline constexpr int g_min_dmg_to_wound = 5;

// NOTE: Damage is reduced with higher distance from the center
inline constexpr int g_expl_dmg_min = 15;
inline constexpr int g_expl_dmg_max = 40;

inline constexpr int g_shock_from_obsession = 30;

// How many "units" of weight the player can carry, without trait modifiers etc
inline constexpr int g_player_carry_weight_base = 600;

// Value used for limiting spawning over time and "breeder" monsters. The actual
// number of actors may sometimes go a bit above this number, e.g. due to a
// group of monsters spawning when the number of actors is near the limit.
// Summoning spells does not check this number at all (because their effects
// should not be arbitrarily limited by this) - so that may also push the number
// of actors above the limit. This number is treated as a soft limit.
inline constexpr size_t g_max_nr_actors_on_map = 125;

inline constexpr int g_hit_chance_pen_vs_unseen = 25;

inline constexpr int g_player_max_clvl = 12;

// -----------------------------------------------------------------------------
// Types
// -----------------------------------------------------------------------------
// This is typically used for functions such as item identification and property
// applying to enable/disable printing to the message log, animating, or other
// such "side effects". For example when loading a saved game, we may want to do
// these things silently.
enum class Verbose
{
        no,
        yes,
};

enum class UpdateScreen
{
        no,
        yes
};

enum class InvType
{
        slots,
        backpack
};

enum class AllowAction
{
        no,
        yes
};

enum class DidAction
{
        no,
        yes
};

enum class WasDestroyed
{
        no,
        yes
};

enum class ItemPrePickResult
{
        do_pickup,
        destroy_item,
        do_nothing
};

enum class PassTime
{
        no,
        yes
};

enum class ConsumeItem
{
        no,
        yes
};

enum class ItemNameType
{
        plain,
        a,
        plural,
        END
};

enum class ItemNameInfo
{
        none,
        yes
};

enum class ItemNameAttackInfo
{
        none,
        main_attack_mode,
        melee,
        ranged,
        thrown
};

enum class ItemNameDmg
{
        average,
        range,
};

enum class AbbrevItemAttackInfo
{
        no,
        yes
};

enum class Article
{
        a,
        the
};

enum class Matl
{
        empty,
        stone,
        metal,
        plant,  // Grass, bushes, reeds, vines, fungi...
        wood,  // Trees, doors, benches...
        cloth,  // Carpet, silk (cocoons)...
        fluid
};

enum class LiquidType
{
        water,
        mud,
        magic_water
};

enum class Condition
{
        breaking,
        shoddy,
        fine
};

enum class DmgType
{
        piercing,
        slashing,
        blunt,
        kicking,
        explosion,
        shotgun,
        fire,
        acid,
        electric,
        spirit,
        light,
        pure,
        control_object_spell,
        END
};

enum class ForceInterruptActions
{
        no,
        yes
};

constexpr bool is_physical_dmg_type(const DmgType type)
{
        switch (type)
        {
        case DmgType::piercing:
                return true;

        case DmgType::slashing:
                return true;

        case DmgType::blunt:
                return true;

        case DmgType::kicking:
                return true;

        case DmgType::explosion:
                return true;

        case DmgType::shotgun:
                return true;

        case DmgType::fire:
                return false;

        case DmgType::acid:
                return false;

        case DmgType::electric:
                return false;

        case DmgType::spirit:
                return false;

        case DmgType::light:
                return false;

        case DmgType::pure:
                return false;

        case DmgType::control_object_spell:
                return false;  // Shouldn't really matter...

        case DmgType::END:
                return false;
        }

        return false;
}

enum class AttackMode
{
        none,
        melee,
        thrown,
        ranged
};

enum class AllowWound
{
        no,
        yes
};

enum class MonShockLvl
{
        none,
        unsettling,
        frightening,
        terrifying,
        mind_shattering,
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

enum class MonRoamingAllowed
{
        no,
        yes
};

enum class GameEntryMode
{
        new_game,
        load_game
};

enum class IsWin
{
        no,
        yes
};

enum class SpawnRate
{
        never,
        extremely_rare,
        very_rare,
        rare,
        common,
        very_common
};

enum class VerDir
{
        up,
        down
};

enum class ActorState
{
        alive,
        corpse,
        destroyed
};

enum class ShouldCtrlTele
{
        if_tele_ctrl_prop,
        never,
        always
};

enum class Axis
{
        hor,
        ver
};

enum class IsSubRoom
{
        no,
        yes
};

// NOTE: Order matters here, higher value --> bigger area.
enum class LightSize
{
        none,
        single,  // Single position
        small,  // Including adjacent positions, i.e. 3x3
        fov
};

enum class ItemType
{
        general,
        melee_wpn,
        ranged_wpn,
        throwing_wpn,
        ammo,
        ammo_mag,
        scroll,
        potion,
        device,
        rod,
        armor,
        head_wear,
        explosive,

        END_OF_EXTRINSIC_ITEMS,
        melee_wpn_intr,
        ranged_wpn_intr
};

#endif  // GLOBAL_HPP

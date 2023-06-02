// =============================================================================
// Copyright 2011-2023 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef AUDIO_DATA_HPP
#define AUDIO_DATA_HPP

#include <string>

namespace audio
{
enum class MusId
{
        cthulhiana_madness,
        END
};

// NOTE: When updating this, also update the translation tables in the cpp file.
enum class SfxId
{
        // Monster sounds
        dog_snarl,
        hiss,
        zombie_growl,
        ghoul_growl,
        ooze_gurgle,
        flapping_wings,
        ape,

        // Weapon and attack sounds
        hit_small,
        hit_medium,
        hit_hard,
        hit_sharp,
        hit_whip_scourge,
        hit_corpse_break,
        miss_light,
        miss_medium,
        miss_heavy,
        pistol_fire,
        pistol_reload,
        revolver_fire,
        revolver_spin,
        rifle_fire,
        rifle_revolver_reload,
        shotgun_sawed_off_fire,
        shotgun_pump_fire,
        shotgun_reload,
        machine_gun_fire,
        machine_gun_reload,
        electric_gun,
        morphic_blaster,
        spike_gun,
        bite,

        // Spell sounds
        darkbolt_impact,
        darkbolt_release,
        aza_gaze,

        // Artifact sounds
        horn,

        // Environment action sounds
        metal_clank,
        ricochet,
        explosion,
        explosion_molotov,
        gas,
        door_bang,
        door_bang_gate,
        door_bang_metal,
        door_break,
        door_break_gate,
        door_close,
        door_close_gate,
        door_close_metal,
        door_open,
        door_open_gate,
        door_open_metal,
        tomb_open,
        fountain_drink,
        boss_voice1,
        boss_voice2,
        chains,
        crystal_key_disable,
        mirror_activate,
        statue_crash,
        lever_pull,
        monolith,
        thunder,
        gong,
        magic_trap_trigger,
        mechanical_trap_trigger,
        wade,
        swim,
        alchemy_rummage,
        bookshelf_rummage,
        cabinet_open,

        // User interface sounds
        backpack,
        pickup,
        electric_lantern,
        potion_quaff,
        strange_device_activate,
        strange_device_damaged,
        spell_shield_break,
        insanity_rising,
        death,
        menu_browse,
        menu_select,

        // Ambient sounds
        AMB_START,

        amb_001,
        amb_002,
        amb_003,
        amb_004,
        amb_005,
        amb_006,
        amb_007,
        amb_008,
        amb_009,
        amb_010,
        amb_011,
        amb_012,
        amb_013,
        amb_014,
        amb_015,
        amb_016,
        amb_017,
        amb_018,
        amb_019,
        amb_020,
        amb_021,
        amb_022,
        amb_023,
        amb_024,
        amb_025,
        amb_026,
        amb_027,
        amb_028,
        amb_029,
        amb_030,
        amb_031,
        amb_032,
        // (033 unused)
        // (034 unused)
        amb_035,
        amb_036,
        amb_037,
        amb_038,
        amb_039,
        amb_040,
        amb_041,
        amb_042,
        amb_043,
        amb_044,
        amb_045,
        amb_046,
        amb_047,
        amb_048,
        amb_049,
        amb_050,
        amb_051,
        amb_052,
        amb_053,
        amb_054,
        amb_055,

        END
};

SfxId str_to_sfx_id(const std::string& str);

std::string sfx_id_to_filename(SfxId id);

}  // namespace audio

#endif  // AUDIO_DATA_HPP

// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "audio_data.hpp"

#include <iterator>
#include <unordered_map>
#include <utility>

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
using StrToSfxIdMap = std::unordered_map<std::string, audio::SfxId>;

static const StrToSfxIdMap s_str_to_sfx_id_map = {
        // Sound effects
        {"dog_snarl", audio::SfxId::dog_snarl},
        {"hiss", audio::SfxId::hiss},
        {"zombie_growl", audio::SfxId::zombie_growl},
        {"ghoul_growl", audio::SfxId::ghoul_growl},
        {"ooze_gurgle", audio::SfxId::ooze_gurgle},
        {"flapping_wings", audio::SfxId::flapping_wings},
        {"ape", audio::SfxId::ape},
        {"hit_small", audio::SfxId::hit_small},
        {"hit_medium", audio::SfxId::hit_medium},
        {"hit_hard", audio::SfxId::hit_hard},
        {"hit_sharp", audio::SfxId::hit_sharp},
        {"hit_corpse_break", audio::SfxId::hit_corpse_break},
        {"miss_light", audio::SfxId::miss_light},
        {"miss_medium", audio::SfxId::miss_medium},
        {"miss_heavy", audio::SfxId::miss_heavy},
        {"pistol_fire", audio::SfxId::pistol_fire},
        {"pistol_reload", audio::SfxId::pistol_reload},
        {"revolver_fire", audio::SfxId::revolver_fire},
        {"revolver_spin", audio::SfxId::revolver_spin},
        {"rifle_fire", audio::SfxId::rifle_fire},
        {"rifle_revolver_reload", audio::SfxId::rifle_revolver_reload},
        {"shotgun_sawed_off_fire", audio::SfxId::shotgun_sawed_off_fire},
        {"shotgun_pump_fire", audio::SfxId::shotgun_pump_fire},
        {"shotgun_reload", audio::SfxId::shotgun_reload},
        {"machine_gun_fire", audio::SfxId::machine_gun_fire},
        {"machine_gun_reload", audio::SfxId::machine_gun_reload},
        {"migo_gun", audio::SfxId::migo_gun},
        {"spike_gun", audio::SfxId::spike_gun},
        {"bite", audio::SfxId::bite},
        {"metal_clank", audio::SfxId::metal_clank},
        {"ricochet", audio::SfxId::ricochet},
        {"explosion", audio::SfxId::explosion},
        {"explosion_molotov", audio::SfxId::explosion_molotov},
        {"gas", audio::SfxId::gas},
        {"darkbolt_impact", audio::SfxId::darkbolt_impact},
        {"darkbolt_release", audio::SfxId::darkbolt_release},
        {"aza_gaze", audio::SfxId::aza_gaze},
        {"door_open", audio::SfxId::door_open},
        {"door_close", audio::SfxId::door_close},
        {"door_bang", audio::SfxId::door_bang},
        {"door_break", audio::SfxId::door_break},
        {"door_open_metallic", audio::SfxId::door_open_metallic},
        {"door_close_metallic", audio::SfxId::door_close_metallic},
        {"door_bang_metallic", audio::SfxId::door_bang_metallic},
        {"door_break_metallic", audio::SfxId::door_break_metallic},
        {"tomb_open", audio::SfxId::tomb_open},
        {"fountain_drink", audio::SfxId::fountain_drink},
        {"boss_voice1", audio::SfxId::boss_voice1},
        {"boss_voice2", audio::SfxId::boss_voice2},
        {"chains", audio::SfxId::chains},
        {"statue_crash", audio::SfxId::statue_crash},
        {"lever_pull", audio::SfxId::lever_pull},
        {"monolith", audio::SfxId::monolith},
        {"thunder", audio::SfxId::thunder},
        {"gong", audio::SfxId::gong},
        {"mechanical_trap_trigger", audio::SfxId::mechanical_trap_trigger},
        {"wade", audio::SfxId::wade},
        {"swim", audio::SfxId::swim},
        {"backpack", audio::SfxId::backpack},
        {"pickup", audio::SfxId::pickup},
        {"electric_lantern", audio::SfxId::electric_lantern},
        {"potion_quaff", audio::SfxId::potion_quaff},
        {"strange_device_activate", audio::SfxId::strange_device_activate},
        {"strange_device_damaged", audio::SfxId::strange_device_damaged},
        {"spell_shield_break", audio::SfxId::spell_shield_break},
        {"insanity_rising", audio::SfxId::insanity_rising},
        {"death", audio::SfxId::death},
        {"menu_browse", audio::SfxId::menu_browse},
        {"menu_select", audio::SfxId::menu_select},

        // Ambient sounds
        {"amb_001", audio::SfxId::amb_001},
        {"amb_002", audio::SfxId::amb_002},
        {"amb_003", audio::SfxId::amb_003},
        {"amb_004", audio::SfxId::amb_004},
        {"amb_005", audio::SfxId::amb_005},
        {"amb_006", audio::SfxId::amb_006},
        {"amb_007", audio::SfxId::amb_007},
        {"amb_008", audio::SfxId::amb_008},
        {"amb_009", audio::SfxId::amb_009},
        {"amb_010", audio::SfxId::amb_010},
        {"amb_011", audio::SfxId::amb_011},
        {"amb_012", audio::SfxId::amb_012},
        {"amb_013", audio::SfxId::amb_013},
        {"amb_014", audio::SfxId::amb_014},
        {"amb_015", audio::SfxId::amb_015},
        {"amb_016", audio::SfxId::amb_016},
        {"amb_017", audio::SfxId::amb_017},
        {"amb_018", audio::SfxId::amb_018},
        {"amb_019", audio::SfxId::amb_019},
        {"amb_020", audio::SfxId::amb_020},
        {"amb_021", audio::SfxId::amb_021},
        {"amb_022", audio::SfxId::amb_022},
        {"amb_023", audio::SfxId::amb_023},
        {"amb_024", audio::SfxId::amb_024},
        {"amb_025", audio::SfxId::amb_025},
        {"amb_026", audio::SfxId::amb_026},
        {"amb_027", audio::SfxId::amb_027},
        {"amb_028", audio::SfxId::amb_028},
        {"amb_029", audio::SfxId::amb_029},
        {"amb_030", audio::SfxId::amb_030},
        {"amb_031", audio::SfxId::amb_031},
        {"amb_032", audio::SfxId::amb_032},
        {"amb_035", audio::SfxId::amb_035},
        {"amb_036", audio::SfxId::amb_036},
        {"amb_037", audio::SfxId::amb_037},
        {"amb_038", audio::SfxId::amb_038},
        {"amb_039", audio::SfxId::amb_039},
        {"amb_040", audio::SfxId::amb_040},
        {"amb_041", audio::SfxId::amb_041},
        {"amb_042", audio::SfxId::amb_042},
        {"amb_043", audio::SfxId::amb_043},
        {"amb_044", audio::SfxId::amb_044},
        {"amb_045", audio::SfxId::amb_045},
        {"amb_046", audio::SfxId::amb_046},
        {"amb_047", audio::SfxId::amb_047},
        {"amb_048", audio::SfxId::amb_048},
        {"amb_049", audio::SfxId::amb_049},
        {"amb_050", audio::SfxId::amb_050},
        {"amb_051", audio::SfxId::amb_051},
        {"amb_052", audio::SfxId::amb_052},
        {"amb_053", audio::SfxId::amb_053},
        {"amb_054", audio::SfxId::amb_054},
        {"amb_055", audio::SfxId::amb_055},
};

using SfxIdToStrMap = std::unordered_map<audio::SfxId, std::string>;

static const SfxIdToStrMap s_sfx_id_to_str_map = {
        // Sound effects
        {audio::SfxId::dog_snarl, "dog_snarl"},
        {audio::SfxId::hiss, "hiss"},
        {audio::SfxId::zombie_growl, "zombie_growl"},
        {audio::SfxId::ghoul_growl, "ghoul_growl"},
        {audio::SfxId::ooze_gurgle, "ooze_gurgle"},
        {audio::SfxId::flapping_wings, "flapping_wings"},
        {audio::SfxId::ape, "ape"},
        {audio::SfxId::hit_small, "hit_small"},
        {audio::SfxId::hit_medium, "hit_medium"},
        {audio::SfxId::hit_hard, "hit_hard"},
        {audio::SfxId::hit_sharp, "hit_sharp"},
        {audio::SfxId::hit_corpse_break, "hit_corpse_break"},
        {audio::SfxId::miss_light, "miss_light"},
        {audio::SfxId::miss_medium, "miss_medium"},
        {audio::SfxId::miss_heavy, "miss_heavy"},
        {audio::SfxId::pistol_fire, "pistol_fire"},
        {audio::SfxId::pistol_reload, "pistol_reload"},
        {audio::SfxId::revolver_fire, "revolver_fire"},
        {audio::SfxId::revolver_spin, "revolver_spin"},
        {audio::SfxId::rifle_fire, "rifle_fire"},
        {audio::SfxId::rifle_revolver_reload, "rifle_revolver_reload"},
        {audio::SfxId::shotgun_sawed_off_fire, "shotgun_sawed_off_fire"},
        {audio::SfxId::shotgun_pump_fire, "shotgun_pump_fire"},
        {audio::SfxId::shotgun_reload, "shotgun_reload"},
        {audio::SfxId::machine_gun_fire, "machine_gun_fire"},
        {audio::SfxId::machine_gun_reload, "machine_gun_reload"},
        {audio::SfxId::migo_gun, "migo_gun"},
        {audio::SfxId::spike_gun, "spike_gun"},
        {audio::SfxId::bite, "bite"},
        {audio::SfxId::metal_clank, "metal_clank"},
        {audio::SfxId::ricochet, "ricochet"},
        {audio::SfxId::explosion, "explosion"},
        {audio::SfxId::explosion_molotov, "explosion_molotov"},
        {audio::SfxId::gas, "gas"},
        {audio::SfxId::darkbolt_impact, "darkbolt_impact"},
        {audio::SfxId::darkbolt_release, "darkbolt_release"},
        {audio::SfxId::aza_gaze, "aza_gaze"},
        {audio::SfxId::door_open, "door_open"},
        {audio::SfxId::door_close, "door_close"},
        {audio::SfxId::door_bang, "door_bang"},
        {audio::SfxId::door_break, "door_break"},
        {audio::SfxId::door_open_metallic, "door_open_metallic"},
        {audio::SfxId::door_close_metallic, "door_close_metallic"},
        {audio::SfxId::door_bang_metallic, "door_bang_metallic"},
        {audio::SfxId::door_break_metallic, "door_break_metallic"},
        {audio::SfxId::tomb_open, "tomb_open"},
        {audio::SfxId::fountain_drink, "fountain_drink"},
        {audio::SfxId::boss_voice1, "boss_voice1"},
        {audio::SfxId::boss_voice2, "boss_voice2"},
        {audio::SfxId::chains, "chains"},
        {audio::SfxId::statue_crash, "statue_crash"},
        {audio::SfxId::lever_pull, "lever_pull"},
        {audio::SfxId::monolith, "monolith"},
        {audio::SfxId::thunder, "thunder"},
        {audio::SfxId::gong, "gong"},
        {audio::SfxId::mechanical_trap_trigger, "mechanical_trap_trigger"},
        {audio::SfxId::wade, "wade"},
        {audio::SfxId::swim, "swim"},
        {audio::SfxId::backpack, "backpack"},
        {audio::SfxId::pickup, "pickup"},
        {audio::SfxId::electric_lantern, "electric_lantern"},
        {audio::SfxId::potion_quaff, "potion_quaff"},
        {audio::SfxId::strange_device_activate, "strange_device_activate"},
        {audio::SfxId::strange_device_damaged, "strange_device_damaged"},
        {audio::SfxId::spell_shield_break, "spell_shield_break"},
        {audio::SfxId::insanity_rising, "insanity_rising"},
        {audio::SfxId::death, "death"},
        {audio::SfxId::menu_browse, "menu_browse"},
        {audio::SfxId::menu_select, "menu_select"},

        {audio::SfxId::AMB_START, ""},

        // Ambient sounds
        {audio::SfxId::amb_001, "amb_001"},
        {audio::SfxId::amb_002, "amb_002"},
        {audio::SfxId::amb_003, "amb_003"},
        {audio::SfxId::amb_004, "amb_004"},
        {audio::SfxId::amb_005, "amb_005"},
        {audio::SfxId::amb_006, "amb_006"},
        {audio::SfxId::amb_007, "amb_007"},
        {audio::SfxId::amb_008, "amb_008"},
        {audio::SfxId::amb_009, "amb_009"},
        {audio::SfxId::amb_010, "amb_010"},
        {audio::SfxId::amb_011, "amb_011"},
        {audio::SfxId::amb_012, "amb_012"},
        {audio::SfxId::amb_013, "amb_013"},
        {audio::SfxId::amb_014, "amb_014"},
        {audio::SfxId::amb_015, "amb_015"},
        {audio::SfxId::amb_016, "amb_016"},
        {audio::SfxId::amb_017, "amb_017"},
        {audio::SfxId::amb_018, "amb_018"},
        {audio::SfxId::amb_019, "amb_019"},
        {audio::SfxId::amb_020, "amb_020"},
        {audio::SfxId::amb_021, "amb_021"},
        {audio::SfxId::amb_022, "amb_022"},
        {audio::SfxId::amb_023, "amb_023"},
        {audio::SfxId::amb_024, "amb_024"},
        {audio::SfxId::amb_025, "amb_025"},
        {audio::SfxId::amb_026, "amb_026"},
        {audio::SfxId::amb_027, "amb_027"},
        {audio::SfxId::amb_028, "amb_028"},
        {audio::SfxId::amb_029, "amb_029"},
        {audio::SfxId::amb_030, "amb_030"},
        {audio::SfxId::amb_031, "amb_031"},
        {audio::SfxId::amb_032, "amb_032"},
        {audio::SfxId::amb_035, "amb_035"},
        {audio::SfxId::amb_036, "amb_036"},
        {audio::SfxId::amb_037, "amb_037"},
        {audio::SfxId::amb_038, "amb_038"},
        {audio::SfxId::amb_039, "amb_039"},
        {audio::SfxId::amb_040, "amb_040"},
        {audio::SfxId::amb_041, "amb_041"},
        {audio::SfxId::amb_042, "amb_042"},
        {audio::SfxId::amb_043, "amb_043"},
        {audio::SfxId::amb_044, "amb_044"},
        {audio::SfxId::amb_045, "amb_045"},
        {audio::SfxId::amb_046, "amb_046"},
        {audio::SfxId::amb_047, "amb_047"},
        {audio::SfxId::amb_048, "amb_048"},
        {audio::SfxId::amb_049, "amb_049"},
        {audio::SfxId::amb_050, "amb_050"},
        {audio::SfxId::amb_051, "amb_051"},
        {audio::SfxId::amb_052, "amb_052"},
        {audio::SfxId::amb_053, "amb_053"},
        {audio::SfxId::amb_054, "amb_054"},
        {audio::SfxId::amb_055, "amb_055"},
};

// -----------------------------------------------------------------------------
// audio
// -----------------------------------------------------------------------------
namespace audio
{
SfxId str_to_sfx_id(const std::string& str)
{
        const auto result = s_str_to_sfx_id_map.find(str);

        if (result == std::end(s_str_to_sfx_id_map)) {
                return SfxId::END;
        }
        else {
                return result->second;
        }
}

std::string sfx_id_to_str(SfxId id)
{
        const auto result = s_sfx_id_to_str_map.find(id);

        if (result == std::end(s_sfx_id_to_str_map)) {
                return {};
        }
        else {
                return result->second;
        }
}

}  // namespace audio

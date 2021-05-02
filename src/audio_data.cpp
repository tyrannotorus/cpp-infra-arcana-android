// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "audio_data.hpp"

#include <unordered_map>

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
using StrToSfxIdMap = std::unordered_map<std::string, audio::SfxId>;

static const StrToSfxIdMap s_str_to_sfx_id_map = {
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
        {"shotgun_sawed_off_fire", audio::SfxId::shotgun_sawed_off_fire},
        {"shotgun_pump_fire", audio::SfxId::shotgun_pump_fire},
        {"shotgun_reload", audio::SfxId::shotgun_reload},
        {"machine_gun_fire", audio::SfxId::machine_gun_fire},
        {"machine_gun_reload", audio::SfxId::machine_gun_reload},
        {"mi_go_gun_fire", audio::SfxId::mi_go_gun_fire},
        {"spike_gun", audio::SfxId::spike_gun},
        {"bite", audio::SfxId::bite},
        {"metal_clank", audio::SfxId::metal_clank},
        {"ricochet", audio::SfxId::ricochet},
        {"explosion", audio::SfxId::explosion},
        {"explosion_molotov", audio::SfxId::explosion_molotov},
        {"gas", audio::SfxId::gas},
        {"door_open", audio::SfxId::door_open},
        {"door_close", audio::SfxId::door_close},
        {"door_bang", audio::SfxId::door_bang},
        {"door_break", audio::SfxId::door_break},
        {"tomb_open", audio::SfxId::tomb_open},
        {"fountain_drink", audio::SfxId::fountain_drink},
        {"boss_voice1", audio::SfxId::boss_voice1},
        {"boss_voice2", audio::SfxId::boss_voice2},
        {"chains", audio::SfxId::chains},
        {"statue_crash", audio::SfxId::statue_crash},
        {"lever_pull", audio::SfxId::lever_pull},
        {"monolith", audio::SfxId::monolith},
        {"wade", audio::SfxId::wade},
        {"swim", audio::SfxId::swim},
        {"backpack", audio::SfxId::backpack},
        {"pickup", audio::SfxId::pickup},
        {"lantern", audio::SfxId::lantern},
        {"potion_quaff", audio::SfxId::potion_quaff},
        {"strange_device_activate", audio::SfxId::strange_device_activate},
        {"strange_device_damaged", audio::SfxId::strange_device_damaged},
        {"spell_generic", audio::SfxId::spell_generic},
        {"spell_shield_break", audio::SfxId::spell_shield_break},
        {"insanity_rise", audio::SfxId::insanity_rise},
        {"death", audio::SfxId::death},
        {"menu_browse", audio::SfxId::menu_browse},
        {"menu_select", audio::SfxId::menu_select},
        {"", audio::SfxId::END}};

using SfxIdToStrMap = std::unordered_map<audio::SfxId, std::string>;

static const SfxIdToStrMap s_sfx_id_to_str_map = {
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
        {audio::SfxId::shotgun_sawed_off_fire, "shotgun_sawed_off_fire"},
        {audio::SfxId::shotgun_pump_fire, "shotgun_pump_fire"},
        {audio::SfxId::shotgun_reload, "shotgun_reload"},
        {audio::SfxId::machine_gun_fire, "machine_gun_fire"},
        {audio::SfxId::machine_gun_reload, "machine_gun_reload"},
        {audio::SfxId::mi_go_gun_fire, "mi_go_gun_fire"},
        {audio::SfxId::spike_gun, "spike_gun"},
        {audio::SfxId::bite, "bite"},
        {audio::SfxId::metal_clank, "metal_clank"},
        {audio::SfxId::ricochet, "ricochet"},
        {audio::SfxId::explosion, "explosion"},
        {audio::SfxId::explosion_molotov, "explosion_molotov"},
        {audio::SfxId::gas, "gas"},
        {audio::SfxId::door_open, "door_open"},
        {audio::SfxId::door_close, "door_close"},
        {audio::SfxId::door_bang, "door_bang"},
        {audio::SfxId::door_break, "door_break"},
        {audio::SfxId::tomb_open, "tomb_open"},
        {audio::SfxId::fountain_drink, "fountain_drink"},
        {audio::SfxId::boss_voice1, "boss_voice1"},
        {audio::SfxId::boss_voice2, "boss_voice2"},
        {audio::SfxId::chains, "chains"},
        {audio::SfxId::statue_crash, "statue_crash"},
        {audio::SfxId::lever_pull, "lever_pull"},
        {audio::SfxId::monolith, "monolith"},
        {audio::SfxId::wade, "wade"},
        {audio::SfxId::swim, "swim"},
        {audio::SfxId::backpack, "backpack"},
        {audio::SfxId::pickup, "pickup"},
        {audio::SfxId::lantern, "lantern"},
        {audio::SfxId::potion_quaff, "potion_quaff"},
        {audio::SfxId::strange_device_activate, "strange_device_activate"},
        {audio::SfxId::strange_device_damaged, "strange_device_damaged"},
        {audio::SfxId::spell_generic, "spell_generic"},
        {audio::SfxId::spell_shield_break, "spell_shield_break"},
        {audio::SfxId::insanity_rise, "insanity_rise"},
        {audio::SfxId::death, "death"},
        {audio::SfxId::menu_browse, "menu_browse"},
        {audio::SfxId::menu_select, "menu_select"},
        {audio::SfxId::END, ""},
};

// -----------------------------------------------------------------------------
// audio
// -----------------------------------------------------------------------------
namespace audio
{
SfxId str_to_sfx_id(const std::string& str)
{
        return s_str_to_sfx_id_map.at(str);
}

std::string sfx_id_to_str(SfxId id)
{
        return s_sfx_id_to_str_map.at(id);
}

}  // namespace audio

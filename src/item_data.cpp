// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "item_data.hpp"

#include <memory>
#include <unordered_map>

#include "colors.hpp"
#include "debug.hpp"
#include "item_att_property.hpp"
#include "property.hpp"
#include "property_data.hpp"
#include "property_factory.hpp"
#include "random.hpp"
#include "saving.hpp"
#include "sound.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
using StrToIdMap = std::unordered_map<std::string, item::Id>;

static const StrToIdMap s_str_to_intr_item_id_map = {
        {"ITEMINTR_BITE", item::Id::intr_bite},
        {"ITEMINTR_CLAW", item::Id::intr_claw},
        {"ITEMINTR_DUST_ENGULF", item::Id::intr_dust_engulf},
        {"ITEMINTR_EARTH_BREATH", item::Id::intr_earth_breath},
        {"ITEMINTR_ENERGY_ENGULF", item::Id::intr_energy_engulf},
        {"ITEMINTR_FIRE_BREATH", item::Id::intr_fire_breath},
        {"ITEMINTR_FIRE_ENGULF", item::Id::intr_fire_engulf},
        {"ITEMINTR_GHOST_TOUCH", item::Id::intr_ghost_touch},
        {"ITEMINTR_HEADBUTT", item::Id::intr_headbutt},
        {"ITEMINTR_KICK", item::Id::intr_kick},
        {"ITEMINTR_LIGHTNING_BREATH", item::Id::intr_lightning_breath},
        {"ITEMINTR_MAUL", item::Id::intr_maul},
        {"ITEMINTR_MIND_LEECH_STING", item::Id::intr_mind_leech_sting},
        {"ITEMINTR_NET_THROW", item::Id::intr_net_throw},
        {"ITEMINTR_PUNCH", item::Id::intr_punch},
        {"ITEMINTR_PUNCH_KNOCKBACK", item::Id::intr_punch_knockback},
        {"ITEMINTR_PUS_SPEW", item::Id::intr_pus_spew},
        {"ITEMINTR_PUTRID_SPIT", item::Id::intr_putrid_spit},
        {"ITEMINTR_RAVEN_PECK", item::Id::intr_raven_peck},
        {"ITEMINTR_SNAKE_VENOM_SPIT", item::Id::intr_snake_venom_spit},
        {"ITEMINTR_SPEAR_THRUST", item::Id::intr_spear_thrust},
        {"ITEMINTR_SPORES", item::Id::intr_spores},
        {"ITEMINTR_STING", item::Id::intr_sting},
        {"ITEMINTR_STRANGE_COLOR_TOUCH", item::Id::intr_strange_color_touch},
        {"ITEMINTR_STRANGLE", item::Id::intr_strangle},
        {"ITEMINTR_STRIKE", item::Id::intr_strike},
        {"ITEMINTR_VAMPIRIC_BITE", item::Id::intr_vampiric_bite},
        {"ITEMINTR_WATER_BREATH", item::Id::intr_water_breath},
        {"ITEMINTR_WEB_BOLA", item::Id::intr_web_bola},
};

using StrToItemSetIdMap = std::unordered_map<std::string, item::ItemSetId>;

static const StrToItemSetIdMap s_str_to_item_set_id_map = {
        {"ITEMSET_MINOR_TREASURE", item::ItemSetId::minor_treasure},
        {"ITEMSET_MAJOR_TREASURE", item::ItemSetId::major_treasure},
        {"ITEMSET_SUPREME_TREASURE", item::ItemSetId::supreme_treasure},
        {"ITEMSET_FIREARM", item::ItemSetId::firearm},
        {"ITEMSET_SPIKE_GUN", item::ItemSetId::spike_gun},
        {"ITEMSET_WITCH_EYE", item::ItemSetId::witch_eye},
        {"ITEMSET_FLUCTUATING_MATERIAL", item::ItemSetId::fluctuating_material},
        {"ITEMSET_ZEALOT_SPIKED_MACE", item::ItemSetId::zealot_spiked_mace},
        {"ITEMSET_PRIEST_DAGGER", item::ItemSetId::priest_dagger},
        {"ITEMSET_ELECTRIC_GUN", item::ItemSetId::electric_gun},
        {"ITEMSET_MORPHIC_BLASTER", item::ItemSetId::morphic_blaster},
        {"ITEMSET_MI_GO_ARMOR", item::ItemSetId::mi_go_armor},
        {"ITEMSET_HIGH_PRIEST_GUARD_WAR_VET", item::ItemSetId::high_priest_guard_war_vet},
        {"ITEMSET_HIGH_PRIEST_GUARD_ROGUE", item::ItemSetId::high_priest_guard_rogue}};

static const std::string s_electric_gun_hp_drained_str =
        std::to_string(g_electric_gun_hp_drained);

static const std::string s_electric_gun_hp_disable_range_str =
        Range(
                g_electric_gun_regen_disabled_min_turns,
                g_electric_gun_regen_disabled_max_turns)
                .str();

static const std::string s_morphic_blaster_hp_drained_str =
        std::to_string(g_morphic_blaster_hp_drained);

static const std::string s_morphic_blaster_hp_disable_range_str =
        Range(
                g_morphic_blaster_regen_disabled_min_turns,
                g_morphic_blaster_regen_disabled_max_turns)
                .str();

static void mod_spawn_chance(item::ItemData& data, const double factor)
{
        data.chance_to_incl_in_spawn_list =
                (int)((double)data.chance_to_incl_in_spawn_list * factor);
}

// Item archetypes (defaults)
static void reset_data(item::ItemData& d, ItemType const item_type)
{
        switch (item_type) {
        case ItemType::general:
                d = {};
                break;

        case ItemType::melee_wpn:
                reset_data(d, ItemType::general);
                d.type = ItemType::melee_wpn;
                d.is_stackable = false;
                d.weight = item::Weight::medium;
                d.character = ')';
                d.color = colors::white();
                d.main_attack_mode = AttackMode::melee;
                d.melee.hit_chance_mod = 0;
                d.melee.is_melee_wpn = true;
                d.melee.miss_sfx = audio::SfxId::miss_medium;
                d.melee.hit_small_sfx = audio::SfxId::hit_small;
                d.melee.hit_medium_sfx = audio::SfxId::hit_medium;
                d.melee.hit_hard_sfx = audio::SfxId::hit_hard;
                d.ranged.is_throwable_wpn = true;
                d.ranged.throw_hit_chance_mod = -25;
                d.ranged.effective_range = {0, 3};
                d.ranged.max_range = d.ranged.effective_range.max + 3;
                d.land_on_hard_snd_msg = "I hear a clanking sound.";
                d.land_on_hard_sfx = audio::SfxId::metal_clank;
                break;

        case ItemType::melee_wpn_intr:
                reset_data(d, ItemType::melee_wpn);
                d.type = ItemType::melee_wpn_intr;
                d.is_intr = true;
                d.spawn_std_range = Range(-1, -1);
                d.chance_to_incl_in_spawn_list = 0;
                d.allow_spawn = false;
                d.melee.hit_small_sfx = audio::SfxId::hit_small;
                d.melee.hit_medium_sfx = audio::SfxId::hit_medium;
                d.melee.hit_hard_sfx = audio::SfxId::hit_hard;
                d.melee.miss_sfx = audio::SfxId::END;
                d.ranged.is_throwable_wpn = false;
                break;

        case ItemType::ranged_wpn:
                reset_data(d, ItemType::general);
                d.type = ItemType::ranged_wpn;
                d.is_stackable = false;
                d.weight = item::Weight::medium;
                d.character = '}';
                d.color = colors::white();
                d.melee.is_melee_wpn = true;
                d.melee.dmg = WpnDmg(1, 3);
                d.melee.dmg_type = DmgType::blunt;
                d.main_attack_mode = AttackMode::ranged;
                d.ranged.is_ranged_wpn = true;
                d.ranged.projectile_character = '/';
                d.ranged.projectile_color = colors::white();
                d.spawn_std_range.max = g_dlvl_last_mid_game;
                d.melee.hit_small_sfx = audio::SfxId::hit_small;
                d.melee.hit_medium_sfx = audio::SfxId::hit_medium;
                d.melee.hit_hard_sfx = audio::SfxId::hit_hard;
                d.melee.miss_sfx = audio::SfxId::miss_medium;
                d.ranged.snd_vol = SndVol::high;
                break;

        case ItemType::ranged_wpn_intr:
                reset_data(d, ItemType::ranged_wpn);
                d.type = ItemType::ranged_wpn_intr;
                d.is_intr = true;
                d.ranged.has_infinite_ammo = true;
                d.spawn_std_range = Range(-1, -1);
                d.chance_to_incl_in_spawn_list = 0;
                d.allow_spawn = false;
                d.melee.is_melee_wpn = false;
                d.ranged.projectile_character = '*';
                d.ranged.snd_vol = SndVol::low;
                break;

        case ItemType::throwing_wpn:
                reset_data(d, ItemType::general);
                d.type = ItemType::throwing_wpn;
                d.weight = item::Weight::extra_light;
                d.is_stackable = true;
                d.spawn_std_range.max = g_dlvl_last_mid_game;
                d.ranged.snd_vol = SndVol::low;
                d.ranged.is_throwable_wpn = true;
                break;

        case ItemType::ammo:
                reset_data(d, ItemType::general);
                d.type = ItemType::ammo;
                d.weight = item::Weight::extra_light;
                d.character = '{';
                d.color = colors::white();
                d.tile = gfx::TileId::ammo;
                d.spawn_std_range.max = g_dlvl_last_mid_game;
                break;

        case ItemType::ammo_mag:
                reset_data(d, ItemType::ammo);
                d.type = ItemType::ammo_mag;
                d.weight = item::Weight::light;
                d.is_stackable = false;
                d.spawn_std_range.max = g_dlvl_last_mid_game;
                break;

        case ItemType::scroll:
                // NOTE: Scroll spawning chances are set elsewhere
                reset_data(d, ItemType::general);
                d.type = ItemType::scroll;
                d.has_std_activate = true;
                d.base_descr = {
                        "A short transcription of an eldritch incantation. "
                        "There is a strange aura about it, as if some power "
                        "was imbued in the paper itself.",
                        "It should be possible to pronounce it correctly, but "
                        "the purpose is unclear."};
                d.value = item::Value::minor_treasure;
                d.weight = item::Weight::none;
                d.is_identified = false;
                d.is_spell_domain_known = false;
                d.xp_on_found = 8;
                d.character = '?';
                d.color = colors::white();
                d.tile = gfx::TileId::scroll;
                d.max_stack_at_spawn = 1;
                d.land_on_hard_snd_msg = "";
                d.native_containers.push_back(terrain::Id::chest);
                d.native_containers.push_back(terrain::Id::tomb);
                d.native_containers.push_back(terrain::Id::cabinet);
                d.native_containers.push_back(terrain::Id::bookshelf);
                d.native_containers.push_back(terrain::Id::cocoon);
                break;

        case ItemType::potion:
                reset_data(d, ItemType::general);
                d.type = ItemType::potion;
                d.has_std_activate = true;
                d.base_descr = {
                        "A small glass bottle containing a mysterious "
                        "concoction."};
                d.value = item::Value::minor_treasure;
                d.chance_to_incl_in_spawn_list = 60;
                d.weight = item::Weight::light;
                d.is_identified = false;
                d.is_alignment_known = false;
                d.xp_on_found = 8;
                d.character = '!';
                d.tile = gfx::TileId::potion;
                d.ranged.throw_hit_chance_mod = 15;
                d.ranged.dmg = WpnDmg(1, 3);
                d.ranged.dmg_type = DmgType::blunt;
                d.ranged.always_break_on_throw = true;
                d.max_stack_at_spawn = 1;
                d.land_on_hard_snd_msg = "";
                d.ranged.is_throwable_wpn = true;
                d.native_containers.push_back(terrain::Id::chest);
                d.native_containers.push_back(terrain::Id::tomb);
                d.native_containers.push_back(terrain::Id::cabinet);
                d.native_containers.push_back(terrain::Id::alchemist_bench);
                d.native_containers.push_back(terrain::Id::cocoon);
                break;

        case ItemType::device:
                reset_data(d, ItemType::general);
                d.type = ItemType::device;
                d.value = item::Value::major_treasure;
                d.has_std_activate = true;
                d.base_name_un_id = {
                        "Strange Device",
                        "Strange Devices",
                        "a Strange Device"};
                d.base_descr = {
                        "A small piece of machinery. It could not possibly "
                        "have been designed by a human mind. Even for its "
                        "small size, it seems incredibly complex. There is no "
                        "hope of understanding the purpose or function of it "
                        "through normal means."};
                d.weight = item::Weight::light;
                d.is_identified = false;
                d.character = '%';
                d.tile = gfx::TileId::device1;
                d.is_stackable = false;
                d.land_on_hard_snd_msg = "I hear a clanking sound.";
                d.land_on_hard_sfx = audio::SfxId::metal_clank;
                d.chance_to_incl_in_spawn_list = 7;
                d.native_containers.push_back(terrain::Id::chest);
                d.native_containers.push_back(terrain::Id::cocoon);
                break;

        case ItemType::rod:
                reset_data(d, ItemType::general);
                d.type = ItemType::rod;
                d.value = item::Value::major_treasure;
                d.has_std_activate = true;
                d.base_descr = {
                        "A metallic device of cylindrical shape. "
                        "It seems to be designed for human hands, "
                        "for whatever nefarious purpose."};
                d.weight = item::Weight::light;
                d.is_identified = false;
                d.xp_on_found = 15;
                d.character = '%';
                d.tile = gfx::TileId::rod;
                d.is_stackable = false;
                d.land_on_hard_snd_msg = "I hear a clanking sound.";
                d.land_on_hard_sfx = audio::SfxId::metal_clank;
                d.chance_to_incl_in_spawn_list = 7;
                d.native_containers.push_back(terrain::Id::chest);
                d.native_containers.push_back(terrain::Id::cocoon);
                break;

        case ItemType::armor:
                reset_data(d, ItemType::general);
                d.type = ItemType::armor;
                d.character = '[';
                d.tile = gfx::TileId::armor;
                d.is_stackable = false;
                break;

        case ItemType::head_wear:
                reset_data(d, ItemType::general);
                d.type = ItemType::head_wear;
                d.character = '[';
                d.is_stackable = false;
                break;

        case ItemType::explosive:
                reset_data(d, ItemType::general);
                d.type = ItemType::explosive;
                d.has_std_activate = true;
                d.weight = item::Weight::light;
                d.character = '-';
                d.max_stack_at_spawn = 2;
                d.land_on_hard_snd_msg = "";
                break;

        default:
                break;
        }
}

// -----------------------------------------------------------------------------
// item
// -----------------------------------------------------------------------------
namespace item
{
ItemData g_data[(size_t)Id::END];

void init()
{
        TRACE_FUNC_BEGIN;

        ItemData d;

        reset_data(d, ItemType::general);
        d.id = Id::trapezohedron;
        d.base_name = {
                "Shining Trapezohedron",
                "Shining Trapezohedrons",
                "The Shining Trapezohedron"};
        d.spawn_std_range = Range(-1, -1);
        d.chance_to_incl_in_spawn_list = 0;
        d.allow_spawn = false;
        d.is_stackable = false;
        d.character = '*';
        d.color = colors::light_red();
        d.tile = gfx::TileId::trapez;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::ranged_wpn);
        d.id = Id::sawed_off;
        d.base_name = {
                "Sawed-off Shotgun",
                "Sawed-off shotguns",
                "a Sawed-off Shotgun"};
        d.base_descr = {
                "Compared to a standard shotgun, the sawed-off has a shorter "
                "effective range - however, at close range it is more "
                "devastating. It holds two barrels, and needs to be reloaded "
                "after both are discharged."};
        d.weight = Weight::medium;
        d.tile = gfx::TileId::shotgun;
        d.ranged.is_shotgun = true;
        d.melee.attack_msgs = {"strike", "strikes"};
        d.ranged.max_ammo = 2;
        d.ranged.dmg = WpnDmg(8, 24);
        d.ranged.hit_chance_mod = 0;
        d.ranged.effective_range = {0, 3};
        d.ranged.dmg_type = DmgType::shotgun;
        d.ranged.ammo_item_id = Id::shotgun_shell;
        d.ranged.attack_msgs = {"fire", "fires"};
        d.ranged.snd_msg = "I hear a shotgun blast.";
        d.ranged.attack_sfx = audio::SfxId::shotgun_sawed_off_fire;
        d.ranged.makes_ricochet_snd = true;
        d.ranged.reload_sfx = audio::SfxId::shotgun_reload;
        d.spawn_std_range.min = 2;
        d.native_containers.push_back(terrain::Id::chest);
        d.native_containers.push_back(terrain::Id::cabinet);
        d.native_containers.push_back(terrain::Id::cocoon);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::ranged_wpn);
        d.id = Id::pump_shotgun;
        d.base_name = {"Pump Shotgun", "Pump shotguns", "a Pump Shotgun"};
        d.base_descr = {
                "A pump-action shotgun has a handgrip that can be pumped back "
                "and forth in order to eject a spent round of ammunition and "
                "to chamber a fresh one. It has a single barrel above a tube "
                "magazine into which shells are inserted. The magazine has a "
                "capacity of 8 shells."};
        d.weight = Weight::medium;
        d.tile = gfx::TileId::shotgun;
        d.ranged.is_shotgun = true;
        d.melee.attack_msgs = {"strike", "strikes"};
        d.ranged.max_ammo = 8;
        d.ranged.dmg = WpnDmg(6, 18);
        d.ranged.hit_chance_mod = 0;
        d.ranged.effective_range = {0, 5};
        d.ranged.dmg_type = DmgType::shotgun;
        d.ranged.ammo_item_id = Id::shotgun_shell;
        d.ranged.attack_msgs = {"fire", "fires"};
        d.ranged.snd_msg = "I hear a shotgun blast.";
        d.ranged.attack_sfx = audio::SfxId::shotgun_pump_fire;
        d.ranged.makes_ricochet_snd = true;
        d.ranged.reload_sfx = audio::SfxId::shotgun_reload;
        d.spawn_std_range.min = 2;
        d.native_containers.push_back(terrain::Id::chest);
        d.native_containers.push_back(terrain::Id::cabinet);
        d.native_containers.push_back(terrain::Id::cocoon);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::ammo);
        d.id = Id::shotgun_shell;
        d.base_name = {"Shotgun shell", "Shotgun shells", "a shotgun shell"};
        d.base_descr = {
                "A cartridge designed to be fired from a shotgun."};
        d.color = colors::light_red();
        d.max_stack_at_spawn = 10;
        d.native_containers.push_back(terrain::Id::chest);
        d.native_containers.push_back(terrain::Id::cabinet);
        d.native_containers.push_back(terrain::Id::cocoon);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::ranged_wpn);
        d.id = Id::morphic_blaster;
        d.base_name = {"Morphic Blaster", "Morphic Blasters", "a Morphic Blaster"};
        d.base_descr = {
                "A weapon created by the Mi-Go. "
                "It launches projectiles that unleash explosive energy upon impact. "
                "The weapon adapts itself to merge with the biology of its wielder. "
                "A guidance system integrates with the brain to "
                "ensure that the projectile hits its intended mark independent of aiming skill "
                "(although there is a small chance that it collides with unintended "
                "targets along the path).",

                "When wielded by creatures lacking the peculiar power sources "
                "employed by the Mi-Go, "
                "this weapon instead draws power from the life force of the wielder (" +
                        s_morphic_blaster_hp_drained_str +
                        " hit points drained per attack, "
                        "passive hit point regeneration is disabled for " +
                        s_morphic_blaster_hp_disable_range_str +
                        " turns, unable to act for the next turn)."};
        d.weight = Weight::moderately_heavy;
        d.tile = gfx::TileId::morphic_blaster;
        d.is_unique = true;
        d.allow_spawn = false;
        d.melee.attack_msgs = {"strike", "strikes"};
        d.ranged.max_ammo = 5;
        d.ranged.dmg = WpnDmg(1, 3);
        d.ranged.effective_range = {0, 999};
        d.allow_display_dmg = false;
        d.ranged.has_infinite_ammo = true;
        d.ranged.attack_msgs = {"fire", "fires"};
        d.ranged.snd_msg = "I hear the blast of a launched projectile.";
        d.ranged.attack_sfx = audio::SfxId::morphic_blaster;
        d.ranged.projectile_character = '*';
        d.ranged.projectile_color = colors::light_blue();
        d.ranged.projectile_tile = gfx::TileId::blast1;
        d.ranged.reload_sfx = audio::SfxId::machine_gun_reload;
        d.spawn_std_range.min = g_dlvl_first_mid_game;
        d.chance_to_incl_in_spawn_list = 35;
        d.native_containers.push_back(terrain::Id::chest);
        d.native_containers.push_back(terrain::Id::cabinet);
        d.native_containers.push_back(terrain::Id::cocoon);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::ranged_wpn);
        d.id = Id::tommy_gun;
        d.base_name = {"Tommy Gun", "Tommy Guns", "a Tommy Gun"};
        d.base_descr = {
                "\"Tommy Gun\" is a nickname for the Thompson submachine gun - "
                "an automatic firearm with a drum magazine and vertical "
                "foregrip. It fires .45 ACP ammunition. The drum magazine has "
                "a capacity of 50 rounds."};
        d.weight = Weight::medium;
        d.tile = gfx::TileId::tommy_gun;
        d.melee.attack_msgs = {"strike", "strikes"};
        d.ranged.is_machine_gun = true;
        d.ranged.max_ammo = 50;
        d.ranged.dmg = WpnDmg(4, 6);
        d.ranged.hit_chance_mod = -10;
        d.ranged.effective_range = {0, 5};
        d.ranged.ammo_item_id = Id::drum_of_bullets;
        d.ranged.attack_msgs = {"fire", "fires"};
        d.ranged.snd_msg = "I hear the burst of a machine gun.";
        d.ranged.attack_sfx = audio::SfxId::machine_gun_fire;
        d.ranged.makes_ricochet_snd = true;
        d.ranged.reload_sfx = audio::SfxId::machine_gun_reload;
        d.spawn_std_range.min = 2;
        d.chance_to_incl_in_spawn_list = 75;
        d.native_containers.push_back(terrain::Id::chest);
        d.native_containers.push_back(terrain::Id::cabinet);
        d.native_containers.push_back(terrain::Id::cocoon);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::ammo_mag);
        d.id = Id::drum_of_bullets;
        d.base_name = {
                "Drum of .45 ACP",
                "Drums of .45 ACP",
                "a Drum of .45 ACP"};
        d.base_descr = {
                "Ammunition used by Tommy Guns."};
        d.ranged.max_ammo = g_data[(size_t)Id::tommy_gun].ranged.max_ammo;
        d.chance_to_incl_in_spawn_list = 50;
        d.native_containers.push_back(terrain::Id::chest);
        d.native_containers.push_back(terrain::Id::cabinet);
        d.native_containers.push_back(terrain::Id::cocoon);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::ranged_wpn);
        d.id = Id::revolver;
        d.base_name = {
                "S&W Revolver",
                "S&W Revolvers",
                "a S&W Revolver"};
        d.base_descr = {
                "A six-shot double-action revolver."};
        d.weight = Weight::moderately_light;
        d.tile = gfx::TileId::revolver;
        d.ranged.max_ammo = 6;
        d.ranged.dmg = WpnDmg(5, 10);
        d.ranged.hit_chance_mod = 5;
        d.ranged.effective_range = {0, 5};
        d.ranged.ammo_item_id = Id::revolver_bullet;
        d.melee.attack_msgs = {"strike", "strikes"};
        d.ranged.attack_msgs = {"fire", "fires"};
        d.ranged.snd_msg = "I hear a revolver being fired.";
        d.ranged.attack_sfx = audio::SfxId::revolver_fire;
        d.ranged.makes_ricochet_snd = true;
        d.ranged.reload_sfx = audio::SfxId::rifle_revolver_reload;
        d.native_containers.push_back(terrain::Id::chest);
        d.native_containers.push_back(terrain::Id::cabinet);
        d.native_containers.push_back(terrain::Id::cocoon);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::ammo);
        d.id = Id::revolver_bullet;
        d.base_name = {
                "Revolver .38 Bullet",
                "Revolver .38 Bullets",
                "a Revolver .38 Bullet"};
        d.base_descr = {
                "Ammunition used by S&W Model 10 Revolvers."};
        d.color = colors::dark_yellow();
        d.max_stack_at_spawn = 10;
        d.native_containers.push_back(terrain::Id::chest);
        d.native_containers.push_back(terrain::Id::cabinet);
        d.native_containers.push_back(terrain::Id::cocoon);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::ranged_wpn);
        d.id = Id::pistol;
        d.base_name = {
                "M1911 Colt",
                "M1911 Colts",
                "an M1911 Colt"};
        d.base_descr = {
                "A semi-automatic, magazine-fed pistol chambered for the .45 "
                "ACP cartridge."};
        d.weight = Weight::moderately_light;
        d.tile = gfx::TileId::pistol;
        d.ranged.max_ammo = 7;
        d.ranged.dmg = WpnDmg(5, 12);
        d.ranged.hit_chance_mod = 0;
        d.ranged.effective_range = {0, 5};
        d.ranged.ammo_item_id = Id::pistol_mag;
        d.melee.attack_msgs = {"strike", "strikes"};
        d.ranged.attack_msgs = {"fire", "fires"};
        d.ranged.snd_msg = "I hear a pistol being fired.";
        d.ranged.attack_sfx = audio::SfxId::pistol_fire;
        d.ranged.makes_ricochet_snd = true;
        d.ranged.reload_sfx = audio::SfxId::pistol_reload;
        d.native_containers.push_back(terrain::Id::chest);
        d.native_containers.push_back(terrain::Id::cabinet);
        d.native_containers.push_back(terrain::Id::cocoon);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::ammo_mag);
        d.id = Id::pistol_mag;
        d.base_name = {
                "Colt .45ACP Magazine",
                "Colt .45ACP Magazines",
                "a Colt .45ACP Magazine"};
        d.base_descr = {
                "Ammunition used by Colt pistols."};
        d.ranged.max_ammo = g_data[(size_t)Id::pistol].ranged.max_ammo;
        d.native_containers.push_back(terrain::Id::chest);
        d.native_containers.push_back(terrain::Id::cabinet);
        d.native_containers.push_back(terrain::Id::cocoon);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::ranged_wpn);
        d.id = Id::rifle;
        d.base_name = {
                "Winchester Rifle", "Winchester Rifles", "a Winchester Rifle"};
        d.base_descr = {
                "A lever-action repeating rifle.",

                "This weapon has an accuracy penalty at close ranges."};
        d.weight = Weight::medium;
        d.tile = gfx::TileId::rifle;
        // d.color = colors::dark_brown();
        d.ranged.max_ammo = 7;
        d.ranged.dmg = WpnDmg(10, 16);
        d.ranged.hit_chance_mod = 15;
        d.ranged.effective_range = {4, 8};
        d.ranged.ammo_item_id = Id::rifle_bullet;
        d.melee.attack_msgs = {"strike", "strikes"};
        d.ranged.attack_msgs = {"fire", "fires"};
        d.ranged.snd_msg = "I hear a rifle being fired.";
        d.ranged.attack_sfx = audio::SfxId::rifle_fire;
        d.ranged.makes_ricochet_snd = true;
        d.ranged.reload_sfx = audio::SfxId::rifle_revolver_reload;
        d.native_containers.push_back(terrain::Id::cabinet);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::ammo);
        d.id = Id::rifle_bullet;
        d.base_name = {
                "Winchester .30 Bullet",
                "Winchester .30 Bullets",
                "a Winchester .30 Bullet"};
        d.base_descr = {
                "Ammunition used by Winchester Rifles."};
        d.color = colors::dark_yellow();
        d.max_stack_at_spawn = 10;
        d.native_containers.push_back(terrain::Id::chest);
        d.native_containers.push_back(terrain::Id::cabinet);
        d.native_containers.push_back(terrain::Id::cocoon);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::ranged_wpn);
        d.id = Id::spike_gun;
        d.base_name = {"Spike Gun", "Spike Guns", "a Spike Gun"};
        d.base_descr = {
                "A very strange and crude weapon capable of launching iron "
                "spikes with enough force to pierce flesh (or even rock). It "
                "seems almost to be deliberately designed for cruelty, rather "
                "than pure stopping power."};
        d.weight = (Weight::medium * 3) / 4;
        d.tile = gfx::TileId::tommy_gun;
        d.color = colors::dark_brown();
        d.melee.attack_msgs = {"strike", "strikes"};
        d.ranged.max_ammo = 12;
        d.ranged.dmg = WpnDmg(1, 7);
        d.ranged.hit_chance_mod = 0;
        d.ranged.effective_range = {0, 4};
        d.ranged.dmg_type = DmgType::piercing;
        d.ranged.knocks_back = true;
        d.ranged.ammo_item_id = Id::iron_spike;
        d.ranged.attack_msgs = {"fire", "fires"};
        d.ranged.snd_msg = "I hear a very crude weapon being fired.";
        d.ranged.makes_ricochet_snd = true;
        d.ranged.projectile_color = colors::gray();
        d.spawn_std_range.min = 4;
        d.ranged.attack_sfx = audio::SfxId::spike_gun;
        d.ranged.snd_vol = SndVol::low;
        d.native_containers.push_back(terrain::Id::chest);
        d.native_containers.push_back(terrain::Id::cabinet);
        d.native_containers.push_back(terrain::Id::cocoon);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::ranged_wpn);
        d.id = Id::electric_gun;
        d.base_name = {
                "Electric Gun", "Electric Gun", "an Electric Gun"};
        d.base_descr = {
                "A weapon created by the Mi-Go. "
                "It fires devastating bolts of electricity.",

                "When wielded by creatures lacking the peculiar power sources "
                "employed by the Mi-Go, "
                "this weapon instead draws power from the life force of the wielder (" +
                        s_electric_gun_hp_drained_str +
                        " hit points drained per attack, "
                        "passive hit point regeneration is disabled for " +
                        s_electric_gun_hp_disable_range_str +
                        " turns)."};
        d.spawn_std_range = Range(-1, -1);
        d.weight = Weight::medium;
        d.tile = gfx::TileId::electric_gun;
        d.color = colors::yellow();
        d.ranged.dmg = WpnDmg(8, 12);
        d.ranged.hit_chance_mod = 5;
        d.ranged.effective_range = {0, 4};
        {
                prop::Prop* prop = prop::make(prop::Id::paralyzed);

                prop->set_duration(2);

                d.ranged.prop_applied = ItemAttackProp(prop);
        }
        d.ranged.dmg_type = DmgType::electric;
        d.ranged.has_infinite_ammo = true;
        d.ranged.projectile_leaves_trail = true;
        d.ranged.projectile_color = colors::yellow();
        d.melee.attack_msgs = {"strike", "strikes"};
        d.ranged.attack_msgs = {"fire", "fires"};
        d.ranged.snd_msg = "I hear a bolt of electricity.";
        d.ranged.attack_sfx = audio::SfxId::electric_gun;
        d.ranged.makes_ricochet_snd = false;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::ranged_wpn);
        d.id = Id::trap_dart;
        d.allow_spawn = false;
        d.ranged.has_infinite_ammo = true;
        d.ranged.dmg = WpnDmg(1, 8);
        d.ranged.hit_chance_mod = 70;
        d.ranged.effective_range = {0, 6};
        d.ranged.snd_msg = "I hear the launching of a projectile.";
        // TODO: Make a sound effect for this
        d.ranged.attack_sfx = audio::SfxId::END;
        d.ranged.makes_ricochet_snd = true;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::ranged_wpn);
        d = g_data[(size_t)Id::trap_dart];
        d.id = Id::trap_dart_poison;
        d.ranged.prop_applied = ItemAttackProp(prop::make(prop::Id::poisoned));
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::melee_wpn);
        d.id = Id::trap_spear;
        d.allow_spawn = false;
        d.weight = Weight::heavy;
        d.melee.dmg = WpnDmg(6, 8);
        d.melee.hit_chance_mod = 85;
        d.melee.dmg_type = DmgType::piercing;
        d.melee.hit_small_sfx = audio::SfxId::hit_sharp;
        d.melee.hit_medium_sfx = audio::SfxId::hit_sharp;
        d.melee.miss_sfx = audio::SfxId::miss_heavy;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::ranged_wpn);
        d = g_data[(size_t)Id::trap_spear];
        d.id = Id::trap_spear_poison;
        d.ranged.prop_applied = ItemAttackProp(prop::make(prop::Id::poisoned));
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::explosive);
        d.id = Id::dynamite;
        d.base_name = {"Dynamite", "Sticks of Dynamite", "a Stick of Dynamite"};
        d.base_descr = {
                "An explosive material based on nitroglycerin. The name comes "
                "from the ancient Greek word for \"power\"."};
        d.weight = Weight::light;
        d.tile = gfx::TileId::dynamite;
        d.color = colors::light_red();
        d.native_containers.push_back(terrain::Id::chest);
        d.native_containers.push_back(terrain::Id::cabinet);
        d.native_containers.push_back(terrain::Id::cocoon);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::explosive);
        d.id = Id::flare;
        d.base_name = {"Flare", "Flares", "a Flare"};
        d.base_descr = {
                "A type of pyrotechnic that produces a brilliant light or "
                "intense heat without an explosion."};
        d.weight = Weight::light;
        d.tile = gfx::TileId::flare;
        d.color = colors::gray();
        d.native_containers.push_back(terrain::Id::chest);
        d.native_containers.push_back(terrain::Id::cabinet);
        d.native_containers.push_back(terrain::Id::cocoon);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::explosive);
        d.id = Id::molotov;
        d.base_name = {
                "Molotov Cocktail", "Molotov Cocktails", "a Molotov Cocktail"};
        d.base_descr = {
                "An improvised incendiary weapon made of a glass bottle "
                "containing flammable liquid and some cloth for ignition. In "
                "action, the cloth is lit and the bottle hurled at a target, "
                "causing an immediate fireball followed by a raging fire."};
        d.weight = Weight::light;
        d.tile = gfx::TileId::molotov;
        d.color = colors::white();
        d.native_containers.push_back(terrain::Id::chest);
        d.native_containers.push_back(terrain::Id::cabinet);
        d.native_containers.push_back(terrain::Id::cocoon);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::explosive);
        d.id = Id::smoke_grenade;
        d.base_name = {"Smoke Grenade", "Smoke Grenades", "a Smoke Grenade"};
        d.base_descr = {
                "A sheet steel cylinder with emission holes releasing smoke "
                "when the grenade is ignited. Their primary use is to create "
                "smoke screens for concealment. The fumes produced can harm "
                "the eyes, throat and lungs - so it is recommended to wear a "
                "protective mask."};
        d.weight = Weight::light;
        d.tile = gfx::TileId::flare;
        d.color = colors::green();
        d.native_containers.push_back(terrain::Id::chest);
        d.native_containers.push_back(terrain::Id::cabinet);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::throwing_wpn);
        d.id = Id::thr_knife;
        d.base_name = {"Throwing Knife", "Throwing Knives", "a Throwing Knife"};
        d.base_descr = {
                "A knife specially designed and weighted so that it can be "
                "thrown effectively."};
        d.weight = Weight::extra_light;
        d.tile = gfx::TileId::dagger;
        d.character = '/';
        d.color = colors::white();
        d.ranged.dmg = WpnDmg(2, 6);
        d.ranged.throw_hit_chance_mod = 10;
        d.ranged.effective_range = {0, 4};
        d.ranged.max_range = d.ranged.effective_range.max + 3;
        d.max_stack_at_spawn = 6;
        d.land_on_hard_snd_msg = "I hear a clanking sound.";
        d.land_on_hard_sfx = audio::SfxId::metal_clank;
        d.main_attack_mode = AttackMode::thrown;
        d.native_containers.push_back(terrain::Id::chest);
        d.native_containers.push_back(terrain::Id::cabinet);
        d.native_containers.push_back(terrain::Id::cocoon);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::throwing_wpn);
        d.id = Id::rock;
        d.base_name = {"Rock", "Rocks", "a Rock"};
        d.base_descr = {
                "Although not a very impressive weapon, with skill they can "
                "be used with some result."};
        d.weight = Weight::extra_light;
        d.tile = gfx::TileId::rock;
        d.character = '*';
        d.color = colors::gray();
        d.ranged.dmg = WpnDmg(1, 3);
        d.ranged.effective_range = {0, 3};
        d.ranged.max_range = d.ranged.effective_range.max + 3;
        d.ranged.dmg_type = DmgType::blunt;
        d.max_stack_at_spawn = 3;
        d.main_attack_mode = AttackMode::thrown;
        d.native_containers.push_back(terrain::Id::cabinet);
        d.native_containers.push_back(terrain::Id::cocoon);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::melee_wpn);
        d.id = Id::dagger;
        d.base_name = {"Dagger", "Daggers", "a Dagger"};
        d.base_descr = {
                "Commonly associated with deception, stealth, and treachery. "
                "Many assassinations have been carried out with the use of a "
                "dagger.",

                "Melee attacks performed with a dagger against an unaware "
                "opponent does +200% damage (in addition to the normal +50% "
                "damage from stealth attacks).",

                "Melee attacks with daggers are silent."};
        d.weight = Weight::light;
        d.tile = gfx::TileId::dagger;
        d.melee.attack_msgs = {"stab", "stabs"};
        d.melee.dmg = WpnDmg(2, 4);
        d.melee.hit_chance_mod = 20;
        d.melee.dmg_type = DmgType::piercing;
        d.melee.is_noisy = false;
        d.melee.hit_medium_sfx = audio::SfxId::hit_sharp;
        d.melee.hit_hard_sfx = audio::SfxId::hit_sharp;
        d.melee.miss_sfx = audio::SfxId::miss_light;
        d.ranged.dmg_type = DmgType::piercing;
        d.native_containers.push_back(terrain::Id::chest);
        d.native_containers.push_back(terrain::Id::cabinet);
        d.native_containers.push_back(terrain::Id::tomb);
        d.native_containers.push_back(terrain::Id::cocoon);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::melee_wpn);
        d.id = Id::hatchet;
        d.base_name = {"Hatchet", "Hatchets", "a Hatchet"};
        d.base_descr = {
                "A small axe with a short handle. Hatchets are reliable "
                "weapons - they are easy to use, and cause decent damage for "
                "their low weight.",

                "Melee attacks with hatchets are silent."};
        d.weight = Weight::light;
        d.tile = gfx::TileId::axe;
        d.melee.attack_msgs = {"strike", "strikes"};
        d.melee.dmg = WpnDmg(1, 6);
        d.melee.hit_chance_mod = 15;
        d.melee.can_attack_corpse = true;
        d.melee.dmg_type = DmgType::slashing;
        d.melee.is_noisy = false;
        d.melee.hit_medium_sfx = audio::SfxId::hit_sharp;
        d.melee.hit_hard_sfx = audio::SfxId::hit_sharp;
        d.melee.miss_sfx = audio::SfxId::miss_light;
        d.ranged.throw_hit_chance_mod = -10;
        d.ranged.effective_range = {0, 4};
        d.ranged.max_range = d.ranged.effective_range.max + 3;
        d.ranged.dmg_type = DmgType::slashing;
        d.native_containers.push_back(terrain::Id::chest);
        d.native_containers.push_back(terrain::Id::cabinet);
        d.native_containers.push_back(terrain::Id::cocoon);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::melee_wpn);
        d.id = Id::club;
        d.base_name = {"Club", "Clubs", "a Club"};
        d.base_descr = {
                "Wielded since prehistoric times.",

                "Melee attacks with clubs are silent."};
        d.spawn_std_range = Range(g_dlvl_first_mid_game, g_dlvl_last);
        d.weight = Weight::medium;
        d.tile = gfx::TileId::club;
        d.color = colors::brown();
        d.melee.attack_msgs = {"strike", "strikes"};
        d.melee.dmg = WpnDmg(3, 6);
        d.melee.hit_chance_mod = 15;
        d.melee.can_attack_corpse = true;
        d.melee.dmg_type = DmgType::blunt;
        d.melee.is_noisy = false;
        d.melee.miss_sfx = audio::SfxId::miss_medium;
        d.ranged.throw_hit_chance_mod = -10;
        d.ranged.effective_range = {0, 4};
        d.ranged.max_range = d.ranged.effective_range.max + 3;
        d.ranged.dmg_type = DmgType::blunt;
        d.land_on_hard_snd_msg = "I hear a thudding sound.";
        d.land_on_hard_sfx = audio::SfxId::END;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::melee_wpn);
        d.id = Id::hammer;
        d.base_name = {"Hammer", "Hammers", "a Hammer"};
        d.base_descr = {
                "Typically used for construction, but can be quite devastating "
                "when wielded as a weapon.",

                "Melee attacks with hammers are noisy."};
        d.weight = Weight::medium;
        d.tile = gfx::TileId::hammer;
        d.melee.attack_msgs = {"smash", "smashes"};
        d.melee.dmg = WpnDmg(4, 7);
        d.melee.hit_chance_mod = 10;
        d.melee.can_attack_corpse = true;
        d.melee.dmg_type = DmgType::blunt;
        d.melee.is_noisy = true;
        d.melee.miss_sfx = audio::SfxId::miss_medium;
        d.ranged.dmg_type = DmgType::blunt;
        d.native_containers.push_back(terrain::Id::cabinet);
        d.native_containers.push_back(terrain::Id::cocoon);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::melee_wpn);
        d.id = Id::machete;
        d.base_name = {"Machete", "Machetes", "a Machete"};
        d.base_descr = {
                "A large cleaver-like knife. It serves well both as a cutting "
                "tool and weapon.",

                "Melee attacks with machetes are noisy."};
        d.weight = Weight::medium;
        d.tile = gfx::TileId::machete;
        d.melee.attack_msgs = {"chop", "chops"};
        d.melee.dmg = WpnDmg(3, 9);
        d.melee.hit_chance_mod = 5;
        d.melee.can_attack_corpse = true;
        d.melee.dmg_type = DmgType::slashing;
        d.melee.hit_small_sfx = audio::SfxId::hit_sharp;
        d.melee.hit_medium_sfx = audio::SfxId::hit_sharp;
        d.melee.miss_sfx = audio::SfxId::miss_medium;
        d.melee.is_noisy = true;
        d.ranged.dmg_type = DmgType::slashing;
        d.native_containers.push_back(terrain::Id::cabinet);
        d.native_containers.push_back(terrain::Id::cocoon);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::melee_wpn);
        d.id = Id::axe;
        d.base_name = {"Axe", "Axes", "an Axe"};
        d.base_descr = {
                "A tool intended for felling trees, splitting timber, etc. "
                "Used as a weapon it can deliver devastating blows, although "
                "it requires some skill to use effectively.",

                "Melee attacks with axes are noisy."};
        d.weight = Weight::medium;
        d.tile = gfx::TileId::axe;
        d.melee.attack_msgs = {"strike", "strikes"};
        d.melee.dmg = WpnDmg(3, 12);
        d.melee.hit_chance_mod = 0;
        d.melee.can_attack_corpse = true;
        d.melee.can_attack_door_wood = true;
        d.melee.dmg_type = DmgType::slashing;
        d.melee.miss_sfx = audio::SfxId::miss_medium;
        d.melee.is_noisy = true;
        d.ranged.dmg_type = DmgType::slashing;
        d.native_containers.push_back(terrain::Id::cabinet);
        d.native_containers.push_back(terrain::Id::tomb);
        d.native_containers.push_back(terrain::Id::cocoon);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::melee_wpn);
        d.id = Id::spiked_mace;
        d.base_name = {"Spiked Mace", "Spiked Maces", "a Spiked Mace"};
        d.base_descr = {
                "A brutal weapon, utilizing a combination of blunt-force and "
                "puncture.",

                "Attacks with this weapon have a 25% chance to stun the "
                "victim, rendering them unable to act for a brief time.",

                "Melee attacks with spiked maces are noisy."};
        d.weight = Weight::moderately_heavy;
        d.tile = gfx::TileId::spiked_mace;
        d.melee.attack_msgs = {"strike", "strikes"};
        d.melee.dmg = WpnDmg(1, 14);
        d.melee.hit_chance_mod = -10;
        d.melee.can_attack_corpse = true;
        d.melee.dmg_type = DmgType::piercing;
        d.melee.miss_sfx = audio::SfxId::miss_heavy;
        d.melee.is_noisy = true;
        {
                prop::Prop* prop = prop::make(prop::Id::paralyzed);

                prop->set_duration(2);

                d.melee.prop_applied.prop.reset(prop);
                d.melee.prop_applied.pct_chance_to_apply = 25;
        }
        d.ranged.dmg_type = DmgType::piercing;
        d.native_containers.push_back(terrain::Id::cabinet);
        d.native_containers.push_back(terrain::Id::tomb);
        d.native_containers.push_back(terrain::Id::cocoon);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::melee_wpn);
        d.id = Id::pitchfork;
        d.base_name = {"Pitchfork", "Pitchforks", "a Pitchfork"};
        d.base_descr = {
                "A long staff with a forked, four-pronged end.",

                "Pitchforks are useful in keeping attackers at bay - "
                "the victim is pushed back when stabbed."};
        d.weight = Weight::moderately_heavy;
        d.tile = gfx::TileId::pitchfork;
        d.melee.attack_msgs = {"strike", "strikes"};
        d.melee.dmg = WpnDmg(1, 7);
        d.melee.hit_chance_mod = -10;
        d.melee.can_attack_corpse = true;
        d.melee.reach = 2;
        d.melee.knocks_back = true;
        d.melee.dmg_type = DmgType::piercing;
        d.melee.is_noisy = true;
        d.melee.hit_small_sfx = audio::SfxId::hit_sharp;
        d.melee.hit_medium_sfx = audio::SfxId::hit_sharp;
        d.melee.miss_sfx = audio::SfxId::miss_heavy;
        d.ranged.dmg_type = DmgType::piercing;
        d.native_containers.push_back(terrain::Id::cabinet);
        d.native_containers.push_back(terrain::Id::cocoon);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::melee_wpn);
        d.id = Id::spear;
        d.base_name = {"Spear", "Spears", "a Spear"};
        d.base_descr = {
                "A pole weapon consisting of a wooden shaft and a steel head."};
        d.weight = Weight::moderately_heavy;
        d.tile = gfx::TileId::spear;
        d.color = colors::brown();
        d.melee.attack_msgs = {"strike", "strikes"};
        d.melee.dmg = WpnDmg(1, 9);
        d.melee.hit_chance_mod = 0;
        d.melee.can_attack_corpse = true;
        d.melee.reach = 2;
        d.melee.dmg_type = DmgType::piercing;
        d.melee.is_noisy = true;
        d.melee.hit_small_sfx = audio::SfxId::hit_sharp;
        d.melee.hit_medium_sfx = audio::SfxId::hit_sharp;
        d.melee.miss_sfx = audio::SfxId::miss_heavy;
        d.ranged.dmg_type = DmgType::piercing;
        d.native_containers.push_back(terrain::Id::cabinet);
        d.native_containers.push_back(terrain::Id::cocoon);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::melee_wpn);
        d.id = Id::sledgehammer;
        d.base_name = {"Sledgehammer", "Sledgehammers", "a Sledgehammer"};
        d.base_descr = {
                "It can deal devastating damage, although it is cumbersome "
                "to carry, and it requires some skill to use effectively."};
        d.weight = Weight::heavy;
        d.tile = gfx::TileId::sledgehammer;
        d.melee.attack_msgs = {"smash", "smashes"};
        d.melee.dmg = WpnDmg(4, 15);
        d.melee.hit_chance_mod = -10;
        d.melee.can_attack_corpse = true;
        d.melee.can_attack_door_wood = true;
        d.melee.can_attack_door_gate = true;
        d.melee.dmg_type = DmgType::blunt;
        d.melee.miss_sfx = audio::SfxId::miss_heavy;
        d.ranged.dmg_type = DmgType::blunt;
        d.native_containers.push_back(terrain::Id::cabinet);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::throwing_wpn);
        d.id = Id::iron_spike;
        d.base_name = {"Iron Spike", "Iron Spikes", "an Iron Spike"};
        d.base_descr = {
                "Can be useful for wedging things closed."};
        d.weight = Weight::extra_light;
        d.tile = gfx::TileId::iron_spike;
        d.is_stackable = true;
        d.color = colors::gray();
        d.character = '/';
        d.ranged.throw_hit_chance_mod = -5;
        d.ranged.dmg = WpnDmg(1, 4);
        d.ranged.effective_range = {0, 3};
        d.ranged.max_range = d.ranged.effective_range.max + 3;
        d.max_stack_at_spawn = 12;
        d.land_on_hard_snd_msg = "I hear a clanking sound.";
        d.land_on_hard_sfx = audio::SfxId::metal_clank;
        d.main_attack_mode = AttackMode::thrown;
        d.native_containers.push_back(terrain::Id::cabinet);
        d.native_containers.push_back(terrain::Id::cocoon);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::melee_wpn_intr);
        d.id = Id::player_kick;
        d.melee.attack_msgs = {"kick", ""};
        d.melee.hit_chance_mod = 15;
        d.melee.dmg = WpnDmg(1, 2);
        d.melee.knocks_back = true;
        d.melee.dmg_type = DmgType::kicking;
        d.melee.can_attack_door_wood = true;
        d.melee.can_attack_door_gate = true;
        d.melee.can_attack_corpse = true;
        d.melee.miss_sfx = audio::SfxId::miss_medium;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::melee_wpn_intr);
        d.id = Id::player_stomp;
        d.melee.attack_msgs = {"stomp", ""};
        d.melee.hit_chance_mod =
                g_data[(size_t)Id::player_kick].melee.hit_chance_mod;
        d.melee.dmg =
                g_data[(size_t)Id::player_kick].melee.dmg;
        d.melee.miss_sfx =
                g_data[(size_t)Id::player_kick].melee.miss_sfx;
        d.melee.dmg_type = DmgType::kicking;
        d.melee.knocks_back = false;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::melee_wpn_intr);
        d.id = Id::player_punch;
        d.base_name = {"Punch", "", "a punch"};
        d.melee.attack_msgs = {"punch", ""};
        d.melee.hit_chance_mod = 20;
        d.melee.dmg = WpnDmg(1, 1);
        d.melee.miss_sfx = audio::SfxId::miss_light;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::melee_wpn_intr);
        d.id = Id::player_ghoul_claw;
        d.base_name = {"Claw", "", "clawing"};
        d.melee.attack_msgs = {"claw", ""};
        d.melee.hit_chance_mod = 20;
        d.melee.dmg = WpnDmg(1, 8);
        d.melee.is_noisy = false;
        d.melee.can_attack_corpse = true;
        d.melee.dmg_type = DmgType::slashing;
        d.melee.hit_small_sfx = audio::SfxId::hit_sharp;
        d.melee.hit_medium_sfx = audio::SfxId::hit_sharp;
        d.melee.miss_sfx = audio::SfxId::miss_medium;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::melee_wpn_intr);
        d.id = Id::intr_kick;
        d.melee.attack_msgs = {"", "kicks"};
        d.melee.dmg_type = DmgType::blunt;
        d.melee.knocks_back = true;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::melee_wpn_intr);
        d.id = Id::intr_bite;
        d.melee.attack_msgs = {"", "bites"};
        d.melee.dmg_type = DmgType::piercing;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::melee_wpn_intr);
        d.id = Id::intr_claw;
        d.melee.attack_msgs = {"", "claws"};
        d.melee.dmg_type = DmgType::slashing;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::melee_wpn_intr);
        d.id = Id::intr_strike;
        d.melee.attack_msgs = {"", "strikes"};
        d.melee.dmg_type = DmgType::blunt;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::melee_wpn_intr);
        d.id = Id::intr_punch;
        d.melee.attack_msgs = {"", "punches"};
        d.melee.dmg_type = DmgType::blunt;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::melee_wpn_intr);
        d.id = Id::intr_punch_knockback;
        d.melee.attack_msgs = {"", "punches"};
        d.melee.dmg_type = DmgType::blunt;
        d.melee.knocks_back = true;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::melee_wpn_intr);
        d.id = Id::intr_headbutt;
        d.melee.attack_msgs = {"", "slams into"};
        d.melee.dmg_type = DmgType::blunt;
        d.melee.knocks_back = true;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::ranged_wpn_intr);
        d.id = Id::intr_putrid_spit;
        d.ranged.attack_msgs = {"", "spits pus"};
        d.ranged.snd_msg = "I hear spitting.";
        d.ranged.projectile_color = colors::light_green();
        d.ranged.dmg_type = DmgType::blunt;
        d.ranged.projectile_character = '*';
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::ranged_wpn_intr);
        d.id = Id::intr_snake_venom_spit;
        d.ranged.attack_msgs = {"", "spits venom"};
        d.ranged.snd_msg = "I hear hissing and spitting.";
        d.ranged.projectile_color = colors::light_green();
        d.ranged.dmg_type = DmgType::piercing;
        d.ranged.projectile_character = '*';
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::ranged_wpn_intr);
        d.id = Id::intr_earth_breath;
        d.ranged.attack_msgs = {"", "breathes forth immense density"};
        d.ranged.snd_msg = "I hear a hammering sound.";
        d.ranged.attack_sfx = audio::SfxId::earth_breath;
        d.ranged.projectile_color = colors::brown();
        d.ranged.projectile_character = '*';
        d.ranged.projectile_tile = gfx::TileId::blast1;
        d.ranged.projectile_leaves_trail = false;
        d.ranged.dmg_type = DmgType::blunt;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::ranged_wpn_intr);
        d.id = Id::intr_water_breath;
        d.ranged.attack_msgs = {"", "breathes forth a raging torrent"};
        d.ranged.snd_msg = "I hear a gushing sound.";
        d.ranged.attack_sfx = audio::SfxId::water_breath;
        d.ranged.projectile_color = colors::light_blue();
        d.ranged.projectile_character = '*';
        d.ranged.projectile_tile = gfx::TileId::blast1;
        d.ranged.projectile_leaves_trail = true;
        d.ranged.dmg_type = DmgType::blunt;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::ranged_wpn_intr);
        d.id = Id::intr_fire_breath;
        d.ranged.attack_msgs = {"", "breathes fire"};
        d.ranged.snd_msg = "I hear a burst of flames.";
        d.ranged.attack_sfx = audio::SfxId::fire_breath;
        d.ranged.projectile_color = colors::light_red();
        d.ranged.projectile_character = '*';
        d.ranged.projectile_tile = gfx::TileId::blast1;
        d.ranged.projectile_leaves_trail = true;
        d.ranged.dmg_type = DmgType::fire;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::ranged_wpn_intr);
        d.id = Id::intr_lightning_breath;
        d.ranged.attack_msgs = {"", "breathes lightning"};
        d.ranged.snd_msg = "I hear a burst of lightning.";
        d.ranged.attack_sfx = audio::SfxId::lightning_breath;
        d.ranged.projectile_color = colors::yellow();
        d.ranged.projectile_character = '*';
        d.ranged.projectile_tile = gfx::TileId::blast1;
        d.ranged.projectile_leaves_trail = true;
        d.ranged.dmg_type = DmgType::electric;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::melee_wpn_intr);
        d.id = Id::intr_raven_peck;
        d.melee.attack_msgs = {"", "pecks"};
        d.melee.dmg_type = DmgType::piercing;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::melee_wpn_intr);
        d.id = Id::intr_vampiric_bite;
        d.melee.attack_msgs = {"", "bites"};
        d.melee.dmg_type = DmgType::piercing;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::melee_wpn_intr);
        d.id = Id::intr_strangle;
        d.melee.attack_msgs = {"", "strangles"};
        d.melee.dmg_type = DmgType::blunt;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::melee_wpn_intr);
        d.id = Id::intr_ghost_touch;
        d.melee.attack_msgs = {"", "reaches for"};
        d.melee.dmg_type = DmgType::spirit;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::melee_wpn_intr);
        d.id = Id::intr_sting;
        d.melee.attack_msgs = {"", "stings"};
        d.melee.dmg_type = DmgType::piercing;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::melee_wpn_intr);
        d.id = Id::intr_mind_leech_sting;
        d.melee.attack_msgs = {"", "stings"};
        d.melee.dmg_type = DmgType::piercing;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::melee_wpn_intr);
        d.id = Id::intr_spear_thrust;
        d.melee.attack_msgs = {"", "strikes"};
        d.melee.dmg_type = DmgType::piercing;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::ranged_wpn_intr);
        d.id = Id::intr_net_throw;
        d.ranged.attack_msgs = {"", "throws a net"};
        d.ranged.snd_msg = "I hear a whooshing sound.";
        d.ranged.projectile_color = colors::brown();
        d.ranged.dmg_type = DmgType::blunt;
        d.ranged.projectile_character = '*';
        d.ranged.projectile_tile = gfx::TileId::web;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::melee_wpn_intr);
        d.id = Id::intr_maul;
        d.melee.attack_msgs = {"", "mauls"};
        d.melee.dmg_type = DmgType::blunt;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::melee_wpn_intr);
        d.id = Id::intr_pus_spew;
        d.melee.attack_msgs = {"", "spews pus on"};
        d.melee.dmg_type = DmgType::blunt;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::melee_wpn_intr);
        d.id = Id::intr_strange_color_touch;
        d.melee.attack_msgs = {"", "touches"};
        d.melee.dmg_type = DmgType::pure;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::melee_wpn_intr);
        d.id = Id::intr_dust_engulf;
        d.melee.attack_msgs = {"", "engulfs"};
        d.melee.dmg_type = DmgType::blunt;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::melee_wpn_intr);
        d.id = Id::intr_fire_engulf;
        d.melee.attack_msgs = {"", "engulfs"};
        d.melee.dmg_type = DmgType::fire;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::melee_wpn_intr);
        d.id = Id::intr_energy_engulf;
        d.melee.attack_msgs = {"", "engulfs"};
        d.melee.dmg_type = DmgType::electric;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::melee_wpn_intr);
        d.id = Id::intr_spores;
        d.melee.attack_msgs = {"", "releases spores on"};
        d.melee.dmg_type = DmgType::blunt;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::ranged_wpn_intr);
        d.id = Id::intr_web_bola;
        d.ranged.attack_msgs = {"", "shoots a web bola"};
        d.ranged.snd_msg = "";
        d.ranged.projectile_color = colors::light_white();
        d.ranged.projectile_tile = gfx::TileId::blast1;
        d.ranged.projectile_character = '*';
        d.ranged.dmg_type = DmgType::blunt;
        d.ranged.snd_vol = SndVol::low;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::armor);
        d.id = Id::armor_leather_jacket;
        d.base_name = {"Leather Jacket", "", "a Leather Jacket"};
        d.base_descr = {
                "It offers some protection."};
        d.weight = Weight::light;
        d.color = colors::brown();
        d.spawn_std_range.min = 1;
        d.armor.armor_points = 1;
        d.armor.dmg_to_durability_factor = 1.0;
        d.land_on_hard_snd_msg = "";
        d.native_containers.push_back(terrain::Id::cabinet);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::armor);
        d.id = Id::armor_heavy_coat;
        d.base_name = {"Heavy Coat", "", "a Heavy Coat"};
        d.base_descr = {
                "It offers decent protection, at the cost of making movement "
                "slightly more difficult (-5% stealth, -5% dodging)."};
        d.ability_mods_while_equipped[(size_t)AbilityId::stealth] = -5;
        d.ability_mods_while_equipped[(size_t)AbilityId::dodging] = -5;
        d.weight = Weight::medium;
        d.color = colors::light_blue();
        d.spawn_std_range.min = 1;
        d.armor.armor_points = 2;
        d.armor.dmg_to_durability_factor = 1.0;
        d.land_on_hard_snd_msg = "";
        d.native_containers.push_back(terrain::Id::cabinet);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::armor);
        d.id = Id::armor_iron_suit;
        d.base_name = {"Iron Suit", "", "an Iron Suit"};
        d.base_descr = {
                "A crude armour constructed from metal plates, bolts, and "
                "leather straps.",

                "It can absorb a high amount of damage, but it makes movement "
                "a lot more difficult (-20% stealth, -20% dodging)."};
        d.ability_mods_while_equipped[(size_t)AbilityId::stealth] = -20;
        d.ability_mods_while_equipped[(size_t)AbilityId::dodging] = -20;
        d.weight = Weight::extra_heavy;
        d.color = colors::white();
        d.spawn_std_range.min = 2;
        d.armor.armor_points = 5;
        d.armor.dmg_to_durability_factor = 0.3;
        d.land_on_hard_snd_msg = "I hear a crashing sound.";
        d.native_containers.push_back(terrain::Id::cabinet);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::armor);
        d.id = Id::armor_flak_jacket;
        d.base_name = {"Flak Jacket", "", "a Flak Jacket"};
        d.base_descr = {
                "An armour consisting of steel plates sewn into a waistcoat.",

                "It offers very good protection for its weight, but is "
                "somewhat bulky to wear (-10% stealth, -10% dodging)."};
        d.ability_mods_while_equipped[(size_t)AbilityId::stealth] = -10;
        d.ability_mods_while_equipped[(size_t)AbilityId::dodging] = -10;
        d.weight = Weight::medium;
        d.color = colors::green();
        d.spawn_std_range.min = 3;
        d.armor.armor_points = 3;
        d.armor.dmg_to_durability_factor = 0.5;
        d.land_on_hard_snd_msg = "I hear a thudding sound.";
        d.native_containers.push_back(terrain::Id::cabinet);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::armor);
        d.id = Id::armor_asb_suit;
        d.base_name = {"Asbestos Suit", "", "an Asbestos Suit"};
        d.base_descr = {
                "A one piece overall of asbestos fabric, including a hood, "
                "furnace mask, gloves and shoes.",

                "It protects the wearer against fire and electricity, "
                "and also against smoke, fumes and gas.",

                "It is somewhat bulky to wear (-10% stealth, -10% dodging)."};
        d.ability_mods_while_equipped[(size_t)AbilityId::stealth] = -10;
        d.ability_mods_while_equipped[(size_t)AbilityId::dodging] = -10;
        d.weight = Weight::medium;
        d.color = colors::light_red();
        d.spawn_std_range.min = 3;
        d.armor.armor_points = 1;
        d.armor.dmg_to_durability_factor = 1.0;
        d.land_on_hard_snd_msg = "";
        d.native_containers.push_back(terrain::Id::cabinet);
        d.native_containers.push_back(terrain::Id::chest);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::armor);
        d.id = Id::armor_mi_go;
        d.base_name = {"Mi-Go Bio-armor", "", "a Mi-Go Bio-armor"};
        d.base_descr = {
                "An extremely durable biological armor created by the Mi-Go."};
        d.spawn_std_range = Range(-1, -1);
        d.weight = Weight::medium;
        d.color = colors::magenta();
        d.tile = gfx::TileId::mi_go_armor;
        d.armor.armor_points = 3;
        d.armor.dmg_to_durability_factor = 0.1;
        d.land_on_hard_snd_msg = "";
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::head_wear);
        d.id = Id::gas_mask;
        d.base_name = {"Gas Mask", "", "a Gas Mask"};
        d.base_descr = {
                "Protects the eyes, throat and lungs from smoke and fumes. It "
                "has a limited useful lifespan that is related to the "
                "absorbent capacity of the filter. ",

                "Due to the small eye windows, aiming is slightly more "
                "difficult, and it is harder to detect sneaking enemies and "
                "hidden objects "
                "(-10% melee and ranged hit chance, -6% searching)."};
        d.ability_mods_while_equipped[(size_t)AbilityId::melee] = -10;
        d.ability_mods_while_equipped[(size_t)AbilityId::ranged] = -10;
        d.ability_mods_while_equipped[(size_t)AbilityId::searching] = -6;
        d.is_stackable = false;
        d.color = colors::brown();
        d.tile = gfx::TileId::gas_mask;
        d.character = '[';
        d.spawn_std_range = Range(1, g_dlvl_last_early_game);
        d.chance_to_incl_in_spawn_list = 50;
        d.weight = Weight::light;
        d.land_on_hard_snd_msg = "";
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::head_wear);
        d.id = Id::torture_collar;
        d.base_name = {"Torture Collar", "", "a Torture Collar"};
        d.base_descr = {
                "A gruesome torture device with spikes driven into the neck "
                "of the wearer. It is impossible to take off.",

                "Walking with the collar requires extra turns, and stealth "
                "and evasion are reduced by 20%. However, wearing the "
                "collar hardens the Flagellant against physical suffering, "
                "armor is increased by 3 points."};
        d.is_stackable = false;
        d.color = colors::red();
        d.tile = gfx::TileId::torture_collar;
        d.character = '[';
        d.weight = Weight::light;
        d.is_unique = true;
        d.value = Value::supreme_treasure;
        d.chance_to_incl_in_spawn_list = 0;
        d.allow_spawn = false;
        d.ability_mods_while_equipped[(size_t)AbilityId::stealth] = -20;
        d.ability_mods_while_equipped[(size_t)AbilityId::dodging] = -20;
        d.armor.armor_points = 3;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::scroll);
        d.id = Id::scroll_aura_of_decay;
        d.spell_cast_from_scroll = SpellId::aura_of_decay;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::scroll);
        d.id = Id::scroll_cataclysm;
        d.spell_cast_from_scroll = SpellId::cataclysm;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::scroll);
        d.id = Id::scroll_telep;
        d.spell_cast_from_scroll = SpellId::teleport;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::scroll);
        d.id = Id::scroll_pestilence;
        d.spell_cast_from_scroll = SpellId::pestilence;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::scroll);
        d.id = Id::scroll_enfeeble;
        d.spell_cast_from_scroll = SpellId::enfeeble;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::scroll);
        d.id = Id::scroll_slow;
        d.spell_cast_from_scroll = SpellId::slow;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::scroll);
        d.id = Id::scroll_terrify;
        d.spell_cast_from_scroll = SpellId::terrify;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::scroll);
        d.id = Id::scroll_bless;
        d.spell_cast_from_scroll = SpellId::bless;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::scroll);
        d.id = Id::scroll_darkbolt;
        d.spell_cast_from_scroll = SpellId::darkbolt;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::scroll);
        d.id = Id::scroll_aza_gaze;
        d.spell_cast_from_scroll = SpellId::aza_gaze;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::scroll);
        d.id = Id::scroll_control_object;
        d.spell_cast_from_scroll = SpellId::control_object;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::scroll);
        d.id = Id::scroll_light;
        d.spell_cast_from_scroll = SpellId::light;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::scroll);
        d.id = Id::scroll_spectral_wpns;
        d.spell_cast_from_scroll = SpellId::spectral_weapons;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::scroll);
        d.id = Id::scroll_transmut;
        d.spell_cast_from_scroll = SpellId::transmut;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::scroll);
        d.id = Id::scroll_thorns;
        d.spell_cast_from_scroll = SpellId::thorns;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::scroll);
        d.id = Id::scroll_blood_temper;
        d.spell_cast_from_scroll = SpellId::blood_tempering;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::scroll);
        d.id = Id::scroll_sacrifice_life;
        d.spell_cast_from_scroll = SpellId::sacrifice_life;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::scroll);
        d.id = Id::scroll_crimson_passage;
        d.spell_cast_from_scroll = SpellId::crimson_passage;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::scroll);
        d.id = Id::scroll_heal;
        d.spell_cast_from_scroll = SpellId::heal;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::scroll);
        d.id = Id::scroll_invis;
        d.spell_cast_from_scroll = SpellId::invis;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::scroll);
        d.id = Id::scroll_see_invis;
        d.spell_cast_from_scroll = SpellId::see_invis;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::scroll);
        d.id = Id::scroll_premonition;
        d.spell_cast_from_scroll = SpellId::premonition;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::scroll);
        d.id = Id::scroll_erudition;
        d.spell_cast_from_scroll = SpellId::erudition;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::scroll);
        d.id = Id::scroll_haste;
        d.spell_cast_from_scroll = SpellId::haste;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::scroll);
        d.id = Id::scroll_spell_shield;
        d.spell_cast_from_scroll = SpellId::spell_shield;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::potion);
        d.id = Id::potion_skill;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::potion);
        d.id = Id::potion_carapace;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::potion);
        d.id = Id::potion_blinking;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::potion);
        d.id = Id::potion_burrowing;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::potion);
        d.id = Id::potion_vitality;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::potion);
        d.id = Id::potion_spirit;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::potion);
        d.id = Id::potion_blindness;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::potion);
        d.id = Id::potion_fortitude;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::potion);
        d.id = Id::potion_paralyze;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::potion);
        d.id = Id::potion_conf;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::potion);
        d.id = Id::potion_poison;
        mod_spawn_chance(d, 0.66);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::potion);
        d.id = Id::potion_insight;
        d.chance_to_incl_in_spawn_list = 100;
        d.spawn_std_range.max = g_dlvl_last_mid_game;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::potion);
        d.id = Id::potion_resistance;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::potion);
        d.id = Id::potion_curing;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::potion);
        d.id = Id::potion_descent;
        mod_spawn_chance(d, 0.15);
        d.spawn_std_range.max = g_dlvl_last_mid_game;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::device);
        d.id = Id::device_blaster;
        d.base_name = {"Blaster Device", "Blaster Devices", "a Blaster Device"};
        d.color = colors::gray();
        d.native_containers.push_back(terrain::Id::chest);
        d.native_containers.push_back(terrain::Id::tomb);
        d.native_containers.push_back(terrain::Id::cocoon);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::device);
        d.id = Id::device_rejuvenator;
        d.base_name = {
                "Rejuvenator Device",
                "Rejuvenator Devices",
                "a Rejuvenator Device"};
        d.color = colors::gray();
        d.native_containers.push_back(terrain::Id::chest);
        d.native_containers.push_back(terrain::Id::tomb);
        d.native_containers.push_back(terrain::Id::cocoon);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::device);
        d.id = Id::device_translocator;
        d.base_name = {
                "Translocator Device",
                "Translocator Devices",
                "a Translocator Device"};
        d.color = colors::gray();
        d.native_containers.push_back(terrain::Id::chest);
        d.native_containers.push_back(terrain::Id::tomb);
        d.native_containers.push_back(terrain::Id::cocoon);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::device);
        d.id = Id::device_sentry_drone;
        d.base_name = {
                "Sentry Drone Device",
                "Sentry Drone Devices",
                "a Sentry Drone Device"};
        d.color = colors::gray();
        d.native_containers.push_back(terrain::Id::chest);
        d.native_containers.push_back(terrain::Id::tomb);
        d.native_containers.push_back(terrain::Id::cocoon);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::device);
        d.id = Id::device_force_field;
        d.base_name = {
                "Force Field Device",
                "Force Field Devices",
                "a Force Field Device"};
        d.color = colors::gray();
        d.native_containers.push_back(terrain::Id::chest);
        d.native_containers.push_back(terrain::Id::tomb);
        d.native_containers.push_back(terrain::Id::cocoon);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::rod);
        d.id = Id::rod_cloud_minds;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::rod);
        d.id = Id::rod_deafening;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::rod);
        d.id = Id::rod_displacement;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::rod);
        d.id = Id::rod_door_creation;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::rod);
        d.id = Id::rod_mi_go_hypno;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::rod);
        d.id = Id::rod_mist;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::rod);
        d.id = Id::rod_opening;
        d.spawn_std_range.max = g_dlvl_first_mid_game;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::rod);
        d.id = Id::rod_shockwave;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::rod);
        d.id = Id::rod_unbinding;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::general);
        d.id = Id::medical_bag;
        d.has_std_activate = true;
        d.is_prio_in_backpack_list = true;
        d.base_name = {"Medical Bag", "Medical Bags", "a Medical Bag"};
        d.base_descr = {
                "A portable bag of medical supplies. Can be used to treat "
                "Wounds or Infections."};
        d.weight = Weight::medium;
        d.spawn_std_range = Range(1, g_dlvl_last_mid_game);
        d.is_stackable = false;
        d.character = '%';
        d.color = colors::dark_brown();
        d.tile = gfx::TileId::medical_bag;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::general);
        d.id = Id::lantern;
        d.has_std_activate = true;
        d.is_prio_in_backpack_list = true;
        d.base_name = {
                "Electric Lantern", "Electric Lanterns", "an Electric Lantern"};
        d.base_descr = {
                "A portable light source."};
        d.weight = item::Weight::light;
        d.character = '%';
        d.spawn_std_range = Range(1, 10);
        d.is_stackable = false;
        d.chance_to_incl_in_spawn_list = 100;
        d.tile = gfx::TileId::lantern;
        d.color = colors::yellow();
        d.land_on_hard_snd_msg = "I hear a clanking sound.";
        d.land_on_hard_sfx = audio::SfxId::metal_clank;
        d.native_containers.push_back(terrain::Id::chest);
        d.native_containers.push_back(terrain::Id::cabinet);
        d.native_containers.push_back(terrain::Id::cocoon);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::melee_wpn);
        d.id = Id::pharaoh_staff;
        d.base_name = {
                "Staff of the Pharaohs",
                "",
                "the Staff of the Pharaohs"};
        d.base_descr = {
                "Wielded by rulers in ancient times, this powerful artifact "
                "holds power over those that were once bound to it. "
                "Any mummy beholding the owner will eventually be converted "
                "(10% chance per turn while the weapon is carried).",

                "Also, a devastating curse may fall upon those struck by this weapon "
                "(50% chance to apply doom, greatly reducing the victim's "
                "hit chances, evasion, and searching ability, and also causes "
                "a small chance to fail when casting spells)."};
        d.color = colors::magenta();
        d.weight = Weight::medium;
        d.tile = gfx::TileId::pharaoh_staff;
        d.melee.attack_msgs = {"strike", "strikes"};
        d.melee.dmg = WpnDmg(1, 12);
        d.melee.hit_chance_mod = 0;
        d.melee.miss_sfx = audio::SfxId::miss_medium;
        d.melee.dmg_type = DmgType::blunt;
        d.melee.reach = 2;
        d.ranged.dmg_type = DmgType::blunt;
        d.is_unique = true;
        d.xp_on_found = 20;
        d.value = Value::supreme_treasure;
        d.allow_cursed = true;
        d.chance_to_incl_in_spawn_list = 1;
        d.native_containers.push_back(terrain::Id::tomb);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::melee_wpn);
        d.id = Id::flagellant_whip;
        d.base_name = {
                "Scourge",
                "",
                "a Scourge"};
        d.base_descr = {
                "A brutal whip affixed with sharpened bones and metal spikes.",

                "Victims struck by its flesh-tearing bite may be paralyzed "
                "with pain (20% chance)."};
        d.color = colors::red();
        d.weight = Weight::light;
        d.tile = gfx::TileId::whip_scourge;
        d.melee.attack_msgs = {"strike", "strikes"};
        d.melee.dmg = WpnDmg(1, 10);
        d.melee.hit_chance_mod = 10;
        d.melee.miss_sfx = audio::SfxId::miss_medium;
        d.melee.hit_small_sfx = audio::SfxId::hit_whip_scourge;
        d.melee.hit_medium_sfx = audio::SfxId::hit_whip_scourge;
        d.melee.hit_hard_sfx = audio::SfxId::hit_whip_scourge;
        d.melee.dmg_type = DmgType::slashing;
        d.melee.reach = 2;
        {
                prop::Prop* prop = prop::make(prop::Id::paralyzed);

                prop->set_duration(2);

                d.melee.prop_applied.prop.reset(prop);
                d.melee.prop_applied.pct_chance_to_apply = 20;
        }
        d.ranged.is_throwable_wpn = false;
        d.is_unique = true;
        d.value = Value::supreme_treasure;
        d.chance_to_incl_in_spawn_list = 0;
        d.allow_spawn = false;
        d.native_containers.push_back(terrain::Id::tomb);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::general);
        d.id = Id::onyx_drop;
        d.base_name = {
                "Onyx Drop",
                "",
                "the Onyx Drop"};
        d.base_descr = {
                "Drinking a malign potion also applies its effect "
                "to all nearby creatures (maximum distance is 6)."};
        d.color = colors::violet();
        d.weight = Weight::extra_light;
        d.tile = gfx::TileId::drop;
        d.character = '*';
        d.is_unique = true;
        d.xp_on_found = 20;
        d.value = Value::supreme_treasure;
        d.allow_cursed = true;
        d.chance_to_incl_in_spawn_list = 1;
        d.native_containers.push_back(terrain::Id::tomb);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::general);
        d.id = Id::refl_talisman;
        d.base_name = {
                "Talisman of Reflection",
                "",
                "the Talisman of Reflection"};
        d.base_descr = {
                "Whenever a hostile spell is blocked due to spell resistance, "
                "it is also reflected. The number of turns to regain spell "
                "resistance is halved."};
        d.color = colors::light_blue();
        d.weight = Weight::light;
        d.tile = gfx::TileId::amulet;
        d.character = '"';
        d.is_unique = true;
        d.xp_on_found = 20;
        d.value = Value::supreme_treasure;
        d.allow_cursed = true;
        d.chance_to_incl_in_spawn_list = 1;
        d.native_containers.push_back(terrain::Id::tomb);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::general);
        d.id = Id::resurrect_talisman;
        d.base_name = {
                "Talisman of Resurrection",
                "",
                "the Talisman of Resurrection"};
        d.base_descr = {
                "This powerful charm brings the owner back to life upon bodily "
                "death. The talisman is destroyed in the process however, so "
                "one may only be brought back once."};
        d.color = colors::light_white();
        d.weight = Weight::light;
        d.tile = gfx::TileId::amulet;
        d.character = '"';
        d.is_unique = true;
        d.xp_on_found = 20;
        d.value = Value::supreme_treasure;
        d.allow_cursed = true;
        d.chance_to_incl_in_spawn_list = 1;
        d.native_containers.push_back(terrain::Id::tomb);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::general);
        d.id = Id::tele_ctrl_talisman;
        d.base_name = {
                "Talisman of Teleportation Control",
                "",
                "the Talisman of Teleportation Control"};
        d.base_descr = {
                "Grants the owner the ability to control the destination when "
                "teleporting."};
        d.color = colors::orange();
        d.weight = Weight::light;
        d.tile = gfx::TileId::amulet;
        d.character = '"';
        d.is_unique = true;
        d.xp_on_found = 20;
        d.value = Value::supreme_treasure;
        d.allow_cursed = true;
        d.chance_to_incl_in_spawn_list = 1;
        d.native_containers.push_back(terrain::Id::tomb);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::general);
        d.id = Id::holy_symbol;
        d.base_name = {
                "Holy Symbol",
                "",
                "a Holy Symbol",
        };
        d.base_descr = {
                "A focal point providing strength and guidance for the "
                "spirit and mind. "
                "Praying over the symbol grants 1-4 spirit points, and "
                "resistance against shock and fear for 6-12 turns.",

                "Some time must pass before the prayer is guaranteed to have "
                "an effect again, however it can be attempted before this "
                "time has passed (with 25% chance to succeed). If an early "
                "attempt fails, faith in the symbol is temporarily lost, and "
                "much time must pass before the symbol can be used again."};
        d.color = colors::gold();
        d.weight = Weight::extra_light;
        d.tile = gfx::TileId::holy_symbol;
        d.character = '%';
        d.is_unique = true;
        d.value = Value::supreme_treasure;
        d.has_std_activate = true;
        d.chance_to_incl_in_spawn_list = 0;
        d.allow_spawn = false;
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::general);
        d.id = Id::clockwork;
        d.base_name = {
                "Arcane Clockwork",
                "",
                "the Arcane Clockwork",
        };
        d.base_descr = {
                "A mainspring-powered clockwork of unreal quality and beauty. "
                "When wound up, it causes the owner to move very swiftly for a "
                "brief time."};
        d.color = colors::yellow();
        d.weight = Weight::extra_light;
        d.tile = gfx::TileId::clockwork;
        d.character = '%';
        d.is_unique = true;
        d.xp_on_found = 20;
        d.value = Value::supreme_treasure;
        d.allow_cursed = true;
        d.has_std_activate = true;
        d.chance_to_incl_in_spawn_list = 1;
        d.native_containers.push_back(terrain::Id::chest);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::general);
        d.id = Id::horn_of_malice;
        d.base_name = {
                "Horn of Malice",
                "",
                "the Horn of Malice"};
        d.base_descr = {
                "When blown, this sinister artifact emits a weird resonance "
                "which corrupts the psyche of all those within hearing range "
                "(excluding the horn blower) - causing them to consider all "
                "other creatures with intense hatred and distrust."};
        d.color = colors::gray();
        d.weight = Weight::light;
        d.tile = gfx::TileId::horn;
        d.character = '%';
        d.is_unique = true;
        d.xp_on_found = 20;
        d.value = Value::supreme_treasure;
        d.allow_cursed = true;
        d.has_std_activate = true;
        d.chance_to_incl_in_spawn_list = 1;
        d.native_containers.push_back(terrain::Id::tomb);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::general);
        d.id = Id::horn_of_banishment;
        d.base_name = {
                "Horn of Banishment",
                "",
                "the Horn of Banishment"};
        d.base_descr = {
                "When blown, this instrument forces all magically summoned "
                "creatures within hearing range back to their original realm."};
        d.color = colors::magenta();
        d.weight = Weight::light;
        d.tile = gfx::TileId::horn;
        d.character = '%';
        d.is_unique = true;
        d.xp_on_found = 20;
        d.value = Value::supreme_treasure;
        d.allow_cursed = true;
        d.has_std_activate = true;
        d.chance_to_incl_in_spawn_list = 1;
        d.native_containers.push_back(terrain::Id::tomb);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::melee_wpn);
        d.id = Id::shadow_dagger;
        d.base_name = {
                "Gahana, The Black Dagger",
                "",
                "Gahana, The Black Dagger"};
        d.base_descr = {
                "A pitch black dagger with elaborate ornaments. The blade "
                "appears blurry, as if perpetually covered in a dark haze.",

                "A creature struck by this weapon is cursed to forever dwell "
                "in darkness, or suffer great agony (becomes permanently "
                "light sensitive, and takes +1 extra damage from light). "
                "Creatures which naturally emit light (such as beings of "
                "fire or energy) takes 1-4 irresistible damage instead.",

                "Attacking an unaware opponent with a dagger does +200% damage "
                "(in addition to the normal +50% damage from stealth attacks).",
        };
        d.weight = Weight::light;
        d.tile = gfx::TileId::dagger;
        d.color = colors::violet();
        d.melee.attack_msgs = {"stab", "stabs"};
        d.melee.dmg = WpnDmg(4, 8);
        d.melee.hit_chance_mod = 20;
        d.melee.is_noisy = false;
        d.melee.hit_medium_sfx = audio::SfxId::hit_sharp;
        d.melee.hit_hard_sfx = audio::SfxId::hit_sharp;
        d.melee.miss_sfx = audio::SfxId::miss_light;
        d.melee.dmg_type = DmgType::piercing;
        d.ranged.dmg_type = DmgType::piercing;
        d.is_unique = true;
        d.xp_on_found = 20;
        d.value = Value::supreme_treasure;
        d.allow_cursed = true;
        d.chance_to_incl_in_spawn_list = 1;
        d.native_containers.push_back(terrain::Id::tomb);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::general);
        d.id = Id::orb_of_life;
        d.base_name = {
                "Orb of Life",
                "",
                "the Orb of Life"};
        d.base_descr = {
                "+4 hit points, grants resistance against poison and disease."};
        d.color = colors::light_white();
        d.weight = Weight::light;
        d.tile = gfx::TileId::orb;
        d.character = '"';
        d.is_unique = true;
        d.xp_on_found = 20;
        d.value = Value::supreme_treasure;
        d.allow_cursed = true;
        d.chance_to_incl_in_spawn_list = 1;
        d.native_containers.push_back(terrain::Id::tomb);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::general);
        d.id = Id::necronomicon;
        d.base_name = {
                "Necronomicon",
                "",
                "the Necronomicon"};
        d.base_descr = {
                "This is the dreaded Necronomicon - the Book of the Dead! "
                "Its pages contain much dire knowledge on esoteric matters. "
                "While carried, all spells are cast at a higher skill level, "
                "and it is possible to reach a fourth level, \"Transcendent\".",

                "All shock taken from spell casting is doubled, and "
                "the presence of one who is consulting such knowledge "
                "is felt strongly (-20% stealth, 2% chance per turn of "
                "alerting nearby creatures)."};
        d.color = colors::dark_sepia();
        d.weight = Weight::light;
        d.tile = gfx::TileId::tome;
        d.character = '?';
        d.is_unique = true;
        d.xp_on_found = 20;
        d.value = Value::supreme_treasure;
        d.chance_to_incl_in_spawn_list = 1;
        d.native_containers.push_back(terrain::Id::tomb);
        d.native_containers.push_back(terrain::Id::bookshelf);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::throwing_wpn);
        d.id = Id::zombie_dust;
        d.base_name = {
                "Zombie Dust",
                "Handfuls of Zombie Dust",
                "a handful of Zombie Dust"};
        d.base_descr = {
                "When thrown at a living (non-undead) creature, this powder "
                "causes paralyzation."};
        d.spawn_std_range.max = g_dlvl_last;
        d.weight = Weight::extra_light;
        d.tile = gfx::TileId::zombie_dust;
        d.character = '*';
        d.color = colors::brown();
        d.ranged.dmg = WpnDmg(0, 0);
        d.ranged.throw_hit_chance_mod = 15;
        d.ranged.always_break_on_throw = true;
        d.ranged.effective_range = {-1, -1};
        d.ranged.max_range = 3;
        d.ranged.dmg_type = DmgType::blunt;
        d.max_stack_at_spawn = 1;
        d.main_attack_mode = AttackMode::thrown;
        d.chance_to_incl_in_spawn_list = 35;
        d.native_containers.push_back(terrain::Id::chest);
        d.native_containers.push_back(terrain::Id::cabinet);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::general);
        d.id = Id::witch_eye;
        d.base_name = {"Witch's Eye", "Witch's Eyes", "a Witch's Eye"};
        d.base_descr = {
                "The eye of a powerful witch. Clutching it in one's hand will "
                "temporarily grant magical vision - doors, traps, stairs, and "
                "other locations of interest are detected in the "
                "surrounding area, and the presence of all items and creatures "
                "is revealed."};
        d.type = ItemType::general;
        d.value = item::Value::major_treasure;
        d.weight = Weight::extra_light;
        d.has_std_activate = true;
        d.color = colors::light_green();
        d.tile = gfx::TileId::witch_eye;
        d.character = '%';
        d.max_stack_at_spawn = 1;
        d.chance_to_incl_in_spawn_list = 0;
        d.spawn_std_range = Range(-1, -1);
        d.allow_spawn = false;
        d.is_stackable = false;
        d.land_on_hard_snd_msg = "";
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::general);
        d.id = Id::bone_charm;
        d.base_name = {"Bone Charm", "Bone Charms", "a Bone Charm"};
        d.base_descr = {
                "An old finger bone, carved with tiny symbols.",

                "Snapping it in two "
                "grants protection against harmful spells "
                "for 6-12 turns, or until a spell is blocked.",

                "It also dispels all seen magic traps "
                "(\"strange shape\" on the floor). "
                "For each trap dispelled, 1-6 spirit points are gained, "
                "which may raise spirit above maximum level."};
        d.type = ItemType::general;
        d.value = item::Value::minor_treasure;
        d.weight = Weight::extra_light;
        d.has_std_activate = true;
        d.color = colors::gray_brown();
        d.tile = gfx::TileId::bone_charm;
        d.character = '%';
        d.max_stack_at_spawn = 3;
        d.chance_to_incl_in_spawn_list = 60;
        d.is_stackable = true;
        d.land_on_hard_snd_msg = "";
        d.native_containers.push_back(terrain::Id::chest);
        d.native_containers.push_back(terrain::Id::cabinet);
        d.native_containers.push_back(terrain::Id::cocoon);
        d.native_containers.push_back(terrain::Id::tomb);
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::general);
        d.id = Id::fluctuating_material;
        d.base_name = {
                "Fluctuating Material",
                "Pieces of Fluctuating Material",
                "a Fluctuating Material"};
        d.base_descr = {
                "It is difficult to tell whether it is stone, metal, "
                "or perhaps even something organic. It seems to be "
                "ever-changing, internally twisting, turning, and flowing.",

                "Glancing at the material is like looking into a "
                "kaleidoscope - and it feels like gazing too deep will "
                "transmute the very nature of the observer (choose one trait "
                "to remove, and then pick a new one)."};
        d.type = ItemType::general;
        d.value = item::Value::minor_treasure;
        d.weight = Weight::extra_light;
        d.has_std_activate = true;
        d.color = colors::yellow();
        d.tile = gfx::TileId::fluctuating_material;
        d.character = '%';
        d.max_stack_at_spawn = 1;
        d.chance_to_incl_in_spawn_list = 0;
        d.spawn_std_range = Range(-1, -1);
        d.allow_spawn = false;
        d.is_stackable = true;
        d.land_on_hard_snd_msg = "I hear a thud.";
        g_data[(size_t)d.id] = d;

        reset_data(d, ItemType::general);
        d.id = Id::astral_opium;
        d.base_name = {
                "Astral Opium",
                "Astral Opium Doses",
                "an Astral Opium Dose"};
        d.base_descr = {
                "A drug extracted from plants of extraterrestrial origin. "
                "It leaves the user in a perfectly serene state, free from all "
                "fear.",

                "There is a price for this however, since it causes "
                "intense hallucinogenic delusions, and it is also severely "
                "addictive."};
        d.type = ItemType::general;
        d.is_stackable = true;
        d.chance_to_incl_in_spawn_list = 40;
        d.allow_spawn = true;
        d.max_stack_at_spawn = 2;
        d.value = item::Value::minor_treasure;
        d.weight = Weight::extra_light;
        d.has_std_activate = true;
        d.color = colors::white();
        d.tile = gfx::TileId::astral_opium;
        d.character = '%';
        d.land_on_hard_snd_msg = "I hear a clanking sound.";
        d.native_containers.push_back(terrain::Id::chest);
        d.native_containers.push_back(terrain::Id::cabinet);
        d.native_containers.push_back(terrain::Id::cocoon);
        g_data[(size_t)d.id] = d;

        TRACE_FUNC_END;
}

void cleanup()
{
        TRACE_FUNC_BEGIN;

        for (size_t i = 0; i < (size_t)Id::END; ++i) {
                ItemData& d = g_data[i];

                d.melee.prop_applied = ItemAttackProp();

                d.ranged.prop_applied = ItemAttackProp();
        }

        TRACE_FUNC_END;
}

void save()
{
        for (size_t i = 0; i < (size_t)Id::END; ++i) {
                const ItemData& d = g_data[i];

                saving::put_bool(d.is_identified);
                saving::put_bool(d.is_alignment_known);
                saving::put_bool(d.is_spell_domain_known);
                saving::put_bool(d.is_tried);
                saving::put_bool(d.is_found);
                saving::put_bool(d.allow_spawn);
                saving::put_int(d.chance_to_incl_in_spawn_list);
        }
}

void load()
{
        for (size_t i = 0; i < (size_t)Id::END; ++i) {
                ItemData& d = g_data[i];

                d.is_identified = saving::get_bool();
                d.is_alignment_known = saving::get_bool();
                d.is_spell_domain_known = saving::get_bool();
                d.is_tried = saving::get_bool();
                d.is_found = saving::get_bool();
                d.allow_spawn = saving::get_bool();
                d.chance_to_incl_in_spawn_list = saving::get_int();
        }
}

ItemSetId str_to_item_set_id(const std::string& str)
{
        return s_str_to_item_set_id_map.at(str);
}

Id str_to_intr_item_id(const std::string& str)
{
        return s_str_to_intr_item_id_map.at(str);
}

MeleeData::MeleeData() :
        is_melee_wpn(false),

        hit_chance_mod(0),
        is_noisy(true),

        dmg_type(DmgType::slashing),
        reach(1),
        knocks_back(false),
        can_attack_door_wood(false),
        can_attack_door_gate(false),
        can_attack_corpse(false),
        hit_small_sfx(audio::SfxId::END),
        hit_medium_sfx(audio::SfxId::END),
        hit_hard_sfx(audio::SfxId::END),
        miss_sfx(audio::SfxId::END)
{}

RangedData::RangedData() :
        is_ranged_wpn(false),
        is_throwable_wpn(false),
        is_machine_gun(false),
        is_shotgun(false),
        max_ammo(0),
        hit_chance_mod(0),
        throw_hit_chance_mod(0),
        always_break_on_throw(false),
        effective_range({0, 6}),
        max_range(g_fov_radi_int * 2),
        knocks_back(false),
        ammo_item_id(Id::END),
        dmg_type(DmgType::piercing),
        has_infinite_ammo(false),
        projectile_character('/'),
        projectile_tile(gfx::TileId::projectile_std_front_slash),
        projectile_color(colors::white()),
        projectile_leaves_trail(false),

        snd_vol(SndVol::low),
        makes_ricochet_snd(false),
        attack_sfx(audio::SfxId::END),
        reload_sfx(audio::SfxId::END)

{
}

ArmorData::ArmorData() :
        armor_points(0),
        dmg_to_durability_factor(0.0) {}

ItemData::ItemData() :
        id(Id::END),
        type(ItemType::general),
        is_intr(false),
        has_std_activate(false),
        is_prio_in_backpack_list(false),
        value(Value::normal),
        allow_cursed(false),
        weight(Weight::none),
        is_unique(false),
        allow_spawn(true),
        spawn_std_range(Range(1, g_dlvl_last)),
        max_stack_at_spawn(1),
        chance_to_incl_in_spawn_list(100),
        is_stackable(true),
        is_identified(true),
        is_alignment_known(true),
        is_spell_domain_known(true),
        is_tried(false),
        is_found(false),
        xp_on_found(0),
        character('X'),
        color(colors::white()),
        tile(gfx::TileId::END),
        main_attack_mode(AttackMode::none),
        spell_cast_from_scroll(SpellId::END),
        land_on_hard_snd_msg("I hear a thudding sound."),
        land_on_hard_sfx(audio::SfxId::END),
        allow_display_dmg(true)
{
        for (size_t i = 0; i < (size_t)AbilityId::END; ++i) {
                ability_mods_while_equipped[i] = 0;
        }

        base_descr.clear();
        native_rooms.clear();
        native_containers.clear();
}

}  // namespace item

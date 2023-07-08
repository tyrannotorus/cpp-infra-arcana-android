// =============================================================================
// Copyright 2011-2023 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "item_factory.hpp"

#include <cstddef>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "debug.hpp"
#include "drop.hpp"
#include "global.hpp"
#include "item.hpp"
#include "item_ammo.hpp"
#include "item_armor.hpp"
#include "item_curse.hpp"
#include "item_curse_ids.hpp"
#include "item_data.hpp"
#include "item_device.hpp"
#include "item_explosive.hpp"
#include "item_head.hpp"
#include "item_misc.hpp"
#include "item_potion.hpp"
#include "item_rod.hpp"
#include "item_scroll.hpp"
#include "item_weapon.hpp"
#include "random.hpp"
#include "wpn_dmg.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static bool should_randomize_weapon_dmg(const item::Item& item)
{
        // Randomize the extra damage if the item is a common melee weapon, and
        // extra damage is not already specified.
        const auto& d = item.data();

        return (
                d.melee.is_melee_wpn &&
                !d.ranged.is_ranged_wpn &&
                !d.is_unique &&
                (item.base_melee_dmg() == d.melee.dmg));
}

static void randomize_wpn_dmg(item::Item& item)
{
        // Element corresponds to damage bonus (+0, +1, +2, etc)
        const std::vector<int> weights = {
                100,  // +0
                100,  // +1
                50,   // +2
                25,   // +3
                4,    // +4
                2,    // +5
                1     // +6
        };

        const int extra_dmg = rnd::weighted_choice(weights);

        item.set_melee_plus(extra_dmg);
}

static void randomize_firearm_loaded_ammo(item::Wpn& wpn)
{
        const auto& d = wpn.data();

        if (wpn.data().ranged.max_ammo == 1) {
                wpn.m_ammo_loaded = rnd::coin_toss() ? 1 : 0;
        }
        else {
                // Weapon ammo capacity > 1
                const int ammo_cap = wpn.data().ranged.max_ammo;

                if (d.ranged.is_machine_gun) {
                        // Number of machine gun bullets loaded needs to
                        // be a multiple of the number of projectiles
                        // fired in each burst

                        const int cap_scaled =
                                ammo_cap / g_nr_mg_projectiles;

                        const int min_scaled =
                                cap_scaled / 4;

                        wpn.m_ammo_loaded =
                                rnd::range(min_scaled, cap_scaled) *
                                g_nr_mg_projectiles;
                }
                else {
                        // Not machinegun
                        wpn.m_ammo_loaded = rnd::range(ammo_cap / 4, ammo_cap);
                }
        }
}

static void randomize_medical_supplies(item::MedicalBag& medbag)
{
        medbag.randomize_nr_supplies();
}

static void randomize_lantern_duration(item::Lantern& lantern)
{
        lantern.randomize_duration();
}

// -----------------------------------------------------------------------------
// item
// -----------------------------------------------------------------------------
namespace item
{
Item* make(const Id item_id, const int nr_items)
{
        Item* r = nullptr;

        auto* d = &g_data[(size_t)item_id];

        // Sanity check
        ASSERT(d->id == item_id);

        switch (item_id) {
        case Id::trapezohedron:
                r = new Trapezohedron(d);
                break;

        case Id::sawed_off:
        case Id::pump_shotgun:
        case Id::tommy_gun:
        case Id::pistol:
        case Id::revolver:
        case Id::rifle:
        case Id::spike_gun:
        case Id::rock:
        case Id::thr_knife:
        case Id::iron_spike:
        case Id::dagger:
        case Id::hatchet:
        case Id::club:
        case Id::hammer:
        case Id::machete:
        case Id::axe:
        case Id::pitchfork:
        case Id::spear:
        case Id::sledgehammer:
        case Id::flagellant_whip:
        case Id::spiked_mace:
        case Id::trap_dart:
        case Id::trap_dart_poison:
        case Id::trap_spear:
        case Id::trap_spear_poison:
        case Id::player_kick:
        case Id::player_stomp:
        case Id::player_punch:
        case Id::intr_kick:
        case Id::intr_bite:
        case Id::intr_claw:
        case Id::intr_strike:
        case Id::intr_punch:
        case Id::intr_punch_knockback:
        case Id::intr_headbutt:
        case Id::intr_acid_spit:
        case Id::intr_fire_breath:
        case Id::intr_energy_breath:
        case Id::intr_strangle:
        case Id::intr_ghost_touch:
        case Id::intr_sting:
        case Id::intr_spear_thrust:
        case Id::intr_net_throw:
        case Id::intr_maul:
        case Id::intr_pus_spew:
        case Id::intr_acid_touch:
        case Id::intr_fire_engulf:
        case Id::intr_energy_engulf:
        case Id::intr_spores:
        case Id::intr_web_bola:
                r = new Wpn(d);
                break;

        case Id::morphic_blaster:
                r = new MorphicBlaster(d);
                break;

        case Id::electric_gun:
                r = new ElectricGun(d);
                break;

        case Id::revolver_bullet:
        case Id::rifle_bullet:
        case Id::shotgun_shell:
                r = new Ammo(d);
                break;

        case Id::drum_of_bullets:
        case Id::pistol_mag:
                r = new AmmoMag(d);
                break;

        case Id::zombie_dust:
                r = new ZombieDust(d);
                break;

        case Id::witch_eye:
                r = new WitchEye(d);
                break;

        case Id::bone_charm:
                r = new BoneCharm(d);
                break;

                // case Id::flask_of_damning:
                //         r = new FlaskOfDamning(d);
                //         break;

                // case Id::obsidian_charm:
                //         r = new ObsidianCharm(d);
                //         break;

        case Id::fluctuating_material:
                r = new FluctuatingMaterial(d);
                break;

                // case Id::bat_wing_salve:
                //         r = new BatWingSalve(d);
                //         break;

        case Id::astral_opium:
                r = new AstralOpium(d);
                break;

        case Id::dynamite:
                r = new Dynamite(d);
                break;

        case Id::flare:
                r = new Flare(d);
                break;

        case Id::molotov:
                r = new Molotov(d);
                break;

        case Id::smoke_grenade:
                r = new SmokeGrenade(d);
                break;

        case Id::player_ghoul_claw:
                r = new PlayerGhoulClaw(d);
                break;

        case Id::intr_raven_peck:
                r = new RavenPeck(d);
                break;

        case Id::intr_vampiric_bite:
                r = new VampiricBite(d);
                break;

        case Id::intr_mind_leech_sting:
                r = new MindLeechSting(d);
                break;

        case Id::intr_dust_engulf:
                r = new DustEngulf(d);
                break;

        case Id::intr_snake_venom_spit:
                r = new SnakeVenomSpit(d);
                break;

        case Id::armor_flak_jacket:
        case Id::armor_leather_jacket:
        case Id::armor_iron_suit:
                r = new Armor(d);
                break;

        case Id::armor_asb_suit:
                r = new ArmorAsbSuit(d);
                break;

        case Id::armor_mi_go:
                r = new ArmorMiGo(d);
                break;

        case Id::gas_mask:
                r = new GasMask(d);
                break;

        case Id::torture_collar:
                r = new TortureCollar(d);
                break;

        case Id::scroll_aura_of_decay:
        case Id::scroll_cataclysm:
        case Id::scroll_telep:
        case Id::scroll_pestilence:
        case Id::scroll_enfeeble:
        case Id::scroll_slow:
        case Id::scroll_terrify:
        case Id::scroll_bless:
        case Id::scroll_darkbolt:
        case Id::scroll_aza_gaze:
        case Id::scroll_control_object:
        case Id::scroll_resistance:
        case Id::scroll_light:
        case Id::scroll_spectral_wpns:
        case Id::scroll_transmut:
        case Id::scroll_heal:
        case Id::scroll_invis:
        case Id::scroll_see_invis:
        case Id::scroll_premonition:
        case Id::scroll_erudition:
        case Id::scroll_haste:
        case Id::scroll_spell_shield:
        case Id::scroll_thorns:
        case Id::scroll_blood_temper:
        case Id::scroll_sacrifice_life:
        case Id::scroll_crimson_passage:
                r = new scroll::Scroll(d);
                break;

        case Id::potion_vitality:
                r = new potion::Vitality(d);
                break;

        case Id::potion_spirit:
                r = new potion::Spirit(d);
                break;

        case Id::potion_blindness:
                r = new potion::Blindness(d);
                break;

        case Id::potion_fortitude:
                r = new potion::Fortitude(d);
                break;

        case Id::potion_paralyze:
                r = new potion::Paral(d);
                break;

        case Id::potion_r_elec:
                r = new potion::RElec(d);
                break;

        case Id::potion_conf:
                r = new potion::Conf(d);
                break;

        case Id::potion_poison:
                r = new potion::Poison(d);
                break;

        case Id::potion_insight:
                r = new potion::Insight(d);
                break;

        case Id::potion_r_fire:
                r = new potion::RFire(d);
                break;

        case Id::potion_curing:
                r = new potion::Curing(d);
                break;

        case Id::potion_descent:
                r = new potion::Descent(d);
                break;

        case Id::device_blaster:
                r = new device::Blaster(d);
                break;

        case Id::device_rejuvenator:
                r = new device::Rejuvenator(d);
                break;

        case Id::device_translocator:
                r = new device::Translocator(d);
                break;

        case Id::device_sentry_drone:
                r = new device::SentryDrone(d);
                break;

        case Id::device_deafening:
                r = new device::Deafening(d);
                break;

        case Id::device_force_field:
                r = new device::ForceField(d);
                break;

        case Id::lantern:
                r = new item::Lantern(d);
                break;

        case Id::rod_curing:
                r = new rod::Curing(d);
                break;

        case Id::rod_opening:
                r = new rod::Opening(d);
                break;

        case Id::rod_bless:
                r = new rod::Bless(d);
                break;

        case Id::rod_cloud_minds:
                r = new rod::CloudMinds(d);
                break;

        case Id::rod_shockwave:
                r = new rod::Shockwave(d);
                break;

        case Id::medical_bag:
                r = new MedicalBag(d);
                break;

        case Id::pharaoh_staff:
                r = new PharaohStaff(d);
                break;

        case Id::onyx_drop:
                r = new Item(d);
                break;

        case Id::refl_talisman:
                r = new ReflTalisman(d);
                break;

        case Id::resurrect_talisman:
                r = new ResurrectTalisman(d);
                break;

        case Id::tele_ctrl_talisman:
                r = new TeleCtrlTalisman(d);
                break;

        case Id::horn_of_malice:
                r = new HornOfMalice(d);
                break;

        case Id::horn_of_banishment:
                r = new HornOfBanishment(d);
                break;

        case Id::holy_symbol:
                r = new HolySymbol(d);
                break;

        case Id::clockwork:
                r = new Clockwork(d);
                break;

        case Id::shadow_dagger:
                r = new ShadowDagger(d);
                break;

        case Id::orb_of_life:
                r = new OrbOfLife(d);
                break;

        case Id::necronomicon:
                r = new Necronomicon(d);
                break;

        case Id::END:
                break;
        }

        if (!r) {
                return nullptr;
        }

        // Sanity check number of items (non-stackable items should never be set
        // to anything other than one item)
        if (!r->data().is_stackable && (nr_items != 1)) {
                TRACE << "Specified number of items ("
                      << nr_items
                      << ") != 1 for "
                      << "non-stackable item: "
                      << (int)d->id << ", "
                      << r->name(ItemNameType::plain)
                      << std::endl;

                ASSERT(false);
        }

        r->m_nr_items = nr_items;

        if (d->is_unique) {
                d->allow_spawn = false;
        }

        return r;
}

void randomize_item_properties(Item& item)
{
        const auto& d = item.data();

        ASSERT(
                (d.type != ItemType::melee_wpn_intr) &&
                (d.type != ItemType::ranged_wpn_intr));

        if (should_randomize_weapon_dmg(item)) {
                randomize_wpn_dmg(item);
        }

        if (d.ranged.is_ranged_wpn && !d.ranged.has_infinite_ammo) {
                auto& wpn = static_cast<Wpn&>(item);
                randomize_firearm_loaded_ammo(wpn);
        }

        if (d.is_stackable) {
                item.m_nr_items = rnd::range(1, d.max_stack_at_spawn);
        }

        if (d.id == Id::medical_bag) {
                auto& medbag = static_cast<MedicalBag&>(item);
                randomize_medical_supplies(medbag);
        }

        if (d.id == Id::lantern) {
                auto& lantern = static_cast<item::Lantern&>(item);
                randomize_lantern_duration(lantern);
        }

        const int cursed_one_in_n = 3;

        if (d.allow_cursed && rnd::one_in(cursed_one_in_n)) {
                auto curse = item_curse::try_make_random_free_curse(item);

                if (curse.id() != item_curse::Id::END) {
                        item.set_curse(std::move(curse));
                }
        }
}

Item* make_item_on_floor(const Id item_id, const P& pos)
{
        auto* item = make(item_id);

        randomize_item_properties(*item);

        item_drop::drop_item_on_map(pos, *item);

        return item;
}

Item* copy_item(const Item& item_to_copy)
{
        auto* new_item = make(item_to_copy.id());

        *new_item = item_to_copy;

        return new_item;
}

}  // namespace item

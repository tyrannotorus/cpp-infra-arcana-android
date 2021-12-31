// =============================================================================
// Copyright 2011-2021 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "actor_items.hpp"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <vector>

#include "actor.hpp"
#include "actor_data.hpp"
#include "actor_mon.hpp"
#include "actor_player.hpp"
#include "debug.hpp"
#include "game.hpp"
#include "global.hpp"
#include "inventory.hpp"
#include "item.hpp"
#include "item_data.hpp"
#include "item_factory.hpp"
#include "map.hpp"
#include "player_bon.hpp"
#include "player_spells.hpp"
#include "random.hpp"
#include "spells.hpp"
#include "wpn_dmg.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static void learn_spell_player(const SpellId spell)
{
        player_spells::learn_spell(spell, Verbose::no);

        // Also identify and "find" the corresponding scroll (if one exists)
        for (auto& d : item::g_data)
        {
                if (d.spell_cast_from_scroll != spell)
                {
                        continue;
                }

                std::unique_ptr<item::Item> temp_scroll(item::make(d.id));

                temp_scroll->identify(Verbose::no);

                game::incr_player_xp(
                        temp_scroll->data().xp_on_found,
                        Verbose::no);

                temp_scroll->data().is_found = true;

                break;
        }
}

static void make_for_player_exorcist()
{
        auto& inv = map::g_player->m_inv;

        auto* const hatchet = item::make(item::Id::hatchet);

        inv.put_in_slot(
                SlotId::wpn,
                hatchet,
                Verbose::no);

        inv.put_in_slot(
                SlotId::wpn_alt,
                item::make(item::Id::revolver),
                Verbose::no);

        auto* revolver_bullets = item::make(item::Id::revolver_bullet);

        revolver_bullets->m_nr_items = 8;

        inv.put_in_backpack(revolver_bullets);

        inv.put_in_backpack(item::make(item::Id::iron_spike, 6));

        map::g_player->set_unarmed_wpn(
                static_cast<item::Wpn*>(
                        item::make(item::Id::player_punch)));

        inv.put_in_backpack(item::make(item::Id::dynamite, 1));
        inv.put_in_backpack(item::make(item::Id::molotov, 1));
        inv.put_in_backpack(item::make(item::Id::medical_bag));
        inv.put_in_backpack(item::make(item::Id::lantern));
        inv.put_in_backpack(item::make(item::Id::holy_symbol));

        learn_spell_player(SpellId::purge);
}

static void make_for_player_occultist_common()
{
        auto& inv = map::g_player->m_inv;

        inv.put_in_slot(
                SlotId::wpn,
                item::make(item::Id::hatchet),
                Verbose::no);

        inv.put_in_slot(
                SlotId::wpn_alt,
                item::make(item::Id::revolver),
                Verbose::no);

        auto* revolver_bullets = item::make(item::Id::revolver_bullet);

        revolver_bullets->m_nr_items = 8;

        inv.put_in_backpack(revolver_bullets);

        inv.put_in_slot(
                SlotId::body,
                item::make(item::Id::armor_leather_jacket),
                Verbose::no);

        auto* spirit_pot = item::make(item::Id::potion_spirit);

        spirit_pot->identify(Verbose::no);

        game::incr_player_xp(
                spirit_pot->data().xp_on_found,
                Verbose::no);

        spirit_pot->data().is_found = true;

        inv.put_in_backpack(spirit_pot);

        map::g_player->set_unarmed_wpn(
                static_cast<item::Wpn*>(
                        item::make(item::Id::player_punch)));

        inv.put_in_backpack(item::make(item::Id::dynamite, 1));
        inv.put_in_backpack(item::make(item::Id::molotov, 1));
        inv.put_in_backpack(item::make(item::Id::medical_bag));
        inv.put_in_backpack(item::make(item::Id::lantern));
}

static void make_for_player_occultist_transmut()
{
        learn_spell_player(SpellId::haste);
        learn_spell_player(SpellId::transmut);
}

static void make_for_player_occultist_clairv()
{
        learn_spell_player(SpellId::premonition);
        learn_spell_player(SpellId::identify);
}

static void make_for_player_occultist_ench()
{
        learn_spell_player(SpellId::terrify);
        learn_spell_player(SpellId::heal);
}

static void make_for_player_occultist_invoc()
{
        learn_spell_player(SpellId::darkbolt);
        learn_spell_player(SpellId::aura_of_decay);
}

// static void make_for_player_occultist_summon()
// {
//         learn_spell_player(SpellId::summon);
// }

static void make_for_player_rogue()
{
        auto& inv = map::g_player->m_inv;

        auto* const dagger = item::make(item::Id::dagger);

        inv.put_in_slot(
                SlotId::wpn,
                dagger,
                Verbose::no);

        inv.put_in_slot(
                SlotId::wpn_alt,
                item::make(item::Id::revolver),
                Verbose::no);

        auto* revolver_bullets = item::make(item::Id::revolver_bullet);

        revolver_bullets->m_nr_items = 8;

        inv.put_in_backpack(revolver_bullets);

        inv.put_in_slot(
                SlotId::body,
                item::make(item::Id::armor_leather_jacket),
                Verbose::no);

        inv.put_in_backpack(item::make(item::Id::iron_spike, 12));

        auto* rod_cloud_minds = item::make(item::Id::rod_cloud_minds);

        rod_cloud_minds->identify(Verbose::no);

        game::incr_player_xp(
                rod_cloud_minds->data().xp_on_found,
                Verbose::no);

        rod_cloud_minds->data().is_found = true;

        inv.put_in_backpack(rod_cloud_minds);

        map::g_player->set_unarmed_wpn(
                static_cast<item::Wpn*>(
                        item::make(item::Id::player_punch)));

        inv.put_in_backpack(item::make(item::Id::dynamite, 2));
        inv.put_in_backpack(item::make(item::Id::molotov, 2));
        inv.put_in_backpack(item::make(item::Id::medical_bag));
        inv.put_in_backpack(item::make(item::Id::lantern));

        auto* const throwing_knives = item::make(item::Id::thr_knife, 12);

        inv.put_in_backpack(throwing_knives);

        map::g_player->m_last_thrown_item = throwing_knives;
}

static void make_for_player_war_vet()
{
        auto& inv = map::g_player->m_inv;

        inv.put_in_slot(
                SlotId::wpn,
                item::make(item::Id::machete),
                Verbose::no);

        for (int i = 0; i < 3; ++i)
        {
                inv.put_in_backpack(item::make(item::Id::pistol_mag));
        }

        inv.put_in_slot(
                SlotId::wpn_alt,
                item::make(item::Id::pistol),
                Verbose::no);

        inv.put_in_slot(
                SlotId::body,
                item::make(item::Id::armor_flak_jacket),
                Verbose::no);

        map::g_player->set_unarmed_wpn(
                static_cast<item::Wpn*>(
                        item::make(item::Id::player_punch)));

        inv.put_in_backpack(item::make(item::Id::dynamite, 2));
        inv.put_in_backpack(item::make(item::Id::molotov, 2));
        inv.put_in_backpack(item::make(item::Id::smoke_grenade, 4));
        inv.put_in_backpack(item::make(item::Id::flare, 2));
        inv.put_in_backpack(item::make(item::Id::medical_bag));
        inv.put_in_backpack(item::make(item::Id::lantern));
        inv.put_in_backpack(item::make(item::Id::gas_mask));

        auto* const throwing_knives = item::make(item::Id::thr_knife, 6);

        inv.put_in_backpack(throwing_knives);

        map::g_player->m_last_thrown_item = throwing_knives;
}

static void make_for_player_ghoul()
{
        map::g_player->set_unarmed_wpn(
                static_cast<item::Wpn*>(
                        item::make(item::Id::player_ghoul_claw)));
}

static void make_for_player()
{
        switch (player_bon::bg())
        {
        case Bg::exorcist:
        {
                make_for_player_exorcist();
        }
        break;

        case Bg::occultist:
                make_for_player_occultist_common();

                switch (player_bon::occultist_domain())
                {
                case OccultistDomain::clairvoyant:
                        make_for_player_occultist_clairv();
                        break;

                case OccultistDomain::enchanter:
                        make_for_player_occultist_ench();
                        break;

                case OccultistDomain::invoker:
                        make_for_player_occultist_invoc();
                        break;

                        // case OccultistDomain::summoner:
                        //         make_for_player_occultist_summoner();
                        //         break;

                case OccultistDomain::transmuter:
                        make_for_player_occultist_transmut();
                        break;

                case OccultistDomain::END:
                        ASSERT(false);
                        break;

                }  // Occultist domain switch
                break;

        case Bg::rogue:
                make_for_player_rogue();
                break;

        case Bg::war_vet:
                make_for_player_war_vet();
                break;

        case Bg::ghoul:
                make_for_player_ghoul();
                break;

        case Bg::END:
                break;
        }  // Background switch
}

static void make_random_item_to_backpack(
        actor::Actor& actor,
        std::vector<item::Id>& item_id_bucket)
{
        if (item_id_bucket.empty())
        {
                return;
        }

        std::vector<int> weights;
        weights.reserve(item_id_bucket.size());

        for (const auto id : item_id_bucket)
        {
                // NOTE: Reusing the "chance to include in spawn list" data for
                // the weight when doing a weighted random choice here.

                // TODO: Consider if items should always be spawned with a
                // weighted choice, instead of randomly discarding items from
                // the list (actor spawning already uses weights instead)

                const int weight =
                        item::g_data[(size_t)id].chance_to_incl_in_spawn_list;

                ASSERT(weight != 0);

                weights.push_back(weight);
        }

        const int idx = rnd::weighted_choice(weights);

        const item::Id item_id = item_id_bucket[idx];

        auto* item = item::make(item_id);

        actor.m_inv.put_in_backpack(item);
}

static void make_item_set_treasure(
        const item::Value value,
        actor::Actor& actor)
{
        std::vector<item::Id> item_bucket;

        for (int i = 0; i < (int)item::Id::END; ++i)
        {
                const auto& d = item::g_data[i];

                if ((d.chance_to_incl_in_spawn_list > 0) &&
                    (d.value == value))
                {
                        item_bucket.push_back((item::Id)i);
                }
        }

        make_random_item_to_backpack(actor, item_bucket);
}

static void make_item_set_minor_treasure(actor::Actor& actor)
{
        make_item_set_treasure(item::Value::minor_treasure, actor);
}

static void make_item_set_rare_treasure(actor::Actor& actor)
{
        make_item_set_treasure(item::Value::rare_treasure, actor);
}

static void make_item_set_supreme_treasure(actor::Actor& actor)
{
        make_item_set_treasure(item::Value::supreme_treasure, actor);
}

static void make_item_set_firearm(actor::Actor& actor)
{
        Inventory& inv = actor.m_inv;

        // On early dungeon levels, lean heavily towards revolvers and pistols
        const bool is_low_dlvl = map::g_dlvl < 4;

        int revolver_weight = 0;
        int pistol_weight = 0;

        if (is_low_dlvl)
        {
                revolver_weight = 12;
                pistol_weight = 8;
        }
        else
        {
                revolver_weight = 3;
                pistol_weight = 3;
        }

        const int pump_shotgun_weight = 3;
        const int sawed_off_shotgun_weight = 3;
        const int rifle_weight = 1;
        const int machine_gun_weight = 1;

        std::vector<int> weights = {
                revolver_weight,
                pistol_weight,
                pump_shotgun_weight,
                sawed_off_shotgun_weight,
                rifle_weight,
                machine_gun_weight};

        const int choice = rnd::weighted_choice(weights);

        switch (choice)
        {
        case 0:
        {
                // Revolver
                auto* item = item::make(item::Id::revolver);
                auto* wpn = static_cast<item::Wpn*>(item);

                const int ammo_cap = wpn->data().ranged.max_ammo;
                wpn->m_ammo_loaded = rnd::range(ammo_cap / 2, ammo_cap);

                inv.put_in_slot(SlotId::wpn, item, Verbose::no);

                item = item::make(item::Id::revolver_bullet);
                item->m_nr_items = rnd::range(1, 6);

                inv.put_in_backpack(item);
        }
        break;

        case 1:
        {
                // Pistol
                auto* item = item::make(item::Id::pistol);
                auto* wpn = static_cast<item::Wpn*>(item);

                const int ammo_cap = wpn->data().ranged.max_ammo;
                wpn->m_ammo_loaded = rnd::range(ammo_cap / 2, ammo_cap);

                inv.put_in_slot(SlotId::wpn, item, Verbose::no);

                if (rnd::coin_toss())
                {
                        inv.put_in_backpack(item::make(item::Id::pistol_mag));
                }
        }
        break;

        case 2:
        {
                // Pump shotgun
                auto* item = item::make(item::Id::pump_shotgun);
                auto* wpn = static_cast<item::Wpn*>(item);

                const int ammo_cap = wpn->data().ranged.max_ammo;
                wpn->m_ammo_loaded = rnd::range(ammo_cap / 2, ammo_cap);

                inv.put_in_slot(SlotId::wpn, item, Verbose::no);

                item = item::make(item::Id::shotgun_shell);
                item->m_nr_items = rnd::range(1, 6);

                inv.put_in_backpack(item);
        }
        break;

        case 3:
        {
                // Sawed-off shotgun
                inv.put_in_slot(
                        SlotId::wpn,
                        item::make(item::Id::sawed_off),
                        Verbose::no);

                auto* item = item::make(item::Id::shotgun_shell);
                item->m_nr_items = rnd::range(1, 6);

                inv.put_in_backpack(item);
        }
        break;

        case 4:
        {
                // Rifle
                auto* item = item::make(item::Id::rifle);
                auto* wpn = static_cast<item::Wpn*>(item);

                const int ammo_cap = wpn->data().ranged.max_ammo;
                wpn->m_ammo_loaded = rnd::range(ammo_cap / 2, ammo_cap);

                inv.put_in_slot(SlotId::wpn, item, Verbose::no);

                item = item::make(item::Id::rifle_bullet);
                item->m_nr_items = rnd::range(1, 6);

                inv.put_in_backpack(item);
        }
        break;

        case 5:
        {
                // Tommy Gun

                // Number of bullets loaded needs to be a multiple of the number
                // of projectiles fired in each burst
                auto* item = item::make(item::Id::machine_gun);
                auto* const wpn = static_cast<item::Wpn*>(item);

                const auto cap_scaled =
                        wpn->data().ranged.max_ammo /
                        g_nr_mg_projectiles;

                const int min_scaled = cap_scaled / 2;

                wpn->m_ammo_loaded =
                        rnd::range(min_scaled, cap_scaled) *
                        g_nr_mg_projectiles;

                inv.put_in_slot(SlotId::wpn, item, Verbose::no);
        }
        break;

        default:
        {
                ASSERT(false);
        }
        break;
        }
}

static void make_item_set_spike_gun(actor::Actor& actor)
{
        Inventory& inv = actor.m_inv;

        {
                auto* item = item::make(item::Id::spike_gun);

                auto* wpn = static_cast<item::Wpn*>(item);

                const int ammo_cap = wpn->data().ranged.max_ammo;

                wpn->m_ammo_loaded = rnd::range(ammo_cap / 2, ammo_cap);

                inv.put_in_slot(SlotId::wpn, item, Verbose::no);
        }

        {
                auto* item = item::make(item::Id::iron_spike);

                item->m_nr_items = rnd::range(1, 6);

                inv.put_in_backpack(item);
        }
}

static void make_item_set_zealot_spiked_mace(actor::Actor& actor)
{
        auto* item = item::make(item::Id::spiked_mace);

        item->set_melee_plus(0);

        actor.m_inv.put_in_slot(SlotId::wpn, item, Verbose::no);
}

static void make_item_set_witches_eye(actor::Actor& actor)
{
        if (player_bon::is_bg(Bg::occultist) &&
            (player_bon::occultist_domain() == OccultistDomain::clairvoyant))
        {
                // Player is clairvoyant occultist, and thus already has
                // permanent magic searching - this does not work well with
                // providing temporary magic searching - just discard the item.
                return;
        }

        auto* item = item::make(item::Id::witch_eye);

        actor.m_inv.put_in_backpack(item);
}

static void make_item_set_fluctuating_material(actor::Actor& actor)
{
        auto* item = item::make(item::Id::fluctuating_material);

        actor.m_inv.put_in_backpack(item);
}

static void make_item_set_priest_dagger(actor::Actor& actor)
{
        auto* item = item::make(item::Id::dagger);

        const std::vector<int> weights = {6, 3, 1};

        item->set_melee_plus(rnd::weighted_choice(weights) + 1);

        actor.m_inv.put_in_slot(SlotId::wpn, item, Verbose::no);
}

static void make_item_set_mi_go_gun(actor::Actor& actor)
{
        actor.m_inv.put_in_slot(
                SlotId::wpn,
                item::make(item::Id::mi_go_gun),
                Verbose::no);
}

static void make_item_set_mi_go_armor(actor::Actor& actor)
{
        actor.m_inv.put_in_slot(
                SlotId::body,
                item::make(item::Id::armor_mi_go),
                Verbose::no);
}

static void make_item_set_high_priest_guard_war_vet(actor::Actor& actor)
{
        actor.m_inv.put_in_slot(
                SlotId::wpn,
                item::make(item::Id::machine_gun),
                Verbose::no);
}

static void make_item_set_high_priest_guard_rogue(actor::Actor& actor)
{
        auto* const item = item::make(item::Id::machete);

        item->set_melee_plus(1);

        actor.m_inv.put_in_slot(
                SlotId::wpn,
                item,
                Verbose::no);
}

static void make_monster_item_sets(actor::Actor& actor)
{
        for (const auto& item_set : actor.m_data->item_sets)
        {
                if (!rnd::percent(item_set.pct_chance_to_spawn))
                {
                        continue;
                }

                const int nr = item_set.nr_spawned_range.roll();

                for (int i = 0; i < nr; ++i)
                {
                        switch (item_set.item_set_id)
                        {
                        case item::ItemSetId::minor_treasure:
                                make_item_set_minor_treasure(actor);
                                break;

                        case item::ItemSetId::rare_treasure:
                                make_item_set_rare_treasure(actor);
                                break;

                        case item::ItemSetId::supreme_treasure:
                                make_item_set_supreme_treasure(actor);
                                break;

                        case item::ItemSetId::firearm:
                                make_item_set_firearm(actor);
                                break;

                        case item::ItemSetId::spike_gun:
                                make_item_set_spike_gun(actor);
                                break;

                        case item::ItemSetId::witch_eye:
                                make_item_set_witches_eye(actor);
                                break;

                        case item::ItemSetId::fluctuating_material:
                                make_item_set_fluctuating_material(actor);
                                break;

                        case item::ItemSetId::zealot_spiked_mace:
                                make_item_set_zealot_spiked_mace(actor);
                                break;

                        case item::ItemSetId::priest_dagger:
                                make_item_set_priest_dagger(actor);
                                break;

                        case item::ItemSetId::mi_go_gun:
                                make_item_set_mi_go_gun(actor);
                                break;

                        case item::ItemSetId::mi_go_armor:
                                make_item_set_mi_go_armor(actor);
                                break;

                        case item::ItemSetId::high_priest_guard_war_vet:
                                make_item_set_high_priest_guard_war_vet(actor);
                                break;

                        case item::ItemSetId::high_priest_guard_rogue:
                                make_item_set_high_priest_guard_rogue(actor);
                                break;
                        }
                }
        }
}

static void make_monster_intr_attacks(actor::Actor& actor)
{
        for (auto& intr_attack : actor.m_data->intr_attacks)
        {
                auto* item = item::make(intr_attack->item_id);

                // Override damage with the damage in the intrinsic attack data
                // (we always override both melee and ranged damage - this
                // doesn't matter, since only one damage type will be used and
                // the other will have no effect)
                const WpnDmg range(1, intr_attack->dmg);

                item->set_base_melee_dmg(range);
                item->set_base_ranged_dmg(range);

                actor.m_inv.put_in_intrinsics(item);
        }
}

static void make_monster_spells(actor::Actor& actor)
{
        ASSERT(!actor::is_player(&actor));

        if (actor::is_player(&actor))
        {
                return;
        }

        auto* const mon = static_cast<actor::Mon*>(&actor);

        for (auto& spell_data : actor.m_data->spells)
        {
                if (!rnd::percent(spell_data.pct_chance_to_know))
                {
                        continue;
                }

                auto* const spell = spells::make(spell_data.spell_id);

                mon->add_spell(spell_data.spell_skill, spell);
        }
}

static void make_for_monster(actor::Actor& actor)
{
        make_monster_item_sets(actor);

        make_monster_intr_attacks(actor);

        make_monster_spells(actor);
}

// -----------------------------------------------------------------------------
// actor_items
// -----------------------------------------------------------------------------
namespace actor_items
{
void make_for_actor(actor::Actor& actor)
{
        if (actor::is_player(&actor))
        {
                make_for_player();
        }
        else
        {
                make_for_monster(actor);
        }
}

}  // namespace actor_items

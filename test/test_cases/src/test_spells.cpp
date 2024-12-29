// =============================================================================
// Copyright 2011-2024 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "actor.hpp"
#include "actor_factory.hpp"
#include "catch.hpp"
#include "game_time.hpp"
#include "global.hpp"
#include "item_data.hpp"
#include "item_factory.hpp"
#include "item_scroll.hpp"
#include "map.hpp"
#include "player_spells.hpp"
#include "pos.hpp"
#include "property_data.hpp"
#include "property_factory.hpp"
#include "property_handler.hpp"
#include "spells.hpp"
#include "terrain.hpp"
#include "terrain_door.hpp"
#include "terrain_factory.hpp"
#include "test_utils.hpp"

TEST_CASE("Test opening spell effect")
{
        test_utils::init_all();

        const P wood_door_pos(3, 3);
        const P warded_door_pos(10, 10);
        const P lever_1_pos(50, 50);
        const P lever_2_pos(75, 75);

        auto* const wood_door =
                static_cast<terrain::Door*>(
                        terrain::make(terrain::Id::door, wood_door_pos));

        wood_door->init_type_and_state(
                terrain::DoorType::wood,
                terrain::DoorSpawnState::closed);

        auto* const warded_door =
                static_cast<terrain::Door*>(
                        terrain::make(terrain::Id::door, warded_door_pos));

        warded_door->init_type_and_state(
                terrain::DoorType::wood,
                terrain::DoorSpawnState::warded);

        auto* const crystal_1 =
                static_cast<terrain::CrystalKey*>(
                        terrain::make(terrain::Id::crystal_key, lever_1_pos));

        auto* const crystal_2 =
                static_cast<terrain::CrystalKey*>(
                        terrain::make(terrain::Id::crystal_key, lever_2_pos));

        map::update_terrain(wood_door);
        map::update_terrain(warded_door);
        map::update_terrain(crystal_1);
        map::update_terrain(crystal_2);

        crystal_1->set_linked_door(*warded_door);
        crystal_2->set_linked_door(*warded_door);
        crystal_1->add_sibbling(crystal_2);
        crystal_2->add_sibbling(crystal_1);

        REQUIRE(!wood_door->is_open());
        REQUIRE(!warded_door->is_open());
        REQUIRE(crystal_1->is_active());
        REQUIRE(crystal_2->is_active());

        const terrain::DidOpen did_open_wood_door =
                spells::run_opening_spell_effect_at(
                        wood_door_pos,
                        SpellSkill::master);

        REQUIRE(did_open_wood_door == terrain::DidOpen::yes);

        REQUIRE(wood_door->is_open());
        REQUIRE(!warded_door->is_open());
        REQUIRE(crystal_1->is_active());
        REQUIRE(crystal_2->is_active());

        const terrain::DidOpen did_open_warded_door =
                spells::run_opening_spell_effect_at(
                        warded_door_pos,
                        SpellSkill::master);

        REQUIRE(did_open_warded_door == terrain::DidOpen::yes);

        REQUIRE(wood_door->is_open());
        REQUIRE(map::g_terrain.at(warded_door_pos)->id() != terrain::Id::door);
        REQUIRE(!crystal_1->is_active());
        REQUIRE(!crystal_2->is_active());
}

TEST_CASE("Test spell bonuses for learned spells")
{
        test_utils::init_all();

        auto& player = *map::g_player;

        player.m_pos.set(5, 5);

        player_spells::learn_spell(SpellId::heal, Verbose::no);

        REQUIRE(player_spells::is_spell_learned(SpellId::heal));

        REQUIRE(
                player_spells::spell_skill(SpellId::heal) ==
                SpellSkill::basic);

        // With altar bonus
        map::update_terrain(
                terrain::make(
                        terrain::Id::altar,
                        player.m_pos.with_x_offset(1)));

        REQUIRE(
                player_spells::spell_skill(SpellId::heal) ==
                SpellSkill::expert);

        // With eruditon bonus
        player.m_properties.apply(prop::make(prop::Id::erudition));

        REQUIRE(
                player_spells::spell_skill(SpellId::heal) ==
                SpellSkill::master);

        // Remove altar bonus
        map::update_terrain(
                terrain::make(
                        terrain::Id::wall,
                        player.m_pos.with_x_offset(1)));

        REQUIRE(
                player_spells::spell_skill(SpellId::heal) ==
                SpellSkill::expert);

        // Necronomicon bonus
        map::g_player->m_inv.put_in_backpack(
                item::make(item::Id::necronomicon));

        REQUIRE(
                player_spells::spell_skill(SpellId::heal) ==
                SpellSkill::master);

        // Re-add the altar bonus
        map::update_terrain(
                terrain::make(
                        terrain::Id::altar,
                        player.m_pos.with_x_offset(1)));

        REQUIRE(
                player_spells::spell_skill(SpellId::heal) ==
                SpellSkill::transcendent);

        // Increase spell skill to master level
        player_spells::incr_spell_skill(SpellId::heal, Verbose::no);
        player_spells::incr_spell_skill(SpellId::heal, Verbose::no);

        REQUIRE(
                player_spells::spell_skill(SpellId::heal) ==
                SpellSkill::transcendent);

        // Remove necronomicon
        map::g_player->m_inv.drop_all_non_intrinsic(map::g_player->m_pos);

        // Even with intrinsic master level + erudition + altar, the total skill
        // should still only be master without necronomicon.
        REQUIRE(
                player_spells::spell_skill(SpellId::heal) ==
                SpellSkill::master);
}

TEST_CASE("Test spell bonuses for manuscripts")
{
        // TODO: Add check for Transcendent level casting (with Necronomicon).

        // NOTE: There is no functionality to get a spell skill from a scroll,
        // so instead we actually cast the spell and check the effect. This is
        // somewhat ugly though since it depends on game design.

        test_utils::init_all();

        auto& player = *map::g_player;

        player.m_pos.set(5, 5);

        auto* const item = item::make(item::Id::scroll_heal);
        auto* const scroll = static_cast<scroll::Scroll*>(item);

        // Casting healing from manuscript (expert level) should clear
        // poisoning, but not deafness.
        player.m_properties.apply(prop::make(prop::Id::poisoned));
        player.m_properties.apply(prop::make(prop::Id::deaf));

        REQUIRE(player.m_properties.has(prop::Id::poisoned));
        REQUIRE(player.m_properties.has(prop::Id::deaf));

        scroll->activate(map::g_player);

        REQUIRE(!player.m_properties.has(prop::Id::poisoned));
        REQUIRE(player.m_properties.has(prop::Id::deaf));

        // Casting healing from manuscript at altar (master level) should clear
        // both poisoning and deafness.
        map::update_terrain(
                terrain::make(
                        terrain::Id::altar,
                        player.m_pos.with_x_offset(1)));

        player.m_properties.apply(prop::make(prop::Id::poisoned));
        player.m_properties.apply(prop::make(prop::Id::deaf));

        game_time::g_allow_tick = true;

        scroll->activate(map::g_player);

        REQUIRE(!player.m_properties.has(prop::Id::poisoned));
        REQUIRE(!player.m_properties.has(prop::Id::deaf));

        // Remove the altar
        map::update_terrain(
                terrain::make(
                        terrain::Id::wall,
                        player.m_pos.with_x_offset(1)));

        // Casting healing from manuscript with erudition (master level) should
        // clear both poisoning and deafness.
        player.m_properties.apply(prop::make(prop::Id::erudition));

        player.m_properties.apply(prop::make(prop::Id::poisoned));
        player.m_properties.apply(prop::make(prop::Id::deaf));

        game_time::g_allow_tick = true;

        scroll->activate(map::g_player);

        REQUIRE(!player.m_properties.has(prop::Id::poisoned));
        REQUIRE(!player.m_properties.has(prop::Id::deaf));
}

TEST_CASE("Test spell shield")
{
        test_utils::init_all();

        map::update_terrain(terrain::make(terrain::Id::floor, {10, 10}));
        map::update_terrain(terrain::make(terrain::Id::floor, {11, 10}));

        map::g_player->m_pos.set(10, 10);

        SECTION("Temporary spell shield")
        {
                auto* const mon = actor::make("MON_ZOMBIE", {11, 10});

                map::update_vision();

                mon->m_properties.apply(prop::make(prop::Id::r_spell));

                const auto* const darkbolt = spells::make(SpellId::darkbolt);

                REQUIRE(mon->m_properties.has(prop::Id::r_spell));

                darkbolt->run_effect(map::g_player, SpellSkill::basic, {mon});

                REQUIRE(mon->m_hp == actor::max_hp(*mon));
                REQUIRE(!mon->m_properties.has(prop::Id::r_spell));

                darkbolt->run_effect(map::g_player, SpellSkill::basic, {mon});

                REQUIRE(mon->m_hp < actor::max_hp(*mon));
                REQUIRE(!mon->m_properties.has(prop::Id::r_spell));
        }

        SECTION("Natural spell shield")
        {
                auto* const mon = actor::make("MON_KHAGA", {11, 10});

                map::update_vision();

                const auto* const darkbolt = spells::make(SpellId::darkbolt);

                REQUIRE(mon->m_properties.has(prop::Id::r_spell));

                darkbolt->run_effect(map::g_player, SpellSkill::basic, {mon});

                REQUIRE(mon->m_hp == actor::max_hp(*mon));
                REQUIRE(mon->m_properties.has(prop::Id::r_spell));

                darkbolt->run_effect(map::g_player, SpellSkill::basic, {mon});

                REQUIRE(mon->m_hp == actor::max_hp(*mon));
                REQUIRE(mon->m_properties.has(prop::Id::r_spell));
        }
}

TEST_CASE("Test spell reflection hits correct creature")
{
        // Verify that a reflected Darkbolt hits the caster, and not the closest
        // creature.

        test_utils::init_all();

        map::update_terrain(terrain::make(terrain::Id::floor, {10, 10}));
        map::update_terrain(terrain::make(terrain::Id::floor, {11, 10}));
        map::update_terrain(terrain::make(terrain::Id::floor, {12, 10}));

        map::g_player->m_pos.set(10, 10);

        auto* const mon_1 = actor::make("MON_ZOMBIE", {11, 10});
        auto* const mon_2 = actor::make("MON_ZOMBIE", {12, 10});

        map::update_vision();

        map::g_player->m_properties.apply(prop::make(prop::Id::r_spell));

        map::g_player->m_properties.apply(prop::make(prop::Id::spell_reflect));

        // Cast darkbolt from monster 2 on the player.
        const auto* const darkbolt = spells::make(SpellId::darkbolt);

        darkbolt->run_effect(mon_2, SpellSkill::basic, {map::g_player});

        // Only monster 2 should be hit (not the closest monster).
        REQUIRE(map::g_player->m_hp == actor::max_hp(*map::g_player));
        REQUIRE(mon_1->m_hp == actor::max_hp(*mon_1));
        REQUIRE(mon_2->m_hp < actor::max_hp(*mon_2));

        actor::restore_hp(*mon_2, 999);

        // Cast darkbolt again, now it should hit the player (no spell shield).
        darkbolt->run_effect(mon_2, SpellSkill::basic, {map::g_player});

        REQUIRE(map::g_player->m_hp <= actor::max_hp(*map::g_player));
        REQUIRE(mon_1->m_hp == actor::max_hp(*mon_1));
        REQUIRE(mon_2->m_hp == actor::max_hp(*mon_2));
}

TEST_CASE("Test reflected knockback spell blocked by caster spell shield")
{
        // Verify that if the caster has spell shield, a reflected knockback
        // spell is blocked by the spell shield.

        test_utils::init_all();

        map::update_terrain(terrain::make(terrain::Id::floor, {9, 10}));
        map::update_terrain(terrain::make(terrain::Id::floor, {10, 10}));
        map::update_terrain(terrain::make(terrain::Id::floor, {11, 10}));
        map::update_terrain(terrain::make(terrain::Id::floor, {12, 10}));

        map::g_player->m_pos.set(10, 10);

        auto* const mon = actor::make("MON_ZOMBIE", {11, 10});

        map::update_vision();

        map::g_player->m_properties.apply(prop::make(prop::Id::r_spell));

        map::g_player->m_properties.apply(prop::make(prop::Id::spell_reflect));

        mon->m_properties.apply(prop::make(prop::Id::r_spell));

        mon->m_ai_state.is_target_seen = true;

        // Cast knockback from monster 2 on the player.
        const auto* const knockback = spells::make(SpellId::knockback);

        knockback->run_effect(mon, SpellSkill::basic, {map::g_player});

        // Neither the player nor the monster should have been hit by the spell,
        // but both should have lost spell shield.
        REQUIRE(map::g_player->m_pos == P(10, 10));
        REQUIRE(mon->m_pos == P(11, 10));

        REQUIRE(!map::g_player->m_properties.has(prop::Id::r_spell));
        REQUIRE(!mon->m_properties.has(prop::Id::r_spell));

        // Re-apply spell shield on the player and cast the spell again.
        map::g_player->m_properties.apply(prop::make(prop::Id::r_spell));

        knockback->run_effect(mon, SpellSkill::basic, {map::g_player});

        // Now the spell should have hit the monster.
        REQUIRE(mon->m_pos == P(12, 10));

        REQUIRE(!mon->m_properties.has(prop::Id::r_spell));
}

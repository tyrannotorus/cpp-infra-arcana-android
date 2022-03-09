// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "actor.hpp"
#include "actor_factory.hpp"
#include "actor_player.hpp"
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
#include "test_utils.hpp"

TEST_CASE("Test opening spell effect")
{
        test_utils::init_all();

        const P wood_door_pos(3, 3);
        const P metal_door_pos(10, 10);
        const P lever_1_pos(50, 50);
        const P lever_2_pos(75, 75);

        auto* const wood_door =
                new terrain::Door(
                        wood_door_pos,
                        nullptr,
                        terrain::DoorType::wood,
                        terrain::DoorSpawnState::closed);

        auto* const metal_door =
                new terrain::Door(
                        metal_door_pos,
                        nullptr,
                        terrain::DoorType::metal,
                        terrain::DoorSpawnState::closed);

        auto* const lever_1 = new terrain::Lever(lever_1_pos);
        auto* const lever_2 = new terrain::Lever(lever_2_pos);

        map::put(wood_door);
        map::put(metal_door);
        map::put(lever_1);
        map::put(lever_2);

        lever_1->set_linked_terrain(*metal_door);
        lever_2->set_linked_terrain(*metal_door);
        lever_1->add_sibbling(lever_2);
        lever_2->add_sibbling(lever_1);

        REQUIRE(!wood_door->is_open());
        REQUIRE(!metal_door->is_open());
        REQUIRE(lever_1->is_left_pos());
        REQUIRE(lever_2->is_left_pos());

        const auto did_open_wood_door =
                spells::run_opening_spell_effect_at(
                        wood_door_pos,
                        SpellSkill::master);

        REQUIRE(did_open_wood_door == terrain::DidOpen::yes);

        REQUIRE(wood_door->is_open());
        REQUIRE(!metal_door->is_open());
        REQUIRE(lever_1->is_left_pos());
        REQUIRE(lever_2->is_left_pos());

        const auto did_open_metal_door =
                spells::run_opening_spell_effect_at(
                        metal_door_pos,
                        SpellSkill::master);

        REQUIRE(did_open_metal_door == terrain::DidOpen::yes);

        REQUIRE(wood_door->is_open());
        REQUIRE(metal_door->is_open());
        REQUIRE(!lever_1->is_left_pos());
        REQUIRE(!lever_2->is_left_pos());
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
        map::put(new terrain::Altar(player.m_pos.with_x_offset(1)));

        REQUIRE(
                player_spells::spell_skill(SpellId::heal) ==
                SpellSkill::expert);

        // With eruditon bonus
        player.m_properties.apply(property_factory::make(PropId::erudition));

        REQUIRE(
                player_spells::spell_skill(SpellId::heal) ==
                SpellSkill::master);

        // Remove altar bonus
        map::put(new terrain::Wall(player.m_pos.with_x_offset(1)));

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
        map::put(new terrain::Altar(player.m_pos.with_x_offset(1)));

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

        // Casting healing from manuscript (expert level) should clear disease,
        // but not poison.
        player.m_properties.apply(property_factory::make(PropId::diseased));
        player.m_properties.apply(property_factory::make(PropId::poisoned));

        REQUIRE(player.m_properties.has(PropId::diseased));
        REQUIRE(player.m_properties.has(PropId::poisoned));

        scroll->activate(map::g_player);

        REQUIRE(!player.m_properties.has(PropId::diseased));
        REQUIRE(player.m_properties.has(PropId::poisoned));

        // Casting healing from manuscript at altar (master level) should clear
        // both disease and poison.
        map::put(new terrain::Altar(player.m_pos.with_x_offset(1)));

        player.m_properties.apply(property_factory::make(PropId::diseased));
        player.m_properties.apply(property_factory::make(PropId::poisoned));

        game_time::g_allow_tick = true;

        scroll->activate(map::g_player);

        REQUIRE(!player.m_properties.has(PropId::diseased));
        REQUIRE(!player.m_properties.has(PropId::poisoned));

        // Remove the altar
        map::put(new terrain::Wall(player.m_pos.with_x_offset(1)));

        // Casting healing from manuscript with erudition (master level) should
        // clear both disease and poison.
        player.m_properties.apply(
                property_factory::make(PropId::erudition));

        player.m_properties.apply(property_factory::make(PropId::diseased));
        player.m_properties.apply(property_factory::make(PropId::poisoned));

        game_time::g_allow_tick = true;

        scroll->activate(map::g_player);

        REQUIRE(!player.m_properties.has(PropId::diseased));
        REQUIRE(!player.m_properties.has(PropId::poisoned));
}

TEST_CASE("Test spell shield")
{
        test_utils::init_all();

        map::put(new terrain::Floor({10, 10}));
        map::put(new terrain::Floor({11, 10}));

        map::g_player->m_pos.set(10, 10);

        SECTION("Temporary spell shield")
        {
                auto* const mon = actor::make(actor::Id::zombie, {11, 10});

                map::update_vision();

                mon->m_properties.apply(
                        property_factory::make(
                                PropId::r_spell));

                const auto* const darkbolt = spells::make(SpellId::darkbolt);

                REQUIRE(mon->m_properties.has(PropId::r_spell));

                darkbolt->run_effect(map::g_player, SpellSkill::basic, {mon});

                REQUIRE(mon->m_hp == actor::max_hp(*mon));
                REQUIRE(!mon->m_properties.has(PropId::r_spell));

                darkbolt->run_effect(map::g_player, SpellSkill::basic, {mon});

                REQUIRE(mon->m_hp < actor::max_hp(*mon));
                REQUIRE(!mon->m_properties.has(PropId::r_spell));
        }

        SECTION("Natural spell shield")
        {
                auto* const mon = actor::make(actor::Id::khaga, {11, 10});

                map::update_vision();

                const auto* const darkbolt = spells::make(SpellId::darkbolt);

                REQUIRE(mon->m_properties.has(PropId::r_spell));

                darkbolt->run_effect(map::g_player, SpellSkill::basic, {mon});

                REQUIRE(mon->m_hp == actor::max_hp(*mon));
                REQUIRE(mon->m_properties.has(PropId::r_spell));

                darkbolt->run_effect(map::g_player, SpellSkill::basic, {mon});

                REQUIRE(mon->m_hp == actor::max_hp(*mon));
                REQUIRE(mon->m_properties.has(PropId::r_spell));
        }
}

TEST_CASE("Test spell reflection hits correct creature")
{
        // Verify that a reflected Darkbolt hits the caster, and not the closest
        // creature.

        test_utils::init_all();

        map::put(new terrain::Floor({10, 10}));
        map::put(new terrain::Floor({11, 10}));
        map::put(new terrain::Floor({12, 10}));

        map::g_player->m_pos.set(10, 10);

        auto* const mon_1 = actor::make(actor::Id::zombie, {11, 10});
        auto* const mon_2 = actor::make(actor::Id::zombie, {12, 10});

        map::update_vision();

        map::g_player->m_properties.apply(
                property_factory::make(
                        PropId::r_spell));

        map::g_player->m_properties.apply(
                property_factory::make(
                        PropId::spell_reflect));

        // Cast darkbolt from monster 2 on the player.
        const auto* const darkbolt = spells::make(SpellId::darkbolt);

        darkbolt->run_effect(mon_2, SpellSkill::basic, {map::g_player});

        // Only monster 2 should be hit (not the closest monster).
        REQUIRE(map::g_player->m_hp == actor::max_hp(*map::g_player));
        REQUIRE(mon_1->m_hp == actor::max_hp(*mon_1));
        REQUIRE(mon_2->m_hp < actor::max_hp(*mon_2));

        mon_2->restore_hp(999);

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

        map::put(new terrain::Floor({9, 10}));
        map::put(new terrain::Floor({10, 10}));
        map::put(new terrain::Floor({11, 10}));
        map::put(new terrain::Floor({12, 10}));

        map::g_player->m_pos.set(10, 10);

        auto* const mon = actor::make(actor::Id::zombie, {11, 10});

        map::update_vision();

        map::g_player->m_properties.apply(
                property_factory::make(
                        PropId::r_spell));

        map::g_player->m_properties.apply(
                property_factory::make(
                        PropId::spell_reflect));

        mon->m_properties.apply(
                property_factory::make(
                        PropId::r_spell));

        mon->m_ai_state.is_target_seen = true;

        // Cast knockback from monster 2 on the player.
        const auto* const knockback = spells::make(SpellId::knockback);

        knockback->run_effect(mon, SpellSkill::basic, {map::g_player});

        // Neither the player nor the monster should have been hit by the spell,
        // but both should have lost spell shield.
        REQUIRE(map::g_player->m_pos == P(10, 10));
        REQUIRE(mon->m_pos == P(11, 10));

        REQUIRE(!map::g_player->m_properties.has(PropId::r_spell));
        REQUIRE(!mon->m_properties.has(PropId::r_spell));

        // Re-apply spell shield on the player and cast the spell again.
        map::g_player->m_properties.apply(
                property_factory::make(
                        PropId::r_spell));

        knockback->run_effect(mon, SpellSkill::basic, {map::g_player});

        // Now the spell should have hit the monster.
        REQUIRE(mon->m_pos == P(12, 10));

        REQUIRE(!mon->m_properties.has(PropId::r_spell));
}

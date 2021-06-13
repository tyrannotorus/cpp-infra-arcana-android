// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "actor.hpp"
#include "actor_factory.hpp"
#include "actor_player.hpp"
#include "catch.hpp"
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

        map::put(new terrain::Altar(player.m_pos.with_x_offset(1)));

        REQUIRE(
                player_spells::spell_skill(SpellId::heal) ==
                SpellSkill::expert);

        player.m_properties.apply(property_factory::make(PropId::erudition));

        REQUIRE(
                player_spells::spell_skill(SpellId::heal) ==
                SpellSkill::master);

        map::put(new terrain::Wall(player.m_pos.with_x_offset(1)));

        REQUIRE(
                player_spells::spell_skill(SpellId::heal) ==
                SpellSkill::expert);
}

TEST_CASE("Test spell bonuses for manuscripts")
{
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

        scroll->activate(map::g_player);

        REQUIRE(!player.m_properties.has(PropId::diseased));
        REQUIRE(!player.m_properties.has(PropId::poisoned));
}

TEST_CASE("Test spell shield")
{
        test_utils::init_all();

        const P p0(10, 10);
        const P p1(11, 10);

        map::put(new terrain::Floor(p0));
        map::put(new terrain::Floor(p1));

        map::g_player->m_pos = p0;

        SECTION("Temporary spell shield")
        {
                auto* const mon = actor::make(actor::Id::zombie, p1);

                map::update_vision();

                mon->m_properties.apply(
                        property_factory::make(PropId::r_spell));

                const auto* const darkbolt = spells::make(SpellId::darkbolt);

                REQUIRE(mon->m_properties.has(PropId::r_spell));

                darkbolt->run_effect(map::g_player, SpellSkill::basic);

                REQUIRE(mon->m_hp == actor::max_hp(*mon));
                REQUIRE(!mon->m_properties.has(PropId::r_spell));

                darkbolt->run_effect(map::g_player, SpellSkill::basic);

                REQUIRE(mon->m_hp < actor::max_hp(*mon));
                REQUIRE(!mon->m_properties.has(PropId::r_spell));
        }

        SECTION("Natural spell shield")
        {
                auto* const mon = actor::make(actor::Id::khaga, p1);

                map::update_vision();

                const auto* const darkbolt = spells::make(SpellId::darkbolt);

                REQUIRE(mon->m_properties.has(PropId::r_spell));

                darkbolt->run_effect(map::g_player, SpellSkill::basic);

                REQUIRE(mon->m_hp == actor::max_hp(*mon));
                REQUIRE(mon->m_properties.has(PropId::r_spell));

                darkbolt->run_effect(map::g_player, SpellSkill::basic);

                REQUIRE(mon->m_hp == actor::max_hp(*mon));
                REQUIRE(mon->m_properties.has(PropId::r_spell));
        }
}

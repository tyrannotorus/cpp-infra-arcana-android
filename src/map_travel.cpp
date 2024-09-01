// =============================================================================
// Copyright 2011-2024 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "map_travel.hpp"

#include <cstddef>
#include <iterator>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "actor.hpp"
#include "actor_player_state.hpp"
#include "debug.hpp"
#include "draw_map.hpp"
#include "game.hpp"
#include "game_time.hpp"
#include "global.hpp"
#include "insanity.hpp"
#include "io.hpp"
#include "map.hpp"
#include "map_builder.hpp"
#include "map_controller.hpp"
#include "minimap.hpp"
#include "msg_log.hpp"
#include "property.hpp"
#include "property_data.hpp"
#include "property_factory.hpp"
#include "property_handler.hpp"
#include "random.hpp"
#include "saving.hpp"
#include "terrain.hpp"
#include "viewport.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
#ifndef NDEBUG
static int s_estimated_avail_xp_items = 0;
static int s_estimated_avail_xp_terrain = 0;
static int s_estimated_avail_xp_monsters = 0;
static int s_estimated_avail_xp_descend = 0;
std::set<item::Id> s_item_ids_estimated_xp_for;
std::set<terrain::Id> s_pylon_ids_estimated_xp_for;
std::set<std::string> s_mon_ids_estimated_xp_for;
#endif  // NDEBUG

static std::vector<MapType> s_map_list;

static void trigger_insanity_sympts_for_descent()
{
        // Phobia of deep places
        if (insanity::has_sympt(InsSymptId::phobia_deep)) {
                msg_log::add("I am plagued by my phobia of deep places!");

                map::g_player->m_properties.apply(prop::make(prop::Id::terrified));
        }

        // Babbling
        for (const auto* const sympt : insanity::active_sympts()) {
                if (sympt->id() == InsSymptId::babbling) {
                        static_cast<const InsBabbling*>(sympt)->babble();
                }
        }
}

#ifndef NDEBUG
static void update_estimated_total_avail_xp()
{
        //
        // DESCEND
        //
        s_estimated_avail_xp_descend += g_xp_on_new_dlvl;

        //
        // ITEMS
        //

        std::vector<const item::Item*> items;

        auto has_estimated_for_item = [](const item::Id id) {
                return s_item_ids_estimated_xp_for.find(id) != s_item_ids_estimated_xp_for.end();
        };

        // Collect items on floor.
        for (const item::Item* const item : map::g_items) {
                if (item) {
                        items.push_back(item);
                }
        }

        // Collect items in monster "backpacks".
        for (actor::Actor* const actor : game_time::g_actors) {
                if (!actor::is_player(actor)) {
                        items.insert(
                                std::end(items),
                                std::cbegin(actor->m_inv.m_backpack),
                                std::cend(actor->m_inv.m_backpack));
                }
        }

        // Collect items in item container terrains (chests, ...).
        for (const terrain::Terrain* const terrain : map::g_terrain) {
                items.insert(
                        std::end(items),
                        std::cbegin(terrain->m_item_container.items()),
                        std::cend(terrain->m_item_container.items()));
        }

        for (const item::Item* const item : items) {
                if (item->id() == item::Id::potion_insight) {
                        s_estimated_avail_xp_items += g_xp_on_drink_insight_potion;
                }

                if (item->data().xp_on_found <= 0) {
                        continue;
                }

                if (has_estimated_for_item(item->id())) {
                        continue;
                }

                s_item_ids_estimated_xp_for.insert(item->id());

                s_estimated_avail_xp_items += item->data().xp_on_found;
        }

        //
        // MONSTERS
        //

        auto has_estimated_for_mon = [](const std::string id) {
                return s_mon_ids_estimated_xp_for.find(id) != s_mon_ids_estimated_xp_for.end();
        };

        for (const actor::Actor* const actor : game_time::g_actors) {
                if (actor::is_player(actor) || has_estimated_for_mon(actor->id())) {
                        continue;
                }

                s_mon_ids_estimated_xp_for.insert(actor->id());

                s_estimated_avail_xp_monsters +=
                        game::mon_shock_lvl_to_xp(
                                actor->m_data->mon_shock_lvl);
        }

        //
        // TERRAIN
        //

        auto has_estimated_for_terrain = [](const terrain::Id id) {
                return s_pylon_ids_estimated_xp_for.find(id) != s_pylon_ids_estimated_xp_for.end();
        };

        for (const terrain::Terrain* const terrain : map::g_terrain) {
                switch (terrain->id()) {
                case terrain::Id::monolith: {
                        s_estimated_avail_xp_terrain += g_xp_on_activate_monolith;
                } break;

                case terrain::Id::crystal_key: {
                        s_estimated_avail_xp_terrain += g_xp_on_deactivate_crystal_key;
                } break;

                case terrain::Id::door: {
                        if (terrain->is_hidden()) {
                                s_estimated_avail_xp_terrain += g_xp_on_reveal_door;
                        }
                } break;

                case terrain::Id::trap: {
                        if (terrain->is_hidden()) {
                                s_estimated_avail_xp_terrain += g_xp_on_reveal_trap;
                        }
                } break;

                case terrain::Id::pylon: {
                        if (!has_estimated_for_terrain(terrain->id())) {
                                s_pylon_ids_estimated_xp_for.insert(terrain::Id());

                                s_estimated_avail_xp_terrain += g_xp_on_identify_pylon;
                        }
                } break;

                case terrain::Id::fountain: {
                        const auto* const fountain =
                                static_cast<const terrain::Fountain*>(terrain);

                        if (fountain->effect() == terrain::FountainEffect::xp) {
                                // Estimating 3 quaffs available (sometimes it
                                // can be much more).
                                s_estimated_avail_xp_terrain +=
                                        g_xp_on_drink_from_xp_fountain * 3;

                                static int bla = 0;
                                bla += g_xp_on_drink_from_xp_fountain * 3;
                                TRACE << "### " << bla << std::endl;
                        }
                }

                default: {
                } break;
                }

                if (terrain->can_be_studied()) {
                        s_estimated_avail_xp_terrain += g_xp_on_study_inscription;
                }
        }
}
#endif  // NDEBUG

// -----------------------------------------------------------------------------
// map_travel
// -----------------------------------------------------------------------------
namespace map_travel
{
void init()
{
#ifndef NDEBUG
        s_estimated_avail_xp_items = 0;
        s_estimated_avail_xp_terrain = 0;
        s_estimated_avail_xp_monsters = 0;
        s_estimated_avail_xp_descend = 0;
        s_item_ids_estimated_xp_for.clear();
        s_pylon_ids_estimated_xp_for.clear();
        s_mon_ids_estimated_xp_for.clear();
#endif  // NDEBUG

        // Forest + dungeon + boss + trapezohedron
        const size_t nr_lvl_tot = g_dlvl_last + 3;

        s_map_list = std::vector<MapType>(nr_lvl_tot, MapType::std);

        if (rnd::one_in(3)) {
                const int deep_one_lvl_nr =
                        rnd::range(
                                g_dlvl_first_mid_game + 4,
                                g_dlvl_last_mid_game - 2);

                s_map_list[deep_one_lvl_nr] = MapType::deep_one_lair;
        }

        if (rnd::one_in(8)) {
                s_map_list[g_dlvl_last_mid_game - 1] = MapType::rat_cave;
        }

        // Mi-Go outpost
        s_map_list[g_dlvl_last_mid_game] = MapType::mi_go_outpost;

        // Pharaoh chamber
        s_map_list[g_dlvl_first_late_game] = MapType::egypt;

        s_map_list[g_dlvl_first_late_game + 1] = MapType::magic_pool;

        s_map_list[g_dlvl_last + 1] = MapType::high_priest;

        s_map_list[g_dlvl_last + 2] = MapType::trapez;
}

void save()
{
        saving::put_int((int)s_map_list.size());

        for (const MapType type : s_map_list) {
                saving::put_int((int)type);
        }
}

void load()
{
        const int nr_maps = saving::get_int();

        s_map_list.resize((size_t)nr_maps);

        for (auto& type : s_map_list) {
                type = (MapType)saving::get_int();
        }
}

void go_to_nxt()
{
        TRACE_FUNC_BEGIN;

        TRACE << "Leaving dungeon level '" << map::g_dlvl << "'" << std::endl;

        minimap::clear();

        s_map_list.erase(std::begin(s_map_list));

        const auto map_type = s_map_list.front();

        ++map::g_dlvl;

        const auto map_builder = map_builder::make(map_type);

        map_builder->build();

        if (map::g_player->m_properties.has(prop::Id::descend)) {
                msg_log::add("My sinking feeling disappears.");

                map::g_player->m_properties.end_prop(
                        prop::Id::descend,
                        prop::PropEndConfig(
                                prop::PropEndAllowCallEndHook::no,
                                prop::PropEndAllowMsg::no,
                                prop::PropEndAllowHistoricMsg::no));
        }

        game_time::g_is_magic_descend_nxt_std_turn = false;

        actor::player_state::g_target = nullptr;

        viewport::show(map::g_player->m_pos, viewport::ForceCentering::yes);

        map::update_vision();

        map::g_player->restore_shock(999, true);

        map::g_player->update_tmp_shock();

        msg_log::add("I have discovered a new area.");

        // NOTE: When the "intro level" is skipped, "go_to_nxt" is called when
        // the game starts - so no XP is missed in that case (same thing when
        // loading the game)
        game::incr_player_xp(g_xp_on_new_dlvl, Verbose::yes);

        map::g_player->on_new_dlvl_reached();

        game::add_history_event(
                "Reached dungeon level " +
                std::to_string(map::g_dlvl));

        trigger_insanity_sympts_for_descent();

        if (map_control::g_controller) {
                map_control::g_controller->on_enter();
        }

#ifndef NDEBUG
        update_estimated_total_avail_xp();

        const int estimated_avail_xp_tot =
                s_estimated_avail_xp_items +
                s_estimated_avail_xp_monsters +
                s_estimated_avail_xp_terrain +
                s_estimated_avail_xp_descend;

        TRACE << "Estimated total XP available current run:" << std::endl;
        TRACE << "Items    : " << s_estimated_avail_xp_items << std::endl;
        TRACE << "Monsters : " << s_estimated_avail_xp_monsters << std::endl;
        TRACE << "Terrain  : " << s_estimated_avail_xp_terrain << std::endl;
        TRACE << "Descend  : " << s_estimated_avail_xp_descend << std::endl;
        TRACE << "TOTAL    : " << estimated_avail_xp_tot << std::endl;
#endif  // NDEBUG

        TRACE
                << "Dungeon level '" << map::g_dlvl << "' ready"
                << std::endl
                << "Map type is: '" << (int)map_type << "'" << std::endl;

        TRACE_FUNC_END;
}

}  // namespace map_travel

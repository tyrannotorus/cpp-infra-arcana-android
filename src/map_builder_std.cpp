// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <memory>
#include <ostream>
#include <vector>

#include "actor_player.hpp"
#include "array2.hpp"
#include "colors.hpp"
#include "debug.hpp"
#include "game_time.hpp"
#include "global.hpp"
#include "map.hpp"
#include "map_builder.hpp"
#include "map_controller.hpp"
#include "map_parsing.hpp"
#include "mapgen.hpp"
#include "panel.hpp"
#include "populate_items.hpp"
#include "populate_monsters.hpp"
#include "populate_traps.hpp"
#include "pos.hpp"
#include "random.hpp"
#include "room.hpp"
#include "state.hpp"
#include "terrain.hpp"
#include "terrain_data.hpp"
#include "terrain_door.hpp"
#include "terrain_event.hpp"

// For map generation demo
#ifndef NDEBUG
#include "init.hpp"
#include "io.hpp"
#include "query.hpp"
#endif  // NDEBUG

// -----------------------------------------------------------------------------
// MapBuilderStd
// -----------------------------------------------------------------------------
bool MapBuilderStd::build_specific()
{
        TRACE_FUNC_BEGIN;

        // TODO: Using hard coded values for now
        if (map::g_dlvl >= g_dlvl_first_late_game)
        {
                // Make late game levels slightly bigger
                map::reset({54, 54});
        }
        else
        {
                map::reset({48, 48});
        }

        mapgen::g_is_map_valid = true;

        mapgen::g_door_proposals.resize(map::dims());

        // NOTE: This must be called before any rooms are created
        room_factory::init_room_bucket();

        TRACE << "Init regions" << std::endl;

        const int split_x_interval = map::w() / 3;
        const int split_y_interval = map::h() / 3;

        const int split_x1 = split_x_interval;
        const int split_x2 = (split_x_interval * 2) + 1;

        const int split_y1 = split_y_interval;
        const int split_y2 = split_y_interval * 2;

        std::vector<int> x0_list = {
                1,
                split_x1 + 1,
                split_x2 + 1};

        std::vector<int> x1_list = {
                split_x1 - 1,
                split_x2 - 1,
                map::w() - 2};

        std::vector<int> y0_list = {
                1,
                split_y1 + 1,
                split_y2 + 1};

        std::vector<int> y1_list = {
                split_y1 - 1,
                split_y2 - 1,
                map::h() - 2};

        Region regions[3][3];

        for (int x_region = 0; x_region < 3; ++x_region)
        {
                const int x0 = x0_list[x_region];
                const int x1 = x1_list[x_region];

                for (int y_region = 0; y_region < 3; ++y_region)
                {
                        const int y0 = y0_list[y_region];
                        const int y1 = y1_list[y_region];

                        regions[x_region][y_region] = Region({x0, y0, x1, y1});
                }
        }

        if (!mapgen::g_is_map_valid)
        {
                return false;
        }

        // ---------------------------------------------------------------------
        // Reserve regions for a "river"
        // ---------------------------------------------------------------------
        const int river_one_in_n = 12;

        if (map::g_dlvl >= g_dlvl_first_mid_game &&
            rnd::one_in(river_one_in_n))
        {
                mapgen::reserve_river(regions);
        }

        if (!mapgen::g_is_map_valid)
        {
                return false;
        }

        // ---------------------------------------------------------------------
        // Merge some regions
        // ---------------------------------------------------------------------
        mapgen::merge_regions(regions);

        if (!mapgen::g_is_map_valid)
        {
                return false;
        }

        // ---------------------------------------------------------------------
        // Make main rooms
        // ---------------------------------------------------------------------
        TRACE << "Making main rooms" << std::endl;

        for (int x = 0; x < 3; ++x)
        {
                for (int y = 0; y < 3; ++y)
                {
                        auto& region = regions[x][y];

                        if (!region.main_room && region.is_free)
                        {
                                mapgen::make_room(region);
                        }
                }
        }

        if (!mapgen::g_is_map_valid)
        {
                return false;
        }

        // ---------------------------------------------------------------------
        // If there are too few rooms at this point, invalidate the map
        // ---------------------------------------------------------------------
        const size_t min_nr_rooms = 5;

        if (map::g_room_list.size() < min_nr_rooms)
        {
                mapgen::g_is_map_valid = false;

                return false;
        }

        // ---------------------------------------------------------------------
        // Make auxiliary rooms
        // ---------------------------------------------------------------------
#ifndef NDEBUG
        if (init::g_is_demo_mapgen)
        {
                io::cover_panel(Panel::log);
                states::draw();
                io::draw_text(
                        "Press any key to make aux rooms...",
                        Panel::screen,
                        P(0, 0),
                        colors::white());
                io::update_screen();
                query::wait_for_key_press();
                io::cover_panel(Panel::log);
        }
#endif  // NDEBUG

        mapgen::make_aux_rooms(regions);

        if (!mapgen::g_is_map_valid)
        {
                return false;
        }

        // ---------------------------------------------------------------------
        // BSP split rooms
        // ---------------------------------------------------------------------
        if (map::g_dlvl <= g_dlvl_last_mid_game)
        {
                mapgen::bsp_split_rooms();
        }

        if (!mapgen::g_is_map_valid)
        {
                return false;
        }

        // ---------------------------------------------------------------------
        // Make sub-rooms
        // ---------------------------------------------------------------------
        if (map::g_dlvl <= g_dlvl_last_mid_game)
        {
#ifndef NDEBUG
                if (init::g_is_demo_mapgen)
                {
                        io::cover_panel(Panel::log);
                        states::draw();
                        io::draw_text(
                                "Press any key to make sub rooms...",
                                Panel::screen,
                                P(0, 0),
                                colors::white());
                        io::update_screen();
                        query::wait_for_key_press();
                        io::cover_panel(Panel::log);
                }
#endif  // NDEBUG

                mapgen::make_sub_rooms();
        }

        if (!mapgen::g_is_map_valid)
        {
                return false;
        }

        // ---------------------------------------------------------------------
        // Sort rooms according to room type
        // ---------------------------------------------------------------------
        // NOTE: This allows common rooms to assume that they are rectangular
        // and have their walls untouched when their reshaping functions run.

        std::sort(
                std::begin(map::g_room_list),
                std::end(map::g_room_list),
                [](const auto r0, const auto r1) {
                        return r0->m_type < r1->m_type;
                });

        if (!mapgen::g_is_map_valid)
        {
                return false;
        }

        // ---------------------------------------------------------------------
        // Run the pre-connect hook on all rooms
        // ---------------------------------------------------------------------
        TRACE << "Running pre-connect for all rooms" << std::endl;

#ifndef NDEBUG
        if (init::g_is_demo_mapgen)
        {
                io::cover_panel(Panel::log);
                states::draw();
                io::draw_text(
                        "Press any key to run pre-connect on rooms...",
                        Panel::screen,
                        P(0, 0),
                        colors::white());
                io::update_screen();
                query::wait_for_key_press();
                io::cover_panel(Panel::log);
        }
#endif  // NDEBUG

        for (auto* const room : map::g_room_list)
        {
                room->on_pre_connect(mapgen::g_door_proposals);
        }

        if (!mapgen::g_is_map_valid)
        {
                return false;
        }

        // ---------------------------------------------------------------------
        // Connect the rooms
        // ---------------------------------------------------------------------
#ifndef NDEBUG
        if (init::g_is_demo_mapgen)
        {
                io::cover_panel(Panel::log);
                states::draw();
                io::draw_text(
                        "Press any key to connect rooms...",
                        Panel::screen,
                        P(0, 0),
                        colors::white());
                io::update_screen();
                query::wait_for_key_press();
                io::cover_panel(Panel::log);
        }
#endif  // NDEBUG

        mapgen::connect_rooms();

        if (!mapgen::g_is_map_valid)
        {
                return false;
        }

        // ---------------------------------------------------------------------
        // Run the post-connect hook on all rooms
        // ---------------------------------------------------------------------
        TRACE << "Running post-connect for all rooms" << std::endl;
#ifndef NDEBUG
        if (init::g_is_demo_mapgen)
        {
                io::cover_panel(Panel::log);
                states::draw();
                io::draw_text(
                        "Press any key to run post-connect on rooms...",
                        Panel::screen,
                        P(0, 0),
                        colors::white());
                io::update_screen();
                query::wait_for_key_press();
                io::cover_panel(Panel::log);
        }
#endif  // NDEBUG

        for (auto* const room : map::g_room_list)
        {
                room->on_post_connect(mapgen::g_door_proposals);
        }

        if (!mapgen::g_is_map_valid)
        {
                return false;
        }

        // ---------------------------------------------------------------------
        // Place doors
        // ---------------------------------------------------------------------
        if (map::g_dlvl <= g_dlvl_last_mid_game)
        {
                mapgen::make_doors();
        }

        if (!mapgen::g_is_map_valid)
        {
                return false;
        }

        // ---------------------------------------------------------------------
        // Set player position
        // ---------------------------------------------------------------------
        map::g_player->m_pos =
                P(rnd::range(1, map::w() - 2),
                  rnd::range(1, map::h() - 2));

        mapgen::move_player_to_nearest_allowed_pos();

        if (!mapgen::g_is_map_valid)
        {
                return false;
        }

        // ---------------------------------------------------------------------
        // Decorate the map
        // ---------------------------------------------------------------------
        mapgen::decorate();

        if (!mapgen::g_is_map_valid)
        {
                return false;
        }

        // ---------------------------------------------------------------------
        // Place the stairs
        // ---------------------------------------------------------------------
        // NOTE: The choke point information gathering below depends on the
        // stairs having been placed.
        P stairs_pos;

        stairs_pos = mapgen::make_stairs_at_random_pos();

        if (!mapgen::g_is_map_valid)
        {
                return false;
        }

        // ---------------------------------------------------------------------
        // Gather data on choke points in the map (check every position where a
        // door has previously been "proposed")
        // ---------------------------------------------------------------------
        Array2<bool> blocked(map::dims());

        map_parsers::BlocksWalking(ParseActors::no)
                .run(blocked, blocked.rect());

        const std::vector<terrain::Id> free_terrains = {
                terrain::Id::door,
                terrain::Id::liquid_deep,
                terrain::Id::stairs};

        const auto is_free_terrain_parser =
                map_parsers::IsAnyOfTerrains(free_terrains);

        for (int x = 0; x < blocked.w(); ++x)
        {
                for (int y = 0; y < blocked.h(); ++y)
                {
                        const P p(x, y);

                        if (is_free_terrain_parser.run(p))
                        {
                                blocked.at(p) = false;
                        }
                }
        }

        for (int x = 0; x < map::w(); ++x)
        {
                for (int y = 0; y < map::h(); ++y)
                {
                        if (blocked.at(x, y) ||
                            !mapgen::g_door_proposals.at(x, y))
                        {
                                continue;
                        }

                        ChokePointData d;

                        const bool is_choke =
                                mapgen::is_choke_point(
                                        P(x, y),
                                        blocked,
                                        &d);

                        // 'is_choke_point' called above may invalidate the map
                        if (!mapgen::g_is_map_valid)
                        {
                                return false;
                        }

                        if (!is_choke)
                        {
                                continue;
                        }

                        // Find player and stair side
                        for (size_t side_idx = 0; side_idx < 2; ++side_idx)
                        {
                                for (const P& p : d.sides[side_idx])
                                {
                                        if (p == map::g_player->m_pos)
                                        {
                                                ASSERT(d.player_side == -1);

                                                d.player_side = side_idx;
                                        }

                                        if (p == stairs_pos)
                                        {
                                                ASSERT(d.stairs_side == -1);

                                                d.stairs_side = side_idx;
                                        }
                                }
                        }

                        ASSERT(d.player_side == 0 || d.player_side == 1);
                        ASSERT(d.stairs_side == 0 || d.stairs_side == 1);

                        // Robustness for release mode
                        if ((d.player_side != 0 && d.player_side != 1) ||
                            (d.stairs_side != 0 && d.stairs_side != 1))
                        {
                                // Invalidate the map
                                mapgen::g_is_map_valid = false;

                                return false;
                        }

                        map::g_choke_point_data.emplace_back(d);
                }  // y loop
        }  // x loop

        TRACE
                << "Found " << map::g_choke_point_data.size()
                << " choke points" << std::endl;

        if (!mapgen::g_is_map_valid)
        {
                return false;
        }

        // ---------------------------------------------------------------------
        // Make metal doors and levers
        // ---------------------------------------------------------------------
        mapgen::make_metal_doors_and_levers();

        if (!mapgen::g_is_map_valid)
        {
                return false;
        }

        // ---------------------------------------------------------------------
        // Make some doors leading to "optional" areas secret or stuck
        // ---------------------------------------------------------------------
        for (const auto& choke_point : map::g_choke_point_data)
        {
                if (choke_point.player_side != choke_point.stairs_side)
                {
                        continue;
                }

                auto* const terrain = map::g_terrain.at(choke_point.p);

                if (terrain->id() == terrain::Id::door)
                {
                        auto* const door = static_cast<terrain::Door*>(terrain);

                        if ((door->type() != terrain::DoorType::gate) &&
                            (door->type() != terrain::DoorType::metal) &&
                            rnd::one_in(6))
                        {
                                door->set_secret();
                        }

                        if (rnd::one_in(6))
                        {
                                door->set_stuck();
                        }
                }
        }

        if (!mapgen::g_is_map_valid)
        {
                return false;
        }

        // ---------------------------------------------------------------------
        // Place Monoliths
        // ---------------------------------------------------------------------
        // NOTE: This depends on choke point data having been gathered
        // (including player side and stairs side)
        mapgen::make_monoliths();

        if (!mapgen::g_is_map_valid)
        {
                return false;
        }

        // ---------------------------------------------------------------------
        // Place pylons and levers
        // ---------------------------------------------------------------------
        // mapgen::make_pylons();

        // if (!mapgen::g_is_map_valid)
        // {
        //         return false;
        // }

        // ---------------------------------------------------------------------
        // Reveal all doors on the path to the stairs
        // ---------------------------------------------------------------------
        mapgen::reveal_doors_on_path_to_stairs(stairs_pos);

        // ---------------------------------------------------------------------
        // Populate the map with monsters
        // ---------------------------------------------------------------------
        for (const auto* const room : map::g_room_list)
        {
                room->populate_monsters();
        }

        populate_mon::populate_std_lvl();

        if (!mapgen::g_is_map_valid)
        {
                return false;
        }

        // ---------------------------------------------------------------------
        // Populate the map with traps
        // ---------------------------------------------------------------------
        populate_traps::populate_std_lvl();

        if (!mapgen::g_is_map_valid)
        {
                return false;
        }

        // ---------------------------------------------------------------------
        // Populate the map with items on the floor
        // ---------------------------------------------------------------------
        populate_items::make_items_on_floor();

        if (!mapgen::g_is_map_valid)
        {
                return false;
        }

        // ---------------------------------------------------------------------
        // Place "snake emerge" events
        // ---------------------------------------------------------------------
        const int nr_snake_emerge_events_to_try =
                rnd::one_in(60)
                ? 2
                : rnd::one_in(16)
                ? 1
                : 0;

        for (int i = 0; i < nr_snake_emerge_events_to_try; ++i)
        {
                auto* const event = new terrain::EventSnakeEmerge();

                if (event->try_find_p())
                {
                        game_time::add_mob(event);
                }
                else
                {
                        delete event;
                }
        }

        if (!mapgen::g_is_map_valid)
        {
                return false;
        }

        // ---------------------------------------------------------------------
        // Occasionally make the whole level dark
        // ---------------------------------------------------------------------
        if (map::g_dlvl > 1)
        {
                int make_drk_pct = 0;

                if (map::g_dlvl <= g_dlvl_last_early_game)
                {
                        make_drk_pct = 1;
                }
                else if (map::g_dlvl <= g_dlvl_last_mid_game)
                {
                        make_drk_pct = 2;
                }
                else
                {
                        make_drk_pct = 15;
                }

                if (rnd::percent(make_drk_pct))
                {
                        const size_t nr_positions = map::nr_positions();
                        for (size_t i = 0; i < nr_positions; ++i)
                        {
                                map::g_dark.at(i) = true;
                        }
                }
        }

        if (!mapgen::g_is_map_valid)
        {
                return false;
        }

        for (auto* const room : map::g_room_list)
        {
                delete room;
        }

        map::g_room_list.clear();

        map::g_room_map.resize({0, 0});

        TRACE_FUNC_END;

        return mapgen::g_is_map_valid;
}

std::unique_ptr<MapController> MapBuilderStd::map_controller() const
{
        return std::make_unique<MapControllerStd>();
}

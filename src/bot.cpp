// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "bot.hpp"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "actor.hpp"
#include "actor_data.hpp"
#include "actor_factory.hpp"
#include "array2.hpp"
#include "colors.hpp"
#include "config.hpp"
#include "debug.hpp"
#include "direction.hpp"
#include "explosion.hpp"
#include "game_commands.hpp"
#include "game_time.hpp"
#include "global.hpp"
#include "inventory.hpp"
#include "io.hpp"
#include "item.hpp"
#include "item_data.hpp"
#include "item_device.hpp"
#include "item_factory.hpp"
#include "map.hpp"
#include "map_travel.hpp"
#include "misc.hpp"
#include "panel.hpp"
#include "pathfind.hpp"
#include "pos.hpp"
#include "property.hpp"
#include "property_data.hpp"
#include "property_factory.hpp"
#include "property_handler.hpp"
#include "random.hpp"
#include "state.hpp"
#include "teleport.hpp"
#include "terrain.hpp"
#include "terrain_data.hpp"
#include "terrain_door.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static std::vector<P> s_path;

static void show_map_and_freeze(const std::string& msg)
{
        TRACE_FUNC_BEGIN;

        const size_t nr_positions = map::nr_positions();
        for (size_t i = 0; i < nr_positions; ++i) {
                map::g_seen.at(i) = true;
        }

        map::update_player_memory();

        for (auto* const actor : game_time::g_actors) {
                if (actor::is_player(actor)) {
                        continue;
                }

                actor->m_mon_aware_state.player_aware_of_me_counter = 999;
        }

        while (true) {
                io::draw_text(
                        "[" + msg + "]",
                        Panel::screen,
                        P(0, 0),
                        colors::light_red());

                io::update_screen();

                io::sleep(1);

                io::clear_input();
        }
}  // show_map_and_freeze

static void find_stair_path()
{
        Array2<bool> blocked =
                map::get_blocked_map_info_for_actor(*map::g_player);

        const int w = map::w();
        const int h = map::h();

        P stair_p(-1, -1);

        for (int x = 0; x < w; ++x) {
                for (int y = 0; y < h; ++y) {
                        switch (map::g_terrain.at(x, y)->id()) {
                        case terrain::Id::stairs: {
                                blocked.at(x, y) = false;

                                stair_p.set(x, y);
                        } break;

                        case terrain::Id::door:
                        case terrain::Id::force_field: {
                                blocked.at(x, y) = false;
                        } break;

                        default:
                        {
                        } break;
                        }
                }
        }

        if (stair_p.x == -1) {
                show_map_and_freeze("Could not find stairs");
        }

        const auto& player_p = map::g_player->m_pos;

        if (blocked.at(player_p)) {
                show_map_and_freeze("Player on blocked position");
        }

        s_path = pathfind(player_p, stair_p, blocked);

        if (s_path.empty()) {
                show_map_and_freeze("Could not find path to stairs");
        }

        ASSERT(s_path.front() == stair_p);
}  // find_stair_path

static bool walk_to_adj_cell(const P& p)
{
        ASSERT(is_pos_adj(map::g_player->m_pos, p, true));

        auto dir = Dir::END;

        if (rnd::fraction(1, 4)) {
                dir = (Dir)rnd::range(0, (int)Dir::END - 1);
        }
        else {
                dir = dir_utils::dir(p - map::g_player->m_pos);
        }

        GameCmd cmd = GameCmd::none;

        switch (dir) {
        case Dir::down_left:
                cmd = GameCmd::down_left;
                break;

        case Dir::down:
                cmd = GameCmd::down;
                break;

        case Dir::down_right:
                cmd = GameCmd::down_right;
                break;

        case Dir::left:
                cmd = GameCmd::left;
                break;

        case Dir::center:
                cmd = GameCmd::wait;
                break;

        case Dir::right:
                cmd = GameCmd::right;
                break;

        case Dir::up_left:
                cmd = GameCmd::up_left;
                break;

        case Dir::up:
                cmd = GameCmd::up;
                break;

        case Dir::up_right:
                cmd = GameCmd::up_right;
                break;

        case Dir::END:
                break;
        }

        game_commands::handle(cmd);

        return (map::g_player->m_pos == p);
}

// Act function used when just using the bot to run through the game.
static void bot_act()
{
        // =====================================================================
        // TESTS
        // =====================================================================
#ifndef NDEBUG
        for (size_t outer_idx = 0;
             outer_idx < game_time::g_actors.size();
             ++outer_idx) {
                const auto* const actor = game_time::g_actors[outer_idx];

                ASSERT(map::is_pos_inside_map(actor->m_pos));

                if (!actor->is_alive()) {
                        continue;
                }

                for (size_t inner_idx = 0;
                     inner_idx < game_time::g_actors.size();
                     ++inner_idx) {
                        const auto* const other_actor =
                                game_time::g_actors[inner_idx];

                        if ((outer_idx == inner_idx) ||
                            !other_actor->is_alive()) {
                                continue;
                        }

                        ASSERT(actor != other_actor);
                        ASSERT(actor->m_pos != other_actor->m_pos);
                }
        }
#endif  // NDEBUG

        // =====================================================================

        // Abort?
        // TODO: Reimplement this
        //    if(io::is_key_held(SDLK_ESCAPE))
        //    {
        //        config::toggle_bot_playing();
        //    }

        // If we are finished with the current run, go back to dlvl 1
        if (map::g_dlvl >= g_dlvl_last) {
                TRACE << "Starting new run on first dungeon level" << std::endl;
                map_travel::init();

                map::g_dlvl = 1;

                return;
        }

        auto& inv = map::g_player->m_inv;

        // If no armor, occasionally equip an asbesthos suite (helps not getting
        // stuck on e.g. Energy Hounds).
        if (!inv.m_slots[(size_t)SlotId::body].item && rnd::one_in(20)) {
                inv.put_in_slot(
                        SlotId::body,
                        item::make(item::Id::armor_asb_suit),
                        Verbose::no);
        }

        // Keep an allied Mi-go around (to help getting out of sticky
        // situations, and for some allied monster code exercise).
        bool has_allied_mon = false;

        for (const auto* const actor : game_time::g_actors) {
                if (map::g_player->is_leader_of(actor)) {
                        has_allied_mon = true;
                        break;
                }
        }

        if (!has_allied_mon) {
                actor::spawn(
                        map::g_player->m_pos, {actor::Id::mi_go}, map::rect())
                        .set_leader(map::g_player)
                        .make_aware_of_player();
        }

        // Apply permanent paralysis resistance, to avoid getting stuck.
        if (!map::g_player->m_properties.has(PropId::r_para)) {
                auto* prop = new PropRPara();

                prop->set_indefinite();

                map::g_player->m_properties.apply(prop);
        }

        // Apply fear resistance to avoid getting stuck.
        if (rnd::one_in(7)) {
                auto* prop = new PropRFear();

                prop->set_duration(4);

                map::g_player->m_properties.apply(prop);
        }

        // Apply burning to a random actor (to avoid getting stuck).
        if (rnd::one_in(10)) {
                const auto element =
                        rnd::range(0, (int)game_time::g_actors.size() - 1);

                auto* const actor = game_time::g_actors[element];

                if (!actor::is_player(actor)) {
                        actor->m_properties.apply(new PropBurning());
                }
        }

        // Teleport (to avoid getting stuck).
        if (rnd::one_in(200)) {
                teleport(*map::g_player);
        }

        // Send a TAB command to attack nearby monsters.
        if (rnd::coin_toss()) {
                game_commands::handle(GameCmd::auto_melee);

                return;
        }

        // Send a 'wait 5 turns' command (just code exercise).
        if (rnd::one_in(50)) {
                game_commands::handle(GameCmd::wait_long);

                return;
        }

        // Fire at a random position.
        if (rnd::one_in(20)) {
                auto* wpn_item = map::g_player->m_inv.item_in_slot(SlotId::wpn);

                if (wpn_item && wpn_item->data().ranged.is_ranged_wpn) {
                        auto* wpn = static_cast<item::Wpn*>(wpn_item);

                        wpn->m_ammo_loaded = wpn->data().ranged.max_ammo;

                        game_commands::handle(GameCmd::fire);

                        return;
                }
        }

        // Apply a random property (to exercise the prop code).
        if (rnd::one_in(30)) {
                std::vector<PropId> prop_bucket;

                for (size_t i = 0; i < (size_t)PropId::END; ++i) {
                        if (property_data::g_data[i].allow_test_on_bot) {
                                prop_bucket.push_back(PropId(i));
                        }
                }

                const auto prop_id = rnd::element(prop_bucket);
                auto* const prop = property_factory::make(prop_id);

                prop->set_duration(5);

                map::g_player->m_properties.apply(prop);
        }

        // End a random property (helps clearing out properties that cause
        // spammy or interrupting effects like mind sapping).
        if (rnd::one_in(100)) {
                const auto id = (PropId)rnd::range(0, (int)PropId::END - 1);

                map::g_player->m_properties.end_prop(id);
        }

        // Swap weapon (just some code exercise).
        if (rnd::one_in(50)) {
                game_commands::handle(GameCmd::swap_weapon);

                return;
        }

        // Cause shock spikes (code exercise).
        if (rnd::one_in(100)) {
                map::g_player->incr_shock(200.0, ShockSrc::misc);

                return;
        }

        // Run an explosion around the player (code exercise, and to avoid
        // getting stuck).
        if (rnd::one_in(50)) {
                explosion::run(map::g_player->m_pos, ExplType::expl);

                return;
        }

        // Run the effect of a random "strange device".
        if (rnd::one_in(50)) {
                std::vector<item::Id> id_bucket;

                for (size_t i = 0; i < (size_t)item::Id::END; ++i) {
                        item::ItemData& d = item::g_data[i];

                        if ((d.type == ItemType::device) &&
                            (d.id != item::Id::lantern)) {
                                d.is_identified = true;
                                id_bucket.push_back((item::Id)i);
                        }
                }

                const item::Id id = rnd::element(id_bucket);

                std::unique_ptr<device::Device> device(
                        static_cast<device::Device*>(item::make(id)));

                device->activate(map::g_player);

                return;
        }

        // Handle blocking door.
        for (const P& d : dir_utils::g_dir_list) {
                const auto p = map::g_player->m_pos + d;

                auto* const t = map::g_terrain.at(p);

                if (t->id() == terrain::Id::door) {
                        auto* const door = static_cast<terrain::Door*>(t);

                        if (door->is_hidden()) {
                                door->reveal(terrain::PrintRevealMsg::no);
                        }

                        if (door->is_stuck()) {
                                t->hit(DmgType::blunt, map::g_player);

                                return;
                        }
                }
        }

        // If we are terrified, wait in place.
        if (map::g_player->m_properties.has(PropId::terrified)) {
                if (walk_to_adj_cell(map::g_player->m_pos)) {
                        return;
                }
        }

        find_stair_path();

        walk_to_adj_cell(s_path.back());
}

// Act function to use when running stress test for time measurements.
static void stress_test_act()
{
        static int stress_test_step = 1;
        static bool move_right = true;
        static bool is_rats_hostile = true;

        map::g_player->m_properties.end_prop(PropId::terrified);
        map::g_player->m_properties.end_prop(PropId::frenzied);
        map::g_player->m_properties.end_prop(PropId::confused);

        if ((stress_test_step % 150) == 0) {
                auto* const leader = is_rats_hostile ? nullptr : map::g_player;

                actor::spawn(
                        map::g_player->m_pos,
                        {400, actor::Id::rat},
                        map::rect())
                        .set_leader(leader);

                is_rats_hostile = !is_rats_hostile;
        }

        const auto p_adj =
                move_right
                ? map::g_player->m_pos.with_x_offset(1)
                : map::g_player->m_pos.with_x_offset(-1);

        const terrain::Id terrain_id = map::g_terrain.at(p_adj)->id();

        if ((terrain_id == terrain::Id::wall) ||
            (terrain_id == terrain::Id::rubble_high) ||
            (terrain_id == terrain::Id::tree)) {
                move_right = !move_right;
        }

        if (move_right) {
                game_commands::handle(GameCmd::right);
        }
        else {
                game_commands::handle(GameCmd::left);
        }

        if (stress_test_step == 1000) {
                states::pop_all();
        }
        else {
                ++stress_test_step;
        }
}

// -----------------------------------------------------------------------------
// bot
// -----------------------------------------------------------------------------
namespace bot
{
void init()
{
        s_path.clear();
}

void act()
{
        if (config::is_stress_test()) {
                stress_test_act();
        }
        else {
                bot_act();
        }
}

}  // namespace bot

// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "game_time.hpp"

#include <algorithm>
#include <cstring>
#include <iterator>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "actor.hpp"
#include "actor_data.hpp"
#include "actor_death.hpp"
#include "actor_player_state.hpp"
#include "actor_see.hpp"
#include "actor_start_turn.hpp"
#include "actor_std_turn.hpp"
#include "array2.hpp"
#include "audio.hpp"
#include "colors.hpp"
#include "debug.hpp"
#include "global.hpp"
#include "inventory.hpp"
#include "item.hpp"
#include "map.hpp"
#include "map_controller.hpp"
#include "map_travel.hpp"
#include "msg_log.hpp"
#include "pos.hpp"
#include "property_data.hpp"
#include "property_handler.hpp"
#include "rect.hpp"
#include "saving.hpp"
#include "smell.hpp"
#include "sound.hpp"
#include "terrain.hpp"
#include "terrain_data.hpp"
#include "terrain_door.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
// Smallest number divisible by both 2 (200% speed) and 3 (300% speed)
static const int s_ticks_per_turn = 6;

static size_t s_current_actor_idx = 0;

static int s_std_turn_delay = s_ticks_per_turn;

static int s_turn_nr = 0;

static int speed_to_pct(const actor::Speed speed)
{
        switch (speed) {
        case actor::Speed::slow:
                return 50;

        case actor::Speed::normal:
                return 100;

        case actor::Speed::fast:
                return 200;

        case actor::Speed::very_fast:
                return 300;
        }

        ASSERT(false);

        return 100;
}

static actor::Speed incr_speed_category(actor::Speed speed)
{
        if (speed < actor::Speed::very_fast) {
                speed = (actor::Speed)((int)speed + 1);
        }

        return speed;
}

static actor::Speed decr_speed_category(actor::Speed speed)
{
        if ((int)speed > 0) {
                speed = (actor::Speed)((int)speed - 1);
        }

        return speed;
}

static actor::Speed current_actor_speed(const actor::Actor& actor)
{
        // Paralyzed actors always act at normal speed (otherwise paralysis will
        // barely affect super fast monsters at all).
        if (actor.m_properties.has(prop::Id::paralyzed)) {
                return actor::Speed::normal;
        }

        if (actor.m_properties.has(prop::Id::extra_hasted)) {
                return actor::Speed::very_fast;
        }

        actor::Speed speed = actor.m_data->speed;

        if (actor.m_properties.has(prop::Id::slowed)) {
                speed = decr_speed_category(speed);
        }

        if (actor.m_properties.has(prop::Id::hasted)) {
                speed = incr_speed_category(speed);
        }

        if (actor.m_properties.has(prop::Id::frenzied)) {
                speed = incr_speed_category(speed);
        }

        return speed;
}

static void erase_destroyed_actor(
        const actor::Actor* const actor,
        const size_t actor_idx)
{
        if (actor::is_player(actor)) {
                return;
        }

        if (actor::player_state::g_target == actor) {
                actor::player_state::g_target = nullptr;
        }

        delete actor;

        game_time::g_actors.erase(
                std::begin(game_time::g_actors) +
                (int)actor_idx);

        actor::unset_actor_as_leader_and_target_for_all_mon(actor);

        if (s_current_actor_idx >= game_time::g_actors.size()) {
                s_current_actor_idx = 0;
        }
}

static void run_std_turn_events()
{
        if (game_time::g_is_magic_descend_nxt_std_turn) {
                map::g_player->interrupt_actions(ForceInterruptActions::yes);

                const prop::PropEndConfig prop_end_config(
                        prop::PropEndAllowCallEndHook::no,
                        prop::PropEndAllowMsg::no,
                        prop::PropEndAllowHistoricMsg::no);

                map::g_player->m_properties.end_prop(prop::Id::nailed, prop_end_config);
                map::g_player->m_properties.end_prop(prop::Id::entangled, prop_end_config);
                map::g_player->m_properties.end_prop(prop::Id::stuck, prop_end_config);

                msg_log::add(
                        "I sink downwards!",
                        colors::white(),
                        MsgInterruptPlayer::yes,
                        MorePromptOnMsg::yes);

                map_travel::go_to_nxt();

                return;
        }

        ++s_turn_nr;

        // NOTE: Iteration must be done by index, since new monsters may be
        // spawned inside the loop when the standard turn hook is called (e.g.
        // from the 'breeding' property)
        for (size_t i = 0; i < game_time::g_actors.size(); /* No increment */) {
                auto* const actor = game_time::g_actors[i];

                // Delete destroyed actors
                if (actor->m_state == ActorState::destroyed) {
                        erase_destroyed_actor(actor, i);
                }
                else {
                        // Actor not destroyed
                        if (!actor::is_player(actor)) {
                                // Count down player awareness of the monster
                                if (actor->is_player_aware_of_me() &&
                                    !actor::can_player_see_actor(*actor)) {
                                        --actor->m_mon_aware_state
                                                  .player_aware_of_me_counter;
                                }
                        }

                        actor::std_turn(*actor);

                        // NOTE: This may spawn new monsters, see NOTE above.
                        actor->m_properties.on_std_turn();

                        ++i;
                }

                if (!map::g_player || !map::g_player->is_alive()) {
                        return;
                }

        }  // Actor loop

        // Allow already burning terrains to damage stuff, spread fire, etc.
        for (auto* const terrain : map::g_terrain) {
                if (terrain->is_burning()) {
                        terrain->m_started_burning_this_turn = false;
                }
        }

        for (auto* const terrain : map::g_terrain) {
                terrain->on_new_turn();
        }

        const std::vector<terrain::Terrain*> mobs_cpy = game_time::g_mobs;

        for (auto* terrain : mobs_cpy) {
                terrain->on_new_turn();
        }

        if (map_control::g_controller) {
                map_control::g_controller->on_std_turn();
        }

        // Run new turn events on all player items
        for (auto* const item : map::g_player->m_inv.m_backpack) {
                item->on_std_turn_in_inv(InvType::backpack);
        }

        for (InvSlot& slot : map::g_player->m_inv.m_slots) {
                if (slot.item) {
                        slot.item->on_std_turn_in_inv(InvType::slots);
                }
        }

        smell::on_std_turn();

        snd_emit::reset_nr_snd_msg_printed_current_turn();

        if ((map::g_dlvl > 0) &&
            !map::g_player->m_properties.has(prop::Id::deaf)) {
                const int play_one_in_n = 200;

                audio::try_play_ambient(play_one_in_n);
        }
}

static void run_atomic_turn_events()
{
        // Stop burning for any actor standing in liquid
        for (auto* const actor : game_time::g_actors) {
                auto& props = actor->m_properties;

                if (props.has(prop::Id::flying) ||
                    props.has(prop::Id::tiny_flying) ||
                    !props.has(prop::Id::burning)) {
                        continue;
                }

                const auto& p = actor->m_pos;

                const auto* const terrain = map::g_terrain.at(p);

                if (terrain->m_data->material_type == Material::fluid) {
                        // TODO: Add a message here.

                        actor->m_properties.end_prop(prop::Id::burning);

                        map::update_light_map();
                }
        }
}

static void set_actor_max_delay(actor::Actor& actor)
{
        const auto speed_category = current_actor_speed(actor);

        const int speed_pct = speed_to_pct(speed_category);

        int delay_to_set = (s_ticks_per_turn * 100) / speed_pct;

        delay_to_set = std::max(1, delay_to_set);

        actor.m_delay = delay_to_set;
}

// -----------------------------------------------------------------------------
// game_time
// -----------------------------------------------------------------------------
namespace game_time
{
std::vector<actor::Actor*> g_actors;
std::vector<terrain::Terrain*> g_mobs;

bool g_is_magic_descend_nxt_std_turn;
bool g_is_player_acting;
bool g_allow_tick;

void init()
{
        s_current_actor_idx = 0;
        s_turn_nr = 0;
        s_std_turn_delay = s_ticks_per_turn;

        g_actors.clear();

        g_mobs.clear();

        g_is_magic_descend_nxt_std_turn = false;
        g_is_player_acting = false;

        g_allow_tick = true;
}

void cleanup()
{
        for (auto* a : g_actors) {
                delete a;
        }

        g_actors.clear();

        for (auto* t : g_mobs) {
                delete t;
        }

        g_mobs.clear();

        g_is_magic_descend_nxt_std_turn = false;
        g_is_player_acting = false;
}

void save()
{
        saving::put_int(s_turn_nr);
}

void load()
{
        s_turn_nr = saving::get_int();
}

int turn_nr()
{
        return s_turn_nr;
}

std::vector<terrain::Terrain*> mobs_at(const P& p)
{
        std::vector<terrain::Terrain*> mobs_at_pos;

        std::copy_if(
                std::begin(g_mobs),
                std::end(g_mobs),
                std::back_inserter(mobs_at_pos),
                [p](const terrain::Terrain* const t) {
                        return t->pos() == p;
                });

        return mobs_at_pos;
}

void add_mob(terrain::Terrain* const terrain)
{
        g_mobs.push_back(terrain);

        terrain->on_placed();
}

void erase_mob(
        const terrain::Terrain* const terrain,
        const bool destroy_object)
{
        for (auto it = std::begin(g_mobs); it != std::end(g_mobs); ++it) {
                if (*it == terrain) {
                        if (destroy_object) {
                                delete terrain;
                        }

                        g_mobs.erase(it);

                        return;
                }
        }

        TRACE
                << "Could not erase mobile terrain '"
                << terrain
                << "', not found in list of size '"
                << g_mobs.size()
                << "'"
                << std::endl;

        TRACE
                << "Terrain name: '" << terrain->name(Article::a) << "'"
                << std::endl;

        ASSERT(false);
}

void erase_all_mobs()
{
        for (auto* m : g_mobs) {
                delete m;
        }

        g_mobs.clear();
}

void add_actor(actor::Actor* actor)
{
        // Sanity checks
        // ASSERT(map::is_pos_inside_map(actor->m_pos));

#ifndef NDEBUG
        for (actor::Actor* const existing_actor : g_actors) {
                ASSERT(actor != existing_actor);

                if (actor->is_alive() && existing_actor->is_alive()) {
                        const P& new_actor_p = actor->m_pos;
                        const P& existing_actor_p = existing_actor->m_pos;

                        ASSERT(new_actor_p != existing_actor_p);
                }
        }
#endif  // NDEBUG

        g_actors.push_back(actor);
}

void reset_current_actor_idx()
{
        s_current_actor_idx = 0;
}

void tick()
{
        if (!map::g_player || !map::g_player->is_alive()) {
                return;
        }

#ifndef NDEBUG
        ASSERT(g_allow_tick);

        g_allow_tick = false;
#endif  // NDEBUG

        g_is_player_acting = false;

        {
                auto* actor = current_actor();

                if (actor::is_player(actor)) {
                        msg_log::newline();
                }

                set_actor_max_delay(*actor);

                actor->m_properties.on_turn_end();
        }

        // Find next actor who can act
        while (true) {
                if (g_actors.empty()) {
                        return;
                }

                ++s_current_actor_idx;

                if (s_current_actor_idx == g_actors.size()) {
                        --s_std_turn_delay;

                        // New standard turn?
                        if (s_std_turn_delay == 0) {
                                // Increment the turn counter, and run standard
                                // turn events.
                                //
                                // NOTE: This will prune destroyed actors, which
                                // will decrease the actor vector size.
                                //
                                run_std_turn_events();

                                s_std_turn_delay = s_ticks_per_turn;
                        }

                        s_current_actor_idx = 0;
                }

                actor::Actor* const actor = current_actor();

                --actor->m_delay;

                if (actor->m_delay <= 0) {
                        // This actor is ready to go.
                        break;
                }

                if (!map::g_player || !map::g_player->is_alive()) {
                        return;
                }
        }

        run_atomic_turn_events();

        auto* const actor = current_actor();

        // Clear flag that this actor is opening a door
        if (map::rect().is_pos_inside(actor->m_opening_door_pos)) {
                auto* const t = map::g_terrain.at(actor->m_opening_door_pos);

                if (t->id() == terrain::Id::door) {
                        auto* const door = static_cast<terrain::Door*>(t);

                        if (door->actor_currently_opening() == actor) {
                                door->clear_actor_currently_opening();
                        }

                        actor->m_opening_door_pos = {-1, -1};
                }
        }

        actor::start_turn(*actor);
}

actor::Actor* current_actor()
{
        ASSERT(s_current_actor_idx < g_actors.size());

        auto* const actor = g_actors[s_current_actor_idx];

        ASSERT(map::is_pos_inside_map(actor->m_pos));

        return actor;
}

}  // namespace game_time

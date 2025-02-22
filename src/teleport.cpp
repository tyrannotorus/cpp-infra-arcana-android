// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "teleport.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "actor.hpp"
#include "actor_data.hpp"
#include "actor_move.hpp"
#include "actor_see.hpp"
#include "array2.hpp"
#include "common_text.hpp"
#include "debug.hpp"
#include "direction.hpp"
#include "flood.hpp"
#include "fov.hpp"
#include "game.hpp"
#include "game_time.hpp"
#include "map.hpp"
#include "map_parsing.hpp"
#include "marker.hpp"
#include "misc.hpp"
#include "msg_log.hpp"
#include "pos.hpp"
#include "property.hpp"
#include "property_data.hpp"
#include "property_factory.hpp"
#include "property_handler.hpp"
#include "random.hpp"
#include "rect.hpp"
#include "state.hpp"
#include "terrain.hpp"
#include "terrain_data.hpp"
#include "terrain_door.hpp"
#include "text_format.hpp"
#include "viewport.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static bool is_void_traveler_affecting_player_teleport(const actor::Actor& actor)
{
        const std::string actor_id = actor::id(actor);

        const bool is_void_traveler =
                (actor_id == "MON_VOID_TRAVELER") ||
                (actor_id == "MON_ELDER_VOID_TRAVELER");

        return (
                is_void_traveler &&
                (actor.m_state == ActorState::alive) &&
                actor.m_properties.allow_act() &&
                !actor.is_actor_my_leader(map::g_player) &&
                actor::is_aware_of_player(actor));
}

static std::vector<P> get_free_positions_around_pos(
        const P& p,
        const Array2<bool>& blocked)
{
        std::vector<P> free_positions;

        for (const P& d : dir_utils::g_dir_list) {
                const P adj_p(p + d);

                if (!blocked.at(adj_p)) {
                        free_positions.push_back(adj_p);
                }
        }

        return free_positions;
}

static bool handle_void_traveler_affecting_player_teleport(
        actor::Actor& actor_teleporting,
        P& target_pos,
        const Array2<bool>& blocked)
{
        if (!actor::is_player(&actor_teleporting)) {
                return false;
        }

        for (actor::Actor* const other_actor : game_time::g_actors) {
                if (!is_void_traveler_affecting_player_teleport(*other_actor)) {
                        continue;
                }

                const std::vector<P> p_bucket =
                        get_free_positions_around_pos(
                                other_actor->m_pos,
                                blocked);

                if (p_bucket.empty()) {
                        continue;
                }

                // Set new teleport destination
                target_pos = rnd::element(p_bucket);

                const std::string actor_name_a =
                        text_format::first_to_upper(
                                actor::name_a(*other_actor));

                msg_log::add(actor_name_a + " intercepts my teleportation!");

                const std::vector<prop::Id> props_ended = {
                        prop::Id::invis,
                        prop::Id::cloaked,
                };

                const prop::PropEndConfig cfg(
                        prop::PropEndAllowCallEndHook::no,
                        prop::PropEndAllowMsg::yes,
                        prop::PropEndAllowHistoricMsg::yes);

                for (prop::Id id : props_ended) {
                        actor_teleporting.m_properties.end_prop(id, cfg);
                }

                other_actor->become_aware_player(actor::AwareSource::other);

                return true;
        }

        return false;
}

static void make_all_mon_not_seeing_player_unaware()
{
        Array2<bool> blocks_los(map::dims());

        const R r = fov::fov_rect(map::g_player->m_pos, blocks_los.dims());

        map_parsers::BlocksLos().run(blocks_los, r, MapParseMode::overwrite);

        for (actor::Actor* const mon : game_time::g_actors) {
                if (actor::is_player(mon)) {
                        continue;
                }

                const bool can_mon_see_player =
                        can_mon_see_actor(*mon, *map::g_player, blocks_los);

                if (!can_mon_see_player) {
                        mon->m_mon_aware_state.aware_counter = 0;
                }
        }
}

static void confuse_player()
{
        msg_log::add("I suddenly find myself in a different location!");

        prop::Prop* prop = prop::make(prop::Id::confused);

        prop->set_duration(8);

        map::g_player->m_properties.apply(prop);
}

static bool should_player_ctrl_tele(const ShouldCtrlTele ctrl_tele)
{
        switch (ctrl_tele) {
        case ShouldCtrlTele::always: {
                return true;
        }

        case ShouldCtrlTele::never: {
                return false;
        }

        case ShouldCtrlTele::if_tele_ctrl_prop: {
                const bool has_tele_ctrl = map::g_player->m_properties.has(prop::Id::tele_ctrl);
                const bool is_confused = map::g_player->m_properties.has(prop::Id::confused);

                return has_tele_ctrl && !is_confused;
        }
        }

        ASSERT(false);

        return false;
}

static void filter_out_near(const P& origin, std::vector<P>& positions)
{
        // Find the distance of the furthest position - this is the highest minimum distance.
        int furthest_dist = 0;

        for (const P& p : positions) {
                const int d = king_dist(origin, p);

                furthest_dist = std::max(d, furthest_dist);
        }

        int min_dist = 0;

        {
                const int desired_min_dist = g_fov_radi_int;

                min_dist = std::min(desired_min_dist, furthest_dist);
        }

        // Remove all positions close than the minimum distance.
        for (auto it = std::begin(positions); it != std::end(positions);) {
                const P p = *it;

                const int d = king_dist(origin, p);

                if (d < min_dist) {
                        positions.erase(it);
                }
                else {
                        ++it;
                }
        }
}

// -----------------------------------------------------------------------------
// teleport
// -----------------------------------------------------------------------------
void teleport(
        actor::Actor& actor,
        const ShouldCtrlTele ctrl_tele,
        const int max_dist)
{
        // First run a floodfill with some terrain unblocked.
        Array2<bool> blocked(map::dims());

        map_parsers::BlocksActor(actor, ParseActors::no)
                .run(blocked, blocked.rect());

        const size_t nr_positions = map::nr_positions();

        // Allow teleporting past doors.
        for (size_t i = 0; i < nr_positions; ++i) {
                if (map::g_terrain.at(i)->id() == terrain::Id::door) {
                        blocked.at(i) = false;
                }
        }

        // Allow teleporting past force fields.
        for (const terrain::Terrain* const mob : game_time::g_mobs) {
                if (mob->id() == terrain::Id::force_field) {
                        blocked.at(mob->pos()) = false;
                }
        }

        // Run the floodfill, mark positions that are either unreached or too far away as
        // blocked. Also mark starting position as blocked.
        const Array2<int> flood = floodfill(actor.m_pos, blocked);

        for (const P& p : map::rect().positions()) {
                if (flood.at(p) <= 0) {
                        // Unreached, or starting position.
                        blocked.at(p) = true;
                }

                if (max_dist > 0) {
                        const int dist = king_dist(actor.m_pos, p);

                        if (dist > max_dist) {
                                // Too far away.
                                blocked.at(p) = true;
                        }
                }
        }

        // Do not allow teleporting into any position that blocks this actor
        map_parsers::BlocksActor(actor, ParseActors::yes)
                .run(blocked, blocked.rect(), MapParseMode::append);

        // Do not allow teleporting into any position that blocks walking (otherwise for example
        // ethereal monsters could teleport far into the walls).
        map_parsers::BlocksWalking(ParseActors::no)
                .run(blocked, blocked.rect(), MapParseMode::append);

        // Teleport control?
        if (actor::is_player(&actor) && should_player_ctrl_tele(ctrl_tele)) {
                states::run_until_state_done(
                        std::make_unique<CtrlTele>(
                                actor.m_pos,
                                blocked,
                                max_dist));

                return;
        }

        // No teleport control - teleport randomly.
        std::vector<P> pos_bucket =
                to_vec(
                        blocked,
                        false,  // Store false values
                        blocked.rect());

        filter_out_near(actor.m_pos, pos_bucket);

        if (pos_bucket.empty()) {
                return;
        }

        const P tgt_pos = rnd::element(pos_bucket);

        teleport(actor, tgt_pos, blocked, false);
}

void teleport(
        actor::Actor& actor,
        P pos,
        const Array2<bool>& blocked,
        const bool has_tele_ctrl)
{
        const bool player_can_see_actor_before = actor::can_player_see_actor(actor);

        if (!actor::is_player(&actor)) {
                actor.m_mon_aware_state.player_aware_of_me_counter = 0;
        }

        const std::vector<prop::Id> props_ended = {
                prop::Id::entangled,
                prop::Id::stuck,
                prop::Id::nailed};

        const prop::PropEndConfig cfg(
                prop::PropEndAllowCallEndHook::no,
                prop::PropEndAllowMsg::no,
                prop::PropEndAllowHistoricMsg::yes);

        for (const prop::Id id : props_ended) {
                actor.m_properties.end_prop(id, cfg);
        }

        // Hostile void travelers "intercepts" players teleporting, and calls the player to them.
        const bool is_affected_by_void_traveler =
                handle_void_traveler_affecting_player_teleport(
                        actor,
                        pos,
                        blocked);

        // Leave current position.
        map::g_terrain.at(actor.m_pos)->on_leave(actor);

        // Update actor position to new position
        actor.m_pos = pos;

        if (actor::is_player(&actor)) {
                viewport::show(map::g_player->m_pos, viewport::ForceCentering::yes);
        }

        map::update_vision();

        if (actor::is_player(&actor)) {
                actor.update_tmp_shock();

                make_all_mon_not_seeing_player_unaware();
        }
        else if (player_can_see_actor_before) {
                const bool player_can_see_actor = actor::can_player_see_actor(actor);

                if (!player_can_see_actor) {
                        actor.m_mon_aware_state.player_aware_of_me_counter = 0;
                }

                const std::string actor_name_the =
                        text_format::first_to_upper(actor::name_the(actor));

                const std::string msg_ending =
                        player_can_see_actor
                        ? common_text::g_mon_disappear_reappear
                        : common_text::g_mon_disappear;

                msg_log::add(actor_name_the + " " + msg_ending);
        }

        actor::make_player_aware_seen_monsters();

        const bool is_confused = actor.m_properties.has(prop::Id::confused);

        if (actor::is_player(&actor) &&
            (!has_tele_ctrl ||
             is_confused ||
             is_affected_by_void_traveler)) {
                confuse_player();
        }

        // Bump the target terrain.
        map::g_terrain.at(pos)->bump(actor);
}

// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "ai.hpp"

#include <algorithm>
#include <string>

#include "ability_values.hpp"
#include "actor.hpp"
#include "actor_data.hpp"
#include "actor_mon.hpp"
#include "actor_move.hpp"
#include "actor_player.hpp"
#include "actor_see.hpp"
#include "actor_sneak.hpp"
#include "array2.hpp"
#include "debug.hpp"
#include "direction.hpp"
#include "fov.hpp"
#include "game_time.hpp"
#include "line_calc.hpp"
#include "map.hpp"
#include "map_parsing.hpp"
#include "misc.hpp"
#include "msg_log.hpp"
#include "pathfind.hpp"
#include "pos.hpp"
#include "property_handler.hpp"
#include "random.hpp"
#include "rect.hpp"
#include "spells.hpp"
#include "terrain.hpp"
#include "terrain_data.hpp"
#include "terrain_door.hpp"
#include "text_format.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
// Check if position is on a line between two points
static bool is_pos_on_line(const P& p, const P& line_p0, const P& line_p1)
{
        // First, do a cheaper check and just see if we are outside a rectangle
        // defined by the two points. If we are outside this area, we can't
        // possibly be on a line between the points.
        const R r(
                std::min(line_p0.x, line_p1.x),
                std::min(line_p0.y, line_p1.y),
                std::max(line_p0.x, line_p1.x),
                std::max(line_p0.y, line_p1.y));

        if (!r.is_pos_inside(p))
        {
                return false;
        }

        // OK, we could be on the line!

        const auto line =
                line_calc::calc_new_line(
                        line_p0,
                        line_p1,
                        true,
                        9999,
                        false);

        return (
                std::any_of(
                        std::cbegin(line),
                        std::cend(line),
                        [&p](const auto& pos_in_line) {
                                return p == pos_in_line;
                        }));
}

// Returns all free positions around the acting monster that is closer to the
// player than the monster's current position
static std::vector<P> move_bucket(actor::Mon& mon)
{
        std::vector<P> bucket;

        const P& mon_p = mon.m_pos;

        const P& player_p = map::g_player->m_pos;

        Array2<bool> blocked(map::dims());

        const R area_to_check_blocked(mon_p - P(1, 1), mon_p + P(1, 1));

        map_parsers::BlocksActor(mon, ParseActors::yes)
                .run(blocked,
                     area_to_check_blocked,
                     MapParseMode::overwrite);

        for (const P& d : dir_utils::g_dir_list)
        {
                const P target_p = mon_p + d;

                const int current_dist_to_player = king_dist(mon_p, player_p);

                const int target_dist_to_player = king_dist(target_p, player_p);

                if ((target_dist_to_player <= current_dist_to_player) &&
                    !blocked.at(target_p))
                {
                        bucket.push_back(target_p);
                }
        }

        return bucket;
}

// -----------------------------------------------------------------------------
// ai
// -----------------------------------------------------------------------------
namespace ai
{
namespace action
{
DidAction try_cast_random_spell(actor::Mon& mon)
{
        if (!mon.is_alive() ||
            mon.m_mon_spells.empty() ||
            !mon.m_properties.allow_cast_intr_spell_absolute(Verbose::no))
        {
                return DidAction::no;
        }

        rnd::shuffle(mon.m_mon_spells);

        for (auto& spell : mon.m_mon_spells)
        {
                int& cooldown = spell.cooldown;

                if ((cooldown > 0) ||
                    !spell.spell->allow_mon_cast_now(mon))
                {
                        continue;
                }

                const int spell_max_spi =
                        spell.spell->spi_cost_range(spell.skill, &mon)
                                .max;

                const int max_hp = actor::max_hp(mon);

                const bool has_spi =
                        spell_max_spi < mon.m_sp;

                const bool is_hostile_player =
                        !map::g_player->is_leader_of(&mon);

                const bool is_low_hp = mon.m_hp < (max_hp / 3);

                // Only cast the spell if monster has enough spirit - or
                // sometimes try anyway if the monster has low HP and is
                // hostile to the player.
                if (has_spi ||
                    (is_hostile_player &&
                     is_low_hp &&
                     rnd::one_in(20)))
                {
                        if (!has_spi &&
                            actor::can_player_see_actor(mon))
                        {
                                const std::string mon_name_the =
                                        text_format::first_to_upper(
                                                mon.name_the());

                                msg_log::add(mon_name_the + " looks desperate.");
                        }

                        cooldown = spell.spell->mon_cooldown();

                        spell.spell->cast(&mon, spell.skill, SpellSrc::learned);

                        return DidAction::yes;
                }
        }

        return DidAction::no;
}

DidAction handle_closed_blocking_door(actor::Mon& mon, std::vector<P>& path)
{
        if (!mon.is_alive() || path.empty())
        {
                return DidAction::no;
        }

        const auto& p = path.back();

        auto* const t = map::g_terrain.at(p);

        if ((t->id() != terrain::Id::door) || t->can_move(mon))
        {
                return DidAction::no;
        }

        // This is a door which is blocking the monster

        auto* const door = static_cast<terrain::Door*>(t);

        // There should never be a path past metal doors
        ASSERT(door->type() != terrain::DoorType::metal);

        if (door->type() == terrain::DoorType::metal)
        {
                return DidAction::no;
        }

        const bool is_stuck = door->is_stuck();

        const bool mon_can_bash = mon.m_data->can_bash_doors;

        const bool mon_can_open = mon.m_data->can_open_doors;

        // There should never be a path past a door if the monster can neither
        // bash nor open
        ASSERT(mon_can_bash || mon_can_open);

        if (!mon_can_bash && !mon_can_open)
        {
                return DidAction::no;
        }

        // Open the door?
        if (mon_can_open && !is_stuck)
        {
                door->actor_try_open(mon);

                return DidAction::yes;
        }

        // Bash the door?
        if (mon_can_bash && (is_stuck || !mon_can_open))
        {
                // When bashing doors, give the bashing monster some bonus
                // awareness time (because monsters trying to bash down doors is
                // a pretty central part of the game, and they should not give
                // up so easily)
                if (!mon.is_actor_my_leader(map::g_player) &&
                    rnd::fraction(3, 5))
                {
                        ++mon.m_mon_aware_state.aware_counter;
                }

                if (actor::can_player_see_actor(mon))
                {
                        const std::string mon_name_the =
                                text_format::first_to_upper(
                                        mon.name_the());

                        const std::string door_name =
                                door->base_name_short();

                        msg_log::add(
                                mon_name_the +
                                " bashes at the " +
                                door_name +
                                "!");
                }

                door->hit(DmgType::blunt, &mon);

                game_time::tick();

                return DidAction::yes;
        }

        return DidAction::no;
}

DidAction handle_inventory(actor::Mon& mon)
{
        (void)mon;
        return DidAction::no;
}

DidAction make_room_for_friend(actor::Mon& mon)
{
        if (!mon.is_alive())
        {
                return DidAction::no;
        }

        Array2<bool> blocked_los(map::dims());

        map_parsers::BlocksLos()
                .run(blocked_los, blocked_los.rect());

        if (!can_mon_see_actor(mon, *map::g_player, blocked_los))
        {
                return DidAction::no;
        }

        const P& player_p = map::g_player->m_pos;

        // Check if there is an allied monster that we should move away for
        for (auto* other_actor : game_time::g_actors)
        {
                if (other_actor == &mon ||
                    !other_actor->is_alive() ||
                    other_actor->is_player() ||
                    map::g_player->is_leader_of(other_actor))
                {
                        continue;
                }

                auto* const other_mon = static_cast<actor::Mon*>(other_actor);

                const bool is_other_adj =
                        is_pos_adj(mon.m_pos, other_mon->m_pos, false);

                // TODO: It's probably better to check LOS than vision
                // here? We don't want to move out of the way for a
                // blind monster.
                const bool is_other_seeing_player =
                        actor::can_mon_see_actor(
                                *other_mon,
                                *map::g_player,
                                blocked_los);

                /*
                  Do we have this situation?
                  #####
                  #.A.#
                  #@#B#
                  #####
                */
                const bool is_other_adj_with_no_player_los =
                        is_other_adj && !is_other_seeing_player;

                // We consider moving out of the way if the other
                // monster EITHER:
                //  * Is seeing the player and we are blocking it, OR
                //  * Is adjacent to us, and is not seeing the player.
                const bool is_between =
                        is_pos_on_line(
                                mon.m_pos,
                                other_mon->m_pos,
                                player_p);

                if ((is_other_seeing_player && is_between) ||
                    is_other_adj_with_no_player_los)
                {
                        // We are blocking a friend! Try to find an adjacent
                        // free cell, which:
                        // * Is NOT further away from the player than our
                        //   current position, and
                        // * Is not also blocking another monster

                        // NOTE: We do not care whether the target cell has LOS
                        // to the player or not. If we move into a cell without
                        // LOS, it will appear as if we are dodging in and out
                        // of cover. It lets us move towards the player with
                        // less time in the player's LOS, and allows blocked
                        // ranged monsters to shoot at the player.

                        // Get a list of neighbouring free cells
                        auto pos_bucket = move_bucket(mon);

                        // Sort the list by distance to player
                        IsCloserToPos cmp(player_p);
                        sort(pos_bucket.begin(), pos_bucket.end(), cmp);

                        // Try to find a position not blocking a third
                        // allied monster
                        for (const auto& target_p : pos_bucket)
                        {
                                bool is_p_ok = true;

                                for (auto* actor3 : game_time::g_actors)
                                {
                                        // NOTE: The third actor here can include the original
                                        // blocked "other" actor, since we must also check if we
                                        // block that actor from the target position
                                        if (actor3 != &mon &&
                                            actor3->is_alive() &&
                                            !actor3->is_player() &&
                                            !map::g_player->is_leader_of(actor3))
                                        {
                                                auto* const mon3 = static_cast<actor::Mon*>(actor3);

                                                const bool other_is_seeing_player =
                                                        actor::can_mon_see_actor(*mon3, *map::g_player, blocked_los);

                                                // TODO: We also need to check that we don't move
                                                // into a cell which is adjacent to a third
                                                // monster, who does not have LOS to player.
                                                // As it is now, we may move out of the way
                                                // for one such monster, only to block
                                                // another in the same way.

                                                if (other_is_seeing_player &&
                                                    is_pos_on_line(target_p, mon3->m_pos, player_p))
                                                {
                                                        is_p_ok = false;
                                                        break;
                                                }
                                        }
                                }

                                if (is_p_ok)
                                {
                                        const P offset = target_p - mon.m_pos;

                                        actor::move(mon, dir_utils::dir(offset));

                                        return DidAction::yes;
                                }
                        }
                }
        }

        return DidAction::no;
}

DidAction move_to_random_adj_cell(actor::Mon& mon)
{
        if (!mon.is_alive())
        {
                return DidAction::no;
        }

        if ((mon.m_ai_state.is_roaming_allowed == MonRoamingAllowed::no) &&
            !mon.is_aware_of_player())
        {
                return DidAction::no;
        }

        Array2<bool> blocked(map::dims());

        const R parse_area(mon.m_pos - 1, mon.m_pos + 1);

        map_parsers::BlocksActor(mon, ParseActors::yes)
                .run(blocked,
                     parse_area,
                     MapParseMode::overwrite);

        const R area_allowed(P(1, 1), P(map::w() - 2, map::h() - 2));

        // First, try the same direction as last travelled
        Dir dir = Dir::END;

        const Dir last_dir_moved = mon.m_ai_state.last_dir_moved;

        if (last_dir_moved != Dir::center &&
            last_dir_moved != Dir::END)
        {
                const P target_p(
                        mon.m_pos +
                        dir_utils::offset(last_dir_moved));

                if (!blocked.at(target_p) &&
                    is_pos_inside(target_p, area_allowed))
                {
                        dir = last_dir_moved;
                }
        }

        // Attempt to find a random non-blocked adjacent cell
        if (dir == Dir::END)
        {
                std::vector<Dir> dir_bucket;
                dir_bucket.clear();

                for (const P& d : dir_utils::g_dir_list)
                {
                        const P target_p(mon.m_pos + d);

                        if (!blocked.at(target_p) &&
                            is_pos_inside(target_p, area_allowed))
                        {
                                dir_bucket.push_back(dir_utils::dir(d));
                        }
                }

                if (!dir_bucket.empty())
                {
                        const auto idx =
                                rnd::range(0, (int)dir_bucket.size() - 1);

                        dir = dir_bucket[idx];
                }
        }

        // Valid direction found?
        if (dir != Dir::END)
        {
                actor::move(mon, dir);

                return DidAction::yes;
        }

        return DidAction::no;
}

DidAction move_to_target_simple(actor::Mon& mon)
{
        if (!mon.is_alive() ||
            !mon.m_ai_state.target ||
            !mon.m_ai_state.is_target_seen)
        {
                return DidAction::no;
        }

        const auto offset = mon.m_ai_state.target->m_pos - mon.m_pos;
        const auto signs = offset.signs();
        const auto new_pos = mon.m_pos + signs;

        const bool is_blocked =
                map_parsers::BlocksActor(mon, ParseActors::yes)
                        .run(new_pos);

        if (!is_blocked)
        {
                actor::move(mon, dir_utils::dir(signs));

                return DidAction::yes;
        }

        return DidAction::no;
}

DidAction step_path(actor::Mon& mon, const std::vector<P>& path)
{
        if (mon.is_alive() &&
            !path.empty())
        {
                const P delta = path.back() - mon.m_pos;

                actor::move(mon, dir_utils::dir(delta));

                return DidAction::yes;
        }

        return DidAction::no;
}

DidAction step_to_lair_if_los(actor::Mon& mon, const P& lair_p)
{
        if (mon.is_alive())
        {
                Array2<bool> blocked(map::dims());

                const R area_check_blocked =
                        fov::fov_rect(mon.m_pos, blocked.dims());

                map_parsers::BlocksLos()
                        .run(blocked,
                             area_check_blocked,
                             MapParseMode::overwrite);

                FovMap fov_map;
                fov_map.hard_blocked = &blocked;
                fov_map.light = &map::g_light;
                fov_map.dark = &map::g_dark;

                const LosResult los =
                        fov::check_cell(mon.m_pos, lair_p, fov_map);

                if (!los.is_blocked_hard)
                {
                        const auto d = (lair_p - mon.m_pos).signs();

                        const auto target_p = mon.m_pos + d;

                        const bool is_blocked =
                                map_parsers::BlocksActor(mon, ParseActors::yes)
                                        .run(target_p);

                        if (is_blocked)
                        {
                                return DidAction::no;
                        }
                        else
                        {
                                // Step is not blocked
                                actor::move(mon, dir_utils::dir(d));

                                return DidAction::yes;
                        }
                }
        }

        return DidAction::no;
}

}  // namespace action

// -----------------------------------------------------------------------------
// info
// -----------------------------------------------------------------------------
namespace info
{
bool look(actor::Mon& mon)
{
        if (!mon.is_alive())
        {
                return false;
        }

        const auto seeable_foes = actor::seeable_foes_for_mon(mon);

        if (seeable_foes.empty())
        {
                return false;
        }

        if (mon.is_aware_of_player())
        {
                mon.become_aware_player(actor::AwareSource::other);

                return false;
        }

        for (auto* actor : seeable_foes)
        {
                if (actor->is_player())
                {
                        actor::SneakParameters p;

                        p.actor_sneaking = actor;
                        p.actor_searching = &mon;

                        const auto result = actor::roll_sneak(p);

                        const bool is_non_critical_fail =
                                (result == ActionResult::fail) ||
                                (result == ActionResult::fail_big);

                        // Become aware if:
                        // * We got a critical fail, OR
                        // * We got a non-critical fail, and we are wary
                        const bool become_aware =
                                (result == ActionResult::fail_critical) ||
                                (is_non_critical_fail &&
                                 mon.is_wary_of_player());

                        if (become_aware)
                        {
                                map::update_vision();

                                mon.become_aware_player(
                                        actor::AwareSource::seeing);
                        }
                        // Not aware, just become wary if non-critical fail
                        else if (is_non_critical_fail)
                        {
                                map::update_vision();

                                mon.become_wary_player();
                        }
                }
                else
                {
                        // Other actor is monster
                        map::update_vision();

                        mon.become_aware_player(actor::AwareSource::other);
                }

                // Did the monster become aware?
                if (mon.is_aware_of_player())
                {
                        return true;
                }
        }

        return false;
}

std::vector<P> find_path_to_lair_if_no_los(actor::Mon& mon, const P& lair_p)
{
        if (!mon.is_alive())
        {
                return {};
        }

        Array2<bool> blocked(map::dims());

        const R fov_lmt = fov::fov_rect(mon.m_pos, blocked.dims());

        map_parsers::BlocksLos()
                .run(blocked,
                     fov_lmt,
                     MapParseMode::overwrite);

        FovMap fov_map;
        fov_map.hard_blocked = &blocked;
        fov_map.dark = &map::g_dark;
        fov_map.light = &map::g_light;

        const LosResult los = fov::check_cell(mon.m_pos, lair_p, fov_map);

        if (!los.is_blocked_hard)
        {
                return {};
        }

        map_parsers::BlocksActor(mon, ParseActors::no)
                .run(blocked, blocked.rect());

        map_parsers::LivingActorsAdjToPos(mon.m_pos)
                .run(blocked,
                     blocked.rect(),
                     MapParseMode::append);

        return pathfind(mon.m_pos, lair_p, blocked);
}

std::vector<P> find_path_to_leader(actor::Mon& mon)
{
        if (!mon.is_alive())
        {
                return {};
        }

        auto* leader = mon.m_leader;

        if (!leader || !leader->is_alive())
        {
                return {};
        }

        Array2<bool> blocked(map::dims());

        const R fov_lmt = fov::fov_rect(mon.m_pos, blocked.dims());

        map_parsers::BlocksLos()
                .run(
                        blocked,
                        fov_lmt,
                        MapParseMode::overwrite);

        FovMap fov_map;
        fov_map.hard_blocked = &blocked;
        fov_map.dark = &map::g_dark;
        fov_map.light = &map::g_light;

        const LosResult los =
                fov::check_cell(mon.m_pos, leader->m_pos, fov_map);

        if (!los.is_blocked_hard)
        {
                return {};
        }

        map_parsers::BlocksActor(mon, ParseActors::no)
                .run(
                        blocked,
                        blocked.rect());

        map_parsers::LivingActorsAdjToPos(mon.m_pos)
                .run(
                        blocked,
                        blocked.rect(),
                        MapParseMode::append);

        return pathfind(mon.m_pos, leader->m_pos, blocked);
}

std::vector<P> find_path_to_target(actor::Mon& mon)
{
        if (!mon.is_alive() || !mon.m_ai_state.target)
        {
                return {};
        }

        const auto& target = *mon.m_ai_state.target;

        // Monsters should not pathfind to the target if there is LOS, but they
        // cannot see the target (e.g. the target is invisible).
        //
        // If the target is invisible for example, we want pathfinding as long
        // as the monster is aware and is around a corner (i.e. they are guided
        // by sound or something else) - but when they come into LOS of an
        // invisible target, they should not approach further.
        //
        // This creates a nice effect, where monsters appear a bit confused that
        // they cannot see anyone when they should have come into sight.
        Array2<bool> blocked(map::dims());

        const int los_x0 = std::min(target.m_pos.x, mon.m_pos.x);
        const int los_y0 = std::min(target.m_pos.y, mon.m_pos.y);
        const int los_x1 = std::max(target.m_pos.x, mon.m_pos.x);
        const int los_y1 = std::max(target.m_pos.y, mon.m_pos.y);

        map_parsers::BlocksLos()
                .run(blocked,
                     R(los_x0, los_y0, los_x1, los_y1),
                     MapParseMode::overwrite);

        FovMap fov_map;
        fov_map.hard_blocked = &blocked;
        fov_map.light = &map::g_light;
        fov_map.dark = &map::g_dark;

        if (!mon.m_ai_state.is_target_seen)
        {
                LosResult los_result =
                        fov::check_cell(
                                mon.m_pos,
                                target.m_pos,
                                fov_map);

                if (!los_result.is_blocked_hard &&
                    !los_result.is_blocked_by_dark)
                {
                        return {};
                }
        }

        // Monster does not have LOS to target - alright, let's go!

        // NOTE: Only actors adjacent to the monster are considered to be
        // blokcing the path
        auto blocked_parser = map_parsers::BlocksActor(mon, ParseActors::no);

        for (int x = 0; x < map::w(); ++x)
        {
                for (int y = 0; y < map::h(); ++y)
                {
                        const P p(x, y);

                        blocked.at(p) = false;

                        if (!blocked_parser.run(p))
                        {
                                continue;
                        }

                        // This cell is blocked

                        const auto* const t = map::g_terrain.at(p);

                        if (t->id() == terrain::Id::door)
                        {
                                // NOTE: The door is guaranteed to be closed,
                                // since this point is only reached for terrains
                                // considered blocking by the map parser

                                const auto* const door =
                                        static_cast<const terrain::Door*>(t);

                                bool should_door_block = false;

                                if (door->type() == terrain::DoorType::metal)
                                {
                                        // Metal door
                                        should_door_block = true;
                                }
                                else if (door->is_stuck())
                                {
                                        // Stuck non-metal door
                                        if (!mon.m_data->can_bash_doors)
                                        {
                                                should_door_block = true;
                                        }
                                }
                                else
                                {
                                        // Non-stuck, non-metal door
                                        if (!mon.m_data->can_bash_doors &&
                                            !mon.m_data->can_open_doors)
                                        {
                                                should_door_block = true;
                                        }
                                }

                                if (should_door_block)
                                {
                                        blocked.at(p) = true;
                                }
                        }
                        else
                        {
                                // Not a door (e.g. a wall)
                                blocked.at(p) = true;
                        }
                }
        }

        // Append living adjacent actors to the blocking array
        map_parsers::LivingActorsAdjToPos(mon.m_pos)
                .run(blocked,
                     blocked.rect(),
                     MapParseMode::append);

        return pathfind(mon.m_pos, target.m_pos, blocked);
}

}  // namespace info
}  // namespace ai

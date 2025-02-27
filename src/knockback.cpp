// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "knockback.hpp"

#include <string>
#include <vector>

#include "actor.hpp"
#include "actor_data.hpp"
#include "actor_death.hpp"
#include "actor_move.hpp"
#include "actor_see.hpp"
#include "array2.hpp"
#include "colors.hpp"
#include "config.hpp"
#include "debug.hpp"
#include "game.hpp"
#include "game_time.hpp"
#include "map.hpp"
#include "map_parsing.hpp"
#include "msg_log.hpp"
#include "pos.hpp"
#include "property.hpp"
#include "property_data.hpp"
#include "property_factory.hpp"
#include "property_handler.hpp"
#include "terrain.hpp"
#include "terrain_data.hpp"
#include "text_format.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static const std::vector<prop::Id> s_prop_ids_cannot_knock_back = {
        prop::Id::entangled,
        prop::Id::stuck,
        prop::Id::ethereal,
        prop::Id::ooze,
};

static const std::vector<terrain::Id> s_terrain_ids_deep = {
        terrain::Id::chasm,
};

static bool can_creature_be_knocked_back(const actor::Actor& actor)
{
        // TODO: Previously knockback was for some reason prevented against the player if the bot
        // was playing ("config::is_bot_playing()"). Consider if that is necessary.

        if (
                actor.m_data->prevent_knockback ||
                (actor.m_data->actor_size >= actor::Size::giant)) {
                return false;
        }

        const bool prop_prevents_knockback =
                std::any_of(
                        std::cbegin(s_prop_ids_cannot_knock_back),
                        std::cend(s_prop_ids_cannot_knock_back),
                        [&actor](const auto id) {
                                return actor.m_properties.has(id);
                        });

        if (prop_prevents_knockback) {
                return false;
        }

        return true;
}

static void nail_actor(actor::Actor& actor)
{
        prop::Prop* prop = prop::make(prop::Id::nailed);

        prop->set_indefinite();

        actor.m_properties.apply(prop);
}

static void paralyze_actor(actor::Actor& actor, const int paralyze_extra_turns)
{
        prop::Prop* prop = prop::make(prop::Id::paralyzed);

        prop->set_duration(1 + paralyze_extra_turns);

        actor.m_properties.apply(prop);
}

static std::string mon_shown_name(const actor::Actor& actor)
{
        const bool is_seen = actor::can_player_see_actor(actor);

        return (
                is_seen
                        ? text_format::first_to_upper(actor::name_the(actor))
                        : "It");
}

static bool is_mon_noticed_by_player(const actor::Actor& actor)
{
        // The monster is noticed by the player if it is seen, or if the player is aware of the
        // monster and can see their position.
        return (
                actor::can_player_see_actor(actor) ||
                (actor::is_player_aware_of_me(actor) && map::g_seen.at(actor.m_pos)));
}

static void print_msg_actor_knocked_back(const actor::Actor& actor)
{
        if (is_mon_noticed_by_player(actor)) {
                if (actor::is_player(&actor)) {
                        msg_log::add("I am knocked back!");
                }
                else {
                        const std::string name = mon_shown_name(actor);

                        msg_log::add(name + " is knocked back!");
                }
        }
}

static void print_msg_fall_into_chasm(const actor::Actor& actor)
{
        if (is_mon_noticed_by_player(actor)) {
                if (actor::is_player(&actor)) {
                        msg_log::add("I perish in the depths!", colors::msg_bad());
                }
                else {
                        const std::string name = mon_shown_name(actor);

                        msg_log::add(name + " perishes in the depths.", colors::msg_good());
                }
        }
}

// -----------------------------------------------------------------------------
// knockback
// -----------------------------------------------------------------------------
namespace knockback
{
void run(
        actor::Actor& actor,
        const P& attacked_from_pos,
        const KnockbackSource source,
        const Verbose verbose,
        const int paralyze_extra_turns)
{
        TRACE_FUNC_BEGIN;

        ASSERT(paralyze_extra_turns >= 0);

        const bool is_player = actor::is_player(&actor);

        if (!can_creature_be_knocked_back(actor)) {
                TRACE_FUNC_END;

                return;
        }

        if (is_player) {
                map::g_player->interrupt_actions(ForceInterruptActions::yes);
        }

        const P d = (actor.m_pos - attacked_from_pos).signs();
        const P new_pos = actor.m_pos + d;

        if (map::living_actor_at(new_pos)) {
                // Target position is occupied by another actor
                return;
        }

        const bool actor_can_move_into_tgt_pos =
                !map_parsers::BlocksActor(actor, ParseActors::no)
                         .run(new_pos);

        const bool is_tgt_pos_deep =
                map_parsers::IsAnyOfTerrains(s_terrain_ids_deep)
                        .run(new_pos);

        const terrain::Terrain* const tgt_terrain = map::g_terrain.at(new_pos);

        if (!actor_can_move_into_tgt_pos && !is_tgt_pos_deep) {
                // Actor nailed to a wall from a spike gun?
                if ((source == KnockbackSource::spike_gun) &&
                    !tgt_terrain->is_projectile_passable() &&
                    !actor.m_properties.has(prop::Id::r_phys)) {
                        nail_actor(actor);
                }

                TRACE_FUNC_END;

                return;
        }

        const bool can_player_see_actor = actor::can_player_see_actor(actor);

        const bool is_player_aware_of_actor =
                is_player
                ? true
                : actor::is_player_aware_of_me(actor);

        if (verbose == Verbose::yes) {
                print_msg_actor_knocked_back(actor);
        }

        const P previous_pos = actor.m_pos;

        // Leave current position.
        map::g_terrain.at(actor.m_pos)->on_leave(actor);

        actor.m_pos = new_pos;

        // Bump player awareness of the monster if the monster was seen, or if the player was aware
        // of the monster and the previous position was seen.
        if (!is_player &&
            (can_player_see_actor ||
             (is_player_aware_of_actor && map::g_seen.at(previous_pos)))) {
                actor::make_player_aware_mon(actor);
        }

        if (!actor_can_move_into_tgt_pos && is_tgt_pos_deep) {
                print_msg_fall_into_chasm(actor);

                actor::kill(actor, IsDestroyed::yes, AllowGore::no, AllowDropItems::no);

                TRACE_FUNC_END;

                return;
        }

        map::update_vision();

        paralyze_actor(actor, paralyze_extra_turns);

        // Bump target position.
        const std::vector<terrain::Terrain*> mobs = game_time::mobs_at(actor.m_pos);

        for (terrain::Terrain* const mob : mobs) {
                mob->bump(actor);
        }

        if (!actor::is_alive(actor)) {
                TRACE_FUNC_END;
                return;
        }

        map::g_terrain.at(actor.m_pos)->bump(actor);

        TRACE_FUNC_END;
}

}  // namespace knockback

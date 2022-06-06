// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
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
static const std::vector<PropId> s_props_cannot_knock_back = {
        PropId::entangled,
        PropId::ethereal,
        PropId::ooze,
};

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

        if (actor.m_data->prevent_knockback ||
            (actor.m_data->actor_size >= actor::Size::giant) ||
            (is_player && config::is_bot_playing()))
        {
                TRACE_FUNC_END;

                return;
        }

        const bool prop_prevents_knockback =
                std::any_of(
                        std::cbegin(s_props_cannot_knock_back),
                        std::cend(s_props_cannot_knock_back),
                        [&actor](const auto id) {
                                return actor.m_properties.has(id);
                        });

        if (prop_prevents_knockback)
        {
                TRACE_FUNC_END;

                return;
        }

        if (is_player)
        {
                map::g_player->interrupt_actions(ForceInterruptActions::yes);
        }

        const auto d = (actor.m_pos - attacked_from_pos).signs();
        const auto new_pos = actor.m_pos + d;

        if (map::living_actor_at(new_pos))
        {
                // Target position is occupied by another actor
                return;
        }

        const bool actor_can_move_into_tgt_pos =
                map::can_actor_move_into_terrain_at(actor, new_pos);

        const std::vector<terrain::Id> deep_terrains = {
                terrain::Id::chasm};

        const bool is_tgt_pos_deep =
                map_parsers::IsAnyOfTerrains(deep_terrains)
                        .run(new_pos);

        auto& tgt_terrain = map::g_terrain.at(new_pos);

        if (!actor_can_move_into_tgt_pos &&
            !is_tgt_pos_deep &&
            !actor.m_properties.has(PropId::r_phys))
        {
                // Actor nailed to a wall from a spike gun?
                if (source == KnockbackSource::spike_gun)
                {
                        if (!tgt_terrain->is_projectile_passable())
                        {
                                auto* prop =
                                        property_factory::make(
                                                PropId::nailed);

                                prop->set_indefinite();

                                actor.m_properties.apply(prop);
                        }
                }

                TRACE_FUNC_END;

                return;
        }

        const bool player_can_see_actor =
                is_player
                ? true
                : actor::can_player_see_actor(actor);

        bool player_is_aware_of_actor = true;

        if (!is_player)
        {
                player_is_aware_of_actor = actor.is_player_aware_of_me();
        }

        std::string actor_name =
                player_can_see_actor
                ? text_format::first_to_upper(actor.name_the())
                : "It";

        if ((verbose == Verbose::yes) && player_is_aware_of_actor)
        {
                if (is_player)
                {
                        msg_log::add("I am knocked back!");
                }
                else
                {
                        msg_log::add(actor_name + " is knocked back!");
                }
        }

        // Leave current cell
        map::g_terrain.at(actor.m_pos)->on_leave(actor);

        actor::set_position(actor, new_pos);

        if (!is_player && player_can_see_actor)
        {
                actor::make_player_aware_mon(actor);
        }

        if (!actor_can_move_into_tgt_pos && is_tgt_pos_deep)
        {
                if (is_player)
                {
                        msg_log::add(
                                "I perish in the depths!",
                                colors::msg_bad());
                }
                else if (
                        player_is_aware_of_actor &&
                        map::g_seen.at(new_pos))
                {
                        msg_log::add(
                                actor_name + " perishes in the depths.",
                                colors::msg_good());
                }

                actor::kill(
                        actor,
                        IsDestroyed::yes,
                        AllowGore::no,
                        AllowDropItems::no);

                TRACE_FUNC_END;

                return;
        }

        map::update_vision();

        auto* prop = property_factory::make(PropId::paralyzed);

        prop->set_duration(1 + paralyze_extra_turns);

        actor.m_properties.apply(prop);

        // Bump target cell
        const auto mobs = game_time::mobs_at(actor.m_pos);

        for (auto* const mob : mobs)
        {
                mob->bump(actor);
        }

        if (!actor.is_alive())
        {
                TRACE_FUNC_END;
                return;
        }

        map::g_terrain.at(actor.m_pos)->bump(actor);

        TRACE_FUNC_END;
}

}  // namespace knockback

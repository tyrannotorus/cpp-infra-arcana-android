// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "actor_hear_sound.hpp"

#include <string>

#include "actor.hpp"
#include "audio.hpp"
#include "colors.hpp"
#include "debug.hpp"
#include "map.hpp"
#include "msg_log.hpp"
#include "property_data.hpp"
#include "property_handler.hpp"
#include "sound.hpp"

namespace actor
{
void hear_sound_player(
        const Snd& snd,
        const bool is_origin_seen_by_player,
        const Dir dir_to_origin,
        const int percent_audible_distance)
{
        if (map::g_player->m_properties.has(prop::Id::deaf)) {
                return;
        }

        const auto sfx = snd.sfx();
        const std::string& msg = snd.msg();
        const bool has_snd_msg = !msg.empty() && msg != " ";

        if (has_snd_msg) {
                const auto should_interrupt =
                        is_origin_seen_by_player
                        ? MsgInterruptPlayer::no
                        : MsgInterruptPlayer::yes;

                msg_log::add(msg, colors::text(), should_interrupt);
        }

        // Play audio after message to ensure sync between audio and animation.
        audio::play_from_direction(
                sfx,
                dir_to_origin,
                percent_audible_distance);

        if (has_snd_msg) {
                auto* const actor_who_made_snd = snd.actor_who_made_sound();

                if (actor_who_made_snd && !is_player(actor_who_made_snd)) {
                        actor_who_made_snd->make_player_aware_of_me();
                }
        }

        snd.on_heard(*map::g_player);
}

void hear_sound_mon(Actor& actor, const Snd& snd)
{
        if (is_player(&actor)) {
                ASSERT(false);

                return;
        }

        if (actor.m_properties.has(prop::Id::deaf)) {
                return;
        }

        snd.on_heard(actor);

        // NOTE: The monster may have become deaf through the sound callback
        // above (e.g. from the Horn of Deafening artifact).
        if (actor.m_properties.has(prop::Id::deaf)) {
                return;
        }

        if (actor.is_alive() && snd.is_alerting_mon()) {
                actor.become_aware_player(AwareSource::heard_sound);
        }
}

}  // namespace actor

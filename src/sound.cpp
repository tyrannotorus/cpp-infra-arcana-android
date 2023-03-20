// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "sound.hpp"

#include <string>
#include <utility>
#include <vector>

#include "actor.hpp"
#include "actor_hear_sound.hpp"
#include "actor_see.hpp"
#include "array2.hpp"
#include "debug.hpp"
#include "direction.hpp"
#include "flood.hpp"
#include "game_time.hpp"
#include "global.hpp"
#include "map.hpp"
#include "map_parsing.hpp"
#include "property_data.hpp"
#include "property_handler.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static int s_nr_snd_msg_printed_current_turn;

static int get_max_dist(const Snd& snd)
{
        switch (snd.volume()) {
        case SndVol::low:
                return g_fov_radi_int;

        case SndVol::high:
                return g_fov_radi_int * 2;

        case SndVol::global:
                return 999;
        }

        ASSERT(false);
        return g_fov_radi_int;
}

static bool is_snd_heard_at_range(const int range, const Snd& snd)
{
        return range <= get_max_dist(snd);
}

static Array2<int> calc_snd_flood(const Snd& snd, const int snd_max_dist)
{
        Array2<bool> blocks_sound(map::dims());

        map_parsers::BlocksSound()
                .run(blocks_sound, blocks_sound.rect());

        const auto& origin = snd.origin();

        // Never block the origin - we want to be able to run the sound from
        // e.g. a closing door, after it was closed (and we don't want this to
        // depend on the floodfill algorithm, so we explicitly set the origin to
        // free here).
        blocks_sound.at(origin) = false;

        auto flood =
                floodfill(
                        origin,
                        blocks_sound,
                        snd_max_dist,
                        {-1, -1},
                        true);

        flood.at(origin.x, origin.y) = 0;

        return flood;
}

static bool is_sound_origin_seen_by_player(
        const actor::Actor* const actor_who_made_sound,
        const P& origin)
{
        // NOTE: If we have an actor as "originator" of the sound, then the
        // origin of the sound is considered seen if the player can see this
        // actor - otherwise the origin is seen if the origin position is seen.
        if (actor_who_made_sound) {
                return actor::can_player_see_actor(*actor_who_made_sound);
        }
        else {
                return map::g_seen.at(origin);
        }
}

static void send_sound_to_player(
        Snd& snd,
        const int flood_val_at_actor,
        const int snd_max_dist)
{
        const auto origin = snd.origin();

        const bool is_origin_seen =
                is_sound_origin_seen_by_player(
                        snd.actor_who_made_sound(),
                        origin);

        if (is_origin_seen && snd.is_msg_ignored_if_origin_seen()) {
                snd.clear_msg();
        }

        if (!snd.msg().empty()) {
                // Add a direction to the message (i.e. "(NW)")
                if (map::g_player->m_pos != origin) {
                        const std::string dir_str =
                                dir_utils::compass_dir_name(
                                        map::g_player->m_pos,
                                        origin);

                        snd.add_string("(" + dir_str + ")");
                }
        }

        const int pct_dist = (flood_val_at_actor * 100) / snd_max_dist;
        const auto offset = (origin - map::g_player->m_pos).signs();
        const auto dir_to_origin = dir_utils::dir(offset);

        actor::hear_sound_player(snd, is_origin_seen, dir_to_origin, pct_dist);
}

static void send_sound_to_mon(actor::Actor& actor, Snd& snd)
{
        actor::hear_sound_mon(actor, snd);
}

static void send_sound_to_actor(
        actor::Actor& actor,
        Snd& snd,
        const int flood_val_at_actor,
        const int snd_max_dist)
{
        if (actor::is_player(&actor)) {
                send_sound_to_player(snd, flood_val_at_actor, snd_max_dist);
        }
        else {
                send_sound_to_mon(actor, snd);
        }
}

// -----------------------------------------------------------------------------
// Sound
// -----------------------------------------------------------------------------
Snd::Snd(
        std::string msg,
        const audio::SfxId sfx,
        const IgnoreMsgIfOriginSeen ignore_msg_if_origin_seen,
        const P& origin,
        actor::Actor* const actor_who_made_sound,
        const SndVol vol,
        const AlertsMon alerting_mon,
        std::shared_ptr<SndHeardEffect> snd_heard_effect) :

        m_msg(std::move(msg)),
        m_sfx(sfx),
        m_is_msg_ignored_if_origin_seen(ignore_msg_if_origin_seen),
        m_origin(origin),
        m_actor_who_made_sound(actor_who_made_sound),
        m_vol(vol),
        m_is_alerting_mon(alerting_mon),
        m_snd_heard_effect(std::move(snd_heard_effect))
{
}

Snd::~Snd() = default;

void Snd::run()
{
        snd_emit::run(*this);
}

void Snd::on_heard(actor::Actor& actor) const
{
        if (m_snd_heard_effect) {
                m_snd_heard_effect->run(actor);
        }
}

// -----------------------------------------------------------------------------
// snd_emit
// -----------------------------------------------------------------------------
namespace snd_emit
{
void reset_nr_snd_msg_printed_current_turn()
{
        s_nr_snd_msg_printed_current_turn = 0;
}

void run(Snd snd)
{
        ASSERT(snd.msg() != " ");

        const int snd_max_dist = get_max_dist(snd);

        auto flood = calc_snd_flood(snd, snd_max_dist);

        const auto& origin = snd.origin();

        for (auto* actor : game_time::g_actors) {
                const auto& actor_pos = actor->m_pos;

                const int flood_val_at_actor = flood.at(actor_pos);

                // Can the sound be heard here?

                if ((flood_val_at_actor == 0) && (actor_pos != origin)) {
                        continue;
                }

                if (!is_snd_heard_at_range(flood_val_at_actor, snd)) {
                        continue;
                }

                // Not hearing the sound due to short hearing range property?
                const int max_dist_short_hearing_range = 2;

                if (actor->m_properties.has(prop::Id::short_hearing_range) &&
                    (flood_val_at_actor > max_dist_short_hearing_range)) {
                        continue;
                }

                send_sound_to_actor(
                        *actor,
                        snd,
                        flood_val_at_actor,
                        snd_max_dist);
        }
}

}  // namespace snd_emit

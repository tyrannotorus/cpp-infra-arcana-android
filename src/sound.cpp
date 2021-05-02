// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "sound.hpp"

#include <string>
#include <utility>
#include <vector>

#include "actor.hpp"
#include "actor_mon.hpp"
#include "actor_player.hpp"
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
static const int s_snd_dist_normal = g_fov_radi_int;

static const int s_snd_dist_loud = s_snd_dist_normal * 2;

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
        if (m_snd_heard_effect)
        {
                m_snd_heard_effect->run(actor);
        }
}

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static int s_nr_snd_msg_printed_current_turn;

static int get_max_dist(const Snd& snd)
{
        return snd.is_loud()
                ? s_snd_dist_loud
                : s_snd_dist_normal;
}

static bool is_snd_heard_at_range(const int range, const Snd& snd)
{
        return range <= get_max_dist(snd);
}

// -----------------------------------------------------------------------------
// Sound emitting
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

        Array2<bool> blocked(map::dims());

        map_parsers::BlocksSound()
                .run(blocked, blocked.rect());

        const P& origin = snd.origin();

        // Never block the origin - we want to be able to run the sound from
        // e.g. a closing door, after it was closed (and we don't want this to
        // depend on the floodfill algorithm, so we explicitly set the origin to
        // free here)
        blocked.at(origin.x, origin.y) = false;

        const int snd_max_dist = get_max_dist(snd);

        auto flood =
                floodfill(
                        origin,
                        blocked,
                        snd_max_dist,
                        P(-1, -1),
                        true);

        flood.at(origin.x, origin.y) = 0;

        for (auto* actor : game_time::g_actors)
        {
                const P& actor_pos = actor->m_pos;

                const int flood_val_at_actor = flood.at(actor_pos);

                // Can the sound be heard at this distance?
                if (((flood_val_at_actor == 0) &&
                     (actor_pos != origin)) ||
                    !is_snd_heard_at_range(flood_val_at_actor, snd))
                {
                        continue;
                }

                // Not hearing the sound due to short hearing range property?
                const int max_dist_short_hearing_range = 2;

                if (actor->m_properties.has(PropId::short_hearing_range) &&
                    (flood_val_at_actor > max_dist_short_hearing_range))
                {
                        continue;
                }

                const bool is_origin_seen_by_player = map::g_seen.at(origin);

                if (actor->is_player())
                {
                        if (is_origin_seen_by_player &&
                            snd.is_msg_ignored_if_origin_seen())
                        {
                                snd.clear_msg();
                        }

                        if (!snd.msg().empty())
                        {
                                // Add a direction to the message (i.e. "(NW)")
                                if (actor_pos != origin)
                                {
                                        const std::string dir_str =
                                                dir_utils::compass_dir_name(
                                                        actor_pos,
                                                        origin);

                                        snd.add_string("(" + dir_str + ")");
                                }
                        }

                        const int pct_dist =
                                (flood_val_at_actor * 100) / snd_max_dist;

                        const P offset = (origin - actor_pos).signs();

                        const Dir dir_to_origin = dir_utils::dir(offset);

                        map::g_player->hear_sound(
                                snd,
                                is_origin_seen_by_player,
                                dir_to_origin,
                                pct_dist);
                }
                else
                {
                        // Not player
                        auto* const mon = static_cast<actor::Mon*>(actor);

                        mon->hear_sound(snd);
                }
        }
}

}  // namespace snd_emit

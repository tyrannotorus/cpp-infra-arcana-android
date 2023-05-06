// =============================================================================
// Copyright 2011-2023 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "actor_eat.hpp"

#include <algorithm>
#include <string>
#include <vector>

#include "actor.hpp"
#include "actor_data.hpp"
#include "actor_see.hpp"
#include "audio_data.hpp"
#include "game_time.hpp"
#include "msg_log.hpp"
#include "pos.hpp"
#include "property.hpp"
#include "property_data.hpp"
#include "property_handler.hpp"
#include "random.hpp"
#include "sound.hpp"
#include "terrain.hpp"
#include "text_format.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static bool can_heal_from_eating_now(const actor::Actor& actor)
{
        prop::Wound* wound = nullptr;

        if (is_player(&actor)) {
                auto* prop = actor.m_properties.prop(prop::Id::wound);

                if (prop) {
                        wound = static_cast<prop::Wound*>(prop);
                }
        }

        return (actor.m_hp < actor::max_hp(actor)) || wound;
}

static actor::Actor* find_corpse_at(const P& p)
{
        actor::Actor* corpse = nullptr;

        // Check all corpses here, if this is the player eating, stop at any
        // corpse which is prioritized for bashing (Zombies).
        for (auto* const actor : game_time::g_actors) {
                if ((actor->m_pos == p) &&
                    (actor->m_state == ActorState::corpse)) {
                        corpse = actor;

                        // NOTE: Monsters will also use this priority, but this
                        // is fine.
                        if (actor->m_data->prio_corpse_bash) {
                                break;
                        }
                }
        }

        return corpse;
}

static bool roll_corpse_destroyed(const actor::Actor& corpse)
{
        const int corpse_max_hp = corpse.m_base_max_hp;
        const int destr_one_in_n = std::clamp(corpse_max_hp / 4, 1, 8);

        return rnd::one_in(destr_one_in_n);
}

static void run_feed_snd(actor::Actor& actor)
{
        Snd snd(
                "I hear ripping and chewing.",
                audio::SfxId::bite,
                IgnoreMsgIfOriginSeen::yes,
                actor.m_pos,
                &actor,
                SndVol::low,
                AlertsMon::no);

        snd.run();
}

static void print_feed_msg(
        const actor::Actor& actor,
        const actor::Actor& corpse)
{
        const std::string corpse_name_the = corpse.m_data->corpse_name_the;

        if (actor::is_player(&actor)) {
                msg_log::add("I feed on " + corpse_name_the + ".");
        }
        else {
                if (can_player_see_actor(actor)) {
                        const std::string actor_name_the =
                                text_format::first_to_upper(
                                        actor.name_the());

                        msg_log::add(
                                actor_name_the +
                                " feeds on " +
                                corpse_name_the +
                                ".");
                }
        }
}

static void print_corpse_destroyed_msg(const actor::Actor& corpse)
{
        const std::string name =
                text_format::first_to_upper(
                        corpse.m_data->corpse_name_the);

        msg_log::add(name + " is completely devoured.");
}

static void print_corpses_remaining(const P& p)
{
        std::vector<actor::Actor*> corpses_here;

        for (auto* const actor : game_time::g_actors) {
                if ((actor->m_pos == p) &&
                    (actor->m_state == ActorState::corpse)) {
                        corpses_here.push_back(actor);
                }
        }

        if (corpses_here.empty()) {
                return;
        }

        msg_log::more_prompt();

        for (auto* const other_corpse : corpses_here) {
                const std::string name =
                        text_format::first_to_upper(
                                other_corpse->m_data
                                        ->corpse_name_a);

                msg_log::add(name + ".");
        }
}

// -----------------------------------------------------------------------------
// actor
// -----------------------------------------------------------------------------
namespace actor
{
DidAction try_eat_corpse(actor::Actor& actor)
{
        if (!can_heal_from_eating_now(actor)) {
                // "Not hungry"
                return DidAction::no;
        }

        Actor* corpse = find_corpse_at(actor.m_pos);

        if (!corpse) {
                return DidAction::no;
        }

        run_feed_snd(actor);

        print_feed_msg(actor, *corpse);

        const bool is_destroyed = roll_corpse_destroyed(*corpse);

        if (is_destroyed) {
                corpse->m_state = ActorState::destroyed;

                terrain::make_gore(actor.m_pos);
                terrain::make_blood(actor.m_pos);

                if (is_player(&actor)) {
                        print_corpse_destroyed_msg(*corpse);

                        print_corpses_remaining(actor.m_pos);
                }
        }

        heal_from_eating(actor);

        return DidAction::yes;
}

void heal_from_eating(actor::Actor& actor)
{
        const int hp_restored = rnd::range(3, 4);

        actor.restore_hp(hp_restored, false, Verbose::no);

        if (is_player(&actor)) {
                auto* const prop = actor.m_properties.prop(prop::Id::wound);

                if (prop && rnd::one_in(6)) {
                        auto* const wound = static_cast<prop::Wound*>(prop);

                        wound->heal_one_wound();
                }
        }
}

}  // namespace actor

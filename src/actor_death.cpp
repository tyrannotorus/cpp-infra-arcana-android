// =============================================================================
// Copyright 2011-2021 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "actor_death.hpp"

#include <string>
#include <vector>

#include "actor.hpp"
#include "actor_data.hpp"
#include "actor_mon.hpp"
#include "actor_player.hpp"
#include "actor_see.hpp"
#include "array2.hpp"
#include "audio_data.hpp"
#include "debug.hpp"
#include "direction.hpp"
#include "game.hpp"
#include "game_time.hpp"
#include "global.hpp"
#include "inventory.hpp"
#include "io.hpp"
#include "item_data.hpp"
#include "map.hpp"
#include "map_travel.hpp"
#include "msg_log.hpp"
#include "popup.hpp"
#include "pos.hpp"
#include "property_data.hpp"
#include "property_handler.hpp"
#include "sound.hpp"
#include "teleport.hpp"
#include "terrain.hpp"
#include "terrain_data.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static bool try_use_talisman_of_resurrection(actor::Actor& actor)
{
        const auto artifact_id = item::Id::resurrect_talisman;

        // Player has the Talisman of Resurrection, and died of physical damage?
        if (!actor.is_player() ||
            !actor.m_inv.has_item_in_backpack(artifact_id) ||
            (map::g_player->ins() >= 100) ||
            (actor.m_sp <= 0))
        {
                return false;
        }

        actor.m_inv.decr_item_type_in_backpack(artifact_id);

        msg_log::more_prompt();

        io::clear_screen();

        io::update_screen();

        const std::string msg =
                "Strange emptiness surrounds me. An eternity passes as I lay "
                "frozen in a world of shadows. Suddenly I awake!";

        popup::Popup(popup::AddToMsgHistory::yes)
                .set_msg(msg)
                .set_title("Dead")
                .run();

        for (auto* const a : game_time::g_actors)
        {
                if (!a->is_player())
                {
                        auto* const mon = static_cast<actor::Mon*>(a);

                        mon->m_mon_aware_state.aware_counter = 0;
                        mon->m_mon_aware_state.wary_counter = 0;
                }
        }

        actor.restore_hp(999, false, Verbose::no);

        actor.m_properties.end_prop(
                PropId::wound,
                PropEndConfig(
                        PropEndAllowCallEndHook::no,
                        PropEndAllowMsg::no,
                        PropEndAllowHistoricMsg::no));

        if (map::g_terrain.at(actor.m_pos)->id() == terrain::Id::chasm)
        {
                // Player died due to falling down a chasm - go to next level
                map_travel::go_to_nxt();
        }
        else
        {
                // Player died on current level - teleport the player so they
                // don't just wake up in the same bad situation again
                teleport(*map::g_player, ShouldCtrlTele::never);
        }

        msg_log::add("I LIVE AGAIN!");

        game::add_history_event("Was brought back from the dead");

        map::g_player->incr_shock(50.0, ShockSrc::misc);

        return true;
}

static void move_actor_to_pos_can_have_corpse(actor::Actor& actor)
{
        if (map::g_terrain.at(actor.m_pos)->can_have_corpse())
        {
                return;
        }

        for (const auto& d : dir_utils::g_dir_list_w_center)
        {
                const auto p = actor.m_pos + d;

                if (map::g_terrain.at(p)->can_have_corpse())
                {
                        actor.m_pos = p;

                        break;
                }
        }
}

// -----------------------------------------------------------------------------
// actor
// -----------------------------------------------------------------------------
namespace actor
{
void kill(
        actor::Actor& actor,
        const IsDestroyed is_destroyed,
        const AllowGore allow_gore,
        const AllowDropItems allow_drop_items)
{
        TRACE_FUNC_BEGIN_VERBOSE;

        // Save player with Talisman of Resurrection?
        const bool did_use_talisman = try_use_talisman_of_resurrection(actor);

        if (did_use_talisman)
        {
                return;
        }

        ASSERT(actor.m_data->can_leave_corpse ||
               (is_destroyed == IsDestroyed::yes));

        unset_actor_as_leader_for_all_mon(actor);

        if (!actor.is_player())
        {
                if (map::g_player->m_tgt == &actor)
                {
                        map::g_player->m_tgt = nullptr;
                }

                if (can_player_see_actor(actor))
                {
                        print_mon_death_msg(actor);
                }
        }

        if (!actor.is_player() && actor.m_data->is_humanoid)
        {
                Snd snd(
                        "I hear agonized screaming.",
                        audio::SfxId::END,
                        IgnoreMsgIfOriginSeen::yes,
                        actor.m_pos,
                        &actor,
                        SndVol::high,
                        AlertsMon::no);

                snd.run();
        }

        if (is_destroyed == IsDestroyed::yes)
        {
                actor.m_state = ActorState::destroyed;

                actor.m_properties.on_destroyed_alive();

                if (actor.m_data->can_bleed && (allow_gore == AllowGore::yes))
                {
                        map::make_gore(actor.m_pos);
                        map::make_blood(actor.m_pos);
                }
        }
        else
        {
                // Not destroyed
                actor.m_state = ActorState::corpse;

                if (!actor.is_player())
                {
                        move_actor_to_pos_can_have_corpse(actor);
                }
        }

        actor.on_death();
        actor.m_properties.on_death();

        if (!actor.is_player() &&
            !actor.m_properties.has(PropId::shapeshifts))
        {
                if (allow_drop_items == AllowDropItems::yes)
                {
                        actor.m_inv.drop_all_non_intrinsic(actor.m_pos);
                }

                game::on_mon_killed(actor);

                actor.m_leader = nullptr;
        }

        TRACE_FUNC_END_VERBOSE;
}

void print_mon_death_msg(const actor::Actor& actor)
{
        const std::string msg = actor.death_msg();

        if (!msg.empty())
        {
                msg_log::add(msg);
        }
}

void unset_actor_as_leader_for_all_mon(const actor::Actor& actor)
{
        for (auto* const other : game_time::g_actors)
        {
                if (other->m_leader == &actor)
                {
                        other->m_leader = nullptr;
                }
        }
}

}  // namespace actor

// =============================================================================
// Copyright 2011-2021 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "map_controller.hpp"

#include <vector>

#include "actor.hpp"
#include "actor_data.hpp"
#include "actor_mon.hpp"
#include "actor_player.hpp"
#include "array2.hpp"
#include "audio.hpp"
#include "audio_data.hpp"
#include "colors.hpp"
#include "game_time.hpp"
#include "map.hpp"
#include "msg_log.hpp"
#include "populate_monsters.hpp"
#include "pos.hpp"
#include "property_data.hpp"
#include "property_handler.hpp"
#include "terrain.hpp"
#include "terrain_data.hpp"

// -----------------------------------------------------------------------------
// MapController
// -----------------------------------------------------------------------------
void MapControllerStd::on_start()
{
        if (!map::g_player->m_properties.has(PropId::deaf))
        {
                audio::try_play_ambient(1);
        }
}

void MapControllerStd::on_std_turn()
{
        const bool has_necronomicon =
                map::g_player->m_inv.has_item_in_backpack(
                        item::Id::necronomicon);

        const int spawn_n_turns =
                has_necronomicon
                ? 200
                : 300;

        if (game_time::turn_nr() % spawn_n_turns == 0)
        {
                populate_mon::spawn_for_repopulate_over_time();
        }
}

void MapControllerBoss::on_start()
{
        audio::play(audio::SfxId::boss_voice1);

        for (auto* const actor : game_time::g_actors)
        {
                if (actor::is_player(actor))
                {
                        continue;
                }

                auto* const mon = static_cast<actor::Mon*>(actor);

                mon->become_aware_player(actor::AwareSource::other);
        }
}

void MapControllerBoss::on_std_turn()
{
        const P stair_pos(map::w() - 2, 11);

        const auto terrain_at_stair_pos =
                map::g_terrain.at(stair_pos)->id();

        if (terrain_at_stair_pos == terrain::Id::stairs)
        {
                // Stairs already created
                return;
        }

        for (const auto* const actor : game_time::g_actors)
        {
                if ((actor->id() == actor::Id::the_high_priest) &&
                    actor->is_alive())
                {
                        // The boss is still alive
                        return;
                }
        }

        // The boss is dead, and stairs have not yet been created

        msg_log::add(
                "The ground rumbles...",
                colors::white(),
                MsgInterruptPlayer::no,
                MorePromptOnMsg::yes);

        map::put(new terrain::Stairs(stair_pos));

        map::put(new terrain::RubbleLow(stair_pos - P(1, 0)));

        // TODO: This was in the 'on_death' hook for TheHighPriest - it should
        // be a property if this event should still exist
        // const size_t nr_snakes = rnd::range(4, 5);

        // actor_factory::spawn(
        //         pos,
        //         {nr_snakes, actor::Id::pit_viper});
}

// -----------------------------------------------------------------------------
// map_control
// -----------------------------------------------------------------------------
namespace map_control
{
std::unique_ptr<MapController> g_controller = nullptr;

}  // namespace map_control

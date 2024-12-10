// =============================================================================
// Copyright 2011-2024 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "map_controller.hpp"

#include <vector>

#include "actor.hpp"
#include "actor_data.hpp"
#include "array2.hpp"
#include "audio.hpp"
#include "audio_data.hpp"
#include "colors.hpp"
#include "game_time.hpp"
#include "global.hpp"
#include "map.hpp"
#include "misc.hpp"
#include "msg_log.hpp"
#include "populate_monsters.hpp"
#include "popup.hpp"
#include "pos.hpp"
#include "property_data.hpp"
#include "property_handler.hpp"
#include "terrain.hpp"
#include "terrain_data.hpp"
#include "terrain_factory.hpp"

// -----------------------------------------------------------------------------
// Map controllers
// -----------------------------------------------------------------------------
void MapControllerStd::on_enter()
{
        if (!map::g_player->m_properties.has(prop::Id::deaf)) {
                audio::try_play_ambient(1);
        }

        if (map::g_dlvl == (g_dlvl_first_mid_game / 2)) {
                const std::string msg =
                        "I did not expect this place to run this deep, "
                        "it is not possible! "
                        "Also there are unmistakable signs of an ancient civilization, "
                        "defying all logic and reason!\n\n"
                        "The way back seems lost somehow, "
                        "could I ever return even if I wanted to?";

                popup::Popup(popup::AddToMsgHistory::yes)
                        .set_msg(msg)
                        .run();
        }
        else if (map::g_dlvl == g_dlvl_first_mid_game) {
                const std::string msg =
                        "It goes on forever! This cannot be real! "
                        "I feel like I am walking in a dream; "
                        "The horror is utterly crushing.";

                popup::Popup(popup::AddToMsgHistory::yes)
                        .set_msg(msg)
                        .run();
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

        if (game_time::turn_nr() % spawn_n_turns == 0) {
                populate_mon::spawn_for_repopulate_over_time();
        }
}

void MapControllerBoss::on_enter()
{
        audio::play(audio::SfxId::boss_voice1);

        msg_log::more_prompt();

        msg_log::add("I feel like my presence here is known!", colors::msg_note());

        for (auto* const actor : game_time::g_actors) {
                if (actor::is_player(actor)) {
                        continue;
                }

                actor->become_aware_player(actor::AwareSource::other);
        }
}

void MapControllerBoss::on_std_turn()
{
        const P stair_pos(map::w() - 2, 11);

        const auto terrain_at_stair_pos =
                map::g_terrain.at(stair_pos)->id();

        if (terrain_at_stair_pos == terrain::Id::stairs) {
                // Stairs already created
                return;
        }

        for (const auto* const actor : game_time::g_actors) {
                if ((actor::id(*actor) == "MON_THE_HIGH_PRIEST") && actor::is_alive(*actor)) {
                        // The boss is still alive
                        return;
                }
        }

        // The boss is dead, and stairs have not yet been created

        msg_log::add("The ground rumbles...");

        map::update_terrain(
                terrain::make(
                        terrain::Id::stairs,
                        stair_pos));

        map::update_terrain(
                terrain::make(
                        terrain::Id::rubble_low,
                        stair_pos - P(1, 0)));

        // TODO: This was in the 'on_death' hook for TheHighPriest - it should
        // be a property if this event should still exist
        // const size_t nr_snakes = rnd::range(4, 5);

        // actor_factory::spawn(
        //         pos,
        //         {nr_snakes, "MON_PIT_VIPER"});
}

void MapControllerEgypt::on_std_turn()
{
        const int aware_dist = 9;

        if (!m_has_triggered_awareness) {
                bool is_in_aware_dist = false;

                for (actor::Actor* const actor : game_time::g_actors) {
                        if ((actor::id(*actor) == "MON_KHEPHREN") &&
                            (king_dist(map::g_player->m_pos, actor->m_pos) <= aware_dist)) {
                                is_in_aware_dist = true;
                                break;
                        }
                }

                if (is_in_aware_dist) {
                        msg_log::more_prompt();

                        msg_log::add(
                                "I feel like my presence here is known!",
                                colors::msg_note());

                        for (auto* const actor : game_time::g_actors) {
                                if (actor::is_player(actor)) {
                                        continue;
                                }

                                actor->become_aware_player(actor::AwareSource::other);

                                actor->m_ai_state.is_roaming_allowed = MonRoamingAllowed::yes;
                        }

                        m_has_triggered_awareness = true;
                }
        }
}

void MapControllerEgypt::on_enter()
{
        const std::string msg =
                "As I make my way into the depths, "
                "I'm taken aback by the sudden change in surroundings. "
                "The ancient architecture of this chamber is unlike anything I've seen before, "
                "with intricate hieroglyphs carved into the stone walls."
                "\n\n"
                "The air is thick with the scent of sand "
                "and an unfamiliar incense, "
                "giving the impression of being transported to another time and place. "
                "The silence is oppressive, broken only by the sound of my footsteps "
                "echoing on the stone floor."
                "\n\n"
                "As I look closer at the carvings, "
                "I can't help but wonder about the civilization that created them. "
                "Who were they, and how did they manage to build something like "
                "this deep underground?";

        popup::Popup(popup::AddToMsgHistory::yes)
                .set_msg(msg)
                .run();
}

void MapControllerDeepOneLair::on_enter()
{
        const std::string msg =
                "The walls here are slick with dampness, "
                "and the air is heavy with the stench of saltwater "
                "and the unmistakable odor of decay. "
                "I can feel the humidity on my skin, "
                "and my breaths come out in shallow gasps. "
                "\n\n"
                "There are tunnels leading deeper into underground waterways, "
                "but I can only guess where they might lead. "
                "It's as if the entire cave system is interconnected, "
                "with passages that could possibly lead to the depths of the ocean or "
                "subterranean lakes and rivers.";

        popup::Popup(popup::AddToMsgHistory::yes)
                .set_msg(msg)
                .run();
}

// -----------------------------------------------------------------------------
// map_control
// -----------------------------------------------------------------------------
namespace map_control
{
std::unique_ptr<MapController> g_controller = nullptr;

}  // namespace map_control

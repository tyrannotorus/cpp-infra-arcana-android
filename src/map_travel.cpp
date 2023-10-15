// =============================================================================
// Copyright 2011-2023 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "map_travel.hpp"

#include <cstddef>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "actor.hpp"
#include "actor_player_state.hpp"
#include "debug.hpp"
#include "draw_map.hpp"
#include "game.hpp"
#include "game_time.hpp"
#include "global.hpp"
#include "insanity.hpp"
#include "io.hpp"
#include "map.hpp"
#include "map_builder.hpp"
#include "map_controller.hpp"
#include "minimap.hpp"
#include "msg_log.hpp"
#include "property.hpp"
#include "property_data.hpp"
#include "property_factory.hpp"
#include "property_handler.hpp"
#include "random.hpp"
#include "saving.hpp"
#include "viewport.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static std::vector<MapType> s_map_list;

static void trigger_insanity_sympts_for_descent()
{
        // Phobia of deep places
        if (insanity::has_sympt(InsSymptId::phobia_deep)) {
                msg_log::add("I am plagued by my phobia of deep places!");

                map::g_player->m_properties.apply(prop::make(prop::Id::terrified));
        }

        // Babbling
        for (const auto* const sympt : insanity::active_sympts()) {
                if (sympt->id() == InsSymptId::babbling) {
                        static_cast<const InsBabbling*>(sympt)->babble();
                }
        }
}

// -----------------------------------------------------------------------------
// map_travel
// -----------------------------------------------------------------------------
namespace map_travel
{
void init()
{
        // Forest + dungeon + boss + trapezohedron
        const size_t nr_lvl_tot = g_dlvl_last + 3;

        s_map_list = std::vector<MapType>(nr_lvl_tot, MapType::std);

        if (rnd::one_in(3)) {
                const int deep_one_lvl_nr =
                        rnd::range(
                                g_dlvl_first_mid_game + 4,
                                g_dlvl_last_mid_game - 2);

                s_map_list[deep_one_lvl_nr] = MapType::deep_one_lair;
        }

        if (rnd::one_in(8)) {
                s_map_list[g_dlvl_last_mid_game - 1] = MapType::rat_cave;
        }

        // Mi-Go outpost
        s_map_list[g_dlvl_last_mid_game] = MapType::mi_go_outpost;

        // Pharaoh chamber
        s_map_list[g_dlvl_first_late_game] = MapType::egypt;

        s_map_list[g_dlvl_first_late_game + 1] = MapType::magic_pool;

        s_map_list[g_dlvl_last + 1] = MapType::high_priest;

        s_map_list[g_dlvl_last + 2] = MapType::trapez;
}

void save()
{
        saving::put_int((int)s_map_list.size());

        for (const MapType type : s_map_list) {
                saving::put_int((int)type);
        }
}

void load()
{
        const int nr_maps = saving::get_int();

        s_map_list.resize((size_t)nr_maps);

        for (auto& type : s_map_list) {
                type = (MapType)saving::get_int();
        }
}

void go_to_nxt()
{
        TRACE_FUNC_BEGIN;

        TRACE << "Leaving dungeon level '" << map::g_dlvl << "'" << std::endl;

        minimap::clear();

        s_map_list.erase(std::begin(s_map_list));

        const auto map_type = s_map_list.front();

        ++map::g_dlvl;

        const auto map_builder = map_builder::make(map_type);

        map_builder->build();

        if (map::g_player->m_properties.has(prop::Id::descend)) {
                msg_log::add("My sinking feeling disappears.");

                map::g_player->m_properties.end_prop(
                        prop::Id::descend,
                        prop::PropEndConfig(
                                prop::PropEndAllowCallEndHook::no,
                                prop::PropEndAllowMsg::no,
                                prop::PropEndAllowHistoricMsg::no));
        }

        game_time::g_is_magic_descend_nxt_std_turn = false;

        actor::player_state::g_target = nullptr;

        viewport::show(map::g_player->m_pos, viewport::ForceCentering::yes);

        map::update_vision();

        map::g_player->restore_shock(999, true);

        map::g_player->update_tmp_shock();

        msg_log::add("I have discovered a new area.");

        // NOTE: When the "intro level" is skipped, "go_to_nxt" is called when
        // the game starts - so no XP is missed in that case (same thing when
        // loading the game)
        game::incr_player_xp(5, Verbose::yes);

        map::g_player->on_new_dlvl_reached();

        game::add_history_event(
                "Reached dungeon level " +
                std::to_string(map::g_dlvl));

        trigger_insanity_sympts_for_descent();

        if (map_control::g_controller) {
                map_control::g_controller->on_enter();
        }

        TRACE
                << "Dungeon level '" << map::g_dlvl << "' ready"
                << std::endl
                << "Map type is: '" << (int)map_type << "'" << std::endl;

        TRACE_FUNC_END;
}

}  // namespace map_travel

// =============================================================================
// Copyright 2011-2024 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "map_builder.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <ostream>
#include <ratio>

#ifndef NDEBUG
#include <chrono>
#endif  // NDEBUG

#include "actor.hpp"
#include "actor_data.hpp"
#include "actor_factory.hpp"
#include "debug.hpp"
#include "game_time.hpp"
#include "gods.hpp"
#include "map.hpp"
#include "map_controller.hpp"
#include "map_templates.hpp"
#include "random.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static void spawn_starting_allies_for_all_mon()
{
        for (size_t i = 0; i < game_time::g_actors.size(); ++i) {
                actor::Actor* const actor = game_time::g_actors[i];

                actor::spawn_starting_allies(*actor);
        }
}

// -----------------------------------------------------------------------------
// map_builder
// -----------------------------------------------------------------------------
namespace map_builder
{
std::unique_ptr<MapBuilder> make(const MapType map_type)
{
        TRACE << "Making map of type '" << (int)map_type << "'" << std::endl;

        switch (map_type) {
        case MapType::deep_one_lair:
                return std::make_unique<MapBuilderDeepOneLair>();

        case MapType::magic_pool:
                return std::make_unique<MapBuilderMagicPool>();

        case MapType::intro_forest:
                return std::make_unique<MapBuilderIntroForest>();

        case MapType::std:
                return std::make_unique<MapBuilderStd>();

        case MapType::mi_go_outpost:
                return std::make_unique<MapBuilderMiGoOutpost>();

        case MapType::egypt:
                return std::make_unique<MapBuilderEgypt>();

        case MapType::rat_cave:
                return std::make_unique<MapBuilderRatCave>();

        case MapType::high_priest:
                return std::make_unique<MapBuilderBoss>();

        case MapType::trapez:
                return std::make_unique<MapBuilderTrapez>();
        }

        return nullptr;
}

}  // namespace map_builder

// -----------------------------------------------------------------------------
// MapBuilder
// -----------------------------------------------------------------------------
void MapBuilder::build()
{
        TRACE_FUNC_BEGIN;

        bool map_ok = false;

#ifndef NDEBUG
        int nr_attempts = 0;
        auto start_time = std::chrono::steady_clock::now();
#endif  // NDEBUG

        // TODO: When the map is invalid, any unique items spawned are lost
        // forever. Currently, the only effect of this should be that slightly
        // fewever unique items are found by the player.

        while (!map_ok) {
#ifndef NDEBUG
                ++nr_attempts;
#endif  // NDEBUG

                map_ok = build_specific();

                if (map_ok) {
                        map_templates::on_map_ok();
                }
                else {
                        map_templates::on_map_discarded();
                }
        }

        gods::set_random_god();

        spawn_starting_allies_for_all_mon();

        map_control::g_controller = map_controller();

#ifndef NDEBUG
        auto diff_time = std::chrono::steady_clock::now() - start_time;

        const double duration =
                std::chrono::duration<double, std::milli>(diff_time)
                        .count();

        TRACE
                << "Map built after " << nr_attempts << " attempt(s)."
                << std::endl
                << "Total time taken: " << duration << " ms"
                << std::endl;
#endif  // NDEBUG

        TRACE_FUNC_END;
}

std::unique_ptr<MapController> MapBuilder::map_controller() const
{
        return nullptr;
}

// -----------------------------------------------------------------------------
// MapBuilderTemplateLevel
// -----------------------------------------------------------------------------
bool MapBuilderTemplateLevel::build_specific()
{
        m_template = map_templates::level_templ(template_id());

        if (allow_transform_template()) {
                if (rnd::coin_toss()) {
                        m_template.rotate_cw();
                }

                if (rnd::coin_toss()) {
                        m_template.flip_hor();
                }

                if (rnd::coin_toss()) {
                        m_template.flip_ver();
                }
        }

        const P templ_dims = m_template.dims();

        map::reset(templ_dims);

        // Move away the player, to avoid placing monsters on the player
        map::g_player->m_pos.set(0, 0);

        for (int x = 0; x < templ_dims.x; ++x) {
                for (int y = 0; y < templ_dims.y; ++y) {
                        const P p(x, y);

                        handle_template_pos(p, m_template.at(p));
                }
        }

        on_template_built();

        return true;
}

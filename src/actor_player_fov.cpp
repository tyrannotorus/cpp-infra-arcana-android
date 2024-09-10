// =============================================================================
// Copyright 2011-2024 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "actor.hpp"

#include "array2.hpp"
#include "direction.hpp"
#include "fov.hpp"
#include "init.hpp"
#include "map.hpp"
#include "map_parsing.hpp"
#include "minimap.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static void fov_hack()
{
        Array2<bool> blocked_los(map::dims());

        map_parsers::BlocksLos()
                .run(blocked_los, blocked_los.rect());

        Array2<bool> blocked(map::dims());

        map_parsers::BlocksWalking(ParseActors::no)
                .run(blocked, blocked.rect());

        const std::vector<terrain::Id> free_terrains = {
                terrain::Id::chasm};

        for (int x = 0; x < blocked.w(); ++x) {
                for (int y = 0; y < blocked.h(); ++y) {
                        const P p(x, y);

                        if (map_parsers::IsAnyOfTerrains(free_terrains).run(p)) {
                                blocked.at(p) = false;
                        }
                }
        }

        const bool has_darkvision = map::g_player->m_properties.has(prop::Id::darkvision);

        for (int x = 0; x < map::w(); ++x) {
                for (int y = 0; y < map::h(); ++y) {
                        if (!blocked_los.at(x, y) || !blocked.at(x, y)) {
                                continue;
                        }

                        const P p(x, y);

                        for (const auto& d : dir_utils::g_dir_list) {
                                const auto p_adj = p + d;

                                if (!map::is_pos_inside_map(p_adj) ||
                                    !map::g_seen.at(p_adj)) {
                                        continue;
                                }

                                const bool allow_explore =
                                        (!map::g_dark.at(p_adj) ||
                                         map::g_light.at(p_adj) ||
                                         has_darkvision) &&
                                        !blocked.at(p_adj);

                                if (!allow_explore) {
                                        continue;
                                }

                                map::g_seen.at(x, y) = true;

                                map::g_los.at(x, y).is_blocked_hard = false;

                                break;
                        }
                }
        }
}

static void apply_cheat_vision()
{
        const size_t nr_map_positions = map::nr_positions();

        Array2<bool> blocked_projectiles(map::dims());

        // Show all cells adjacent to cells which can be shot through or
        // seen through
        map_parsers::BlocksProjectiles()
                .run(blocked_projectiles, blocked_projectiles.rect());

        map_parsers::BlocksLos()
                .run(
                        blocked_projectiles,
                        blocked_projectiles.rect(),
                        MapParseMode::append);

        for (auto& reveal_cell : blocked_projectiles) {
                reveal_cell = !reveal_cell;
        }

        const auto reveal_expanded =
                map_parsers::expand(
                        blocked_projectiles,
                        blocked_projectiles.rect());

        for (size_t i = 0; i < nr_map_positions; ++i) {
                if (reveal_expanded.at(i)) {
                        map::g_seen.at(i) = true;
                }
        }
}

// -----------------------------------------------------------------------------
// actor
// -----------------------------------------------------------------------------
namespace actor
{
void update_player_fov()
{
        const size_t nr_map_positions = map::nr_positions();

        for (size_t i = 0; i < nr_map_positions; ++i) {
                map::g_seen.at(i) = false;

                LosResult& los = map::g_los.at(i);
                los.is_blocked_hard = true;
                los.is_blocked_by_dark = false;
        }

        const bool has_darkvision = map::g_player->m_properties.has(prop::Id::darkvision);

        if (map::g_player->m_properties.allow_see()) {
                Array2<bool> hard_blocked(map::dims());

                const auto fov_lmt = fov::fov_rect(map::g_player->m_pos, map::dims());

                map_parsers::BlocksLos()
                        .run(hard_blocked,
                             fov_lmt,
                             MapParseMode::overwrite);

                FovMap fov_map;
                fov_map.hard_blocked = &hard_blocked;
                fov_map.light = &map::g_light;
                fov_map.dark = &map::g_dark;

                const Array2<LosResult> fov_result = fov::run(map::g_player->m_pos, fov_map);

                for (int x = fov_lmt.p0.x; x <= fov_lmt.p1.x; ++x) {
                        for (int y = fov_lmt.p0.y; y <= fov_lmt.p1.y; ++y) {
                                const LosResult& los_result =
                                        fov_result.at(x, y);

                                LosResult& los_to_update = map::g_los.at(x, y);

                                map::g_seen.at(x, y) =
                                        !los_result.is_blocked_hard &&
                                        (!los_result.is_blocked_by_dark ||
                                         has_darkvision);

                                los_to_update = los_result;

#ifndef NDEBUG
                                // Sanity check - if the cell is ONLY blocked by
                                // darkness (i.e. not by a wall or other
                                // blocking terrain), it should NOT be lit
                                if (!los_result.is_blocked_hard &&
                                    los_result.is_blocked_by_dark) {
                                        ASSERT(!map::g_light.at(x, y));
                                }
#endif  // NDEBUG
                        }
                }

                fov_hack();
        }

        // The player's current cell is always seen.
        map::g_seen.at(map::g_player->m_pos) = true;

        // Cheat vision
        if (init::g_is_cheat_vision_enabled) {
                apply_cheat_vision();
        }

        minimap::update();
}

}  // namespace actor

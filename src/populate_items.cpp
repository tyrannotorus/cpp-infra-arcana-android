// =============================================================================
// Copyright 2011-2023 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "populate_items.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <vector>

#include "actor.hpp"
#include "array2.hpp"
#include "global.hpp"
#include "item_data.hpp"
#include "item_factory.hpp"
#include "map.hpp"
#include "map_parsing.hpp"
#include "mapgen.hpp"
#include "player_bon.hpp"
#include "pos.hpp"
#include "random.hpp"
#include "terrain_data.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static int nr_items()
{
        int nr = rnd::range(4, 5);

        if (player_bon::has_trait(Trait::treasure_hunter)) {
                nr += rnd::range(1, 2);
        }

        return nr;
}

static std::vector<item::Id> make_item_bucket()
{
        std::vector<item::Id> item_bucket;
        item_bucket.clear();

        for (int i = 0; i < (int)item::Id::END; ++i) {
                const auto& data = item::g_data[i];

                if (data.type < ItemType::END_OF_EXTRINSIC_ITEMS &&
                    data.spawn_std_range.is_in_range(map::g_dlvl) &&
                    data.allow_spawn &&
                    rnd::percent(data.chance_to_incl_in_spawn_list)) {
                        item_bucket.push_back(item::Id(i));
                }
        }

        return item_bucket;
}

static Array2<bool> make_blocked_map()
{
        Array2<bool> result(map::dims());

        map_parsers::BlocksItems()
                .run(result, result.rect());

        // Liquid doesn't block items, but let's not spawn there...
        map_parsers::IsAnyOfTerrains(
                terrain::Id::liquid)
                .run(result,
                     result.rect(),
                     MapParseMode::append);

        const P& player_p = map::g_player->m_pos;

        result.at(player_p) = true;

        return result;
}

// -----------------------------------------------------------------------------
// populate_items
// -----------------------------------------------------------------------------
namespace populate_items
{
void make_items_on_floor()
{
        auto item_bucket = make_item_bucket();

        // Spawn items with a weighted random choice

        // NOTE: Each index in the position vector corresponds to the same index
        // in the weights vector.
        std::vector<P> positions;

        std::vector<int> position_weights;

        const auto blocked = make_blocked_map();

        mapgen::make_explore_spawn_weights(
                blocked,
                positions,
                position_weights);

        const int nr = nr_items();

        for (int i = 0; i < nr; ++i) {
                if (positions.empty() || item_bucket.empty()) {
                        break;
                }

                const size_t p_idx = rnd::weighted_choice(position_weights);

                const P& p = positions[p_idx];

                const size_t item_idx =
                        rnd::range(0, (int)item_bucket.size() - 1);

                const item::Id id = item_bucket[item_idx];

                if (item::g_data[(size_t)id].allow_spawn) {
                        item::make_item_on_floor(id, p);

                        positions.erase(begin(positions) + p_idx);
                        position_weights.erase(begin(position_weights) + p_idx);
                }
                else {
                        item_bucket.erase(begin(item_bucket) + item_idx);
                }
        }
}

}  // namespace populate_items

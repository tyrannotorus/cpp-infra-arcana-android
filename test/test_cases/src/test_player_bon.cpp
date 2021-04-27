// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include <algorithm>
#include <iterator>
#include <vector>

#include "catch.hpp"
#include "player_bon.hpp"
#include "test_utils.hpp"

static bool can_be_removed(
        const Trait id,
        const std::vector<Trait> traits_can_be_removed)
{
        const auto result =
                std::find(
                        std::begin(traits_can_be_removed),
                        std::end(traits_can_be_removed),
                        id);

        return result != std::end(traits_can_be_removed);
}

TEST_CASE("Get traits that can be removed")
{
        test_utils::init_all();

        player_bon::pick_bg(Bg::war_vet);

        player_bon::set_all_traits_to_picked();

        player_bon::remove_trait(Trait::master_marksman);

        const auto traits_be_removed = player_bon::traits_can_be_removed();

        REQUIRE(!can_be_removed(Trait::adept_marksman, traits_be_removed));
        REQUIRE(can_be_removed(Trait::expert_marksman, traits_be_removed));
        REQUIRE(!can_be_removed(Trait::master_marksman, traits_be_removed));
}

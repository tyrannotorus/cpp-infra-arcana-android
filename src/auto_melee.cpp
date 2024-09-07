// =============================================================================
// Copyright 2011-2024 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "auto_melee.hpp"

#include <algorithm>
#include <iterator>
#include <vector>

#include "actor.hpp"
#include "actor_player_state.hpp"
#include "array2.hpp"
#include "attack.hpp"
#include "bash.hpp"
#include "config.hpp"
#include "game_time.hpp"
#include "inventory.hpp"
#include "item_data.hpp"
#include "item_weapon.hpp"
#include "line_calc.hpp"
#include "map.hpp"
#include "misc.hpp"
#include "msg_log.hpp"
#include "pos.hpp"
#include "query.hpp"
#include "terrain.hpp"

namespace item
{
class Item;
}  // namespace item

namespace terrain
{
class Terrain;
}  // namespace terrain

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static void sort_by_closest_to_player(std::vector<actor::Actor*>& actors)
{
        std::sort(
                std::begin(actors),
                std::end(actors),
                [](const actor::Actor* const a1, const actor::Actor* const a2) {
                        const int d1 = king_dist(a1->m_pos, map::g_player->m_pos);
                        const int d2 = king_dist(a2->m_pos, map::g_player->m_pos);

                        return d1 < d2;
                });
}

static std::vector<actor::Actor*> get_all_foes_aware_of()
{
        std::vector<actor::Actor*> actors;
        actors.reserve(game_time::g_actors.size());

        for (actor::Actor* const actor : game_time::g_actors) {
                if (!is_player(actor) &&
                    !map::g_player->is_leader_of(actor) &&
                    actor::is_player_aware_of_me(*actor) &&
                    actor::is_alive(*actor)) {
                        actors.push_back(actor);
                }
        }

        return actors;
}

static void put_current_player_target_first(std::vector<actor::Actor*>& actors)
{
        const auto player_target_result =
                std::find_if(
                        std::begin(actors),
                        std::end(actors),
                        [](actor::Actor* const actor) {
                                return actor == actor::player_state::g_target;
                        });

        if ((actors.size() > 1) &&
            (player_target_result != std::begin(actors)) &&
            (player_target_result != std::end(actors))) {
                // Put the player target first.
                actors.erase(player_target_result);
                actors.insert(std::begin(actors), actor::player_state::g_target);
        }
}

static item::Wpn& get_weapon()
{
        item::Item* wpn_item = map::g_player->m_inv.item_in_slot(SlotId::wpn);

        if (!wpn_item) {
                wpn_item = &map::g_player->unarmed_wpn();
        }

        return static_cast<item::Wpn&>(*wpn_item);
}

static actor::Actor* find_reachable_actor(
        std::vector<actor::Actor*>& actors,
        const int wpn_range)
{
        for (actor::Actor* const actor : actors) {
                if (king_dist(map::g_player->m_pos, actor->m_pos) > wpn_range) {
                        // Actor is out of range.
                        continue;
                }

                const std::vector<P> line =
                        line_calc::calc_new_line(
                                map::g_player->m_pos,
                                actor->m_pos,
                                true,
                                999,
                                false);

                bool has_path = false;

                for (const P& p : line) {
                        if (p == actor->m_pos) {
                                has_path = true;
                                break;
                        }

                        if (!map::g_seen.at(p)) {
                                break;
                        }

                        terrain::Terrain* const terrain = map::g_terrain.at(p);

                        if (!terrain->is_projectile_passable()) {
                                break;
                        }
                }

                if (has_path) {
                        return actor;
                }
        }

        return nullptr;
}

// -----------------------------------------------------------------------------
// auto_melee
// -----------------------------------------------------------------------------
namespace auto_melee
{
void run()
{
        map::update_vision();

        std::vector<actor::Actor*> actors = get_all_foes_aware_of();

        if (actors.empty()) {
                // Player is not aware of any hostile monsters.
                return;
        }

        sort_by_closest_to_player(actors);

        put_current_player_target_first(actors);

        item::Wpn& wpn = get_weapon();

        actor::Actor* actor_to_attack = find_reachable_actor(actors, wpn.data().melee.reach);

        if (!actor_to_attack) {
                return;
        }

        // If this is also a ranged weapon, ask if player really intended to use
        // it as melee weapon.
        if (wpn.data().ranged.is_ranged_wpn && config::warn_on_ranged_wpn_melee()) {
                const BinaryAnswer answer =
                        attack::query_player_attack_mon_with_ranged_wpn(
                                wpn,
                                *actor_to_attack);

                msg_log::clear();

                if (answer == BinaryAnswer::no) {
                        return;
                }
        }

        actor::player_state::g_target = actor_to_attack;

        attack::melee(map::g_player, map::g_player->m_pos, actor_to_attack->m_pos, wpn);
}

}  // namespace auto_melee

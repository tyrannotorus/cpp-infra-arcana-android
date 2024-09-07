// =============================================================================
// Copyright 2011-2024 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "actor.hpp"

#include <algorithm>
#include <climits>
#include <memory>
#include <optional>

#include "ability_values.hpp"
#include "actor_data.hpp"
#include "actor_items.hpp"
#include "actor_player_state.hpp"
#include "actor_see.hpp"
#include "array2.hpp"
#include "debug.hpp"
#include "fov.hpp"
#include "game_time.hpp"
#include "global.hpp"
#include "inventory.hpp"
#include "item.hpp"
#include "item_data.hpp"
#include "item_device.hpp"
#include "item_explosive.hpp"
#include "item_misc.hpp"
#include "map.hpp"
#include "map_parsing.hpp"
#include "msg_log.hpp"
#include "player_bon.hpp"
#include "player_spells.hpp"
#include "property.hpp"
#include "property_data.hpp"
#include "property_handler.hpp"
#include "random.hpp"
#include "rect.hpp"
#include "text_format.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// actor
// -----------------------------------------------------------------------------
namespace actor
{
// -----------------------------------------------------------------------------
// Actor
// -----------------------------------------------------------------------------
Actor::~Actor()
{
        // Free all items owning actors.
        for (auto* item : m_inv.m_backpack) {
                item->clear_actor_carrying();
        }

        for (auto& slot : m_inv.m_slots) {
                if (slot.item) {
                        slot.item->clear_actor_carrying();
                }
        }

        // Free monster spells.
        for (auto& spell : m_mon_spells) {
                delete spell.spell;
        }
}

bool Actor::is_leader_of(const Actor* const actor) const
{
        if (actor) {
                return actor->m_leader == this;
        }
        else {
                return false;
        }
}

bool Actor::is_actor_my_leader(const Actor* const actor) const
{
        if (m_leader) {
                return m_leader == actor;
        }
        else {
                return false;
        }
}

}  // namespace actor

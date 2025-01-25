// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "actor.hpp"

#include "item.hpp"

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

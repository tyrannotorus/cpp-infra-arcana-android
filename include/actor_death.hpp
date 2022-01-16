// =============================================================================
// Copyright 2011-2021 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef ACTOR_DEATH_HPP
#define ACTOR_DEATH_HPP

enum class IsDestroyed
{
        no,
        yes
};

enum class AllowGore
{
        no,
        yes
};

enum class AllowDropItems
{
        no,
        yes
};

namespace actor
{
class Actor;

void kill(
        Actor& actor,
        IsDestroyed is_destroyed,
        AllowGore allow_gore,
        AllowDropItems allow_drop_items);

void print_mon_death_msg(const actor::Actor& actor);

void unset_actor_as_leader_for_all_mon(const Actor* actor);

}  // namespace actor

#endif  // ACTOR_DEATH_HPP

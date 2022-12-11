// =============================================================================
// Copyright 2011-2021 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "terrain_factory.hpp"
#include "debug.hpp"
#include "terrain.hpp"
#include "terrain_data.hpp"
#include "terrain_door.hpp"
#include "terrain_event.hpp"
#include "terrain_gong.hpp"
#include "terrain_mob.hpp"
#include "terrain_monolith.hpp"
#include "terrain_pylon.hpp"
#include "terrain_trap.hpp"

namespace terrain
{
Terrain* make(const Id id, const P& pos)
{
        const auto* const d = &data(id);

        switch (id) {
        case Id::floor:
                return new Floor(pos, d);
                break;

        case Id::bridge:
                return new Bridge(pos, d);
                break;

        case Id::wall:
                return new Wall(pos, d);
                break;

        case Id::tree:
                return new Tree(pos, d);
                break;

        case Id::grass:
                return new Grass(pos, d);
                break;

        case Id::bush:
                return new Bush(pos, d);
                break;

        case Id::vines:
                return new Vines(pos, d);
                break;

        case Id::chains:
                return new Chains(pos, d);
                break;

        case Id::grate:
                return new Grate(pos, d);
                break;

        case Id::stairs:
                return new Stairs(pos, d);
                break;

        case Id::lever:
                return new Lever(pos, d);
                break;

        case Id::brazier:
                return new Brazier(pos, d);
                break;

        case Id::gravestone:
                return new GraveStone(pos, d);
                break;

        case Id::tomb:
                return new Tomb(pos, d);
                break;

        case Id::church_bench:
                return new ChurchBench(pos, d);
                break;

        case Id::altar:
                return new Altar(pos, d);
                break;

        case Id::gong:
                return new Gong(pos, d);
                break;

        case Id::carpet:
                return new Carpet(pos, d);
                break;

        case Id::rubble_high:
                return new RubbleHigh(pos, d);
                break;

        case Id::rubble_low:
                return new RubbleLow(pos, d);
                break;

        case Id::bones:
                return new Bones(pos, d);
                break;

        case Id::statue:
                return new Statue(pos, d);
                break;

        case Id::cocoon:
                return new Cocoon(pos, d);
                break;

        case Id::chest:
                return new Chest(pos, d);
                break;

        case Id::cabinet:
                return new Cabinet(pos, d);
                break;

        case Id::bookshelf:
                return new Bookshelf(pos, d);
                break;

        case Id::alchemist_bench:
                return new AlchemistBench(pos, d);
                break;

        case Id::fountain:
                return new Fountain(pos, d);
                break;

        case Id::monolith:
                return new Monolith(pos, d);
                break;

        case Id::pylon:
                return new Pylon(pos, d);
                break;

        case Id::stalagmite:
                return new Stalagmite(pos, d);
                break;

        case Id::chasm:
                return new Chasm(pos, d);
                break;

        case Id::liquid:
                return new Liquid(pos, d);
                break;

        case Id::door:
                return new Door(pos, d);
                break;

        case Id::lit_dynamite:
                return new LitDynamite(pos, d);
                break;

        case Id::lit_flare:
                return new LitFlare(pos, d);
                break;

        case Id::trap:
                return new Trap(pos, d);
                break;

        case Id::smoke:
                return new Smoke(pos, d);
                break;

        case Id::force_field:
                return new ForceField(pos, d);
                break;

        case Id::event_wall_crumble:
                return new EventWallCrumble(pos, d);
                break;

        case Id::event_snake_emerge:
                return new EventSnakeEmerge(pos, d);
                break;

        case Id::event_spawn_monsters_delayed:
                return new EventSpawnMonstersDelayed(pos, d);
                break;

        case Id::event_rat_cave_discovery:
                return new EventRatsInTheWallsDiscovery(pos, d);
                break;

        case Id::END:
                break;
        }

        ASSERT(false);
        return nullptr;
}

}  // namespace terrain

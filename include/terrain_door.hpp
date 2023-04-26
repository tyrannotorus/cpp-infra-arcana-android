// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef TERRAIN_DOOR_HPP
#define TERRAIN_DOOR_HPP

#include <string>

#include "colors.hpp"
#include "gfx.hpp"
#include "global.hpp"
#include "item.hpp"
#include "property_data.hpp"
#include "terrain.hpp"

namespace actor
{
class Actor;
}  // namespace actor
struct P;

namespace terrain
{
struct TerrainData;

enum class DoorSpawnState
{
        open,
        closed,
        stuck,
        secret,
        secret_and_stuck,
        warded,
        any
};

enum class DoorType
{
        wood,
        metal,
        gate
};

enum class WardState
{
        not_warded,  // Has never been warded.
        warded,  // Is warded.
        unwarded,  // Was warded, but ward has been removed.
};

class Door : public Terrain
{
public:
        Door(const P& pos, const TerrainData* const data) :
                Terrain(pos, data) {}

        Door() = delete;

        ~Door();

        void init_type_and_state(
                DoorType type,
                DoorSpawnState spawn_state = DoorSpawnState::any);

        void set_mimic_terrain(terrain::Terrain* const terrain)
        {
                m_mimic_terrain = terrain;
        }

        // Sometimes we want to refer to a door as just a "door", instead of
        // something verbose like "the open wooden door".
        std::string base_name() const;  // E.g. "wooden door"

        std::string base_name_short() const;  // E.g. "door"

        std::string name(Article article) const override;

        char character() const override;

        gfx::TileId tile() const override;

        WasDestroyed on_finished_burning() override;

        bool allow_player_melee_attack(
                DmgType dmg_type,
                const item::Item& wpn) const override;

        void hit(
                DmgType dmg_type,
                actor::Actor* actor,
                const P& from_pos,
                int dmg) override;

        void bump(actor::Actor& actor_bumping) override;

        bool is_walkable() const override;

        bool can_move(const actor::Actor& actor) const override;

        bool is_property_allowing_move(prop::Id id) const override;

        bool is_los_passable() const override;

        bool is_projectile_passable() const override;

        bool is_smoke_passable() const override;

        void actor_try_open(actor::Actor& actor_trying);
        void actor_try_close(actor::Actor& actor_trying);
        bool actor_try_jam(actor::Actor& actor_trying);

        bool is_open() const
        {
                return m_is_open;
        }

        bool is_stuck() const
        {
                return m_is_stuck;
        }

        bool is_known_stuck() const
        {
                return m_is_known_stuck;
        }

        bool is_warded() const
        {
                return (m_ward_state == WardState::warded);
        }

        void try_trigger_ward_trap();

        void remove_ward();

        Material material() const override;

        void reveal(PrintRevealMsg print_reveal_msg) override;

        void on_revealed_from_searching() override;

        void reveal_stuck_status(PrintRevealMsg print_reveal_msg);

        void set_secret();
        void set_stuck();

        // NOTE: These do not affect levers - they only open or close the door.
        DidOpen open(actor::Actor* actor_opening) override;
        DidClose close(actor::Actor* actor_closing) override;

        void jam(actor::Actor* actor_jamming);

        actor::Actor* actor_currently_opening() const
        {
                return m_actor_currently_opening;
        }

        void clear_actor_currently_opening()
        {
                m_actor_currently_opening = nullptr;
        }

        const Terrain* mimic() const
        {
                return m_mimic_terrain;
        }

        DoorType type() const
        {
                return m_type;
        }

private:
        Color color_default() const override;

        void bash(DmgType dmg_type, actor::Actor& actor, int dmg);
        void player_bash(DmgType dmg_type, int dmg);
        void mon_bash(actor::Actor& mon);

        Terrain* m_mimic_terrain {nullptr};

        bool m_is_open {false};
        bool m_is_stuck {false};
        bool m_is_known_stuck {false};
        WardState m_ward_state {WardState::not_warded};

        int m_jam_level {0};

        DoorType m_type {DoorType::wood};

        actor::Actor* m_actor_currently_opening {nullptr};

};  // Door

}  // namespace terrain

#endif  // TERRAIN_DOOR_HPP

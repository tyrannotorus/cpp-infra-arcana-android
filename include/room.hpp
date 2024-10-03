// =============================================================================
// Copyright 2011-2024 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef ROOM_HPP
#define ROOM_HPP

#include <string>
#include <vector>

#include "global.hpp"
#include "pos.hpp"
#include "rect.hpp"
#include "terrain_data.hpp"

template <typename T>
class Array2;

namespace room
{
// Room theming occurs in three steps:
//
//   1) Pre-connect (before corridors):
//
//   In pre-connect, reshaping is performed, e.g. plus-shape, cavern-shape,
//   pillars, etc.
//
//   When pre-connect starts, it is assumed that all (standard) rooms are
//   rectangular with unbroken walls.
//
//   2) Post-connect (after corridors):
//
//   In post-connect, auto-terrains such as chests and altars are placed, as
//   well as room-specific stuff like trees, altars, etc. It can then be
//   verified for each terrain that the map is still connected.
//
//   3) Affect surroundings (after doors are placed):
//
//   Here the rooms may put terrain outside of themselves, e.g. spider webs
//   outside of a spider room.
//
//   This hook is called with a random room order, since several rooms may put
//   terrain in overlapping areas, and creation order should not determine which
//   room goes first.
//
// As a rule of thumb, place walkable terrains in the pre-connect step, and
// blocking terrains in the post-connect step.
//

// NOTE: There are both 'RoomType' ids, and 'Room' classes. A room of a certain
// RoomType id does NOT have to be an instance of the corresponding Room child
// class. For example, templated rooms are always created as the TemplateRoom
// class, but they may have any standard room RoomType id. There may even be
// RoomType ids which doesn't have a corresponding Room class at all.

class Room;

enum class RoomType
{
        // Standard rooms (standardized terrain spawning and reshaping)
        plain,  // NOTE: "plain" must be the first type
        human,
        ritual,
        jail,
        spider,
        crawling_pit,
        crypt,
        monster,
        damp,  // Shallow water/mud scattered over the room
        pool,  // Larger body of water - artificial or natural pools or lakes
        cave,
        chasm,
        forest,
        END_OF_STD_ROOMS,

        // Special room types
        corridor,
        crumble_room,
        river
};

struct RoomAutoTerrainRule
{
        RoomAutoTerrainRule() :
                id(terrain::Id::END),
                nr_allowed(0) {}

        RoomAutoTerrainRule(
                const terrain::Id terrain_id,
                const int nr_terrains_allowed) :
                id(terrain_id),
                nr_allowed(nr_terrains_allowed) {}

        terrain::Id id;
        int nr_allowed;
};
void init_room_bucket();

// NOTE: These functions do not make rooms on the map, just create Room objects.
// Use the "make_room..." functions in the map generator for a convenient way to
// generate rooms on the map.
Room* make(RoomType type, const R& r);

Room* make_random_room(const R& r, IsSubRoom is_subroom);

RoomType str_to_room_type(const std::string& str);

#ifndef NDEBUG
std::string room_type_to_str(RoomType type);
#endif  // NDEBUG

class Room
{
public:
        Room(R r, RoomType type);

        Room() = delete;

        virtual ~Room() = default;

        std::vector<P> positions_in_room() const;

        void on_pre_connect(Array2<bool>& door_proposals);
        void on_post_connect(Array2<bool>& door_proposals);
        void affect_surroundings();

        virtual std::vector<RoomAutoTerrainRule> auto_terrains_allowed() const
        {
                return {};
        }

        virtual void populate_monsters() const {}

        virtual bool is_allowed() const
        {
                return true;
        }

        virtual int max_nr_mon_groups_spawned() const
        {
                return 2;
        }

        virtual bool allow_sub_rooms() const
        {
                return true;
        }

        R m_r;
        const RoomType m_type;
        bool m_is_sub_room;
        std::vector<Room*> m_rooms_con_to;
        std::vector<Room*> m_sub_rooms;

protected:
        virtual void on_pre_connect_hook(Array2<bool>& door_proposals)
        {
                (void)door_proposals;
        }

        virtual void on_post_connect_hook(Array2<bool>& door_proposals)
        {
                (void)door_proposals;
        }

        virtual void affect_surroundings_hook()
        {
        }

        void make_dark() const;
};

class PlainRoom : public Room
{
public:
        PlainRoom(R r) :
                Room(r, RoomType::plain) {}

        std::vector<RoomAutoTerrainRule> auto_terrains_allowed() const override;

protected:
        void on_pre_connect_hook(Array2<bool>& door_proposals) override;

        void on_post_connect_hook(Array2<bool>& door_proposals) override;
};

class HumanRoom : public Room
{
public:
        HumanRoom(R r) :
                Room(r, RoomType::human) {}

        bool is_allowed() const override;

        std::vector<RoomAutoTerrainRule> auto_terrains_allowed() const override;

protected:
        void on_pre_connect_hook(Array2<bool>& door_proposals) override;

        void on_post_connect_hook(Array2<bool>& door_proposals) override;
};

class JailRoom : public Room
{
public:
        JailRoom(R r) :
                Room(r, RoomType::jail) {}

        std::vector<RoomAutoTerrainRule> auto_terrains_allowed() const override;

protected:
        void on_pre_connect_hook(Array2<bool>& door_proposals) override;

        void on_post_connect_hook(Array2<bool>& door_proposals) override;
};

class RitualRoom : public Room
{
public:
        RitualRoom(R r) :
                Room(r, RoomType::ritual) {}

        bool is_allowed() const override;

        std::vector<RoomAutoTerrainRule> auto_terrains_allowed() const override;

protected:
        void on_pre_connect_hook(Array2<bool>& door_proposals) override;

        void on_post_connect_hook(Array2<bool>& door_proposals) override;

        void affect_surroundings_hook() override;
};

class SpiderRoom : public Room
{
public:
        SpiderRoom(R r) :
                Room(r, RoomType::spider) {}

        bool is_allowed() const override;

        int max_nr_mon_groups_spawned() const override
        {
                return 1;
        }

        std::vector<RoomAutoTerrainRule> auto_terrains_allowed() const override;

protected:
        void on_pre_connect_hook(Array2<bool>& door_proposals) override;

        void on_post_connect_hook(Array2<bool>& door_proposals) override;

        void affect_surroundings_hook() override;
};

class CrawlingPitRoom : public Room
{
public:
        CrawlingPitRoom(R r) :
                Room(r, RoomType::monster) {}

        bool is_allowed() const override;

        void populate_monsters() const override;

        std::vector<RoomAutoTerrainRule> auto_terrains_allowed() const override;

protected:
        void on_pre_connect_hook(Array2<bool>& door_proposals) override;

        void on_post_connect_hook(Array2<bool>& door_proposals) override;

        std::string get_random_monster_type() const;
};

class CryptRoom : public Room
{
public:
        CryptRoom(R r) :
                Room(r, RoomType::crypt) {}

        bool is_allowed() const override;

        std::vector<RoomAutoTerrainRule> auto_terrains_allowed() const override;

protected:
        void on_pre_connect_hook(Array2<bool>& door_proposals) override;

        void on_post_connect_hook(Array2<bool>& door_proposals) override;
};

class MonsterRoom : public Room
{
public:
        MonsterRoom(R r) :
                Room(r, RoomType::monster) {}

        bool is_allowed() const override;

        std::vector<RoomAutoTerrainRule> auto_terrains_allowed() const override;

protected:
        void on_pre_connect_hook(Array2<bool>& door_proposals) override;

        void on_post_connect_hook(Array2<bool>& door_proposals) override;

        void affect_surroundings_hook() override;
};

class DampRoom : public Room
{
public:
        DampRoom(R r) :
                Room(r, RoomType::damp) {}

        bool is_allowed() const override;

        std::vector<RoomAutoTerrainRule> auto_terrains_allowed() const override;

protected:
        void on_pre_connect_hook(Array2<bool>& door_proposals) override;

        void on_post_connect_hook(Array2<bool>& door_proposals) override;
};

class PoolRoom : public Room
{
public:
        PoolRoom(R r) :
                Room(r, RoomType::pool) {}

        bool is_allowed() const override;

        std::vector<RoomAutoTerrainRule> auto_terrains_allowed() const override;

protected:
        void on_pre_connect_hook(Array2<bool>& door_proposals) override;

        void on_post_connect_hook(Array2<bool>& door_proposals) override;
};

class CaveRoom : public Room
{
public:
        CaveRoom(R r) :
                Room(r, RoomType::cave) {}

        bool is_allowed() const override;

        std::vector<RoomAutoTerrainRule> auto_terrains_allowed() const override;

protected:
        void on_pre_connect_hook(Array2<bool>& door_proposals) override;

        void on_post_connect_hook(Array2<bool>& door_proposals) override;
};

class ChasmRoom : public Room
{
public:
        ChasmRoom(R r) :
                Room(r, RoomType::chasm) {}

        bool is_allowed() const override;

        std::vector<RoomAutoTerrainRule> auto_terrains_allowed() const override;

protected:
        void on_pre_connect_hook(Array2<bool>& door_proposals) override;

        void on_post_connect_hook(Array2<bool>& door_proposals) override;
};

class ForestRoom : public Room
{
public:
        ForestRoom(R r) :
                Room(r, RoomType::forest) {}

        bool is_allowed() const override;

protected:
        void on_pre_connect_hook(Array2<bool>& door_proposals) override;

        void on_post_connect_hook(Array2<bool>& door_proposals) override;
};

class TemplateRoom : public Room
{
public:
        TemplateRoom(const R& r, RoomType type) :
                Room(r, type) {}

        bool allow_sub_rooms() const override
        {
                return false;
        }
};

class CorridorRoom : public Room
{
public:
        CorridorRoom(const R& r) :
                Room(r, RoomType::corridor) {}

protected:
        void on_pre_connect_hook(Array2<bool>& door_proposals) override
        {
                (void)door_proposals;
        }

        void on_post_connect_hook(Array2<bool>& door_proposals) override;

        std::vector<RoomAutoTerrainRule> auto_terrains_allowed() const override;
};

class CrumbleRoom : public Room
{
public:
        CrumbleRoom(const R& r) :
                Room(r, RoomType::crumble_room) {}

        void on_pre_connect_hook(Array2<bool>& door_proposals) override
        {
                (void)door_proposals;
        }

        void on_post_connect_hook(Array2<bool>& door_proposals) override
        {
                (void)door_proposals;
        }
};

class RiverRoom : public Room
{
public:
        RiverRoom(const R& r) :
                Room(r, RoomType::river),
                m_axis(Axis::hor) {}

        void on_pre_connect_hook(Array2<bool>& door_proposals) override;

        void on_post_connect_hook(Array2<bool>& door_proposals) override
        {
                (void)door_proposals;
        }

        Axis m_axis;
};

}  // namespace room

#endif  // ROOM_HPP

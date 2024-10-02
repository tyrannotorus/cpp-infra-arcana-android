// =============================================================================
// Copyright 2011-2024 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "room.hpp"

#include <algorithm>
#include <climits>
#include <cstddef>
#include <functional>
#include <iterator>
#include <numeric>
#include <ostream>
#include <unordered_map>

#include "actor_data.hpp"
#include "array2.hpp"
#include "colors.hpp"
#include "debug.hpp"
#include "direction.hpp"
#include "flood.hpp"
#include "global.hpp"
#include "init.hpp"
#include "map.hpp"
#include "map_parsing.hpp"
#include "mapgen.hpp"
#include "misc.hpp"
#include "panel.hpp"
#include "populate_monsters.hpp"
#include "populate_traps.hpp"
#include "random.hpp"
#include "room_auto_spawn_terrain.hpp"
#include "state.hpp"
#include "terrain.hpp"
#include "terrain_factory.hpp"
#include "terrain_trap.hpp"

#ifndef NDEBUG
#include "io.hpp"
#endif  // NDEBUG

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static std::vector<room::RoomType> s_room_bucket;

static const std::unordered_map<std::string, room::RoomType> s_str_to_room_type_map = {
        {"ROOM_PLAIN", room::RoomType::plain},
        {"ROOM_HUMAN", room::RoomType::human},
        {"ROOM_RITUAL", room::RoomType::ritual},
        {"ROOM_JAIL", room::RoomType::jail},
        {"ROOM_SPIDER", room::RoomType::spider},
        {"ROOM_CRAWLING_PIT", room::RoomType::crawling_pit},
        {"ROOM_CRYPT", room::RoomType::crypt},
        {"ROOM_MONSTER", room::RoomType::monster},
        {"ROOM_DAMP", room::RoomType::damp},
        {"ROOM_POOL", room::RoomType::pool},
        {"ROOM_CAVE", room::RoomType::cave},
        {"ROOM_CHASM", room::RoomType::chasm},
        {"ROOM_FOREST", room::RoomType::forest},
};

static const std::unordered_map<room::RoomType, std::string> s_room_type_to_str_map = {
        {room::RoomType::plain, "ROOM_PLAIN"},
        {room::RoomType::human, "ROOM_HUMAN"},
        {room::RoomType::ritual, "ROOM_RITUAL"},
        {room::RoomType::jail, "ROOM_JAIL"},
        {room::RoomType::spider, "ROOM_SPIDER"},
        {room::RoomType::crawling_pit, "ROOM_CRAWLING_PIT"},
        {room::RoomType::crypt, "ROOM_CRYPT"},
        {room::RoomType::monster, "ROOM_MONSTER"},
        {room::RoomType::damp, "ROOM_DAMP"},
        {room::RoomType::pool, "ROOM_POOL"},
        {room::RoomType::cave, "ROOM_CAVE"},
        {room::RoomType::chasm, "ROOM_CHASM"},
        {room::RoomType::forest, "ROOM_FOREST"},
};

static void add_to_room_bucket(const room::RoomType type, const size_t nr)
{
        if (nr > 0) {
                s_room_bucket.reserve(s_room_bucket.size() + nr);

                for (size_t i = 0; i < nr; ++i) {
                        s_room_bucket.push_back(type);
                }
        }
}

// NOTE: This cannot be a virtual class method, since a room of a certain
// RoomType doesn't have to be an instance of the corresponding Room child class
// (it could be for example a TemplateRoom object with RoomType "ritual", in
// that case we still want the same chance to make it dark).
static int base_pct_chance_dark(const room::RoomType room_type)
{
        // TODO: Corridors could possibly be dark as well (maybe that's even
        // more fitting, so they should have a pretty high chance)? But only if
        // they have a significant size (significantly bigger than 1x1). And
        // maybe also never if the corridor is widened (it's kind of annoying),
        // if so use a Room member variable to tell if it's a a widened
        // corridor.

        switch (room_type) {
        case room::RoomType::plain:
                return 5;

        case room::RoomType::human:
                return 10;

        case room::RoomType::ritual:
                return 15;

        case room::RoomType::jail:
                return 60;

        case room::RoomType::spider:
                return 50;

        case room::RoomType::crawling_pit:
                return 75;

        case room::RoomType::crypt:
                return 60;

        case room::RoomType::monster:
                return 80;

        case room::RoomType::damp:
                return 25;

        case room::RoomType::pool:
                return 25;

        case room::RoomType::cave:
                return 30;

        case room::RoomType::chasm:
                return 25;

        case room::RoomType::forest:
                return 10;

        case room::RoomType::END_OF_STD_ROOMS:
        case room::RoomType::corridor:
        case room::RoomType::crumble_room:
        case room::RoomType::river:
                break;
        }

        return 0;
}

// -----------------------------------------------------------------------------
// room
// -----------------------------------------------------------------------------
namespace room
{
void init_room_bucket()
{
        TRACE_FUNC_BEGIN;

        s_room_bucket.clear();

        const int dlvl = map::g_dlvl;

        if (dlvl <= g_dlvl_last_early_game) {
                add_to_room_bucket(RoomType::human, rnd::range(4, 5));
                add_to_room_bucket(RoomType::jail, 1);
                add_to_room_bucket(RoomType::ritual, 2);
                add_to_room_bucket(RoomType::crypt, rnd::range(2, 3));
                add_to_room_bucket(RoomType::monster, 1);
                add_to_room_bucket(RoomType::damp, rnd::range(1, 2));
                add_to_room_bucket(RoomType::pool, rnd::range(2, 3));
                add_to_room_bucket(RoomType::crawling_pit, 1);

                const size_t nr_plain_rooms = s_room_bucket.size() * 2;

                add_to_room_bucket(RoomType::plain, nr_plain_rooms);
        }
        else if (dlvl <= g_dlvl_last_mid_game) {
                add_to_room_bucket(RoomType::human, 3);
                add_to_room_bucket(RoomType::jail, rnd::range(1, 2));
                add_to_room_bucket(RoomType::ritual, 2);
                add_to_room_bucket(RoomType::spider, rnd::range(1, 3));
                add_to_room_bucket(RoomType::crawling_pit, 1);
                add_to_room_bucket(RoomType::crypt, 4);
                add_to_room_bucket(RoomType::monster, 2);
                add_to_room_bucket(RoomType::damp, rnd::range(1, 3));
                add_to_room_bucket(RoomType::pool, rnd::range(2, 3));
                add_to_room_bucket(RoomType::cave, 2);
                add_to_room_bucket(RoomType::chasm, 1);
                add_to_room_bucket(RoomType::forest, 2);

                const size_t nr_plain_rooms = s_room_bucket.size() * 2;

                add_to_room_bucket(RoomType::plain, nr_plain_rooms);
        }
        else {
                // Late game
                add_to_room_bucket(RoomType::monster, 1);
                add_to_room_bucket(RoomType::spider, 1);
                add_to_room_bucket(RoomType::crawling_pit, 1);
                add_to_room_bucket(RoomType::damp, rnd::range(1, 3));
                add_to_room_bucket(RoomType::pool, rnd::range(2, 3));
                add_to_room_bucket(RoomType::chasm, 2);
                add_to_room_bucket(RoomType::forest, 2);
                add_to_room_bucket(RoomType::crypt, rnd::range(1, 2));
                add_to_room_bucket(RoomType::ritual, rnd::range(1, 2));

                const size_t nr_cave_rooms = s_room_bucket.size() * 2;

                add_to_room_bucket(RoomType::cave, nr_cave_rooms);
        }

        rnd::shuffle(s_room_bucket);

        TRACE_FUNC_END;
}

Room* make(const RoomType type, const R& r)
{
        switch (type) {
        case RoomType::cave:
                return new CaveRoom(r);

        case RoomType::chasm:
                return new ChasmRoom(r);

        case RoomType::crypt:
                return new CryptRoom(r);

        case RoomType::damp:
                return new DampRoom(r);

        case RoomType::pool:
                return new PoolRoom(r);

        case RoomType::human:
                return new HumanRoom(r);

        case RoomType::jail:
                return new JailRoom(r);

        case RoomType::monster:
                return new MonsterRoom(r);

        case RoomType::crawling_pit:
                return new CrawlingPitRoom(r);

        case RoomType::plain:
                return new PlainRoom(r);

        case RoomType::ritual:
                return new RitualRoom(r);

        case RoomType::spider:
                return new SpiderRoom(r);

        case RoomType::forest:
                return new ForestRoom(r);

        case RoomType::corridor:
                return new CorridorRoom(r);

        case RoomType::crumble_room:
                return new CrumbleRoom(r);

        case RoomType::river:
                return new RiverRoom(r);

                // Does not have room classes
        case RoomType::END_OF_STD_ROOMS:
                TRACE << "Illegal room type id: " << (int)type << std::endl;

                ASSERT(false);

                return nullptr;
        }

        ASSERT(false);

        return nullptr;
}

Room* make_random_room(const R& r, const IsSubRoom is_subroom)
{
        TRACE_FUNC_BEGIN_VERBOSE;

        auto room_bucket = std::begin(s_room_bucket);

        Room* room = nullptr;

        while (true) {
                if (room_bucket == std::end(s_room_bucket)) {
                        // No more rooms to pick from, generate a new bucket
                        init_room_bucket();

                        room_bucket = std::begin(s_room_bucket);
                }
                else {
                        // There are still room types in the bucket
                        const RoomType room_type = *room_bucket;

                        room = make(room_type, r);

                        // NOTE: This must be set before "is_allowed()" below is
                        // called (some room types should never exist as sub
                        // rooms).
                        room->m_is_sub_room = (is_subroom == IsSubRoom::yes);

                        if (room->is_allowed()) {
                                s_room_bucket.erase(room_bucket);

                                TRACE_FUNC_END_VERBOSE
                                        << "Made room type: "
                                        << (int)room_type
                                        << std::endl;
                                break;
                        }
                        else {
                                // Room not allowed (e.g. wrong dimensions)
                                delete room;

                                // Try next room type in the bucket
                                ++room_bucket;
                        }
                }
        }

        TRACE_FUNC_END_VERBOSE;

        return room;
}

RoomType str_to_room_type(const std::string& str)
{
        return s_str_to_room_type_map.at(str);
}

#ifndef NDEBUG
std::string room_type_to_str(const RoomType type)
{
        if (s_room_type_to_str_map.find(type) == std::end(s_room_type_to_str_map)) {
                return "ROOM N/A";
        }
        else {
                return s_room_type_to_str_map.at(type);
        }
}
#endif  // NDEBUG

// -----------------------------------------------------------------------------
// Room
// -----------------------------------------------------------------------------
Room::Room(R r, RoomType type) :
        m_r(r),
        m_type(type),
        m_is_sub_room(false) {}

std::vector<P> Room::positions_in_room() const
{
        std::vector<P> positions;

        positions.reserve((size_t)m_r.w() * (size_t)m_r.h());

        for (int x = m_r.p0.x; x <= m_r.p1.x; ++x) {
                for (int y = m_r.p0.y; y <= m_r.p1.y; ++y) {
                        if (map::g_room_map.at(x, y) == this) {
                                positions.emplace_back(x, y);
                        }
                }
        }

        return positions;
}

void Room::on_pre_connect(Array2<bool>& door_proposals)
{
        on_pre_connect_hook(door_proposals);
}

void Room::on_post_connect(Array2<bool>& door_proposals)
{
        place_auto_terrains(*this);

        on_post_connect_hook(door_proposals);

        // Make the room dark?

        // Do not make the room dark if it has a light source

        bool room_has_light_source = false;

        for (int x = m_r.p0.x; x <= m_r.p1.x; ++x) {
                for (int y = m_r.p0.y; y <= m_r.p1.y; ++y) {
                        // TODO: This should really be handled in a more generic
                        // way, but currently the only map terrains that are
                        // light sources are braziers - so it works for now.

                        const terrain::Id id = map::g_terrain.at(x, y)->id();

                        if (id == terrain::Id::brazier) {
                                room_has_light_source = true;
                        }
                }
        }

        if (!room_has_light_source) {
                const int base_pct_chance = base_pct_chance_dark(m_type);

                if (base_pct_chance > 0) {
                        int pct_chance_dark = base_pct_chance_dark(m_type) - 10;

                        // Increase chance with deeper dungeon levels
                        pct_chance_dark += map::g_dlvl;

                        pct_chance_dark = std::clamp(pct_chance_dark, 0, 100);

                        if (rnd::percent(pct_chance_dark)) {
                                make_dark();
                        }
                }
        }
}

void Room::affect_surroundings()
{
        affect_surroundings_hook();
}

void Room::make_dark() const
{
        const size_t nr_positions = map::nr_positions();
        for (size_t i = 0; i < nr_positions; ++i) {
                if (map::g_room_map.at(i) == this) {
                        map::g_dark.at(i) = true;
                }
        }

        // Also make sub rooms dark
        for (Room* const sub_room : m_sub_rooms) {
                sub_room->make_dark();
        }
}

// -----------------------------------------------------------------------------
// Corridor
// -----------------------------------------------------------------------------
void CorridorRoom::on_post_connect_hook(Array2<bool>& door_proposals)
{
        (void)door_proposals;
}

std::vector<RoomAutoTerrainRule> CorridorRoom::auto_terrains_allowed() const
{
        return {};

        // TODO: This can look good:
        // return {
        //         {terrain::Id::wall, 100}};

        // return {
        //         {terrain::Id::statue, rnd::one_in(2) ? rnd::range(10, 20) : 0},
        //         {terrain::Id::chains, rnd::one_in(2) ? rnd::range(1, 2) : 0}};
}

// -----------------------------------------------------------------------------
// Plain room
// -----------------------------------------------------------------------------
std::vector<RoomAutoTerrainRule> PlainRoom::auto_terrains_allowed() const
{
        int fountain_one_in_n = 0;

        if (map::g_dlvl <= 4) {
                fountain_one_in_n = 7;
        }
        else if (map::g_dlvl <= g_dlvl_last_mid_game) {
                fountain_one_in_n = 10;
        }
        else {
                fountain_one_in_n = 20;
        }

        return {
                {terrain::Id::brazier, rnd::one_in(4) ? 1 : 0},
                {terrain::Id::statue, rnd::one_in(7) ? rnd::range(1, 4) : 0},
                {terrain::Id::urn, rnd::one_in(7) ? rnd::range(1, 4) : 0},
                {terrain::Id::fountain, rnd::one_in(fountain_one_in_n) ? 1 : 0},
                {terrain::Id::chains, rnd::one_in(7) ? rnd::range(1, 2) : 0}};
}

void PlainRoom::on_pre_connect_hook(Array2<bool>& door_proposals)
{
        (void)door_proposals;

        if (rnd::coin_toss()) {
                mapgen::cut_room_corners(*this);
        }

        if (rnd::fraction(1, 4)) {
                mapgen::make_pillars_in_room(*this);
        }
}

void PlainRoom::on_post_connect_hook(Array2<bool>& door_proposals)
{
        (void)door_proposals;
}

// -----------------------------------------------------------------------------
// Human room
// -----------------------------------------------------------------------------
std::vector<RoomAutoTerrainRule> HumanRoom::auto_terrains_allowed() const
{
        std::vector<RoomAutoTerrainRule> result;

        result.emplace_back(terrain::Id::brazier, rnd::range(0, 2));
        result.emplace_back(terrain::Id::statue, rnd::range(0, 3));

        if (rnd::one_in(3)) {
                result.emplace_back(terrain::Id::urn, rnd::range(0, 3));
        }

        // Control how many item container terrains that can spawn in the room
        std::vector<terrain::Id> item_containers = {
                terrain::Id::chest,
                terrain::Id::cabinet,
                terrain::Id::bookshelf,
                terrain::Id::alchemist_bench};

        const int nr_item_containers = rnd::range(0, 2);

        for (int i = 0; i < nr_item_containers; ++i) {
                result.emplace_back(rnd::element(item_containers), 1);
        }

        return result;
}

bool HumanRoom::is_allowed() const
{
        return (m_r.min_dim() >= 3) && (m_r.max_dim() <= 8);
}

void HumanRoom::on_pre_connect_hook(Array2<bool>& door_proposals)
{
        (void)door_proposals;

        if (rnd::coin_toss()) {
                mapgen::cut_room_corners(*this);
        }

        if (rnd::fraction(1, 4)) {
                mapgen::make_pillars_in_room(*this);
        }
}

void HumanRoom::on_post_connect_hook(Array2<bool>& door_proposals)
{
        (void)door_proposals;

        if (!rnd::coin_toss()) {
                return;
        }

        Array2<bool> blocked(map::dims());

        map_parsers::BlocksWalking(ParseActors::no)
                .run(blocked, blocked.rect());

        for (int x = m_r.p0.x + 1; x <= m_r.p1.x - 1; ++x) {
                for (int y = m_r.p0.y + 1; y <= m_r.p1.y - 1; ++y) {
                        if (blocked.at(x, y) ||
                            (map::g_room_map.at(x, y) != this)) {
                                continue;
                        }

                        map::set_terrain(
                                terrain::make(terrain::Id::carpet, {x, y}));
                }
        }
}

// -----------------------------------------------------------------------------
// Jail room
// -----------------------------------------------------------------------------
std::vector<RoomAutoTerrainRule> JailRoom::auto_terrains_allowed() const
{
        return {
                {terrain::Id::chains, rnd::range(2, 8)},
                {terrain::Id::brazier, rnd::one_in(4) ? 1 : 0},
                {terrain::Id::rubble_low, rnd::range(1, 4)}};
}

void JailRoom::on_pre_connect_hook(Array2<bool>& door_proposals)
{
        (void)door_proposals;

        if (rnd::coin_toss()) {
                mapgen::cut_room_corners(*this);
        }

        if (rnd::fraction(1, 4)) {
                mapgen::make_pillars_in_room(*this);
        }
}

void JailRoom::on_post_connect_hook(Array2<bool>& door_proposals)
{
        (void)door_proposals;
}

// -----------------------------------------------------------------------------
// Ritual room
// -----------------------------------------------------------------------------
std::vector<RoomAutoTerrainRule> RitualRoom::auto_terrains_allowed() const
{
        if (map::g_dlvl <= g_dlvl_last_mid_game) {
                return {
                        {terrain::Id::altar, 1},
                        {terrain::Id::gong, rnd::one_in(3) ? 1 : 0},
                        {terrain::Id::alchemist_bench, rnd::one_in(4) ? 1 : 0},
                        {terrain::Id::brazier, rnd::range(1, 4)},
                        {terrain::Id::chains, rnd::one_in(7) ? rnd::range(1, 2) : 0},
                        {terrain::Id::urn, rnd::one_in(3) ? rnd::range(1, 2) : 0},
                };
        }
        else {
                // Late game
                return {
                        {terrain::Id::altar, 1},
                        {terrain::Id::rubble_low, rnd::range(1, 2)},
                        {terrain::Id::stalagmite, rnd::range(1, 2)},
                        {terrain::Id::urn, rnd::range(1, 4)},
                };
        }
}

bool RitualRoom::is_allowed() const
{
        return m_r.min_dim() >= 4 && m_r.max_dim() <= 9;
}

void RitualRoom::on_pre_connect_hook(Array2<bool>& door_proposals)
{
        (void)door_proposals;

        if ((map::g_dlvl >= g_dlvl_first_late_game) ||
            ((map::g_dlvl >= g_dlvl_first_mid_game) && rnd::one_in(4))) {
                mapgen::cavify_room(*this);
        }
        else {
                if (rnd::coin_toss()) {
                        mapgen::cut_room_corners(*this);
                }
        }

        if (rnd::fraction(1, 4)) {
                mapgen::make_pillars_in_room(*this);
        }
}

void RitualRoom::on_post_connect_hook(Array2<bool>& door_proposals)
{
        (void)door_proposals;

        Array2<bool> blocked(map::dims());

        map_parsers::BlocksWalking(ParseActors::no)
                .run(blocked, blocked.rect());

        const int bloody_chamber_pct = 60;

        if (rnd::percent(bloody_chamber_pct)) {
                P origin(-1, -1);
                std::vector<P> origin_bucket;

                for (int y = m_r.p0.y; y <= m_r.p1.y; ++y) {
                        for (int x = m_r.p0.x; x <= m_r.p1.x; ++x) {
                                if (map::g_terrain.at(x, y)->id() == terrain::Id::altar) {
                                        origin = P(x, y);
                                        y = 999;
                                        x = 999;
                                }
                                else {
                                        if (!blocked.at(x, y)) {
                                                origin_bucket.emplace_back(x, y);
                                        }
                                }
                        }
                }

                if (!origin_bucket.empty()) {
                        if (origin.x == -1) {
                                origin = rnd::element(origin_bucket);
                        }

                        for (const P& d : dir_utils::g_dir_list) {
                                if (rnd::percent(bloody_chamber_pct / 2)) {
                                        const P pos = origin + d;

                                        if (!blocked.at(pos)) {
                                                terrain::make_gore(pos);
                                                terrain::make_blood(pos);
                                        }
                                }
                        }
                }
        }
}

// -----------------------------------------------------------------------------
// Spider room
// -----------------------------------------------------------------------------
std::vector<RoomAutoTerrainRule> SpiderRoom::auto_terrains_allowed() const
{
        return {{terrain::Id::cocoon, rnd::range(0, 3)}};
}

bool SpiderRoom::is_allowed() const
{
        return m_r.min_dim() >= 3 && m_r.max_dim() <= 8;
}

void SpiderRoom::on_pre_connect_hook(Array2<bool>& door_proposals)
{
        (void)door_proposals;

        const bool is_early = map::g_dlvl <= g_dlvl_last_early_game;

        const bool is_mid = !is_early && (map::g_dlvl <= g_dlvl_last_mid_game);

        if (is_early || (is_mid && rnd::coin_toss())) {
                if (rnd::coin_toss()) {
                        mapgen::cut_room_corners(*this);
                }
        }
        else {
                mapgen::cavify_room(*this);
        }

        if ((is_early || is_mid) && rnd::fraction(1, 4)) {
                mapgen::make_pillars_in_room(*this);
        }
}

void SpiderRoom::on_post_connect_hook(Array2<bool>& door_proposals)
{
        (void)door_proposals;
}

void SpiderRoom::affect_surroundings_hook()
{
        P starting_pos(-1, -1);

        std::vector<P> room_positions = positions_in_room();

        rnd::shuffle(room_positions);

        Array2<bool> blocks_walking(map::dims());
        Array2<bool> blocks_traps(map::dims());

        map_parsers::BlocksWalking(ParseActors::no)
                .run(blocks_walking, blocks_walking.rect());

        map_parsers::BlocksTraps()
                .run(blocks_traps, blocks_traps.rect());

        for (const P& pos : map::positions()) {
                if (map::g_terrain.at(pos)->id() == terrain::Id::door) {
                        blocks_walking.at(pos) = false;
                }
        }

        for (const P& pos : room_positions) {
                if (!blocks_walking.at(pos) && !blocks_traps.at(pos)) {
                        starting_pos = pos;
                }
        }

        if (starting_pos.x == -1) {
                return;
        }

        const int travel_limit = rnd::range_binom(3, 12, 0.4);

        const Fraction web_chance(1, 6);

        const Array2<int> flood = floodfill(starting_pos, blocks_walking, travel_limit);

        for (const P& pos : map::positions()) {
                const bool is_reached = (flood.at(pos) > 0) || (pos == starting_pos);

                if (is_reached &&
                    !blocks_traps.at(pos) &&
                    web_chance.roll()) {
                        terrain::Trap* const trap =
                                populate_traps::try_make_trap(
                                        terrain::TrapId::web,
                                        pos);

                        if (trap) {
                                map::set_terrain(trap);

                                TRACE << "### WEB, " << pos.x << "," << pos.y << std::endl;
                        }
                }
        }
}

// -----------------------------------------------------------------------------
// Crawling pit room
// -----------------------------------------------------------------------------
std::vector<RoomAutoTerrainRule> CrawlingPitRoom::auto_terrains_allowed() const
{
        return {};
}

bool CrawlingPitRoom::is_allowed() const
{
        return m_r.min_dim() >= 2 && m_r.max_dim() <= 6;
}

void CrawlingPitRoom::on_pre_connect_hook(Array2<bool>& door_proposals)
{
        (void)door_proposals;

        if ((map::g_dlvl >= g_dlvl_first_late_game) || rnd::fraction(3, 4)) {
                mapgen::cavify_room(*this);
        }
        else {
                // Late game
                mapgen::make_pillars_in_room(*this);
        }
}

void CrawlingPitRoom::on_post_connect_hook(Array2<bool>& door_proposals)
{
        (void)door_proposals;

        // Put lots of rubble, to make the room more "pit like"
        for (int x = m_r.p0.x; x <= m_r.p1.x; ++x) {
                for (int y = m_r.p0.y; y <= m_r.p1.y; ++y) {
                        if (map::g_room_map.at(x, y) != this) {
                                continue;
                        }

                        const P p(x, y);

                        if (!map::g_terrain.at(p)->m_data->is_floor_like ||
                            !rnd::coin_toss()) {
                                continue;
                        }

                        map::set_terrain(terrain::make(terrain::Id::rubble_low, p));
                }
        }
}

std::string CrawlingPitRoom::get_random_monster_type() const
{
        std::vector<std::string> actor_id_bucket;

        // Snakes
        if (map::g_dlvl <= g_dlvl_last_mid_game) {
                for (const auto& it : actor::g_data) {
                        const actor::ActorData& d = it.second;

                        // NOTE: We do not allow Spitting Cobras, because it's
                        // VERY tedious to fight swarms of them (attack, get
                        // blinded, back away, repeat...).
                        if (d.is_snake && (d.id != "MON_SPITTING_COBRA")) {
                                actor_id_bucket.push_back(d.id);
                        }
                }
        }

        // Worm Masses
        if (map::g_dlvl <= g_dlvl_last_mid_game) {
                actor_id_bucket.emplace_back("MON_WORM_MASS");
        }

        // Mind Worms
        if ((map::g_dlvl >= 3) && (map::g_dlvl <= g_dlvl_last_mid_game)) {
                actor_id_bucket.emplace_back("MON_MIND_WORM");
        }

        // Primordial Worms
        if (map::g_dlvl >= g_dlvl_first_late_game) {
                actor_id_bucket.emplace_back("MON_PRIMORDIAL_WORM");
        }

        return rnd::element(actor_id_bucket);
}

void CrawlingPitRoom::populate_monsters() const
{
        const auto actor_id = get_random_monster_type();

        auto blocked = populate_mon::forbidden_spawn_positions();

        const int nr_groups = rnd::range(3, 4);

        for (int group_idx = 0; group_idx < nr_groups; ++group_idx) {
                // Find an origin
                std::vector<P> origin_bucket;

                for (int x = m_r.p0.x; x <= m_r.p1.x; ++x) {
                        for (int y = m_r.p0.y; y <= m_r.p1.y; ++y) {
                                if (!blocked.at(x, y) &&
                                    (map::g_room_map.at(x, y) == this)) {
                                        origin_bucket.emplace_back(x, y);
                                }
                        }
                }

                if (origin_bucket.empty()) {
                        return;
                }

                const auto origin = rnd::element(origin_bucket);

                const auto sorted_free_positions =
                        populate_mon::make_sorted_free_cells(origin, blocked);

                if (sorted_free_positions.empty()) {
                        return;
                }

                populate_mon::make_group_at(
                        actor_id,
                        sorted_free_positions,
                        &blocked,  // New blocked positions (output)
                        MonRoamingAllowed::yes);
        }
}

// -----------------------------------------------------------------------------
// Crypt room
// -----------------------------------------------------------------------------
std::vector<RoomAutoTerrainRule> CryptRoom::auto_terrains_allowed() const
{
        return {
                {terrain::Id::tomb, rnd::one_in(6) ? 2 : 1},
                {terrain::Id::urn, rnd::range(0, 3)},
                {terrain::Id::rubble_low, rnd::range(0, 3)},
        };
}

bool CryptRoom::is_allowed() const
{
        return m_r.min_dim() >= 2 && m_r.max_dim() <= 14;
}

void CryptRoom::on_pre_connect_hook(Array2<bool>& door_proposals)
{
        (void)door_proposals;

        if (rnd::coin_toss()) {
                mapgen::cut_room_corners(*this);
        }

        if (rnd::fraction(1, 3)) {
                mapgen::make_pillars_in_room(*this);
        }
}

void CryptRoom::on_post_connect_hook(Array2<bool>& door_proposals)
{
        (void)door_proposals;
}

// -----------------------------------------------------------------------------
// Monster room
// -----------------------------------------------------------------------------
std::vector<RoomAutoTerrainRule> MonsterRoom::auto_terrains_allowed() const
{
        return {{terrain::Id::rubble_low, rnd::range(3, 6)}};
}

bool MonsterRoom::is_allowed() const
{
        return m_r.min_dim() >= 4 && m_r.max_dim() <= 8;
}

void MonsterRoom::on_pre_connect_hook(Array2<bool>& door_proposals)
{
        (void)door_proposals;

        const bool is_early =
                map::g_dlvl <= g_dlvl_last_early_game;

        const bool is_mid =
                !is_early &&
                (map::g_dlvl <= g_dlvl_last_mid_game);

        if (is_early || is_mid) {
                if (rnd::fraction(3, 4)) {
                        mapgen::cut_room_corners(*this);
                }

                if (rnd::fraction(1, 3)) {
                        mapgen::make_pillars_in_room(*this);
                }
        }
        else {
                // Is late game
                mapgen::cavify_room(*this);
        }
}

void MonsterRoom::on_post_connect_hook(Array2<bool>& door_proposals)
{
        (void)door_proposals;

        Array2<bool> blocked(map::dims());

        map_parsers::BlocksWalking(ParseActors::no)
                .run(blocked, blocked.rect());

        int nr_blood_put = 0;

        // TODO: Hacky, needs improving
        const int nr_tries = 1000;

        for (int i = 0; i < nr_tries; ++i) {
                for (int x = m_r.p0.x; x <= m_r.p1.x; ++x) {
                        for (int y = m_r.p0.y; y <= m_r.p1.y; ++y) {
                                if (!blocked.at(x, y) &&
                                    map::g_room_map.at(x, y) == this &&
                                    rnd::fraction(2, 5)) {
                                        terrain::make_gore({x, y});
                                        terrain::make_blood({x, y});
                                        nr_blood_put++;
                                }
                        }
                }

                if (nr_blood_put > 0) {
                        break;
                }
        }
}

// -----------------------------------------------------------------------------
// Damp room
// -----------------------------------------------------------------------------
std::vector<RoomAutoTerrainRule> DampRoom::auto_terrains_allowed() const
{
        if (rnd::coin_toss()) {
                return {{terrain::Id::vines, rnd::range(2, 8)}};
        }
        else {
                return {};
        }
}

bool DampRoom::is_allowed() const
{
        return true;
}

void DampRoom::on_pre_connect_hook(Array2<bool>& door_proposals)
{
        (void)door_proposals;

        const bool is_early = map::g_dlvl <= g_dlvl_last_early_game;

        const bool is_mid = !is_early && (map::g_dlvl <= g_dlvl_last_mid_game);

        if (is_early || (is_mid && rnd::coin_toss())) {
                if (rnd::coin_toss()) {
                        mapgen::cut_room_corners(*this);
                }
        }
        else {
                mapgen::cavify_room(*this);
        }

        if ((is_early || is_mid) && rnd::fraction(1, 3)) {
                mapgen::make_pillars_in_room(*this);
        }
}

void DampRoom::on_post_connect_hook(Array2<bool>& door_proposals)
{
        (void)door_proposals;

        Array2<bool> blocked(map::dims());

        map_parsers::BlocksWalking(ParseActors::no)
                .run(blocked, blocked.rect());

        const int liquid_one_in_n = rnd::range(2, 5);

        const LiquidType liquid_type = rnd::coin_toss() ? LiquidType::water : LiquidType::mud;

        for (int x = m_r.p0.x; x <= m_r.p1.x; ++x) {
                for (int y = m_r.p0.y; y <= m_r.p1.y; ++y) {
                        if (blocked.at(x, y) ||
                            (map::g_room_map.at(x, y) != this) ||
                            !rnd::one_in(liquid_one_in_n)) {
                                continue;
                        }

                        auto* const liquid =
                                static_cast<terrain::Liquid*>(
                                        terrain::make(
                                                terrain::Id::liquid,
                                                {x, y}));

                        liquid->m_type = liquid_type;

                        map::set_terrain(liquid);
                }
        }
}

// -----------------------------------------------------------------------------
// Pool room
// -----------------------------------------------------------------------------
std::vector<RoomAutoTerrainRule> PoolRoom::auto_terrains_allowed() const
{
        if (rnd::coin_toss()) {
                return {{terrain::Id::vines, rnd::range(2, 8)}};
        }
        else {
                return {};
        }
}

bool PoolRoom::is_allowed() const
{
        return m_r.min_dim() >= 5;
}

void PoolRoom::on_pre_connect_hook(Array2<bool>& door_proposals)
{
        (void)door_proposals;

        const bool is_early = (map::g_dlvl <= g_dlvl_last_early_game);
        const bool is_mid = (!is_early && (map::g_dlvl <= g_dlvl_last_mid_game));

        if (is_early || (is_mid && rnd::coin_toss()) || m_is_sub_room) {
                if (rnd::coin_toss()) {
                        mapgen::cut_room_corners(*this);
                }
        }
        else {
                mapgen::cavify_room(*this);
        }

        if ((is_early || is_mid) && rnd::fraction(1, 3)) {
                mapgen::make_pillars_in_room(*this);
        }
}

void PoolRoom::on_post_connect_hook(Array2<bool>& door_proposals)
{
        (void)door_proposals;

        Array2<bool> blocked(map::dims());

        map_parsers::BlocksWalking(ParseActors::no)
                .run(blocked, blocked.rect());

        std::vector<P> origin_bucket;

        origin_bucket.reserve((size_t)m_r.w() * (size_t)m_r.h());

        for (int x = 0; x < blocked.w(); ++x) {
                for (int y = 0; y < blocked.w(); ++y) {
                        if (blocked.at(x, y)) {
                                continue;
                        }

                        if (map::g_room_map.at(x, y) == this) {
                                origin_bucket.emplace_back(x, y);
                        }
                        else {
                                blocked.at(x, y) = true;
                        }
                }
        }

        if (origin_bucket.empty()) {
                // There are no free room positions
                ASSERT(false);

                return;
        }

        const P origin = rnd::element(origin_bucket);

        const int flood_travel_limit = rnd::range(3, 12);

        const bool allow_diagonal_flood = true;

        const auto flood =
                floodfill(
                        origin,
                        blocked,
                        flood_travel_limit,
                        {-1, -1},  // Target position
                        allow_diagonal_flood);

        // Do not place any liquid if we cannot place at least a certain amount.
        {
                const int min_nr_liquid_positions = 4;

                int nr_liquid_positions = 0;

                for (int x = 0; x < flood.w(); ++x) {
                        for (int y = 0; y < flood.w(); ++y) {
                                const P p(x, y);

                                const bool should_put_liquid = (flood.at(p) > 0) || (p == origin);

                                if (should_put_liquid) {
                                        ++nr_liquid_positions;
                                }
                        }
                }

                if (nr_liquid_positions < min_nr_liquid_positions) {
                        return;
                }
        }

        // OK, we can place liquid

        // Decide if this is a "natural" pool or an "artificial" pool.
        bool is_artificial_pool = true;

        if (map::g_dlvl >= g_dlvl_first_late_game) {
                // Late game, only make natural pools.
                is_artificial_pool = false;
        }
        else if (map::g_dlvl >= g_dlvl_first_mid_game) {
                // Mid game, same chance of natural or artificial.
                is_artificial_pool = rnd::coin_toss();
        }
        else {
                // Early game, make mostly artificial pools.
                is_artificial_pool = rnd::fraction(3, 4);
        }

        // Decide liquid type. If artificial pool, only make water (there
        // should not be mud baths in the dungeon...).
        auto liquid_type = LiquidType::water;

        if (!is_artificial_pool) {
                liquid_type = rnd::coin_toss() ? LiquidType::water : LiquidType::mud;
        }

        for (int x = 0; x < flood.w(); ++x) {
                for (int y = 0; y < flood.w(); ++y) {
                        const P p(x, y);

                        const bool should_put_liquid = (flood.at(p) > 0) || (p == origin);

                        if (!should_put_liquid) {
                                continue;
                        }

                        // If artificial pool, just fill as far as possible
                        // within the floodfill, for a more rectangular
                        // "constructed" look, otherwise put liquid randomly.
                        if (is_artificial_pool ||
                            (flood.at(p) < (flood_travel_limit / 2)) ||
                            rnd::fraction(2, 3)) {
                                auto* const liquid =
                                        static_cast<terrain::Liquid*>(
                                                terrain::make(terrain::Id::liquid, p));

                                liquid->m_type = liquid_type;

                                map::set_terrain(liquid);
                        }
                }
        }
}

// -----------------------------------------------------------------------------
// Cave room
// -----------------------------------------------------------------------------
std::vector<RoomAutoTerrainRule> CaveRoom::auto_terrains_allowed() const
{
        std::vector<RoomAutoTerrainRule> result {
                {terrain::Id::rubble_low, rnd::range(2, 4)},
                {terrain::Id::stalagmite, rnd::range(1, 4)},
                {terrain::Id::urn, rnd::one_in(7) ? rnd::range(1, 4) : 0},
        };

        if ((map::g_dlvl >= g_dlvl_first_late_game) && m_is_sub_room) {
                result.emplace_back(terrain::Id::chest, rnd::range(1, 3));
                result.emplace_back(terrain::Id::tomb, rnd::coin_toss() ? rnd::range(1, 2) : 0);
                result.emplace_back(terrain::Id::altar, rnd::one_in(6) ? 1 : 0);
        }

        return result;
}

bool CaveRoom::is_allowed() const
{
        // NOTE: Caves are only allowed as sub rooms in late game. The rationale
        // for this is that in the late game there are no "plain" rooms, so
        // every sub room would otherwise be a pool room, spider room, etc.

        if (map::g_dlvl <= g_dlvl_last_mid_game) {
                return !m_is_sub_room;
        }
        else {
                return true;
        }
}

void CaveRoom::on_pre_connect_hook(Array2<bool>& door_proposals)
{
        (void)door_proposals;

        mapgen::cavify_room(*this);
}

void CaveRoom::on_post_connect_hook(Array2<bool>& door_proposals)
{
        (void)door_proposals;
}

// -----------------------------------------------------------------------------
// Forest room
// -----------------------------------------------------------------------------
bool ForestRoom::is_allowed() const
{
        return !m_is_sub_room && (m_r.min_dim() >= 5);
}

void ForestRoom::on_pre_connect_hook(Array2<bool>& door_proposals)
{
        (void)door_proposals;

        mapgen::cavify_room(*this);
}

void ForestRoom::on_post_connect_hook(Array2<bool>& door_proposals)
{
        (void)door_proposals;

        Array2<bool> blocked(map::dims());

        map_parsers::BlocksWalking(ParseActors::no)
                .run(blocked, blocked.rect());

        const std::vector<terrain::Id> free_terrains = {
                terrain::Id::stairs,
                terrain::Id::door,
        };

        for (const auto& p : blocked.rect().positions()) {
                const bool is_free_terrain =
                        map_parsers::IsAnyOfTerrains(free_terrains)
                                .run(p);

                if (is_free_terrain) {
                        blocked.at(p) = false;
                }
        }

        std::vector<P> tree_pos_bucket;

        for (const auto& p : m_r.positions()) {
                const bool is_floor_like =
                        map::g_terrain.at(p)->m_data->is_floor_like;

                const bool is_this_room = (map::g_room_map.at(p) == this);

                if (blocked.at(p) || !is_floor_like || !is_this_room) {
                        continue;
                }

                tree_pos_bucket.push_back(p);

                if (rnd::one_in(10)) {
                        map::set_terrain(terrain::make(terrain::Id::bush, p));
                }
                else {
                        map::set_terrain(terrain::make(terrain::Id::grass, p));
                }
        }

        rnd::shuffle(tree_pos_bucket);

        const int tree_one_in_n = rnd::range(3, 10);

        while (!tree_pos_bucket.empty()) {
                const auto p = tree_pos_bucket.back();

                tree_pos_bucket.pop_back();

                if (!rnd::one_in(tree_one_in_n)) {
                        continue;
                }

                blocked.at(p) = true;

                if (map_parsers::is_map_connected(blocked)) {
                        map::set_terrain(terrain::make(terrain::Id::tree, p));
                }
                else {
                        blocked.at(p) = false;
                }
        }
}

// -----------------------------------------------------------------------------
// Chasm room
// -----------------------------------------------------------------------------
std::vector<RoomAutoTerrainRule> ChasmRoom::auto_terrains_allowed() const
{
        return {};
}

bool ChasmRoom::is_allowed() const
{
        return ((m_r.min_dim() >= 5) && (m_r.max_dim() <= 9) && !m_is_sub_room);
}

void ChasmRoom::on_pre_connect_hook(Array2<bool>& door_proposals)
{
        (void)door_proposals;

        mapgen::cavify_room(*this);
}

void ChasmRoom::on_post_connect_hook(Array2<bool>& door_proposals)
{
        (void)door_proposals;

        Array2<bool> blocked(map::dims());

        map_parsers::BlocksWalking(ParseActors::no)
                .run(blocked, blocked.rect());

        const size_t nr_positions = map::nr_positions();
        for (size_t i = 0; i < nr_positions; ++i) {
                if (map::g_room_map.at(i) != this) {
                        blocked.at(i) = true;
                }
        }

        blocked = map_parsers::expand(blocked, blocked.rect());

        P origin;

        for (int x = m_r.p0.x; x <= m_r.p1.x; ++x) {
                for (int y = m_r.p0.y; y <= m_r.p1.y; ++y) {
                        if (!blocked.at(x, y)) {
                                origin.set(x, y);
                        }
                }
        }

        const auto flood = floodfill(
                origin,
                blocked,
                10000,
                {-1, -1},
                false);

        for (int x = m_r.p0.x; x <= m_r.p1.x; ++x) {
                for (int y = m_r.p0.y; y <= m_r.p1.y; ++y) {
                        const P p(x, y);

                        if (p == origin || flood.at(x, y) != 0) {
                                map::set_terrain(
                                        terrain::make(terrain::Id::chasm, p));
                        }
                }
        }
}

// -----------------------------------------------------------------------------
// River room
// -----------------------------------------------------------------------------
void RiverRoom::on_pre_connect_hook(Array2<bool>& door_proposals)
{
        TRACE_FUNC_BEGIN;

        // Strategy: Expand the the river on both sides until parallel to the
        // closest center position of another room

        const bool is_hor = m_axis == Axis::hor;

        TRACE << "Finding room centers" << std::endl;
        Array2<bool> centers(map::dims());

        for (Room* const room : map::g_room_list) {
                if (room != this) {
                        const P c_pos(room->m_r.center());

                        centers.at(c_pos) = true;
                }
        }

        TRACE << "Finding closest room center coordinates on both sides "
                 "(y coordinate if horizontal river, x if vertical)"
              << std::endl;

        int closest_center0 = -1;
        int closest_center1 = -1;

        // Using nestled scope to avoid declaring x and y at function scope
        {
                int x = 0;
                int y = 0;

                // i_outer and i_inner should be references to x or y.
                auto find_closest_center0 =
                        [&](const Range& r_outer,
                            const Range& r_inner,
                            int& i_outer,
                            int& i_inner) {
                                for (i_outer = r_outer.min;
                                     i_outer >= r_outer.max;
                                     --i_outer) {
                                        for (i_inner = r_inner.min;
                                             i_inner <= r_inner.max;
                                             ++i_inner) {
                                                if (centers.at(x, y)) {
                                                        closest_center0 =
                                                                i_outer;
                                                        break;
                                                }
                                        }

                                        if (closest_center0 != -1) {
                                                break;
                                        }
                                }
                        };

                auto find_closest_center1 =
                        [&](const Range& r_outer,
                            const Range& r_inner,
                            int& i_outer,
                            int& i_inner) {
                                for (i_outer = r_outer.min;
                                     i_outer <= r_outer.max;
                                     ++i_outer) {
                                        for (i_inner = r_inner.min;
                                             i_inner <= r_inner.max;
                                             ++i_inner) {
                                                if (centers.at(x, y)) {
                                                        closest_center1 =
                                                                i_outer;
                                                        break;
                                                }
                                        }

                                        if (closest_center1 != -1) {
                                                break;
                                        }
                                }
                        };

                if (is_hor) {
                        const int river_y = m_r.p0.y;

                        find_closest_center0(
                                Range(river_y - 1, 1),
                                Range(1, map::w() - 2),
                                y,
                                x);

                        find_closest_center1(
                                Range(river_y + 1, map::h() - 2),
                                Range(1, map::w() - 2),
                                y,
                                x);
                }
                else {
                        // Vertical
                        const int river_x = m_r.p0.x;

                        find_closest_center0(
                                Range(river_x - 1, 1),
                                Range(1, map::h() - 2),
                                x,
                                y);

                        find_closest_center1(
                                Range(river_x + 1, map::w() - 2),
                                Range(1, map::h() - 2),
                                x,
                                y);
                }
        }

        TRACE << "Expanding and filling river" << std::endl;

        Array2<bool> blocked(map::dims());

        // Within the expansion limits, mark all positions not belonging to
        // another room as free. All other positions are considered as blocking.
        for (int x = 0; x < map::w(); ++x) {
                for (int y = 0; y < map::h(); ++y) {
                        blocked.at(x, y) = true;

                        if ((m_axis == Axis::hor &&
                             (y >= closest_center0 && y <= closest_center1)) ||
                            (m_axis == Axis::ver &&
                             (x >= closest_center0 && x <= closest_center1))) {
                                Room* r = map::g_room_map.at(x, y);

                                blocked.at(x, y) = r && r != this;
                        }
                }
        }

        blocked = map_parsers::expand(blocked, blocked.rect());

        const P origin(m_r.center());

        const auto flood = floodfill(
                origin,
                blocked,
                INT_MAX,
                {-1, -1},
                true);

        for (int x = 0; x < map::w(); ++x) {
                for (int y = 0; y < map::h(); ++y) {
                        const P p(x, y);

                        if (flood.at(x, y) > 0 || p == origin) {
                                map::set_terrain(
                                        terrain::make(terrain::Id::chasm, p));

                                map::g_room_map.at(x, y) = this;

                                m_r.p0.x = std::min(m_r.p0.x, x);
                                m_r.p0.y = std::min(m_r.p0.y, y);
                                m_r.p1.x = std::max(m_r.p1.x, x);
                                m_r.p1.y = std::max(m_r.p1.y, y);
                        }
                }
        }

        TRACE << "Making bridge(s)" << std::endl;

        // Mark which side each position belongs to
        enum Side
        {
                in_river,
                side0,
                side1
        };

        Array2<Side> sides(map::dims());

        // Scoping to avoid declaring x and y at function scope
        {
                int x = 0;
                int y = 0;

                // i_outer and i_inner should be references to x or y.
                auto mark_sides =
                        [&](const Range& r_outer,
                            const Range& r_inner,
                            int& i_outer,
                            int& i_inner) {
                                for (i_outer = r_outer.min;
                                     i_outer <= r_outer.max;
                                     ++i_outer) {
                                        bool is_on_side0 = true;

                                        for (i_inner = r_inner.min;
                                             i_inner <= r_inner.max;
                                             ++i_inner) {
                                                if (map::g_room_map.at(x, y) == this) {
                                                        is_on_side0 = false;
                                                        sides.at(x, y) = in_river;
                                                }
                                                else {
                                                        sides.at(x, y) =
                                                                is_on_side0
                                                                ? side0
                                                                : side1;
                                                }
                                        }
                                }
                        };

                if (m_axis == Axis::hor) {
                        mark_sides(
                                Range(1, map::w() - 2),
                                Range(1, map::h() - 2),
                                x,
                                y);
                }
                else {
                        mark_sides(
                                Range(1, map::h() - 2),
                                Range(1, map::w() - 2),
                                y,
                                x);
                }
        }

        Array2<bool> valid_room_entries0(map::dims());
        Array2<bool> valid_room_entries1(map::dims());

        const size_t nr_positions = map::nr_positions();
        for (size_t i = 0; i < nr_positions; ++i) {
                valid_room_entries0.at(i) = valid_room_entries1.at(i) = false;
        }

        const int edge_d = 4;

        for (int x = edge_d; x < map::w() - edge_d; ++x) {
                for (int y = edge_d; y < map::h() - edge_d; ++y) {
                        const auto terrain_id =
                                map::g_terrain.at(x, y)->id();

                        if ((terrain_id != terrain::Id::wall) ||
                            map::g_room_map.at(x, y)) {
                                continue;
                        }

                        const P p(x, y);
                        int nr_cardinal_floor = 0;
                        int nr_cardinal_river = 0;

                        for (const auto& d : dir_utils::g_cardinal_list) {
                                const auto p_adj(p + d);

                                const auto* const t = map::g_terrain.at(p_adj);

                                if (t->id() == terrain::Id::floor) {
                                        nr_cardinal_floor++;
                                }

                                if (map::g_room_map.at(p_adj) == this) {
                                        nr_cardinal_river++;
                                }
                        }

                        if (nr_cardinal_floor == 1 &&
                            nr_cardinal_river == 1) {
                                switch (sides.at(x, y)) {
                                case side0:
                                        valid_room_entries0.at(x, y) = true;
                                        break;

                                case side1:
                                        valid_room_entries1.at(x, y) = true;
                                        break;

                                case in_river:
                                        break;
                                }
                        }
                }
        }

        std::vector<int> positions(
                is_hor
                        ? map::w()
                        : map::h());

        std::iota(std::begin(positions), std::end(positions), 0);

        rnd::shuffle(positions);

        std::vector<int> c_built;

        const int min_edge_dist = 6;

        const int max_nr_bridges = rnd::range(1, 3);

        for (const int bridge_n : positions) {
                if ((bridge_n < min_edge_dist) ||
                    (is_hor && (bridge_n > (map::w() - 1 - min_edge_dist))) ||
                    (!is_hor && (bridge_n > (map::h() - 1 - min_edge_dist)))) {
                        continue;
                }

                bool is_too_close_to_other_bridge = false;

                const int min_d = 2;

                for (int c_other : c_built) {
                        const bool is_in_range = Range(
                                                         c_other - min_d,
                                                         c_other + min_d)
                                                         .is_in_range(bridge_n);

                        if (is_in_range) {
                                is_too_close_to_other_bridge = true;
                                break;
                        }
                }

                if (is_too_close_to_other_bridge) {
                        continue;
                }

                // Check if current bridge coord would connect matching room
                // connections. If so both room_con0 and room_con1 will be set.
                P room_con0(-1, -1);
                P room_con1(-1, -1);

                const int c0_0 = is_hor ? m_r.p1.y : m_r.p1.x;
                const int c1_0 = is_hor ? m_r.p0.y : m_r.p0.x;

                for (int c = c0_0; c != c1_0; --c) {
                        if ((is_hor && sides.at(bridge_n, c) == side0) ||
                            (!is_hor && sides.at(c, bridge_n) == side0)) {
                                break;
                        }

                        const P p_nxt = is_hor ? P(bridge_n, c - 1) : P(c - 1, bridge_n);

                        if (valid_room_entries0.at(p_nxt)) {
                                room_con0 = p_nxt;
                                break;
                        }
                }

                const int c0_1 = is_hor ? m_r.p0.y : m_r.p0.x;
                const int c1_1 = is_hor ? m_r.p1.y : m_r.p1.x;

                for (int c = c0_1; c != c1_1; ++c) {
                        if ((is_hor && sides.at(bridge_n, c) == side1) ||
                            (!is_hor && sides.at(c, bridge_n) == side1)) {
                                break;
                        }

                        const P p_nxt = is_hor ? P(bridge_n, c + 1) : P(c + 1, bridge_n);

                        if (valid_room_entries1.at(p_nxt)) {
                                room_con1 = p_nxt;
                                break;
                        }
                }

                // Make the bridge if valid connection pairs found
                if (room_con0.x != -1 && room_con1.x != -1) {
                        TRACE << "Found valid connection pair at: "
                              << room_con0.x << "," << room_con0.y << " / "
                              << room_con1.x << "," << room_con1.y << std::endl
                              << "Making bridge at pos: " << bridge_n
                              << std::endl;

                        if (is_hor) {
                                for (int y = room_con0.y; y <= room_con1.y; ++y) {
                                        if (map::g_room_map.at(bridge_n, y) !=
                                            this) {
                                                continue;
                                        }

                                        auto* const floor =
                                                static_cast<terrain::Floor*>(
                                                        terrain::make(
                                                                terrain::Id::floor,
                                                                {bridge_n, y}));

                                        floor->m_type = terrain::FloorType::common;

                                        map::set_terrain(floor);
                                }
                        }
                        else {
                                // Vertical
                                for (int x = room_con0.x; x <= room_con1.x; ++x) {
                                        if (map::g_room_map.at(x, bridge_n) !=
                                            this) {
                                                continue;
                                        }

                                        auto* const floor =
                                                static_cast<terrain::Floor*>(
                                                        terrain::make(
                                                                terrain::Id::floor,
                                                                {x, bridge_n}));

                                        floor->m_type =
                                                terrain::FloorType::common;

                                        map::set_terrain(floor);
                                }
                        }

                        map::set_terrain(
                                terrain::make(terrain::Id::floor, room_con0));

                        map::set_terrain(
                                terrain::make(terrain::Id::floor, room_con1));

                        door_proposals.at(room_con0) = true;
                        door_proposals.at(room_con1) = true;

                        c_built.push_back(bridge_n);
                }

                if (int(c_built.size()) >= max_nr_bridges) {
                        TRACE << "Enough bridges built" << std::endl;
                        break;
                }
        }

        TRACE << "Bridges built/attempted: "
              << c_built.size() << "/"
              << max_nr_bridges << std::endl;

        if (c_built.empty()) {
                mapgen::g_is_map_valid = false;
        }
        else {
                // Map is valid (at least one bridge was built)
                Array2<bool> valid_room_entries(map::dims());

                for (int x = 0; x < map::w(); ++x) {
                        for (int y = 0; y < map::h(); ++y) {
                                valid_room_entries.at(x, y) =
                                        valid_room_entries0.at(x, y) ||
                                        valid_room_entries1.at(x, y);

                                // Convert some remaining valid room entries
                                if (valid_room_entries.at(x, y) &&
                                    (std::find(
                                             std::begin(c_built),
                                             std::end(c_built),
                                             x) ==
                                     std::end(c_built))) {
                                        map::set_terrain(
                                                terrain::make(
                                                        terrain::Id::floor,
                                                        {x, y}));

                                        map::g_room_map.at(x, y) = this;
                                }
                        }
                }

                // Convert wall positions adjacent to river positions to river
                valid_room_entries = map_parsers::expand(valid_room_entries, 2);

                for (int x = 2; x < map::w() - 2; ++x) {
                        for (int y = 2; y < map::h() - 2; ++y) {
                                if (valid_room_entries.at(x, y) &&
                                    map::g_room_map.at(x, y) == this) {
                                        auto* const floor =
                                                static_cast<terrain::Floor*>(
                                                        terrain::make(
                                                                terrain::Id::floor,
                                                                {x, y}));

                                        floor->m_type = terrain::FloorType::common;

                                        map::set_terrain(floor);

                                        map::g_room_map.at(x, y) = nullptr;
                                }
                        }
                }
        }

        TRACE_FUNC_END;

}  // RiverRoom::on_pre_connect

}  // namespace room

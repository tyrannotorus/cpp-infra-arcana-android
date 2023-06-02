// =============================================================================
// Copyright 2011-2023 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "actor.hpp"
#include "actor_factory.hpp"
#include "array2.hpp"
#include "debug.hpp"
#include "direction.hpp"
#include "game_time.hpp"
#include "global.hpp"
#include "highscore.hpp"
#include "item_data.hpp"
#include "item_factory.hpp"
#include "map.hpp"
#include "map_builder.hpp"
#include "map_controller.hpp"
#include "map_parsing.hpp"
#include "misc.hpp"
#include "player_bon.hpp"
#include "populate_items.hpp"
#include "populate_monsters.hpp"
#include "pos.hpp"
#include "property.hpp"
#include "property_data.hpp"
#include "property_factory.hpp"
#include "property_handler.hpp"
#include "random.hpp"
#include "room.hpp"
#include "terrain.hpp"
#include "terrain_data.hpp"
#include "terrain_door.hpp"
#include "terrain_factory.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static void put_intro_forest_graves(
        const std::vector<P>& available_positions,
        const std::vector<HighscoreEntry>& highscores)
{
        size_t entry_idx = 0;

        // NOTE: Here we assume the following:
        // * The positions are sorted from left to right
        // * The highscore entries are sorted from highest to lowest
        for (auto it = std::rbegin(available_positions);
             it != std::rend(available_positions);
             ++it) {
                const P& pos = *it;

                auto* const grave =
                        static_cast<terrain::GraveStone*>(
                                terrain::make(terrain::Id::gravestone, pos));

                const auto& entry = highscores[entry_idx];

                grave->set_inscription(
                        "RIP " +
                        entry.name +
                        ", " +
                        player_bon::bg_title(entry.bg) +
                        ", " +
                        entry.date +
                        ", " +
                        "Score: " +
                        std::to_string(entry.calculate_score()));

                map::set_terrain(grave);

                ++entry_idx;

                if (entry_idx == highscores.size()) {
                        break;
                }
        }
}

static void put_intro_forest_statues(
        const std::vector<P>& available_positions,
        const std::vector<HighscoreEntry>& highscores)
{
        size_t entry_idx = 0;

        // NOTE: Here we assume the following:
        // * The positions are sorted from left to right
        // * The highscore entries are sorted from highest to lowest
        for (auto it = std::rbegin(available_positions);
             it != std::rend(available_positions);
             ++it) {
                const P& pos = *it;

                auto* const statue =
                        static_cast<terrain::Statue*>(
                                terrain::make(terrain::Id::statue, pos));

                const auto& entry = highscores[entry_idx];

                statue->set_player_bg(entry.bg);
                statue->set_type(terrain::StatueType::common);

                statue->set_inscription(
                        entry.name +
                        ", " +
                        player_bon::bg_title(entry.bg) +
                        ", " +
                        entry.date +
                        ", " +
                        "Score: " +
                        std::to_string(entry.calculate_score()));

                map::set_terrain(statue);

                ++entry_idx;

                if (entry_idx == highscores.size()) {
                        break;
                }
        }
}

// -----------------------------------------------------------------------------
// MapBuilderDeepOneLair
// -----------------------------------------------------------------------------
MapBuilderDeepOneLair::MapBuilderDeepOneLair() :

        m_passage_symbol((char)((int)'1' + rnd::range(0, 1)))
{
}

void MapBuilderDeepOneLair::handle_template_pos(const P& p, const char c)
{
        switch (c) {
        case '@':
        case '.':
        case 'd':
        case '%':  // TODO: Just put random blood/gore on the level instead?
        case 'B': {
                auto* const floor =
                        static_cast<terrain::Floor*>(
                                terrain::make(terrain::Id::floor, p));

                floor->m_type = terrain::FloorType::cave;

                map::set_terrain(floor);

                if (c == '@') {
                        map::g_player->m_pos = p;
                }
                else if (c == 'd') {
                        actor::make("MON_DEEP_ONE", p);
                }
                else if (c == 'B') {
                        actor::make("MON_NIDUZA", p);
                }
                else if (c == '%') {
                        terrain::make_blood(p);
                        terrain::make_gore(p);
                }
        } break;

        case '&':  // TODO: Just put random bones on the level instead?
        {
                map::set_terrain(terrain::make(terrain::Id::bones, p));
        } break;

        case '#':
        case '1':
        case '2': {
                if (c == m_passage_symbol) {
                        map::set_terrain(terrain::make(terrain::Id::floor, p));
                }
                else {
                        auto* const wall =
                                static_cast<terrain::Wall*>(
                                        terrain::make(terrain::Id::wall, p));

                        wall->m_type = terrain::WallType::cave;

                        map::set_terrain(wall);
                }
        } break;

        case '~': {
                auto* const liquid =
                        static_cast<terrain::Liquid*>(
                                terrain::make(terrain::Id::liquid, p));

                liquid->m_type = LiquidType::water;

                map::set_terrain(liquid);
        } break;

        case 'x': {
                auto* const door =
                        static_cast<terrain::Door*>(
                                terrain::make(terrain::Id::door, p));

                door->init_type_and_state(
                        terrain::DoorType::gate,
                        terrain::DoorSpawnState::closed);

                map::set_terrain(door);
        } break;

        case '>': {
                map::set_terrain(terrain::make(terrain::Id::stairs, p));
        } break;

        case '|': {
                map::set_terrain(terrain::make(terrain::Id::monolith, p));
        } break;

        case '-': {
                map::set_terrain(terrain::make(terrain::Id::altar, p));
        } break;

        case '0': {
                map::set_terrain(terrain::make(terrain::Id::gong, p));
        } break;

        case ':': {
                map::set_terrain(terrain::make(terrain::Id::stalagmite, p));
        } break;

        default:
        {
                ASSERT(false);
        } break;
        }
}

void MapBuilderDeepOneLair::on_template_built()
{
        populate_items::make_items_on_floor();
}

// -----------------------------------------------------------------------------
// MapBuilderMagicPool
// -----------------------------------------------------------------------------
MapBuilderMagicPool::MapBuilderMagicPool()

        = default;

void MapBuilderMagicPool::handle_template_pos(const P& p, const char c)
{
        switch (c) {
        case '@':
        case '.': {
                auto* const floor =
                        static_cast<terrain::Floor*>(
                                terrain::make(terrain::Id::floor, p));

                floor->m_type = terrain::FloorType::cave;

                map::set_terrain(floor);

                if (c == '@') {
                        map::g_player->m_pos = p;
                }
        } break;

        case '#': {
                auto* const wall =
                        static_cast<terrain::Wall*>(
                                terrain::make(terrain::Id::wall, p));

                wall->m_type = terrain::WallType::cave;

                map::set_terrain(wall);
        } break;

        case 't': {
                map::set_terrain(terrain::make(terrain::Id::tree, p));
        } break;

        case '~': {
                auto* const liquid =
                        static_cast<terrain::Liquid*>(
                                terrain::make(terrain::Id::liquid, p));

                liquid->m_type = LiquidType::magic_water;

                map::set_terrain(liquid);
        } break;

        case '>': {
                map::set_terrain(terrain::make(terrain::Id::stairs, p));
        } break;

        case '^': {
                map::set_terrain(terrain::make(terrain::Id::stalagmite, p));
        } break;

        default:
        {
                ASSERT(false);
        } break;
        }
}

void MapBuilderMagicPool::on_template_built()
{
        populate_items::make_items_on_floor();

        const std::vector<RoomType> mon_room_types = {
                RoomType::cave,
                RoomType::forest};

        populate_mon::populate_lvl_as_room_types(mon_room_types);
}

// -----------------------------------------------------------------------------
// MapBuilderIntroForest
// -----------------------------------------------------------------------------
void MapBuilderIntroForest::handle_template_pos(const P& p, const char c)
{
        switch (c) {
        case '@':
        case '=': {
                auto* const floor =
                        static_cast<terrain::Floor*>(
                                terrain::make(terrain::Id::floor, p));

                floor->m_type = terrain::FloorType::stone_path;

                map::set_terrain(floor);

                if (c == '@') {
                        map::g_player->m_pos = p;
                }
        } break;

        case '_': {
                auto* const grass =
                        static_cast<terrain::Grass*>(
                                terrain::make(terrain::Id::grass, p));

                grass->m_type = terrain::GrassType::withered;

                map::set_terrain(grass);
        } break;

        case 'W': {
                // Store this position for placing player statues
                m_possible_statue_positions.push_back(p);
        }
        // fallthrough
        case '.': {
                if (rnd::one_in(6)) {
                        if (rnd::one_in(6)) {
                                map::set_terrain(terrain::make(terrain::Id::bush, p));
                        }
                        else {
                                map::set_terrain(terrain::make(terrain::Id::grass, p));
                        }
                }
                else {
                        // Normal stone floor
                        map::set_terrain(terrain::make(terrain::Id::floor, p));
                }
        } break;

        case '#': {
                bool is_door_adj = false;

                for (const P& d : dir_utils::g_dir_list) {
                        const char adj_c = get_template().at(p + d);

                        if (adj_c == '+') {
                                is_door_adj = true;
                                break;
                        }
                }

                terrain::Terrain* terrain = nullptr;

                if (!is_door_adj) {
                        if (rnd::one_in(16)) {
                                map::set_terrain(
                                        terrain::make(
                                                terrain::Id::rubble_low, p));
                        }
                        else if (rnd::one_in(4)) {
                                map::set_terrain(
                                        terrain::make(
                                                terrain::Id::rubble_high, p));
                        }
                }

                if (!terrain) {
                        auto* const wall =
                                static_cast<terrain::Wall*>(
                                        terrain::make(terrain::Id::wall, p));

                        if (rnd::one_in(20)) {
                                wall->set_moss_grown();
                        }

                        map::set_terrain(wall);
                }
        } break;

        case '&': {
                // Store this position for placing player graves
                m_possible_grave_positions.push_back(p);
        }
        // fallthrough
        case ',': {
                if (rnd::one_in(12)) {
                        map::set_terrain(
                                terrain::make(terrain::Id::bush, p));
                }
                else {
                        map::set_terrain(
                                terrain::make(terrain::Id::grass, p));
                }
        } break;

        case '~': {
                auto* const liquid =
                        static_cast<terrain::Liquid*>(
                                terrain::make(terrain::Id::liquid, p));

                liquid->m_type = LiquidType::water;

                map::set_terrain(liquid);
        } break;

        case 't': {
                map::set_terrain(terrain::make(terrain::Id::tree, p));
        } break;

        case 'v': {
                map::set_terrain(terrain::make(terrain::Id::brazier, p));
        } break;

        case '[': {
                map::set_terrain(terrain::make(terrain::Id::church_bench, p));
        } break;

        case '-': {
                map::set_terrain(terrain::make(terrain::Id::altar, p));
        } break;

        case '*': {
                map::set_terrain(terrain::make(terrain::Id::carpet, p));
        } break;

        case '>': {
                map::set_terrain(terrain::make(terrain::Id::stairs, p));
        } break;

        case '+': {
                auto* const door =
                        static_cast<terrain::Door*>(
                                terrain::make(terrain::Id::door, p));

                door->set_mimic_terrain(terrain::make(terrain::Id::wall, p));

                door->init_type_and_state(
                        terrain::DoorType::wood,
                        terrain::DoorSpawnState::closed);

                map::set_terrain(door);
        } break;

        case '"': {
                map::set_terrain(terrain::make(terrain::Id::bones, p));
        } break;

        default:
        {
                ASSERT(false);
        } break;
        }
}

void MapBuilderIntroForest::on_template_built()
{
        const auto highscores = highscore::entries_sorted();

        std::vector<HighscoreEntry> highscores_lose;
        std::vector<HighscoreEntry> highscores_win;

        highscores_lose.reserve(highscores.size());
        highscores_win.reserve(highscores.size());

        for (const auto& e : highscores) {
                if (e.is_win == IsWin::no) {
                        highscores_lose.push_back(e);
                }
                else {
                        highscores_win.push_back(e);
                }
        }

        if (!highscores_lose.empty()) {
                put_intro_forest_graves(
                        m_possible_grave_positions,
                        highscores_lose);
        }

        if (!highscores_win.empty()) {
                put_intro_forest_statues(
                        m_possible_statue_positions,
                        highscores_win);
        }
}

// -----------------------------------------------------------------------------
// MapBuilderMiGoOutpost
// -----------------------------------------------------------------------------
void MapBuilderMiGoOutpost::handle_template_pos(const P& p, const char c)
{
        switch (c) {
        case '@':
        case ',': {
                auto* const floor =
                        static_cast<terrain::Floor*>(
                                terrain::make(terrain::Id::floor, p));

                floor->m_type = terrain::FloorType::cave;

                map::set_terrain(floor);

                if (c == '@') {
                        map::g_player->m_pos = p;
                }
        } break;

        case '.':
        case '1': {
                auto* const floor =
                        static_cast<terrain::Floor*>(
                                terrain::make(terrain::Id::floor, p));

                floor->m_type = terrain::FloorType::common;

                map::set_terrain(floor);

                if (c == '1') {
                        actor::make("MON_MI_GO_OMEGA_SENTINEL", p);
                }
                else {
                        m_possible_mon_positions.push_back(p);
                }

        } break;

        case '#': {
                auto* const wall =
                        static_cast<terrain::Wall*>(
                                terrain::make(terrain::Id::wall, p));

                wall->m_type = terrain::WallType::cave;

                map::set_terrain(wall);
        } break;

        case 'x': {
                auto* const wall =
                        static_cast<terrain::Wall*>(
                                terrain::make(terrain::Id::wall, p));

                wall->m_type = terrain::WallType::mi_go;

                map::set_terrain(wall);
        } break;

        case '>': {
                map::set_terrain(terrain::make(terrain::Id::stairs, p));
        } break;

        case ':': {
                map::set_terrain(terrain::make(terrain::Id::stalagmite, p));
        } break;

        default:
        {
                ASSERT(false);
        } break;
        }
}

void MapBuilderMiGoOutpost::on_template_built()
{
        rnd::shuffle(m_possible_mon_positions);

        size_t pos_idx = 0U;

        const size_t nr_bio_engineer = 2U;
        const size_t nr_commander = 2U;

        for (size_t i = 0; i < nr_bio_engineer; ++i) {
                actor::make("MON_MI_GO_BIO_ENGINEER", m_possible_mon_positions[pos_idx]);

                ++pos_idx;
        }

        for (size_t i = 0; i < nr_commander; ++i) {
                actor::make("MON_MI_GO_COMMANDER", m_possible_mon_positions[pos_idx]);

                ++pos_idx;
        }

        populate_items::make_items_on_floor();
}

// -----------------------------------------------------------------------------
// MapBuilderEgypt
// -----------------------------------------------------------------------------
MapBuilderEgypt::MapBuilderEgypt() :

        m_stair_symbol((char)((int)'1' + rnd::range(0, 3)))
{
}

void MapBuilderEgypt::handle_template_pos(const P& p, const char c)
{
        switch (c) {
        case '.':
        case '@':
        case 'P':
        case 'M':
        case 'C':
        case '1':
        case '2':
        case '3':
        case '4': {
                if (c == '@') {
                        map::g_player->m_pos = p;
                }

                if (c == m_stair_symbol) {
                        map::set_terrain(terrain::make(terrain::Id::stairs, p));
                }
                else {
                        map::set_terrain(terrain::make(terrain::Id::floor, p));
                }

                std::string actor_id;

                switch (c) {
                case 'P':
                        actor_id = "MON_KHEPHREN";
                        break;

                case 'M':
                        actor_id = "MON_MUMMY";
                        break;

                case 'C':
                        actor_id = "MON_CROC_HEAD_MUMMY";
                        break;

                default:
                        break;
                }

                if (!actor_id.empty()) {
                        actor::Actor* const actor = actor::make(actor_id, p);

                        actor->m_ai_state.is_roaming_allowed =
                                MonRoamingAllowed::no;
                }
        } break;

        case '#': {
                auto* const wall =
                        static_cast<terrain::Wall*>(
                                terrain::make(terrain::Id::wall, p));

                wall->m_type = terrain::WallType::egypt;

                map::set_terrain(wall);
        } break;

        case 'v': {
                map::set_terrain(terrain::make(terrain::Id::brazier, p));
        } break;

        case 'S': {
                map::set_terrain(terrain::make(terrain::Id::statue, p));
        } break;

        case '+': {
                auto* const door =
                        static_cast<terrain::Door*>(
                                terrain::make(terrain::Id::door, p));

                door->set_mimic_terrain(terrain::make(terrain::Id::wall, p));

                door->init_type_and_state(
                        terrain::DoorType::wood,
                        terrain::DoorSpawnState::closed);

                map::set_terrain(door);
        } break;

        case '~': {
                auto* const liquid =
                        static_cast<terrain::Liquid*>(
                                terrain::make(terrain::Id::liquid, p));

                liquid->m_type = LiquidType::water;

                map::set_terrain(liquid);
        } break;

        default:
        {
                ASSERT(false);
        } break;
        }
}

void MapBuilderEgypt::on_template_built()
{
        populate_items::make_items_on_floor();
}

// -----------------------------------------------------------------------------
// MapBuilderRatCave
// -----------------------------------------------------------------------------
void MapBuilderRatCave::handle_template_pos(const P& p, const char c)
{
        switch (c) {
        case '@':
        case '.':
        case ',':
        case '&':
        case 'r':
        case '1': {
                if ((c == '&') ||
                    ((c == ',' || c == 'r') && rnd::coin_toss())) {
                        map::set_terrain(terrain::make(terrain::Id::bones, p));
                }
                else {
                        auto* const floor =
                                static_cast<terrain::Floor*>(
                                        terrain::make(terrain::Id::floor, p));

                        floor->m_type = terrain::FloorType::cave;

                        map::set_terrain(floor);
                }

                if (c == '@') {
                        map::g_player->m_pos = p;
                }
                else if (c == '1') {
                        // TODO: Should be handled by map controller instead
                        game_time::add_mob(
                                terrain::make(
                                        terrain::Id::event_rat_cave_discovery,
                                        p));
                }
                else if (c == 'r') {
                        actor::Actor* actor = nullptr;

                        if (rnd::one_in(6)) {
                                actor = actor::make("MON_RAT_THING", p);
                        }
                        else {
                                actor = actor::make("MON_RAT", p);
                        }

                        prop::Prop* prop = prop::make(prop::Id::frenzied);

                        prop->set_indefinite();

                        actor->m_properties.apply(
                                prop,
                                prop::PropSrc::intr,
                                false,
                                Verbose::no);
                }
        } break;

        case '#': {
                auto* const wall =
                        static_cast<terrain::Wall*>(
                                terrain::make(terrain::Id::wall, p));

                wall->m_type = terrain::WallType::cave;

                map::set_terrain(wall);
        } break;

        case 'x': {
                if (rnd::one_in(3)) {
                        map::set_terrain(
                                terrain::make(terrain::Id::rubble_low, p));
                }
                else if (rnd::one_in(5)) {
                        map::set_terrain(
                                terrain::make(terrain::Id::rubble_high, p));
                }
                else {
                        auto* const wall =
                                static_cast<terrain::Wall*>(
                                        terrain::make(terrain::Id::wall, p));

                        wall->m_type = terrain::WallType::common;

                        map::set_terrain(wall);
                }
        } break;

        case '>': {
                map::set_terrain(terrain::make(terrain::Id::stairs, p));
        } break;

        case '|': {
                map::set_terrain(terrain::make(terrain::Id::monolith, p));
        } break;

        case ':': {
                map::set_terrain(terrain::make(terrain::Id::stalagmite, p));
        } break;

        default:
        {
                ASSERT(false);
        } break;
        }
}

void MapBuilderRatCave::on_template_built()
{
        // Set all actors to non-roaming (they will be set to roaming later)
        for (actor::Actor* const actor : game_time::g_actors) {
                if (actor::is_player(actor)) {
                        continue;
                }

                actor->m_ai_state.is_roaming_allowed = MonRoamingAllowed::no;
        }

        populate_items::make_items_on_floor();
}

// -----------------------------------------------------------------------------
// MapBuilderBoss
// -----------------------------------------------------------------------------
void MapBuilderBoss::handle_template_pos(const P& p, const char c)
{
        switch (c) {
        case '@':
        case 'P':
        case 'W':
        case 'R':
        case 'G':
        case '.': {
                map::set_terrain(terrain::make(terrain::Id::floor, p));

                if (c == '@') {
                        map::g_player->m_pos = p;
                }
                else if (c == 'P') {
                        actor::make("MON_THE_HIGH_PRIEST", p);
                }
        } break;

        case '#': {
                auto* const wall =
                        static_cast<terrain::Wall*>(
                                terrain::make(terrain::Id::wall, p));

                wall->m_type = terrain::WallType::egypt;

                map::set_terrain(wall);
        } break;

        case 'v': {
                map::set_terrain(terrain::make(terrain::Id::brazier, p));
        } break;

        case '>': {
                map::set_terrain(terrain::make(terrain::Id::stairs, p));
        } break;

        default:
        {
                ASSERT(false);
        } break;
        }
}

void MapBuilderBoss::on_template_built()
{
        // Spawn guardians.

        Array2<bool> blocked(map::dims());

        map_parsers::BlocksWalking(ParseActors::yes).run(blocked, blocked.rect());

        std::vector<P> free_positions = to_vec(blocked, false, blocked.rect());

        // Remove any positions to the left of the middle of the map.
        free_positions.erase(
                std::remove_if(
                        std::begin(free_positions),
                        std::end(free_positions),
                        [](const P& pos) {
                                return pos.x < (map::w() / 2);
                        }),
                std::end(free_positions));

        rnd::shuffle(free_positions);

        const std::vector<std::string> guardian_ids = {
                "MON_HIGH_PRIEST_GUARD_WAR_VET",
                "MON_HIGH_PRIEST_GUARD_ROGUE",
                "MON_HIGH_PRIEST_GUARD_GHOUL",
        };

        for (size_t i = 0; i < guardian_ids.size(); ++i) {
                if (i == free_positions.size()) {
                        ASSERT(false);

                        break;
                }

                const std::string& id = guardian_ids[i];
                const P pos = free_positions[i];

                actor::make(id, pos);
        }

        // Make the High Priest leader of all other monsters.
        actor::Actor* high_priest = nullptr;

        for (actor::Actor* const actor : game_time::g_actors) {
                if (actor->id() == "MON_THE_HIGH_PRIEST") {
                        high_priest = actor;

                        break;
                }
        }

        for (actor::Actor* const actor : game_time::g_actors) {
                if (!actor::is_player(actor) && (actor != high_priest)) {
                        actor->m_leader = high_priest;
                }
        }
}

std::unique_ptr<MapController> MapBuilderBoss::map_controller() const
{
        return std::make_unique<MapControllerBoss>();
}

// -----------------------------------------------------------------------------
// MapBuilderTrapez
// -----------------------------------------------------------------------------
void MapBuilderTrapez::handle_template_pos(const P& p, const char c)
{
        map::g_dark.at(p) = true;

        switch (c) {
        case '@':
        case '.':
        case 'o': {
                map::set_terrain(terrain::make(terrain::Id::floor, p));

                if (c == '@') {
                        map::g_player->m_pos = p;
                }
                else if (c == 'o') {
                        item::make_item_on_floor(item::Id::trapezohedron, p);
                }
        } break;

        case '#': {
                auto* const wall =
                        static_cast<terrain::Wall*>(
                                terrain::make(terrain::Id::wall, p));

                wall->m_type = terrain::WallType::egypt;

                map::set_terrain(wall);
        } break;

        case 'v': {
                map::set_terrain(terrain::make(terrain::Id::brazier, p));
        } break;

        default:
        {
                ASSERT(false);
        } break;
        }
}

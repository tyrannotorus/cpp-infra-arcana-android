// =============================================================================
// Copyright 2011-2023 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "terrain.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <ostream>
#include <string>

#include "actor.hpp"
#include "actor_data.hpp"
#include "actor_factory.hpp"
#include "actor_hit.hpp"
#include "actor_see.hpp"
#include "array2.hpp"
#include "audio.hpp"
#include "audio_data.hpp"
#include "bash.hpp"
#include "colors.hpp"
#include "common_text.hpp"
#include "debug.hpp"
#include "drop.hpp"
#include "explosion.hpp"
#include "game.hpp"
#include "game_time.hpp"
#include "gfx.hpp"
#include "hints.hpp"
#include "insanity.hpp"
#include "inventory.hpp"
#include "io.hpp"
#include "item.hpp"
#include "item_curse.hpp"
#include "item_data.hpp"
#include "item_factory.hpp"
#include "item_weapon.hpp"
#include "map.hpp"
#include "map_parsing.hpp"
#include "map_travel.hpp"
#include "msg_log.hpp"
#include "pickup.hpp"
#include "player_bon.hpp"
#include "popup.hpp"
#include "property.hpp"
#include "property_data.hpp"
#include "property_factory.hpp"
#include "property_handler.hpp"
#include "query.hpp"
#include "random.hpp"
#include "saving.hpp"
#include "sound.hpp"
#include "state.hpp"
#include "terrain_dmg.hpp"
#include "terrain_door.hpp"
#include "terrain_factory.hpp"
#include "terrain_mob.hpp"
#include "text_format.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
struct MaterialBurnData
{
        int finish_burning_one_in_n {1};
        int hit_adjacent_one_in_n {1};
};

static MaterialBurnData get_material_burn_data(const Material material)
{
        MaterialBurnData d;

        switch (material) {
        case Material::fluid:
        case Material::empty:
                d.finish_burning_one_in_n = 1;
                d.hit_adjacent_one_in_n = 1;
                break;

        case Material::stone:
                d.finish_burning_one_in_n = 14;
                d.hit_adjacent_one_in_n = 10;
                break;

        case Material::metal:
                d.finish_burning_one_in_n = 14;
                d.hit_adjacent_one_in_n = 10;
                break;

        case Material::plant:
                d.finish_burning_one_in_n = 25;
                d.hit_adjacent_one_in_n = 9;
                break;

        case Material::wood:
                d.finish_burning_one_in_n = 25;
                d.hit_adjacent_one_in_n = 2;
                break;

        case Material::cloth:
                d.finish_burning_one_in_n = 20;
                d.hit_adjacent_one_in_n = 8;
                break;
        }

        return d;
}

static void scorch_actor(actor::Actor& actor)
{
        if (actor::is_player(&actor)) {
                msg_log::add(
                        "I am scorched by flames.",
                        colors::msg_bad());
        }
        else if (actor::can_player_see_actor(actor)) {
                const std::string name_the =
                        text_format::first_to_upper(
                                actor.name_the());

                msg_log::add(
                        name_the +
                                " is scorched by flames.",
                        colors::msg_good());
        }

        actor::hit(actor, 1, DmgType::fire, nullptr);
}

static void spread_burning(const terrain::Terrain& terrain)
{
        // Center position not allowed.
        const P p(dir_utils::rnd_adj_pos(terrain.pos(), false));

        if (!map::is_pos_inside_outer_walls(p)) {
                return;
        }

        auto* const other_terrain = map::g_terrain.at(p);

        other_terrain->hit(DmgType::fire, nullptr);

        if (other_terrain->is_burning()) {
                other_terrain->m_started_burning_this_turn = true;

                if (map::g_player->m_pos == p) {
                        msg_log::add(
                                "Fire has spread here!",
                                colors::msg_note(),
                                MsgInterruptPlayer::yes,
                                MorePromptOnMsg::yes);
                }
        }
}

static WasDestroyed on_new_turn_terrain_burning(terrain::Terrain& terrain)
{
        terrain.clear_gore();

        // TODO: Hit dead actors

        // Hit actor standing on terrain
        auto* actor = map::living_actor_at(terrain.pos());

        if (actor) {
                // Occasionally try to set actor on fire, otherwise just do
                // small fire damage.
                if (rnd::one_in(4)) {
                        actor->m_properties.apply(prop::make(prop::Id::burning));
                }
                else {
                        scorch_actor(*actor);
                }
        }

        const auto material_burn_data = get_material_burn_data(terrain.material());

        if (rnd::one_in(material_burn_data.finish_burning_one_in_n)) {
                terrain.m_burn_state = terrain::BurnState::has_burned;

                const auto was_destroyed = terrain.on_finished_burning();

                if (was_destroyed == WasDestroyed::yes) {
                        return was_destroyed;
                }
        }

        if (rnd::one_in(material_burn_data.hit_adjacent_one_in_n)) {
                spread_burning(terrain);
        }

        // Create smoke?
        if (rnd::one_in(20)) {
                const auto p = dir_utils::rnd_adj_pos(terrain.pos(), true);

                if (map::is_pos_inside_outer_walls(p) &&
                    !map_parsers::BlocksProjectiles().run(p)) {
                        auto* const smoke =
                                static_cast<terrain::Smoke*>(
                                        terrain::make(terrain::Id::smoke, p));

                        smoke->set_nr_turns(10);

                        game_time::add_mob(smoke);
                }
        }

        return WasDestroyed::no;
}

// -----------------------------------------------------------------------------
// terrain
// -----------------------------------------------------------------------------
namespace terrain
{
void make_blood(const P& origin)
{
        for (const auto& d : dir_utils::g_dir_list_w_center) {
                if (!rnd::one_in(3)) {
                        continue;
                }

                const auto p = origin + d;

                map::g_terrain.at(p)->try_make_bloody();
        }
}

void make_gore(const P& origin)
{
        for (int dx = -1; dx <= 1; ++dx) {
                for (int dy = -1; dy <= 1; ++dy) {
                        const auto c = origin + P(dx, dy);

                        if (rnd::one_in(3)) {
                                map::g_terrain.at(c)->try_put_gore();
                        }
                }
        }
}

void Terrain::bump(actor::Actor& actor_bumping)
{
        if (!can_move(actor_bumping) &&
            actor::is_player(&actor_bumping)) {
                if (map::g_seen.at(m_pos)) {
                        msg_log::add(m_data->msg_on_player_blocked);
                }
                else {
                        msg_log::add(m_data->msg_on_player_blocked_blind);
                }
        }
}

AllowAction Terrain::pre_bump(actor::Actor& actor_bumping)
{
        if (!actor::is_player(&actor_bumping) ||
            actor_bumping.m_properties.has(prop::Id::confused)) {
                return AllowAction::yes;
        }

        const prop::PropHandler& props = actor_bumping.m_properties;

        if ((m_burn_state == BurnState::burning) &&
            !props.has(prop::Id::r_fire) &&
            !props.has(prop::Id::flying) &&
            !props.has(prop::Id::tiny_flying) &&
            can_move(actor_bumping) &&
            map::g_seen.at(m_pos)) {
                const std::string msg =
                        "Step into the flames? " +
                        common_text::g_yes_or_no_hint;

                msg_log::add(
                        msg,
                        colors::light_white(),
                        MsgInterruptPlayer::no,
                        MorePromptOnMsg::no,
                        CopyToMsgHistory::no);

                const auto query_result = query::yes_or_no();

                msg_log::clear();

                const bool allowed = query_result == BinaryAnswer::yes;

                return allowed ? AllowAction::yes : AllowAction::no;
        }

        return AllowAction::yes;
}

void Terrain::on_new_turn()
{
        if (m_nr_turns_color_corrupted > 0) {
                --m_nr_turns_color_corrupted;
        }

        if ((m_burn_state == BurnState::burning) &&
            !m_started_burning_this_turn) {
                const auto was_destroyed = on_new_turn_terrain_burning(*this);

                if (was_destroyed == WasDestroyed::yes) {
                        return;
                }
        }

        // Run specialized new turn actions
        on_new_turn_hook();
}

void Terrain::try_start_burning(const Verbose verbose)
{
        clear_gore();

        const bool is_not_burned = m_burn_state == BurnState::not_burned;
        const bool has_burnt = m_burn_state == BurnState::has_burned;

        if (is_not_burned || (has_burnt && rnd::one_in(3))) {
                if (map::g_seen.at(m_pos) &&
                    (verbose == Verbose::yes)) {
                        std::string str = name(Article::the) + " catches fire.";

                        str[0] = (char)std::toupper(str[0]);

                        msg_log::add(str);
                }

                m_burn_state = BurnState::burning;

                m_started_burning_this_turn = true;
        }
}  // namespace terrain

WasDestroyed Terrain::on_finished_burning()
{
        return WasDestroyed::no;
}

int Terrain::shock_when_adj() const
{
        int shock = base_shock_when_adj();

        if (is_corrupted_color()) {
                shock += 6;
        }

        if (has_gore()) {
                shock +=
                        player_bon::is_bg(Bg::ghoul)
                        ? 2
                        : 3;
        }

        return shock;
}

int Terrain::base_shock_when_adj() const
{
        return m_data->shock_when_adjacent;
}

void Terrain::try_make_bloody()
{
        if (can_have_blood()) {
                m_is_bloody = true;
        }
}

bool Terrain::is_bloody() const
{
        return m_is_bloody;
}

void Terrain::try_put_gore()
{
        if (!can_have_gore()) {
                return;
        }

        const int roll_character = rnd::range(1, 2);

        switch (roll_character) {
        case 1:
                m_gore_character = '\'';
                break;

        case 2:
                m_gore_character = '`';
                break;

        default:
                ASSERT(false);
                break;
        }

        const int roll_tile = rnd::range(1, 8);

        switch (roll_tile) {
        case 1:
                m_gore_tile = gfx::TileId::gore1;
                break;

        case 2:
                m_gore_tile = gfx::TileId::gore2;
                break;

        case 3:
                m_gore_tile = gfx::TileId::gore3;
                break;

        case 4:
                m_gore_tile = gfx::TileId::gore4;
                break;

        case 5:
                m_gore_tile = gfx::TileId::gore5;
                break;

        case 6:
                m_gore_tile = gfx::TileId::gore6;
                break;

        case 7:
                m_gore_tile = gfx::TileId::gore7;
                break;

        case 8:
                m_gore_tile = gfx::TileId::gore8;
                break;

        default:
                ASSERT(false);
                break;
        }
}

bool Terrain::has_gore() const
{
        // TODO: This is hacky
        return (m_gore_tile != gfx::TileId::END);
}

void Terrain::try_corrupt_color()
{
        if (id() == terrain::Id::chasm) {
                return;
        }

        m_nr_turns_color_corrupted = rnd::range(200, 220);
}

bool Terrain::is_corrupted_color() const
{
        return (m_nr_turns_color_corrupted > 0);
}

void Terrain::cycle_graphics(const io::GraphicsCycle cycle)
{
        if (cycle == io::GraphicsCycle::fast) {
                m_burn_color_bg.set_rgb(
                        (uint8_t)rnd::range(80, 255),
                        0,
                        0);

                m_corrupt_color.set_rgb(
                        rnd::range(40, 255),
                        rnd::range(40, 255),
                        rnd::range(40, 255));
        }
}

Color Terrain::color() const
{
        if (m_burn_state == BurnState::burning) {
                return colors::orange();
        }
        else {
                // Not burning
                if (is_corrupted_color()) {
                        return m_corrupt_color;
                }
                else if (m_is_bloody) {
                        return colors::light_red();
                }
                else {
                        if (m_burn_state == BurnState::not_burned) {
                                return color_default();
                        }
                        else {
                                return colors::dark_gray();
                        }
                }
        }
}

Color Terrain::color_bg() const
{
        switch (m_burn_state) {
        case BurnState::not_burned:
        case BurnState::has_burned:
                return colors::black();

        case BurnState::burning:
                return m_burn_color_bg;
        }

        ASSERT(false && "Failed to set color");

        return colors::yellow();
}

void Terrain::clear_gore()
{
        m_gore_tile = gfx::TileId::END;
        m_gore_character = ' ';
        m_is_bloody = false;
}

void Terrain::add_light(Array2<bool>& light) const
{
        if (m_burn_state == BurnState::burning) {
                for (const P& d : dir_utils::g_dir_list_w_center) {
                        const P p(m_pos + d);

                        if (map::is_pos_inside_map(p)) {
                                light.at(p) = true;
                        }
                }
        }

        add_light_hook(light);
}

// -----------------------------------------------------------------------------
// Floor
// -----------------------------------------------------------------------------
Floor::Floor(const P& p, const TerrainData* const data) :
        Terrain(p, data),
        m_type(FloorType::common) {}

void Floor::hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const P& from_pos,
        int dmg)
{
        (void)actor;
        (void)from_pos;
        (void)dmg;

        switch (dmg_type) {
        case DmgType::fire:
                if (rnd::one_in(3)) {
                        try_start_burning(Verbose::no);
                }
                break;

        default:
                break;
        }
}

gfx::TileId Floor::tile() const
{
        if (m_burn_state == BurnState::has_burned) {
                return gfx::TileId::scorched_ground;
        }
        else {
                return m_data->tile;
        }
}

std::string Floor::name(const Article article) const
{
        std::string str = (article == Article::a) ? "" : "the ";

        if (m_burn_state == BurnState::burning) {
                str += "flames";
        }
        else {
                if (m_burn_state == BurnState::has_burned) {
                        str += "scorched ";
                }

                switch (m_type) {
                case FloorType::common:
                        str += "stone floor";
                        break;

                case FloorType::cave:
                        str += "cavern floor";
                        break;

                case FloorType::stone_path:
                        if (article == Article::a) {
                                str.insert(0, "a ");
                        }

                        str += "stone path";
                        break;
                }
        }

        return str;
}

Color Floor::color_default() const
{
        return colors::white();
}

// -----------------------------------------------------------------------------
// Wall
// -----------------------------------------------------------------------------
Wall::Wall(const P& p, const TerrainData* const data) :
        Terrain(p, data),
        m_type(WallType::common),
        m_is_mossy(false) {}

void Wall::hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const P& from_pos,
        int dmg)
{
        (void)actor;
        (void)from_pos;
        (void)dmg;

        switch (dmg_type) {
        case DmgType::pure:
        case DmgType::explosion: {
                destr_all_adj_doors(m_pos);

                if ((dmg_type == DmgType::pure) || rnd::coin_toss()) {
                        destr_stone_wall(m_pos);
                }
                else {
                        map::update_terrain(make(Id::rubble_high, m_pos));
                }

                map::update_vision();
        } break;

        default:
        {
        } break;
        }
}

gfx::TileId Wall::tile() const
{
        const P p_below = m_pos.with_y_offset(1);

        Id id_below = Id::END;

        if (p_below.y < map::h()) {
                const Terrain* const terrain_below = map::g_terrain.at(p_below);

                if (map::g_seen.at(p_below)) {
                        id_below = terrain_below->id();
                }
                else {
                        const map::PlayerMemoryTerrain& memory_below =
                                map::g_terrain_memory.at(p_below);

                        id_below = memory_below.id;
                }
        }

        switch (id_below) {
        case Id::wall:
        case Id::door:
        case Id::rubble_high:
                return top_wall_tile();

        default:
                return front_wall_tile();
        }
}

std::string Wall::name(const Article article) const
{
        std::string article_str;
        std::string name_str;

        switch (m_type) {
        case WallType::common:
        case WallType::common_alt:
        case WallType::leng_monestary:
        case WallType::egypt:
                article_str = "a";
                name_str = "stone wall";
                break;

        case WallType::mi_go:
                article_str = "an";
                name_str = "alien wall";
                break;

        case WallType::pillar:
                article_str = "a";
                name_str = "pillar";
                break;

        case WallType::pillar_broken:
                article_str = "a";
                name_str = "broken pillar";
                break;

        case WallType::cave:
                article_str = "a";
                name_str = "cavern wall";
                break;

        case WallType::cliff:
                article_str = "a";
                name_str = "cliff";
                break;
        }

        if (m_is_mossy) {
                article_str = "a";
                name_str = "moss-grown " + name_str;
        }

        if (article == Article::the) {
                article_str = "the";
        }

        return article_str + " " + name_str;
}

Color Wall::color_default() const
{
        if (m_is_mossy) {
                return colors::dark_green();
        }

        switch (m_type) {
        case WallType::cliff:
                return colors::dark_gray();

        case WallType::egypt:
        case WallType::cave:
                return colors::gray_brown();

        case WallType::mi_go:
                return colors::orange();

        case WallType::pillar:
        case WallType::pillar_broken:
                return colors::gray();

        case WallType::common:
        case WallType::common_alt:
                // Return the wall color of the current map
                return map::g_wall_color;

        case WallType::leng_monestary:
                return colors::red();
        }

        ASSERT(false && "Failed to set color");
        return colors::yellow();
}

gfx::TileId Wall::front_wall_tile() const
{
        switch (m_type) {
        case WallType::common:
                return gfx::TileId::wall_front;

        case WallType::common_alt:
                return gfx::TileId::wall_front_alt1;

        case WallType::pillar:
                return gfx::TileId::pillar;

        case WallType::pillar_broken:
                return gfx::TileId::pillar_broken;

        case WallType::cliff:
        case WallType::cave:
                return gfx::TileId::wall_cave_front;

        case WallType::leng_monestary:
        case WallType::egypt:
                return gfx::TileId::wall_egypt_front;

        case WallType::mi_go:
                return gfx::TileId::wall_mi_go_front;
        }

        ASSERT(false && "Failed to set front wall tile");
        return gfx::TileId::END;
}

gfx::TileId Wall::top_wall_tile() const
{
        switch (m_type) {
        case WallType::common:
        case WallType::common_alt:
                return gfx::TileId::wall_top;

        case WallType::pillar:
                return gfx::TileId::pillar;

        case WallType::pillar_broken:
                return gfx::TileId::pillar_broken;

        case WallType::cliff:
        case WallType::cave:
                return gfx::TileId::wall_cave_top;

        case WallType::leng_monestary:
        case WallType::egypt:
                return gfx::TileId::wall_egypt_top;

        case WallType::mi_go:
                return gfx::TileId::wall_mi_go_top;
        }

        ASSERT(false && "Failed to set top wall tile");
        return gfx::TileId::END;
}

void Wall::set_rnd_common_wall()
{
        m_type = (rnd::one_in(6)) ? WallType::common_alt : WallType::common;
}

void Wall::set_moss_grown()
{
        m_is_mossy = true;
}

// -----------------------------------------------------------------------------
// High rubble
// -----------------------------------------------------------------------------
RubbleHigh::RubbleHigh(const P& p, const TerrainData* const data) :
        Terrain(p, data) {}

void RubbleHigh::hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const P& from_pos,
        int dmg)
{
        (void)actor;
        (void)from_pos;
        (void)dmg;

        switch (dmg_type) {
        case DmgType::pure:
        case DmgType::explosion:
                destr_stone_wall(m_pos);
                map::update_vision();
                break;

        default:
                break;
        }
}

std::string RubbleHigh::name(const Article article) const
{
        std::string a = (article == Article::a) ? "a " : "the ";

        return a + "big pile of debris";
}

Color RubbleHigh::color_default() const
{
        // Return the wall color of the current map
        return map::g_wall_color;
}

// -----------------------------------------------------------------------------
// Low rubble
// -----------------------------------------------------------------------------
RubbleLow::RubbleLow(const P& p, const TerrainData* const data) :
        Terrain(p, data) {}

void RubbleLow::hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const P& from_pos,
        int dmg)
{
        (void)actor;
        (void)from_pos;
        (void)dmg;

        switch (dmg_type) {
        case DmgType::fire:
                try_start_burning(Verbose::no);
                break;

        default:
                break;
        }
}

std::string RubbleLow::name(const Article article) const
{
        std::string str;

        if (article == Article::the) {
                str += "the ";
        }

        if (m_burn_state == BurnState::burning) {
                str += "burning ";
        }

        return str + "rubble";
}

Color RubbleLow::color_default() const
{
        // Return the wall color of the current map
        return map::g_wall_color;
}

// -----------------------------------------------------------------------------
// Bones
// -----------------------------------------------------------------------------
Bones::Bones(const P& p, const TerrainData* const data) :
        Terrain(p, data) {}

void Bones::hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const P& from_pos,
        int dmg)
{
        (void)dmg_type;
        (void)actor;
        (void)from_pos;
        (void)dmg;
}

std::string Bones::name(const Article article) const
{
        std::string str;

        if (article == Article::the) {
                str += "the ";
        }

        return str + "bones";
}

Color Bones::color_default() const
{
        return colors::dark_gray();
}

// -----------------------------------------------------------------------------
// Grave
// -----------------------------------------------------------------------------
GraveStone::GraveStone(const P& p, const TerrainData* const data) :
        Terrain(p, data) {}

void GraveStone::hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const P& from_pos,
        int dmg)
{
        (void)dmg_type;
        (void)actor;
        (void)from_pos;
        (void)dmg;
}

void GraveStone::bump(actor::Actor& actor_bumping)
{
        if (actor::is_player(&actor_bumping)) {
                msg_log::add(m_inscr);
        }
}

std::string GraveStone::name(const Article article) const
{
        const std::string a = (article == Article::a) ? "a " : "the ";

        return a + "gravestone (\"" + m_inscr + "\")";
}

Color GraveStone::color_default() const
{
        return colors::white();
}

// -----------------------------------------------------------------------------
// Church bench
// -----------------------------------------------------------------------------
ChurchBench::ChurchBench(const P& p, const TerrainData* const data) :
        Terrain(p, data) {}

void ChurchBench::hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const P& from_pos,
        int dmg)
{
        (void)actor;
        (void)from_pos;
        (void)dmg;

        switch (dmg_type) {
        case DmgType::explosion:
        case DmgType::pure:
                if (map::g_seen.at(m_pos)) {
                        msg_log::add("The church bench is destroyed.");
                }

                map::update_terrain(make(Id::rubble_low, m_pos));
                map::update_vision();
                break;

        case DmgType::fire:
                try_start_burning(Verbose::yes);
                break;

        default:
                break;
        }
}

std::string ChurchBench::name(const Article article) const
{
        const std::string a = (article == Article::a) ? "a " : "the ";

        return a + "church bench";
}

Color ChurchBench::color_default() const
{
        return colors::brown();
}

// -----------------------------------------------------------------------------
// Statue
// -----------------------------------------------------------------------------
Statue::Statue(const P& p, const TerrainData* const data) :
        Terrain(p, data),
        m_type(rnd::one_in(8) ? StatueType::ghoul : StatueType::common),
        m_player_bg(Bg::END)
{
}

int Statue::base_shock_when_adj() const
{
        const bool is_ghoul_statue =
                (m_type == StatueType::ghoul) ||
                (m_player_bg == Bg::ghoul);

        if (is_ghoul_statue && !player_bon::is_bg(Bg::ghoul)) {
                return 10;
        }
        else {
                return 0;
        }
}

void Statue::topple(
        const Dir direction,
        actor::Actor* const actor_toppling)
{
        const auto alerts_mon =
                actor::is_player(actor_toppling)
                ? AlertsMon::yes
                : AlertsMon::no;

        if (map::g_seen.at(m_pos)) {
                msg_log::add("The statue topples over.");
        }

        Snd snd(
                "I hear a crash.",
                audio::SfxId::statue_crash,
                IgnoreMsgIfOriginSeen::yes,
                m_pos,
                actor_toppling,
                SndVol::low,
                alerts_mon);

        snd_emit::run(snd);

        const auto dst_pos = m_pos + dir_utils::offset(direction);

        map::update_terrain(make(Id::rubble_low, m_pos));

        // NOTE: This object is now deleted!

        actor::Actor* const actor_behind = map::living_actor_at(dst_pos);

        if (actor_behind &&
            actor_behind->is_alive() &&
            !actor_behind->m_properties.has(prop::Id::ethereal)) {
                if (actor::is_player(actor_behind)) {
                        msg_log::add("It falls on me!");
                }
                else {
                        // Monster is hit
                        const bool is_player_seeing_actor =
                                actor::can_player_see_actor(
                                        *actor_behind);

                        if (is_player_seeing_actor) {
                                msg_log::add(
                                        "It falls on " +
                                        actor_behind->name_a() +
                                        ".");
                        }
                }

                actor::hit(
                        *actor_behind,
                        rnd::range(3, 6),
                        DmgType::blunt,
                        actor_toppling);

                if (actor_behind->is_alive()) {
                        auto* const paralyzed =
                                prop::make(prop::Id::paralyzed);

                        paralyzed->set_duration(rnd::range(2, 3));

                        actor_behind->m_properties.apply(paralyzed);
                }
        }

        const auto terrain_id = map::g_terrain.at(dst_pos)->id();

        // NOTE: This is kinda hacky, but the rubble is mostly just for
        // decoration anyway, so it doesn't really matter.
        if (terrain_id == terrain::Id::floor ||
            terrain_id == terrain::Id::grass ||
            terrain_id == terrain::Id::carpet) {
                map::update_terrain(make(Id::rubble_low, dst_pos));
        }

        map::update_vision();
}

bool Statue::allow_player_melee_attack(
        const DmgType dmg_type,
        const item::Item& wpn) const
{
        (void)wpn;

        switch (dmg_type) {
        case DmgType::kicking:
                return true;
                break;

        default:
                break;
        }

        return false;
}

void Statue::hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const P& from_pos,
        int dmg)
{
        (void)dmg;

        switch (dmg_type) {
        case DmgType::kicking:
        case DmgType::control_object_spell: {
                ASSERT(actor);

                if ((dmg_type == DmgType::kicking) &&
                    actor->m_properties.has(prop::Id::weakened)) {
                        msg_log::add("It wiggles a bit.");

                        return;
                }

                const auto direction = dir_utils::dir(m_pos - from_pos);

                // NOTE: This call deletes the object!
                topple(direction, actor);
        } break;

        case DmgType::explosion:
        case DmgType::pure: {
                map::update_terrain(make(Id::rubble_low, m_pos));
                map::update_vision();
        } break;

        default:
        {
        } break;
        }
}

void Statue::bump(actor::Actor& actor_bumping)
{
        if (!m_inscr.empty() && actor::is_player(&actor_bumping)) {
                msg_log::add(m_inscr);
        }
        else {
                Terrain::bump(actor_bumping);
        }
}

std::string Statue::name(const Article article) const
{
        std::string str = (article == Article::a) ? "a " : "the ";

        switch (m_type) {
        case StatueType::common:
                str += "statue";
                break;

        case StatueType::ghoul:
                str += "statue of a ghoulish creature";
                break;
        }

        if (!m_inscr.empty()) {
                str += " (\"" + m_inscr + "\")";
        }

        return str;
}

gfx::TileId Statue::tile() const
{
        if (m_player_bg == Bg::ghoul) {
                return gfx::TileId::ghoul;
        }
        else if (m_player_bg != Bg::END) {
                return gfx::TileId::player_unarmed;
        }
        else if (m_type == StatueType::ghoul) {
                return gfx::TileId::ghoul;
        }
        else {
                return gfx::TileId::witch_or_warlock;
        }
}

Color Statue::color_default() const
{
        if (m_player_bg == Bg::END) {
                return colors::white();
        }
        else {
                // The statue will be drawn with a player tile - make it more
                // distinct from the player character
                return colors::gray();
        }
}

void Statue::set_player_bg(const Bg bg)
{
        m_player_bg = bg;
}

// -----------------------------------------------------------------------------
// Stalagmite
// -----------------------------------------------------------------------------
Stalagmite::Stalagmite(const P& p, const TerrainData* const data) :
        Terrain(p, data) {}

void Stalagmite::hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const P& from_pos,
        int dmg)
{
        (void)actor;
        (void)from_pos;
        (void)dmg;

        switch (dmg_type) {
        case DmgType::pure:
        case DmgType::explosion:
                map::update_terrain(make(Id::rubble_low, m_pos));
                map::update_vision();
                break;

        default:
                break;
        }
}

std::string Stalagmite::name(const Article article) const
{
        std::string a = (article == Article::a) ? "a " : "the ";

        return a + "stalagmite";
}

Color Stalagmite::color_default() const
{
        return colors::gray_brown();
}

// -----------------------------------------------------------------------------
// Stairs
// -----------------------------------------------------------------------------
Stairs::Stairs(const P& p, const TerrainData* const data) :
        Terrain(p, data) {}

void Stairs::hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const P& from_pos,
        int dmg)
{
        (void)dmg_type;
        (void)actor;
        (void)from_pos;
        (void)dmg;
}

void Stairs::on_new_turn_hook()
{
        ASSERT(!map::g_items.at(m_pos));
}

void Stairs::bump(actor::Actor& actor_bumping)
{
        if (!actor::is_player(&actor_bumping)) {
                return;
        }

        map::memorize_terrain_at(m_pos);

        int choice = 0;

        popup::Popup(popup::AddToMsgHistory::no)
                .set_title("A staircase leading downwards")
                .setup_menu_mode(
                        {"(D)escend",
                         "(S)ave and quit",
                         "(space, esc) Cancel"},
                        {'d', 's', 0},
                        &choice)
                .run();

        switch (choice) {
        case 0:
                map::g_player->m_pos = m_pos;

                if (is_fake()) {
                        // NOTE: This destroys this object
                        player_use_fake_stairs();

                        return;
                }

                msg_log::clear();

                msg_log::add("I descend the stairs.");

                // Always auto-save the game when descending
                //
                // NOTE: We descend one dlvl when loading the game, so
                // auto-saving should be done BEFORE descending here
                saving::save_game();

                map_travel::go_to_nxt();
                break;

        case 1:
                map::g_player->m_pos = m_pos;

                if (is_fake()) {
                        // NOTE: This destroys this object
                        player_use_fake_stairs();

                        return;
                }

                saving::save_game();

                states::pop();
                break;

        default:
                msg_log::clear();
                break;
        }
}

void Stairs::player_use_fake_stairs()
{
        const auto* const msg =
                "As I descend the stairs and observe my surroundings, to my "
                "great bewilderment I realize that I have stepped out into "
                "the very same ground from which I started my downward climb! "
                "Turning around, the stairs are nowhere to be found.";

        popup::Popup(popup::AddToMsgHistory::yes)
                .set_msg(msg)
                .run();

        map::update_terrain(make(Id::floor, m_pos));

        auto* prop = prop::make(prop::Id::confused);

        prop->set_duration(8);

        map::g_player->m_properties.apply(prop);

        map::g_player->incr_shock(2.0, ShockSrc::misc);
}

std::string Stairs::name(const Article article) const
{
        std::string a = (article == Article::a) ? "a " : "the ";

        return a + "downward staircase";
}

Color Stairs::color_default() const
{
        return colors::yellow();
}

void Stairs::add_light_hook(Array2<bool>& light) const
{
        light.at(m_pos) = true;
}

// -----------------------------------------------------------------------------
// Bridge
// -----------------------------------------------------------------------------
gfx::TileId Bridge::tile() const
{
        return (
                (m_axis == Axis::hor)
                        ? gfx::TileId::hangbridge_hor
                        : gfx::TileId::hangbridge_ver);
}

void Bridge::hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const P& from_pos,
        int dmg)
{
        (void)dmg_type;
        (void)actor;
        (void)from_pos;
        (void)dmg;
}

char Bridge::character() const
{
        return (m_axis == Axis::hor)
                ? '|'
                : '=';
}

std::string Bridge::name(const Article article) const
{
        std::string a = (article == Article::a) ? "a " : "the ";

        return a + "bridge";
}

Color Bridge::color_default() const
{
        return colors::dark_brown();
}

// -----------------------------------------------------------------------------
// Shallow liquid
// -----------------------------------------------------------------------------
Liquid::Liquid(const P& p, const TerrainData* const data) :
        Terrain(p, data),
        m_type(LiquidType::water) {}

void Liquid::hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const P& from_pos,
        int dmg)
{
        (void)dmg_type;
        (void)actor;
        (void)from_pos;
        (void)dmg;
}

void Liquid::bump(actor::Actor& actor_bumping)
{
        const prop::PropHandler& props = actor_bumping.m_properties;

        if (props.has(prop::Id::ethereal) ||
            props.has(prop::Id::flying) ||
            props.has(prop::Id::tiny_flying) ||
            actor_bumping.m_data->is_amphibian) {
                return;
        }

        if (!props.has(prop::Id::crimson_passage)) {
                actor_bumping.m_properties.apply(prop::make(prop::Id::delayed_by_liquid));

                // Print message if player, unless player is wearing torture
                // collar (in that case the message is redundant since the
                // player is delayed on every move they make anyway).
                if (actor::is_player(&actor_bumping) &&
                    !map::g_player->m_inv.has_item_in_slot(
                            SlotId::head,
                            item::Id::torture_collar)) {
                        std::string type_str;
                        std::string verb_str;

                        switch (m_type) {
                        case LiquidType::water:
                        case LiquidType::magic_water:
                                type_str = "water";
                                verb_str = "wade";
                                break;

                        case LiquidType::mud:
                                type_str = "mud";
                                verb_str = "trudge";
                                break;
                        }

                        msg_log::add(
                                "I " +
                                verb_str +
                                " slowly through the knee high " +
                                type_str +
                                ".");
                }

                // The creature might also get stuck.
                if ((m_type == LiquidType::mud) && rnd::one_in(5)) {
                        actor_bumping.m_properties.apply(prop::make(prop::Id::stuck));
                }
        }

        // Make a sound, unless player with Silent trait.
        if (!player_bon::has_trait(Trait::silent) ||
            !actor::is_player(&actor_bumping)) {
                const std::string msg =
                        actor::is_player(&actor_bumping)
                        ? ""
                        : "I hear a splash.";

                const auto alerts_mon =
                        actor::is_player(&actor_bumping)
                        ? AlertsMon::yes
                        : AlertsMon::no;

                Snd snd(
                        msg,
                        audio::SfxId::wade,
                        IgnoreMsgIfOriginSeen::yes,
                        actor_bumping.m_pos,
                        &actor_bumping,
                        SndVol::low,
                        alerts_mon);

                snd_emit::run(snd);
        }

        // "Magic pool" effects.
        if (actor::is_player(&actor_bumping) && (m_type == LiquidType::magic_water)) {
                run_magic_pool_effects_on_player();
        }
}

void Liquid::run_magic_pool_effects_on_player()
{
        std::vector<item::Item*> cursed_items;

        for (const auto& slot : map::g_player->m_inv.m_slots) {
                if (slot.item && slot.item->is_cursed()) {
                        cursed_items.push_back(slot.item);
                }
        }

        for (auto* const item : map::g_player->m_inv.m_backpack) {
                if (item->is_cursed()) {
                        cursed_items.push_back(item);
                }
        }

        map::g_player->m_properties.end_prop(prop::Id::cursed);
        map::g_player->m_properties.end_prop(prop::Id::infected);
        map::g_player->m_properties.end_prop(prop::Id::diseased);
        map::g_player->m_properties.end_prop(prop::Id::wound);

        for (auto* const item : cursed_items) {
                const auto name =
                        item->name(
                                ItemNameType::plain,
                                ItemNameInfo::none);

                msg_log::add("The " + name + " seems cleansed!");

                item->current_curse().on_curse_end();

                item->remove_curse();
        }
}

std::string Liquid::name(const Article article) const
{
        std::string str;

        if (article == Article::the) {
                str += "the ";
        }

        switch (m_type) {
        case LiquidType::water:
                str += "water";
                break;

        case LiquidType::mud:
                str += "shallow mud";
                break;

        case LiquidType::magic_water:
                str += "gleaming pool";
                break;
        }

        return str;
}

Color Liquid::color_default() const
{
        switch (m_type) {
        case LiquidType::water:
                return colors::light_blue();
                break;

        case LiquidType::mud:
                return colors::brown();
                break;

        case LiquidType::magic_water:
                return colors::light_cyan();
                break;
        }

        ASSERT(false && "Failed to set color");
        return colors::yellow();
}

// -----------------------------------------------------------------------------
// Chasm
// -----------------------------------------------------------------------------
Chasm::Chasm(const P& p, const TerrainData* const data) :
        Terrain(p, data) {}

void Chasm::hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const P& from_pos,
        int dmg)
{
        (void)dmg_type;
        (void)actor;
        (void)from_pos;
        (void)dmg;
}

std::string Chasm::name(const Article article) const
{
        std::string a = (article == Article::a) ? "a " : "the ";

        return a + "chasm";
}

Color Chasm::color_default() const
{
        return colors::dark_blue();
}

// -----------------------------------------------------------------------------
// Crystal Key
// -----------------------------------------------------------------------------
CrystalKey::CrystalKey(const P& p, const TerrainData* const data) :
        Terrain(p, data) {}

void CrystalKey::hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const P& from_pos,
        int dmg)
{
        (void)dmg_type;
        (void)actor;
        (void)from_pos;
        (void)dmg;
}

std::string CrystalKey::name(const Article article) const
{
        std::string str = (article == Article::a) ? "a" : "the";

        str += " ";

        str += m_is_active ? "gleaming" : "dead";

        str += " crystal";

        return str;
}

Color CrystalKey::color_default() const
{
        return m_is_active ? colors::light_red() : colors::gray();
}

gfx::TileId CrystalKey::tile() const
{
        return gfx::TileId::crystal;

        // return m_is_active ? gfx::TileId::crystal : gfx::TileId::crystal_destroyed;
}

void CrystalKey::bump(actor::Actor& actor_bumping)
{
        (void)actor_bumping;

        TRACE_FUNC_BEGIN;

        map::memorize_terrain_at(m_pos);
        map::update_vision();

        const bool is_seen = map::g_seen.at(m_pos);

        const std::string terrain_name = text_format::first_to_lower(name(Article::the));

        if (is_seen) {
                msg_log::add("I touch " + terrain_name + ".");
        }
        else {
                msg_log::clear();

                msg_log::add("I touch some crystal object.");
        }

        if (!m_is_active) {
                if (is_seen) {
                        msg_log::add("Nothing happens.");
                }

                return;
        }

        if (is_seen) {
                msg_log::add("The light inside fades.");
        }

        player_deactivate();

        game_time::tick();

        TRACE_FUNC_END;
}

void CrystalKey::player_deactivate()
{
        audio::play(audio::SfxId::crystal_key_disable);

        msg_log::add("I sense that a path has opened somewhere.");

        game::incr_player_xp(3, Verbose::yes);

        deactivate();

        map::memorize_terrain_at(m_pos);
        map::update_vision();
}

void CrystalKey::deactivate()
{
        m_is_active = false;

        // Signal that the crystal has been deactivated to the linked door.
        if (m_linked_door) {
                m_linked_door->remove_ward();
        }

        // Deactivate all siblings.
        for (CrystalKey* const sibbling : m_sibblings) {
                sibbling->m_is_active = false;
        }
}

void CrystalKey::add_light_hook(Array2<bool>& light) const
{
        if (m_is_active) {
                light.at(m_pos) = true;
        }
}

// -----------------------------------------------------------------------------
// Altar
// -----------------------------------------------------------------------------
Altar::Altar(const P& p, const TerrainData* const data) :
        Terrain(p, data) {}

void Altar::hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const P& from_pos,
        int dmg)
{
        (void)actor;
        (void)from_pos;
        (void)dmg;

        switch (dmg_type) {
        case DmgType::explosion:
        case DmgType::pure:
                if (map::g_seen.at(m_pos)) {
                        msg_log::add("The altar is destroyed.");
                }

                map::update_terrain(make(Id::rubble_low, m_pos));
                map::update_vision();

                if (player_bon::is_bg(Bg::exorcist)) {
                        const auto msg =
                                rnd::element(
                                        common_text::g_exorcist_purge_phrases);

                        msg_log::add(msg);

                        game::incr_player_xp(10);

                        map::g_player->restore_sp(999, false, Verbose::no);
                        map::g_player->restore_sp(10, true);
                }
                break;

        default:
                break;
        }
}

void Altar::on_new_turn()
{
        if (map::g_player->m_pos.is_adjacent(m_pos) &&
            map::g_seen.at(m_pos) &&
            !player_bon::is_bg(Bg::exorcist)) {
                hints::display(hints::Id::altars);
        }
}

void Altar::bump(actor::Actor& actor_bumping)
{
        if (!actor::is_player(&actor_bumping)) {
                return;
        }

        map::memorize_terrain_at(m_pos);
        map::update_vision();

        if (player_bon::is_bg(Bg::exorcist) &&
            map::g_seen.at(m_pos)) {
                // Exorcist player is bumping a seen altar
                msg_log::add(
                        "A diabolic altar has been raised here, it must be "
                        "destroyed!");

                return;
        }

        // Nothing special happening - use standard bump handling
        Terrain::bump(actor_bumping);
}

std::string Altar::name(const Article article) const
{
        std::string str = (article == Article::a) ? "an " : "the ";

        return str + "altar";
}

Color Altar::color_default() const
{
        return colors::white();
}

// -----------------------------------------------------------------------------
// Carpet
// -----------------------------------------------------------------------------
Carpet::Carpet(const P& p, const TerrainData* const data) :
        Terrain(p, data) {}

void Carpet::hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const P& from_pos,
        int dmg)
{
        (void)actor;
        (void)from_pos;
        (void)dmg;

        switch (dmg_type) {
        case DmgType::fire:
                try_start_burning(Verbose::no);
                break;

        default:
                break;
        }
}

WasDestroyed Carpet::on_finished_burning()
{
        auto* const floor =
                static_cast<Floor*>(make(Id::floor, m_pos));

        floor->m_burn_state = BurnState::has_burned;

        map::update_terrain(floor);

        return WasDestroyed::yes;
}

std::string Carpet::name(const Article article) const
{
        std::string str = (article == Article::a) ? "" : "the ";

        return str + "carpet";
}

Color Carpet::color_default() const
{
        return colors::red();
}

// -----------------------------------------------------------------------------
// Grass
// -----------------------------------------------------------------------------
Grass::Grass(const P& p, const TerrainData* const data) :
        Terrain(p, data),
        m_type(GrassType::common)
{
        if (rnd::one_in(5)) {
                m_type = GrassType::withered;
        }
}

void Grass::hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const P& from_pos,
        int dmg)
{
        (void)actor;
        (void)from_pos;
        (void)dmg;

        switch (dmg_type) {
        case DmgType::fire:
                try_start_burning(Verbose::no);
                break;

        default:
                break;
        }
}

gfx::TileId Grass::tile() const
{
        if (m_burn_state == BurnState::has_burned) {
                return gfx::TileId::scorched_ground;
        }
        else {
                return m_data->tile;
        }
}

std::string Grass::name(const Article article) const
{
        std::string str;

        if (article == Article::the) {
                str += "the ";
        }

        switch (m_burn_state) {
        case BurnState::not_burned:
                switch (m_type) {
                case GrassType::common:
                        return str + "grass";

                case GrassType::withered:
                        return str + "withered grass";
                }
                break;

        case BurnState::burning:
                return str + "burning grass";

        case BurnState::has_burned:
                return str + "scorched ground";
        }

        ASSERT("Failed to set name" && false);
        return "";
}

Color Grass::color_default() const
{
        switch (m_type) {
        case GrassType::common:
                return colors::green();
                break;

        case GrassType::withered:
                return colors::dark_brown();
                break;
        }

        ASSERT(false && "Failed to set color");
        return colors::yellow();
}

// -----------------------------------------------------------------------------
// Bush
// -----------------------------------------------------------------------------
Bush::Bush(const P& p, const TerrainData* const data) :
        Terrain(p, data),
        m_type(GrassType::common)
{
        if (rnd::one_in(5)) {
                m_type = GrassType::withered;
        }
}

void Bush::hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const P& from_pos,
        int dmg)
{
        (void)actor;
        (void)from_pos;
        (void)dmg;

        switch (dmg_type) {
        case DmgType::fire:
                try_start_burning(Verbose::no);
                break;

        default:
                break;
        }
}

WasDestroyed Bush::on_finished_burning()
{
        auto* const grass =
                static_cast<Grass*>(make(Id::grass, m_pos));

        grass->m_burn_state = BurnState::has_burned;

        map::update_terrain(grass);

        return WasDestroyed::yes;
}

std::string Bush::name(const Article article) const
{
        std::string str = (article == Article::a) ? "a " : "the ";

        switch (m_burn_state) {
        case BurnState::not_burned:
                switch (m_type) {
                case GrassType::common:
                        return str + "shrub";

                case GrassType::withered:
                        return str + "withered shrub";
                }
                break;

        case BurnState::burning:
                return str + "burning shrub";

        case BurnState::has_burned:
                // Should not happen
                break;
        }

        ASSERT("Failed to set name" && false);
        return "";
}

Color Bush::color_default() const
{
        switch (m_type) {
        case GrassType::common:
                return colors::green();
                break;

        case GrassType::withered:
                return colors::dark_brown();
                break;
        }

        ASSERT(false && "Failed to set color");
        return colors::yellow();
}

// -----------------------------------------------------------------------------
// Vines
// -----------------------------------------------------------------------------
Vines::Vines(const P& p, const TerrainData* const data) :
        Terrain(p, data) {}

void Vines::hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const P& from_pos,
        int dmg)
{
        (void)actor;
        (void)from_pos;
        (void)dmg;

        switch (dmg_type) {
        case DmgType::explosion:
        case DmgType::pure:
                map::update_terrain(make(Id::rubble_low, m_pos));
                map::update_vision();
                break;

        case DmgType::fire:
                try_start_burning(Verbose::no);
                break;

        default:
                break;
        }
}

WasDestroyed Vines::on_finished_burning()
{
        auto* const floor =
                static_cast<Floor*>(make(Id::floor, m_pos));

        floor->m_burn_state = BurnState::has_burned;

        map::update_terrain(floor);

        return WasDestroyed::yes;
}

std::string Vines::name(const Article article) const
{
        std::string str = (article == Article::a) ? "" : "the ";

        switch (m_burn_state) {
        case BurnState::not_burned:
                return str + "hanging vines";

        case BurnState::burning:
                return str + "burning vines";

        case BurnState::has_burned:
                // Should not happen
                break;
        }

        ASSERT("Failed to set name" && false);
        return "";
}

Color Vines::color_default() const
{
        return colors::green();
}

// -----------------------------------------------------------------------------
// Chains
// -----------------------------------------------------------------------------
Chains::Chains(const P& p, const TerrainData* const data) :
        Terrain(p, data) {}

std::string Chains::name(const Article article) const
{
        std::string a = (article == Article::a) ? "" : "the ";

        return a + "rusty chains";
}

Color Chains::color_default() const
{
        return colors::gray();
}

void Chains::bump(actor::Actor& actor_bumping)
{
        if (actor_bumping.m_data->actor_size > actor::Size::floor &&
            !actor_bumping.m_properties.has(prop::Id::ethereal) &&
            !actor_bumping.m_properties.has(prop::Id::ooze)) {
                std::string msg;

                if (map::g_seen.at(m_pos)) {
                        msg = "The chains rattle.";
                }
                else {
                        msg = "I hear chains rattling.";
                }

                const auto alerts_mon =
                        actor::is_player(&actor_bumping)
                        ? AlertsMon::yes
                        : AlertsMon::no;

                Snd snd(
                        msg,
                        audio::SfxId::chains,
                        IgnoreMsgIfOriginSeen::no,
                        actor_bumping.m_pos,
                        &actor_bumping,
                        SndVol::low,
                        alerts_mon);

                snd_emit::run(snd);
        }
}

void Chains::hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const P& from_pos,
        int dmg)
{
        (void)actor;
        (void)from_pos;
        (void)dmg;

        switch (dmg_type) {
        case DmgType::explosion:
        case DmgType::pure:
                map::update_terrain(make(Id::rubble_low, m_pos));
                map::update_vision();
                break;

        default:
                break;
        }
}

// -----------------------------------------------------------------------------
// Grate
// -----------------------------------------------------------------------------
Grate::Grate(const P& p, const TerrainData* const data) :
        Terrain(p, data) {}

void Grate::hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const P& from_pos,
        int dmg)
{
        (void)actor;
        (void)from_pos;
        (void)dmg;

        switch (dmg_type) {
        case DmgType::pure:
        case DmgType::explosion:
                destr_all_adj_doors(m_pos);
                map::update_terrain(make(Id::rubble_low, m_pos));
                map::update_vision();
                break;

        default:
                break;
        }
}

std::string Grate::name(const Article article) const
{
        std::string a = (article == Article::a) ? "a " : "the ";

        return a + "grate";
}

Color Grate::color_default() const
{
        return colors::gray();
}

// -----------------------------------------------------------------------------
// Tree
// -----------------------------------------------------------------------------
Tree::Tree(const P& p, const TerrainData* const data) :
        Terrain(p, data)
{
        if (is_fungi()) {
                const std::vector<Color> base_color_bucket = {
                        colors::white(),
                        colors::cyan(),
                        colors::gray_brown(),
                        colors::orange(),
                        colors::green()};

                const std::vector<int> weights = {
                        100,
                        10,
                        10,
                        1,
                        1};

                const size_t choice = rnd::weighted_choice(weights);

                m_color = base_color_bucket[choice];

                m_color.randomize_rgb(20);
        }
        else {
                m_color = colors::dark_brown();
        }
}

void Tree::hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const P& from_pos,
        int dmg)
{
        (void)actor;
        (void)from_pos;
        (void)dmg;

        switch (dmg_type) {
        case DmgType::fire:
                if (rnd::fraction(2, 3)) {
                        try_start_burning(Verbose::no);
                }
                break;

        default:
                break;
        }
}

WasDestroyed Tree::on_finished_burning()
{
        if (!map::is_pos_inside_outer_walls(m_pos)) {
                return WasDestroyed::no;
        }

        auto* const grass =
                static_cast<Grass*>(make(Id::grass, m_pos));

        grass->m_burn_state = BurnState::has_burned;

        map::update_terrain(grass);

        map::update_vision();

        return WasDestroyed::yes;
}

gfx::TileId Tree::tile() const
{
        return (
                is_fungi()
                        ? gfx::TileId::tree_fungi
                        : gfx::TileId::tree);
}

std::string Tree::name(const Article article) const
{
        std::string result = (article == Article::a) ? "a " : "the ";

        switch (m_burn_state) {
        case BurnState::not_burned:
                break;

        case BurnState::burning:
                result += "burning ";
                break;

        case BurnState::has_burned:
                result += "scorched ";
                break;
        }

        if (is_fungi()) {
                result += "giant fungi";
        }
        else {
                result += "tree";
        }

        return result;
}

Color Tree::color_default() const
{
        return m_color;
}

bool Tree::is_fungi() const
{
        return (map::g_dlvl > 1);
}

// -----------------------------------------------------------------------------
// Brazier
// -----------------------------------------------------------------------------
std::string Brazier::name(const Article article) const
{
        std::string a = (article == Article::a) ? "a " : "the ";

        return a + "brazier";
}

bool Brazier::allow_player_melee_attack(
        const DmgType dmg_type,
        const item::Item& wpn) const
{
        (void)wpn;

        switch (dmg_type) {
        case DmgType::kicking:
                return true;
                break;

        default:
                break;
        }

        return false;
}

void Brazier::hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const P& from_pos,
        int dmg)
{
        (void)dmg;

        switch (dmg_type) {
        case DmgType::kicking:
        case DmgType::control_object_spell: {
                ASSERT(actor);

                if ((dmg_type == DmgType::kicking) &&
                    actor->m_properties.has(prop::Id::weakened)) {
                        msg_log::add("It wiggles a bit.");

                        return;
                }

                const auto alerts_mon =
                        actor::is_player(actor)
                        ? AlertsMon::yes
                        : AlertsMon::no;

                if (map::g_seen.at(m_pos)) {
                        msg_log::add("It topples over.");
                }

                Snd snd(
                        "I hear a crash.",
                        audio::SfxId::END,
                        IgnoreMsgIfOriginSeen::yes,
                        m_pos,
                        actor,
                        SndVol::low,
                        alerts_mon);

                snd_emit::run(snd);

                const P dst_pos = m_pos + (m_pos - from_pos);

                const P my_pos = m_pos;

                map::update_terrain(make(Id::rubble_low, m_pos));

                // NOTE: "this" is now deleted!

                const auto* const tgt_f = map::g_terrain.at(dst_pos);

                if (tgt_f->id() != terrain::Id::chasm) {
                        P expl_pos;

                        int expl_d = 0;

                        if (tgt_f->is_projectile_passable()) {
                                expl_pos = dst_pos;
                                expl_d = -1;
                        }
                        else {
                                expl_pos = my_pos;
                                expl_d = -2;
                        }

                        // TODO: Emit sound from explosion center

                        explosion::run(
                                expl_pos,
                                ExplType::apply_prop,
                                EmitExplSnd::no,
                                expl_d,
                                ExplExclCenter::no,
                                {prop::make(prop::Id::burning)});
                }

                map::update_vision();
        } break;

        case DmgType::explosion:
        case DmgType::pure: {
                map::update_terrain(make(Id::rubble_low, m_pos));
                map::update_vision();
        } break;

        default:
        {
        } break;
        }
}

void Brazier::add_light_hook(Array2<bool>& light) const
{
        for (const P& d : dir_utils::g_dir_list_w_center) {
                const P p(m_pos + d);

                light.at(p) = true;
        }
}

Color Brazier::color_default() const
{
        return colors::yellow();
}

// -----------------------------------------------------------------------------
// Item container
// -----------------------------------------------------------------------------
ItemContainer::ItemContainer()
{
        clear();
}

ItemContainer::~ItemContainer()
{
        for (auto* item : m_items) {
                delete item;
        }
}

void ItemContainer::init(
        const terrain::Id terrain_id,
        const int nr_items_to_attempt)
{
        for (auto* item : m_items) {
                delete item;
        }

        m_items.clear();

        if (nr_items_to_attempt <= 0) {
                return;
        }

        // Try until actually succeeded to add at least one item
        while (m_items.empty()) {
                std::vector<item::Id> item_bucket;

                for (size_t i = 0; i < (size_t)item::Id::END; ++i) {
                        auto& item_d = item::g_data[i];

                        if (!item_d.allow_spawn) {
                                // Item not allowed to spawn - next item!
                                continue;
                        }

                        const bool can_spawn_in_container =
                                std::find(
                                        std::begin(item_d.native_containers),
                                        std::end(item_d.native_containers),
                                        terrain_id) !=
                                std::end(item_d.native_containers);

                        if (!can_spawn_in_container) {
                                // Item not allowed to spawn in this terrain -
                                // next item!
                                continue;
                        }

                        if (rnd::percent(item_d.chance_to_incl_in_spawn_list)) {
                                item_bucket.push_back(item::Id(i));
                        }
                }

                for (int i = 0; i < nr_items_to_attempt; ++i) {
                        if (item_bucket.empty()) {
                                break;
                        }

                        const int idx =
                                rnd::range(0, (int)item_bucket.size() - 1);

                        const auto id = item_bucket[idx];

                        // Is this item still allowed to spawn (perhaps unique)?
                        if (item::g_data[(size_t)id].allow_spawn) {
                                auto* item = item::make(item_bucket[idx]);

                                item::randomize_item_properties(*item);

                                m_items.push_back(item);
                        }
                        else {
                                // Not allowed to spawn
                                item_bucket.erase(begin(item_bucket) + idx);
                        }
                }
        }
}

void ItemContainer::open(
        const P& terrain_pos,
        actor::Actor* const actor_opening)
{
        if (!actor_opening) {
                // Not opened by an actor (probably opened by the opening spell)
                for (auto* item : m_items) {
                        item_drop::drop_item_on_map(terrain_pos, *item);
                }

                m_items.clear();

                return;
        }

        for (auto* item : m_items) {
                on_item_found(item, terrain_pos);
        }

        msg_log::add("There are no more items of interest.");

        m_items.clear();
}

void ItemContainer::on_item_found(
        item::Item* const item,
        const P& terrain_pos)
{
        msg_log::clear();

        const std::string name =
                item->name(
                        ItemNameType::plural,
                        ItemNameInfo::yes,
                        ItemNameAttackInfo::main_attack_mode);

        const std::string msg =
                "Pick up " +
                name +
                "? " +
                common_text::g_yes_or_no_hint;

        msg_log::add(
                msg,
                colors::light_white(),
                MsgInterruptPlayer::no,
                MorePromptOnMsg::no,
                CopyToMsgHistory::no);

        const auto& data = item->data();

        auto* wpn =
                data.ranged.is_ranged_wpn
                ? static_cast<item::Wpn*>(item)
                : nullptr;

        const bool is_unloadable_wpn =
                wpn &&
                (wpn->m_ammo_loaded > 0) &&
                !data.ranged.has_infinite_ammo;

        if (is_unloadable_wpn) {
                msg_log::add("Unload? [u]");
        }

        auto answer = BinaryAnswer::no;

        if (is_unloadable_wpn) {
                answer = query::yes_or_no('u');
        }
        else {
                answer = query::yes_or_no();
        }

        msg_log::clear();

        if (answer == BinaryAnswer::yes) {
                const auto pre_pickup_result = item->pre_pickup_hook();

                switch (pre_pickup_result) {
                case ItemPrePickResult::do_pickup: {
                        audio::play(audio::SfxId::pickup);

                        map::g_player->m_inv.put_in_backpack(item);
                } break;

                case ItemPrePickResult::destroy_item: {
                        delete item;
                } break;

                case ItemPrePickResult::do_nothing: {
                } break;
                }
        }
        else if (answer == BinaryAnswer::no) {
                item_drop::drop_item_on_map(terrain_pos, *item);

                item->discover();
        }
        else {
                // Special key (unload in this case)
                ASSERT(is_unloadable_wpn);

                if (is_unloadable_wpn) {
                        audio::play(audio::SfxId::pickup);

                        auto* const spawned_ammo =
                                item_pickup::unload_ranged_wpn(*wpn);

                        map::g_player->m_inv.put_in_backpack(
                                spawned_ammo);

                        item_drop::drop_item_on_map(terrain_pos, *wpn);
                }
        }

        msg_log::more_prompt();
}

void ItemContainer::clear()
{
        for (auto* item : m_items) {
                delete item;
        }

        m_items.clear();
}

void ItemContainer::destroy_single_fragile()
{
        // TODO: Generalize this (something like "is_fragile" item data)

        for (auto it = std::begin(m_items); it != std::end(m_items); ++it) {
                auto* const item = *it;

                const auto& d = item->data();

                if ((d.type == ItemType::potion) ||
                    (d.id == item::Id::molotov)) {
                        delete item;
                        m_items.erase(it);
                        msg_log::add("I hear a muffled shatter.");
                        break;
                }
        }
}

// -----------------------------------------------------------------------------
// Tomb
// -----------------------------------------------------------------------------
Tomb::Tomb(const P& p, const TerrainData* const data) :
        Terrain(p, data),
        m_is_open(false),
        m_is_trait_known(false),
        m_push_lid_one_in_n(rnd::range(4, 10)),
        m_appearance(TombAppearance::common),
        m_trait(TombTrait::END)
{
        // Contained items
        const int nr_items_min =
                rnd::one_in(4)
                ? 0
                : 1;

        int nr_items_max = 1;

        int incr_max_items_one_in = 12;

        if (player_bon::has_trait(Trait::treasure_hunter)) {
                incr_max_items_one_in /= 2;
        }

        if (rnd::one_in(incr_max_items_one_in)) {
                ++nr_items_max;
        }

        m_item_container.init(
                terrain::Id::tomb,
                rnd::range(nr_items_min, nr_items_max));

        // Set appearance - sometimes we base the appearance on the value of the
        // contained items, and sometimes we set a "common" appearance
        // regardless of the items. If the tomb is nicer than "common", then it
        // ALWAYS reflects the items. I.e. if the tomb is "common" it *may*
        // contain good items, if it's nicer than "common" it's guaranteed to
        // have good items.
        if (rnd::one_in(4)) {
                // Base appearance on value of contained items

                for (const auto* item : m_item_container.items()) {
                        const auto item_value = item->data().value;

                        if (item_value == item::Value::supreme_treasure) {
                                m_appearance = TombAppearance::marvelous;

                                break;
                        }
                        else if (item_value >= item::Value::minor_treasure) {
                                m_appearance = TombAppearance::ornate;
                        }
                }
        }
        else {
                // Do not base appearance on items - use a common appearance
                m_appearance = TombAppearance::common;
        }

        if (m_appearance == TombAppearance::marvelous) {
                m_trait = TombTrait::ghost;
        }
        else {
                // Randomized trait
                std::vector<int> weights((size_t)TombTrait::END + 1, 0);

                weights[(size_t)TombTrait::ghost] = 5;
                weights[(size_t)TombTrait::other_undead] = 3;
                weights[(size_t)TombTrait::stench] = 2;
                weights[(size_t)TombTrait::cursed] = 1;
                weights[(size_t)TombTrait::END] = 2;

                m_trait = TombTrait(rnd::weighted_choice(weights));
        }
}

void Tomb::hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const P& from_pos,
        int dmg)
{
        (void)actor;
        (void)from_pos;
        (void)dmg;

        switch (dmg_type) {
        case DmgType::explosion:
        case DmgType::pure:
                if (map::g_seen.at(m_pos)) {
                        msg_log::add("The tomb is destroyed.");
                }

                map::update_terrain(make(Id::rubble_low, m_pos));
                map::update_vision();
                break;

        default:
                break;
        }
}

std::string Tomb::name(const Article article) const
{
        const bool is_empty = m_is_open && m_item_container.is_empty();

        const std::string empty_str =
                is_empty
                ? "empty "
                : "";

        const std::string open_str =
                (m_is_open && !is_empty)
                ? "open "
                : "";

        std::string a;

        if (article == Article::a) {
                a =
                        (m_is_open || (m_appearance == TombAppearance::ornate))
                        ? "an "
                        : "a ";
        }
        else {
                a = "the ";
        }

        std::string appear_str;

        if (!is_empty) {
                switch (m_appearance) {
                case TombAppearance::common:
                case TombAppearance::END: {
                } break;

                case TombAppearance::ornate:
                        appear_str = "ornate ";
                        break;

                case TombAppearance::marvelous:
                        appear_str = "marvelous ";
                        break;
                }
        }

        return a + empty_str + open_str + appear_str + "tomb";
}

gfx::TileId Tomb::tile() const
{
        return (
                m_is_open
                        ? gfx::TileId::tomb_open
                        : gfx::TileId::tomb_closed);
}

Color Tomb::color_default() const
{
        switch (m_appearance) {
        case TombAppearance::common:
                return colors::gray();

        case TombAppearance::ornate:
                return colors::cyan();

        case TombAppearance::marvelous:
                return colors::yellow();

        case TombAppearance::END:
                break;
        }

        ASSERT("Failed to set Tomb color" && false);
        return colors::black();
}

void Tomb::bump(actor::Actor& actor_bumping)
{
        if (!actor::is_player(&actor_bumping)) {
                return;
        }

        map::memorize_terrain_at(m_pos);
        map::update_vision();

        if (m_item_container.is_empty() && m_is_open) {
                msg_log::add("The tomb is empty.");

                return;
        }

        if (!map::g_seen.at(m_pos)) {
                msg_log::add("There is a stone box here.");

                return;
        }

        if (m_is_open) {
                player_loot();

                map::memorize_terrain_at(m_pos);
                map::update_vision();

                game_time::tick();

                return;
        }

        msg_log::add("I attempt to push the lid.");

        if (actor_bumping.m_properties.has(prop::Id::weakened)) {
                msg_log::add("It seems futile.");

                game_time::tick();

                return;
        }

        int bon = 0;

        if (player_bon::has_trait(Trait::rugged)) {
                bon = 8;
        }
        else if (player_bon::has_trait(Trait::tough)) {
                bon = 4;
        }
        else {
                bon = 0;
        }

        TRACE
                << "Base chance to push lid is: 1 in "
                << m_push_lid_one_in_n << std::endl;

        TRACE
                << "Bonus to roll: "
                << bon << std::endl;

        const int roll_tot = rnd::range(1, m_push_lid_one_in_n) + bon;

        TRACE << "Roll + bonus = " << roll_tot << std::endl;

        bool is_success = false;

        if (roll_tot < (m_push_lid_one_in_n - 9)) {
                msg_log::add("It does not yield at all.");
        }
        else if (roll_tot < (m_push_lid_one_in_n - 2)) {
                msg_log::add("It resists.");
        }
        else if (roll_tot == (m_push_lid_one_in_n - 2)) {
                msg_log::add("It moves a little!");
                --m_push_lid_one_in_n;
        }
        else {
                is_success = true;
        }

        if (is_success) {
                open(map::g_player);
        }
        else {
                bash::try_sprain_player();
        }

        map::memorize_terrain_at(m_pos);
        map::update_vision();

        game_time::tick();
}

void Tomb::player_loot()
{
        msg_log::add("I peer inside the tomb.");

        if (m_item_container.is_empty()) {
                msg_log::add("There is nothing of value inside.");
        }
        else {
                m_item_container.open(m_pos, map::g_player);
        }
}

DidOpen Tomb::open(actor::Actor* const actor_opening)
{
        if (m_is_open) {
                return DidOpen::no;
        }
        else {
                // Was not already open
                m_is_open = true;

                Snd snd(
                        "I hear heavy stone sliding.",
                        audio::SfxId::tomb_open,
                        IgnoreMsgIfOriginSeen::yes,
                        m_pos,
                        map::g_player,
                        SndVol::high,
                        AlertsMon::yes);

                snd.run();

                if (map::g_seen.at(m_pos)) {
                        msg_log::add("The lid comes off.");

                        if (!m_item_container.is_empty()) {
                                msg_log::add("There is something inside.");
                        }
                }

                trigger_trap(actor_opening);

                return DidOpen::yes;
        }
}

DidTriggerTrap Tomb::trigger_trap(actor::Actor* const actor)
{
        TRACE_FUNC_BEGIN;

        (void)actor;

        DidTriggerTrap did_trigger_trap = DidTriggerTrap::no;

        std::string id_to_spawn;

        const bool is_seen = map::g_seen.at(m_pos);

        switch (m_trait) {
        case TombTrait::ghost: {
                id_to_spawn = "MON_GHOST";

                const std::string msg = "The air suddenly feels colder.";

                msg_log::add(
                        msg,
                        colors::white(),
                        MsgInterruptPlayer::no,
                        MorePromptOnMsg::yes);

                did_trigger_trap = DidTriggerTrap::yes;
        } break;

        case TombTrait::other_undead: {
                std::vector<std::string> mon_bucket = {
                        "MON_MUMMY",
                        "MON_CROC_HEAD_MUMMY",
                        "MON_ZOMBIE",
                        "MON_FLOATING_SKULL"};

                id_to_spawn = rnd::element(mon_bucket);

                const std::string msg = "Something rises from the tomb!";

                msg_log::add(
                        msg,
                        colors::white(),
                        MsgInterruptPlayer::no,
                        MorePromptOnMsg::yes);

                did_trigger_trap = DidTriggerTrap::yes;
        } break;

        case TombTrait::stench: {
                if (rnd::coin_toss()) {
                        if (is_seen) {
                                msg_log::add(
                                        "Fumes burst out from the tomb!",
                                        colors::white(),
                                        MsgInterruptPlayer::no,
                                        MorePromptOnMsg::yes);
                        }

                        Snd snd(
                                "I hear a burst of gas.",
                                audio::SfxId::gas,
                                IgnoreMsgIfOriginSeen::yes,
                                m_pos,
                                nullptr,
                                SndVol::low,
                                AlertsMon::yes);

                        snd_emit::run(snd);

                        prop::Prop* prop = nullptr;

                        Color fume_color = colors::magenta();

                        const int rnd = rnd::range(1, 100);

                        if (rnd < 20) {
                                prop = prop::make(prop::Id::poisoned);

                                fume_color = colors::light_green();
                        }
                        else if (rnd < 40) {
                                prop = prop::make(prop::Id::diseased);

                                fume_color = colors::green();
                        }
                        else {
                                prop = prop::make(prop::Id::paralyzed);

                                prop->set_duration(prop->nr_turns_left() * 2);
                        }

                        explosion::run(
                                m_pos,
                                ExplType::apply_prop,
                                EmitExplSnd::no,
                                0,
                                ExplExclCenter::no,
                                {prop},
                                fume_color,
                                ExplIsGas::yes);
                }
                else {
                        // Not fumes
                        std::vector<std::string> mon_bucket;

                        for (const auto& it : actor::g_data) {
                                const actor::ActorData& d = it.second;

                                if (d.natural_props[(size_t)prop::Id::ooze] &&
                                    d.is_auto_spawn_allowed &&
                                    !d.is_unique) {
                                        mon_bucket.push_back(d.id);
                                }
                        }

                        id_to_spawn = rnd::element(mon_bucket);

                        if (is_seen) {
                                msg_log::add(
                                        "Something repulsive creeps up from the tomb!",
                                        colors::white(),
                                        MsgInterruptPlayer::no,
                                        MorePromptOnMsg::yes);
                        }
                }

                did_trigger_trap = DidTriggerTrap::yes;
        } break;

        case TombTrait::cursed: {
                map::g_player->m_properties.apply(prop::make(prop::Id::cursed));

                did_trigger_trap = DidTriggerTrap::yes;
        } break;

        case TombTrait::END:
                break;
        }

        if (!id_to_spawn.empty()) {
                const actor::MonSpawnResult summoned =
                        actor::spawn(m_pos, {id_to_spawn}, map::rect())
                                .make_aware_of_player();

                std::for_each(
                        std::begin(summoned.monsters),
                        std::end(summoned.monsters),
                        [this](auto* const mon) {
                                auto* prop =
                                        prop::make(
                                                prop::Id::waiting);

                                prop->set_duration(1);

                                mon->m_properties.apply(prop);

                                if (m_appearance == TombAppearance::marvelous) {
                                        mon->change_max_hp(
                                                mon->m_hp,
                                                Verbose::no);

                                        mon->restore_hp(
                                                999,
                                                false,
                                                Verbose::no);
                                }
                        });
        }

        m_trait = TombTrait::END;

        m_is_trait_known = true;

        TRACE_FUNC_END;

        return did_trigger_trap;
}

// -----------------------------------------------------------------------------
// Chest
// -----------------------------------------------------------------------------
Chest::Chest(const P& p, const TerrainData* const data) :
        Terrain(p, data),
        m_is_open(false),
        m_is_locked(false),
        m_material(ChestMaterial::wood)
{
        if ((map::g_dlvl >= 3) && rnd::fraction(2, 3)) {
                m_material = ChestMaterial::iron;
        }

        // Contained items
        const int nr_items_min = rnd::one_in(4) ? 0 : 1;
        int nr_items_max = 2;
        int incr_max_items_one_in = 12;

        if (player_bon::has_trait(Trait::treasure_hunter)) {
                incr_max_items_one_in /= 2;
        }

        if (rnd::one_in(incr_max_items_one_in)) {
                ++nr_items_max;
        }

        m_item_container.init(
                terrain::Id::chest,
                rnd::range(nr_items_min, nr_items_max));

        const int locked_numer =
                m_item_container.is_empty()
                ? 1
                : std::min(8, map::g_dlvl);

        m_is_locked = rnd::fraction(locked_numer, 10);
}

void Chest::bump(actor::Actor& actor_bumping)
{
        if (!actor::is_player(&actor_bumping)) {
                return;
        }

        map::memorize_terrain_at(m_pos);
        map::update_vision();

        if (!map::g_seen.at(m_pos)) {
                msg_log::add("There is a chest here.");

                return;
        }

        if (m_burn_state == BurnState::burning) {
                msg_log::add("The chest is on fire.");

                return;
        }

        if (m_item_container.is_empty() && m_is_open) {
                msg_log::add("The chest is empty.");

                return;
        }

        if (m_is_locked) {
                msg_log::add("The chest is locked.");

                return;
        }

        // Not locked
        if (m_is_open) {
                player_loot();
        }
        else {
                open(map::g_player);
        }

        map::memorize_terrain_at(m_pos);
        map::update_vision();

        game_time::tick();
}

void Chest::player_loot()
{
        msg_log::add("I search the chest.");

        if (m_item_container.is_empty()) {
                msg_log::add("There is nothing of value inside.");
        }
        else {
                // Not empty
                m_item_container.open(m_pos, map::g_player);
        }
}

DidOpen Chest::open(actor::Actor* const actor_opening)
{
        (void)actor_opening;

        m_is_locked = false;

        if (m_is_open) {
                return DidOpen::no;
        }
        else {
                m_is_open = true;

                if (map::g_seen.at(m_pos)) {
                        msg_log::add("The chest opens.");
                }

                return DidOpen::yes;
        }
}

bool Chest::allow_player_melee_attack(
        const DmgType dmg_type,
        const item::Item& wpn) const
{
        (void)wpn;

        switch (dmg_type) {
        case DmgType::kicking:
                return true;
                break;

        default:
                break;
        }

        return false;
}

void Chest::hit(
        DmgType dmg_type,
        actor::Actor* const actor,
        const P& from_pos,
        const int dmg)
{
        (void)actor;
        (void)from_pos;
        (void)dmg;

        switch (dmg_type) {
        case DmgType::kicking:
                on_player_kick();
                break;

        default:
                break;
        }
}

void Chest::on_player_kick()
{
        if (!map::g_seen.at(m_pos)) {
                // If player is blind, call the parent hit function
                // instead (generic kicking)
                Terrain::hit(DmgType::kicking, map::g_player);

                return;
        }

        if (m_is_open) {
                msg_log::add("It is already open.");

                return;
        }

        Snd snd(
                "",
                audio::SfxId::END,
                IgnoreMsgIfOriginSeen::yes,
                m_pos,
                map::g_player,
                SndVol::high,
                AlertsMon::yes);

        // Is seen and closed

        if (!m_is_locked) {
                msg_log::add("The lid slams open, then falls shut.");

                snd.run();

                return;
        }

        // Is locked

        if (map::g_player->m_properties.has(prop::Id::weakened) ||
            (m_material == ChestMaterial::iron)) {
                msg_log::add("It seems futile.");

                snd.run();

                return;
        }

        // Chest can be bashed open
        if (rnd::one_in(3)) {
                m_item_container.destroy_single_fragile();
        }

        int open_one_in_n = 0;

        if (player_bon::has_trait(Trait::rugged)) {
                open_one_in_n = 2;
        }
        else if (player_bon::has_trait(Trait::tough)) {
                open_one_in_n = 3;
        }
        else {
                open_one_in_n = 4;
        }

        if (rnd::one_in(open_one_in_n)) {
                msg_log::add(
                        "The lock breaks and the lid flies open!",
                        colors::text(),
                        MsgInterruptPlayer::no,
                        MorePromptOnMsg::yes);

                m_is_locked = false;
                m_is_open = true;
        }
        else {
                msg_log::add("The lock resists.");
        }

        snd.run();
}

std::string Chest::name(const Article article) const
{
        std::string material_str;
        std::string locked_str;
        std::string empty_str;
        std::string open_str;
        std::string a;

        if (m_material == ChestMaterial::wood) {
                material_str = "wooden ";
                a = "a ";
        }
        else {
                material_str = "iron ";
                a = "an ";
        }

        if (m_is_open) {
                if (m_item_container.is_empty()) {
                        empty_str = "empty ";
                }
                else {
                        open_str = "open ";
                }

                a = "an ";
        }
        else if (m_is_locked) {
                locked_str = "locked ";

                a = "a ";
        }

        if (article == Article::the) {
                a = "the ";
        }

        return a + locked_str + empty_str + open_str + material_str + "chest";
}

gfx::TileId Chest::tile() const
{
        return m_is_open ? gfx::TileId::chest_open : gfx::TileId::chest_closed;
}

Color Chest::color_default() const
{
        return (m_material == ChestMaterial::wood) ? colors::dark_brown() : colors::gray();
}

// -----------------------------------------------------------------------------
// Fountain
// -----------------------------------------------------------------------------
Fountain::Fountain(const P& p, const TerrainData* const data) :
        Terrain(p, data)
{
        std::vector<int> weights = {
                4,  // Refreshing
                1,  // XP
                1,  // Bad effect
        };

        const int choice = rnd::weighted_choice(weights);

        switch (choice) {
        case 0: {
                m_fountain_effect = FountainEffect::refreshing;
        } break;

        case 1: {
                m_fountain_effect = FountainEffect::xp;
        } break;

        case 2: {
                const int min = (int)FountainEffect::START_OF_BAD_EFFECTS + 1;
                const int max = (int)FountainEffect::END - 1;

                m_fountain_effect = (FountainEffect)rnd::range(min, max);
        } break;

        default:
        {
                ASSERT(false);
        } break;
        }
}

void Fountain::hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const P& from_pos,
        int dmg)
{
        (void)actor;
        (void)from_pos;
        (void)dmg;

        switch (dmg_type) {
        case DmgType::explosion:
        case DmgType::pure:
                if (map::g_seen.at(m_pos)) {
                        msg_log::add("The fountain is destroyed.");
                }

                map::update_terrain(make(Id::rubble_low, m_pos));
                map::update_vision();
                break;

        default:
                break;
        }
}

Color Fountain::color_default() const
{
        if (m_has_drinks_left) {
                if (m_is_tried) {
                        // Has drinks left, tried
                        const auto is_bad =
                                m_fountain_effect >
                                FountainEffect::START_OF_BAD_EFFECTS;

                        return is_bad ? colors::magenta() : colors::light_cyan();
                }
                else {
                        // Has drinks left, not tried
                        return colors::light_blue();
                }
        }
        else {
                // No drinks left
                return colors::gray();
        }
}

std::string Fountain::name(const Article article) const
{
        std::string type_str;

        std::string indefinite_article = "a";

        if (m_has_drinks_left) {
                if (m_is_tried) {
                        type_str = type_name();

                        indefinite_article = type_indefinite_article();
                }
        }
        else {
                type_str = "dried-up";
        }

        const std::string a =
                (article == Article::a)
                ? indefinite_article
                : "the";

        if (!type_str.empty()) {
                type_str = " " + type_str;
        }

        return a + type_str + " fountain";
}

void Fountain::bump(actor::Actor& actor_bumping)
{
        if (!actor::is_player(&actor_bumping)) {
                return;
        }

        const bool is_seen = map::g_seen.at(m_pos);

        map::memorize_terrain_at(m_pos);
        map::update_vision();

        if (!m_has_drinks_left) {
                if (is_seen) {
                        msg_log::add("The fountain is dried-up.");
                }
                else {
                        msg_log::add(
                                "There is a fountain here, "
                                "but it's dried-up.");
                }

                return;
        }

        const auto is_bad =
                m_fountain_effect >
                FountainEffect::START_OF_BAD_EFFECTS;

        if (!is_seen || (m_is_tried && is_bad)) {
                msg_log::clear();

                std::string msg;

                if (is_seen) {
                        const std::string name_the =
                                text_format::first_to_lower(
                                        name(Article::the));

                        msg = "Drink from " + name_the + "?";
                }
                else {
                        const std::string name_a =
                                text_format::first_to_lower(
                                        name(Article::a));

                        msg = "There is " + name_a + " here. Drink from it?";
                }

                msg += " " + common_text::g_yes_or_no_hint;

                msg_log::add(
                        msg,
                        colors::light_white(),
                        MsgInterruptPlayer::no,
                        MorePromptOnMsg::no,
                        CopyToMsgHistory::no);

                const auto answer = query::yes_or_no();

                if (answer == BinaryAnswer::no) {
                        msg_log::clear();

                        return;
                }
        }

        auto& properties = map::g_player->m_properties;

        if (!properties.allow_eat(Verbose::yes)) {
                return;
        }

        msg_log::clear();
        msg_log::add("I drink from the fountain...");

        audio::play(audio::SfxId::fountain_drink);

        switch (m_fountain_effect) {
        case FountainEffect::refreshing: {
                msg_log::add("It's very refreshing.");
                map::g_player->restore_hp(1, false, Verbose::no);
                map::g_player->restore_sp(1, false, Verbose::no);
                map::g_player->restore_shock(5, true);
        } break;

        case FountainEffect::xp: {
                msg_log::add("I feel more powerful!");
                game::incr_player_xp(2);
        } break;

        case FountainEffect::curse: {
                properties.apply(prop::make(prop::Id::cursed));
        } break;

        case FountainEffect::disease: {
                properties.apply(prop::make(prop::Id::diseased));
        } break;

        case FountainEffect::poison: {
                properties.apply(prop::make(prop::Id::poisoned));
        } break;

        case FountainEffect::frenzy: {
                properties.apply(prop::make(prop::Id::frenzied));
        } break;

        case FountainEffect::paralyze: {
                properties.apply(prop::make(prop::Id::paralyzed));
        } break;

        case FountainEffect::blind: {
                properties.apply(prop::make(prop::Id::blind));
        } break;

        case FountainEffect::faint: {
                prop::Prop* prop = prop::make(prop::Id::fainted);

                prop->set_duration(10);

                properties.apply(prop);
        } break;

        case FountainEffect::START_OF_BAD_EFFECTS:
        case FountainEffect::END:
                break;
        }

        m_is_tried = true;

        const int dry_one_in_n = 3;

        if (rnd::one_in(dry_one_in_n)) {
                m_has_drinks_left = false;

                msg_log::add("The fountain dries up.");
        }

        map::memorize_terrain_at(m_pos);
        map::update_vision();

        game_time::tick();
}

void Fountain::on_new_turn()
{
        if (map::g_player->m_pos.is_adjacent(m_pos) &&
            map::g_seen.at(m_pos)) {
                hints::display(hints::Id::fountains);
        }
}

void Fountain::bless()
{
        if (!has_drinks_left()) {
                return;
        }

        const bool is_bad_effect =
                m_fountain_effect >
                FountainEffect::START_OF_BAD_EFFECTS;

        if (!is_bad_effect) {
                return;
        }

        m_is_tried = false;

        m_fountain_effect = FountainEffect::refreshing;

        if (map::g_seen.at(m_pos)) {
                const std::string name_the =
                        text_format::first_to_lower(
                                name(Article::the));

                msg_log::add(
                        "The water in " + name_the +
                        " seems clearer.");
        }
}

void Fountain::curse()
{
        if (!has_drinks_left()) {
                return;
        }

        const bool is_good_effect =
                m_fountain_effect <
                FountainEffect::START_OF_BAD_EFFECTS;

        if (!is_good_effect) {
                return;
        }

        m_is_tried = false;

        const int min = (int)FountainEffect::START_OF_BAD_EFFECTS + 1;
        const int max = (int)FountainEffect::END - 1;

        m_fountain_effect = (FountainEffect)rnd::range(min, max);

        if (map::g_seen.at(m_pos)) {
                std::string name_the =
                        text_format::first_to_lower(
                                name(Article::the));

                msg_log::add(
                        "The water in " +
                        name_the +
                        " seems murkier.");
        }
}

std::string Fountain::type_name() const
{
        switch (m_fountain_effect) {
        case FountainEffect::refreshing:
                return "refreshing";
                break;

        case FountainEffect::xp:
                return "exalting";
                break;

        case FountainEffect::curse:
                return "cursed";
                break;

        case FountainEffect::disease:
                return "diseased";
                break;

        case FountainEffect::poison:
                return "poisonous";
                break;

        case FountainEffect::frenzy:
                return "enraging";
                break;

        case FountainEffect::paralyze:
                return "paralyzing";
                break;

        case FountainEffect::blind:
                return "blinding";
                break;

        case FountainEffect::faint:
                return "sleep-inducing";
                break;

        case FountainEffect::START_OF_BAD_EFFECTS:
        case FountainEffect::END:
                break;
        }

        ASSERT(false);

        return "";
}

std::string Fountain::type_indefinite_article() const
{
        switch (m_fountain_effect) {
        case FountainEffect::refreshing:
                return "a";
                break;

        case FountainEffect::xp:
                return "an";
                break;

        case FountainEffect::curse:
                return "a";
                break;

        case FountainEffect::disease:
                return "a";
                break;

        case FountainEffect::poison:
                return "a";
                break;

        case FountainEffect::frenzy:
                return "an";
                break;

        case FountainEffect::paralyze:
                return "a";
                break;

        case FountainEffect::blind:
                return "a";
                break;

        case FountainEffect::faint:
                return "a";
                break;

        case FountainEffect::START_OF_BAD_EFFECTS:
        case FountainEffect::END:
                break;
        }

        ASSERT(false);

        return "";
}

// -----------------------------------------------------------------------------
// Cabinet
// -----------------------------------------------------------------------------
Cabinet::Cabinet(const P& p, const TerrainData* const data) :
        Terrain(p, data),
        m_is_open(false)
{
        // Contained items
        const int nr_items_min =
                rnd::coin_toss()
                ? 0
                : 1;

        int nr_items_max = 1;

        int incr_max_items_one_in = 12;

        if (player_bon::has_trait(Trait::treasure_hunter)) {
                incr_max_items_one_in /= 2;
        }

        if (rnd::one_in(incr_max_items_one_in)) {
                ++nr_items_max;
        }

        m_item_container.init(
                terrain::Id::cabinet,
                rnd::range(nr_items_min, nr_items_max));
}

void Cabinet::hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const P& from_pos,
        int dmg)
{
        (void)actor;
        (void)from_pos;
        (void)dmg;

        switch (dmg_type) {
        case DmgType::explosion:
        case DmgType::pure:
                if (map::g_seen.at(m_pos)) {
                        msg_log::add("The cabinet is destroyed.");
                }

                map::update_terrain(make(Id::rubble_low, m_pos));
                map::update_vision();
                break;

        case DmgType::fire:
                m_item_container.clear();
                try_start_burning(Verbose::yes);
                break;

        default:
                break;
        }
}

WasDestroyed Cabinet::on_finished_burning()
{
        if (map::g_seen.at(m_pos)) {
                msg_log::add("The cabinet burns down.");
        }

        auto* const rubble = make(Id::rubble_low, m_pos);

        rubble->m_burn_state = BurnState::has_burned;

        map::update_terrain(rubble);
        map::update_vision();

        return WasDestroyed::yes;
}

void Cabinet::bump(actor::Actor& actor_bumping)
{
        if (!actor::is_player(&actor_bumping)) {
                return;
        }

        map::memorize_terrain_at(m_pos);
        map::update_vision();

        if (!map::g_seen.at(m_pos)) {
                msg_log::add("There is a cabinet here.");

                return;
        }

        if (m_burn_state == BurnState::burning) {
                msg_log::add("The cabinet is on fire.");

                return;
        }

        if (m_item_container.is_empty() && m_is_open) {
                msg_log::add("The cabinet is empty.");

                return;
        }

        if (m_is_open) {
                Snd snd(
                        "",
                        audio::SfxId::bookshelf_rummage,
                        IgnoreMsgIfOriginSeen::yes,
                        m_pos,
                        &actor_bumping,
                        SndVol::high,
                        AlertsMon::no);

                snd.run();
                player_loot();
        }
        else {
                open(map::g_player);
        }

        map::memorize_terrain_at(m_pos);
        map::update_vision();

        game_time::tick();
}

void Cabinet::player_loot()
{
        msg_log::add("I search the cabinet.");

        if (m_item_container.is_empty()) {
                msg_log::add("There is nothing of value inside.");
        }
        else {
                m_item_container.open(m_pos, map::g_player);
        }
}

DidOpen Cabinet::open(actor::Actor* const actor_opening)
{
        (void)actor_opening;

        if (m_is_open) {
                return DidOpen::no;
        }
        else {
                // Was not already open
                Snd snd(
                        "",
                        audio::SfxId::cabinet_open,
                        IgnoreMsgIfOriginSeen::yes,
                        m_pos,
                        actor_opening,
                        SndVol::high,
                        AlertsMon::no);

                snd.run();
                m_is_open = true;

                if (map::g_seen.at(m_pos)) {
                        msg_log::add("The cabinet opens.");
                }

                return DidOpen::yes;
        }
}

std::string Cabinet::name(const Article article) const
{
        std::string str = (article == Article::a) ? "a " : "the ";

        if (m_burn_state == BurnState::burning) {
                str += "burning ";
        }

        return str + "cabinet";
}

gfx::TileId Cabinet::tile() const
{
        if (m_is_open) {
                return gfx::TileId::cabinet_open;
        }
        else {
                return gfx::TileId::cabinet_closed;
        }
}

Color Cabinet::color_default() const
{
        return colors::dark_brown();
}

// -----------------------------------------------------------------------------
// Bookshelf
// -----------------------------------------------------------------------------
Bookshelf::Bookshelf(const P& p, const TerrainData* const data) :
        Terrain(p, data),
        m_is_looted(false)
{
        // Contained items
        const int nr_items_min =
                rnd::coin_toss()
                ? 0
                : 1;

        int nr_items_max = 1;

        int incr_max_items_one_in = 12;

        if (player_bon::has_trait(Trait::treasure_hunter)) {
                incr_max_items_one_in /= 2;
        }

        if (rnd::one_in(incr_max_items_one_in)) {
                ++nr_items_max;
        }

        m_item_container.init(
                terrain::Id::bookshelf,
                rnd::range(nr_items_min, nr_items_max));
}

void Bookshelf::hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const P& from_pos,
        int dmg)
{
        (void)actor;
        (void)from_pos;
        (void)dmg;

        switch (dmg_type) {
        case DmgType::explosion:
        case DmgType::pure:
                if (map::g_seen.at(m_pos)) {
                        msg_log::add("The bookshelf is destroyed.");
                }

                map::update_terrain(make(Id::rubble_low, m_pos));
                map::update_vision();
                break;

        case DmgType::fire:
                m_item_container.clear();
                m_is_looted = true;
                try_start_burning(Verbose::yes);
                break;

        default:
                break;
        }
}

WasDestroyed Bookshelf::on_finished_burning()
{
        if (map::g_seen.at(m_pos)) {
                msg_log::add("The bookshelf burns down.");
        }

        auto* const rubble = make(Id::rubble_low, m_pos);

        rubble->m_burn_state = BurnState::has_burned;

        map::update_terrain(rubble);

        map::update_vision();

        return WasDestroyed::yes;
}

void Bookshelf::bump(actor::Actor& actor_bumping)
{
        if (!actor::is_player(&actor_bumping)) {
                return;
        }

        map::memorize_terrain_at(m_pos);
        map::update_vision();

        if (!map::g_seen.at(m_pos)) {
                msg_log::add("There is a bookshelf here.");

                return;
        }

        if (m_burn_state == BurnState::burning) {
                msg_log::add("The bookshelf is on fire.");

                return;
        }

        if (m_item_container.is_empty() && m_is_looted) {
                msg_log::add("The bookshelf is empty.");

                return;
        }

        if (!m_is_looted) {
                Snd snd(
                        "",
                        audio::SfxId::bookshelf_rummage,
                        IgnoreMsgIfOriginSeen::yes,
                        m_pos,
                        &actor_bumping,
                        SndVol::high,
                        AlertsMon::no);

                snd.run();
                player_loot();
        }

        map::memorize_terrain_at(m_pos);
        map::update_vision();

        game_time::tick();
}

void Bookshelf::player_loot()
{
        msg_log::add(
                "I search the bookshelf.",
                colors::text(),
                MsgInterruptPlayer::no,
                MorePromptOnMsg::yes);

        m_is_looted = true;

        if (m_item_container.is_empty()) {
                msg_log::add("There is nothing of interest.");
        }
        else {
                m_item_container.open(m_pos, map::g_player);
        }
}

std::string Bookshelf::name(const Article article) const
{
        std::string str = (article == Article::a) ? "a " : "the ";

        if (m_burn_state == BurnState::burning) {
                str += "burning ";
        }

        return str + "bookshelf";
}

gfx::TileId Bookshelf::tile() const
{
        return (
                m_is_looted
                        ? gfx::TileId::bookshelf_empty
                        : gfx::TileId::bookshelf_full);
}

Color Bookshelf::color_default() const
{
        return colors::dark_brown();
}

// -----------------------------------------------------------------------------
// AlchemistBench
// -----------------------------------------------------------------------------
AlchemistBench::AlchemistBench(const P& p, const TerrainData* const data) :
        Terrain(p, data),
        m_is_looted(false)
{
        // Contained items
        const int nr_items_min =
                rnd::coin_toss()
                ? 0
                : 1;

        int nr_items_max = 1;

        int incr_max_items_one_in = 12;

        if (player_bon::has_trait(Trait::treasure_hunter)) {
                incr_max_items_one_in /= 2;
        }

        if (rnd::one_in(incr_max_items_one_in)) {
                ++nr_items_max;
        }

        m_item_container.init(
                terrain::Id::alchemist_bench,
                rnd::range(nr_items_min, nr_items_max));
}

void AlchemistBench::hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const P& from_pos,
        int dmg)
{
        (void)actor;
        (void)from_pos;
        (void)dmg;

        switch (dmg_type) {
        case DmgType::explosion:
        case DmgType::pure:
                if (map::g_seen.at(m_pos)) {
                        msg_log::add("The alchemist's workbench is destroyed.");
                }

                map::update_terrain(make(Id::rubble_low, m_pos));
                map::update_vision();
                break;

        case DmgType::fire:
                m_item_container.clear();
                m_is_looted = true;
                try_start_burning(Verbose::yes);
                break;

        default:
                break;
        }
}

WasDestroyed AlchemistBench::on_finished_burning()
{
        if (map::g_seen.at(m_pos)) {
                msg_log::add("The alchemist's workbench burns down.");
        }

        auto* const rubble = make(Id::rubble_low, m_pos);

        rubble->m_burn_state = BurnState::has_burned;

        map::update_terrain(rubble);

        map::update_vision();

        return WasDestroyed::yes;
}

void AlchemistBench::bump(actor::Actor& actor_bumping)
{
        if (!actor::is_player(&actor_bumping)) {
                return;
        }

        map::memorize_terrain_at(m_pos);
        map::update_vision();

        if (!map::g_seen.at(m_pos)) {
                msg_log::add("There is an alchemist's workbench here.");

                return;
        }

        if (m_burn_state == BurnState::burning) {
                msg_log::add("The alchemist's workbench is on fire.");

                return;
        }

        if (m_item_container.is_empty() && m_is_looted) {
                msg_log::add("The alchemist's workbench is empty.");

                return;
        }

        if (!m_is_looted) {
                Snd snd(
                        "",
                        audio::SfxId::alchemy_rummage,
                        IgnoreMsgIfOriginSeen::yes,
                        m_pos,
                        &actor_bumping,
                        SndVol::high,
                        AlertsMon::no);

                snd.run();
                player_loot();
        }

        map::memorize_terrain_at(m_pos);
        map::update_vision();

        game_time::tick();
}

void AlchemistBench::player_loot()
{
        msg_log::add(
                "I search the alchemist's workbench.",
                colors::text(),
                MsgInterruptPlayer::no,
                MorePromptOnMsg::yes);

        m_is_looted = true;

        if (m_item_container.is_empty()) {
                msg_log::add("There is nothing of interest.");
        }
        else {
                m_item_container.open(m_pos, map::g_player);
        }
}

std::string AlchemistBench::name(const Article article) const
{
        std::string a = (article == Article::a) ? "an " : "the ";

        std::string mod;

        if (m_burn_state == BurnState::burning) {
                if (article == Article::a) {
                        a = "a ";
                }

                mod = "burning ";
        }

        return a + mod + "alchemist's workbench";
}

gfx::TileId AlchemistBench::tile() const
{
        return (
                m_is_looted
                        ? gfx::TileId::alchemist_bench_empty
                        : gfx::TileId::alchemist_bench_full);
}

Color AlchemistBench::color_default() const
{
        return colors::brown();
}

// -----------------------------------------------------------------------------
// Cocoon
// -----------------------------------------------------------------------------
Cocoon::Cocoon(const P& p, const TerrainData* const data) :
        Terrain(p, data),
        m_is_trapped(rnd::fraction(6, 10)),
        m_is_open(false)
{
        if (m_is_trapped) {
                m_item_container.init(terrain::Id::cocoon, 0);
        }
        else {
                const bool is_treasure_hunter =
                        player_bon::has_trait(Trait::treasure_hunter);

                const Fraction fraction_empty(6, 10);

                const int nr_items_min =
                        fraction_empty.roll()
                        ? 0
                        : 1;

                const int nr_items_max =
                        nr_items_min +
                        (is_treasure_hunter
                                 ? 1
                                 : 0);

                m_item_container.init(
                        terrain::Id::cocoon,
                        rnd::range(nr_items_min, nr_items_max));
        }
}

void Cocoon::hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const P& from_pos,
        int dmg)
{
        (void)actor;
        (void)from_pos;
        (void)dmg;

        switch (dmg_type) {
        case DmgType::explosion:
        case DmgType::pure:
                if (map::g_seen.at(m_pos)) {
                        msg_log::add("The cocoon is destroyed.");
                }

                map::update_terrain(make(Id::rubble_low, m_pos));
                map::update_vision();
                break;

        case DmgType::fire:
                m_item_container.clear();
                try_start_burning(Verbose::yes);
                break;

        default:
                break;
        }
}

WasDestroyed Cocoon::on_finished_burning()
{
        if (map::g_seen.at(m_pos)) {
                msg_log::add("The cocoon burns down.");
        }

        auto* const rubble = make(Id::rubble_low, m_pos);

        rubble->m_burn_state = BurnState::has_burned;

        map::update_terrain(rubble);

        map::update_vision();

        return WasDestroyed::yes;
}

void Cocoon::bump(actor::Actor& actor_bumping)
{
        if (!actor::is_player(&actor_bumping)) {
                return;
        }

        map::memorize_terrain_at(m_pos);
        map::update_vision();

        if (!map::g_seen.at(m_pos)) {
                msg_log::add("There is a cocoon here.");

                return;
        }

        if (m_burn_state == BurnState::burning) {
                msg_log::add("The cocoon is on fire.");

                return;
        }

        if (m_item_container.is_empty() && m_is_open) {
                msg_log::add("The cocoon is empty.");

                return;
        }

        if (insanity::has_sympt(InsSymptId::phobia_spider)) {
                map::g_player->m_properties.apply(prop::make(prop::Id::terrified));
        }

        if (m_is_open) {
                player_loot();
        }
        else {
                open(map::g_player);
        }

        map::memorize_terrain_at(m_pos);
        map::update_vision();

        game_time::tick();
}

DidTriggerTrap Cocoon::trigger_trap(actor::Actor* const actor)
{
        (void)actor;

        if (!m_is_trapped) {
                return DidTriggerTrap::no;
        }

        const int rnd = rnd::range(1, 100);

        if (rnd < 15) {
                // A dead body

                if (player_bon::is_bg(Bg::ghoul)) {
                        m_is_trapped = false;

                        return DidTriggerTrap::no;
                }

                msg_log::add("There is a half-dissolved human body inside!");

                map::g_player->incr_shock(12.0, ShockSrc::misc);

                m_is_trapped = false;

                return DidTriggerTrap::yes;
        }
        else if (rnd < 50) {
                // Spiders
                TRACE << "Attempting to spawn spiders" << std::endl;
                std::vector<std::string> spawn_bucket;

                for (const auto& it : actor::g_data) {
                        const actor::ActorData& d = it.second;

                        if (d.is_spider &&
                            (d.actor_size == actor::Size::floor) &&
                            d.is_auto_spawn_allowed &&
                            !d.is_unique) {
                                spawn_bucket.push_back(d.id);
                        }
                }

                const int nr_candidates = (int)spawn_bucket.size();

                if (nr_candidates > 0) {
                        TRACE << "Spawn candidates found, attempting to place"
                              << std::endl;

                        msg_log::add("There are spiders inside!");

                        const auto nr_spiders =
                                (size_t)rnd::range(2, 5);

                        const auto idx =
                                rnd::range(0, nr_candidates - 1);

                        const auto actor_id_to_summon =
                                spawn_bucket[idx];

                        actor::spawn(
                                m_pos,
                                {nr_spiders, actor_id_to_summon},
                                map::rect())
                                .make_aware_of_player();

                        m_is_trapped = false;

                        return DidTriggerTrap::yes;
                }
        }

        return DidTriggerTrap::no;
}

void Cocoon::player_loot()
{
        msg_log::add("I search the Cocoon.");

        if (m_item_container.is_empty()) {
                msg_log::add("It is empty.");
        }
        else {
                m_item_container.open(m_pos, map::g_player);
        }
}

DidOpen Cocoon::open(actor::Actor* const actor_opening)
{
        if (m_is_open) {
                return DidOpen::no;
        }
        else {
                // Was not already open
                m_is_open = true;

                if (map::g_seen.at(m_pos)) {
                        msg_log::add("The cocoon opens.");
                }

                trigger_trap(actor_opening);

                return DidOpen::yes;
        }
}

std::string Cocoon::name(const Article article) const
{
        std::string str = (article == Article::a) ? "a " : "the ";

        if (m_burn_state == BurnState::burning) {
                str += "burning ";
        }

        return str + "cocoon";
}

gfx::TileId Cocoon::tile() const
{
        return (
                m_is_open
                        ? gfx::TileId::cocoon_open
                        : gfx::TileId::cocoon_closed);
}

Color Cocoon::color_default() const
{
        return colors::white();
}

}  // namespace terrain

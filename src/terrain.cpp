// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "terrain.hpp"

#include <string>

#include "actor_factory.hpp"
#include "actor_hit.hpp"
#include "actor_mon.hpp"
#include "actor_player.hpp"
#include "actor_see.hpp"
#include "common_text.hpp"
#include "drop.hpp"
#include "explosion.hpp"
#include "game.hpp"
#include "game_time.hpp"
#include "hints.hpp"
#include "init.hpp"
#include "io.hpp"
#include "item_factory.hpp"
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
#include "saving.hpp"
#include "sound.hpp"
#include "terrain_dmg.hpp"
#include "terrain_mob.hpp"
#include "text_format.hpp"
#include "wham.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static void scorch_actor(actor::Actor& actor)
{
        if (actor.is_player())
        {
                msg_log::add(
                        "I am scorched by flames.",
                        colors::msg_bad());
        }
        else if (actor::can_player_see_actor(actor))
        {
                const std::string name_the =
                        text_format::first_to_upper(
                                actor.name_the());

                msg_log::add(
                        name_the +
                                " is scorched by flames.",
                        colors::msg_good());
        }

        actor::hit(actor, 1, DmgType::fire);
}

// -----------------------------------------------------------------------------
// terrain
// -----------------------------------------------------------------------------
namespace terrain
{
void Terrain::bump(actor::Actor& actor_bumping)
{
        if (!can_move(actor_bumping) && actor_bumping.is_player())
        {
                if (map::is_pos_seen_by_player(m_pos))
                {
                        msg_log::add(data().msg_on_player_blocked);
                }
                else
                {
                        msg_log::add(data().msg_on_player_blocked_blind);
                }
        }
}

AllowAction Terrain::pre_bump(actor::Actor& actor_bumping)
{
        if (!actor_bumping.is_player() ||
            actor_bumping.m_properties.has(PropId::confused))
        {
                return AllowAction::yes;
        }

        if ((m_burn_state == BurnState::burning) &&
            !actor_bumping.m_properties.has(PropId::r_fire) &&
            !actor_bumping.m_properties.has(PropId::flying) &&
            can_move(actor_bumping) &&
            map::g_seen.at(m_pos))
        {
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

                return (query_result == BinaryAnswer::no)
                        ? AllowAction::no
                        : AllowAction::yes;
        }

        return AllowAction::yes;
}

void Terrain::on_new_turn()
{
        if (m_nr_turns_color_corrupted > 0)
        {
                --m_nr_turns_color_corrupted;
        }

        if ((m_burn_state == BurnState::burning) &&
            !m_started_burning_this_turn)
        {
                clear_gore();

                // TODO: Hit dead actors

                // Hit actor standing on terrain
                auto* actor = map::first_actor_at_pos(m_pos);

                if (actor)
                {
                        // Occasionally try to set actor on fire, otherwise just
                        // do small fire damage
                        if (rnd::one_in(4))
                        {
                                actor->m_properties.apply(new PropBurning());
                        }
                        else
                        {
                                scorch_actor(*actor);
                        }
                }

                // Finished burning?
                int finish_burning_one_in_n = 1;
                int hit_adjacent_one_in_n = 1;

                switch (matl())
                {
                case Matl::fluid:
                case Matl::empty:
                        finish_burning_one_in_n = 1;
                        hit_adjacent_one_in_n = 1;
                        break;

                case Matl::stone:
                        finish_burning_one_in_n = 14;
                        hit_adjacent_one_in_n = 10;
                        break;

                case Matl::metal:
                        finish_burning_one_in_n = 14;
                        hit_adjacent_one_in_n = 10;
                        break;

                case Matl::plant:
                        finish_burning_one_in_n = 30;
                        hit_adjacent_one_in_n = 10;
                        break;

                case Matl::wood:
                        finish_burning_one_in_n = 40;
                        hit_adjacent_one_in_n = 10;
                        break;

                case Matl::cloth:
                        finish_burning_one_in_n = 20;
                        hit_adjacent_one_in_n = 8;
                        break;
                }

                if (rnd::one_in(finish_burning_one_in_n))
                {
                        m_burn_state = BurnState::has_burned;

                        if (on_finished_burning() == WasDestroyed::yes)
                        {
                                return;
                        }
                }

                // Hit actors and adjacent terrains?
                if (rnd::one_in(hit_adjacent_one_in_n))
                {
                        actor = map::first_actor_at_pos(m_pos);

                        if (actor)
                        {
                                scorch_actor(*actor);
                        }

                        const P p(dir_utils::rnd_adj_pos(m_pos, false));

                        if (map::is_pos_inside_map(p))
                        {
                                auto* const terrain = map::g_terrain.at(p);

                                terrain->hit(DmgType::fire, nullptr);

                                if (terrain->is_burning())
                                {
                                        terrain->m_started_burning_this_turn = true;

                                        if (map::g_player->m_pos == p)
                                        {
                                                msg_log::add(
                                                        "Fire has spread here!",
                                                        colors::msg_note(),
                                                        MsgInterruptPlayer::yes,
                                                        MorePromptOnMsg::yes);
                                        }
                                }
                        }
                }

                // Create smoke?
                if (rnd::one_in(20))
                {
                        const P p(dir_utils::rnd_adj_pos(m_pos, true));

                        if (map::is_pos_inside_map(p) &&
                            !map_parsers::BlocksProjectiles().run(p))
                        {
                                game_time::add_mob(new Smoke(p, 10));
                        }
                }
        }

        // Run specialized new turn actions
        on_new_turn_hook();
}

void Terrain::try_start_burning(const Verbose verbose)
{
        clear_gore();

        if ((m_burn_state == BurnState::not_burned) ||
            ((m_burn_state == BurnState::has_burned) &&
             rnd::one_in(3)))
        {
                if (map::is_pos_seen_by_player(m_pos) &&
                    (verbose == Verbose::yes))
                {
                        std::string str = name(Article::the) + " catches fire.";

                        str[0] = toupper(str[0]);

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

void Terrain::hit(
        const DmgType dmg_type,
        actor::Actor* actor,
        const int dmg)
{
        bool is_terrain_hit = true;

        if (actor == map::g_player)
        {
                switch (dmg_type)
                {
                case DmgType::kicking:
                case DmgType::blunt:
                case DmgType::slashing:
                {
                        const bool is_blocking =
                                !is_walkable() &&
                                (id() != terrain::Id::stairs) &&
                                (id() != terrain::Id::liquid_deep);

                        if (is_blocking)
                        {
                                if (dmg_type == DmgType::kicking)
                                {
                                        const std::string terrain_name =
                                                map::g_seen.at(m_pos)
                                                ? name(Article::the)
                                                : "something";

                                        msg_log::add(
                                                "I kick " +
                                                terrain_name +
                                                "!");

                                        wham::try_sprain_player();
                                }
                                else
                                {
                                        // Not kicking
                                        msg_log::add("*WHAM!*");
                                }
                        }
                        else
                        {
                                // The terrain is not blocking
                                is_terrain_hit = false;

                                msg_log::add("*Whoosh!*");

                                audio::play(audio::SfxId::miss_medium);
                        }
                }
                break;

                default:
                {
                }
                break;
                }
        }

        if (is_terrain_hit)
        {
                on_hit(dmg_type, actor, dmg);
        }
}

int Terrain::shock_when_adj() const
{
        int shock = base_shock_when_adj();

        if (is_corrupted_color())
        {
                shock += 6;
        }
        else if (has_gore())
        {
                shock += 3;
        }
        else if (is_bloody())
        {
                shock += 1;
        }

        return shock;
}

int Terrain::base_shock_when_adj() const
{
        return data().shock_when_adjacent;
}

void Terrain::try_make_bloody()
{
        if (can_have_blood())
        {
                m_is_bloody = true;
        }
}

bool Terrain::is_bloody() const
{
        return m_is_bloody;
}

void Terrain::try_put_gore()
{
        if (!can_have_gore())
        {
                return;
        }

        const int roll_character = rnd::range(1, 4);

        switch (roll_character)
        {
        case 1:
                m_gore_character = ',';
                break;

        case 2:
                m_gore_character = '`';
                break;

        case 3:
                m_gore_character = 39;
                break;

        case 4:
                m_gore_character = ';';
                break;

        default:
                ASSERT(false);
                break;
        }

        const int roll_tile = rnd::range(1, 8);

        switch (roll_tile)
        {
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
        if (id() == terrain::Id::chasm)
        {
                return;
        }

        m_nr_turns_color_corrupted = rnd::range(200, 220);
}

bool Terrain::is_corrupted_color() const
{
        return (m_nr_turns_color_corrupted > 0);
}

Color Terrain::color() const
{
        if (m_burn_state == BurnState::burning)
        {
                return colors::orange();
        }
        else
        {
                // Not burning
                if (is_corrupted_color())
                {
                        Color color = colors::light_magenta();

                        color.set_rgb(
                                rnd::range(40, 255),
                                rnd::range(40, 255),
                                rnd::range(40, 255));

                        return color;
                }
                else if (m_is_bloody)
                {
                        return colors::light_red();
                }
                else
                {
                        return (m_burn_state == BurnState::not_burned)
                                ? color_default()
                                : colors::dark_gray();
                }
        }
}

Color Terrain::color_bg() const
{
        switch (m_burn_state)
        {
        case BurnState::not_burned:
        case BurnState::has_burned:
                return color_bg_default();

        case BurnState::burning:
                auto color = Color((uint8_t)rnd::range(32, 255), 0, 0);

                return color;
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
        if (m_burn_state == BurnState::burning)
        {
                for (const P& d : dir_utils::g_dir_list_w_center)
                {
                        const P p(m_pos + d);

                        if (map::is_pos_inside_map(p))
                        {
                                light.at(p) = true;
                        }
                }
        }

        add_light_hook(light);
}

Color Terrain::color_bg_default() const
{
        return colors::black();
}

// -----------------------------------------------------------------------------
// Floor
// -----------------------------------------------------------------------------
Floor::Floor(const P& p) :
        Terrain(p),
        m_type(FloorType::common) {}

void Floor::on_hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const int dmg)
{
        (void)actor;
        (void)dmg;

        switch (dmg_type)
        {
        case DmgType::fire:
                if (rnd::one_in(3))
                {
                        try_start_burning(Verbose::no);
                }
                break;

        default:
                break;
        }
}

gfx::TileId Floor::tile() const
{
        return m_burn_state == BurnState::has_burned
                ? gfx::TileId::scorched_ground
                : data().tile;
}

std::string Floor::name(const Article article) const
{
        std::string ret = (article == Article::a) ? "" : "the ";

        if (m_burn_state == BurnState::burning)
        {
                ret += "flames";
        }
        else
        {
                if (m_burn_state == BurnState::has_burned)
                {
                        ret += "scorched ";
                }

                switch (m_type)
                {
                case FloorType::common:
                        ret += "stone floor";
                        break;

                case FloorType::cave:
                        ret += "cavern floor";
                        break;

                case FloorType::stone_path:
                        if (article == Article::a)
                        {
                                ret.insert(0, "a ");
                        }

                        ret += "stone path";
                        break;
                }
        }

        return ret;
}

Color Floor::color_default() const
{
        return colors::white();
}

// -----------------------------------------------------------------------------
// Wall
// -----------------------------------------------------------------------------
Wall::Wall(const P& p) :
        Terrain(p),
        m_type(WallType::common),
        m_is_mossy(false) {}

void Wall::on_hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const int dmg)
{
        (void)dmg;
        (void)actor;

        switch (dmg_type)
        {
        case DmgType::pure:
        case DmgType::explosion:
        {
                destr_all_adj_doors(m_pos);

                if ((dmg_type == DmgType::pure) || rnd::coin_toss())
                {
                        destr_stone_wall(m_pos);
                }
                else
                {
                        map::put(new RubbleHigh(m_pos));
                }

                map::update_vision();
        }
        break;

        default:
        {
        }
        break;
        }
}

bool Wall::is_wall_front_tile(const gfx::TileId tile)
{
        switch (tile)
        {
        case gfx::TileId::wall_front:
        case gfx::TileId::wall_front_alt1:
        case gfx::TileId::wall_front_alt2:
        case gfx::TileId::cave_wall_front:
        case gfx::TileId::egypt_wall_front:
                return true;

        default:
                return false;
        }
}

bool Wall::is_wall_top_tile(const gfx::TileId tile)
{
        switch (tile)
        {
        case gfx::TileId::wall_top:
        case gfx::TileId::cave_wall_top:
        case gfx::TileId::egypt_wall_top:
        case gfx::TileId::rubble_high:
                return true;

        default:
                return false;
        }
}

std::string Wall::name(const Article article) const
{
        std::string ret = (article == Article::a) ? "a " : "the ";

        if (m_is_mossy)
        {
                ret += "moss-grown ";
        }

        switch (m_type)
        {
        case WallType::common:
        case WallType::common_alt:
        case WallType::leng_monestary:
        case WallType::egypt:
                ret += "stone wall";
                break;

        case WallType::cave:
                ret += "cavern wall";
                break;

        case WallType::cliff:
                ret += "cliff";
                break;
        }

        return ret;
}

Color Wall::color_default() const
{
        if (m_is_mossy)
        {
                return colors::dark_green();
        }

        switch (m_type)
        {
        case WallType::cliff:
                return colors::dark_gray();

        case WallType::egypt:
        case WallType::cave:
                return colors::gray_brown();

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

char Wall::character() const
{
        return config::is_text_mode_wall_full_square()
                ? 10
                : '#';
}

gfx::TileId Wall::front_wall_tile() const
{
        if (config::is_tiles_wall_full_square())
        {
                switch (m_type)
                {
                case WallType::common:
                case WallType::common_alt:
                        return gfx::TileId::wall_top;

                case WallType::cliff:
                case WallType::cave:
                        return gfx::TileId::cave_wall_top;

                case WallType::leng_monestary:
                case WallType::egypt:
                        return gfx::TileId::egypt_wall_top;
                }
        }
        else
        {
                switch (m_type)
                {
                case WallType::common:
                        return gfx::TileId::wall_front;

                case WallType::common_alt:
                        return gfx::TileId::wall_front_alt1;

                case WallType::cliff:
                case WallType::cave:
                        return gfx::TileId::cave_wall_front;

                case WallType::leng_monestary:
                case WallType::egypt:
                        return gfx::TileId::egypt_wall_front;
                }
        }

        ASSERT(false && "Failed to set front wall tile");
        return gfx::TileId::END;
}

gfx::TileId Wall::top_wall_tile() const
{
        switch (m_type)
        {
        case WallType::common:
        case WallType::common_alt:
                return gfx::TileId::wall_top;

        case WallType::cliff:
        case WallType::cave:
                return gfx::TileId::cave_wall_top;

        case WallType::leng_monestary:
        case WallType::egypt:
                return gfx::TileId::egypt_wall_top;
        }

        ASSERT(false && "Failed to set top wall tile");
        return gfx::TileId::END;
}

void Wall::set_rnd_common_wall()
{
        m_type =
                (rnd::one_in(6))
                ? WallType::common_alt
                : WallType::common;
}

void Wall::set_moss_grown()
{
        m_is_mossy = true;
}

// -----------------------------------------------------------------------------
// High rubble
// -----------------------------------------------------------------------------
RubbleHigh::RubbleHigh(const P& p) :
        Terrain(p) {}

void RubbleHigh::on_hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const int dmg)
{
        (void)dmg;
        (void)actor;

        switch (dmg_type)
        {
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
RubbleLow::RubbleLow(const P& p) :
        Terrain(p) {}

void RubbleLow::on_hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const int dmg)
{
        (void)dmg;
        (void)actor;

        switch (dmg_type)
        {
        case DmgType::fire:
                try_start_burning(Verbose::no);
                break;

        default:
                break;
        }
}

std::string RubbleLow::name(const Article article) const
{
        std::string ret;

        if (article == Article::the)
        {
                ret += "the ";
        }

        if (m_burn_state == BurnState::burning)
        {
                ret += "burning ";
        }

        return ret + "rubble";
}

Color RubbleLow::color_default() const
{
        // Return the wall color of the current map
        return map::g_wall_color;
}

// -----------------------------------------------------------------------------
// Bones
// -----------------------------------------------------------------------------
Bones::Bones(const P& p) :
        Terrain(p) {}

void Bones::on_hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const int dmg)
{
        (void)dmg;
        (void)dmg_type;
        (void)actor;
}

std::string Bones::name(const Article article) const
{
        std::string ret;

        if (article == Article::the)
        {
                ret += "the ";
        }

        return ret + "bones";
}

Color Bones::color_default() const
{
        return colors::dark_gray();
}

// -----------------------------------------------------------------------------
// Grave
// -----------------------------------------------------------------------------
GraveStone::GraveStone(const P& p) :
        Terrain(p) {}

void GraveStone::on_hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const int dmg)
{
        (void)dmg;
        (void)dmg_type;
        (void)actor;
}

void GraveStone::bump(actor::Actor& actor_bumping)
{
        if (actor_bumping.is_player())
        {
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
ChurchBench::ChurchBench(const P& p) :
        Terrain(p) {}

void ChurchBench::on_hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const int dmg)
{
        (void)actor;
        (void)dmg;

        switch (dmg_type)
        {
        case DmgType::explosion:
        case DmgType::pure:
                if (map::is_pos_seen_by_player(m_pos))
                {
                        msg_log::add("The church bench is destroyed.");
                }

                map::put(new RubbleLow(m_pos));
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
Statue::Statue(const P& p) :
        Terrain(p),
        m_type(rnd::one_in(8) ? StatueType::ghoul : StatueType::common),
        m_player_bg(Bg::END)
{
}

int Statue::base_shock_when_adj() const
{
        const bool is_ghoul_statue =
                (m_type == StatueType::ghoul) ||
                (m_player_bg == Bg::ghoul);

        if (is_ghoul_statue && !player_bon::is_bg(Bg::ghoul))
        {
                return 10;
        }
        else
        {
                return 0;
        }
}

void Statue::topple(
        const Dir direction,
        actor::Actor* const actor_toppling)
{
        const auto alerts_mon =
                (actor_toppling == map::g_player)
                ? AlertsMon::yes
                : AlertsMon::no;

        if (map::g_seen.at(m_pos))
        {
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

        map::put(new RubbleLow(m_pos));

        // NOTE: This object is now deleted!

        actor::Actor* const actor_behind =
                map::first_actor_at_pos(dst_pos);

        if (actor_behind &&
            actor_behind->is_alive() &&
            !actor_behind->m_properties.has(PropId::ethereal))
        {
                if (actor_behind->is_player())
                {
                        msg_log::add("It falls on me!");
                }
                else
                {
                        // Monster is hit
                        const bool is_player_seeing_actor =
                                actor::can_player_see_actor(
                                        *actor_behind);

                        if (is_player_seeing_actor)
                        {
                                msg_log::add(
                                        "It falls on " +
                                        actor_behind->name_a() +
                                        ".");
                        }
                }

                actor::hit(
                        *actor_behind,
                        rnd::range(3, 6),
                        DmgType::blunt);

                if (actor_behind->is_alive())
                {
                        auto* const paralyzed =
                                property_factory::make(PropId::paralyzed);

                        paralyzed->set_duration(rnd::range(2, 3));

                        actor_behind->m_properties.apply(paralyzed);
                }
        }

        const auto terrain_id = map::g_terrain.at(dst_pos)->id();

        // NOTE: This is kinda hacky, but the rubble is mostly just for
        // decoration anyway, so it doesn't really matter.
        if (terrain_id == terrain::Id::floor ||
            terrain_id == terrain::Id::grass ||
            terrain_id == terrain::Id::carpet)
        {
                map::put(new RubbleLow(dst_pos));
        }

        map::update_vision();
}

void Statue::on_hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const int dmg)
{
        (void)dmg;

        switch (dmg_type)
        {
        case DmgType::kicking:
        {
                ASSERT(actor);

                if (actor->m_properties.has(PropId::weakened))
                {
                        msg_log::add("It wiggles a bit.");

                        return;
                }

                const auto direction = dir_utils::dir(m_pos - actor->m_pos);

                // NOTE: This call deletes the object!
                topple(direction, actor);
        }
        break;

        case DmgType::explosion:
        case DmgType::pure:
        {
                map::put(new RubbleLow(m_pos));
                map::update_vision();
        }
        break;

        default:
        {
        }
        break;
        }
}

void Statue::bump(actor::Actor& actor_bumping)
{
        if ((m_inscr != "") && actor_bumping.is_player())
        {
                msg_log::add(m_inscr);
        }
        else
        {
                Terrain::bump(actor_bumping);
        }
}

std::string Statue::name(const Article article) const
{
        std::string ret = (article == Article::a) ? "a " : "the ";

        switch (m_type)
        {
        case StatueType::common:
                ret += "statue";
                break;

        case StatueType::ghoul:
                ret += "statue of a ghoulish creature";
                break;
        }

        if (m_inscr != "")
        {
                ret += " (\"" + m_inscr + "\")";
        }

        return ret;
}

gfx::TileId Statue::tile() const
{
        if (m_player_bg == Bg::ghoul)
        {
                return gfx::TileId::ghoul;
        }
        else if (m_player_bg != Bg::END)
        {
                return gfx::TileId::player_melee;
        }
        else if (m_type == StatueType::ghoul)
        {
                return gfx::TileId::ghoul;
        }
        else
        {
                return gfx::TileId::witch_or_warlock;
        }
}

Color Statue::color_default() const
{
        if (m_player_bg == Bg::END)
        {
                return colors::white();
        }
        else
        {
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
Stalagmite::Stalagmite(const P& p) :
        Terrain(p) {}

void Stalagmite::on_hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const int dmg)
{
        (void)dmg;
        (void)actor;

        switch (dmg_type)
        {
        case DmgType::pure:
        case DmgType::explosion:
                map::put(new RubbleLow(m_pos));
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
Stairs::Stairs(const P& p) :
        Terrain(p) {}

void Stairs::on_hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const int dmg)
{
        (void)dmg;
        (void)dmg_type;
        (void)actor;
}

void Stairs::on_new_turn_hook()
{
        ASSERT(!map::g_items.at(m_pos));
}

void Stairs::bump(actor::Actor& actor_bumping)
{
        if (actor_bumping.is_player())
        {
                int choice = 0;

                popup::Popup(popup::AddToMsgHistory::no)
                        .set_title("A staircase leading downwards")
                        .set_menu(
                                {"(D)escend", "(S)ave and quit", "(C)ancel"},
                                {'d', 's', 'c'},
                                &choice)
                        .run();

                switch (choice)
                {
                case 0:
                        map::g_player->m_pos = m_pos;

                        if (is_fake())
                        {
                                // NOTE: This destroys this object
                                player_use_fake_stairs();

                                return;
                        }

                        msg_log::clear();

                        msg_log::add("I descend the stairs.");

                        // Always auto-save the game when descending
                        // NOTE: We descend one dlvl when loading the game, so
                        // auto-saving should be done BEFORE descending here
                        saving::save_game();

                        map_travel::go_to_nxt();
                        break;

                case 1:
                        map::g_player->m_pos = m_pos;

                        if (is_fake())
                        {
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
}

void Stairs::player_use_fake_stairs()
{
        const auto msg =
                "As I descend the stairs and observe my surroundings, to my "
                "great bewilderment I realize that I have stepped out into "
                "the very same ground from which I started my downward climb! "
                "Turning around, the stairs are nowhere to be found.";

        popup::Popup(popup::AddToMsgHistory::yes)
                .set_msg(msg)
                .run();

        map::put(new Floor(m_pos));

        auto* prop = new PropConfused();

        prop->set_duration(8);

        map::g_player->m_properties.apply(prop);

        map::g_player->incr_shock(ShockLvl::unsettling, ShockSrc::misc);
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
        return (m_axis == Axis::hor)
                ? gfx::TileId::hangbridge_hor
                : gfx::TileId::hangbridge_ver;
}

void Bridge::on_hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const int dmg)
{
        (void)dmg;
        (void)dmg_type;
        (void)actor;
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
LiquidShallow::LiquidShallow(const P& p) :
        Terrain(p),
        m_type(LiquidType::water) {}

void LiquidShallow::on_hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const int dmg)
{
        (void)dmg;
        (void)dmg_type;
        (void)actor;
}

void LiquidShallow::bump(actor::Actor& actor_bumping)
{
        if (actor_bumping.m_properties.has(PropId::ethereal) ||
            actor_bumping.m_properties.has(PropId::flying) ||
            actor_bumping.m_data->is_amphibian)
        {
                return;
        }

        actor_bumping.m_properties.apply(
                property_factory::make(PropId::waiting));

        if (actor_bumping.is_player())
        {
                std::string type_str;

                switch (m_type)
                {
                case LiquidType::water:
                case LiquidType::magic_water:
                        type_str = "water";
                        break;

                case LiquidType::mud:
                        type_str = "mud";
                        break;
                }

                msg_log::add(
                        "I wade slowly through the knee high " +
                        type_str +
                        ".");
        }

        // Make a sound, unless player with Silent trait
        if (!player_bon::has_trait(Trait::silent) ||
            !actor_bumping.is_player())
        {
                const std::string msg =
                        actor_bumping.is_player()
                        ? ""
                        : "I hear a splash.";

                const auto alerts_mon =
                        actor_bumping.is_player()
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

        if (actor_bumping.is_player() &&
            (m_type == LiquidType::magic_water))
        {
                run_magic_pool_effects_on_player();
        }
}

void LiquidShallow::run_magic_pool_effects_on_player()
{
        std::vector<item::Item*> cursed_items;

        for (const auto& slot : map::g_player->m_inv.m_slots)
        {
                if (slot.item && slot.item->is_cursed())
                {
                        cursed_items.push_back(slot.item);
                }
        }

        for (auto* const item : map::g_player->m_inv.m_backpack)
        {
                if (item->is_cursed())
                {
                        cursed_items.push_back(item);
                }
        }

        map::g_player->m_properties.end_prop(PropId::cursed);
        map::g_player->m_properties.end_prop(PropId::diseased);
        map::g_player->m_properties.end_prop(PropId::wound);

        for (auto* const item : cursed_items)
        {
                const auto name =
                        item->name(
                                ItemRefType::plain,
                                ItemRefInf::none);

                msg_log::add("The " + name + " seems cleansed!");

                item->current_curse().on_curse_end();

                item->remove_curse();
        }
}

std::string LiquidShallow::name(const Article article) const
{
        std::string ret;

        if (article == Article::the)
        {
                ret += "the ";
        }

        switch (m_type)
        {
        case LiquidType::water:
                ret += "shallow water";
                break;

        case LiquidType::mud:
                ret += "shallow mud";
                break;

        case LiquidType::magic_water:
                ret += "gleaming pool";
                break;
        }

        return ret;
}

Color LiquidShallow::color_default() const
{
        switch (m_type)
        {
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

Color LiquidShallow::color_bg_default() const
{
        const auto* const item = map::g_items.at(m_pos);

        const auto* const corpse =
                map::first_actor_at_pos(
                        m_pos,
                        ActorState::corpse);

        if (item || corpse)
        {
                return color();
        }
        else
        {
                // Nothing is "over" the liquid
                return colors::black();
        }
}

// -----------------------------------------------------------------------------
// Deep liquid
// -----------------------------------------------------------------------------
LiquidDeep::LiquidDeep(const P& p) :
        Terrain(p),
        m_type(LiquidType::water) {}

void LiquidDeep::on_hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const int dmg)
{
        (void)dmg;
        (void)dmg_type;
        (void)actor;
}

bool LiquidDeep::must_swim_on_enter(const actor::Actor& actor) const
{
        return !actor.m_properties.has(PropId::ethereal) &&
                !actor.m_properties.has(PropId::flying);
}

AllowAction LiquidDeep::pre_bump(actor::Actor& actor_bumping)
{
        if (!actor_bumping.is_player() ||
            actor_bumping.m_properties.has(PropId::confused))
        {
                return AllowAction::yes;
        }

        if (map::g_player->m_active_explosive)
        {
                const std::string explosive_name =
                        map::g_player->m_active_explosive->name(
                                ItemRefType::plain);

                const std::string liquid_name_the = name(Article::the);

                const std::string msg =
                        "Swim through " +
                        liquid_name_the +
                        "? (The " +
                        explosive_name +
                        " will be extinguished) " +
                        common_text::g_yes_or_no_hint;

                msg_log::add(
                        msg,
                        colors::light_white(),
                        MsgInterruptPlayer::no,
                        MorePromptOnMsg::no,
                        CopyToMsgHistory::no);

                const auto result = query::yes_or_no();

                msg_log::clear();

                return (result == BinaryAnswer::no)
                        ? AllowAction::no
                        : AllowAction::yes;
        }

        return AllowAction::yes;
}

void LiquidDeep::bump(actor::Actor& actor_bumping)
{
        const bool must_swim = must_swim_on_enter(actor_bumping);

        const bool is_amphibian = actor_bumping.m_data->is_amphibian;

        if (must_swim && !is_amphibian)
        {
                auto* const waiting = new PropWaiting();

                waiting->set_duration(1);

                actor_bumping.m_properties.apply(waiting);
        }

        if (!must_swim)
        {
                return;
        }

        if (actor_bumping.is_player())
        {
                const std::string liquid_name_the =
                        name(Article::the);

                msg_log::add(
                        "I swim through " +
                        liquid_name_the +
                        ".");
        }

        // Make a sound, unless player with Silent trait
        if (!player_bon::has_trait(Trait::silent) ||
            !actor_bumping.is_player())
        {
                const std::string msg =
                        actor_bumping.is_player()
                        ? ""
                        : "I hear a splash.";

                const auto alerts_mon =
                        actor_bumping.is_player()
                        ? AlertsMon::yes
                        : AlertsMon::no;

                Snd snd(
                        msg,
                        audio::SfxId::swim,
                        IgnoreMsgIfOriginSeen::yes,
                        actor_bumping.m_pos,
                        &actor_bumping,
                        SndVol::low,
                        alerts_mon);

                snd_emit::run(snd);
        }

        if (actor_bumping.is_player() &&
            map::g_player->m_active_explosive)
        {
                const std::string expl_name =
                        map::g_player->m_active_explosive->name(
                                ItemRefType::plain);

                msg_log::add("The " + expl_name + " is extinguished.");

                map::g_player->m_active_explosive = nullptr;
        }

        if (!actor_bumping.m_properties.has(PropId::swimming))
        {
                auto* const swimming =
                        property_factory::make(PropId::swimming);

                swimming->set_indefinite();

                actor_bumping.m_properties.apply(swimming);
        }
}

void LiquidDeep::on_leave(actor::Actor& actor_leaving)
{
        actor_leaving.m_properties.end_prop(PropId::swimming);
}

std::string LiquidDeep::name(const Article article) const
{
        std::string ret;

        if (article == Article::the)
        {
                ret += "the ";
        }

        ret += "deep ";

        switch (m_type)
        {
        case LiquidType::water:
                ret += "water";
                break;

        case LiquidType::mud:
                ret += "mud";
                break;

        case LiquidType::magic_water:
                // Should not happen
                ASSERT(false);

                ret += "water";
                break;
        }

        return ret;
}

Color LiquidDeep::color_default() const
{
        switch (m_type)
        {
        case LiquidType::water:
                return colors::blue();

        case LiquidType::mud:
                return colors::dark_brown();

        case LiquidType::magic_water:
                // Should not happen
                ASSERT(false);

                return colors::blue();
        }

        ASSERT(false);

        return colors::blue();
}

bool LiquidDeep::can_move(const actor::Actor& actor) const
{
        return actor.m_data->can_swim ||
                actor.m_properties.has(PropId::flying) ||
                actor.m_properties.has(PropId::ethereal);
}

// -----------------------------------------------------------------------------
// Chasm
// -----------------------------------------------------------------------------
Chasm::Chasm(const P& p) :
        Terrain(p) {}

void Chasm::on_hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const int dmg)
{
        (void)dmg;
        (void)dmg_type;
        (void)actor;
}

std::string Chasm::name(const Article article) const
{
        std::string a = (article == Article::a) ? "a " : "the ";

        return a + "chasm";
}

Color Chasm::color_default() const
{
        return colors::blue();
}

// -----------------------------------------------------------------------------
// Lever
// -----------------------------------------------------------------------------
Lever::Lever(const P& p) :
        Terrain(p),
        m_is_left_pos(true),
        m_linked_terrain(nullptr) {}

void Lever::on_hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const int dmg)
{
        (void)dmg;
        (void)dmg_type;
        (void)actor;
}

std::string Lever::name(const Article article) const
{
        std::string ret = (article == Article::a) ? "a" : "the";

        ret += " lever (in ";

        ret +=
                m_is_left_pos
                ? "left"
                : "right";

        ret += " position)";

        return ret;
}

Color Lever::color_default() const
{
        if (m_is_left_pos)
        {
                return colors::gray();
        }
        else
        {
                return colors::white();
        }
}

gfx::TileId Lever::tile() const
{
        if (m_is_left_pos)
        {
                return gfx::TileId::lever_left;
        }
        else
        {
                return gfx::TileId::lever_right;
        }
}

void Lever::bump(actor::Actor& actor_bumping)
{
        (void)actor_bumping;

        TRACE_FUNC_BEGIN;

        // If player is blind, ask it they really want to pull the lever
        if (!map::g_seen.at(m_pos))
        {
                msg_log::clear();

                const std::string msg =
                        "There is a lever here. Pull it? " +
                        common_text::g_yes_or_no_hint;

                msg_log::add(
                        msg,
                        colors::light_white(),
                        MsgInterruptPlayer::no,
                        MorePromptOnMsg::no,
                        CopyToMsgHistory::no);

                const auto answer = query::yes_or_no();

                if (answer == BinaryAnswer::no)
                {
                        msg_log::clear();

                        TRACE_FUNC_END;

                        return;
                }
        }

        msg_log::add("I pull the lever.");

        Snd snd(
                "",
                audio::SfxId::lever_pull,
                IgnoreMsgIfOriginSeen::yes,
                m_pos,
                map::g_player,
                SndVol::low,
                AlertsMon::yes);

        snd.run();

        toggle();

        game_time::tick();

        TRACE_FUNC_END;
}

void Lever::toggle()
{
        m_is_left_pos = !m_is_left_pos;

        // Signal that the lever has been pulled to any linked terrain
        if (m_linked_terrain)
        {
                m_linked_terrain->on_lever_pulled(this);
        }

        // Set all sibblings to same status as this lever
        for (auto* const sibbling : m_sibblings)
        {
                sibbling->m_is_left_pos = m_is_left_pos;
        }
}

// -----------------------------------------------------------------------------
// Altar
// -----------------------------------------------------------------------------
Altar::Altar(const P& p) :
        Terrain(p) {}

void Altar::on_hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const int dmg)
{
        (void)dmg;
        (void)actor;

        switch (dmg_type)
        {
        case DmgType::explosion:
        case DmgType::pure:
                if (map::is_pos_seen_by_player(m_pos))
                {
                        msg_log::add("The altar is destroyed.");
                }

                map::put(new RubbleLow(m_pos));
                map::update_vision();

                if (player_bon::is_bg(Bg::exorcist))
                {
                        const auto msg =
                                rnd::element(
                                        common_text::g_exorcist_purge_phrases);

                        msg_log::add(msg);

                        game::incr_player_xp(10);

                        map::g_player->restore_sp(999, false);
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
            map::is_pos_seen_by_player(m_pos) &&
            !player_bon::is_bg(Bg::exorcist))
        {
                hints::display(hints::Id::altars);
        }
}

void Altar::bump(actor::Actor& actor_bumping)
{
        if (!actor_bumping.is_player())
        {
                return;
        }

        if (player_bon::is_bg(Bg::exorcist) &&
            map::is_pos_seen_by_player(m_pos))
        {
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
        std::string ret = (article == Article::a) ? "an " : "the ";

        return ret + "altar";
}

Color Altar::color_default() const
{
        return colors::white();
}

// -----------------------------------------------------------------------------
// Carpet
// -----------------------------------------------------------------------------
Carpet::Carpet(const P& p) :
        Terrain(p) {}

void Carpet::on_hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const int dmg)
{
        (void)actor;
        (void)dmg;

        switch (dmg_type)
        {
        default:
                try_start_burning(Verbose::no);
                break;
        }
}

WasDestroyed Carpet::on_finished_burning()
{
        auto* const floor = new Floor(m_pos);

        floor->m_burn_state = BurnState::has_burned;

        map::put(floor);

        return WasDestroyed::yes;
}

std::string Carpet::name(const Article article) const
{
        std::string ret = (article == Article::a)
                ? ""
                : "the ";

        return ret + "carpet";
}

Color Carpet::color_default() const
{
        return colors::red();
}

// -----------------------------------------------------------------------------
// Grass
// -----------------------------------------------------------------------------
Grass::Grass(const P& p) :
        Terrain(p),
        m_type(GrassType::common)
{
        if (rnd::one_in(5))
        {
                m_type = GrassType::withered;
        }
}

void Grass::on_hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const int dmg)
{
        (void)actor;
        (void)dmg;

        switch (dmg_type)
        {
        case DmgType::fire:
                try_start_burning(Verbose::no);
                break;

        default:
                break;
        }
}

gfx::TileId Grass::tile() const
{
        if (m_burn_state == BurnState::has_burned)
        {
                return gfx::TileId::scorched_ground;
        }
        else
        {
                return data().tile;
        }
}

std::string Grass::name(const Article article) const
{
        std::string ret;

        if (article == Article::the)
        {
                ret += "the ";
        }

        switch (m_burn_state)
        {
        case BurnState::not_burned:
                switch (m_type)
                {
                case GrassType::common:
                        return ret + "grass";

                case GrassType::withered:
                        return ret + "withered grass";
                }
                break;

        case BurnState::burning:
                return ret + "burning grass";

        case BurnState::has_burned:
                return ret + "scorched ground";
        }

        ASSERT("Failed to set name" && false);
        return "";
}

Color Grass::color_default() const
{
        switch (m_type)
        {
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
Bush::Bush(const P& p) :
        Terrain(p),
        m_type(GrassType::common)
{
        if (rnd::one_in(5))
        {
                m_type = GrassType::withered;
        }
}

void Bush::on_hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const int dmg)
{
        (void)actor;
        (void)dmg;

        switch (dmg_type)
        {
        case DmgType::fire:
                try_start_burning(Verbose::no);
                break;

        default:
                break;
        }
}

WasDestroyed Bush::on_finished_burning()
{
        auto* const grass = new Grass(m_pos);

        grass->m_burn_state = BurnState::has_burned;

        map::put(grass);

        return WasDestroyed::yes;
}

std::string Bush::name(const Article article) const
{
        std::string ret = (article == Article::a) ? "a " : "the ";

        switch (m_burn_state)
        {
        case BurnState::not_burned:
                switch (m_type)
                {
                case GrassType::common:
                        return ret + "shrub";

                case GrassType::withered:
                        return ret + "withered shrub";
                }
                break;

        case BurnState::burning:
                return ret + "burning shrub";

        case BurnState::has_burned:
                // Should not happen
                break;
        }

        ASSERT("Failed to set name" && false);
        return "";
}

Color Bush::color_default() const
{
        switch (m_type)
        {
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
Vines::Vines(const P& p) :
        Terrain(p) {}

void Vines::on_hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const int dmg)
{
        (void)actor;
        (void)dmg;

        switch (dmg_type)
        {
        case DmgType::explosion:
        case DmgType::pure:
                map::put(new RubbleLow(m_pos));
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
        auto* const floor = new Floor(m_pos);

        floor->m_burn_state = BurnState::has_burned;

        map::put(floor);

        return WasDestroyed::yes;
}

std::string Vines::name(const Article article) const
{
        std::string ret = (article == Article::a) ? "" : "the ";

        switch (m_burn_state)
        {
        case BurnState::not_burned:
                return ret + "hanging vines";

        case BurnState::burning:
                return ret + "burning vines";

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
Chains::Chains(const P& p) :
        Terrain(p) {}

std::string Chains::name(const Article article) const
{
        std::string a = (article == Article::a) ? "" : "the ";

        return a + "rusty chains";
}

Color Chains::color_default() const
{
        return colors::gray();
}

Color Chains::color_bg_default() const
{
        const auto* const item = map::g_items.at(m_pos);

        const auto* const corpse =
                map::first_actor_at_pos(
                        m_pos,
                        ActorState::corpse);

        if (item || corpse)
        {
                return color();
        }
        else
        {
                // Nothing is "over" the chains
                return colors::black();
        }
}

void Chains::bump(actor::Actor& actor_bumping)
{
        if (actor_bumping.m_data->actor_size > actor::Size::floor &&
            !actor_bumping.m_properties.has(PropId::ethereal) &&
            !actor_bumping.m_properties.has(PropId::ooze))
        {
                std::string msg;

                if (map::g_seen.at(m_pos))
                {
                        msg = "The chains rattle.";
                }
                else
                {
                        msg = "I hear chains rattling.";
                }

                const auto alerts_mon =
                        actor_bumping.is_player()
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

void Chains::on_hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const int dmg)
{
        (void)actor;
        (void)dmg;

        switch (dmg_type)
        {
        case DmgType::explosion:
        case DmgType::pure:
                map::put(new RubbleLow(m_pos));
                map::update_vision();
                break;

        default:
                break;
        }
}

// -----------------------------------------------------------------------------
// Grate
// -----------------------------------------------------------------------------
Grate::Grate(const P& p) :
        Terrain(p) {}

void Grate::on_hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const int dmg)
{
        (void)dmg;
        (void)actor;

        switch (dmg_type)
        {
        case DmgType::pure:
        case DmgType::explosion:
                destr_all_adj_doors(m_pos);
                map::put(new RubbleLow(m_pos));
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
        return colors::brown();
}

// -----------------------------------------------------------------------------
// Tree
// -----------------------------------------------------------------------------
Tree::Tree(const P& p) :
        Terrain(p)
{
        if (is_fungi())
        {
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
        else
        {
                m_color = colors::dark_brown();
        }
}

void Tree::on_hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const int dmg)
{
        (void)actor;
        (void)dmg;

        switch (dmg_type)
        {
        case DmgType::fire:
                if (rnd::fraction(2, 3))
                {
                        try_start_burning(Verbose::no);
                }
                break;

        default:
                break;
        }
}

WasDestroyed Tree::on_finished_burning()
{
        if (!map::is_pos_inside_outer_walls(m_pos))
        {
                return WasDestroyed::no;
        }

        auto* const grass = new Grass(m_pos);

        grass->m_burn_state = BurnState::has_burned;

        map::put(grass);

        map::update_vision();

        return WasDestroyed::yes;
}

gfx::TileId Tree::tile() const
{
        return is_fungi()
                ? gfx::TileId::tree_fungi
                : gfx::TileId::tree;
}

std::string Tree::name(const Article article) const
{
        std::string result = (article == Article::a) ? "a " : "the ";

        switch (m_burn_state)
        {
        case BurnState::not_burned:
                break;

        case BurnState::burning:
                result += "burning ";
                break;

        case BurnState::has_burned:
                result += "scorched ";
                break;
        }

        if (is_fungi())
        {
                result += "giant fungi";
        }
        else
        {
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

void Brazier::on_hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const int dmg)
{
        (void)dmg;

        switch (dmg_type)
        {
        case DmgType::kicking:
        {
                ASSERT(actor);

                if (actor->m_properties.has(PropId::weakened))
                {
                        msg_log::add("It wiggles a bit.");
                        return;
                }

                const auto alerts_mon =
                        (actor == map::g_player)
                        ? AlertsMon::yes
                        : AlertsMon::no;

                if (map::g_seen.at(m_pos))
                {
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

                const P dst_pos = m_pos + (m_pos - actor->m_pos);

                const P my_pos = m_pos;

                map::put(new RubbleLow(m_pos));

                // NOTE: "this" is now deleted!

                const auto* const tgt_f = map::g_terrain.at(dst_pos);

                if (tgt_f->id() != terrain::Id::chasm &&
                    tgt_f->id() != terrain::Id::liquid_deep)
                {
                        P expl_pos;

                        int expl_d = 0;

                        if (tgt_f->is_projectile_passable())
                        {
                                expl_pos = dst_pos;
                                expl_d = -1;
                        }
                        else
                        {
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
                                {new PropBurning()});
                }

                map::update_vision();
        }
        break;

        case DmgType::explosion:
        case DmgType::pure:
        {
                map::put(new RubbleLow(m_pos));
                map::update_vision();
        }
        break;

        default:
        {
        }
        break;
        }
}

void Brazier::add_light_hook(Array2<bool>& light) const
{
        for (const P& d : dir_utils::g_dir_list_w_center)
        {
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
        for (auto* item : m_items)
        {
                delete item;
        }
}

void ItemContainer::init(
        const terrain::Id terrain_id,
        const int nr_items_to_attempt)
{
        for (auto* item : m_items)
        {
                delete item;
        }

        m_items.clear();

        if (nr_items_to_attempt <= 0)
        {
                return;
        }

        // Try until actually succeeded to add at least one item
        while (m_items.empty())
        {
                std::vector<item::Id> item_bucket;

                for (size_t i = 0; i < (size_t)item::Id::END; ++i)
                {
                        auto& item_d = item::g_data[i];

                        if (!item_d.allow_spawn)
                        {
                                // Item not allowed to spawn - next item!
                                continue;
                        }

                        const bool can_spawn_in_container =
                                std::find(
                                        std::begin(item_d.native_containers),
                                        std::end(item_d.native_containers),
                                        terrain_id) !=
                                std::end(item_d.native_containers);

                        if (!can_spawn_in_container)
                        {
                                // Item not allowed to spawn in this terrain -
                                // next item!
                                continue;
                        }

                        if (rnd::percent(item_d.chance_to_incl_in_spawn_list))
                        {
                                item_bucket.push_back(item::Id(i));
                        }
                }

                for (int i = 0; i < nr_items_to_attempt; ++i)
                {
                        if (item_bucket.empty())
                        {
                                break;
                        }

                        const int idx =
                                rnd::range(0, (int)item_bucket.size() - 1);

                        const auto id = item_bucket[idx];

                        // Is this item still allowed to spawn (perhaps unique)?
                        if (item::g_data[(size_t)id].allow_spawn)
                        {
                                auto* item = item::make(item_bucket[idx]);

                                item::set_item_randomized_properties(*item);

                                m_items.push_back(item);
                        }
                        else
                        {
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
        if (!actor_opening)
        {
                // Not opened by an actor (probably opened by the opening spell)
                for (auto* item : m_items)
                {
                        item_drop::drop_item_on_map(terrain_pos, *item);
                }

                m_items.clear();

                return;
        }

        for (auto* item : m_items)
        {
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
                        ItemRefType::plural,
                        ItemRefInf::yes,
                        ItemRefAttInf::wpn_main_att_mode);

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

        if (is_unloadable_wpn)
        {
                msg_log::add("Unload? [u]");
        }

        const BinaryAnswer answer =
                query::yes_or_no(
                        is_unloadable_wpn
                                ? 'u'
                                : -1);

        msg_log::clear();

        if (answer == BinaryAnswer::yes)
        {
                const auto pre_pickup_result = item->pre_pickup_hook();

                switch (pre_pickup_result)
                {
                case ItemPrePickResult::do_pickup:
                {
                        audio::play(audio::SfxId::pickup);

                        map::g_player->m_inv.put_in_backpack(item);
                }
                break;

                case ItemPrePickResult::destroy_item:
                {
                        delete item;
                }
                break;

                case ItemPrePickResult::do_nothing:
                {
                }
                break;
                }
        }
        else if (answer == BinaryAnswer::no)
        {
                item_drop::drop_item_on_map(terrain_pos, *item);

                item->on_player_found();
        }
        else
        {
                // Special key (unload in this case)
                ASSERT(is_unloadable_wpn);

                if (is_unloadable_wpn)
                {
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
        for (auto* item : m_items)
        {
                delete item;
        }

        m_items.clear();
}

void ItemContainer::destroy_single_fragile()
{
        // TODO: Generalize this (something like "is_fragile" item data)

        for (size_t i = 0; i < m_items.size(); ++i)
        {
                auto* const item = m_items[i];

                const auto& d = item->data();

                if ((d.type == ItemType::potion) ||
                    (d.id == item::Id::molotov))
                {
                        delete item;
                        m_items.erase(m_items.begin() + i);
                        msg_log::add("I hear a muffled shatter.");
                        break;
                }
        }
}

// -----------------------------------------------------------------------------
// Tomb
// -----------------------------------------------------------------------------
Tomb::Tomb(const P& p) :
        Terrain(p),
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

        if (player_bon::has_trait(Trait::treasure_hunter))
        {
                incr_max_items_one_in /= 2;
        }

        if (rnd::one_in(incr_max_items_one_in))
        {
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
        if (rnd::one_in(4))
        {
                // Base appearance on value of contained items

                for (const auto* item : m_item_container.items())
                {
                        const auto item_value = item->data().value;

                        if (item_value == item::Value::supreme_treasure)
                        {
                                m_appearance = TombAppearance::marvelous;

                                break;
                        }
                        else if (item_value >= item::Value::minor_treasure)
                        {
                                m_appearance = TombAppearance::ornate;
                        }
                }
        }
        else
        {
                // Do not base appearance on items - use a common appearance
                m_appearance = TombAppearance::common;
        }

        if (m_appearance == TombAppearance::marvelous)
        {
                m_trait = TombTrait::ghost;
        }
        else
        {
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

void Tomb::on_hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const int dmg)
{
        (void)dmg;
        (void)actor;

        switch (dmg_type)
        {
        case DmgType::explosion:
        case DmgType::pure:
                if (map::is_pos_seen_by_player(m_pos))
                {
                        msg_log::add("The tomb is destroyed.");
                }

                map::put(new RubbleLow(m_pos));
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

        if (article == Article::a)
        {
                a =
                        (m_is_open || (m_appearance == TombAppearance::ornate))
                        ? "an "
                        : "a ";
        }
        else
        {
                a = "the ";
        }

        std::string appear_str;

        if (!is_empty)
        {
                switch (m_appearance)
                {
                case TombAppearance::common:
                case TombAppearance::END:
                {
                }
                break;

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
        return m_is_open
                ? gfx::TileId::tomb_open
                : gfx::TileId::tomb_closed;
}

Color Tomb::color_default() const
{
        switch (m_appearance)
        {
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
        if (actor_bumping.is_player())
        {
                if (m_item_container.is_empty() && m_is_open)
                {
                        msg_log::add("The tomb is empty.");
                }
                else if (!map::g_seen.at(m_pos))
                {
                        msg_log::add("There is a stone box here.");
                }
                else
                {
                        // Player can see
                        if (m_is_open)
                        {
                                player_loot();
                        }
                        else
                        {
                                // Not open
                                msg_log::add("I attempt to push the lid.");

                                if (actor_bumping.m_properties.has(PropId::weakened))
                                {
                                        msg_log::add("It seems futile.");
                                }
                                else
                                {
                                        // Not weakened
                                        int bon = 0;

                                        if (player_bon::has_trait(Trait::rugged))
                                        {
                                                bon = 8;
                                        }
                                        else if (player_bon::has_trait(Trait::tough))
                                        {
                                                bon = 4;
                                        }
                                        else
                                        {
                                                bon = 0;
                                        }

                                        TRACE << "Base chance to push lid is: 1 in "
                                              << m_push_lid_one_in_n << std::endl;

                                        TRACE << "Bonus to roll: "
                                              << bon << std::endl;

                                        const int roll_tot =
                                                rnd::range(1, m_push_lid_one_in_n) + bon;

                                        TRACE << "Roll + bonus = " << roll_tot << std::endl;

                                        bool is_success = false;

                                        if (roll_tot < (m_push_lid_one_in_n - 9))
                                        {
                                                msg_log::add("It does not yield at all.");
                                        }
                                        else if (roll_tot < (m_push_lid_one_in_n - 2))
                                        {
                                                msg_log::add("It resists.");
                                        }
                                        else if (roll_tot == (m_push_lid_one_in_n - 2))
                                        {
                                                msg_log::add("It moves a little!");
                                                --m_push_lid_one_in_n;
                                        }
                                        else
                                        {
                                                is_success = true;
                                        }

                                        if (is_success)
                                        {
                                                open(map::g_player);
                                        }
                                        else
                                        {
                                                wham::try_sprain_player();
                                        }
                                }
                        }

                        game_time::tick();
                }
        }
}

void Tomb::player_loot()
{
        msg_log::add("I peer inside the tomb.");

        if (m_item_container.is_empty())
        {
                msg_log::add("There is nothing of value inside.");
        }
        else
        {
                m_item_container.open(m_pos, map::g_player);
        }
}

DidOpen Tomb::open(actor::Actor* const actor_opening)
{
        if (m_is_open)
        {
                return DidOpen::no;
        }
        else
        {
                // Was not already open
                m_is_open = true;

                Snd snd("I hear heavy stone sliding.",
                        audio::SfxId::tomb_open,
                        IgnoreMsgIfOriginSeen::yes,
                        m_pos,
                        map::g_player,
                        SndVol::high,
                        AlertsMon::yes);

                snd.run();

                if (map::g_seen.at(m_pos))
                {
                        msg_log::add("The lid comes off.");
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

        auto id_to_spawn = actor::Id::END;

        const bool is_seen = map::g_seen.at(m_pos);

        switch (m_trait)
        {
        case TombTrait::ghost:
        {
                id_to_spawn = actor::Id::ghost;

                const std::string msg = "The air suddenly feels colder.";

                msg_log::add(
                        msg,
                        colors::white(),
                        MsgInterruptPlayer::no,
                        MorePromptOnMsg::yes);

                did_trigger_trap = DidTriggerTrap::yes;
        }
        break;

        case TombTrait::other_undead:
        {
                std::vector<actor::Id> mon_bucket = {
                        actor::Id::mummy,
                        actor::Id::croc_head_mummy,
                        actor::Id::zombie,
                        actor::Id::floating_skull};

                id_to_spawn = rnd::element(mon_bucket);

                const std::string msg = "Something rises from the tomb!";

                msg_log::add(
                        msg,
                        colors::white(),
                        MsgInterruptPlayer::no,
                        MorePromptOnMsg::yes);

                did_trigger_trap = DidTriggerTrap::yes;
        }
        break;

        case TombTrait::stench:
        {
                if (rnd::coin_toss())
                {
                        if (is_seen)
                        {
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

                        Prop* prop = nullptr;

                        Color fume_color = colors::magenta();

                        const int rnd = rnd::range(1, 100);

                        if (rnd < 20)
                        {
                                prop = new PropPoisoned();

                                fume_color = colors::light_green();
                        }
                        else if (rnd < 40)
                        {
                                prop = new PropDiseased();

                                fume_color = colors::green();
                        }
                        else
                        {
                                prop = new PropParalyzed();

                                prop->set_duration(prop->nr_turns_left() * 2);
                        }

                        explosion::run(
                                m_pos,
                                ExplType::apply_prop,
                                EmitExplSnd::no,
                                0,
                                ExplExclCenter::no,
                                {prop},
                                fume_color);
                }
                else
                {
                        // Not fumes
                        std::vector<actor::Id> mon_bucket;

                        for (size_t i = 0; i < size_t(actor::Id::END); ++i)
                        {
                                const auto& d = actor::g_data[i];

                                if (d.natural_props[(size_t)PropId::ooze] &&
                                    d.is_auto_spawn_allowed &&
                                    !d.is_unique)
                                {
                                        mon_bucket.push_back((actor::Id)i);
                                }
                        }

                        id_to_spawn = rnd::element(mon_bucket);

                        if (is_seen)
                        {
                                msg_log::add(
                                        "Something repulsive creeps up from the tomb!",
                                        colors::white(),
                                        MsgInterruptPlayer::no,
                                        MorePromptOnMsg::yes);
                        }
                }

                did_trigger_trap = DidTriggerTrap::yes;
        }
        break;

        case TombTrait::cursed:
        {
                map::g_player->m_properties.apply(new PropCursed());

                did_trigger_trap = DidTriggerTrap::yes;
        }
        break;

        case TombTrait::END:
                break;
        }

        if (id_to_spawn != actor::Id::END)
        {
                const auto summoned =
                        actor::spawn(m_pos, {id_to_spawn}, map::rect())
                                .make_aware_of_player();

                std::for_each(
                        std::begin(summoned.monsters),
                        std::end(summoned.monsters),
                        [this](auto* const mon) {
                                auto* prop = new PropWaiting();

                                prop->set_duration(1);

                                mon->m_properties.apply(prop);

                                if (m_appearance == TombAppearance::marvelous)
                                {
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
Chest::Chest(const P& p) :
        Terrain(p),
        m_is_open(false),
        m_is_locked(false),
        m_matl(ChestMatl::wood)
{
        if (map::g_dlvl >= 3 &&
            rnd::fraction(2, 3))
        {
                m_matl = ChestMatl::iron;
        }

        // Contained items
        const int nr_items_min =
                rnd::one_in(4)
                ? 0
                : 1;

        int nr_items_max = 2;

        int incr_max_items_one_in = 12;

        if (player_bon::has_trait(Trait::treasure_hunter))
        {
                incr_max_items_one_in /= 2;
        }

        if (rnd::one_in(incr_max_items_one_in))
        {
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
        if (!actor_bumping.is_player())
        {
                return;
        }

        if (!map::g_seen.at(m_pos))
        {
                msg_log::add("There is a chest here.");

                return;
        }

        if (m_burn_state == BurnState::burning)
        {
                msg_log::add("The chest is on fire.");

                return;
        }

        if (m_item_container.is_empty() && m_is_open)
        {
                msg_log::add("The chest is empty.");

                return;
        }

        if (m_is_locked)
        {
                msg_log::add("The chest is locked.");

                return;
        }

        // Not locked
        if (m_is_open)
        {
                player_loot();
        }
        else
        {
                open(map::g_player);
        }

        game_time::tick();
}

void Chest::player_loot()
{
        msg_log::add("I search the chest.");

        if (m_item_container.is_empty())
        {
                msg_log::add("There is nothing of value inside.");
        }
        else
        {
                // Not empty
                m_item_container.open(m_pos, map::g_player);
        }
}

DidOpen Chest::open(actor::Actor* const actor_opening)
{
        (void)actor_opening;

        m_is_locked = false;

        if (m_is_open)
        {
                return DidOpen::no;
        }
        else
        {
                // Chest is closed
                m_is_open = true;

                if (map::g_seen.at(m_pos))
                {
                        msg_log::add("The chest opens.");
                }

                return DidOpen::yes;
        }
}

void Chest::hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const int dmg)
{
        (void)dmg;
        (void)actor;

        switch (dmg_type)
        {
        case DmgType::kicking:
                on_player_kick();
                break;

        default:
                break;
        }
}

void Chest::on_hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const int dmg)
{
        (void)dmg;
        (void)actor;

        switch (dmg_type)
        {
        case DmgType::explosion:
        case DmgType::pure:
                if (map::is_pos_seen_by_player(m_pos))
                {
                        msg_log::add("The chest is destroyed.");
                }

                map::put(new RubbleLow(m_pos));
                map::update_vision();
                break;

        case DmgType::fire:
                if (m_matl == ChestMatl::wood)
                {
                        m_item_container.clear();
                        try_start_burning(Verbose::yes);
                }
                break;

        default:
                break;
        }
}

WasDestroyed Chest::on_finished_burning()
{
        if (map::is_pos_seen_by_player(m_pos))
        {
                msg_log::add("The chest burns down.");
        }

        auto* const rubble = new RubbleLow(m_pos);

        rubble->m_burn_state = BurnState::has_burned;

        map::put(rubble);

        map::update_vision();

        return WasDestroyed::yes;
}

void Chest::on_player_kick()
{
        if (!map::g_seen.at(m_pos))
        {
                // If player is blind, call the parent hit function
                // instead (generic kicking)
                Terrain::hit(DmgType::kicking, map::g_player);

                return;
        }

        if (m_is_open)
        {
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
        if (!m_is_locked)
        {
                msg_log::add("The lid slams open, then falls shut.");

                snd.run();

                return;
        }

        // Is locked

        msg_log::add("I kick the lid.");

        if (map::g_player->m_properties.has(PropId::weakened) ||
            (m_matl == ChestMatl::iron))
        {
                wham::try_sprain_player();

                msg_log::add("It seems futile.");

                snd.run();

                return;
        }

        // Chest can be bashed open
        if (rnd::one_in(3))
        {
                m_item_container.destroy_single_fragile();
        }

        int open_one_in_n = 0;

        if (player_bon::has_trait(Trait::rugged))
        {
                open_one_in_n = 2;
        }
        else if (player_bon::has_trait(Trait::tough))
        {
                open_one_in_n = 3;
        }
        else
        {
                open_one_in_n = 4;
        }

        if (rnd::one_in(open_one_in_n))
        {
                msg_log::add(
                        "The lock breaks and the lid flies open!",
                        colors::text(),
                        MsgInterruptPlayer::no,
                        MorePromptOnMsg::yes);

                m_is_locked = false;
                m_is_open = true;
        }
        else
        {
                msg_log::add("The lock resists.");

                wham::try_sprain_player();
        }

        snd.run();
}

std::string Chest::name(const Article article) const
{
        std::string matl_str;
        std::string locked_str;
        std::string empty_str;
        std::string open_str;
        std::string a;

        if (m_matl == ChestMatl::wood)
        {
                matl_str = "wooden ";
                a = "a ";
        }
        else
        {
                matl_str = "iron ";
                a = "an ";
        }

        if (m_is_open)
        {
                if (m_item_container.is_empty())
                {
                        empty_str = "empty ";
                }
                else
                {
                        open_str = "open ";
                }

                a = "an ";
        }
        else if (m_is_locked)
        {
                locked_str = "locked ";

                a = "a ";
        }

        if (article == Article::the)
        {
                a = "the ";
        }

        return a + locked_str + empty_str + open_str + matl_str + "chest";
}

gfx::TileId Chest::tile() const
{
        return m_is_open
                ? gfx::TileId::chest_open
                : gfx::TileId::chest_closed;
}

Color Chest::color_default() const
{
        return (m_matl == ChestMatl::wood)
                ? colors::dark_brown()
                : colors::gray();
}

// -----------------------------------------------------------------------------
// Fountain
// -----------------------------------------------------------------------------
Fountain::Fountain(const P& p) :
        Terrain(p)
{
        std::vector<int> weights = {
                4,  // Refreshing
                1,  // XP
                1,  // Bad effect
        };

        const int choice = rnd::weighted_choice(weights);

        switch (choice)
        {
        case 0:
        {
                m_fountain_effect = FountainEffect::refreshing;
        }
        break;

        case 1:
        {
                m_fountain_effect = FountainEffect::xp;
        }
        break;

        case 2:
        {
                const int min = (int)FountainEffect::START_OF_BAD_EFFECTS + 1;
                const int max = (int)FountainEffect::END - 1;

                m_fountain_effect = (FountainEffect)rnd::range(min, max);
        }
        break;

        default:
        {
                ASSERT(false);
        }
        break;
        }
}

void Fountain::on_hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const int dmg)
{
        (void)dmg;
        (void)actor;

        switch (dmg_type)
        {
        case DmgType::explosion:
        case DmgType::pure:
                if (map::is_pos_seen_by_player(m_pos))
                {
                        msg_log::add("The fountain is destroyed.");
                }

                map::put(new RubbleLow(m_pos));
                map::update_vision();
                break;

        default:
                break;
        }
}

Color Fountain::color_default() const
{
        if (m_has_drinks_left)
        {
                if (m_is_tried)
                {
                        // Has drinks left, tried
                        const auto is_bad =
                                m_fountain_effect >
                                FountainEffect::START_OF_BAD_EFFECTS;

                        return is_bad
                                ? colors::magenta()
                                : colors::light_cyan();
                }
                else
                {
                        // Has drinks left, not tried
                        return colors::light_blue();
                }
        }
        else
        {
                // No drinks left
                return colors::gray();
        }
}

std::string Fountain::name(const Article article) const
{
        std::string type_str;

        std::string indefinite_article = "a";

        if (m_has_drinks_left)
        {
                if (m_is_tried)
                {
                        type_str = type_name();

                        indefinite_article = type_indefinite_article();
                }
        }
        else
        {
                type_str = "dried-up";
        }

        const std::string a =
                (article == Article::a)
                ? indefinite_article
                : "the";

        if (!type_str.empty())
        {
                type_str = " " + type_str;
        }

        return a + type_str + " fountain";
}

void Fountain::bump(actor::Actor& actor_bumping)
{
        if (!actor_bumping.is_player())
        {
                return;
        }

        if (!m_has_drinks_left)
        {
                msg_log::add("The fountain is dried-up.");

                return;
        }

        PropHandler& properties = map::g_player->m_properties;

        if (!map::g_seen.at(m_pos))
        {
                msg_log::clear();

                const std::string name_a =
                        text_format::first_to_lower(
                                name(Article::a));

                const std::string msg =
                        ("There is " +
                         name_a +
                         " here. Drink from it? ") +
                        common_text::g_yes_or_no_hint;

                msg_log::add(
                        msg,
                        colors::light_white(),
                        MsgInterruptPlayer::no,
                        MorePromptOnMsg::no,
                        CopyToMsgHistory::no);

                const auto answer = query::yes_or_no();

                if (answer == BinaryAnswer::no)
                {
                        msg_log::clear();

                        return;
                }
        }

        if (!properties.allow_eat(Verbose::yes))
        {
                return;
        }

        msg_log::clear();
        msg_log::add("I drink from the fountain...");

        audio::play(audio::SfxId::fountain_drink);

        switch (m_fountain_effect)
        {
        case FountainEffect::refreshing:
        {
                msg_log::add("It's very refreshing.");
                map::g_player->restore_hp(1, false, Verbose::no);
                map::g_player->restore_sp(1, false, Verbose::no);
                map::g_player->restore_shock(5, true);
        }
        break;

        case FountainEffect::xp:
        {
                msg_log::add("I feel more powerful!");
                game::incr_player_xp(1);
        }
        break;

        case FountainEffect::curse:
        {
                properties.apply(new PropCursed());
        }
        break;

        case FountainEffect::disease:
        {
                properties.apply(new PropDiseased());
        }
        break;

        case FountainEffect::poison:
        {
                properties.apply(new PropPoisoned());
        }
        break;

        case FountainEffect::frenzy:
        {
                properties.apply(new PropFrenzied());
        }
        break;

        case FountainEffect::paralyze:
        {
                properties.apply(new PropParalyzed());
        }
        break;

        case FountainEffect::blind:
        {
                properties.apply(new PropBlind());
        }
        break;

        case FountainEffect::faint:
        {
                auto* prop = new PropFainted();

                prop->set_duration(10);

                properties.apply(prop);
        }
        break;

        case FountainEffect::START_OF_BAD_EFFECTS:
        case FountainEffect::END:
                break;
        }

        m_is_tried = true;

        const int dry_one_in_n = 3;

        if (rnd::one_in(dry_one_in_n))
        {
                m_has_drinks_left = false;

                msg_log::add("The fountain dries up.");
        }

        game_time::tick();
}

void Fountain::on_new_turn()
{
        if (map::g_player->m_pos.is_adjacent(m_pos) &&
            map::g_seen.at(m_pos))
        {
                hints::display(hints::Id::fountains);
        }
}

void Fountain::bless()
{
        if (!has_drinks_left())
        {
                return;
        }

        const bool is_bad_effect =
                m_fountain_effect >
                FountainEffect::START_OF_BAD_EFFECTS;

        if (!is_bad_effect)
        {
                return;
        }

        m_is_tried = false;

        m_fountain_effect = FountainEffect::refreshing;

        if (map::g_seen.at(m_pos))
        {
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
        if (!has_drinks_left())
        {
                return;
        }

        const bool is_good_effect =
                m_fountain_effect <
                FountainEffect::START_OF_BAD_EFFECTS;

        if (!is_good_effect)
        {
                return;
        }

        m_is_tried = false;

        const int min = (int)FountainEffect::START_OF_BAD_EFFECTS + 1;
        const int max = (int)FountainEffect::END - 1;

        m_fountain_effect = (FountainEffect)rnd::range(min, max);

        if (map::g_seen.at(m_pos))
        {
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
        switch (m_fountain_effect)
        {
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
        switch (m_fountain_effect)
        {
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
Cabinet::Cabinet(const P& p) :
        Terrain(p),
        m_is_open(false)
{
        // Contained items
        const int nr_items_min =
                rnd::coin_toss()
                ? 0
                : 1;

        int nr_items_max = 1;

        int incr_max_items_one_in = 12;

        if (player_bon::has_trait(Trait::treasure_hunter))
        {
                incr_max_items_one_in /= 2;
        }

        if (rnd::one_in(incr_max_items_one_in))
        {
                ++nr_items_max;
        }

        m_item_container.init(
                terrain::Id::cabinet,
                rnd::range(nr_items_min, nr_items_max));
}

void Cabinet::on_hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const int dmg)
{
        (void)actor;
        (void)dmg;

        switch (dmg_type)
        {
        case DmgType::explosion:
        case DmgType::pure:
                if (map::is_pos_seen_by_player(m_pos))
                {
                        msg_log::add("The cabinet is destroyed.");
                }

                map::put(new RubbleLow(m_pos));
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
        if (map::is_pos_seen_by_player(m_pos))
        {
                msg_log::add("The cabinet burns down.");
        }

        auto* const rubble = new RubbleLow(m_pos);

        rubble->m_burn_state = BurnState::has_burned;

        map::put(rubble);

        map::update_vision();

        return WasDestroyed::yes;
}

void Cabinet::bump(actor::Actor& actor_bumping)
{
        if (!actor_bumping.is_player())
        {
                return;
        }

        if (!map::g_seen.at(m_pos))
        {
                msg_log::add("There is a cabinet here.");

                return;
        }

        if (m_burn_state == BurnState::burning)
        {
                msg_log::add("The cabinet is on fire.");

                return;
        }

        if (m_item_container.is_empty() && m_is_open)
        {
                msg_log::add("The cabinet is empty.");

                return;
        }

        if (m_is_open)
        {
                player_loot();
        }
        else
        {
                open(map::g_player);
        }
}

void Cabinet::player_loot()
{
        msg_log::add("I search the cabinet.");

        if (m_item_container.is_empty())
        {
                msg_log::add("There is nothing of value inside.");
        }
        else
        {
                m_item_container.open(m_pos, map::g_player);
        }
}

DidOpen Cabinet::open(actor::Actor* const actor_opening)
{
        (void)actor_opening;

        if (m_is_open)
        {
                return DidOpen::no;
        }
        else
        {
                // Was not already open
                m_is_open = true;

                if (map::g_seen.at(m_pos))
                {
                        msg_log::add("The cabinet opens.");
                }

                return DidOpen::yes;
        }
}

std::string Cabinet::name(const Article article) const
{
        std::string ret = (article == Article::a) ? "a " : "the ";

        if (m_burn_state == BurnState::burning)
        {
                ret += "burning ";
        }

        return ret + "cabinet";
}

gfx::TileId Cabinet::tile() const
{
        return m_is_open
                ? gfx::TileId::cabinet_open
                : gfx::TileId::cabinet_closed;
}

Color Cabinet::color_default() const
{
        return colors::dark_brown();
}

// -----------------------------------------------------------------------------
// Bookshelf
// -----------------------------------------------------------------------------
Bookshelf::Bookshelf(const P& p) :
        Terrain(p),
        m_is_looted(false)
{
        // Contained items
        const int nr_items_min =
                rnd::coin_toss()
                ? 0
                : 1;

        int nr_items_max = 1;

        int incr_max_items_one_in = 12;

        if (player_bon::has_trait(Trait::treasure_hunter))
        {
                incr_max_items_one_in /= 2;
        }

        if (rnd::one_in(incr_max_items_one_in))
        {
                ++nr_items_max;
        }

        m_item_container.init(
                terrain::Id::bookshelf,
                rnd::range(nr_items_min, nr_items_max));
}

void Bookshelf::on_hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const int dmg)
{
        (void)dmg;
        (void)actor;

        switch (dmg_type)
        {
        case DmgType::explosion:
        case DmgType::pure:
                if (map::is_pos_seen_by_player(m_pos))
                {
                        msg_log::add("The bookshelf is destroyed.");
                }

                map::put(new RubbleLow(m_pos));
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
        if (map::is_pos_seen_by_player(m_pos))
        {
                msg_log::add("The bookshelf burns down.");
        }

        auto* const rubble = new RubbleLow(m_pos);

        rubble->m_burn_state = BurnState::has_burned;

        map::put(rubble);

        map::update_vision();

        return WasDestroyed::yes;
}

void Bookshelf::bump(actor::Actor& actor_bumping)
{
        if (!actor_bumping.is_player())
        {
                return;
        }

        if (!map::g_seen.at(m_pos))
        {
                msg_log::add("There is a bookshelf here.");

                return;
        }

        if (m_burn_state == BurnState::burning)
        {
                msg_log::add("The bookshelf is on fire.");

                return;
        }

        if (m_item_container.is_empty() && m_is_looted)
        {
                msg_log::add("The bookshelf is empty.");

                return;
        }

        if (!m_is_looted)
        {
                player_loot();
        }
}

void Bookshelf::player_loot()
{
        msg_log::add(
                "I search the bookshelf.",
                colors::text(),
                MsgInterruptPlayer::no,
                MorePromptOnMsg::yes);

        m_is_looted = true;

        if (m_item_container.is_empty())
        {
                msg_log::add("There is nothing of interest.");
        }
        else
        {
                m_item_container.open(m_pos, map::g_player);
        }
}

std::string Bookshelf::name(const Article article) const
{
        std::string ret = (article == Article::a) ? "a " : "the ";

        if (m_burn_state == BurnState::burning)
        {
                ret += "burning ";
        }

        return ret + "bookshelf";
}

gfx::TileId Bookshelf::tile() const
{
        return m_is_looted
                ? gfx::TileId::bookshelf_empty
                : gfx::TileId::bookshelf_full;
}

Color Bookshelf::color_default() const
{
        return colors::dark_brown();
}

// -----------------------------------------------------------------------------
// AlchemistBench
// -----------------------------------------------------------------------------
AlchemistBench::AlchemistBench(const P& p) :
        Terrain(p),
        m_is_looted(false)
{
        // Contained items
        const int nr_items_min =
                rnd::coin_toss()
                ? 0
                : 1;

        int nr_items_max = 1;

        int incr_max_items_one_in = 12;

        if (player_bon::has_trait(Trait::treasure_hunter))
        {
                incr_max_items_one_in /= 2;
        }

        if (rnd::one_in(incr_max_items_one_in))
        {
                ++nr_items_max;
        }

        m_item_container.init(
                terrain::Id::alchemist_bench,
                rnd::range(nr_items_min, nr_items_max));
}

void AlchemistBench::on_hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const int dmg)
{
        (void)actor;
        (void)dmg;

        switch (dmg_type)
        {
        case DmgType::explosion:
        case DmgType::pure:
                if (map::is_pos_seen_by_player(m_pos))
                {
                        msg_log::add("The alchemist's workbench is destroyed.");
                }

                map::put(new RubbleLow(m_pos));
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
        if (map::is_pos_seen_by_player(m_pos))
        {
                msg_log::add("The alchemist's workbench burns down.");
        }

        auto* const rubble = new RubbleLow(m_pos);

        rubble->m_burn_state = BurnState::has_burned;

        map::put(rubble);

        map::update_vision();

        return WasDestroyed::yes;
}

void AlchemistBench::bump(actor::Actor& actor_bumping)
{
        if (!actor_bumping.is_player())
        {
                return;
        }

        if (!map::g_seen.at(m_pos))
        {
                msg_log::add("There is a wooden bench here.");

                return;
        }

        if (m_burn_state == BurnState::burning)
        {
                msg_log::add("The alchemist's workbench is on fire.");

                return;
        }

        if (m_item_container.is_empty() && m_is_looted)
        {
                msg_log::add("The alchemist's workbench is empty.");

                return;
        }

        if (!m_is_looted)
        {
                player_loot();
        }
}

void AlchemistBench::player_loot()
{
        msg_log::add(
                "I search the alchemist's workbench.",
                colors::text(),
                MsgInterruptPlayer::no,
                MorePromptOnMsg::yes);

        m_is_looted = true;

        if (m_item_container.is_empty())
        {
                msg_log::add("There is nothing of interest.");
        }
        else
        {
                m_item_container.open(m_pos, map::g_player);
        }
}

std::string AlchemistBench::name(const Article article) const
{
        std::string a = (article == Article::a) ? "an " : "the ";

        std::string mod;

        if (m_burn_state == BurnState::burning)
        {
                if (article == Article::a)
                {
                        a = "a ";
                }

                mod = "burning ";
        }

        return a + mod + "alchemist's workbench";
}

gfx::TileId AlchemistBench::tile() const
{
        return m_is_looted
                ? gfx::TileId::alchemist_bench_empty
                : gfx::TileId::alchemist_bench_full;
}

Color AlchemistBench::color_default() const
{
        return colors::brown();
}

// -----------------------------------------------------------------------------
// Cocoon
// -----------------------------------------------------------------------------
Cocoon::Cocoon(const P& p) :
        Terrain(p),
        m_is_trapped(rnd::fraction(6, 10)),
        m_is_open(false)
{
        if (m_is_trapped)
        {
                m_item_container.init(terrain::Id::cocoon, 0);
        }
        else
        {
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

void Cocoon::on_hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const int dmg)
{
        (void)actor;
        (void)dmg;

        switch (dmg_type)
        {
        case DmgType::explosion:
        case DmgType::pure:
                if (map::is_pos_seen_by_player(m_pos))
                {
                        msg_log::add("The cocoon is destroyed.");
                }

                map::put(new RubbleLow(m_pos));
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
        if (map::is_pos_seen_by_player(m_pos))
        {
                msg_log::add("The cocoon burns down.");
        }

        auto* const rubble = new RubbleLow(m_pos);

        rubble->m_burn_state = BurnState::has_burned;

        map::put(rubble);

        map::update_vision();

        return WasDestroyed::yes;
}

void Cocoon::bump(actor::Actor& actor_bumping)
{
        if (!actor_bumping.is_player())
        {
                return;
        }

        if (!map::g_seen.at(m_pos))
        {
                msg_log::add("There is a cocoon here.");

                return;
        }

        if (m_burn_state == BurnState::burning)
        {
                msg_log::add("The cocoon is on fire.");

                return;
        }

        if (m_item_container.is_empty() && m_is_open)
        {
                msg_log::add("The cocoon is empty.");

                return;
        }

        if (insanity::has_sympt(InsSymptId::phobia_spider))
        {
                map::g_player->m_properties.apply(
                        new PropTerrified());
        }

        if (m_is_open)
        {
                player_loot();
        }
        else
        {
                open(map::g_player);
        }
}

DidTriggerTrap Cocoon::trigger_trap(actor::Actor* const actor)
{
        (void)actor;

        if (m_is_trapped)
        {
                const int rnd = rnd::range(1, 100);

                if (rnd < 15)
                {
                        // A dead body
                        msg_log::add(
                                "There is a half-dissolved human body inside!");

                        map::g_player->incr_shock(
                                ShockLvl::terrifying,
                                ShockSrc::misc);

                        m_is_trapped = false;

                        return DidTriggerTrap::yes;
                }
                else if (rnd < 50)
                {
                        // Spiders
                        TRACE << "Attempting to spawn spiders" << std::endl;
                        std::vector<actor::Id> spawn_bucket;

                        for (int i = 0; i < (int)actor::Id::END; ++i)
                        {
                                const auto& d = actor::g_data[i];

                                if (d.is_spider &&
                                    d.actor_size == actor::Size::floor &&
                                    d.is_auto_spawn_allowed &&
                                    !d.is_unique)
                                {
                                        spawn_bucket.push_back(d.id);
                                }
                        }

                        const int nr_candidates = spawn_bucket.size();

                        if (nr_candidates > 0)
                        {
                                TRACE << "Spawn candidates found, attempting to place"
                                      << std::endl;

                                msg_log::add("There are spiders inside!");

                                const size_t nr_spiders = rnd::range(2, 5);

                                const int idx =
                                        rnd::range(0, nr_candidates - 1);

                                const actor::Id actor_id_to_summon =
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
        }

        return DidTriggerTrap::no;
}

void Cocoon::player_loot()
{
        msg_log::add("I search the Cocoon.");

        if (m_item_container.is_empty())
        {
                msg_log::add("It is empty.");
        }
        else
        {
                m_item_container.open(m_pos, map::g_player);
        }
}

DidOpen Cocoon::open(actor::Actor* const actor_opening)
{
        if (m_is_open)
        {
                return DidOpen::no;
        }
        else
        {
                // Was not already open
                m_is_open = true;

                if (map::g_seen.at(m_pos))
                {
                        msg_log::add("The cocoon opens.");
                }

                trigger_trap(actor_opening);

                return DidOpen::yes;
        }
}

std::string Cocoon::name(const Article article) const
{
        std::string ret = (article == Article::a) ? "a " : "the ";

        if (m_burn_state == BurnState::burning)
        {
                ret += "burning ";
        }

        return ret + "cocoon";
}

gfx::TileId Cocoon::tile() const
{
        return m_is_open
                ? gfx::TileId::cocoon_open
                : gfx::TileId::cocoon_closed;
}

Color Cocoon::color_default() const
{
        return colors::white();
}

}  // namespace terrain

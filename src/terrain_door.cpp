// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "terrain_door.hpp"

#include "actor.hpp"
#include "actor_player.hpp"
#include "actor_see.hpp"
#include "debug.hpp"
#include "game.hpp"
#include "game_time.hpp"
#include "init.hpp"
#include "map.hpp"
#include "map_parsing.hpp"
#include "msg_log.hpp"
#include "player_bon.hpp"
#include "postmortem.hpp"
#include "property_handler.hpp"
#include "terrain_data.hpp"
#include "text_format.hpp"

namespace terrain
{
Door::Door(
        const P& terrain_pos,
        const Wall* const mimic_terrain,
        DoorType type,
        DoorSpawnState spawn_state) :

        Terrain(terrain_pos),
        m_mimic_terrain(mimic_terrain),
        m_nr_spikes(0),
        m_is_open(false),
        m_is_stuck(false),
        m_type(type)
{
        // Gates should never be secret
        ASSERT(!(m_type == DoorType::gate && m_mimic_terrain));

        ASSERT(
                !(m_type == DoorType::gate &&
                  (spawn_state == DoorSpawnState::secret ||
                   spawn_state == DoorSpawnState::secret_and_stuck)));

        if (spawn_state == DoorSpawnState::any)
        {
                // NOTE: The chances below are just generic default behavior for
                // random doors placed wherever. Doors may be explicitly set to
                // other states elsewhere during map generation (e.g. set to
                // secret to hide an optional branch of the map).

                const int pct_secret = 10 + (map::g_dlvl - 1);
                const int pct_stuck = 10;

                if ((m_type != DoorType::gate) && rnd::percent(pct_secret))
                {
                        // Secret
                        spawn_state =
                                rnd::percent(pct_stuck)
                                ? DoorSpawnState::secret_and_stuck
                                : DoorSpawnState::secret;
                }
                else
                {
                        // Not secret
                        Fraction chance_open(3, 4);

                        if (chance_open.roll())
                        {
                                spawn_state = DoorSpawnState::open;
                        }
                        else
                        {
                                // Closed
                                spawn_state =
                                        rnd::percent(pct_stuck)
                                        ? DoorSpawnState::stuck
                                        : DoorSpawnState::closed;
                        }
                }
        }

        switch (DoorSpawnState(spawn_state))
        {
        case DoorSpawnState::open:
                m_is_open = true;
                m_is_stuck = false;
                m_is_hidden = false;
                break;

        case DoorSpawnState::closed:
                m_is_open = false;
                m_is_stuck = false;
                m_is_hidden = false;
                break;

        case DoorSpawnState::stuck:
                m_is_open = false;
                m_is_stuck = true;
                m_is_hidden = false;
                break;

        case DoorSpawnState::secret:
                m_is_open = false;
                m_is_stuck = false;
                m_is_hidden = true;
                break;

        case DoorSpawnState::secret_and_stuck:
                m_is_open = false;
                m_is_stuck = true;
                m_is_hidden = true;
                break;

        case DoorSpawnState::any:
                ASSERT(false);

                m_is_open = false;
                m_is_stuck = false;
                m_is_hidden = false;
                break;
        }

}  // Door

Door::~Door()
{
        // Unlink all levers
        if (m_type == DoorType::metal)
        {
                const size_t nr_positions = map::nr_positions();
                for (size_t i = 0; i < nr_positions; ++i)
                {
                        auto* const terrain = map::g_terrain.at(i);

                        if (terrain && (terrain->id() == terrain::Id::lever))
                        {
                                auto* const lever =
                                        static_cast<Lever*>(terrain);

                                if (lever->is_linked_to(*this))
                                {
                                        lever->unlink();
                                }
                        }
                }
        }

        delete m_mimic_terrain;
}

void Door::on_hit(
        const DmgType dmg_type,
        actor::Actor* const actor,
        const int dmg)
{
        if (dmg_type == DmgType::pure)
        {
                map::put(new RubbleLow(m_pos));

                map::update_vision();

                return;
        }

        if (dmg_type == DmgType::shotgun)
        {
                if (!m_is_open)
                {
                        switch (m_type)
                        {
                        case DoorType::wood:
                        case DoorType::gate:
                        {
                                if (map::is_pos_seen_by_player(m_pos))
                                {
                                        const std::string a =
                                                m_is_hidden
                                                ? "A "
                                                : "The ";

                                        msg_log::add(
                                                a +
                                                base_name_short() +
                                                " is blown to pieces!");
                                }

                                map::put(new RubbleLow(m_pos));

                                map::update_vision();

                                return;
                        }
                        break;

                        case DoorType::metal:
                                break;
                        }
                }
        }

        if (dmg_type == DmgType::explosion)
        {
                //TODO
        }

        // Kicking, blunt (sledgehammers), or slashing (axes)
        if ((dmg_type == DmgType::kicking) ||
            (dmg_type == DmgType::blunt) ||
            (dmg_type == DmgType::slashing))
        {
                ASSERT(actor);

                const bool is_player = actor == map::g_player;
                const bool is_cell_seen = map::is_pos_seen_by_player(m_pos);
                const bool is_weak = actor->m_properties.has(PropId::weakened);

                switch (m_type)
                {
                case DoorType::wood:
                case DoorType::gate:
                {
                        if (is_player)
                        {
                                int destr_chance_pct =
                                        25 +
                                        (dmg * 5) -
                                        (m_nr_spikes * 4);

                                destr_chance_pct = std::max(1, destr_chance_pct);

                                if (player_bon::has_trait(Trait::tough))
                                {
                                        destr_chance_pct += 15;
                                }

                                if (player_bon::has_trait(Trait::rugged))
                                {
                                        destr_chance_pct += 15;
                                }

                                if (actor->m_properties.has(PropId::frenzied))
                                {
                                        destr_chance_pct += 30;
                                }

                                if (is_weak || is_hidden())
                                {
                                        destr_chance_pct = 0;
                                }

                                destr_chance_pct = std::min(100, destr_chance_pct);

                                if (destr_chance_pct > 0)
                                {
                                        if (rnd::percent(destr_chance_pct))
                                        {
                                                Snd snd(
                                                        "",
                                                        audio::SfxId::door_break,
                                                        IgnoreMsgIfOriginSeen::yes,
                                                        m_pos,
                                                        actor,
                                                        SndVol::low,
                                                        AlertsMon::yes);

                                                snd.run();

                                                if (is_cell_seen)
                                                {
                                                        if (m_is_hidden)
                                                        {
                                                                msg_log::add(
                                                                        "A " +
                                                                        base_name_short() +
                                                                        " crashes open!");
                                                        }
                                                        else
                                                        {
                                                                msg_log::add(
                                                                        "The " +
                                                                        base_name_short() +
                                                                        " crashes open!");
                                                        }
                                                }
                                                else
                                                {
                                                        // Cell not seen
                                                        msg_log::add("I feel a door crashing open!");
                                                }

                                                map::put(new RubbleLow(m_pos));

                                                map::update_vision();
                                        }
                                        else
                                        {
                                                // Not destroyed
                                                const audio::SfxId sfx =
                                                        m_is_hidden ? audio::SfxId::END : audio::SfxId::door_bang;

                                                Snd snd("",
                                                        sfx,
                                                        IgnoreMsgIfOriginSeen::no,
                                                        m_pos,
                                                        actor,
                                                        SndVol::low,
                                                        AlertsMon::yes);

                                                snd.run();
                                        }
                                }
                                else
                                {
                                        // No chance of success
                                        if (is_cell_seen && !m_is_hidden)
                                        {
                                                Snd snd("",
                                                        audio::SfxId::door_bang,
                                                        IgnoreMsgIfOriginSeen::no,
                                                        actor->m_pos,
                                                        actor,
                                                        SndVol::low,
                                                        AlertsMon::yes);

                                                snd.run();

                                                msg_log::add("It seems futile.");
                                        }
                                }
                        }
                        else
                        {
                                // Is monster
                                int destr_chance_pct = 7 - (m_nr_spikes * 2);

                                destr_chance_pct = std::max(1, destr_chance_pct);

                                if (is_weak)
                                {
                                        destr_chance_pct = 0;
                                }

                                if (rnd::percent(destr_chance_pct))
                                {
                                        // NOTE: When it's a monster bashing down the door, we
                                        // make the sound alert other monsters - since causes
                                        // nicer AI behavior (everyone near the door understands
                                        // that it's time to run inside)
                                        Snd snd("I hear a door crashing open!",
                                                audio::SfxId::door_break,
                                                IgnoreMsgIfOriginSeen::yes,
                                                m_pos,
                                                actor,
                                                SndVol::high,
                                                AlertsMon::yes);

                                        snd.run();

                                        if (actor::can_player_see_actor(*actor))
                                        {
                                                msg_log::add(
                                                        "The " +
                                                        base_name_short() +
                                                        " crashes open!");
                                        }
                                        else if (is_cell_seen)
                                        {
                                                msg_log::add(
                                                        "A " +
                                                        base_name_short() +
                                                        " crashes open!");
                                        }

                                        map::put(new RubbleLow(m_pos));

                                        map::update_vision();
                                }
                                else
                                {
                                        // Not destroyed
                                        Snd snd("I hear a loud banging.",
                                                audio::SfxId::door_bang,
                                                IgnoreMsgIfOriginSeen::yes,
                                                actor->m_pos,
                                                actor,
                                                SndVol::high,
                                                AlertsMon::no);

                                        snd.run();
                                }
                        }
                }
                break;  // wood, gate

                case DoorType::metal:
                {
                        if (is_player &&
                            is_cell_seen &&
                            !m_is_hidden)
                        {
                                msg_log::add(
                                        "It seems futile.",
                                        colors::msg_note(),
                                        MsgInterruptPlayer::no,
                                        MorePromptOnMsg::yes);
                        }
                }
                break;  // metal

                }  // Door type switch
        }

        if ((dmg_type == DmgType::fire) && (matl() == Matl::wood))
        {
                try_start_burning(Verbose::yes);
                reveal(Verbose::yes);
        }
}  // on_hit

WasDestroyed Door::on_finished_burning()
{
        if (map::is_pos_seen_by_player(m_pos))
        {
                msg_log::add("The door burns down.");
        }

        auto* const rubble = new RubbleLow(m_pos);

        rubble->m_burn_state = BurnState::has_burned;

        map::put(rubble);

        map::update_vision();

        return WasDestroyed::yes;
}

bool Door::is_walkable() const
{
        return m_is_open;
}

bool Door::can_move(const actor::Actor& actor) const
{
        if (m_is_open)
        {
                return true;
        }

        // The door is closed

        const auto& properties = actor.m_properties;

        // Can move through all door types
        if (properties.has(PropId::ethereal) ||
            properties.has(PropId::ooze))
        {
                return true;
        }

        // Small crawling creatures can pass through gates
        if ((m_type == DoorType::gate) &&
            properties.has(PropId::small_crawling))
        {
                return true;
        }

        return false;
}

bool Door::is_los_passable() const
{
        return m_is_open || (m_type == DoorType::gate);
}

bool Door::is_projectile_passable() const
{
        return m_is_open || (m_type == DoorType::gate);
}

bool Door::is_smoke_passable() const
{
        return m_is_open || (m_type == DoorType::gate);
}

std::string Door::base_name() const
{
        std::string ret;

        switch (m_type)
        {
        case DoorType::wood:
                ret = "wooden door";
                break;

        case DoorType::metal:
                ret = "metal door";
                break;

        case DoorType::gate:
                ret = "barred gate";
                break;
        }

        return ret;
}

std::string Door::base_name_short() const
{
        std::string ret;

        switch (m_type)
        {
        case DoorType::wood:
                ret = "door";
                break;

        case DoorType::metal:
                ret = "door";
                break;

        case DoorType::gate:
                ret = "barred gate";
                break;
        }

        return ret;
}

std::string Door::name(const Article article) const
{
        if (m_is_hidden)
        {
                ASSERT(m_type != DoorType::gate);
                ASSERT(m_mimic_terrain);

                return m_mimic_terrain->name(article);
        }

        std::string a;

        std::string mod;

        if (m_burn_state == BurnState::burning)
        {
                a =
                        (article == Article::a)
                        ? "a "
                        : "the ";

                mod = "burning ";
        }

        if (m_is_open)
        {
                if (a.empty())
                {
                        a =
                                (article == Article::a)
                                ? "an "
                                : "the ";
                }

                mod += "open ";
        }

        if (m_is_stuck &&
            map::g_player->m_pos.is_adjacent(m_pos) &&
            (m_type != DoorType::metal))
        {
                mod = "stuck ";
        }

        if (a.empty())
        {
                a =
                        (article == Article::a)
                        ? "a "
                        : "the ";
        }

        return a + mod + base_name();

}  // name

Color Door::color_default() const
{
        Color color;

        if (m_is_hidden)
        {
                color = m_mimic_terrain->color();
        }
        else if (
                m_is_stuck &&
                map::g_player->m_pos.is_adjacent(m_pos) &&
                (m_type != DoorType::metal))
        {
                // Non-metal door is stuck, and player is adjacent to it
                color = colors::red();
        }
        else
        {
                switch (m_type)
                {
                case DoorType::wood:
                        color = colors::dark_brown();
                        break;

                case DoorType::metal:
                        color = colors::cyan();
                        break;

                case DoorType::gate:
                        color = colors::gray();
                        break;
                }
        }

        return color;
}

char Door::character() const
{
        if (m_is_hidden)
        {
                ASSERT(m_type != DoorType::gate);
                ASSERT(m_mimic_terrain);

                return m_mimic_terrain->character();
        }
        else
        {
                return m_is_open ? 39 : '+';
        }
}

gfx::TileId Door::tile() const
{
        gfx::TileId ret = gfx::TileId::END;

        if (m_is_hidden)
        {
                ASSERT(m_type != DoorType::gate);
                ASSERT(m_mimic_terrain);

                ret = m_mimic_terrain->tile();
        }
        else
        {
                // Not secret
                switch (m_type)
                {
                case DoorType::wood:
                case DoorType::metal:
                {
                        ret =
                                m_is_open
                                ? gfx::TileId::door_open
                                : gfx::TileId::door_closed;
                }
                break;

                case DoorType::gate:
                {
                        ret =
                                m_is_open
                                ? gfx::TileId::gate_open
                                : gfx::TileId::gate_closed;
                }
                break;
                }
        }

        return ret;
}

Matl Door::matl() const
{
        switch (m_type)
        {
        case DoorType::wood:
                return Matl::wood;
                break;

        case DoorType::metal:
        case DoorType::gate:
                return Matl::metal;
                break;
        }

        ASSERT(false);

        return Matl::wood;
}

void Door::bump(actor::Actor& actor_bumping)
{
        if (!actor_bumping.is_player())
        {
                return;
        }

        if (m_is_hidden)
        {
                ASSERT(m_type != DoorType::gate);

                // Print messages as if this was a wall

                if (map::g_seen.at(m_pos))
                {
                        TRACE << "Player bumped into secret door, "
                              << "with vision in cell" << std::endl;

                        msg_log::add(
                                terrain::data(terrain::Id::wall)
                                        .msg_on_player_blocked);
                }
                else
                {
                        // Not seen by player
                        TRACE << "Player bumped into secret door, "
                              << "without vision in cell" << std::endl;

                        msg_log::add(
                                terrain::data(terrain::Id::wall)
                                        .msg_on_player_blocked_blind);
                }

                return;
        }

        if (!m_is_open)
        {
                actor_try_open(actor_bumping);
        }

}  // bump

void Door::reveal(const Verbose verbose)
{
        const bool is_hidden_before = m_is_hidden;

        m_is_hidden = false;

        if (is_hidden_before &&
            (verbose == Verbose::yes) &&
            map::g_seen.at(m_pos))
        {
                msg_log::add("A secret is revealed.");
        }

        m_is_hidden = false;
}

void Door::on_revealed_from_searching()
{
        game::incr_player_xp(2);
}

void Door::set_secret()
{
        ASSERT(m_type != DoorType::gate);

        m_is_open = false;
        m_is_hidden = true;
}

bool Door::actor_try_jam(actor::Actor& actor_trying)
{
        const bool is_player = actor_trying.is_player();

        const bool tryer_is_blind = !actor_trying.m_properties.allow_see();

        if (m_is_hidden || m_is_open)
        {
                return false;
        }

        // Door is in correct state for spiking (known, closed)
        ++m_nr_spikes;
        m_is_stuck = true;

        if (is_player)
        {
                std::string a =
                        tryer_is_blind
                        ? "a "
                        : "the ";

                msg_log::add(
                        "I jam " +
                        a +
                        base_name_short() +
                        " with a spike.");
        }

        game_time::tick();
        return true;
}

void Door::actor_try_close(actor::Actor& actor_trying)
{
        // TODO: Refactor this function

        const bool is_player = actor_trying.is_player();

        const bool tryer_is_blind = !actor_trying.m_properties.allow_see();

        if (is_player && (m_type == DoorType::metal))
        {
                if (tryer_is_blind)
                {
                        msg_log::add(
                                "There is a metal door here, but it's stuck.");
                }
                else
                {
                        msg_log::add("The door is stuck.");
                }

                msg_log::add("Perhaps it is handled elsewhere.");

                return;
        }

        const bool player_see_tryer =
                is_player
                ? true
                : actor::can_player_see_actor(actor_trying);

        // Already closed?
        if (!m_is_open)
        {
                if (is_player)
                {
                        if (tryer_is_blind)
                        {
                                msg_log::add("I find nothing there to close.");
                        }
                        else
                        {
                                // Can see
                                msg_log::add("I see nothing there to close.");
                        }
                }

                return;
        }

        // Currently being opened by another actor?
        if (m_actor_currently_opening &&
            (m_actor_currently_opening != &actor_trying))
        {
                TRACE << "Door marked as currently being opened, checking if "
                         "opening actor still exists and is alive"
                      << std::endl;

                bool is_opening_actor_alive = false;

                for (const auto* const actor : game_time::g_actors)
                {
                        if ((actor == m_actor_currently_opening) &&
                            actor->is_alive())
                        {
                                is_opening_actor_alive = true;
                        }
                }

                if (is_opening_actor_alive)
                {
                        TRACE << "Opening actor exists and is alive"
                              << std::endl;

                        if (is_player)
                        {
                                msg_log::add(
                                        "The door is currently being opened, "
                                        "and cannot be closed.");
                        }

                        return;
                }
                else
                {
                        TRACE << "Opening actor no longer exists, or is dead"
                              << std::endl;

                        m_actor_currently_opening = nullptr;
                }
        }

        // Blocked?
        bool is_blocked_by_actor = false;

        for (auto* actor : game_time::g_actors)
        {
                if ((actor->m_state != ActorState::destroyed) &&
                    (actor->m_pos == m_pos))
                {
                        is_blocked_by_actor = true;

                        break;
                }
        }

        if (is_blocked_by_actor || map::g_items.at(m_pos))
        {
                if (is_player)
                {
                        if (tryer_is_blind)
                        {
                                msg_log::add(
                                        "Something is blocking the " +
                                        base_name_short() +
                                        ".");
                        }
                        else
                        {
                                // Can see
                                msg_log::add(
                                        "The " +
                                        base_name_short() +
                                        " is blocked.");
                        }
                }

                return;
        }

        // Door can be closed

        if (tryer_is_blind)
        {
                if (rnd::coin_toss())
                {
                        m_is_open = false;

                        map::update_vision();

                        if (is_player)
                        {
                                Snd snd(
                                        "",
                                        audio::SfxId::door_close,
                                        IgnoreMsgIfOriginSeen::yes,
                                        m_pos,
                                        &actor_trying,
                                        SndVol::low,
                                        AlertsMon::yes);

                                snd.run();

                                msg_log::add(
                                        "I fumble with a " +
                                        base_name_short() +
                                        ", but manage to close it.");
                        }
                        else
                        {
                                // Monster closing
                                Snd snd(
                                        "I hear a door closing.",
                                        audio::SfxId::door_close,
                                        IgnoreMsgIfOriginSeen::yes,
                                        m_pos,
                                        &actor_trying,
                                        SndVol::low,
                                        AlertsMon::no);

                                snd.run();

                                if (player_see_tryer)
                                {
                                        const std::string actor_name_the =
                                                text_format::first_to_upper(
                                                        actor_trying.name_the());

                                        msg_log::add(
                                                actor_name_the +
                                                "fumbles, but manages to close a " +
                                                base_name_short() +
                                                ".");
                                }
                        }

                        game_time::tick();
                }
                else
                {
                        // Failed to close
                        if (is_player)
                        {
                                msg_log::add(
                                        "I fumble blindly with a " +
                                        base_name_short() +
                                        ", and fail to close it.");
                        }
                        else
                        {
                                // Monster failing to close
                                if (player_see_tryer)
                                {
                                        const std::string actor_name_the =
                                                text_format::first_to_upper(
                                                        actor_trying.name_the());

                                        msg_log::add(
                                                actor_name_the +
                                                " fumbles blindly, and fails to close a " +
                                                base_name_short() +
                                                ".");
                                }
                        }

                        game_time::tick();
                }

                return;
        }

        // Door can be closed, and actor can see

        m_is_open = false;

        map::update_vision();

        if (is_player)
        {
                if (!player_bon::has_trait(Trait::silent))
                {
                        Snd snd(
                                "",
                                audio::SfxId::door_close,
                                IgnoreMsgIfOriginSeen::yes,
                                m_pos,
                                &actor_trying,
                                SndVol::low,
                                AlertsMon::yes);

                        snd.run();
                }

                msg_log::add(
                        "I close the " +
                        base_name_short() +
                        ".");
        }
        else
        {
                // Monster closing
                Snd snd(
                        "I hear a door closing.",
                        audio::SfxId::door_close,
                        IgnoreMsgIfOriginSeen::yes,
                        m_pos,
                        &actor_trying,
                        SndVol::low,
                        AlertsMon::no);

                snd.run();

                if (player_see_tryer)
                {
                        const std::string actor_name_the =
                                text_format::first_to_upper(
                                        actor_trying.name_the());

                        msg_log::add(
                                actor_name_the +
                                " closes a " +
                                base_name_short() +
                                ".");
                }
        }

        game_time::tick();

}  // actor_try_close

void Door::actor_try_open(actor::Actor& actor_trying)
{
        TRACE_FUNC_BEGIN;

        const bool is_player = actor_trying.is_player();
        const bool player_see_door = map::g_seen.at(m_pos);

        const bool player_see_tryer =
                is_player
                ? true
                : actor::can_player_see_actor(actor_trying);

        if (is_player && (m_type == DoorType::metal))
        {
                if (!player_see_door)
                {
                        msg_log::add("There is a closed metal door here.");
                }

                msg_log::add("I find no way to open it.");

                msg_log::add("Perhaps it is handled elsewhere.");

                return;
        }

        if (m_is_stuck)
        {
                TRACE << "Is stuck" << std::endl;

                if (is_player)
                {
                        msg_log::add(
                                "The " +
                                base_name_short() +
                                " seems to be stuck.");
                }
        }
        else
        {
                // Not stuck
                TRACE << "Is not stuck" << std::endl;

                const bool tryer_can_see =
                        actor_trying.m_properties.allow_see();

                if (tryer_can_see)
                {
                        TRACE << "Tryer can see, opening" << std::endl;
                        m_is_open = true;

                        if (is_player)
                        {
                                if (!player_bon::has_trait(Trait::silent))
                                {
                                        Snd snd(
                                                "",
                                                audio::SfxId::door_open,
                                                IgnoreMsgIfOriginSeen::yes,
                                                m_pos,
                                                &actor_trying,
                                                SndVol::low,
                                                AlertsMon::yes);

                                        snd.run();
                                }

                                msg_log::add(
                                        "I open the " +
                                        base_name_short() +
                                        ".");
                        }
                        else
                        {
                                // Is monster
                                Snd snd(
                                        "I hear a door open.",
                                        audio::SfxId::door_open,
                                        IgnoreMsgIfOriginSeen::yes,
                                        m_pos,
                                        &actor_trying,
                                        SndVol::low,
                                        AlertsMon::no);

                                snd.run();

                                if (player_see_tryer)
                                {
                                        const std::string actor_name_the =
                                                text_format::first_to_upper(
                                                        actor_trying.name_the());

                                        msg_log::add(
                                                actor_name_the +
                                                " opens a " +
                                                base_name_short() +
                                                ".");
                                }
                                else if (player_see_door)
                                {
                                        msg_log::add(
                                                "I see a " +
                                                base_name_short() +
                                                " opening.");
                                }
                        }
                }
                else
                {
                        // Tryer is blind
                        if (rnd::coin_toss())
                        {
                                TRACE << "Tryer is blind, but open succeeded anyway"
                                      << std::endl;

                                m_is_open = true;

                                if (is_player)
                                {
                                        Snd snd(
                                                "",
                                                audio::SfxId::door_open,
                                                IgnoreMsgIfOriginSeen::yes,
                                                m_pos,
                                                &actor_trying,
                                                SndVol::low,
                                                AlertsMon::yes);

                                        snd.run();

                                        msg_log::add(
                                                "I fumble with a " +
                                                base_name_short() +
                                                ", but finally manage to open it.");
                                }
                                else
                                {
                                        // Is monster
                                        Snd snd(
                                                "I hear something open a door awkwardly.",
                                                audio::SfxId::door_open,
                                                IgnoreMsgIfOriginSeen::yes,
                                                m_pos,
                                                &actor_trying,
                                                SndVol::low,
                                                AlertsMon::no);

                                        snd.run();

                                        if (player_see_tryer)
                                        {
                                                const std::string actor_name_the =
                                                        text_format::first_to_upper(
                                                                actor_trying.name_the());

                                                msg_log::add(
                                                        actor_name_the +
                                                        "fumbles, but manages to open a " +
                                                        base_name_short() +
                                                        ".");
                                        }
                                        else if (player_see_door)
                                        {
                                                msg_log::add(
                                                        "I see a " +
                                                        base_name_short() +
                                                        " open awkwardly.");
                                        }
                                }
                        }
                        else
                        {
                                // Failed to open
                                TRACE << "Tryer is blind, and open failed" << std::endl;

                                if (is_player)
                                {
                                        Snd snd(
                                                "",
                                                audio::SfxId::END,
                                                IgnoreMsgIfOriginSeen::yes,
                                                m_pos,
                                                &actor_trying,
                                                SndVol::low,
                                                AlertsMon::yes);

                                        snd.run();

                                        msg_log::add(
                                                "I fumble blindly with a " +
                                                base_name_short() +
                                                ", and fail to open it.");
                                }
                                else
                                {
                                        // Is monster

                                        // Emitting the sound from the actor instead of the door,
                                        // because the sound message should be received even if the
                                        // door is seen
                                        Snd snd(
                                                "I hear something attempting to open a door.",
                                                audio::SfxId::END,
                                                IgnoreMsgIfOriginSeen::yes,
                                                actor_trying.m_pos,
                                                &actor_trying,
                                                SndVol::low,
                                                AlertsMon::no);

                                        snd.run();

                                        if (player_see_tryer)
                                        {
                                                const std::string actor_name_the =
                                                        text_format::first_to_upper(
                                                                actor_trying.name_the());

                                                msg_log::add(
                                                        actor_name_the +
                                                        " fumbles blindly, and fails to open a " +
                                                        base_name_short() +
                                                        ".");
                                        }
                                }

                                game_time::tick();
                        }
                }
        }

        if (m_is_open)
        {
                TRACE << "Open was successful" << std::endl;

                if (m_is_hidden)
                {
                        TRACE << "Was secret, now revealing" << std::endl;

                        reveal(Verbose::yes);
                }

                m_actor_currently_opening = &actor_trying;

                actor_trying.m_opening_door_pos = m_pos;

                game_time::tick();

                map::update_vision();
        }

}  // actor_try_open

void Door::on_lever_pulled(Lever* const lever)
{
        (void)lever;

        if (m_is_open)
        {
                close(nullptr);
        }
        else
        {
                open(nullptr);
        }
}

DidOpen Door::open(actor::Actor* const actor_opening)
{
        (void)actor_opening;

        m_is_open = true;
        m_is_hidden = false;
        m_is_stuck = false;

        if (actor_opening)
        {
                m_actor_currently_opening = actor_opening;
                actor_opening->m_opening_door_pos = m_pos;
        }

        if (m_type == DoorType::metal)
        {
                Snd snd(
                        "",
                        audio::SfxId::END,
                        IgnoreMsgIfOriginSeen::yes,
                        m_pos,
                        nullptr,
                        SndVol::low,
                        AlertsMon::yes);

                snd.run();
        }

        return DidOpen::yes;
}

DidClose Door::close(actor::Actor* const actor_closing)
{
        (void)actor_closing;

        m_is_open = false;

        if (m_type == DoorType::metal)
        {
                Snd snd(
                        "",
                        audio::SfxId::END,
                        IgnoreMsgIfOriginSeen::yes,
                        m_pos,
                        nullptr,
                        SndVol::low,
                        AlertsMon::yes);

                snd.run();
        }

        return DidClose::yes;
}

}  // namespace terrain

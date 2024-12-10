// =============================================================================
// Copyright 2011-2024 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "terrain_door.hpp"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <ostream>
#include <unordered_map>
#include <utility>
#include <vector>

#include "actor.hpp"
#include "actor_data.hpp"
#include "actor_see.hpp"
#include "array2.hpp"
#include "audio.hpp"
#include "audio_data.hpp"
#include "bash.hpp"
#include "common_text.hpp"
#include "debug.hpp"
#include "game.hpp"
#include "game_time.hpp"
#include "inventory.hpp"
#include "item.hpp"
#include "item_data.hpp"
#include "item_factory.hpp"
#include "item_weapon.hpp"
#include "map.hpp"
#include "minimap.hpp"
#include "msg_log.hpp"
#include "player_bon.hpp"
#include "pos.hpp"
#include "property_data.hpp"
#include "property_factory.hpp"
#include "property_handler.hpp"
#include "query.hpp"
#include "random.hpp"
#include "sound.hpp"
#include "terrain_data.hpp"
#include "terrain_event.hpp"
#include "terrain_factory.hpp"
#include "text_format.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static audio::SfxId get_bang_sfx(const terrain::DoorType type)
{
        switch (type) {
        case terrain::DoorType::wood:
                return audio::SfxId::door_bang;
                break;

        case terrain::DoorType::gate:
                return audio::SfxId::door_bang_gate;
                break;

        case terrain::DoorType::metal:
                return audio::SfxId::door_bang_metal;
                break;
        }

        ASSERT(false);
        return audio::SfxId::END;
}

static audio::SfxId get_break_sfx(const terrain::DoorType type)
{
        switch (type) {
        case terrain::DoorType::wood:
                return audio::SfxId::door_break;
                break;

        case terrain::DoorType::gate:
                return audio::SfxId::door_break_gate;
                break;

        case terrain::DoorType::metal:
                // Cannot be bashed open.
                return audio::SfxId::END;
                break;
        }

        ASSERT(false);
        return audio::SfxId::END;
}

static audio::SfxId get_close_sfx(const terrain::DoorType type)
{
        switch (type) {
        case terrain::DoorType::wood:
                return audio::SfxId::door_close;
                break;

        case terrain::DoorType::gate:
                return audio::SfxId::door_close_gate;
                break;

        case terrain::DoorType::metal:
                return audio::SfxId::door_close_metal;
                break;
        }

        ASSERT(false);
        return audio::SfxId::END;
}

static audio::SfxId get_open_sfx(const terrain::DoorType type)
{
        switch (type) {
        case terrain::DoorType::wood:
                return audio::SfxId::door_open;
                break;

        case terrain::DoorType::gate:
                return audio::SfxId::door_open_gate;
                break;

        case terrain::DoorType::metal:
                return audio::SfxId::door_open_metal;
                break;
        }

        ASSERT(false);
        return audio::SfxId::END;
}

static terrain::DoorSpawnState get_random_spawn_state(
        const terrain::DoorType door_type)
{
        // NOTE: The chances below are just generic default behavior for random
        // doors placed wherever. Doors may be explicitly set to other states
        // elsewhere during map generation.

        const int pct_secret = 10 + (map::g_dlvl - 1);
        const int pct_stuck = 10;

        const bool is_gate = (door_type == terrain::DoorType::gate);
        const bool is_metal = (door_type == terrain::DoorType::metal);

        if (!is_gate && rnd::percent(pct_secret)) {
                // Secret.

                if (!is_metal && rnd::percent(pct_stuck)) {
                        return terrain::DoorSpawnState::secret_and_stuck;
                }
                else {
                        return terrain::DoorSpawnState::secret;
                }
        }
        else {
                // Not secret.

                if (rnd::coin_toss()) {
                        return terrain::DoorSpawnState::open;
                }
                else if (!is_metal && rnd::percent(pct_stuck)) {
                        return terrain::DoorSpawnState::stuck;
                }
                else {
                        return terrain::DoorSpawnState::closed;
                }
        }
}

static int calc_player_bash_chance(
        const int dmg,
        const DmgType dmg_type,
        const int jam_lvl)
{
        int destr_chance_pct = 25 + (dmg * 5) - (jam_lvl * 4);

        destr_chance_pct = std::max(1, destr_chance_pct);

        if (dmg_type != DmgType::control_object_spell) {
                if (player_bon::has_trait(Trait::tough)) {
                        destr_chance_pct += 15;
                }

                if (player_bon::has_trait(Trait::rugged)) {
                        destr_chance_pct += 15;
                }

                if (map::g_player->m_properties.has(prop::Id::frenzied)) {
                        destr_chance_pct += 30;
                }
        }

        destr_chance_pct = std::min(100, destr_chance_pct);

        return destr_chance_pct;
}

static int calc_mon_bash_chance(const int jam_level)
{
        int destr_chance_pct = 7 - (jam_level * 2);

        destr_chance_pct = std::max(1, destr_chance_pct);

        return destr_chance_pct;
}

static void communicate_player_bash_futile(const audio::SfxId sfx)
{
        Snd snd(
                "",
                sfx,
                IgnoreMsgIfOriginSeen::no,
                map::g_player->m_pos,
                map::g_player,
                SndVol::low,
                AlertsMon::yes);

        snd.run();

        msg_log::add("It seems futile.");
}

static void communicate_player_bash_success(
        const P& pos,
        const audio::SfxId sfx,
        const std::string& door_name,
        const std::string& break_descr,
        const bool is_hidden)
{
        Snd snd(
                "",
                sfx,
                IgnoreMsgIfOriginSeen::yes,
                pos,
                map::g_player,
                SndVol::low,
                AlertsMon::yes);

        snd.run();

        if (is_hidden) {
                msg_log::add("A " + door_name + " crashes " + break_descr + "!");
        }
        else {
                msg_log::add("The " + door_name + " crashes " + break_descr + "!");
        }
}

static void communicate_mon_bash_success(
        const P& pos,
        const audio::SfxId sfx,
        const std::string& door_name,
        const std::string& break_descr,
        actor::Actor& mon)
{
        const bool is_actor_seen = actor::can_player_see_actor(mon);
        const bool is_door_seen = map::g_seen.at(pos);

        const std::string snd_msg =
                (is_actor_seen || is_door_seen)
                ? ""
                : "I hear a door crashing open!";

        // NOTE: When it's a monster bashing down the door, we make the
        // sound alert other monsters since this causes nicer AI
        // behavior (everyone near the door wants to run inside).
        Snd snd(
                snd_msg,
                sfx,
                IgnoreMsgIfOriginSeen::yes,
                pos,
                &mon,
                SndVol::high,
                AlertsMon::yes);

        snd.run();

        if (is_actor_seen) {
                msg_log::add("The " + door_name + " crashes " + break_descr + "!");
        }
        else if (is_door_seen) {
                msg_log::add("A " + door_name + " crashes " + break_descr + "!");

                mon.make_player_aware_of_me();
        }
}

static void communicate_player_bash_failed(
        const P& pos,
        const audio::SfxId sfx)
{
        Snd snd(
                "",
                sfx,
                IgnoreMsgIfOriginSeen::no,
                pos,
                map::g_player,
                SndVol::low,
                AlertsMon::yes);

        snd.run();
}

static void communicate_mon_bash_failed(
        const P& pos,
        const audio::SfxId sfx,
        actor::Actor& mon)
{
        Snd snd(
                "I hear a loud banging.",
                sfx,
                IgnoreMsgIfOriginSeen::yes,
                pos,
                &mon,
                SndVol::high,
                AlertsMon::no);

        snd.run();
}

static void communicate_player_open(
        const P& pos,
        const audio::SfxId sfx,
        const std::string& door_name)
{
        if (!player_bon::has_trait(Trait::silent)) {
                Snd snd(
                        "",
                        sfx,
                        IgnoreMsgIfOriginSeen::yes,
                        pos,
                        map::g_player,
                        SndVol::low,
                        AlertsMon::yes);

                snd.run();
        }

        msg_log::add("I open the " + door_name + ".");
}

static void communicate_mon_open(
        const P& pos,
        const audio::SfxId sfx,
        const std::string& door_name,
        actor::Actor& mon)
{
        const bool is_actor_seen = actor::can_player_see_actor(mon);
        const bool is_door_seen = map::g_seen.at(pos);

        const std::string snd_msg =
                (is_actor_seen || is_door_seen)
                ? ""
                : "I hear a door open.";

        Snd snd(
                snd_msg,
                sfx,
                IgnoreMsgIfOriginSeen::no,
                pos,
                &mon,
                SndVol::low,
                AlertsMon::no);

        snd.run();

        if (is_actor_seen) {
                const std::string actor_name_the =
                        text_format::first_to_upper(
                                actor::name_the(mon));

                msg_log::add(actor_name_the + " opens a " + door_name + ".");
        }
        else if (is_door_seen) {
                msg_log::add("I see a " + door_name + " opening.");

                mon.make_player_aware_of_me();
        }
}

static void communicate_player_open_blind(
        const P& pos,
        const audio::SfxId sfx,
        const std::string& door_name)
{
        Snd snd(
                "",
                sfx,
                IgnoreMsgIfOriginSeen::yes,
                pos,
                map::g_player,
                SndVol::low,
                AlertsMon::yes);

        snd.run();

        msg_log::add("I fumble with a " + door_name + ", but manage to open it.");
}

static void communicate_mon_open_blind(
        const P& pos,
        const audio::SfxId sfx,
        const std::string& door_name,
        actor::Actor& mon)
{
        const bool is_actor_seen = actor::can_player_see_actor(mon);
        const bool is_door_seen = map::g_seen.at(pos);

        const std::string snd_msg =
                (is_actor_seen || is_door_seen)
                ? ""
                : "I hear something open a door awkwardly.";

        Snd snd(
                snd_msg,
                sfx,
                IgnoreMsgIfOriginSeen::no,
                pos,
                &mon,
                SndVol::low,
                AlertsMon::no);

        snd.run();

        if (is_actor_seen) {
                const std::string actor_name_the =
                        text_format::first_to_upper(
                                actor::name_the(mon));

                msg_log::add(
                        actor_name_the +
                        "fumbles, but manages to open a " +
                        door_name +
                        ".");
        }
        else if (is_door_seen) {
                msg_log::add("I see a " + door_name + " open awkwardly.");

                mon.make_player_aware_of_me();
        }
}

static void communicate_player_fail_open_blind(
        const P& pos,
        const std::string& door_name)
{
        Snd snd(
                "",
                audio::SfxId::END,
                IgnoreMsgIfOriginSeen::yes,
                pos,
                map::g_player,
                SndVol::low,
                AlertsMon::yes);

        snd.run();

        msg_log::add(
                "I fumble blindly with a " +
                door_name +
                ", and fail to open it.");
}

static void communicate_mon_fail_open_blind(
        const P& pos,
        const std::string& door_name,
        actor::Actor& mon)
{
        Snd snd(
                "I hear something attempting to open a door.",
                audio::SfxId::END,
                IgnoreMsgIfOriginSeen::yes,
                pos,
                &mon,
                SndVol::low,
                AlertsMon::no);

        snd.run();

        const bool is_actor_seen = actor::can_player_see_actor(mon);

        if (is_actor_seen) {
                const std::string actor_name_the =
                        text_format::first_to_upper(
                                actor::name_the(mon));

                msg_log::add(
                        actor_name_the +
                        " fumbles blindly, and fails to open a " +
                        door_name +
                        ".");
        }
}

static void communicate_player_close(
        const P& pos,
        const audio::SfxId sfx,
        const std::string& door_name)
{
        if (!player_bon::has_trait(Trait::silent)) {
                Snd snd(
                        "",
                        sfx,
                        IgnoreMsgIfOriginSeen::yes,
                        pos,
                        map::g_player,
                        SndVol::low,
                        AlertsMon::yes);

                snd.run();
        }

        msg_log::add("I close the " + door_name + ".");
}

static void communicate_player_close_blind(
        const P& pos,
        const audio::SfxId sfx,
        const std::string& door_name)
{
        Snd snd(
                "",
                sfx,
                IgnoreMsgIfOriginSeen::yes,
                pos,
                map::g_player,
                SndVol::low,
                AlertsMon::yes);

        snd.run();

        msg_log::add("I fumble with a " + door_name + ", but manage to close it.");
}

static void communicate_player_fail_close_blind(
        const P& pos,
        const std::string& door_name)
{
        Snd snd(
                "",
                audio::SfxId::END,
                IgnoreMsgIfOriginSeen::yes,
                pos,
                map::g_player,
                SndVol::low,
                AlertsMon::yes);

        snd.run();

        msg_log::add(
                "I fumble blindly with a " +
                door_name +
                ", and fail to close it.");
}

static std::vector<std::string> get_warded_door_summon_bucket_for_dlvl_range(
        const Range range)
{
        std::vector<std::string> summon_bucket;

        summon_bucket.reserve(actor::g_data.size());

        for (const auto& it : actor::g_data) {
                const actor::ActorData& data = it.second;

                if (data.can_be_summoned_by_mon) {
                        if (range.is_in_range(data.spawn_min_dlvl)) {
                                summon_bucket.push_back(data.id);
                        }
                }
        }

        return summon_bucket;
}

static std::vector<std::string> get_warded_door_summon_bucket()
{
        // First, try with a narrow spawn range (it is best if the monsters
        // spawned have an allowed minimum spawn level near the current dungeon
        // level, so that they aren't too weak or powerful).
        std::vector<std::string> summon_bucket =
                get_warded_door_summon_bucket_for_dlvl_range(
                        Range(map::g_dlvl - 2, map::g_dlvl + 2));

        if (!summon_bucket.empty()) {
                // At least one allowed monster found, good!

                TRACE
                        << "Found '"
                        << summon_bucket.size()
                        << "' allowed monsters in narrow spawn range"
                        << std::endl;

                return summon_bucket;
        }

        // No monsters found yet, do a second try with a wider range.
        summon_bucket =
                get_warded_door_summon_bucket_for_dlvl_range(
                        Range(map::g_dlvl - 6, map::g_dlvl + 2));

        TRACE
                << "Found '"
                << summon_bucket.size()
                << "' allowed monsters in wide spawn range"
                << std::endl;

        return summon_bucket;
}

// -----------------------------------------------------------------------------
// terrain
// -----------------------------------------------------------------------------
namespace terrain
{
Door::~Door()
{
        if (m_ward_state == WardState::warded) {
                const size_t nr_positions = map::nr_positions();

                for (size_t i = 0; i < nr_positions; ++i) {
                        Terrain* const terrain = map::g_terrain.at(i);

                        if (terrain && (terrain->id() == terrain::Id::crystal_key)) {
                                auto* const crystal = static_cast<CrystalKey*>(terrain);

                                if (crystal->is_linked_to(this)) {
                                        crystal->unlink();
                                }
                        }
                }
        }

        delete m_mimic_terrain;
}

void Door::init_type_and_state(const DoorType type, DoorSpawnState spawn_state)
{
        m_type = type;

        // Gates should never be secret.
        ASSERT(!((m_type == DoorType::gate) && m_mimic_terrain));

        ASSERT(
                !((m_type == DoorType::gate) &&
                  ((spawn_state == DoorSpawnState::secret) ||
                   (spawn_state == DoorSpawnState::secret_and_stuck))));

        // Metal doors should never be stuck.
        ASSERT(
                !((m_type == DoorType::metal) &&
                  ((spawn_state == DoorSpawnState::stuck) ||
                   (spawn_state == DoorSpawnState::secret_and_stuck))));

        // Only wooden doors can be warded.
        ASSERT(!((spawn_state == DoorSpawnState::warded) && (m_type != DoorType::wood)));

        if (spawn_state == DoorSpawnState::any) {
                spawn_state = get_random_spawn_state(m_type);
        }

        switch (spawn_state) {
        case DoorSpawnState::open:
                m_is_open = true;
                m_is_stuck = false;
                m_is_hidden = false;
                m_ward_state = WardState::not_warded;
                break;

        case DoorSpawnState::closed:
                m_is_open = false;
                m_is_stuck = false;
                m_is_hidden = false;
                m_ward_state = WardState::not_warded;
                break;

        case DoorSpawnState::stuck:
                m_is_open = false;
                m_is_stuck = true;
                m_is_hidden = false;
                m_ward_state = WardState::not_warded;
                break;

        case DoorSpawnState::secret:
                m_is_open = false;
                m_is_stuck = false;
                m_is_hidden = true;
                m_ward_state = WardState::not_warded;
                break;

        case DoorSpawnState::secret_and_stuck:
                m_is_open = false;
                m_is_stuck = true;
                m_is_hidden = true;
                m_ward_state = WardState::not_warded;
                break;

        case DoorSpawnState::warded:
                m_is_open = false;
                m_is_stuck = false;
                m_is_hidden = false;
                m_ward_state = WardState::warded;
                break;

        case DoorSpawnState::any:
                // Should have been set to specific state previously.
                ASSERT(false);
                m_is_open = false;
                m_is_stuck = false;
                m_is_hidden = false;
                m_ward_state = WardState::not_warded;
                break;
        }
}

bool Door::allow_player_melee_attack(
        const DmgType dmg_type,
        const item::Item& wpn) const
{
        if (m_is_open) {
                return false;
        }
        else if (m_is_hidden) {
                // Emulate walls.
                return m_mimic_terrain->allow_player_melee_attack(dmg_type, wpn);
        }
        else {
                // Revealed door.
                switch (m_type) {
                case terrain::DoorType::wood: {
                        return wpn.data().melee.can_attack_door_wood;
                } break;

                case terrain::DoorType::gate: {
                        return wpn.data().melee.can_attack_door_gate;
                } break;

                case terrain::DoorType::metal: {
                        return false;
                } break;
                }

                return false;
        }
}

void Door::hit(
        DmgType dmg_type,
        actor::Actor* actor,
        const P& from_pos,
        int dmg)
{
        (void)from_pos;

        switch (dmg_type) {
        case DmgType::pure: {
                const P pos = m_pos;

                // NOTE: This might destroy the door:
                try_trigger_ward_trap();

                map::update_terrain(make(Id::rubble_low, pos));
                map::update_vision();

                return;
        } break;

        case DmgType::shotgun: {
                if (!m_is_open) {
                        switch (m_type) {
                        case DoorType::wood:
                        case DoorType::gate: {
                                if (map::g_seen.at(m_pos)) {
                                        const std::string a =
                                                m_is_hidden
                                                ? "A "
                                                : "The ";

                                        msg_log::add(
                                                a +
                                                base_name_short() +
                                                " is blown to pieces!");
                                }

                                const P pos = m_pos;

                                // NOTE: This might destroy the door:
                                try_trigger_ward_trap();

                                map::update_terrain(make(Id::rubble_low, pos));
                                map::update_vision();

                                return;
                        } break;

                        case DoorType::metal:
                                break;
                        }
                }
        } break;

        case DmgType::explosion: {
                const P pos = m_pos;

                // NOTE: This might destroy the door:
                try_trigger_ward_trap();

                map::update_terrain(terrain::make(terrain::Id::rubble_low, pos));

                map::update_vision();
        } break;

        // Kicking, blunt (sledgehammers), slashing (axes), or the control
        // object spell.
        case DmgType::kicking:
        case DmgType::blunt:
        case DmgType::slashing:
        case DmgType::control_object_spell: {
                if (!actor) {
                        ASSERT(false);

                        return;
                }

                bash(dmg_type, *actor, dmg);
        } break;

        case DmgType::fire: {
                // TODO: Warded doors do not burn, but consider printing some
                // message about the door resisting fire.
                if ((material() == Material::wood) && (m_ward_state != WardState::warded)) {
                        reveal(PrintRevealMsg::if_seen);

                        try_start_burning(Verbose::yes);
                }
        } break;

        default: {
        } break;
        }
}  // on_hit

void Door::bash(const DmgType dmg_type, actor::Actor& actor, const int dmg)
{
        const bool is_player = actor::is_player(&actor);
        const bool is_pos_seen = map::g_seen.at(m_pos);

        if ((m_type == DoorType::metal) &&
            is_player &&
            is_pos_seen &&
            !m_is_hidden) {
                msg_log::add("It seems futile.", colors::msg_note());

                return;
        }

        if (is_player) {
                player_bash(dmg_type, dmg);
        }
        else {
                mon_bash(actor);
        }
}

void Door::player_bash(const DmgType dmg_type, const int dmg)
{
        const bool is_weak = map::g_player->m_properties.has(prop::Id::weakened);
        const bool is_ctrl_obj = (dmg_type == DmgType::control_object_spell);

        int destr_chance_pct = 0;

        // Calculate a (non-zero) chance if:
        // * The door is seen, AND
        // * The player is not weak, or is using the "control object" spell
        if (!m_is_hidden && (!is_weak || is_ctrl_obj)) {
                destr_chance_pct = calc_player_bash_chance(dmg, dmg_type, m_jam_level);
        }

        const audio::SfxId sfx_door_bang = get_bang_sfx(m_type);
        const audio::SfxId sfx_door_break = get_break_sfx(m_type);

        const std::string break_descr = (m_type == DoorType::gate) ? "to the floor" : "open";

        if (destr_chance_pct <= 0) {
                if (map::g_seen.at(m_pos) && !m_is_hidden) {
                        communicate_player_bash_futile(sfx_door_bang);
                }
        }
        else if (rnd::percent(destr_chance_pct)) {
                communicate_player_bash_success(
                        m_pos,
                        sfx_door_break,
                        base_name_short(),
                        break_descr,
                        m_is_hidden);

                const P pos = m_pos;

                // NOTE: This might destroy the door:
                try_trigger_ward_trap();

                map::update_terrain(make(Id::rubble_low, pos));

                map::memorize_terrain_at(pos);
                map::update_vision();
        }
        else {
                const audio::SfxId sfx = m_is_hidden ? audio::SfxId::END : sfx_door_bang;

                communicate_player_bash_failed(m_pos, sfx);
        }
}

void Door::mon_bash(actor::Actor& mon)
{
        ASSERT(m_ward_state != WardState::warded);

        const bool is_weak = mon.m_properties.has(prop::Id::weakened);

        int destr_chance_pct = 0;

        // NOTE: Unlike the player, monsters can (and will), attempt to bash
        // open metal doors, which should never succeed.
        if (!is_weak && (m_type != DoorType::metal)) {
                destr_chance_pct = calc_mon_bash_chance(m_jam_level);
        }

        const audio::SfxId sfx_door_bang = get_bang_sfx(m_type);
        const audio::SfxId sfx_door_break = get_break_sfx(m_type);

        const std::string break_descr = (m_type == DoorType::gate) ? "to the floor" : "open";

        if (rnd::percent(destr_chance_pct)) {
                // Destroyed

                communicate_mon_bash_success(
                        m_pos,
                        sfx_door_break,
                        base_name_short(),
                        break_descr,
                        mon);

                const P pos = m_pos;

                map::update_terrain(make(Id::rubble_low, m_pos));

                const bool is_actor_seen = actor::can_player_see_actor(mon);
                const bool is_door_seen = map::g_seen.at(pos);

                if (is_actor_seen || is_door_seen) {
                        map::memorize_terrain_at(pos);
                }

                map::update_vision();
        }
        else {
                // Not destroyed

                communicate_mon_bash_failed(m_pos, sfx_door_bang, mon);
        }
}

WasDestroyed Door::on_finished_burning()
{
        // Warded doors do not burn.
        ASSERT(m_ward_state != WardState::warded);

        if (map::g_seen.at(m_pos)) {
                msg_log::add("The door burns down.");
        }

        terrain::Terrain* const t = make(Id::rubble_low, m_pos);

        t->m_burn_state = BurnState::has_burned;

        map::update_terrain(t);

        map::update_vision();

        return WasDestroyed::yes;
}

bool Door::is_walkable() const
{
        return m_is_open;
}

bool Door::can_move(const actor::Actor& actor) const
{
        if (m_is_open) {
                return true;
        }

        // The door is closed

        const prop::PropHandler& properties = actor.m_properties;

        // Can move through all door types.
        if (properties.has(prop::Id::ethereal) || properties.has(prop::Id::ooze)) {
                return true;
        }

        // Small crawling creatures can pass through gates.
        if ((m_type == DoorType::gate) && properties.has(prop::Id::small_crawling)) {
                return true;
        }

        return false;
}

bool Door::is_property_allowing_move(prop::Id id) const
{
        // Can move through all door types.
        if ((id == prop::Id::ethereal) || (id == prop::Id::ooze)) {
                return true;
        }

        // Small creatures can pass through gates
        const bool is_gate = (m_type == DoorType::gate);

        const bool is_small_creature =
                (id == prop::Id::small_crawling) ||
                (id == prop::Id::tiny_flying);

        if (is_gate && is_small_creature) {
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

        switch (m_type) {
        case DoorType::wood:
                switch (m_ward_state) {
                case WardState::not_warded:
                        return "wooden door";
                        break;

                case WardState::warded:
                        return "warded door";
                        break;

                case WardState::unwarded:
                        return "unwarded door";
                        break;
                };
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

        switch (m_type) {
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
        if (m_is_hidden) {
                ASSERT(m_type != DoorType::gate);
                ASSERT(m_mimic_terrain);

                return m_mimic_terrain->name(article);
        }

        std::string a;
        std::string mod;

        if (m_burn_state == BurnState::burning) {
                a = (article == Article::a) ? "a " : "the ";

                mod = "burning ";
        }

        if (m_is_open) {
                if (a.empty()) {
                        a = (article == Article::a) ? "an " : "the ";
                }

                mod += "open ";
        }

        if (m_is_stuck && m_is_known_stuck) {
                mod = "stuck ";
        }

        if (a.empty()) {
                if (m_ward_state == WardState::unwarded) {
                        a = (article == Article::a) ? "an " : "the ";
                }
                else {
                        a = (article == Article::a) ? "a " : "the ";
                }
        }

        return a + mod + base_name();

}  // name

Color Door::color_default() const
{
        if (m_is_hidden) {
                return m_mimic_terrain->color();
        }

        Color color;

        switch (m_type) {
        case DoorType::wood:
                switch (m_ward_state) {
                case WardState::not_warded:
                        return colors::dark_brown();
                        break;

                case WardState::warded:
                        return colors::light_red();
                        break;

                case WardState::unwarded:
                        return colors::gray_brown();
                        break;
                };
                break;

        case DoorType::metal:
                color = colors::cyan();
                break;

        case DoorType::gate:
                color = colors::gray();
                break;
        }

        if (m_is_stuck && (m_type != DoorType::metal)) {
                color = color.shaded(20);
        }

        return color;
}

std::optional<map::MinimapAppearance> Door::minimap_appearance() const
{
        if (is_hidden()) {
                return {};
        }

        map::MinimapAppearance appearance;

        appearance.symbol = map::MinimapSymbol::rectangle_filled;

        if (type() == terrain::DoorType::metal) {
                appearance.color = colors::light_teal();
                appearance.legend_text = "Door (metal)";
        }
        else if (is_warded()) {
                appearance.color = colors::light_red();
                appearance.legend_text = "Door (warded)";
        }
        else {
                appearance.color = colors::light_white();
                appearance.legend_text = "Door";
        }

        return appearance;
}

char Door::character() const
{
        if (m_is_hidden) {
                ASSERT(m_type != DoorType::gate);
                ASSERT(m_mimic_terrain);

                return m_mimic_terrain->character();
        }

        const bool is_metal = (m_type == DoorType::metal);

        if (m_is_open) {
                return 39;
        }
        else if (m_ward_state != WardState::not_warded) {
                return '+';
        }
        else if (m_is_stuck && m_is_known_stuck && !is_metal) {
                return 'X';
        }
        else {
                return '+';
        }
}

gfx::TileId Door::tile() const
{
        if (m_is_hidden) {
                ASSERT(m_type != DoorType::gate);
                ASSERT(m_mimic_terrain);

                return m_mimic_terrain->tile();
        }

        // Not secret

        switch (m_type) {
        case DoorType::wood: {
                if (m_is_open) {
                        return gfx::TileId::door_open;
                }
                else if (m_ward_state != WardState::not_warded) {
                        return gfx::TileId::door_warded;
                }
                else if (m_is_stuck && m_is_known_stuck) {
                        return gfx::TileId::door_stuck;
                }
                else {
                        return gfx::TileId::door_closed;
                }
        } break;

        case DoorType::gate: {
                if (m_is_open) {
                        return gfx::TileId::gate_open;
                }
                else if (m_is_stuck && m_is_known_stuck) {
                        return gfx::TileId::gate_stuck;
                }
                else {
                        return gfx::TileId::gate_closed;
                }
        } break;

        case DoorType::metal: {
                if (m_is_open) {
                        return gfx::TileId::door_open;
                }
                else {
                        return gfx::TileId::door_closed;
                }
        } break;
        }

        ASSERT(false);

        return gfx::TileId::door_closed;
}

Material Door::material() const
{
        switch (m_type) {
        case DoorType::wood:
                return Material::wood;
                break;

        case DoorType::metal:
        case DoorType::gate:
                return Material::metal;
                break;
        }

        ASSERT(false);

        return Material::wood;
}

void Door::bump(actor::Actor& actor_bumping)
{
        if (!actor::is_player(&actor_bumping)) {
                return;
        }

        const bool is_seen = map::g_seen.at(m_pos);

        if (m_is_hidden) {
                ASSERT(m_type != DoorType::gate);

                // Print messages as if this was a wall.

                if (is_seen) {
                        TRACE
                                << "Player bumped into secret door, "
                                << "with vision in position"
                                << std::endl;

                        msg_log::add(
                                terrain::data(terrain::Id::wall)
                                        .msg_on_player_blocked);
                }
                else {
                        // Not seen by player
                        TRACE
                                << "Player bumped into secret door, "
                                << "without vision in position"
                                << std::endl;

                        msg_log::add(
                                terrain::data(terrain::Id::wall)
                                        .msg_on_player_blocked_blind);
                }

                return;
        }

        if (!is_seen && !m_is_open) {
                if (m_ward_state == WardState::warded) {
                        // Print a message as a rationale as to how a blind
                        // player can know that the door is warded.
                        msg_log::add("Something gives me a foreboding feeling.");
                }

                const std::string name_a = text_format::first_to_lower(name(Article::a));

                msg_log::add("There is " + name_a + " here.");

                const bool is_known_stuck_before = m_is_known_stuck;

                reveal_stuck_status(PrintRevealMsg::yes);

                map::memorize_terrain_at(m_pos);
                map::update_vision();

                if (m_is_stuck) {
                        if (!is_known_stuck_before) {
                                msg_log::more_prompt();
                        }

                        return;
                }
        }

        if (m_is_stuck && m_is_known_stuck) {
                // TODO: Copy pasted from bash.cpp, refactor.
                auto kick_wpn =
                        std::unique_ptr<item::Item>(
                                item::make(item::Id::player_kick));

                const item::Item* wpn = map::g_player->m_inv.item_in_slot(SlotId::wpn);

                if (!wpn) {
                        wpn = &map::g_player->unarmed_wpn();
                }

                if (bash::is_allowed_use_wpn_on_terrain(*wpn, *this)) {
                        bash::attack_terrain(m_pos, *wpn);
                }
                else {
                        bash::attack_terrain(m_pos, *kick_wpn);
                }

                return;
        }

        if (!m_is_open) {
                map::memorize_terrain_at(m_pos);
                map::update_vision();

                bool do_query = false;
                std::string query_msg;

                if (!is_seen) {
                        // NOTE: If the door is unseen and warded, it would be
                        // redundant to refer to it as a warded door here
                        // (already done previously).
                        do_query = true;
                        query_msg = "Attempt to open it?";
                }
                else if (m_ward_state == WardState::warded) {
                        do_query = true;
                        std::string door_name = text_format::first_to_lower(name(Article::a));
                        query_msg = "Open " + door_name + "?";
                }

                if (do_query) {
                        query_msg += " " + common_text::g_yes_or_no_hint;

                        msg_log::add(
                                query_msg,
                                colors::light_white(),
                                MsgInterruptPlayer::no,
                                MorePromptOnMsg::no,
                                CopyToMsgHistory::no);

                        const BinaryAnswer answer = query::yes_or_no();

                        msg_log::clear();

                        if (answer == BinaryAnswer::no) {
                                return;
                        }
                }

                const P pos = m_pos;

                // NOTE: This might destroy the door:
                actor_try_open(actor_bumping);

                map::memorize_terrain_at(pos);
                map::update_vision();
        }
}

void Door::reveal(const PrintRevealMsg print_reveal_msg)
{
        const bool is_hidden_before = m_is_hidden;

        m_is_hidden = false;

        const bool allow_print =
                ((print_reveal_msg == PrintRevealMsg::if_seen) &&
                 map::g_seen.at(m_pos)) ||
                (print_reveal_msg == PrintRevealMsg::yes);

        if (is_hidden_before && allow_print) {
                msg_log::add("A secret is revealed.");
        }

        // If the player is adjacent, also reveal stuck status to avoid an
        // inconsistent state (the player standing next to a revealed door that
        // they don't know is stuck).
        if (m_pos.is_adjacent(map::g_player->m_pos)) {
                reveal_stuck_status(print_reveal_msg);
        }
}

void Door::on_revealed_from_searching()
{
        game::incr_player_xp(g_xp_on_reveal_door);
}

void Door::reveal_stuck_status(const PrintRevealMsg print_reveal_msg)
{
        if (m_is_hidden) {
                ASSERT(false);
                return;
        }

        if (!m_is_stuck) {
                return;
        }

        const bool is_known_before = m_is_known_stuck;

        m_is_known_stuck = true;

        if (!is_known_before) {
                const bool allow_print =
                        ((print_reveal_msg == PrintRevealMsg::if_seen) &&
                         map::g_seen.at(m_pos)) ||
                        (print_reveal_msg == PrintRevealMsg::yes);

                if (allow_print) {
                        const std::string door_name = base_name_short();

                        msg_log::add("The " + door_name + " seems to be stuck.");
                }
        }
}

void Door::set_secret()
{
        ASSERT(m_type != DoorType::gate);
        ASSERT(m_ward_state == WardState::not_warded);

        m_is_open = false;
        m_is_hidden = true;
}

void Door::set_stuck()
{
        ASSERT(m_type != DoorType::metal);
        ASSERT(m_ward_state == WardState::not_warded);

        m_is_open = false;
        m_is_stuck = true;
}

bool Door::actor_try_jam(actor::Actor& actor_trying)
{
        if (m_is_hidden || m_is_open || (type() == DoorType::metal)) {
                return false;
        }

        // Door is in correct state for jamming (known, closed).

        const bool is_player = actor::is_player(&actor_trying);
        const bool tryer_is_blind = !actor_trying.m_properties.allow_see();

        ++m_jam_level;
        m_is_stuck = true;

        if (is_player) {
                m_is_known_stuck = true;

                std::string a = tryer_is_blind ? "a " : "the ";

                msg_log::add("I jam " + a + base_name_short() + " with a spike.");
        }

        game_time::tick();
        return true;
}

void Door::actor_try_close(actor::Actor& actor_trying)
{
        const bool is_player = actor::is_player(&actor_trying);
        const bool is_door_seen = map::g_seen.at(m_pos);

        // Already closed?
        if (!m_is_open) {
                if (is_player) {
                        if (is_door_seen) {
                                msg_log::add("I see nothing there to close.");
                        }
                        else {
                                msg_log::add("I find nothing there to close.");
                        }
                }

                return;
        }

        // Currently being opened by another actor?
        if (m_actor_currently_opening &&
            (m_actor_currently_opening != &actor_trying)) {
                TRACE
                        << "Door marked as currently being opened, checking if "
                           "opening actor still exists and is alive"
                        << std::endl;

                bool is_opening_actor_alive = false;

                for (const actor::Actor* const actor : game_time::g_actors) {
                        if ((actor == m_actor_currently_opening) &&
                            actor::is_alive(*actor)) {
                                is_opening_actor_alive = true;
                        }
                }

                if (is_opening_actor_alive) {
                        TRACE
                                << "Opening actor exists and is alive"
                                << std::endl;

                        if (is_player) {
                                msg_log::add(
                                        "The door is currently being opened, "
                                        "and cannot be closed.");
                        }

                        return;
                }
                else {
                        TRACE
                                << "Opening actor no longer exists, or is dead"
                                << std::endl;

                        m_actor_currently_opening = nullptr;
                }
        }

        // Blocked?
        bool is_blocked_by_actor = false;

        for (actor::Actor* actor : game_time::g_actors) {
                if ((actor->m_state != ActorState::destroyed) &&
                    (actor->m_pos == m_pos)) {
                        is_blocked_by_actor = true;
                        break;
                }
        }

        if (is_blocked_by_actor || map::g_items.at(m_pos)) {
                if (is_player) {
                        if (is_door_seen) {
                                // Can see
                                msg_log::add(
                                        "The " +
                                        base_name_short() +
                                        " is blocked.");
                        }
                        else {
                                msg_log::add(
                                        "Something is blocking the " +
                                        base_name_short() +
                                        ".");
                        }
                }

                return;
        }

        // Door can be closed.
        const audio::SfxId sfx_door_close = get_close_sfx(m_type);

        if (!is_door_seen) {
                if (rnd::coin_toss()) {
                        m_is_open = false;

                        map::memorize_terrain_at(m_pos);
                        map::update_vision();

                        ASSERT(is_player);

                        if (is_player) {
                                communicate_player_close_blind(
                                        m_pos,
                                        sfx_door_close,
                                        base_name_short());
                        }

                        game_time::tick();
                }
                else {
                        // Failed to close
                        ASSERT(is_player);

                        if (is_player) {
                                communicate_player_fail_close_blind(
                                        m_pos,
                                        base_name_short());
                        }

                        game_time::tick();
                }

                return;
        }

        // Door can be closed, and actor can see

        m_is_open = false;

        map::memorize_terrain_at(m_pos);
        map::update_vision();

        ASSERT(is_player);

        if (is_player) {
                communicate_player_close(
                        m_pos,
                        sfx_door_close,
                        base_name_short());
        }

        game_time::tick();

}  // actor_try_close

void Door::actor_try_open(actor::Actor& actor_trying)
{
        TRACE_FUNC_BEGIN;

        const bool is_player = actor::is_player(&actor_trying);

        const audio::SfxId sfx_door_open = get_open_sfx(m_type);

        if (m_is_stuck) {
                TRACE << "Is stuck" << std::endl;

                if (is_player) {
                        msg_log::add("The " + base_name_short() + " seems to be stuck.");
                }
        }
        else {
                // Not stuck
                TRACE << "Is not stuck" << std::endl;

                const bool tryer_can_see = actor_trying.m_properties.allow_see();

                if (tryer_can_see) {
                        TRACE << "Tryer can see, opening" << std::endl;
                        m_is_open = true;

                        if (is_player) {
                                communicate_player_open(
                                        m_pos,
                                        sfx_door_open,
                                        base_name_short());
                        }
                        else {
                                communicate_mon_open(
                                        m_pos,
                                        sfx_door_open,
                                        base_name_short(),
                                        actor_trying);
                        }
                }
                else {
                        // Tryer is blind
                        if (rnd::coin_toss()) {
                                TRACE
                                        << "Tryer is blind, but open succeeded anyway"
                                        << std::endl;

                                m_is_open = true;

                                if (is_player) {
                                        communicate_player_open_blind(
                                                m_pos,
                                                sfx_door_open,
                                                base_name_short());
                                }
                                else {
                                        communicate_mon_open_blind(
                                                m_pos,
                                                sfx_door_open,
                                                base_name_short(),
                                                actor_trying);
                                }
                        }
                        else {
                                // Failed to open while blind
                                TRACE << "Tryer is blind, and open failed" << std::endl;

                                if (is_player) {
                                        communicate_player_fail_open_blind(
                                                m_pos,
                                                base_name_short());
                                }
                                else {
                                        communicate_mon_fail_open_blind(
                                                m_pos,
                                                base_name_short(),
                                                actor_trying);
                                }

                                game_time::tick();
                        }
                }
        }

        if (m_is_open) {
                TRACE << "Open was successful" << std::endl;

                if (m_is_hidden) {
                        TRACE << "Was secret, now revealing" << std::endl;

                        reveal(terrain::PrintRevealMsg::if_seen);
                }

                m_actor_currently_opening = &actor_trying;

                actor_trying.m_opening_door_pos = m_pos;

                const bool is_actor_seen = actor::can_player_see_actor(actor_trying);
                const bool is_door_seen = map::g_seen.at(m_pos);

                const P pos = m_pos;

                // NOTE: This might destroy the door:
                try_trigger_ward_trap();

                game_time::tick();

                if (is_actor_seen || is_door_seen) {
                        map::memorize_terrain_at(pos);
                }

                map::update_vision();
        }

}  // actor_try_open

DidOpen Door::open(actor::Actor* const actor_opening)
{
        m_is_open = true;
        m_is_hidden = false;
        m_is_stuck = false;
        m_is_known_stuck = false;

        if (map::g_seen.at(m_pos)) {
                const std::string name = base_name();

                msg_log::add("The " + name + " opens.");
        }

        if (actor_opening) {
                m_actor_currently_opening = actor_opening;
                actor_opening->m_opening_door_pos = m_pos;
        }

        try_trigger_ward_trap();

        return DidOpen::yes;
}

DidClose Door::close(actor::Actor* const actor_closing)
{
        (void)actor_closing;

        m_is_open = false;

        if (map::g_seen.at(m_pos)) {
                const std::string name = base_name();

                msg_log::add("The " + name + " closes.");
        }

        return DidClose::yes;
}

void Door::jam(actor::Actor* const actor_jamming)
{
        if (m_is_open || (m_type == DoorType::metal)) {
                ASSERT(false);
                return;
        }

        ++m_jam_level;
        m_is_stuck = true;

        if (actor_jamming && actor::is_player(actor_jamming)) {
                m_is_known_stuck = true;
        }
}

void Door::try_trigger_ward_trap()
{
        if (m_ward_state != WardState::warded) {
                return;
        }

        // Disable any linked crystals.
        for (terrain::Terrain* const terrain : map::g_terrain) {
                if (terrain->id() != Id::crystal_key) {
                        continue;
                }

                auto* const crystal = static_cast<CrystalKey*>(terrain);

                if (crystal->is_linked_to(this)) {
                        // NOTE: This will make the lever remove the ward status
                        // on this door. The lever will also disable any
                        // "siblings", so no need call any other crystals.
                        crystal->deactivate();

                        break;
                }
        }

        const P pos = m_pos;

        // Destroying the door, otherwise it causes a dumb situation where the
        // player can close the door to avoid or split the monsters that spawn.
        map::update_terrain(terrain::make(Id::rubble_low, pos));

        // NOTE: This object is now destroyed.

        // NOTE: This will play regardless of whether the door is seen or not,
        // or whether it is actually the player that caused the ward to
        // trigger. This should be acceptable howeever since it will always be
        // coupled with the "something approaches" message below.
        audio::play(audio::SfxId::thunder);

        const std::vector<std::string> summon_bucket = get_warded_door_summon_bucket();

        if (!summon_bucket.empty()) {
                msg_log::add("Something approaches...");

                const std::string id_to_spawn = rnd::element(summon_bucket);

                const size_t nr_mon = rnd::range(1, 3);

                auto* const event =
                        static_cast<terrain::EventSpawnMonstersDelayed*>(
                                make(terrain::Id::event_spawn_monsters_delayed, pos));

                event->set_mon_id(id_to_spawn);

                event->set_nr_mon((int)nr_mon);

                game_time::add_mob(event);
        }

        if (rnd::one_in(3)) {
                map::g_player->m_properties.apply(prop::make(prop::Id::cursed));
        }

        map::g_player->incr_shock(12.0, ShockSrc::misc);
}

void Door::remove_ward()
{
        if (m_ward_state == WardState::warded) {
                m_ward_state = WardState::unwarded;
        }
}

}  // namespace terrain

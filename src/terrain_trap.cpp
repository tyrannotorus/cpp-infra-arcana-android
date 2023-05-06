// =============================================================================
// Copyright 2011-2023 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "terrain_trap.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <memory>
#include <ostream>
#include <vector>

#include "actor.hpp"
#include "actor_data.hpp"
#include "actor_factory.hpp"
#include "actor_see.hpp"
#include "array2.hpp"
#include "attack.hpp"
#include "audio_data.hpp"
#include "common_text.hpp"
#include "debug.hpp"
#include "direction.hpp"
#include "explosion.hpp"
#include "game.hpp"
#include "game_time.hpp"
#include "io.hpp"
#include "item_data.hpp"
#include "item_factory.hpp"
#include "item_weapon.hpp"
#include "map.hpp"
#include "msg_log.hpp"
#include "player_bon.hpp"
#include "player_spells.hpp"
#include "property.hpp"
#include "property_data.hpp"
#include "property_factory.hpp"
#include "property_handler.hpp"
#include "query.hpp"
#include "random.hpp"
#include "sound.hpp"
#include "spells.hpp"
#include "state.hpp"
#include "teleport.hpp"
#include "terrain_data.hpp"
#include "terrain_factory.hpp"
#include "text_format.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static bool is_player_seeing_trap_trigger(
        const actor::Actor& actor,
        const P& pos)
{
        if (actor::is_player(&actor)) {
                return true;
        }
        else {
                const bool can_player_see_trap = map::g_seen.at(pos);
                const bool can_player_see_actor = actor::can_player_see_actor(actor);

                return can_player_see_trap || can_player_see_actor;
        }
}

static void communicate_magic_trap_trigger(
        const actor::Actor& actor,
        const P& pos,
        const bool is_hidden)
{
        map::update_vision();

        const bool can_player_see_trap = map::g_seen.at(pos);

        if (actor::is_player(&actor)) {
                if (can_player_see_trap) {
                        std::string msg = "A beam of light shoots out from";

                        if (!is_hidden) {
                                msg += " a curious shape on";
                        }

                        msg += " the floor!";

                        msg_log::add(msg);
                }
                else {
                        msg_log::add("I feel a peculiar energy around me!");
                }
        }
        else {
                // Is a monster

                const bool can_player_see_actor = actor::can_player_see_actor(actor);

                if (can_player_see_actor || can_player_see_trap) {
                        const std::string actor_name =
                                can_player_see_actor
                                ? actor.name_the()
                                : "it";

                        msg_log::add("A beam of light shoots out under " + actor_name + ".");
                }
        }

        Snd snd(
                "I hear an otherworldly blaze.",
                audio::SfxId::magic_trap_trigger,
                IgnoreMsgIfOriginSeen::yes,
                pos,
                nullptr,
                SndVol::low,
                AlertsMon::no);

        snd.run();

        if (can_player_see_trap) {
                const int flash_speed_pct = 15;

                io::flash_at(pos, colors::yellow(), flash_speed_pct);
        }

        if (actor::is_player(&actor)) {
                msg_log::more_prompt();
        }
}

static void communicate_mechanical_trap_trigger(const actor::Actor& actor, const P& pos)
{
        std::string msg = "I hear a click.";

        auto alerts = AlertsMon::no;

        if (actor::is_player(&actor)) {
                alerts = AlertsMon::yes;

                // Use a amore foreboding message when the player is triggering.
                msg += "..";
        }

        Snd snd(
                msg,
                audio::SfxId::mechanical_trap_trigger,
                IgnoreMsgIfOriginSeen::no,
                pos,
                nullptr,
                SndVol::low,
                alerts);

        snd.run();

        if (actor::is_player(&actor)) {
                const bool is_deaf = map::g_player->m_properties.has(prop::Id::deaf);

                if (is_deaf) {
                        msg_log::add("I feel the ground shifting slightly under my foot.");
                }

                msg_log::more_prompt();
        }
}

static terrain::TrapImpl* make_trap_impl_from_id(
        const terrain::TrapId trap_id,
        const P& pos,
        terrain::Trap* parent_trap)
{
        switch (trap_id) {
        case terrain::TrapId::dart:
                return new terrain::TrapDart(pos, parent_trap);
                break;

        case terrain::TrapId::spear:
                return new terrain::TrapSpear(pos, parent_trap);
                break;

        case terrain::TrapId::blinding:
                return new terrain::TrapBlindingFlash(pos, parent_trap);
                break;

        case terrain::TrapId::deafening:
                return new terrain::TrapDeafening(pos, parent_trap);
                break;

        case terrain::TrapId::teleport:
                return new terrain::TrapTeleport(pos, parent_trap);
                break;

        case terrain::TrapId::summon:
                return new terrain::TrapSummonMon(pos, parent_trap);
                break;

        case terrain::TrapId::hp_sap:
                return new terrain::TrapHpSap(pos, parent_trap);
                break;

        case terrain::TrapId::spi_sap:
                return new terrain::TrapSpiSap(pos, parent_trap);
                break;

        case terrain::TrapId::smoke:
                return new terrain::TrapSmoke(pos, parent_trap);
                break;

        case terrain::TrapId::fire:
                return new terrain::TrapFire(pos, parent_trap);
                break;

        case terrain::TrapId::alarm:
                return new terrain::TrapAlarm(pos, parent_trap);
                break;

        case terrain::TrapId::web:
                return new terrain::TrapWeb(pos, parent_trap);
                break;

        case terrain::TrapId::slow:
                return new terrain::TrapSlow(pos, parent_trap);
                break;

        case terrain::TrapId::haste:
                return new terrain::TrapHaste(pos, parent_trap);
                break;

        case terrain::TrapId::alter_env:
                return new terrain::TrapAlterEnv(pos, parent_trap);
                break;

        case terrain::TrapId::curse:
                return new terrain::TrapCurse(pos, parent_trap);
                break;

        case terrain::TrapId::bless:
                return new terrain::TrapBless(pos, parent_trap);
                break;

        case terrain::TrapId::unlearn_spell:
                return new terrain::TrapUnlearnSpell(pos, parent_trap);
                break;

        case terrain::TrapId::END_MECHANICAL:
        case terrain::TrapId::END:
        case terrain::TrapId::any:
                break;
        }

        return nullptr;
}

static terrain::TrapImpl* try_make_impl(
        const terrain::TrapId trap_id,
        const P& pos,
        terrain::Trap* parent_trap)
{
        terrain::TrapImpl* impl =
                make_trap_impl_from_id(trap_id, pos, parent_trap);

        terrain::TrapPlacementValid valid = impl->on_place();

        if (valid == terrain::TrapPlacementValid::yes) {
                return impl;
        }
        else {
                // Placement not valid.
                delete impl;

                return nullptr;
        }
}

static terrain::TrapId get_random_id_to_spawn()
{
        Range id_range;

        // Spawn magic traps most of the time.
        if (rnd::one_in(3)) {
                // "Mechanical" trap.
                id_range.min = 0;
                id_range.max = (int)terrain::TrapId::END_MECHANICAL - 1;
        }
        else {
                // Magic trap.
                id_range.min = (int)terrain::TrapId::END_MECHANICAL + 1;
                id_range.max = (int)terrain::TrapId::END - 1;
        }

        return (terrain::TrapId)id_range.roll();
}

// -----------------------------------------------------------------------------
// terrain
// -----------------------------------------------------------------------------
namespace terrain
{
Trap::~Trap()
{
        delete m_trap_impl;
        delete m_mimic_terrain;
}

bool Trap::try_init_type(const TrapId id)
{
        ASSERT(id != TrapId::END_MECHANICAL);
        ASSERT(id != TrapId::END);

        Terrain* const terrain_here = map::g_terrain.at(m_pos);

        if (!terrain_here->can_have_trap()) {
                TRACE
                        << "Cannot place trap on terrain id: "
                        << (int)terrain_here->id()
                        << std::endl
                        << "Trap id: " << (int)id
                        << std::endl;

                ASSERT(false);

                return false;
        }

        if (id == TrapId::any) {
                // Attempt to set a trap implementation until succeeding.
                while (true) {
                        const TrapId random_id = get_random_id_to_spawn();

                        TrapImpl* const impl = try_make_impl(random_id, m_pos, this);

                        if (impl) {
                                // Trap placement is good!
                                m_trap_impl = impl;

                                break;
                        }
                }
        }
        else {
                // Make a specific trap type.

                // NOTE: This may fail, in which case we have no trap
                // implementation. The trap creator is responsible for handling
                // this situation.
                m_trap_impl = try_make_impl(id, m_pos, this);
        }

        if (m_trap_impl) {
                // "Mechanical" traps are always created as hidden, magical
                // traps always created as visible.
                m_is_hidden = !is_magical();

                return true;
        }
        else {
                return false;
        }
}

void Trap::hit(
        DmgType dmg_type,
        actor::Actor* actor,
        const P& from_pos,
        int dmg)
{
        (void)dmg_type;
        (void)actor;
        (void)from_pos;
        (void)dmg;
}

TrapId Trap::type() const
{
        ASSERT(m_trap_impl);

        return m_trap_impl->type();
}

bool Trap::is_magical() const
{
        ASSERT(m_trap_impl);

        return m_trap_impl->is_magical();
}

void Trap::on_new_turn_hook()
{
        if (m_nr_turns_until_trigger > 0) {
                --m_nr_turns_until_trigger;

                TRACE
                        << "Number of turns until trigger: "
                        << m_nr_turns_until_trigger << std::endl;

                if (m_nr_turns_until_trigger == 0) {
                        // NOTE: This will reset number of turns until triggered
                        trigger_trap(nullptr);
                }
        }
}

void Trap::trigger_start(const actor::Actor* actor)
{
        TRACE_FUNC_BEGIN;

        ASSERT(m_trap_impl);

        TRACE
                << "Start trigger for trap of type "
                << "'" << m_trap_impl->name(Article::a) << "', "
                << "with trap implementation id "
                << "'" << (int)m_trap_impl->type() << "'"
                << std::endl;

        if (actor::is_player(actor)) {
                // Reveal trap if triggered by player stepping on it
                if (is_hidden()) {
                        reveal(PrintRevealMsg::no);
                }

                map::g_player->update_fov();

                states::draw();
        }

        if (is_magical()) {
                communicate_magic_trap_trigger(*actor, m_pos, is_hidden());
        }
        else if (type() != TrapId::web) {
                communicate_mechanical_trap_trigger(*actor, m_pos);
        }

        // Get a randomized value for number of remaining turns
        const Range turns_range = m_trap_impl->nr_turns_range_to_trigger();

        const int rnd_nr_turns = turns_range.roll();

        // Set number of remaining turns to the randomized value if not set
        // already, or if the new value will make it trigger sooner
        if ((m_nr_turns_until_trigger == -1) ||
            (rnd_nr_turns < m_nr_turns_until_trigger)) {
                m_nr_turns_until_trigger = rnd_nr_turns;
        }

        ASSERT(m_nr_turns_until_trigger >= 0);

        // If number of remaining turns is zero, trigger immediately
        if (m_nr_turns_until_trigger == 0) {
                // NOTE: This will reset number of turns until triggered
                trigger_trap(nullptr);
        }

        TRACE_FUNC_END;
}

AllowAction Trap::pre_bump(actor::Actor& actor_bumping)
{
        if (!actor::is_player(&actor_bumping) ||
            actor_bumping.m_properties.has(prop::Id::confused)) {
                return AllowAction::yes;
        }

        const prop::PropHandler& props = actor_bumping.m_properties;

        if (map::g_seen.at(m_pos) &&
            !m_is_hidden &&
            !props.has(prop::Id::ethereal) &&
            !props.has(prop::Id::flying) &&
            !props.has(prop::Id::tiny_flying)) {
                // The trap is known, and would be triggered by the player.

                const std::string name_the = name(Article::the);

                const std::string msg =
                        "Step into " +
                        name_the +
                        "? " +
                        common_text::g_yes_or_no_hint;

                msg_log::add(
                        msg,
                        colors::light_white(),
                        MsgInterruptPlayer::no,
                        MorePromptOnMsg::no,
                        CopyToMsgHistory::no);

                const BinaryAnswer query_result = query::yes_or_no();

                msg_log::clear();

                return (
                        (query_result == BinaryAnswer::no)
                                ? AllowAction::no
                                : AllowAction::yes);
        }
        else {
                // The trap is hidden, or would not be triggered by the player -
                // delegate the question to the mimicked terrain.

                const AllowAction result =
                        m_mimic_terrain->pre_bump(actor_bumping);

                return result;
        }
}

void Trap::bump(actor::Actor& actor_bumping)
{
        TRACE_FUNC_BEGIN_VERBOSE;

        const actor::ActorData& d = *actor_bumping.m_data;

        const prop::PropHandler& props = actor_bumping.m_properties;

        if (props.has(prop::Id::ethereal) ||
            props.has(prop::Id::flying) ||
            props.has(prop::Id::tiny_flying) ||
            (d.actor_size < actor::Size::humanoid) ||
            d.is_spider) {
                TRACE_FUNC_END_VERBOSE;

                return;
        }

        if (!actor::is_player(&actor_bumping)) {
                // TODO: This seems to prevent the trap from triggering when the
                // player kicks a monster into the trap in some cases? Perhaps
                // when the monster has just stepped into sight of the player?
                // The problem can happen both when the monster is aware or
                // unaware?

                // Put some extra restrictions on monsters triggering traps.
                if (!actor_bumping.m_ai_state.is_target_seen ||
                    !actor_bumping.is_aware_of_player() ||
                    is_hidden()) {
                        TRACE_FUNC_END_VERBOSE;

                        return;
                }
        }

        trigger_start(&actor_bumping);

        TRACE_FUNC_END_VERBOSE;
}

bool Trap::disarm()
{
        if (m_nr_turns_until_trigger != -1) {
                msg_log::add("It cannot be disarmed now!");

                return false;
        }

        msg_log::add(m_trap_impl->disarm_msg());

        destroy();

        return true;
}

void Trap::destroy()
{
        ASSERT(m_mimic_terrain);

        // Magical traps and webs simply "dissapear" (place their mimic
        // terrain), and mechanical traps puts rubble.

        if (is_magical() || type() == TrapId::web) {
                // Magical trap or wbb.
                Terrain* const mimic_terrain = m_mimic_terrain;

                m_mimic_terrain = nullptr;

                // NOTE: This call destroys this object!
                map::update_terrain(mimic_terrain);
        }
        else {
                // "Mechanical" trap
                map::update_terrain(
                        terrain::make(terrain::Id::rubble_low, m_pos));
        }
}

DidTriggerTrap Trap::trigger_trap(actor::Actor* const actor)
{
        TRACE_FUNC_BEGIN;

        (void)actor;

        TRACE_VERBOSE
                << "Name of trap triggering: "
                << m_trap_impl->name(Article::a)
                << std::endl;

        m_nr_turns_until_trigger = -1;

        TRACE_VERBOSE << "Calling trap implementation trigger" << std::endl;

        m_trap_impl->trigger();

        // Traps are always destroyed after being triggered.

        // NOTE: This deletes this terrain object!
        destroy();

        TRACE_FUNC_END;

        return DidTriggerTrap::yes;
}

void Trap::reveal(const PrintRevealMsg print_reveal_msg)
{
        TRACE_FUNC_BEGIN;

        const bool is_hidden_before = m_is_hidden;

        m_is_hidden = false;

        clear_gore();

        const bool allow_print =
                ((print_reveal_msg == PrintRevealMsg::if_seen) &&
                 map::g_seen.at(m_pos)) ||
                (print_reveal_msg == PrintRevealMsg::yes);

        if (is_hidden_before && allow_print) {
                states::draw();

                std::string msg;

                const std::string trap_name_a = m_trap_impl->name(Article::a);

                if (m_pos == map::g_player->m_pos) {
                        msg = "There is " + trap_name_a + " here!";
                }
                else {
                        // Trap is not at player position
                        msg = "I spot " + trap_name_a + ".";
                }

                msg_log::add(msg);
        }

        TRACE_FUNC_END;
}

void Trap::on_revealed_from_searching()
{
        game::incr_player_xp(1);
}

std::string Trap::name(const Article article) const
{
        if (m_is_hidden) {
                return m_mimic_terrain->name(article);
        }
        else {
                return m_trap_impl->name(article);
        }
}

Color Trap::color_default() const
{
        if (m_is_hidden) {
                return m_mimic_terrain->color();
        }
        else {
                return m_trap_impl->color();
        }
}

char Trap::character() const
{
        if (m_is_hidden) {
                return m_mimic_terrain->character();
        }
        else {
                return m_trap_impl->character();
        }
}

gfx::TileId Trap::tile() const
{
        if (m_is_hidden) {
                return m_mimic_terrain->tile();
        }
        else {
                return m_trap_impl->tile();
        }
}

Material Trap::material() const
{
        if (m_is_hidden) {
                return m_mimic_terrain->material();
        }
        else {
                return m_data->material_type;
        }
}

// -----------------------------------------------------------------------------
// Trap implementations
// -----------------------------------------------------------------------------
TrapDart::TrapDart(P pos, Trap* const base_trap) :
        MechTrapImpl(pos, TrapId::dart, base_trap),
        m_is_poisoned((map::g_dlvl >= g_dlvl_harder_traps) && rnd::one_in(3)),

        m_is_dart_origin_destroyed(false)
{}

TrapPlacementValid TrapDart::on_place()
{
        std::vector<P> offsets = dir_utils::g_cardinal_list;

        rnd::shuffle(offsets);

        const int nr_steps_min = 2;
        const int nr_steps_max = g_fov_radi_int;

        TrapPlacementValid trap_plament_valid = TrapPlacementValid::no;

        for (const P& d : offsets) {
                P p = m_pos;

                for (int i = 0; i <= nr_steps_max; ++i) {
                        p += d;

                        const Terrain* const terrain = map::g_terrain.at(p);

                        const bool is_wall = terrain->id() == terrain::Id::wall;

                        const bool is_passable =
                                terrain->is_projectile_passable();

                        if (!is_passable &&
                            ((i < nr_steps_min) || !is_wall)) {
                                // We are blocked too early - OR - blocked by a
                                // terrain other than a wall. Give up on this
                                // direction.
                                break;
                        }

                        if ((i >= nr_steps_min) && is_wall) {
                                // This is a good origin!
                                m_dart_origin = p;
                                trap_plament_valid = TrapPlacementValid::yes;
                                break;
                        }
                }

                if (trap_plament_valid == TrapPlacementValid::yes) {
                        // A valid origin has been found

                        if (rnd::fraction(2, 3)) {
                                terrain::make_gore(m_pos);
                                terrain::make_blood(m_pos);
                        }

                        break;
                }
        }

        return trap_plament_valid;
}

void TrapDart::trigger()
{
        TRACE_FUNC_BEGIN;

        ASSERT((m_dart_origin.x == m_pos.x) || (m_dart_origin.y == m_pos.y));
        ASSERT(m_dart_origin != m_pos);

        if (map::g_terrain.at(m_dart_origin)->id() != terrain::Id::wall) {
                // NOTE: This is permanently set from now on
                m_is_dart_origin_destroyed = true;
        }

        if (m_is_dart_origin_destroyed) {
                return;
        }

        // Aim target is the wall on the other side of the map
        P aim_pos = m_dart_origin;

        if (m_dart_origin.x == m_pos.x) {
                aim_pos.y =
                        (m_dart_origin.y > m_pos.y)
                        ? 0
                        : (map::h() - 1);
        }
        else {
                // Dart origin is on same vertial line as the trap
                aim_pos.x =
                        (m_dart_origin.x > m_pos.x)
                        ? 0
                        : (map::w() - 1);
        }

        if (map::g_seen.at(m_dart_origin)) {
                const std::string name =
                        map::g_terrain.at(m_dart_origin)
                                ->name(Article::the);

                msg_log::add("A dart is launched from " + name + "!");
        }

        // Make a temporary dart weapon
        item::Wpn* wpn = nullptr;

        if (m_is_poisoned) {
                wpn =
                        static_cast<item::Wpn*>(
                                item::make(item::Id::trap_dart_poison));
        }
        else {
                // Not poisoned
                wpn =
                        static_cast<item::Wpn*>(
                                item::make(item::Id::trap_dart));
        }

        // Fire!
        attack::ranged(
                nullptr,
                m_dart_origin,
                aim_pos,
                *wpn);

        delete wpn;

        TRACE_FUNC_END;
}

TrapSpear::TrapSpear(P pos, Trap* const base_trap) :
        MechTrapImpl(pos, TrapId::spear, base_trap),
        m_is_poisoned((map::g_dlvl >= g_dlvl_harder_traps) && rnd::one_in(4)),

        m_is_spear_origin_destroyed(false)
{}

TrapPlacementValid TrapSpear::on_place()
{
        std::vector<P> offsets = dir_utils::g_cardinal_list;

        rnd::shuffle(offsets);

        TrapPlacementValid trap_plament_valid = TrapPlacementValid::no;

        for (const P& d : offsets) {
                const P p = m_pos + d;

                const Terrain* const terrain = map::g_terrain.at(p);

                const bool is_wall = terrain->id() == terrain::Id::wall;

                const bool is_passable = terrain->is_projectile_passable();

                if (is_wall && !is_passable) {
                        // This is a good origin!
                        m_spear_origin = p;
                        trap_plament_valid = TrapPlacementValid::yes;

                        if (rnd::fraction(2, 3)) {
                                terrain::make_gore(m_pos);
                                terrain::make_blood(m_pos);
                        }

                        break;
                }
        }

        return trap_plament_valid;
}

void TrapSpear::trigger()
{
        TRACE_FUNC_BEGIN;

        ASSERT(m_spear_origin.x == m_pos.x || m_spear_origin.y == m_pos.y);
        ASSERT(m_spear_origin != m_pos);

        if (map::g_terrain.at(m_spear_origin)->id() != terrain::Id::wall) {
                // NOTE: This is permanently set from now on
                m_is_spear_origin_destroyed = true;
        }

        if (m_is_spear_origin_destroyed) {
                return;
        }

        if (map::g_seen.at(m_spear_origin)) {
                const std::string name =
                        map::g_terrain.at(m_spear_origin)
                                ->name(Article::the);

                msg_log::add("A spear shoots out from " + name + "!");
        }

        // Is anyone standing on the trap now?
        actor::Actor* const actor_on_trap = map::living_actor_at(m_pos);

        if (actor_on_trap) {
                // Make a temporary spear weapon
                item::Wpn* wpn = nullptr;

                if (m_is_poisoned) {
                        wpn = static_cast<item::Wpn*>(
                                item::make(item::Id::trap_spear_poison));
                }
                else {
                        // Not poisoned
                        wpn =
                                static_cast<item::Wpn*>(
                                        item::make(item::Id::trap_spear));
                }

                // Attack!
                attack::melee(nullptr, m_spear_origin, m_pos, *wpn);

                delete wpn;
        }

        TRACE_FUNC_BEGIN;
}

void TrapBlindingFlash::trigger()
{
        TRACE_FUNC_BEGIN;

        if (map::g_seen.at(m_pos)) {
                msg_log::add("There is an intense flash of light!");
        }

        explosion::run(
                m_pos,
                ExplType::apply_prop,
                EmitExplSnd::no,
                -1,
                ExplExclCenter::no,
                {prop::make(prop::Id::blind)},
                colors::yellow());

        TRACE_FUNC_END;
}

void TrapDeafening::trigger()
{
        TRACE_FUNC_BEGIN;

        if (map::g_seen.at(m_pos)) {
                msg_log::add(
                        "There is suddenly a crushing pressure in the air!");
        }

        explosion::run(
                m_pos,
                ExplType::apply_prop,
                EmitExplSnd::no,
                -1,
                ExplExclCenter::no,
                {prop::make(prop::Id::deaf)},
                colors::light_white());

        TRACE_FUNC_END;
}

void TrapTeleport::trigger()
{
        TRACE_FUNC_BEGIN;

        actor::Actor* const actor_here = map::living_actor_at(m_pos);

        ASSERT(actor_here);

        if (!actor_here) {
                // Should never happen.
                ASSERT(false);
                return;
        }

        teleport(*actor_here);

        TRACE_FUNC_END;
}

void TrapSummonMon::trigger()
{
        TRACE_FUNC_BEGIN;

        actor::Actor* const actor_here = map::living_actor_at(m_pos);

        ASSERT(actor_here);

        if (!actor_here) {
                // Should never happen.
                ASSERT(false);
                return;
        }

        TRACE << "Finding summon candidates" << std::endl;
        std::vector<std::string> summon_bucket;

        for (const auto& it : actor::g_data) {
                const actor::ActorData& data = it.second;

                if (data.can_be_summoned_by_mon &&
                    data.spawn_min_dlvl <= (map::g_dlvl + 2)) {
                        summon_bucket.push_back(data.id);
                }
        }

        if (summon_bucket.empty()) {
                TRACE << "No eligible candidates found" << std::endl;
        }
        else {
                // Eligible monsters found
                const std::string id_to_summon = rnd::element(summon_bucket);

                TRACE << "Actor id: " << id_to_summon << std::endl;

                const actor::MonSpawnResult summoned =
                        actor::spawn(m_pos, {id_to_summon}, map::rect())
                                .make_aware_of_player();

                std::for_each(
                        std::begin(summoned.monsters),
                        std::end(summoned.monsters),
                        [](actor::Actor* const mon) {
                                prop::Prop* prop_summoned = prop::make(prop::Id::summoned);

                                prop_summoned->set_indefinite();

                                mon->m_properties.apply(prop_summoned);

                                prop::Prop* prop_waiting = prop::make(prop::Id::waiting);

                                prop_waiting->set_duration(2);

                                mon->m_properties.apply(prop_waiting);

                                if (actor::can_player_see_actor(*mon)) {
                                        states::draw();

                                        const std::string name_a =
                                                text_format::first_to_upper(
                                                        mon->name_a());

                                        msg_log::add(name_a + " appears!");
                                }
                        });
        }

        TRACE_FUNC_END;
}

void TrapHpSap::trigger()
{
        TRACE_FUNC_BEGIN;

        actor::Actor* const actor_here = map::living_actor_at(m_pos);

        ASSERT(actor_here);

        if (!actor_here) {
                // Should never happen.
                ASSERT(false);
                return;
        }

        auto* const hp_sap = static_cast<prop::HpSap*>(prop::make(prop::Id::hp_sap));

        if (!actor::is_player(actor_here)) {
                // This is a monster triggering the trap - drain half of the
                // monsters hit points instead, so that this trap will actually
                // have a tangible effect.
                const int max_hp = actor::max_hp(*actor_here);

                hp_sap->set_nr_drained(max_hp / 2);
        }

        hp_sap->set_indefinite();

        actor_here->m_properties.apply(hp_sap);

        TRACE_FUNC_END;
}

void TrapSpiSap::trigger()
{
        TRACE_FUNC_BEGIN;

        actor::Actor* const actor_here = map::living_actor_at(m_pos);

        ASSERT(actor_here);

        if (!actor_here) {
                // Should never happen.
                ASSERT(false);
                return;
        }

        prop::Prop* const sp_sap = prop::make(prop::Id::spi_sap);

        sp_sap->set_indefinite();

        actor_here->m_properties.apply(sp_sap);

        TRACE_FUNC_END;
}

void TrapSmoke::trigger()
{
        TRACE_FUNC_BEGIN;

        if (map::g_seen.at(m_pos)) {
                msg_log::add(
                        "A burst of smoke is released from a vent in the "
                        "floor!");
        }

        Snd snd(
                "I hear a burst of gas.",
                audio::SfxId::gas,
                IgnoreMsgIfOriginSeen::yes,
                m_pos,
                nullptr,
                SndVol::low,
                AlertsMon::yes);

        snd.run();

        explosion::run_smoke_explosion_at(m_pos);

        TRACE_FUNC_END;
}

void TrapFire::trigger()
{
        TRACE_FUNC_BEGIN;

        if (map::g_seen.at(m_pos)) {
                msg_log::add("Flames burst out from a vent in the floor!");
        }

        Snd snd(
                "I hear a burst of flames.",
                audio::SfxId::END,
                IgnoreMsgIfOriginSeen::yes,
                m_pos,
                nullptr,
                SndVol::low,
                AlertsMon::yes);

        snd.run();

        explosion::run(
                m_pos,
                ExplType::apply_prop,
                EmitExplSnd::no,
                -1,
                ExplExclCenter::no,
                {prop::make(prop::Id::burning)});

        TRACE_FUNC_END;
}

void TrapAlarm::trigger()
{
        TRACE_FUNC_BEGIN;

        Snd snd(
                "An alarm sounds!",
                audio::SfxId::END,
                IgnoreMsgIfOriginSeen::no,
                m_pos,
                nullptr,
                SndVol::global,
                AlertsMon::yes);

        snd.run();

        TRACE_FUNC_END;
}

void TrapWeb::trigger()
{
        TRACE_FUNC_BEGIN;

        actor::Actor* const actor_here = map::living_actor_at(m_pos);

        ASSERT(actor_here);

        if (!actor_here) {
                return;
        }

        if (actor::is_player(actor_here)) {
                if (actor_here->m_properties.allow_see()) {
                        msg_log::add("I am entangled in a spider web!");
                }
                else {
                        // Cannot see
                        msg_log::add("I am entangled in a sticky mass of threads!");
                }
        }
        else {
                // Is a monster
                if (actor::can_player_see_actor(*actor_here)) {
                        const std::string actor_name =
                                text_format::first_to_upper(
                                        actor_here->name_the());

                        msg_log::add(actor_name + " is entangled in a huge spider web!");
                }
        }

        prop::Prop* const entangled = prop::make(prop::Id::entangled);

        entangled->set_indefinite();

        actor_here->m_properties.apply(
                entangled,
                prop::PropSrc::intr,
                false,
                Verbose::no);

        // Players getting stuck in spider webs alerts all spiders
        if (actor::is_player(actor_here)) {
                for (actor::Actor* const actor : game_time::g_actors) {
                        if (actor::is_player(actor) ||
                            !actor->m_data->is_spider) {
                                continue;
                        }

                        actor->become_aware_player(actor::AwareSource::other);
                }
        }

        TRACE_FUNC_END;
}

void TrapSlow::trigger()
{
        TRACE_FUNC_BEGIN;

        actor::Actor* const actor_here = map::living_actor_at(m_pos);

        ASSERT(actor_here);

        if (!actor_here) {
                // Should never happen.
                ASSERT(false);
                return;
        }

        actor_here->m_properties.apply(prop::make(prop::Id::slowed));

        TRACE_FUNC_END;
}

void TrapHaste::trigger()
{
        TRACE_FUNC_BEGIN;

        actor::Actor* const actor_here = map::living_actor_at(m_pos);

        ASSERT(actor_here);

        if (!actor_here) {
                // Should never happen.
                ASSERT(false);
                return;
        }

        actor_here->m_properties.apply(prop::make(prop::Id::hasted));

        TRACE_FUNC_END;
}

void TrapAlterEnv::trigger()
{
        TRACE_FUNC_BEGIN;

        actor::Actor* const actor_here = map::living_actor_at(m_pos);

        ASSERT(actor_here);

        if (!actor_here) {
                // Should never happen.
                ASSERT(false);
                return;
        }

        if (is_player_seeing_trap_trigger(*actor_here, m_pos)) {
                msg_log::add("The surroundings change!");
        }

        const int change_one_in_n = 2;

        prop::run_alter_env_effect(m_pos, change_one_in_n);

        TRACE_FUNC_END;
}

void TrapCurse::trigger()
{
        TRACE_FUNC_BEGIN;

        actor::Actor* const actor_here = map::living_actor_at(m_pos);

        ASSERT(actor_here);

        if (!actor_here) {
                // Should never happen.
                ASSERT(false);
                return;
        }

        actor_here->m_properties.apply(prop::make(prop::Id::cursed));

        TRACE_FUNC_END;
}

void TrapBless::trigger()
{
        TRACE_FUNC_BEGIN;

        actor::Actor* const actor_here = map::living_actor_at(m_pos);

        ASSERT(actor_here);

        if (!actor_here) {
                // Should never happen.
                ASSERT(false);
                return;
        }

        actor_here->m_properties.apply(prop::make(prop::Id::blessed));

        TRACE_FUNC_END;
}

void TrapUnlearnSpell::trigger()
{
        TRACE_FUNC_BEGIN;

        actor::Actor* const actor_here = map::living_actor_at(m_pos);

        if (!actor_here) {
                // Should never happen.
                ASSERT(false);
                return;
        }

        if (actor::is_player(actor_here)) {
                try_unlearn_for_player();
        }
        else {
                try_unlearn_for_monster(*actor_here);
        }

        TRACE_FUNC_END;
}

void TrapUnlearnSpell::try_unlearn_for_player() const
{
        std::vector<SpellId> id_bucket;

        // Do not unlearn spells for the Exorcist.
        if (!player_bon::is_bg(Bg::exorcist)) {
                id_bucket.reserve((size_t)SpellId::END);

                for (int i = 0; i < (int)SpellId::END; ++i) {
                        const auto id = (SpellId)i;

                        bool has_scroll = false;

                        for (const item::ItemData& d : item::g_data) {
                                if (d.spell_cast_from_scroll == id) {
                                        has_scroll = true;
                                        break;
                                }
                        }

                        if (!has_scroll) {
                                continue;
                        }

                        if (!player_spells::is_spell_learned(id)) {
                                continue;
                        }

                        if (player_spells::is_spell_forgotten(id)) {
                                continue;
                        }

                        id_bucket.push_back(id);
                }
        }

        if (id_bucket.empty()) {
                msg_log::add("There is no apparent effect.");

                return;
        }

        msg_log::add("I am surrounded by a misty haze.");

        const SpellId id = rnd::element(id_bucket);

        player_spells::forget_spell(id);
}

void TrapUnlearnSpell::try_unlearn_for_monster(actor::Actor& actor) const
{
        std::vector<actor::MonSpell>& spells = actor.m_mon_spells;

        const bool player_sees_actor = actor::can_player_see_actor(actor);

        if (spells.empty()) {
                if (player_sees_actor) {
                        msg_log::add("There is no apparent effect.");
                }

                return;
        }

        if (player_sees_actor) {
                const std::string actor_name =
                        text_format::first_to_upper(
                                actor.name_the());

                msg_log::add("A misty haze surrounds " + actor_name + ".");
        }

        const int idx = rnd::idx(spells);

        delete spells[idx].spell;

        spells.erase(std::begin(spells) + idx);
}

}  // namespace terrain

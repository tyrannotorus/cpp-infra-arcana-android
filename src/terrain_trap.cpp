// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "terrain_trap.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
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
#include "item.hpp"
#include "item_data.hpp"
#include "item_factory.hpp"
#include "map.hpp"
#include "msg_log.hpp"
#include "player_bon.hpp"
#include "player_spells.hpp"
#include "property.hpp"
#include "property_data.hpp"
#include "property_factory.hpp"
#include "property_handler.hpp"
#include "query.hpp"
#include "sound.hpp"
#include "spells.hpp"
#include "teleport.hpp"
#include "terrain_data.hpp"
#include "terrain_factory.hpp"
#include "text_format.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------

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
        ASSERT(id != TrapId::END);

        m_is_hidden = true;

        auto* const terrain_here = map::g_terrain.at(m_pos);

        if (!terrain_here->can_have_trap()) {
                TRACE << "Cannot place trap on terrain id: "
                      << (int)terrain_here->id()
                      << std::endl
                      << "Trap id: " << (int)id
                      << std::endl;

                ASSERT(false);

                return false;
        }

        auto try_make_impl = [&](const TrapId trap_id) {
                auto* impl = make_trap_impl_from_id(trap_id);

                auto valid = impl->on_place();

                if (valid == TrapPlacementValid::yes) {
                        m_trap_impl = impl;
                }
                else {
                        // Placement not valid
                        delete impl;
                }
        };

        if (id == TrapId::any) {
                // Attempt to set a trap implementation until succeeding
                while (true) {
                        const int last = (int)TrapId::END - 1;

                        const auto random_id = (TrapId)rnd::range(0, last);

                        try_make_impl(random_id);

                        if (m_trap_impl) {
                                // Trap placement is good!
                                break;
                        }
                }
        }
        else {
                // Make a specific trap type

                // NOTE: This may fail, in which case we have no trap
                // implementation. The trap creator is responsible for handling
                // this situation.
                try_make_impl(id);
        }

        return m_trap_impl != nullptr;
}

TrapImpl* Trap::make_trap_impl_from_id(const TrapId trap_id)
{
        switch (trap_id) {
        case TrapId::dart:
                return new TrapDart(m_pos, this);
                break;

        case TrapId::spear:
                return new TrapSpear(m_pos, this);
                break;

        case TrapId::blinding:
                return new TrapBlindingFlash(m_pos, this);
                break;

        case TrapId::deafening:
                return new TrapDeafening(m_pos, this);
                break;

        case TrapId::teleport:
                return new TrapTeleport(m_pos, this);
                break;

        case TrapId::summon:
                return new TrapSummonMon(m_pos, this);
                break;

        case TrapId::hp_sap:
                return new TrapHpSap(m_pos, this);
                break;

        case TrapId::spi_sap:
                return new TrapSpiSap(m_pos, this);
                break;

        case TrapId::smoke:
                return new TrapSmoke(m_pos, this);
                break;

        case TrapId::fire:
                return new TrapFire(m_pos, this);
                break;

        case TrapId::alarm:
                return new TrapAlarm(m_pos, this);
                break;

        case TrapId::web:
                return new TrapWeb(m_pos, this);
                break;

        case TrapId::slow:
                return new TrapSlow(m_pos, this);
                break;

        case TrapId::curse:
                return new TrapCurse(m_pos, this);
                break;

        case TrapId::unlearn_spell:
                return new TrapUnlearnSpell(m_pos, this);
                break;

        case TrapId::END:
        case TrapId::any:
                break;
        }

        return nullptr;
}

void Trap::on_hit(
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

        return m_trap_impl->m_type;
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

                TRACE_VERBOSE << "Number of turns until trigger: "
                              << m_nr_turns_until_trigger << std::endl;

                if (m_nr_turns_until_trigger == 0) {
                        // NOTE: This will reset number of turns until triggered
                        trigger_trap(nullptr);
                }
        }
}

void Trap::trigger_start(const actor::Actor* actor)
{
        TRACE_FUNC_BEGIN_VERBOSE;

        ASSERT(m_trap_impl);

        if (actor::is_player(actor)) {
                // Reveal trap if triggered by player stepping on it
                if (is_hidden()) {
                        reveal(PrintRevealMsg::no);
                }

                map::g_player->update_fov();

                states::draw();
        }

        if (is_magical()) {
                // TODO: Play sfx for magic traps (if player)
        }
        else if (type() != TrapId::web) {
                // Not magical, not spider web
                std::string msg = "I hear a click.";

                auto alerts = AlertsMon::no;

                if (actor::is_player(actor)) {
                        alerts = AlertsMon::yes;

                        // If player triggering, use more foreboding message
                        msg += "..";
                }

                Snd snd(
                        msg,
                        audio::SfxId::mechanical_trap_trigger,
                        IgnoreMsgIfOriginSeen::no,
                        m_pos,
                        nullptr,
                        SndVol::low,
                        alerts);

                snd.run();

                if (actor::is_player(actor)) {
                        const bool is_deaf =
                                map::g_player->m_properties.has(
                                        PropId::deaf);

                        if (is_deaf) {
                                msg_log::add(
                                        "I feel the ground shifting "
                                        "slightly under my foot.");
                        }

                        msg_log::more_prompt();
                }
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

        TRACE_FUNC_END_VERBOSE;
}

AllowAction Trap::pre_bump(actor::Actor& actor_bumping)
{
        if (!actor::is_player(&actor_bumping) ||
            actor_bumping.m_properties.has(PropId::confused)) {
                return AllowAction::yes;
        }

        const auto& props = actor_bumping.m_properties;

        if (map::g_seen.at(m_pos) &&
            !m_is_hidden &&
            !props.has(PropId::ethereal) &&
            !props.has(PropId::flying) &&
            !props.has(PropId::tiny_flying)) {
                // The trap is known, and will be triggered by the player

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

                const auto query_result = query::yes_or_no();

                msg_log::clear();

                return (
                        (query_result == BinaryAnswer::no)
                                ? AllowAction::no
                                : AllowAction::yes);
        }
        else {
                // The trap is unknown, or will not be triggered by the player -
                // delegate the question to the mimicked terrain

                const auto result = m_mimic_terrain->pre_bump(actor_bumping);

                return result;
        }
}

void Trap::bump(actor::Actor& actor_bumping)
{
        TRACE_FUNC_BEGIN_VERBOSE;

        const auto& d = *actor_bumping.m_data;

        const auto& props = actor_bumping.m_properties;

        if (props.has(PropId::ethereal) ||
            props.has(PropId::flying) ||
            props.has(PropId::tiny_flying) ||
            (d.actor_size < actor::Size::humanoid) ||
            d.is_spider) {
                TRACE_FUNC_END_VERBOSE;

                return;
        }

        if (!actor::is_player(&actor_bumping)) {
                // Put some extra restrictions on monsters triggering traps.
                // This helps prevent stupid situations like a group of monsters
                // in a small room repeatedly triggering a trap.
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

void Trap::disarm()
{
        const bool is_magic_trap = is_magical();

        if (is_magic_trap && (player_bon::bg() != Bg::occultist)) {
                msg_log::add("I do not know how to dispel magic traps.");

                return;
        }

        if (m_nr_turns_until_trigger != -1) {
                msg_log::add("It cannot be disarmed now!");

                return;
        }

        msg_log::add(m_trap_impl->disarm_msg());

        destroy();

        if (is_magic_trap) {
                map::g_player->restore_sp(rnd::range(1, 6), true);
        }

        game_time::tick();
}

void Trap::destroy()
{
        ASSERT(m_mimic_terrain);

        // Magical traps and webs simply "dissapear" (place their mimic
        // terrain), and mechanical traps puts rubble.

        if (is_magical() || type() == TrapId::web) {
                auto* const f_tmp = m_mimic_terrain;

                m_mimic_terrain = nullptr;

                // NOTE: This call destroys the object!
                map::update_terrain(f_tmp);
        }
        else {
                // "Mechanical" trap
                map::update_terrain(
                        terrain::make(terrain::Id::rubble_low, m_pos));
        }
}

DidTriggerTrap Trap::trigger_trap(actor::Actor* const actor)
{
        TRACE_FUNC_BEGIN_VERBOSE;

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

        TRACE_FUNC_END_VERBOSE;

        return DidTriggerTrap::yes;
}

void Trap::reveal(const PrintRevealMsg print_reveal_msg)
{
        TRACE_FUNC_BEGIN_VERBOSE;

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

        TRACE_FUNC_END_VERBOSE;
}

void Trap::on_revealed_from_searching()
{
        game::incr_player_xp(1);
}

std::string Trap::name(const Article article) const
{
        return (
                m_is_hidden
                        ? m_mimic_terrain->name(article)
                        : m_trap_impl->name(article));
}

Color Trap::color_default() const
{
        return (
                m_is_hidden
                        ? m_mimic_terrain->color()
                        : m_trap_impl->color());
}

char Trap::character() const
{
        return m_is_hidden
                ? m_mimic_terrain->character()
                : m_trap_impl->character();
}

gfx::TileId Trap::tile() const
{
        return m_is_hidden ? m_mimic_terrain->tile() : m_trap_impl->tile();
}

Matl Trap::matl() const
{
        return m_is_hidden ? m_mimic_terrain->matl() : m_data->matl_type;
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
        auto offsets = dir_utils::g_cardinal_list;

        rnd::shuffle(offsets);

        const int nr_steps_min = 2;
        const int nr_steps_max = g_fov_radi_int;

        auto trap_plament_valid = TrapPlacementValid::no;

        for (const P& d : offsets) {
                P p = m_pos;

                for (int i = 0; i <= nr_steps_max; ++i) {
                        p += d;

                        const auto* const terrain = map::g_terrain.at(p);

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
        TRACE_FUNC_BEGIN_VERBOSE;

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
                wpn = static_cast<item::Wpn*>(
                        item::make(item::Id::trap_dart_poison));
        }
        else {
                // Not poisoned
                wpn = static_cast<item::Wpn*>(
                        item::make(item::Id::trap_dart));
        }

        // Fire!
        attack::ranged(
                nullptr,
                m_dart_origin,
                aim_pos,
                *wpn);

        delete wpn;

        TRACE_FUNC_END_VERBOSE;
}

TrapSpear::TrapSpear(P pos, Trap* const base_trap) :
        MechTrapImpl(pos, TrapId::spear, base_trap),
        m_is_poisoned((map::g_dlvl >= g_dlvl_harder_traps) && rnd::one_in(4)),

        m_is_spear_origin_destroyed(false)
{}

TrapPlacementValid TrapSpear::on_place()
{
        auto offsets = dir_utils::g_cardinal_list;

        rnd::shuffle(offsets);

        auto trap_plament_valid = TrapPlacementValid::no;

        for (const P& d : offsets) {
                const P p = m_pos + d;

                const auto* const terrain = map::g_terrain.at(p);

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
        TRACE_FUNC_BEGIN_VERBOSE;

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
        auto* const actor_on_trap = map::living_actor_at(m_pos);

        if (actor_on_trap) {
                // Make a temporary spear weapon
                item::Wpn* wpn = nullptr;

                if (m_is_poisoned) {
                        wpn = static_cast<item::Wpn*>(
                                item::make(item::Id::trap_spear_poison));
                }
                else {
                        // Not poisoned
                        wpn = static_cast<item::Wpn*>(
                                item::make(item::Id::trap_spear));
                }

                // Attack!
                attack::melee(
                        nullptr,
                        m_spear_origin,
                        *actor_on_trap,
                        *wpn);

                delete wpn;
        }

        TRACE_FUNC_BEGIN_VERBOSE;
}

void TrapBlindingFlash::trigger()
{
        TRACE_FUNC_BEGIN_VERBOSE;

        if (map::g_seen.at(m_pos)) {
                msg_log::add("There is an intense flash of light!");
        }

        explosion::run(
                m_pos,
                ExplType::apply_prop,
                EmitExplSnd::no,
                -1,
                ExplExclCenter::no,
                {property_factory::make(PropId::blind)},
                colors::yellow());

        TRACE_FUNC_END_VERBOSE;
}

void TrapDeafening::trigger()
{
        TRACE_FUNC_BEGIN_VERBOSE;

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
                {property_factory::make(PropId::deaf)},
                colors::light_white());

        TRACE_FUNC_END_VERBOSE;
}

void TrapTeleport::trigger()
{
        TRACE_FUNC_BEGIN_VERBOSE;

        auto* const actor_here = map::living_actor_at(m_pos);

        ASSERT(actor_here);

        if (!actor_here) {
                // Should never happen
                return;
        }

        const auto is_player = actor::is_player(actor_here);
        const auto can_see = actor_here->m_properties.allow_see();
        const auto player_sees_actor = actor::can_player_see_actor(*actor_here);
        const auto actor_name = actor_here->name_the();
        const auto is_hidden = m_base_trap->is_hidden();

        if (is_player) {
                map::update_vision();

                if (can_see) {
                        std::string msg = "A beam of light shoots out from";

                        if (!is_hidden) {
                                msg += " a curious shape on";
                        }

                        msg += " the floor!";

                        msg_log::add(msg);
                }
                else {
                        // Cannot see
                        msg_log::add("I feel a peculiar energy around me!");
                }
        }
        else {
                // Is a monster
                if (player_sees_actor) {
                        msg_log::add(
                                "A beam shoots out under " + actor_name + ".");
                }
        }

        teleport(*actor_here);

        TRACE_FUNC_END_VERBOSE;
}

void TrapSummonMon::trigger()
{
        TRACE_FUNC_BEGIN;

        auto* const actor_here = map::living_actor_at(m_pos);

        ASSERT(actor_here);

        if (!actor_here) {
                // Should never happen
                return;
        }

        const bool is_player = actor::is_player(actor_here);
        const bool is_hidden = m_base_trap->is_hidden();

        TRACE_VERBOSE << "Is player: " << is_player << std::endl;

        if (!is_player) {
                TRACE_VERBOSE << "Not triggered by player" << std::endl;
                TRACE_FUNC_END_VERBOSE;
                return;
        }

        const bool can_see = actor_here->m_properties.allow_see();
        TRACE_VERBOSE << "Actor can see: " << can_see << std::endl;

        const std::string actor_name = actor_here->name_the();
        TRACE_VERBOSE << "Actor name: " << actor_name << std::endl;

        map::g_player->update_fov();

        if (can_see) {
                std::string msg = "A beam of light shoots out from";

                if (!is_hidden) {
                        msg += " a curious shape on";
                }

                msg += " the floor!";

                msg_log::add(msg);
        }
        else {
                // Cannot see
                msg_log::add("I feel a peculiar energy around me!");
        }

        TRACE << "Finding summon candidates" << std::endl;
        std::vector<actor::Id> summon_bucket;

        for (size_t i = 0; i < (size_t)actor::Id::END; ++i) {
                const auto& data = actor::g_data[i];

                if (data.can_be_summoned_by_mon &&
                    data.spawn_min_dlvl <= (map::g_dlvl + 2)) {
                        summon_bucket.push_back((actor::Id)i);
                }
        }

        if (summon_bucket.empty()) {
                TRACE_VERBOSE << "No eligible candidates found" << std::endl;
        }
        else {
                // Eligible monsters found
                const auto id_to_summon = rnd::element(summon_bucket);

                TRACE_VERBOSE << "Actor id: " << int(id_to_summon) << std::endl;

                const auto summoned =
                        actor::spawn(m_pos, {id_to_summon}, map::rect())
                                .make_aware_of_player();

                std::for_each(
                        std::begin(summoned.monsters),
                        std::end(summoned.monsters),
                        [](auto* const mon) {
                                auto* prop_summoned =
                                        property_factory::make(
                                                PropId::summoned);

                                prop_summoned->set_indefinite();

                                mon->m_properties.apply(prop_summoned);

                                auto* prop_waiting =
                                        property_factory::make(
                                                PropId::waiting);

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
        TRACE_FUNC_BEGIN_VERBOSE;

        auto* const actor_here = map::living_actor_at(m_pos);

        ASSERT(actor_here);

        if (!actor_here) {
                // Should never happen
                return;
        }

        const bool is_player = actor::is_player(actor_here);
        const bool is_hidden = m_base_trap->is_hidden();

        TRACE_VERBOSE << "Is player: " << is_player << std::endl;

        if (!is_player) {
                TRACE_VERBOSE << "Not triggered by player" << std::endl;

                TRACE_FUNC_END_VERBOSE;

                return;
        }

        const bool can_see = actor_here->m_properties.allow_see();

        TRACE_VERBOSE << "Actor can see: " << can_see << std::endl;

        const std::string actor_name = actor_here->name_the();

        TRACE_VERBOSE << "Actor name: " << actor_name << std::endl;

        if (can_see) {
                std::string msg = "A beam of light shoots out from";

                if (!is_hidden) {
                        msg += " a curious shape on";
                }

                msg += " the floor!";

                msg_log::add(msg);
        }
        else {
                // Cannot see
                msg_log::add("I feel a peculiar energy around me!");
        }

        auto* const hp_sap = property_factory::make(PropId::hp_sap);

        hp_sap->set_indefinite();

        actor_here->m_properties.apply(hp_sap);

        TRACE_FUNC_END_VERBOSE;
}

void TrapSpiSap::trigger()
{
        TRACE_FUNC_BEGIN_VERBOSE;

        auto* const actor_here = map::living_actor_at(m_pos);

        ASSERT(actor_here);

        if (!actor_here) {
                // Should never happen
                return;
        }

        const bool is_player = actor::is_player(actor_here);
        const bool is_hidden = m_base_trap->is_hidden();

        TRACE_VERBOSE << "Is player: " << is_player << std::endl;

        if (!is_player) {
                TRACE_VERBOSE << "Not triggered by player" << std::endl;

                TRACE_FUNC_END_VERBOSE;

                return;
        }

        const bool can_see = actor_here->m_properties.allow_see();

        TRACE_VERBOSE << "Actor can see: " << can_see << std::endl;

        const std::string actor_name = actor_here->name_the();

        TRACE_VERBOSE << "Actor name: " << actor_name << std::endl;

        if (can_see) {
                std::string msg = "A beam of light shoots out from";

                if (!is_hidden) {
                        msg += " a curious shape on";
                }

                msg += " the floor!";

                msg_log::add(msg);
        }
        else {
                // Cannot see
                msg_log::add("I feel a peculiar energy around me!");
        }

        auto* const sp_sap = property_factory::make(PropId::spi_sap);

        sp_sap->set_indefinite();

        actor_here->m_properties.apply(sp_sap);

        TRACE_FUNC_END_VERBOSE;
}

void TrapSmoke::trigger()
{
        TRACE_FUNC_BEGIN_VERBOSE;

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

        TRACE_FUNC_END_VERBOSE;
}

void TrapFire::trigger()
{
        TRACE_FUNC_BEGIN_VERBOSE;

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
                {property_factory::make(PropId::burning)});

        TRACE_FUNC_END_VERBOSE;
}

void TrapAlarm::trigger()
{
        TRACE_FUNC_BEGIN_VERBOSE;

        Snd snd(
                "An alarm sounds!",
                audio::SfxId::END,
                IgnoreMsgIfOriginSeen::no,
                m_pos,
                nullptr,
                SndVol::high,
                AlertsMon::yes);

        snd.run();

        TRACE_FUNC_END_VERBOSE;
}

void TrapWeb::trigger()
{
        TRACE_FUNC_BEGIN_VERBOSE;

        auto* const actor_here = map::living_actor_at(m_pos);

        ASSERT(actor_here);

        if (!actor_here) {
                return;
        }

        if (actor::is_player(actor_here)) {
                if (actor_here->m_properties.allow_see()) {
                        msg_log::add(
                                "I am entangled in a spider web!");
                }
                else {
                        // Cannot see
                        msg_log::add(
                                "I am entangled in a sticky mass of threads!");
                }
        }
        else {
                // Is a monster
                if (actor::can_player_see_actor(*actor_here)) {
                        const std::string actor_name =
                                text_format::first_to_upper(
                                        actor_here->name_the());

                        msg_log::add(
                                actor_name +
                                " is entangled in a huge spider web!");
                }
        }

        auto* const entangled = property_factory::make(PropId::entangled);

        entangled->set_indefinite();

        actor_here->m_properties.apply(
                entangled,
                PropSrc::intr,
                false,
                Verbose::no);

        // Players getting stuck in spider webs alerts all spiders
        if (actor::is_player(actor_here)) {
                for (auto* const actor : game_time::g_actors) {
                        if (actor::is_player(actor) ||
                            !actor->m_data->is_spider) {
                                continue;
                        }

                        actor->become_aware_player(actor::AwareSource::other);
                }
        }

        TRACE_FUNC_END_VERBOSE;
}

void TrapSlow::trigger()
{
        TRACE_FUNC_BEGIN_VERBOSE;

        auto* const actor_here = map::living_actor_at(m_pos);

        ASSERT(actor_here);

        if (!actor_here) {
                // Should never happen
                return;
        }

        actor_here->m_properties.apply(
                property_factory::make(PropId::slowed));

        TRACE_FUNC_END_VERBOSE;
}

void TrapCurse::trigger()
{
        TRACE_FUNC_BEGIN_VERBOSE;

        auto* const actor_here = map::living_actor_at(m_pos);

        ASSERT(actor_here);

        if (!actor_here) {
                // Should never happen
                return;
        }

        actor_here->m_properties.apply(
                property_factory::make(PropId::cursed));

        TRACE_FUNC_END_VERBOSE;
}

void TrapUnlearnSpell::trigger()
{
        TRACE_FUNC_BEGIN_VERBOSE;

        auto* const actor_here = map::living_actor_at(m_pos);

        if (!actor_here) {
                // Should never happen
                ASSERT(false);

                return;
        }

        // TODO: Monsters could unlearn spells too
        if (!actor::is_player(actor_here)) {
                return;
        }

        const bool can_see = actor_here->m_properties.allow_see();
        const bool is_hidden = m_base_trap->is_hidden();

        if (can_see) {
                std::string msg = "A beam of light shoots out from";

                if (!is_hidden) {
                        msg += " a curious shape on";
                }

                msg += " the floor!";

                msg_log::add(msg);
        }
        else {
                // Cannot see
                msg_log::add("I feel a peculiar energy around me!");
        }

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

        const SpellId id = rnd::element(id_bucket);

        player_spells::forget_spell(id);

        TRACE_FUNC_END_VERBOSE;
}

}  // namespace terrain

// =============================================================================
// Copyright 2011-2023 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "property.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <optional>
#include <ostream>

#include "actor.hpp"
#include "actor_data.hpp"
#include "actor_death.hpp"
#include "actor_eat.hpp"
#include "actor_factory.hpp"
#include "actor_hit.hpp"
#include "actor_player_state.hpp"
#include "actor_see.hpp"
#include "array2.hpp"
#include "audio_data.hpp"
#include "colors.hpp"
#include "common_text.hpp"
#include "debug.hpp"
#include "direction.hpp"
#include "draw_blast.hpp"
#include "explosion.hpp"
#include "fov.hpp"
#include "game_time.hpp"
#include "global.hpp"
#include "hints.hpp"
#include "insanity.hpp"
#include "inventory.hpp"
#include "item.hpp"
#include "item_data.hpp"
#include "item_explosive.hpp"
#include "item_factory.hpp"
#include "knockback.hpp"
#include "line_calc.hpp"
#include "map.hpp"
#include "map_parsing.hpp"
#include "mapgen.hpp"
#include "misc.hpp"
#include "msg_log.hpp"
#include "player_bon.hpp"
#include "pos.hpp"
#include "property_data.hpp"
#include "property_factory.hpp"
#include "property_handler.hpp"
#include "random.hpp"
#include "rect.hpp"
#include "saving.hpp"
#include "sound.hpp"
#include "teleport.hpp"
#include "terrain.hpp"
#include "terrain_data.hpp"
#include "terrain_factory.hpp"
#include "text_format.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static bool is_player_aware_of_hostile_mon_at(const P& pos)
{
        const actor::Actor* const mon = map::living_actor_at(pos);

        return (
                mon &&
                mon->is_player_aware_of_me() &&
                !map::g_player->is_leader_of(mon));
}

static void bless_adjacent(const P& pos)
{
        // "Bless" adjacent fountains
        for (const P& d : dir_utils::g_dir_list_w_center) {
                const auto p_adj = pos + d;

                terrain::Terrain* const terrain = map::g_terrain.at(p_adj);

                if (terrain->id() != terrain::Id::fountain) {
                        continue;
                }

                static_cast<terrain::Fountain*>(terrain)->bless();
        }
}

static void curse_adjacent(const P& pos)
{
        // "Curse" adjacent fountains
        for (const P& d : dir_utils::g_dir_list_w_center) {
                const auto p_adj = pos + d;

                terrain::Terrain* const terrain = map::g_terrain.at(p_adj);

                if (terrain->id() != terrain::Id::fountain) {
                        continue;
                }

                static_cast<terrain::Fountain*>(terrain)->curse();
        }
}

namespace prop
{
void run_alter_env_effect(const P& origin, const int change_pos_one_in_n)
{
        Array2<bool> blocked(map::dims());

        map_parsers::BlocksWalking(ParseActors::no)
                .run(blocked, blocked.rect());

        const std::vector<terrain::Id> free_terrains = {
                terrain::Id::door,  // Can be toggled to passable
        };

        const int blocked_w = blocked.w();
        const int blocked_h = blocked.h();

        for (int x = 0; x < blocked_w; ++x) {
                for (int y = 0; y < blocked_h; ++y) {
                        const P p(x, y);

                        if (map_parsers::IsAnyOfTerrains(free_terrains).run(p)) {
                                blocked.at(p) = false;
                        }
                }
        }

        Array2<bool> has_actor(map::dims());

        for (actor::Actor* actor : game_time::g_actors) {
                if (actor->m_state != ActorState::destroyed) {
                        has_actor.at(actor->m_pos) = true;
                }
        }

        // Some blocking terrain must be reachable so that the player is not
        // prevented from progressing (other blocking terrain such as Monoliths
        // may be walled in however).
        const std::vector<terrain::Id> terrains_must_be_reachable = {
                terrain::Id::stairs,
                terrain::Id::crystal_key,
        };

        std::vector<P> positions_must_be_reachable;

        for (const P& p : blocked.rect().positions()) {
                if (map_parsers::IsAnyOfTerrains(terrains_must_be_reachable).run(p)) {
                        positions_must_be_reachable.push_back(p);
                }
        }

        const int r = 3;

        // NOTE: The first elements of these two vectors have a special meaning,
        // see comment below.
        //
        // NOTE: Do not include "liquid" terrain here, as then the "magic pool"
        // (LiquidType "magic_water") can be destroyed.
        //
        const std::vector<terrain::Id> spawnable_passable_terrains = {
                terrain::Id::floor,
                terrain::Id::rubble_low,
                terrain::Id::grass,
                terrain::Id::vines,
                terrain::Id::chains,
        };

        const std::vector<terrain::Id> spawnable_blocking_terrains = {
                terrain::Id::wall,
                terrain::Id::rubble_high,
                terrain::Id::grate,
                terrain::Id::stalagmite,
        };

        const map_parsers::IsAnyOfTerrains is_spawnable_passable(spawnable_passable_terrains);
        const map_parsers::IsAnyOfTerrains is_spawnable_blocking(spawnable_blocking_terrains);

        // Use the first element of the above vectors with this chance, instead
        // of a random element. Putting too many different terrains looks messy.
        const Fraction normal_terrain_chance(5, 8);

        const R area(
                std::max(1, origin.x - r),
                std::max(1, origin.y - r),
                std::min(map::w() - 2, origin.x + r),
                std::min(map::h() - 2, origin.y + r));

        for (const P& p : area.positions()) {
                if (!rnd::one_in(change_pos_one_in_n) ||
                    has_actor.at(p) ||
                    map::g_items.at(p)) {
                        continue;
                }

                if (is_spawnable_blocking.run(p)) {
                        blocked.at(p) = false;

                        if (map_parsers::is_map_connected(blocked, positions_must_be_reachable)) {
                                const terrain::Id id =
                                        normal_terrain_chance.roll()
                                        ? spawnable_passable_terrains.at(0)
                                        : rnd::element(spawnable_passable_terrains);

                                map::update_terrain(terrain::make(id, p));
                        }
                        else {
                                blocked.at(p) = true;
                        }
                }
                else if (is_spawnable_passable.run(p)) {
                        blocked.at(p) = true;

                        if (map_parsers::is_map_connected(blocked, positions_must_be_reachable)) {
                                const terrain::Id id =
                                        normal_terrain_chance.roll()
                                        ? spawnable_blocking_terrains.at(0)
                                        : rnd::element(spawnable_blocking_terrains);

                                map::update_terrain(terrain::make(id, p));
                        }
                        else {
                                blocked.at(p) = false;
                        }
                }
        }
}

Prop::Prop(Id id) :
        m_id(id),
        m_data(g_data[(size_t)id]),
        m_nr_turns_left(m_data.std_rnd_turns.roll()),
        m_nr_dlvls_left(m_data.std_rnd_dlvls.roll())
{
}

void Prop::set_duration(const int nr_turns)
{
        ASSERT(nr_turns > 0);

        m_duration_mode = PropDurationMode::specific;

        m_nr_turns_left = nr_turns;
}

void Blessed::on_applied()
{
        m_owner->m_properties.end_prop(
                Id::cursed,
                PropEndConfig(
                        PropEndAllowCallEndHook::no,
                        PropEndAllowMsg::no,
                        PropEndAllowHistoricMsg::yes));

        bless_adjacent(m_owner->m_pos);
}

void Blessed::on_more(const Prop& new_prop)
{
        (void)new_prop;

        bless_adjacent(m_owner->m_pos);
}

int Blessed::ability_mod(const AbilityId ability) const
{
        switch (ability) {
        case AbilityId::melee:
        case AbilityId::ranged:
        case AbilityId::dodging:
        case AbilityId::stealth:
        case AbilityId::searching:
                return 10;

        case AbilityId::END:
                break;
        }

        return 0;
}

void Cursed::on_applied()
{
        m_owner->m_properties.end_prop(
                Id::blessed,
                PropEndConfig(
                        PropEndAllowCallEndHook::no,
                        PropEndAllowMsg::no,
                        PropEndAllowHistoricMsg::yes));

        curse_adjacent(m_owner->m_pos);
}

void Cursed::on_more(const Prop& new_prop)
{
        (void)new_prop;

        curse_adjacent(m_owner->m_pos);
}

int Cursed::ability_mod(const AbilityId ability) const
{
        switch (ability) {
        case AbilityId::melee:
        case AbilityId::ranged:
        case AbilityId::dodging:
        case AbilityId::stealth:
        case AbilityId::searching:
                return -10;

        case AbilityId::END:
                break;
        }

        return 0;
}

void Doomed::on_applied()
{
        m_owner->m_properties.end_prop(
                Id::blessed,
                PropEndConfig(
                        PropEndAllowCallEndHook::no,
                        PropEndAllowMsg::no,
                        PropEndAllowHistoricMsg::yes));

        curse_adjacent(m_owner->m_pos);
}

void Doomed::on_more(const Prop& new_prop)
{
        (void)new_prop;

        curse_adjacent(m_owner->m_pos);
}

bool Doomed::allow_read_chance(const Verbose verbose) const
{
        if (rnd::percent(10)) {
                if (verbose == Verbose::yes) {
                        if (actor::is_player(m_owner)) {
                                msg_log::add(common_text::g_miscast_player);
                        }
                        else if (actor::can_player_see_actor(*m_owner)) {
                                const std::string name =
                                        text_format::first_to_upper(
                                                m_owner->name_the());

                                msg_log::add(
                                        name +
                                        " " +
                                        common_text::g_miscast_mon);
                        }
                }

                return false;
        }
        else {
                return true;
        }
}

bool Doomed::allow_cast_intr_spell_chance(const Verbose verbose) const
{
        if (rnd::percent(10)) {
                if (verbose == Verbose::yes) {
                        if (actor::is_player(m_owner)) {
                                msg_log::add(common_text::g_miscast_player);
                        }
                        else if (actor::can_player_see_actor(*m_owner)) {
                                const std::string name =
                                        text_format::first_to_upper(
                                                m_owner->name_the());

                                msg_log::add(
                                        name +
                                        " " +
                                        common_text::g_miscast_mon);
                        }
                }

                return false;
        }
        else {
                return true;
        }
}

int Doomed::ability_mod(const AbilityId ability) const
{
        switch (ability) {
        case AbilityId::melee:
        case AbilityId::ranged:
        case AbilityId::dodging:
        case AbilityId::stealth:
        case AbilityId::searching:
                return -20;

        case AbilityId::END:
                break;
        }

        return 0;
}

void Entangled::on_applied()
{
        // TODO: Rather than doing this on the "on_applied" hook (which should
        // probably be reserved for when the property actually does get applied,
        // i.e. it shall not be removed), consider checking this elsewhere,
        // perhaps in a new function such as "is_resisting"
        try_player_end_with_machete();
}

PropEnded Entangled::affect_move_dir(Dir& dir)
{
        if (dir == Dir::center) {
                return PropEnded::no;
        }

        bool did_end_with_matchete = try_player_end_with_machete();

        if (did_end_with_matchete) {
                return PropEnded::yes;
        }

        dir = Dir::center;

        if (actor::is_player(m_owner)) {
                msg_log::add("I struggle to tear free!", colors::msg_bad());
        }
        else {
                // Is monster
                if (actor::can_player_see_actor(*m_owner)) {
                        const std::string actor_name_the =
                                text_format::first_to_upper(
                                        m_owner->name_the());

                        msg_log::add(
                                actor_name_the + " struggles to tear free.",
                                colors::msg_good());
                }
        }

        if (rnd::one_in(6)) {
                m_owner->m_properties.end_prop(id());

                return PropEnded::yes;
        }

        return PropEnded::no;
}

bool Entangled::try_player_end_with_machete()
{
        if (!actor::is_player(m_owner)) {
                return false;
        }

        item::Item* item = m_owner->m_inv.item_in_slot(SlotId::wpn);

        if (item && (item->id() == item::Id::machete)) {
                msg_log::add("I cut myself free with my Machete.");

                m_owner->m_properties.end_prop(
                        id(),
                        PropEndConfig(
                                PropEndAllowCallEndHook::no,
                                PropEndAllowMsg::no,
                                PropEndAllowHistoricMsg::yes));

                return true;
        }

        return false;
}

PropEnded Stuck::affect_move_dir(Dir& dir)
{
        if (dir == Dir::center) {
                return PropEnded::no;
        }

        dir = Dir::center;

        if (actor::is_player(m_owner)) {
                msg_log::add("I struggle to pull free!", colors::msg_bad());
        }
        else {
                // Is monster
                if (actor::can_player_see_actor(*m_owner)) {
                        const std::string actor_name_the =
                                text_format::first_to_upper(
                                        m_owner->name_the());

                        msg_log::add(
                                actor_name_the + " struggles to pull free.",
                                colors::msg_good());
                }
        }

        if (rnd::one_in(6)) {
                m_owner->m_properties.end_prop(id());

                return PropEnded::yes;
        }

        return PropEnded::no;
}

void Slowed::on_applied()
{
        m_owner->m_properties.end_prop(
                Id::hasted,
                PropEndConfig(
                        PropEndAllowCallEndHook::no,
                        PropEndAllowMsg::no,
                        PropEndAllowHistoricMsg::yes));
}

void Hasted::on_applied()
{
        m_owner->m_properties.end_prop(
                Id::slowed,
                PropEndConfig(
                        PropEndAllowCallEndHook::no,
                        PropEndAllowMsg::no,
                        PropEndAllowHistoricMsg::yes));
}

void ExtraHasted::on_applied()
{
        m_owner->m_properties.end_prop(
                Id::slowed,
                PropEndConfig(
                        PropEndAllowCallEndHook::no,
                        PropEndAllowMsg::no,
                        PropEndAllowHistoricMsg::yes));
}

void Summoned::on_end()
{
        m_owner->m_state = ActorState::destroyed;

        actor::unset_actor_as_leader_and_target_for_all_mon(m_owner);
}

PropEnded Infected::on_actor_turn()
{
#ifndef NDEBUG
        ASSERT(!m_owner->m_properties.has(Id::diseased));
#endif  // NDEBUG

        if (actor::is_player(m_owner)) {
                if (actor::player_state::g_active_medical_bag) {
                        ++m_nr_turns_left;

                        return PropEnded::no;
                }

                if ((m_nr_turns_left < 20) &&
                    !has_warned &&
                    rnd::coin_toss()) {
                        msg_log::add(
                                "My infection is getting worse!",
                                colors::msg_note(),
                                MsgInterruptPlayer::no,
                                MorePromptOnMsg::yes);

                        has_warned = true;
                }
        }

        const bool apply_disease = (m_nr_turns_left <= 1);

        if (!apply_disease) {
                return PropEnded::no;
        }

        // Time to apply disease

        actor::Actor* const owner = m_owner;

        owner->m_properties.end_prop(
                id(),
                PropEndConfig(
                        PropEndAllowCallEndHook::no,
                        PropEndAllowMsg::no,
                        PropEndAllowHistoricMsg::yes));

        // NOTE: This property is now deleted

        Prop* prop_diseased = make(Id::diseased);

        prop_diseased->set_indefinite();

        owner->m_properties.apply(prop_diseased);

        msg_log::more_prompt();

        return PropEnded::yes;
}

void Infected::on_applied()
{
        if (actor::is_player(m_owner)) {
                hints::display(hints::Id::infected);
        }
}

int Diseased::affect_max_hp(const int hp_max) const
{
        return hp_max / 2;
}

void Diseased::on_applied()
{
        // End infection
        m_owner->m_properties.end_prop(
                Id::infected,
                PropEndConfig(
                        PropEndAllowCallEndHook::no,
                        PropEndAllowMsg::no,
                        PropEndAllowHistoricMsg::yes));
}

bool Diseased::is_resisting_other_prop(const Id prop_id) const
{
#ifndef NDEBUG
        ASSERT(!m_owner->m_properties.has(Id::infected));
#endif  // NDEBUG

        // Getting infected while already diseased is just annoying
        return prop_id == Id::infected;
}

PropEnded Descend::on_actor_turn()
{
        ASSERT(actor::is_player(m_owner));

        if (m_nr_turns_left <= 0) {
                game_time::g_is_magic_descend_nxt_std_turn = true;
        }

        return PropEnded::no;
}

void ZuulPossessPriest::on_placed()
{
        // If the number left allowed to spawn is zero now, this means that Zuul
        // has been spawned for the first time - hide Zuul inside a priest.
        // Otherwise we do nothing, and just spawn Zuul. When the possessed
        // actor is killed, Zuul will be allowed to spawn infinitely (this is
        // handled elsewhere).

        const int nr_left_allowed = m_owner->m_data->nr_left_allowed_to_spawn;

        ASSERT(nr_left_allowed <= 0);

        const bool should_possess = (nr_left_allowed >= 0);

        if (should_possess) {
                m_owner->m_state = ActorState::destroyed;

                actor::Actor* actor = actor::make("MON_CULTIST_PRIEST", m_owner->m_pos);

                Prop* prop = make(prop::Id::possessed_by_zuul);

                prop->set_indefinite();

                actor->m_properties.apply(
                        prop,
                        PropSrc::intr,
                        true,
                        Verbose::no);

                actor->restore_hp(999, false, Verbose::no);
        }
}

void PossessedByZuul::on_death()
{
        // An actor possessed by Zuul has died - release Zuul!

        if (actor::can_player_see_actor(*m_owner)) {
                const std::string& name1 =
                        text_format::first_to_upper(
                                m_owner->name_the());

                const std::string& name2 = actor::g_data["MON_ZUUL"].name_the;

                msg_log::add(name1 + " was possessed by " + name2 + "!");
        }

        m_owner->m_state = ActorState::destroyed;

        const auto& pos = m_owner->m_pos;

        terrain::make_gore(pos);

        terrain::make_blood(pos);

        // Zuul is now free, allow it to spawn infinitely
        actor::g_data["MON_ZUUL"].nr_left_allowed_to_spawn = -1;

        actor::spawn(
                pos,
                {"MON_ZUUL"},
                map::rect())
                .make_aware_of_player();

        map::update_vision();
}

void Shapeshifts::on_placed()
{
        // NOTE: This function will only ever run for the original shapeshifter
        // monster, never for the monsters shapeshifted into.

        set_indefinite();

        // The shapeshifter should change into something else asap.
        m_countdown = 0;
}

void Shapeshifts::on_std_turn()
{
        if (!m_owner->is_alive()) {
                return;
        }

        if (!m_owner->m_properties.allow_act()) {
                return;
        }

        --m_countdown;

        if (m_countdown <= 0) {
                shapeshift(Verbose::yes);
        }
}

void Shapeshifts::on_death()
{
        m_owner->m_state = ActorState::destroyed;

        const auto spawned =
                actor::spawn(
                        m_owner->m_pos,
                        {"MON_SHAPESHIFTER"},
                        map::rect());

        map::update_vision();

        ASSERT(!spawned.monsters.empty());

        if (!spawned.monsters.empty()) {
                actor::Actor* const shapeshifter = spawned.monsters[0];

                shapeshifter->m_mon_aware_state.aware_counter =
                        m_owner->m_mon_aware_state.aware_counter;

                shapeshifter->m_mon_aware_state.wary_counter =
                        m_owner->m_mon_aware_state.wary_counter;

                // No more shapeshifting
                shapeshifter->m_properties.end_prop(prop::Id::shapeshifts);

                // It's affraid!
                shapeshifter->m_properties.apply(
                        prop::make(prop::Id::terrified));

                // Make the Shapeshifter skip a turn - it looks better if it
                // doesn't immediately move to an adjacent cell when spawning
                Prop* const waiting = prop::make(prop::Id::waiting);

                waiting->set_duration(1);

                shapeshifter->m_properties.apply(waiting);
        }
}

void Shapeshifts::shapeshift(const Verbose verbose) const
{
        std::vector<std::string> mon_id_bucket;

        for (const auto& it : actor::g_data) {
                const actor::ActorData& d = it.second;

                Range allowed_mon_depth_range(
                        d.spawn_min_dlvl - 2,
                        d.spawn_max_dlvl);

                if (allowed_mon_depth_range.max == -1) {
                        allowed_mon_depth_range.max = 999;
                }

                if (!d.can_be_shapeshifted_into ||
                    (d.id == m_owner->id()) ||
                    (d.id == "MON_SHAPESHIFTER") ||
                    !d.is_auto_spawn_allowed ||
                    d.is_unique ||
                    !allowed_mon_depth_range.is_in_range(map::g_dlvl)) {
                        continue;
                }

                mon_id_bucket.push_back(d.id);
        }

        if (mon_id_bucket.empty()) {
                ASSERT(false);

                return;
        }

        if ((verbose == Verbose::yes) &&
            actor::can_player_see_actor(*m_owner)) {
                msg_log::add("It changes shape!");

                draw_blast_at_cells({m_owner->m_pos}, colors::yellow());
        }

        m_owner->m_state = ActorState::destroyed;

        const auto spawned =
                actor::spawn(
                        m_owner->m_pos,
                        {rnd::element(mon_id_bucket)},
                        map::rect());

        if (spawned.monsters.size() != 1) {
                ASSERT(false);

                return;
        }

        actor::Actor* const mon = spawned.monsters[0];

        // Set HP percentage a bit higher than the previous monster
        {
                const int max_hp_prev = std::max(1, m_owner->m_base_max_hp);
                const int hp_pct_prev = (m_owner->m_hp * 100) / max_hp_prev;
                const int hp_pct_new = std::clamp(hp_pct_prev + 15, 1, 100);

                mon->m_hp = (actor::max_hp(*mon) * hp_pct_new) / 100;

                // New HP value should be >= 0, and also not "unreasonably" high
                mon->m_hp = std::clamp(mon->m_hp, 1, 10'000);
        }

        // Set same awareness as the previous monster
        {
                mon->m_mon_aware_state.aware_counter =
                        m_owner->m_mon_aware_state.aware_counter;

                mon->m_mon_aware_state.wary_counter =
                        m_owner->m_mon_aware_state.wary_counter;
        }

        // Apply shapeshifting on the new monster
        {
                auto* const shapeshifts =
                        static_cast<prop::Shapeshifts*>(
                                prop::make(prop::Id::shapeshifts));

                shapeshifts->set_indefinite();

                shapeshifts->m_countdown = rnd::range(3, 5);

                mon->m_properties.apply(shapeshifts);
        }

        map::update_vision();
}

PropEnded ZealotStop::affect_move_dir(Dir& dir)
{
        const int stop_pct_chance = 7;

        if (!m_owner->is_alive() ||
            !m_owner->m_properties.allow_act() ||
            (dir == Dir::center) ||
            m_owner->m_properties.has(prop::Id::burning) ||
            m_owner->m_properties.has(prop::Id::entangled) ||
            m_owner->m_properties.has(prop::Id::stuck) ||
            m_owner->m_properties.has(prop::Id::terrified) ||
            m_owner->m_properties.has(prop::Id::frenzied) ||
            !rnd::percent(stop_pct_chance)) {
                return PropEnded::no;
        }

        if (actor::can_player_see_actor(*m_owner)) {
                const auto name = text_format::first_to_upper(m_owner->name_the());

                msg_log::add(name + " stops and gropes about.");
        }

        dir = Dir::center;

        return PropEnded::no;
}

PropEnded Poisoned::on_actor_turn()
{
        if (!m_owner->is_alive()) {
                return PropEnded::no;
        }

        // NOTE: Monsters have a shorter, more intense poisoning

        if (!actor::is_player(m_owner) && (m_nr_turns_left > 1)) {
                --m_nr_turns_left;
        }

        const auto owner_hp = m_owner->m_hp;

        int dmg = 0;
        int pct_chance = 0;

        if (actor::is_player(m_owner)) {
                dmg = 1;
                pct_chance = (int)std::pow((double)owner_hp, 1.5);
        }
        else {
                dmg = 2;
                pct_chance = (int)std::pow((double)owner_hp, 2.0);
        }

        if (owner_hp <= dmg) {
                return PropEnded::no;
        }

        if (!rnd::percent(pct_chance)) {
                return PropEnded::no;
        }

        if (actor::is_player(m_owner)) {
                msg_log::add(
                        "I am suffering from the poison!",
                        colors::msg_bad(),
                        MsgInterruptPlayer::yes);
        }
        else if (actor::can_player_see_actor(*m_owner)) {
                // Is seen monster
                const std::string actor_name_the =
                        text_format::first_to_upper(
                                m_owner->name_the());

                msg_log::add(
                        actor_name_the +
                        " suffers from poisoning!");
        }

        actor::hit(*m_owner, dmg, DmgType::pure, nullptr);

        return PropEnded::no;
}

PropEnded Aiming::on_hit(
        const int dmg,
        const DmgType dmg_type,
        actor::Actor* const attacker)
{
        (void)dmg;
        (void)dmg_type;
        (void)attacker;

        m_owner->m_properties.end_prop(id());

        return PropEnded::yes;
}

bool Terrified::allow_attack_melee(const Verbose verbose) const
{
        if (actor::is_player(m_owner) && verbose == Verbose::yes) {
                msg_log::add("I am too terrified to engage in close combat!");
        }

        return false;
}

bool Terrified::allow_attack_ranged(const Verbose verbose) const
{
        (void)verbose;
        return true;
}

void Terrified::on_applied()
{
        // If this is a monster, we reset its last direction moved. Otherwise it
        // would probably tend to move toward the player even while terrified
        // (the AI would typically use the idle movement algorithm, which
        // favors stepping in the same direction as the last move).

        if (!actor::is_player(m_owner)) {
                m_owner->m_ai_state.last_dir_moved = Dir::center;
        }
}

PropEnded Nailed::affect_move_dir(Dir& dir)
{
        if (dir == Dir::center) {
                return PropEnded::no;
        }

        // Allow the player to melee attack adjacent known monsters
        if (actor::is_player(m_owner)) {
                const auto intended_target =
                        m_owner->m_pos +
                        dir_utils::offset(dir);

                if (is_player_aware_of_hostile_mon_at(intended_target)) {
                        return PropEnded::no;
                }
        }

        dir = Dir::center;

        if (actor::is_player(m_owner)) {
                msg_log::add(
                        "I struggle to tear out the spike!",
                        colors::msg_bad());
        }
        else {
                // Is monster
                if (actor::can_player_see_actor(*m_owner)) {
                        const std::string actor_name_the =
                                text_format::first_to_upper(
                                        m_owner->name_the());

                        msg_log::add(
                                actor_name_the + " struggles in pain!",
                                colors::msg_good());
                }
        }

        actor::hit(*m_owner, rnd::range(1, 3), DmgType::pure, nullptr);

        if (!m_owner->is_alive() || !rnd::one_in(4)) {
                return PropEnded::no;
        }

        --m_nr_spikes;

        if (m_nr_spikes > 0) {
                if (actor::is_player(m_owner)) {
                        msg_log::add("I rip out a spike from my flesh!");
                }
                else if (actor::can_player_see_actor(*m_owner)) {
                        const std::string actor_name_the =
                                text_format::first_to_upper(
                                        m_owner->name_the());

                        msg_log::add(actor_name_the + " tears out a spike!");
                }
        }

        return PropEnded::no;
}

int Nailed::ability_mod(const AbilityId ability) const
{
        switch (ability) {
        case AbilityId::ranged:
                return -10;

        case AbilityId::melee:
                return -20;

        default:
                return 0;
        }
}

void Wound::save() const
{
        saving::put_int(m_nr_wounds);
}

void Wound::load()
{
        m_nr_wounds = saving::get_int();
}

std::string Wound::name_short() const
{
        return "Wounded(" + std::to_string(m_nr_wounds) + ")";
}

int Wound::ability_mod(const AbilityId ability) const
{
        // A player with Survivalist receives no ability penalties
        if (actor::is_player(m_owner) &&
            player_bon::has_trait(Trait::survivalist)) {
                return 0;
        }

        if (ability == AbilityId::melee) {
                return (m_nr_wounds * -5);
        }
        else if (ability == AbilityId::dodging) {
                return (m_nr_wounds * -5);
        }

        return 0;
}

int Wound::affect_max_hp(const int hp_max) const
{
        const int pen_pct_per_wound = 10;

        int hp_pen_pct = m_nr_wounds * pen_pct_per_wound;

        // The HP penalty is halved for a player with Survivalist
        if (actor::is_player(m_owner) &&
            player_bon::has_trait(Trait::survivalist)) {
                hp_pen_pct /= 2;
        }

        // Cap the penalty percentage
        hp_pen_pct = std::min(70, hp_pen_pct);

        return (hp_max * (100 - hp_pen_pct)) / 100;
}

std::string Wound::get_one_wound_heal_str() const
{
        return "A wound is healed.";
}

std::string Wound::get_all_wounds_heal_str() const
{
        return "All my wounds are healed!";
}

std::string Wound::msg_end_player() const
{
        return (
                (m_nr_wounds > 1)
                        ? get_all_wounds_heal_str()
                        : get_one_wound_heal_str());
}

void Wound::print_one_wound_healed_msg() const
{
        msg_log::add(get_one_wound_heal_str());
}

void Wound::heal_one_wound()
{
        ASSERT(m_nr_wounds > 0);

        --m_nr_wounds;

        if (m_nr_wounds > 0) {
                print_one_wound_healed_msg();
        }
        else {
                // This was the last wound, end self
                m_owner->m_properties.end_prop(id());
        }
}

void Wound::on_more(const Prop& new_prop)
{
        (void)new_prop;

        ++m_nr_wounds;

        if (m_nr_wounds >= 5) {
                if (actor::is_player(m_owner)) {
                        msg_log::add("I succumb to my wounds!");
                }

                actor::kill(
                        *m_owner,
                        IsDestroyed::no,
                        AllowGore::no,
                        AllowDropItems::yes);
        }
}

PropEnded Flagellant::on_hit(
        const int dmg,
        const DmgType dmg_type,
        actor::Actor* const attacker)
{
        (void)dmg_type;
        (void)attacker;

        if ((m_owner->m_hp - dmg) <= 6) {
                Prop* const moribund = make(Id::moribund);

                const Range duration(6, 8);

                moribund->set_duration(duration.roll());

                m_owner->m_properties.apply(moribund);
        }

        return PropEnded::no;
}

int Moribund::ability_mod(AbilityId ability) const
{
        if (ability != AbilityId::melee) {
                return 0;
        }

        int melee_bonus = 30;

        if (player_bon::has_trait(Trait::fervor)) {
                melee_bonus *= 2;
        }

        return melee_bonus;
}

int Moribund::armor_points() const
{
        int armor_bonus = 3;

        if (player_bon::has_trait(Trait::fervor)) {
                armor_bonus *= 2;
        }

        return armor_bonus;
}

HpSap::HpSap() :
        Prop(prop::Id::hp_sap),
        m_nr_drained(rnd::range(1, 3)) {}

void HpSap::save() const
{
        saving::put_int(m_nr_drained);
}

void HpSap::load()
{
        m_nr_drained = saving::get_int();
}

int HpSap::affect_max_hp(const int hp_max) const
{
        return (hp_max - m_nr_drained);
}

void HpSap::on_more(const Prop& new_prop)
{
        m_nr_drained += static_cast<const HpSap*>(&new_prop)->m_nr_drained;
}

void HpSap::set_nr_drained(const int value)
{
        m_nr_drained = value;
}

SpiSap::SpiSap() :
        Prop(prop::Id::spi_sap),
        m_nr_drained(1) {}

void SpiSap::save() const
{
        saving::put_int(m_nr_drained);
}

void SpiSap::load()
{
        m_nr_drained = saving::get_int();
}

int SpiSap::affect_max_spi(const int spi_max) const
{
        return (spi_max - m_nr_drained);
}

void SpiSap::on_more(const Prop& new_prop)
{
        m_nr_drained += static_cast<const SpiSap*>(&new_prop)->m_nr_drained;
}

MindSap::MindSap() :
        Prop(prop::Id::mind_sap),
        m_nr_drained(rnd::range(1, 3)) {}

void MindSap::save() const
{
        saving::put_int(m_nr_drained);
}

void MindSap::load()
{
        m_nr_drained = saving::get_int();
}

int MindSap::player_extra_min_shock() const
{
        return m_nr_drained;
}

void MindSap::on_more(const Prop& new_prop)
{
        m_nr_drained += static_cast<const MindSap*>(&new_prop)->m_nr_drained;
}

bool Confused::allow_read_absolute(const Verbose verbose) const
{
        if (actor::is_player(m_owner) && verbose == Verbose::yes) {
                msg_log::add("I am too confused to read.");
        }

        return false;
}

bool Confused::allow_cast_intr_spell_absolute(
        const Verbose verbose) const
{
        if (actor::is_player(m_owner) && (verbose == Verbose::yes)) {
                msg_log::add("I am too confused to concentrate!");
        }

        return false;
}

bool Confused::allow_pray(Verbose verbose) const
{
        if (actor::is_player(m_owner) && (verbose == Verbose::yes)) {
                msg_log::add("I am too confused to concentrate!");
        }

        return false;
}

bool Confused::allow_attack_melee(const Verbose verbose) const
{
        (void)verbose;

        if (!actor::is_player(m_owner)) {
                return rnd::coin_toss();
        }

        return true;
}

bool Confused::allow_attack_ranged(const Verbose verbose) const
{
        (void)verbose;

        if (!actor::is_player(m_owner)) {
                return rnd::coin_toss();
        }

        return true;
}

PropEnded Confused::affect_move_dir(Dir& dir)
{
        if (dir == Dir::center) {
                return PropEnded::no;
        }

        if (!rnd::one_in(8)) {
                return PropEnded::no;
        }

        Array2<bool> blocked(map::dims());

        const R area_check_blocked(
                m_owner->m_pos - P(1, 1),
                m_owner->m_pos + P(1, 1));

        map_parsers::BlocksActor(*m_owner, ParseActors::yes)
                .run(blocked,
                     area_check_blocked,
                     MapParseMode::overwrite);

        std::vector<P> d_bucket;

        for (const P& d : dir_utils::g_dir_list) {
                const P tgt_p = m_owner->m_pos + d;

                if (!blocked.at(tgt_p)) {
                        d_bucket.push_back(d);
                }
        }

        if (!d_bucket.empty()) {
                const P& d = rnd::element(d_bucket);

                dir = dir_utils::dir(d);
        }

        return PropEnded::no;
}

void Hallucinating::on_applied()
{
        if (!actor::is_player(m_owner)) {
                return;
        }

        apply_fake_actor_data();

        create_fake_stairs();
}

void Hallucinating::on_std_turn()
{
        if (!actor::is_player(m_owner)) {
                return;
        }

        if (rnd::one_in(10)) {
                apply_fake_actor_data();
        }

        if (rnd::one_in(250)) {
                create_fake_stairs();
        }
}

void Hallucinating::on_end()
{
        if (!actor::is_player(m_owner)) {
                return;
        }

        clear_fake_actor_data();

        clear_all_fake_stairs();
}

void Hallucinating::apply_fake_actor_data() const
{
        const auto allowed_data = get_allowed_fake_mon_data();

        if (allowed_data.empty()) {
                ASSERT(false);

                return;
        }

        for (actor::Actor* const actor : game_time::g_actors) {
                if (!actor::is_player(actor)) {
                        actor->m_mimic_data = rnd::element(allowed_data);
                }
        }
}

void Hallucinating::clear_fake_actor_data() const
{
        for (actor::Actor* const actor : game_time::g_actors) {
                if (!actor::is_player(actor)) {
                        actor->m_mimic_data = nullptr;
                }
        }
}

std::vector<const actor::ActorData*> Hallucinating::get_allowed_fake_mon_data() const
{
        std::vector<const actor::ActorData*> result;

        result.reserve(actor::g_data.size());

        for (const auto& it : actor::g_data) {
                const actor::ActorData& d = it.second;

                if ((d.id == "MON_PLAYER") ||
                    (d.id == "MON_SPECTRAL_WPN") ||
                    (d.id == "MON_STRANGE_COLOR") ||
                    (d.id == "MON_CULTIST")) {
                        continue;
                }

                const bool gives_xp = (d.mon_shock_lvl > MonShockLvl::none);

                if (gives_xp && !d.has_player_seen) {
                        continue;
                }

                result.push_back(&d);
        }

        return result;
}

void Hallucinating::create_fake_stairs() const
{
        const bool was_valid_before = mapgen::g_is_map_valid;

        const auto p = mapgen::make_stairs_at_random_pos();

        // It's OK to fail placing the stairs, don't let this invalidate the map
        // (although map validity probably doesn't matter at all at this point).
        mapgen::g_is_map_valid = was_valid_before;

        if ((p.x > 0) && (p.y > 0)) {
                terrain::Terrain* const terrain = map::g_terrain.at(p);

                if (terrain->id() != terrain::Id::stairs) {
                        ASSERT(false);

                        return;
                }

                auto* const stairs = static_cast<terrain::Stairs*>(terrain);

                stairs->set_fake();
        }
}

void Hallucinating::clear_all_fake_stairs() const
{
        for (terrain::Terrain* const terrain : map::g_terrain) {
                if (terrain->id() != terrain::Id::stairs) {
                        continue;
                }

                // Is stairs

                const auto* const stairs = static_cast<const terrain::Stairs*>(terrain);

                if (!stairs->is_fake()) {
                        continue;
                }

                // Is fake stairs

                map::update_terrain(
                        terrain::make(
                                terrain::Id::rubble_low,
                                terrain->pos()));
        }
}

void AstralOpiumAddict::save() const
{
        saving::put_int(m_shock_lvl);
        saving::put_int(m_nr_dlvls_to_penalty);
        saving::put_int(m_nr_turns_to_penalty);
}

void AstralOpiumAddict::load()
{
        m_shock_lvl = saving::get_int();
        m_nr_dlvls_to_penalty = saving::get_int();
        m_nr_turns_to_penalty = saving::get_int();
}

std::string AstralOpiumAddict::name_short() const
{
        std::string str = m_data.name_short;

        if (is_active()) {
                str += "(" + std::to_string(m_shock_lvl) + "%)";
        }

        return str;
}

void AstralOpiumAddict::on_applied()
{
        reset_penalty_countdown();

        m_shock_lvl = 10;
}

void AstralOpiumAddict::on_std_turn()
{
        if ((m_nr_dlvls_to_penalty == 0) && (m_nr_turns_to_penalty > 0)) {
                --m_nr_turns_to_penalty;

                if (m_nr_turns_to_penalty == 0) {
                        msg_log::add(
                                "I crave Astral Opium!!",
                                colors::msg_note(),
                                MsgInterruptPlayer::no,
                                MorePromptOnMsg::yes);
                }
        }
}

void AstralOpiumAddict::on_new_dlvl()
{
        if (m_nr_dlvls_to_penalty > 0) {
                --m_nr_dlvls_to_penalty;
        }
}

void AstralOpiumAddict::on_more(const Prop& new_prop)
{
        (void)new_prop;

        msg_log::add("I need more!!");

        reset_penalty_countdown();

        m_shock_lvl += 5;
}

void AstralOpiumAddict::reset_penalty_countdown()
{
        m_nr_dlvls_to_penalty = rnd::range(0, 1);
        m_nr_turns_to_penalty = rnd::range(50, 75);
}

bool AstralOpiumAddict::is_active() const
{
        return ((m_nr_dlvls_to_penalty == 0) && (m_nr_turns_to_penalty == 0));
}

int AstralOpiumAddict::player_extra_min_shock() const
{
        if (is_active()) {
                return m_shock_lvl;
        }
        else {
                return 0;
        }
}

int Frenzied::ability_mod(const AbilityId ability) const
{
        if (ability == AbilityId::melee) {
                return 10;
        }
        else {
                return 0;
        }
}

bool Frenzied::allow_move_dir(const Dir dir)
{
        if (!actor::is_player(m_owner) || (dir == Dir::center)) {
                return true;
        }

        const P new_pos = m_owner->m_pos + dir_utils::offset(dir);

        const actor::Actor* actor_at_tgt = map::living_actor_at(new_pos);

        if (actor_at_tgt &&
            actor_at_tgt->is_player_aware_of_me() &&
            !actor_at_tgt->is_actor_my_leader(map::g_player)) {
                // There is a known hostile monster at the target position,
                // allow "moving" into it (i.e. try attacking it).
                return true;
        }

        const std::vector<actor::Actor*> seen_foes =
                actor::seen_foes(*m_owner);

        if (seen_foes.empty()) {
                return true;
        }

        Array2<bool> blocked(map::dims());

        map_parsers::BlocksActor(*m_owner, ParseActors::no)
                .run(blocked, blocked.rect());

        // Mark the positions of all seen actors as free (the monsters may be
        // inside wall cells, e.g. worms crawling through rubble).
        for (const actor::Actor* const actor : seen_foes) {
                blocked.at(actor->m_pos) = false;
        }

        std::vector<const actor::Actor*> seen_reachable_foes;
        seen_reachable_foes.reserve(seen_foes.size());

        for (const actor::Actor* const actor : seen_foes) {
                const auto line =
                        line_calc::calc_new_line(
                                m_owner->m_pos,
                                actor->m_pos,
                                true,
                                999,
                                false);

                if (line.empty()) {
                        continue;
                }

                const auto is_blocked =
                        std::any_of(
                                std::cbegin(line),
                                std::cend(line),
                                [blocked](const P& p) {
                                        return blocked.at(p);
                                });

                if (is_blocked) {
                        continue;
                }

                seen_reachable_foes.push_back(actor);
        }

        if (seen_reachable_foes.empty()) {
                return true;
        }

        // There are seen reachable seen foes

        for (const actor::Actor* actor : seen_reachable_foes) {
                const int old_dist = king_dist(m_owner->m_pos, actor->m_pos);
                const int new_dist = king_dist(new_pos, actor->m_pos);

                if (new_dist < old_dist) {
                        // This step would take the player closer to at least
                        // one reachable seen foe - allow the move.
                        return true;
                }
        }

        msg_log::add("I will not step away!");

        return false;
}

bool Frenzied::is_resisting_other_prop(const prop::Id prop_id) const
{
        return (
                (prop_id == prop::Id::confused) ||
                (prop_id == prop::Id::fainted) ||
                (prop_id == prop::Id::terrified) ||
                (prop_id == prop::Id::weakened));
}

void Frenzied::on_applied()
{
        const PropEndConfig prop_end_config(
                PropEndAllowCallEndHook::no,
                PropEndAllowMsg::no,
                PropEndAllowHistoricMsg::yes);

        const auto props_ended = {
                prop::Id::confused,
                prop::Id::fainted,
                prop::Id::terrified,
                prop::Id::weakened,
                prop::Id::meditative_focused};

        for (auto prop : props_ended) {
                m_owner->m_properties.end_prop(prop, prop_end_config);
        }
}

void Frenzied::on_end()
{
        // Only the player (except for Ghoul background) gets tired after a
        // frenzy (it looks weird for monsters)
        if (actor::is_player(m_owner) && (player_bon::bg() != Bg::ghoul)) {
                // TODO: Base the weakened duration on the number of turns
                // frenzied was active (m_nr_turns_active) (within some upper
                // and lower bounds)?
                m_owner->m_properties.apply(
                        prop::make(prop::Id::weakened));
        }
}

bool Frenzied::allow_read_absolute(const Verbose verbose) const
{
        if (actor::is_player(m_owner) && verbose == Verbose::yes) {
                msg_log::add("I am too enraged to read!");
        }

        return false;
}

bool Frenzied::allow_cast_intr_spell_absolute(
        const Verbose verbose) const
{
        if (actor::is_player(m_owner) && (verbose == Verbose::yes)) {
                msg_log::add("I am too enraged to concentrate!");
        }

        return false;
}

bool Frenzied::allow_pray(Verbose verbose) const
{
        if (actor::is_player(m_owner) && (verbose == Verbose::yes)) {
                msg_log::add("I am too enraged to concentrate!");
        }

        return false;
}

PropEnded Burning::on_actor_turn()
{
        if (actor::is_player(m_owner)) {
                msg_log::add("AAAARGH IT BURNS!!!", colors::light_red());
        }

        actor::hit(*m_owner, rnd::range(1, 3), DmgType::fire, nullptr);

        return PropEnded::no;
}

bool Burning::allow_read_chance(const Verbose verbose) const
{
        if (rnd::coin_toss()) {
                if (verbose == Verbose::yes) {
                        if (actor::is_player(m_owner)) {
                                msg_log::add(common_text::g_miscast_player);
                        }
                        else if (actor::can_player_see_actor(*m_owner)) {
                                const std::string name =
                                        text_format::first_to_upper(
                                                m_owner->name_the());

                                msg_log::add(
                                        name +
                                        " " +
                                        common_text::g_miscast_mon);
                        }
                }

                return false;
        }
        else {
                return true;
        }
}

bool Burning::allow_cast_intr_spell_chance(const Verbose verbose) const
{
        if (rnd::coin_toss()) {
                if (verbose == Verbose::yes) {
                        if (actor::is_player(m_owner)) {
                                msg_log::add(common_text::g_miscast_player);
                        }
                        else if (actor::can_player_see_actor(*m_owner)) {
                                const std::string name =
                                        text_format::first_to_upper(
                                                m_owner->name_the());

                                msg_log::add(
                                        name +
                                        " " +
                                        common_text::g_miscast_mon);
                        }
                }

                return false;
        }
        else {
                return true;
        }
}

bool Burning::allow_pray(Verbose verbose) const
{
        if (actor::is_player(m_owner) && (verbose == Verbose::yes)) {
                msg_log::add("I am burning!");
        }

        return false;
}

bool Burning::allow_attack_ranged(const Verbose verbose) const
{
        if (actor::is_player(m_owner) && (verbose == Verbose::yes)) {
                msg_log::add("Not while burning.");
        }

        return false;
}

std::optional<Color> Burning::override_actor_color() const
{
        return colors::light_red();
}

PropActResult Recloaks::on_act()
{
        if (m_owner->is_alive() &&
            !m_owner->m_properties.has(prop::Id::cloaked) &&
            rnd::one_in(8)) {
                Prop* prop_cloaked = prop::make(prop::Id::cloaked);

                prop_cloaked->set_indefinite();

                m_owner->m_properties.apply(prop_cloaked);

                game_time::tick();

                PropActResult result;

                result.did_action = DidAction::yes;
                result.prop_ended = PropEnded::no;

                return result;
        }

        return {};
}

bool Blind::allow_read_absolute(const Verbose verbose) const
{
        if (actor::is_player(m_owner) && (verbose == Verbose::yes)) {
                msg_log::add("I cannot read while blind.");
        }

        return false;
}

bool Blind::should_update_vision_on_toggled() const
{
        return actor::is_player(m_owner);
}

void MeleeCooldown::on_melee_attack()
{
        Prop* const disabled_melee = make(Id::disabled_melee);

        disabled_melee->set_duration(rnd::range(3, 4));

        m_owner->m_properties.apply(disabled_melee);
}

void Paralyzed::on_applied()
{
        if (actor::is_player(m_owner)) {
                item::Explosive* const explosive =
                        actor::player_state::g_active_explosive.get();

                if (explosive) {
                        explosive->on_player_paralyzed();
                }
        }
}

PropEnded Fainted::on_hit(
        const int dmg,
        const DmgType dmg_type,
        actor::Actor* const attacker)
{
        (void)dmg;
        (void)dmg_type;
        (void)attacker;

        m_owner->m_properties.end_prop(id());

        return PropEnded::yes;
}

bool Fainted::should_update_vision_on_toggled() const
{
        return actor::is_player(m_owner);
}

void RShock::on_applied()
{
        if (actor::is_player(m_owner)) {
                actor::player_state::g_nr_turns_until_insanity = -1;
        }
}

DmgResistData RAcid::is_resisting_dmg(const DmgType dmg_type) const
{
        DmgResistData d;

        d.is_resisted = (dmg_type == DmgType::acid);

        d.msg_resist_player = "I feel a faint burning sensation.";

        d.msg_resist_mon = "seems unaffected.";

        return d;
}

DmgResistData RElec::is_resisting_dmg(const DmgType dmg_type) const
{
        DmgResistData d;

        d.is_resisted = (dmg_type == DmgType::electric);

        d.msg_resist_player = "I feel a faint tingle.";

        d.msg_resist_mon = "seems unaffected.";

        return d;
}

bool RConf::is_resisting_other_prop(const prop::Id prop_id) const
{
        return prop_id == prop::Id::confused;
}

void RConf::on_applied()
{
        m_owner->m_properties.end_prop(
                prop::Id::confused,
                PropEndConfig(
                        PropEndAllowCallEndHook::no,
                        PropEndAllowMsg::no,
                        PropEndAllowHistoricMsg::yes));
}

bool RFear::is_resisting_other_prop(const prop::Id prop_id) const
{
        return prop_id == prop::Id::terrified;
}

void RFear::on_applied()
{
        m_owner->m_properties.end_prop(
                prop::Id::terrified,
                PropEndConfig(
                        PropEndAllowCallEndHook::no,
                        PropEndAllowMsg::no,
                        PropEndAllowHistoricMsg::yes));

        if (actor::is_player(m_owner) &&
            m_duration_mode == PropDurationMode::indefinite) {
                insanity::on_permanent_rfear();
        }
}

bool RSlow::is_resisting_other_prop(const prop::Id prop_id) const
{
        return prop_id == prop::Id::slowed;
}

void RSlow::on_applied()
{
        m_owner->m_properties.end_prop(
                prop::Id::slowed,
                PropEndConfig(
                        PropEndAllowCallEndHook::no,
                        PropEndAllowMsg::no,
                        PropEndAllowHistoricMsg::yes));
}

bool RPhys::is_resisting_other_prop(const prop::Id prop_id) const
{
        (void)prop_id;
        return false;
}

void RPhys::on_applied()
{
}

DmgResistData RPhys::is_resisting_dmg(const DmgType dmg_type) const
{
        DmgResistData d;

        d.is_resisted = is_physical_dmg_type(dmg_type);

        d.msg_resist_player = "I resist harm.";

        d.msg_resist_mon = "seems unharmed.";

        return d;
}

bool RFire::is_resisting_other_prop(const prop::Id prop_id) const
{
        return prop_id == prop::Id::burning;
}

void RFire::on_applied()
{
        m_owner->m_properties.end_prop(
                prop::Id::burning,
                PropEndConfig(
                        PropEndAllowCallEndHook::no,
                        PropEndAllowMsg::no,
                        PropEndAllowHistoricMsg::yes));
}

DmgResistData RFire::is_resisting_dmg(const DmgType dmg_type) const
{
        DmgResistData d;

        d.is_resisted = (dmg_type == DmgType::fire);

        d.msg_resist_player = "I feel warm.";

        d.msg_resist_mon = "seems unaffected.";

        return d;
}

bool RPoison::is_resisting_other_prop(const prop::Id prop_id) const
{
        return prop_id == prop::Id::poisoned;
}

void RPoison::on_applied()
{
        m_owner->m_properties.end_prop(
                prop::Id::poisoned,
                PropEndConfig(
                        PropEndAllowCallEndHook::no,
                        PropEndAllowMsg::no,
                        PropEndAllowHistoricMsg::yes));
}

bool RSleep::is_resisting_other_prop(const prop::Id prop_id) const
{
        return prop_id == prop::Id::fainted;
}

void RSleep::on_applied()
{
        m_owner->m_properties.end_prop(
                prop::Id::fainted,
                PropEndConfig(
                        PropEndAllowCallEndHook::no,
                        PropEndAllowMsg::no,
                        PropEndAllowHistoricMsg::yes));
}

bool RDisease::is_resisting_other_prop(const prop::Id prop_id) const
{
        return prop_id == prop::Id::diseased || prop_id == prop::Id::infected;
}

void RDisease::on_applied()
{
        m_owner->m_properties.end_prop(
                prop::Id::diseased,
                PropEndConfig(
                        PropEndAllowCallEndHook::no,
                        PropEndAllowMsg::no,
                        PropEndAllowHistoricMsg::yes));

        m_owner->m_properties.end_prop(
                prop::Id::infected,
                PropEndConfig(
                        PropEndAllowCallEndHook::no,
                        PropEndAllowMsg::no,
                        PropEndAllowHistoricMsg::yes));
}

bool RBlind::is_resisting_other_prop(const prop::Id prop_id) const
{
        return prop_id == prop::Id::blind;
}

void RBlind::on_applied()
{
        m_owner->m_properties.end_prop(
                prop::Id::blind,
                PropEndConfig(
                        PropEndAllowCallEndHook::no,
                        PropEndAllowMsg::no,
                        PropEndAllowHistoricMsg::yes));
}

bool RPara::is_resisting_other_prop(const prop::Id prop_id) const
{
        return prop_id == prop::Id::paralyzed;
}

void RPara::on_applied()
{
        m_owner->m_properties.end_prop(
                prop::Id::paralyzed,
                PropEndConfig(
                        PropEndAllowCallEndHook::no,
                        PropEndAllowMsg::no,
                        PropEndAllowHistoricMsg::yes));
}

bool SeeInvis::is_resisting_other_prop(const prop::Id prop_id) const
{
        return prop_id == prop::Id::blind;
}

void SeeInvis::on_applied()
{
        m_owner->m_properties.end_prop(
                prop::Id::blind,
                PropEndConfig(
                        PropEndAllowCallEndHook::no,
                        PropEndAllowMsg::no,
                        PropEndAllowHistoricMsg::yes));
}

PropEnded Burrowing::on_actor_turn()
{
        const auto& p = m_owner->m_pos;

        map::g_terrain.at(p)->hit(DmgType::pure, nullptr);

        return PropEnded::no;
}

void LgtSens::raise_extra_damage_to(const int dmg)
{
        m_extra_dmg = std::max(dmg, m_extra_dmg);
}

PropActResult Vortex::on_act()
{
        if (actor::is_player(m_owner) || !m_owner->is_alive()) {
                return {};
        }

        if (m_cooldown > 0) {
                --m_cooldown;

                return {};
        }

        if (!actor::is_player(m_owner->m_ai_state.target) ||
            !m_owner->m_ai_state.is_target_seen) {
                return {};
        }

        const auto& player_pos = map::g_player->m_pos;

        if (is_pos_adj(m_owner->m_pos, player_pos, true)) {
                return {};
        }

        if (!rnd::fraction(3, 4)) {
                return {};
        }

        TRACE
                << "Monster with vortex property attempting to pull player"
                << std::endl;

        const auto delta = player_pos - m_owner->m_pos;

        auto knockback_from_pos = player_pos;

        if (delta.x > 1) {
                ++knockback_from_pos.x;
        }

        if (delta.x < -1) {
                --knockback_from_pos.x;
        }

        if (delta.y > 1) {
                ++knockback_from_pos.y;
        }

        if (delta.y < -1) {
                --knockback_from_pos.y;
        }

        if (knockback_from_pos == player_pos) {
                return {};
        }

        TRACE
                << "Pos found to knockback player from: "
                << knockback_from_pos.x << ", "
                << knockback_from_pos.y << std::endl;

        TRACE
                << "Player pos: "
                << player_pos.x << ", " << player_pos.y << std::endl;

        m_owner->make_player_aware_of_me();

        if (actor::can_player_see_actor(*m_owner)) {
                const auto name_the = text_format::first_to_upper(
                        m_owner->name_the());

                msg_log::add(name_the + " pulls me!");
        }
        else {
                msg_log::add("A powerful wind is pulling me!");
        }

        TRACE << "Attempt pull (knockback)" << std::endl;

        // TODO: Add sfx
        knockback::run(
                *map::g_player,
                knockback_from_pos,
                knockback::KnockbackSource::other,
                Verbose::no);

        m_cooldown = 2;

        game_time::tick();

        PropActResult result;

        result.did_action = DidAction::yes;
        result.prop_ended = PropEnded::no;

        return result;
}

void ExplodesOnDeath::on_death()
{
        TRACE_FUNC_BEGIN;

        // Setting the actor to destroyed here, just to make sure there will not
        // be a recursive call chain due to explosion damage to the corpse
        m_owner->m_state = ActorState::destroyed;

        explosion::run(m_owner->m_pos, ExplType::expl);

        TRACE_FUNC_END;
}

void SplitsOnDeath::on_death()
{
        if (actor::is_player(m_owner)) {
                return;
        }

        const bool is_player_seeing_owner =
                actor::can_player_see_actor(*m_owner);

        const int actor_max_hp = actor::max_hp(*m_owner);

        // Do not allow splitting if HP is reduced to this point (if the monster
        // is killed "hard" enough, it doesn't split)
        const bool is_very_destroyed =
                m_owner->m_hp <=
                (actor_max_hp * (-5));

        const auto pos = m_owner->m_pos;

        const auto f_id = map::g_terrain.at(pos)->id();

        if (is_very_destroyed ||
            m_owner->m_properties.has(prop::Id::burning) ||
            (f_id == terrain::Id::chasm) ||
            (game_time::g_actors.size() >= g_max_nr_actors_on_map)) {
                if (is_player_seeing_owner) {
                        // Print a standard death message
                        m_prevent_std_death_msg = false;

                        actor::print_mon_death_msg(*m_owner);
                }

                return;
        }

        // The monster should split

        if (is_player_seeing_owner) {
                // NOTE: This is printed instead of the standard death message
                const std::string name =
                        text_format::first_to_upper(
                                m_owner->name_the());

                msg_log::add(name + " splits.");
        }

        actor::Actor* const leader = m_owner->m_leader;

        const auto spawned =
                actor::spawn(pos, {2, m_owner->id()}, map::rect())
                        .make_aware_of_player()
                        .set_leader(leader);

        std::for_each(
                std::begin(spawned.monsters),
                std::end(spawned.monsters),
                [](actor::Actor* const mon) {
                        Prop* prop_waiting = prop::make(prop::Id::waiting);

                        prop_waiting->set_duration(1);

                        mon->m_properties.apply(prop_waiting);

                        // The new actors should usually not also split
                        if (rnd::fraction(4, 5)) {
                                mon->m_properties.end_prop(prop::Id::splits_on_death);
                        }

                        // Do not print a new "monster in view" message
                        mon->m_mon_aware_state.is_msg_mon_in_view_printed = true;
                });

        // If no leader yet, set the first actor as leader of the second
        if (!leader && (spawned.monsters.size() == 2)) {
                spawned.monsters[1]->m_leader = spawned.monsters[0];
        }

        map::update_vision();
}

void OthersTerrifiedOnDeath::on_death()
{
        const int max_dist = g_fov_radi_int * 2;

        const int duration = rnd::range(6, 8);

        for (actor::Actor* const actor : game_time::g_actors) {
                if (actor == m_owner) {
                        continue;
                }

                if (!actor->is_alive()) {
                        continue;
                }

                if (!actor->m_properties.has(id())) {
                        continue;
                }

                const int dist = king_dist(m_owner->m_pos, actor->m_pos);

                if (dist > max_dist) {
                        continue;
                }

                Prop* const prop = prop::make(prop::Id::terrified);

                prop->set_duration(duration);

                actor->m_properties.apply(prop);
        }
}

PropActResult CorpseEater::on_act()
{
        if (!m_owner->is_alive()) {
                return {};
        }

        auto did_action = DidAction::no;

        // NOTE: Always try to feed if the player is the leader of the monster
        // (its very annoying if your pet refuses to feed when it stands on a
        // corpse), otherwise only occasionally feed.
        if (m_owner->is_actor_my_leader(map::g_player) || rnd::coin_toss()) {
                actor::try_eat_corpse(*m_owner);

                if (did_action == DidAction::yes) {
                        game_time::tick();
                }
        }

        PropActResult result;

        result.did_action = did_action;
        result.prop_ended = PropEnded::no;

        return result;
}

PropActResult Teleports::on_act()
{
        const int teleport_one_in_n = 12;

        if (!m_owner->is_alive() || !rnd::one_in(teleport_one_in_n)) {
                return {};
        }

        teleport(*m_owner);

        game_time::tick();

        PropActResult result;
        result.did_action = DidAction::yes;
        result.prop_ended = PropEnded::no;
        return result;
}

PropActResult TeleportsAway::on_act()
{
        const int dist = king_dist(m_owner->m_pos, map::g_player->m_pos);
        const bool is_player_near = (dist <= 1);

        if (!is_player_near ||
            !m_owner->is_alive() ||
            m_owner->is_actor_my_leader(map::g_player)) {
                return {};
        }

        const int max_dist = rnd::one_in(50) ? -1 : rnd::range(3, 4);

        teleport(
                *m_owner,
                ShouldCtrlTele::never,
                max_dist);

        game_time::tick();

        PropActResult result;
        result.did_action = DidAction::yes;
        result.prop_ended = PropEnded::no;
        return result;
}

void AlwaysAware::on_std_turn()
{
        m_owner->m_mon_aware_state.aware_counter = 10;
}

void CorruptsEnvColor::cycle_graphics()
{
        const Range range(40, 255);

        m_color.set_rgb(
                range.roll(),
                range.roll(),
                range.roll());
}

std::optional<Color> CorruptsEnvColor::override_actor_color() const
{
        return m_color;
}

PropActResult CorruptsEnvColor::on_act()
{
        terrain::Terrain* const terrain = map::g_terrain.at(m_owner->m_pos);

        terrain->try_corrupt_color();

        return {};
}

void AltersEnv::on_std_turn()
{
        run_alter_env_effect(m_owner->m_pos);
}

void Regenerating::on_std_turn()
{
        if (!m_owner->is_alive() ||
            m_owner->m_properties.has(prop::Id::burning) ||
            m_owner->m_properties.has(prop::Id::disabled_hp_regen)) {
                return;
        }

        m_owner->restore_hp(1, false, Verbose::no);
}

PropActResult CorpseRises::on_act()
{
        if (!m_owner->is_corpse() ||
            map::living_actor_at(m_owner->m_pos)) {
                return {};
        }

        const bool is_seen_by_player = actor::can_player_see_actor(*m_owner);

        if (is_seen_by_player) {
                hints::display(hints::Id::destroying_corpses);
        }

        if (m_nr_turns_until_allow_rise > 0) {
                --m_nr_turns_until_allow_rise;

                return {};
        }

        const int rise_one_in_n = 9;

        if (!rnd::one_in(rise_one_in_n)) {
                return {};
        }

        m_owner->m_state = ActorState::alive;

        m_owner->m_hp = actor::max_hp(*m_owner) / 2;

        --m_owner->m_data->nr_kills;

        if (is_seen_by_player) {
                ASSERT(!m_owner->m_data->corpse_name_the.empty());

                const std::string name =
                        text_format::first_to_upper(
                                m_owner->m_data->corpse_name_the);

                msg_log::add(
                        name + " rises again!!",
                        colors::text(),
                        MsgInterruptPlayer::yes);

                map::g_player->incr_shock(4.0, ShockSrc::see_mon);
        }

        m_owner->become_aware_player(actor::AwareSource::other);

        game_time::tick();

        m_has_risen = true;

        PropActResult result;

        result.did_action = DidAction::yes;
        result.prop_ended = PropEnded::no;

        return result;
}

void CorpseRises::on_death()
{
        // If we have already risen before, and were killed again leaving a
        // corpse, destroy the corpse to prevent rising multiple times
        if (m_owner->is_corpse() && m_has_risen) {
                m_owner->m_state = ActorState::destroyed;
                m_owner->m_properties.on_destroyed_alive();
        }
}

void SpawnsZombiePartsOnDestroyed::on_destroyed_alive()
{
        try_spawn_zombie_dust();

        try_spawn_zombie_parts();
}

void SpawnsZombiePartsOnDestroyed::on_destroyed_corpse()
{
        // NOTE: We do not spawn zombie parts when the corpse is destroyed (it's
        // pretty annoying if parts are spawned when you bash a corpse)
        try_spawn_zombie_dust();
}

void SpawnsZombiePartsOnDestroyed::try_spawn_zombie_parts() const
{
        if (m_owner->m_properties.has(prop::Id::burning)) {
                return;
        }

        if (!is_allowed_to_spawn_parts_here()) {
                return;
        }

        const auto pos = m_owner->m_pos;

        // Spawning zombie part monsters is only allowed if the monster is not
        // destroyed "too hard". This is also rewarding heavy weapons, since
        // they will more often prevent spawning
        const bool is_very_destroyed = (m_owner->m_hp <= -8);

        const int summon_one_in_n = 5;

        if (is_very_destroyed || !rnd::one_in(summon_one_in_n)) {
                return;
        }

        std::string id_to_spawn;

        const std::vector<int> weights = {
                25,  // Hand
                25,  // Intestines
                1    // Floating skull
        };

        const int mon_choice = rnd::weighted_choice(weights);

        const std::string my_name = m_owner->name_the();

        std::string spawn_msg;

        switch (mon_choice) {
        case 0:
                id_to_spawn = "MON_CRAWLING_HAND";

                spawn_msg =
                        "The hand of " +
                        my_name +
                        " comes off and starts crawling around!";
                break;

        case 1:
                id_to_spawn = "MON_CRAWLING_INTESTINES";

                spawn_msg =
                        "The intestines of " +
                        my_name +
                        " starts crawling around!";
                break;

        case 2:
                id_to_spawn = "MON_FLOATING_SKULL";

                spawn_msg =
                        "The head of " +
                        my_name +
                        " starts floating around!";
                break;

        default:
                ASSERT(false);
                break;
        }

        if (map::g_seen.at(pos)) {
                ASSERT(!spawn_msg.empty());

                msg_log::add(spawn_msg);

                map::g_player->incr_shock(4.0, ShockSrc::see_mon);
        }

        ASSERT(!id_to_spawn.empty());

        const auto spawned =
                actor::spawn(
                        pos,
                        {id_to_spawn},
                        map::rect())
                        .make_aware_of_player();

        std::for_each(
                std::begin(spawned.monsters),
                std::end(spawned.monsters),
                [](actor::Actor* const mon) {
                        Prop* waiting = prop::make(prop::Id::waiting);

                        waiting->set_duration(1);

                        mon->m_properties.apply(waiting);
                });

        map::update_vision();
}

void SpawnsZombiePartsOnDestroyed::try_spawn_zombie_dust() const
{
        if (!is_allowed_to_spawn_parts_here()) {
                return;
        }

        const int make_dust_one_in_n = 7;

        if (rnd::one_in(make_dust_one_in_n)) {
                item::make_item_on_floor(item::Id::zombie_dust, m_owner->m_pos);
        }
}

bool SpawnsZombiePartsOnDestroyed::is_allowed_to_spawn_parts_here() const
{
        const auto& pos = m_owner->m_pos;

        const auto t_id = map::g_terrain.at(pos)->id();

        return (t_id != terrain::Id::chasm);
}

void Breeds::on_std_turn()
{
        const int spawn_new_one_in_n = 50;

        if (actor::is_player(m_owner) ||
            !m_owner->is_alive() ||
            m_owner->m_properties.has(prop::Id::burning) ||
            m_owner->m_properties.has(prop::Id::paralyzed) ||
            (game_time::g_actors.size() >= g_max_nr_actors_on_map) ||
            !rnd::one_in(spawn_new_one_in_n)) {
                return;
        }

        actor::Actor* const leader_of_spawned_mon =
                m_owner->m_leader
                ? m_owner->m_leader
                : m_owner;

        const auto area_allowed = R(m_owner->m_pos - 1, m_owner->m_pos + 1);

        auto spawned =
                actor::spawn_random_position(
                        {m_owner->id()},
                        area_allowed)
                        .set_leader(leader_of_spawned_mon);

        std::for_each(
                std::begin(spawned.monsters),
                std::end(spawned.monsters),
                [](actor::Actor* const spawned_mon) {
                        Prop* prop_waiting = prop::make(prop::Id::waiting);

                        prop_waiting->set_duration(2);

                        spawned_mon->m_properties.apply(prop_waiting);

                        if (actor::can_player_see_actor(*spawned_mon)) {
                                const std::string name =
                                        text_format::first_to_upper(spawned_mon->name_a());

                                msg_log::add(name + " is spawned.");
                        }
                });

        if (m_owner->is_aware_of_player()) {
                spawned.make_aware_of_player();
        }
}

void VomitsOoze::on_std_turn()
{
        const int spawn_new_one_in_n = m_has_triggered_before ? 15 : 5;

        if (actor::is_player(m_owner) ||
            !m_owner->is_alive() ||
            !m_owner->is_aware_of_player() ||
            m_owner->m_properties.has(prop::Id::burning) ||
            m_owner->m_properties.has(prop::Id::paralyzed) ||
            (game_time::g_actors.size() >= g_max_nr_actors_on_map) ||
            !rnd::one_in(spawn_new_one_in_n)) {
                return;
        }

        const auto area_allowed = R(m_owner->m_pos - 1, m_owner->m_pos + 1);

        std::string ooze_ids[] = {
                "MON_OOZE_PUTRID",
                "MON_OOZE_LURKING",
                "MON_OOZE_POISON"};

        std::vector<std::string> id_bucket;

        for (const std::string& id : ooze_ids) {
                const actor::ActorData& d = actor::g_data[id];

                if (map::g_dlvl >= d.spawn_min_dlvl) {
                        id_bucket.push_back(id);
                }
        }

        // Robustness - always allow at least Putrid Ooze
        if (id_bucket.empty()) {
                id_bucket.emplace_back("MON_OOZE_PUTRID");
        }

        if (actor::can_player_see_actor(*m_owner)) {
                const auto parent_name =
                        text_format::first_to_upper(
                                m_owner->name_the());

                msg_log::add(parent_name + " spews ooze.");
        }

        std::string id_to_spawn = rnd::element(id_bucket);

        actor::Actor* const leader =
                m_owner->m_leader
                ? m_owner->m_leader
                : m_owner;

        actor::MonSpawnResult spawned =
                actor::spawn_random_position(
                        {id_to_spawn},
                        area_allowed)
                        .set_leader(leader);

        std::for_each(
                std::begin(spawned.monsters),
                std::end(spawned.monsters),
                [](auto* const spawned_mon) {
                        Prop* prop_waiting = prop::make(prop::Id::waiting);

                        prop_waiting->set_duration(1);

                        spawned_mon->m_properties.apply(prop_waiting);
                });

        draw_blast_at_seen_actors(spawned.monsters, colors::white());

        map::update_vision();

        if (m_owner->is_aware_of_player()) {
                spawned.make_aware_of_player();
        }

        m_has_triggered_before = true;
}

void ConfusesAdjacent::on_std_turn()
{
        if (!m_owner->is_alive() ||
            !actor::can_player_see_actor(*m_owner) ||
            !map::g_player->m_pos.is_adjacent(m_owner->m_pos)) {
                return;
        }

        if (!map::g_player->m_properties.has(prop::Id::confused)) {
                const std::string msg =
                        text_format::first_to_upper(m_owner->name_the()) +
                        " bewilders me.";

                msg_log::add(msg);
        }

        Prop* prop_confusd = prop::make(prop::Id::confused);

        prop_confusd->set_duration(rnd::range(8, 12));

        map::g_player->m_properties.apply(prop_confusd);
}

void FrenzyPlayerOnSeen::on_player_see()
{
        const int taunt_on_in_n = 3;

        prop::PropHandler& properties = map::g_player->m_properties;

        if (!properties.has(prop::Id::frenzied) &&
            rnd::one_in(taunt_on_in_n)) {
                const std::string name = text_format::first_to_upper(m_owner->name_the());

                msg_log::add(name + " is taunting me!");

                Prop* const frenzy = prop::make(prop::Id::frenzied);

                frenzy->set_duration(rnd::range(20, 35));

                properties.apply(frenzy);
        }
}

void AuraOfDecay::save() const
{
        saving::put_int(m_dmg_range.min);
        saving::put_int(m_dmg_range.max);
}

void AuraOfDecay::load()
{
        m_dmg_range.min = saving::get_int();
        m_dmg_range.max = saving::get_int();
}

int AuraOfDecay::range() const
{
        return g_expl_std_radi;
}

void AuraOfDecay::on_std_turn()
{
        if (!m_owner->is_alive()) {
                return;
        }

        run_effect_on_actors();

        run_effect_on_env();
}

void AuraOfDecay::run_effect_on_actors() const
{
        for (actor::Actor* const actor : game_time::g_actors) {
                if ((actor == m_owner) ||
                    (actor->m_state == ActorState::destroyed) ||
                    (king_dist(m_owner->m_pos, actor->m_pos) > range())) {
                        continue;
                }

                if (actor::can_player_see_actor(*actor)) {
                        print_msg_actor_hit(*actor);
                }

                if (m_allow_instant_kill && rnd::percent(2)) {
                        draw_blast_at_seen_actors({actor}, colors::light_red());

                        actor::kill(
                                *actor,
                                IsDestroyed::yes,
                                AllowGore::yes,
                                AllowDropItems::yes);

                        continue;
                }

                const int dmg = m_dmg_range.roll();

                actor::hit(*actor, dmg, DmgType::pure, m_owner);

                if (!actor::is_player(actor)) {
                        actor->become_aware_player(
                                actor::AwareSource::other);
                }
        }
}

void AuraOfDecay::run_effect_on_env() const
{
        for (const P& d : dir_utils::g_dir_list) {
                const P p = m_owner->m_pos + d;

                if (!map::is_pos_inside_outer_walls(p)) {
                        continue;
                }

                run_effect_on_env_at(p);
        }
}

void AuraOfDecay::run_effect_on_env_at(const P& p) const
{
        terrain::Terrain* const terrain = map::g_terrain.at(p);

        switch (terrain->id()) {
        case terrain::Id::floor: {
                if (rnd::one_in(100)) {
                        map::update_terrain(
                                terrain::make(terrain::Id::rubble_low, p));
                }
        } break;

        case terrain::Id::grass: {
                if (rnd::one_in(10)) {
                        static_cast<terrain::Grass*>(terrain)->m_type =
                                terrain::GrassType::withered;
                }
        } break;

        case terrain::Id::fountain: {
                static_cast<terrain::Fountain*>(terrain)->curse();
        } break;

        case terrain::Id::wall:
        case terrain::Id::rubble_high: {
                if (rnd::one_in(250)) {
                        if (map::g_seen.at(p)) {
                                const std::string name =
                                        text_format::first_to_upper(
                                                terrain->name(Article::the));

                                msg_log::add(name + " collapses!");

                                msg_log::more_prompt();
                        }

                        terrain->hit(DmgType::pure, nullptr);
                }
        } break;

        default:
        {
        } break;
        }
}

void AuraOfDecay::print_msg_actor_hit(const actor::Actor& actor) const
{
        if (actor::is_player(&actor)) {
                msg_log::add("I am decaying!", colors::msg_bad());
        }
}

PropActResult MajorClaphamSummon::on_act()
{
        if (actor::is_player(m_owner) ||
            !m_owner->is_alive() ||
            !actor::is_player(m_owner->m_ai_state.target) ||
            !m_owner->m_ai_state.is_target_seen) {
                return {};
        }

        Snd snd(
                "A voice is calling forth Tomb-Legions!",
                audio::SfxId::END,
                IgnoreMsgIfOriginSeen::yes,
                m_owner->m_pos,
                m_owner,
                SndVol::high,
                AlertsMon::no);

        snd.run();

        if (actor::can_player_see_actor(*m_owner)) {
                msg_log::add("Major Clapham Lee calls forth his Tomb-Legions!");
        }

        std::vector<std::string> ids_to_summon = {"MON_DEAN_HALSEY"};

        const int nr_of_extra_spawns = 4;

        const std::vector<std::string> possible_random_id_choices = {
                "MON_ZOMBIE",
                "MON_BLOATED_ZOMBIE"};

        const std::vector<int> weights = {
                4,
                1};

        for (int i = 0; i < nr_of_extra_spawns; ++i) {
                const int idx = rnd::weighted_choice(weights);

                ids_to_summon.push_back(possible_random_id_choices[idx]);
        }

        const actor::MonSpawnResult spawned =
                actor::spawn(m_owner->m_pos, ids_to_summon, map::rect())
                        .make_aware_of_player()
                        .set_leader(m_owner);

        std::for_each(
                std::begin(spawned.monsters),
                std::end(spawned.monsters),
                [](actor::Actor* const spawned_mon) {
                        Prop* prop_summoned = prop::make(prop::Id::summoned);

                        prop_summoned->set_indefinite();

                        spawned_mon->m_properties.apply(prop_summoned);

                        Prop* waiting = prop::make(prop::Id::waiting);

                        waiting->set_duration(1);

                        spawned_mon->m_properties.apply(waiting);

                        spawned_mon->m_mon_aware_state.is_player_feeling_msg_allowed = false;
                });

        map::update_vision();

        map::g_player->incr_shock(12.0, ShockSrc::see_mon);

        m_owner->m_properties.end_prop(id());

        game_time::tick();

        PropActResult result;

        result.did_action = DidAction::yes;
        result.prop_ended = PropEnded::yes;

        return result;
}

PropActResult AlliesPlayerGhoul::on_act()
{
        if (actor::is_player(m_owner) ||
            !m_owner->is_alive() ||
            !player_bon::is_bg(Bg::ghoul) ||
            m_owner->is_actor_my_leader(map::g_player)) {
                return {};
        }

        Array2<bool> blocks_los(map::dims());

        const R r = fov::fov_rect(map::g_player->m_pos, blocks_los.dims());

        map_parsers::BlocksLos().run(blocks_los, r, MapParseMode::overwrite);

        if (!actor::can_mon_see_actor(*m_owner, *map::g_player, blocks_los)) {
                return {};
        }

        const prop::Id prop_id = id();

        for (actor::Actor* const actor : game_time::g_actors) {
                if (!actor->m_properties.has(prop_id)) {
                        continue;
                }

                if (actor::can_player_see_actor(*actor) &&
                    actor->is_alive() &&
                    !actor->is_actor_my_leader(map::g_player)) {
                        const std::string actor_name =
                                text_format::first_to_upper(
                                        actor->name_the());

                        const std::string pronoun =
                                actor->m_data->is_unique
                                ? "their"
                                : "its";

                        msg_log::add(actor_name + " recognizes me as " + pronoun + " leader.");
                }

                actor::unset_actor_as_leader_and_target_for_all_mon(actor);

                actor->m_leader = map::g_player;

                actor->m_properties.end_prop(prop_id);
        }

        game_time::tick();

        PropActResult result;
        result.did_action = DidAction::yes;
        result.prop_ended = PropEnded::yes;

        return result;
}

void MagicSearching::save() const
{
        saving::put_int(m_range);

        saving::put_bool(m_allow_reveal_items);
        saving::put_bool(m_allow_reveal_creatures);
}

void MagicSearching::load()
{
        m_range = saving::get_int();

        m_allow_reveal_items = saving::get_bool();
        m_allow_reveal_creatures = saving::get_bool();
}

PropEnded MagicSearching::on_actor_turn()
{
        ASSERT(actor::is_player(m_owner));

        const int orig_x = map::g_player->m_pos.x;
        const int orig_y = map::g_player->m_pos.y;

        const int x0 = std::max(0, orig_x - m_range);
        const int y0 = std::max(0, orig_y - m_range);
        const int x1 = std::min(map::w() - 1, orig_x + m_range);
        const int y1 = std::min(map::h() - 1, orig_y + m_range);

        for (int y = y0; y <= y1; ++y) {
                for (int x = x0; x <= x1; ++x) {
                        const P p(x, y);

                        terrain::Terrain* const t = map::g_terrain.at(p);

                        const terrain::Id id = t->id();

                        if ((id == terrain::Id::trap) ||
                            (id == terrain::Id::door) ||
                            (id == terrain::Id::monolith) ||
                            (id == terrain::Id::stairs)) {
                                if (t->is_hidden()) {
                                        map::update_vision();

                                        t->reveal(terrain::PrintRevealMsg::yes);

                                        t->on_revealed_from_searching();

                                        msg_log::more_prompt();
                                }

                                map::memorize_terrain_at(p);
                        }

                        if (m_allow_reveal_items && map::g_items.at(p)) {
                                map::memorize_item_at(p);
                        }
                }
        }

        if (m_allow_reveal_creatures) {
                const int det_mon_multiplier = 20;

                for (actor::Actor* actor : game_time::g_actors) {
                        const auto& p = actor->m_pos;

                        if (actor::is_player(actor) ||
                            !actor->is_alive() ||
                            (king_dist(map::g_player->m_pos, p) > m_range)) {
                                continue;
                        }

                        actor->make_player_aware_of_me(det_mon_multiplier);
                }
        }

        return PropEnded::no;
}

bool CannotReadCurse::allow_read_absolute(const Verbose verbose) const
{
        if (actor::is_player(m_owner) && verbose == Verbose::yes) {
                msg_log::add("I cannot read it.");
        }

        return false;
}

PropActResult FrenziesSelf::on_act()
{
        if (!m_owner->is_alive()) {
                return {};
        }

        if (m_cooldown > 0) {
                --m_cooldown;

                return {};
        }

        if (!m_owner->m_ai_state.target) {
                return {};
        }

        const bool is_low_hp = (m_owner->m_hp <= (actor::max_hp(*m_owner) / 2));

        if (!is_low_hp) {
                return {};
        }

        m_cooldown = 30;

        Prop* prop = prop::make(prop::Id::frenzied);

        prop->set_duration(rnd::range(6, 8));

        m_owner->m_properties.apply(prop);

        return {};
}

PropActResult FrenziesFollowers::on_act()
{
        if (!m_owner->is_alive()) {
                return {};
        }

        // Don't frenzy followers if owning actor will join the player anyway
        // (it can look bad if they frenzy, and then immediately join the
        // player).
        if (m_owner->m_properties.has(prop::Id::allies_ghoul_player) &&
            player_bon::is_bg(Bg::ghoul)) {
        }

        if (m_cooldown > 0) {
                --m_cooldown;

                return {};
        }

        if (!m_owner->m_ai_state.target) {
                return {};
        }

        m_cooldown = 30;

        std::vector<actor::Actor*> actors_to_frenzy;

        std::copy_if(
                std::begin(game_time::g_actors),
                std::end(game_time::g_actors),
                std::back_insert_iterator(actors_to_frenzy),
                [this](const actor::Actor* actor) { return m_owner->is_leader_of(actor); });

        if (actors_to_frenzy.empty()) {
                return {};
        }

        // Also frenzy the owning actor, but only if there are other actors as
        // well.
        actors_to_frenzy.push_back(m_owner);

        Snd snd(
                "A voice is stirring up a great frenzy!",
                audio::SfxId::END,
                IgnoreMsgIfOriginSeen::yes,
                m_owner->m_pos,
                m_owner,
                SndVol::high,
                AlertsMon::no);

        snd.run();

        if (actor::can_player_see_actor(*m_owner)) {
                const std::string name = text_format::first_to_upper(m_owner->name_the());

                msg_log::add(name + " stirs up a great frenzy!");
        }

        const int duration = rnd::range(10, 30);

        for (actor::Actor* const actor : actors_to_frenzy) {
                Prop* prop = prop::make(prop::Id::frenzied);

                prop->set_duration(duration);

                actor->m_properties.apply(prop);
        }

        game_time::tick();

        PropActResult result;

        result.did_action = DidAction::yes;
        result.prop_ended = PropEnded::no;

        return result;
}

PropActResult SummonsLocusts::on_act()
{
        if (!m_owner->is_alive() ||
            m_has_summoned ||
            !m_owner->is_aware_of_player()) {
                return {};
        }

        Array2<bool> blocked(map::dims());

        const R fov_rect = fov::fov_rect(m_owner->m_pos, blocked.dims());

        map_parsers::BlocksLos()
                .run(blocked,
                     fov_rect,
                     MapParseMode::overwrite);

        if (!actor::can_mon_see_actor(*m_owner, *map::g_player, blocked)) {
                return {};
        }

        if (actor::can_player_see_actor(*m_owner)) {
                const std::string name = text_format::first_to_upper(m_owner->name_the());

                msg_log::add(name + " calls a plague of Locusts!");

                map::g_player->incr_shock(12.0, ShockSrc::misc);
        }

        actor::Actor* const leader_of_spawned_mon =
                m_owner->m_leader
                ? m_owner->m_leader
                : m_owner;

        const size_t nr_of_spawns = 15;

        actor::MonSpawnResult summoned =
                actor::spawn(
                        m_owner->m_pos,
                        {nr_of_spawns, "MON_LOCUST"},
                        map::rect());

        summoned.set_leader(leader_of_spawned_mon);
        summoned.make_aware_of_player();

        std::for_each(
                std::begin(summoned.monsters),
                std::end(summoned.monsters),
                [](auto* const actor) {
                        Prop* prop = prop::make(prop::Id::summoned);

                        prop->set_indefinite();

                        actor->m_properties.apply(prop);
                });

        map::update_vision();

        m_has_summoned = true;

        game_time::tick();

        PropActResult result;

        result.did_action = DidAction::yes;
        result.prop_ended = PropEnded::no;

        return result;
}

SpectralWpn::SpectralWpn() :
        Prop(prop::Id::spectral_wpn) {}

void SpectralWpn::on_death()
{
        // Remove the item from the inventory to avoid dropping it on the floor
        // (but do not yet delete the item, in case it's still being used in the
        // the call stack)
        item::Item* const item =
                m_owner->m_inv.remove_item_in_slot(
                        SlotId::wpn,
                        false);  // Do not delete the item

        m_discarded_item.reset(item);
}

std::string SpectralWpn::get_weapon_name() const
{
        item::Item* item = m_owner->m_inv.item_in_slot(SlotId::wpn);

        ASSERT(item);

        std::string name =
                item->name(
                        ItemNameType::plain,
                        ItemNameInfo::yes,
                        ItemNameAttackInfo::none);

        // HACK: Remove all characters from the first comma. This is intended to
        // give unique weapons a more sensible name - e.g. "Spectral Gahana",
        // instead of "Spectral Gahana, The Black Dagger".
        const size_t comma_pos = name.find_first_of(',');

        if (comma_pos != std::string::npos) {
                name.erase(comma_pos, name.size());
        }

        return name;
}

std::optional<std::string> SpectralWpn::override_actor_name_the() const
{
        return "The Spectral " + get_weapon_name();
}

std::optional<std::string> SpectralWpn::override_actor_name_a() const
{
        return "A Spectral " + get_weapon_name();
}

std::optional<char> SpectralWpn::override_actor_character() const
{
        item::Item* item = m_owner->m_inv.item_in_slot(SlotId::wpn);

        ASSERT(item);

        return item->character();
}

std::optional<gfx::TileId> SpectralWpn::override_actor_tile() const
{
        item::Item* item = m_owner->m_inv.item_in_slot(SlotId::wpn);

        ASSERT(item);

        return item->tile();
}

std::optional<std::string> SpectralWpn::override_actor_descr() const
{
        item::Item* item = m_owner->m_inv.item_in_slot(SlotId::wpn);

        ASSERT(item);

        std::string str =
                item->name(
                        ItemNameType::a,
                        ItemNameInfo::yes,
                        ItemNameAttackInfo::none);

        str = text_format::first_to_upper(str);

        str += ", floating through the air as if wielded by an invisible hand.";

        return str;
}

void Thorns::save() const
{
        saving::put_int(m_dmg);
}

void Thorns::load()
{
        m_dmg = saving::get_int();
}

PropEnded Thorns::on_hit(
        const int dmg,
        const DmgType dmg_type,
        actor::Actor* const attacker)
{
        (void)dmg;
        (void)dmg_type;

        if (!attacker) {
                return PropEnded::no;
        }

        if (!m_owner->is_alive()) {
                return PropEnded::no;
        }

        hit_actor(*attacker);

        return PropEnded::no;
}

void Thorns::hit_actor(actor::Actor& target)
{
        const bool player_see_retaliator =
                actor::can_player_see_actor(*m_owner);

        const bool player_see_target =
                actor::can_player_see_actor(target);

        const bool player_see_target_pos =
                map::g_seen.at(target.m_pos);

        const bool player_see_retaliator_pos =
                map::g_seen.at(m_owner->m_pos);

        const bool should_print_msg =
                player_see_retaliator ||
                player_see_target ||
                player_see_retaliator_pos ||
                player_see_target_pos;

        if (should_print_msg) {
                draw_blast_at_cells({target.m_pos}, colors::red());

                if (actor::is_player(&target)) {
                        // Target is player.
                        print_msg_mon_retaliate_player();
                }
                else {
                        // Target is monster.
                        if (actor::is_player(m_owner)) {
                                // Player retaliating on monster.
                                print_msg_player_retaliate_mon(target);
                        }
                        else {
                                // Monster retaliating on monster.
                                print_msg_mon_retaliate_mon(target);
                        }
                }
        }

        // NOTE: Setting attacker to nullptr here to avoid an "infinite" loop
        // (or until one of them are dead), in case both creatures have the
        // thorns effect.
        actor::hit(target, m_dmg, DmgType::pure, nullptr);
}

void Thorns::print_msg_player_retaliate_mon(
        const actor::Actor& target) const
{
        const bool player_see_target =
                actor::can_player_see_actor(target);

        std::string target_name;

        if (player_see_target) {
                target_name = target.name_the();
        }
        else {
                target_name = "it";
        }

        const std::string msg = "I retaliate upon " + target_name + "!";

        msg_log::add(msg, colors::msg_good());
}

void Thorns::print_msg_mon_retaliate_player() const
{
        const bool player_see_retaliator =
                actor::can_player_see_actor(*m_owner);

        std::string retaliator_name;

        if (player_see_retaliator) {
                retaliator_name = m_owner->name_the();
        }
        else {
                retaliator_name = "it";
        }

        const std::string msg =
                "My attack upon " +
                retaliator_name +
                " is retaliated by a magic aura!";

        msg_log::add(msg, colors::msg_bad());
}

void Thorns::print_msg_mon_retaliate_mon(
        const actor::Actor& target) const
{
        const bool player_see_retaliator =
                actor::can_player_see_actor(*m_owner);

        const bool player_see_target =
                actor::can_player_see_actor(target);

        std::string retaliator_name;
        std::string target_name;

        if (player_see_retaliator) {
                retaliator_name =
                        text_format::first_to_upper(
                                m_owner->name_the());
        }
        else {
                retaliator_name = "It";
        }

        if (player_see_target) {
                target_name = target.name_the();
        }
        else {
                target_name = "it";
        }

        const std::string msg =
                retaliator_name +
                "retaliates upon " +
                target_name +
                " by a magic aura!";

        msg_log::add(msg);
}

PropEnded Sanctuary::on_moved_non_center_dir()
{
        m_owner->m_properties.end_prop(id());

        return PropEnded::yes;
}

void CrimsonPassage::save() const
{
        saving::put_int(m_nr_steps_allowed);
        saving::put_int(m_nr_steps_taken);
}

void CrimsonPassage::load()
{
        m_nr_steps_allowed = saving::get_int();
        m_nr_steps_taken = saving::get_int();
}

std::string CrimsonPassage::name_short() const
{
        std::string nr_str =
                (m_nr_steps_allowed >= 0)
                ? std::to_string(m_nr_steps_allowed - m_nr_steps_taken)
                : "INF";

        return "Crims Psg(" + nr_str + ")";
}

PropEnded CrimsonPassage::on_moved_non_center_dir()
{
        const int dmg = 2;

        if (m_owner->m_hp <= dmg) {
                m_owner->m_properties.end_prop(id());

                return PropEnded::yes;
        }

        actor::hit(*m_owner, dmg, DmgType::pure, nullptr, AllowWound::no);

        ++m_nr_steps_taken;

        if ((m_nr_steps_allowed != -1) &&
            m_nr_steps_taken >= m_nr_steps_allowed) {
                m_owner->m_properties.end_prop(id());

                return PropEnded::yes;
        }

        return PropEnded::no;
}

}  // namespace prop

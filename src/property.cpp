// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "property.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <ext/alloc_traits.h>
#include <iterator>
#include <ostream>

#include "actor.hpp"
#include "actor_data.hpp"
#include "actor_death.hpp"
#include "actor_factory.hpp"
#include "actor_hit.hpp"
#include "actor_mon.hpp"
#include "actor_player.hpp"
#include "actor_see.hpp"
#include "array2.hpp"
#include "audio_data.hpp"
#include "debug.hpp"
#include "direction.hpp"
#include "explosion.hpp"
#include "fov.hpp"
#include "game_time.hpp"
#include "global.hpp"
#include "hints.hpp"
#include "insanity.hpp"
#include "inventory.hpp"
#include "io.hpp"
#include "item.hpp"
#include "item_data.hpp"
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
#include "state.hpp"
#include "teleport.hpp"
#include "terrain.hpp"
#include "terrain_data.hpp"
#include "text_format.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static bool
is_player_aware_of_hostile_mon_at(const P& pos)
{
        const auto* const mon = map::first_actor_at_pos(pos);

        return (
                mon &&
                mon->is_player_aware_of_me() &&
                !map::g_player->is_leader_of(mon));
}

// -----------------------------------------------------------------------------
// Property base class
// -----------------------------------------------------------------------------
Prop::Prop(PropId id) :
        m_id(id),
        m_data(property_data::g_data[(size_t)id]),
        m_nr_turns_left(m_data.std_rnd_turns.roll()),
        m_nr_dlvls_left(m_data.std_rnd_dlvls.roll())
{
}

// -----------------------------------------------------------------------------
// Specific properties
// -----------------------------------------------------------------------------
void PropBlessed::on_applied()
{
        m_owner->m_properties.end_prop(
                PropId::cursed,
                PropEndConfig(
                        PropEndAllowCallEndHook::no,
                        PropEndAllowMsg::no,
                        PropEndAllowHistoricMsg::yes));

        bless_adjacent();
}

void PropBlessed::on_more(const Prop& new_prop)
{
        (void)new_prop;

        bless_adjacent();
}

int PropBlessed::ability_mod(const AbilityId ability) const
{
        switch (ability)
        {
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

void PropBlessed::bless_adjacent() const
{
        // "Bless" adjacent fountains
        const P& p = m_owner->m_pos;

        for (const P& d : dir_utils::g_dir_list_w_center)
        {
                const auto p_adj = p + d;

                auto* const terrain = map::g_terrain.at(p_adj);

                if (terrain->id() != terrain::Id::fountain)
                {
                        continue;
                }

                static_cast<terrain::Fountain*>(terrain)->bless();
        }
}

void PropCursed::on_applied()
{
        m_owner->m_properties.end_prop(
                PropId::blessed,
                PropEndConfig(
                        PropEndAllowCallEndHook::no,
                        PropEndAllowMsg::no,
                        PropEndAllowHistoricMsg::yes));

        curse_adjacent();
}

void PropCursed::on_more(const Prop& new_prop)
{
        (void)new_prop;

        curse_adjacent();
}

int PropCursed::ability_mod(const AbilityId ability) const
{
        switch (ability)
        {
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

void PropCursed::curse_adjacent() const
{
        // "Curse" adjacent fountains
        const P& p = m_owner->m_pos;

        for (const P& d : dir_utils::g_dir_list_w_center)
        {
                const auto p_adj = p + d;

                auto* const terrain = map::g_terrain.at(p_adj);

                if (terrain->id() != terrain::Id::fountain)
                {
                        continue;
                }

                static_cast<terrain::Fountain*>(terrain)->curse();
        }
}

PropEnded PropEntangled::on_actor_turn()
{
        // Handle drowning

        if (!m_owner->is_alive())
        {
                return PropEnded::no;
        }

        if (!m_owner->m_properties.has(PropId::swimming))
        {
                return PropEnded::no;
        }

        if (m_owner->is_player())
        {
                msg_log::add("I am drowning!", colors::msg_bad());
        }
        else if (actor::can_player_see_actor(*m_owner))
        {
                const auto name_the =
                        text_format::first_to_upper(
                                m_owner->name_the());

                msg_log::add(name_the + " is drowning.", colors::msg_good());
        }

        actor::hit(*m_owner, 1, DmgType::pure);

        return PropEnded::no;
}

void PropEntangled::on_applied()
{
        // TODO: Rather than doing this on the "on_applied" hook (which should
        // probably be reserved for when the property actually does get applied,
        // i.e. it shall not be removed), consider checking this elsewhere,
        // perhaps in a new function such as "is_resisting_self"
        try_player_end_with_machete();
}

PropEnded PropEntangled::affect_move_dir(Dir& dir)
{
        if (dir == Dir::center)
        {
                return PropEnded::no;
        }

        if (try_player_end_with_machete())
        {
                return PropEnded::yes;
        }

        dir = Dir::center;

        if (m_owner->is_player())
        {
                msg_log::add("I struggle to tear free!", colors::msg_bad());
        }
        else
        {
                // Is monster
                if (actor::can_player_see_actor(*m_owner))
                {
                        const std::string actor_name_the =
                                text_format::first_to_upper(
                                        m_owner->name_the());

                        msg_log::add(
                                actor_name_the + " struggles to tear free.",
                                colors::msg_good());
                }
        }

        if (rnd::one_in(8))
        {
                m_owner->m_properties.end_prop(id());

                return PropEnded::yes;
        }

        return PropEnded::no;
}

bool PropEntangled::try_player_end_with_machete()
{
        if (m_owner != map::g_player)
        {
                return false;
        }

        auto* item = m_owner->m_inv.item_in_slot(SlotId::wpn);

        if (item && (item->id() == item::Id::machete))
        {
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

void PropSlowed::on_applied()
{
        m_owner->m_properties.end_prop(
                PropId::hasted,
                PropEndConfig(
                        PropEndAllowCallEndHook::no,
                        PropEndAllowMsg::no,
                        PropEndAllowHistoricMsg::yes));
}

void PropHasted::on_applied()
{
        m_owner->m_properties.end_prop(
                PropId::slowed,
                PropEndConfig(
                        PropEndAllowCallEndHook::no,
                        PropEndAllowMsg::no,
                        PropEndAllowHistoricMsg::yes));
}

void PropClockworkHasted::on_applied()
{
        m_owner->m_properties.end_prop(
                PropId::slowed,
                PropEndConfig(
                        PropEndAllowCallEndHook::no,
                        PropEndAllowMsg::no,
                        PropEndAllowHistoricMsg::yes));
}

void PropSummoned::on_end()
{
        m_owner->m_state = ActorState::destroyed;

        actor::unset_actor_as_leader_for_all_mon(*m_owner);
}

PropEnded PropInfected::on_actor_turn()
{
#ifndef NDEBUG
        ASSERT(!m_owner->m_properties.has(PropId::diseased));
#endif  // NDEBUG

        if (m_owner->is_player())
        {
                if (map::g_player->m_active_medical_bag)
                {
                        ++m_nr_turns_left;

                        return PropEnded::no;
                }

                if ((m_nr_turns_left < 20) &&
                    !has_warned &&
                    rnd::coin_toss())
                {
                        msg_log::add(
                                "My infection is getting worse!",
                                colors::msg_note(),
                                MsgInterruptPlayer::no,
                                MorePromptOnMsg::yes);

                        has_warned = true;
                }
        }

        const bool apply_disease = (m_nr_turns_left <= 1);

        if (!apply_disease)
        {
                return PropEnded::no;
        }

        // Time to apply disease

        auto* const owner = m_owner;

        owner->m_properties.end_prop(
                id(),
                PropEndConfig(
                        PropEndAllowCallEndHook::no,
                        PropEndAllowMsg::no,
                        PropEndAllowHistoricMsg::yes));

        // NOTE: This property is now deleted

        auto* prop_diseased = new PropDiseased();

        prop_diseased->set_indefinite();

        owner->m_properties.apply(prop_diseased);

        msg_log::more_prompt();

        return PropEnded::yes;
}

void PropInfected::on_applied()
{
        if (m_owner->is_player())
        {
                hints::display(hints::infected);
        }
}

int PropDiseased::affect_max_hp(const int hp_max) const
{
        return hp_max / 2;
}

void PropDiseased::on_applied()
{
        // End infection
        m_owner->m_properties.end_prop(
                PropId::infected,
                PropEndConfig(
                        PropEndAllowCallEndHook::no,
                        PropEndAllowMsg::no,
                        PropEndAllowHistoricMsg::yes));
}

bool PropDiseased::is_resisting_other_prop(const PropId prop_id) const
{
#ifndef NDEBUG
        ASSERT(!m_owner->m_properties.has(PropId::infected));
#endif  // NDEBUG

        // Getting infected while already diseased is just annoying
        return prop_id == PropId::infected;
}

PropEnded PropDescend::on_actor_turn()
{
        ASSERT(m_owner->is_player());

        if (m_nr_turns_left <= 1)
        {
                game_time::g_is_magic_descend_nxt_std_turn = true;
        }

        return PropEnded::no;
}

void PropZuulPossessPriest::on_placed()
{
        // If the number left allowed to spawn is zero now, this means that Zuul
        // has been spawned for the first time - hide Zuul inside a priest.
        // Otherwise we do nothing, and just spawn Zuul. When the possessed
        // actor is killed, Zuul will be allowed to spawn infinitely (this is
        // handled elsewhere).

        const int nr_left_allowed = m_owner->m_data->nr_left_allowed_to_spawn;

        ASSERT(nr_left_allowed <= 0);

        const bool should_possess = (nr_left_allowed >= 0);

        if (should_possess)
        {
                m_owner->m_state = ActorState::destroyed;

                auto* actor =
                        actor::make(
                                actor::Id::cultist_priest,
                                m_owner->m_pos);

                auto* prop = new PropPossessedByZuul();

                prop->set_indefinite();

                actor->m_properties.apply(
                        prop,
                        PropSrc::intr,
                        true,
                        Verbose::no);

                actor->restore_hp(999, false, Verbose::no);
        }
}

void PropPossessedByZuul::on_death()
{
        // An actor possessed by Zuul has died - release Zuul!

        if (actor::can_player_see_actor(*m_owner))
        {
                const std::string& name1 =
                        text_format::first_to_upper(
                                m_owner->name_the());

                const std::string& name2 =
                        actor::g_data[(size_t)actor::Id::zuul].name_the;

                msg_log::add(name1 + " was possessed by " + name2 + "!");
        }

        m_owner->m_state = ActorState::destroyed;

        const auto& pos = m_owner->m_pos;

        map::make_gore(pos);

        map::make_blood(pos);

        // Zuul is now free, allow it to spawn infinitely
        actor::g_data[(size_t)actor::Id::zuul].nr_left_allowed_to_spawn = -1;

        actor::spawn(
                pos,
                {actor::Id::zuul},
                map::rect())
                .make_aware_of_player();
}

void PropShapeshifts::on_placed()
{
        // NOTE: This function will only ever run for the original shapeshifter
        // monster, never for the monsters shapeshifted into.

        set_indefinite();

        // The shapeshifter should change into something else asap.
        m_countdown = 0;
}

void PropShapeshifts::on_std_turn()
{
        if (!m_owner->is_alive())
        {
                return;
        }

        if (!m_owner->m_properties.allow_act())
        {
                return;
        }

        --m_countdown;

        if (m_countdown <= 0)
        {
                shapeshift(Verbose::yes);
        }
}

void PropShapeshifts::on_death()
{
        m_owner->m_state = ActorState::destroyed;

        const auto spawned =
                actor::spawn(
                        m_owner->m_pos,
                        {actor::Id::shapeshifter},
                        map::rect());

        ASSERT(!spawned.monsters.empty());

        if (!spawned.monsters.empty())
        {
                auto* const shapeshifter = spawned.monsters[0];

                shapeshifter->m_mon_aware_state.aware_counter =
                        m_owner->m_mon_aware_state.aware_counter;

                shapeshifter->m_mon_aware_state.wary_counter =
                        m_owner->m_mon_aware_state.wary_counter;

                // No more shapeshifting
                shapeshifter->m_properties.end_prop(PropId::shapeshifts);

                // It's affraid!
                shapeshifter->m_properties.apply(
                        property_factory::make(PropId::terrified));

                // Make the Shapeshifter skip a turn - it looks better if it
                // doesn't immediately move to an adjacent cell when spawning
                auto* const waiting = property_factory::make(PropId::waiting);

                waiting->set_duration(1);

                shapeshifter->m_properties.apply(waiting);
        }
}

void PropShapeshifts::shapeshift(const Verbose verbose) const
{
        std::vector<actor::Id> mon_id_bucket;

        for (const auto& d : actor::g_data)
        {
                const Range allowed_mon_depth_range(
                        d.spawn_min_dlvl - 2,
                        d.spawn_max_dlvl);

                if (!d.can_be_shapeshifted_into ||
                    (d.id == m_owner->id()) ||
                    (d.id == actor::Id::shapeshifter) ||
                    !d.is_auto_spawn_allowed ||
                    d.is_unique ||
                    !allowed_mon_depth_range.is_in_range(map::g_dlvl))
                {
                        continue;
                }

                mon_id_bucket.push_back(d.id);
        }

        if (mon_id_bucket.empty())
        {
                ASSERT(false);

                return;
        }

        if ((verbose == Verbose::yes) &&
            actor::can_player_see_actor(*m_owner))
        {
                msg_log::add("It changes shape!");

                io::draw_blast_at_cells({m_owner->m_pos}, colors::yellow());
        }

        m_owner->m_state = ActorState::destroyed;

        const auto spawned =
                actor::spawn(
                        m_owner->m_pos,
                        {rnd::element(mon_id_bucket)},
                        map::rect());

        if (spawned.monsters.size() != 1)
        {
                ASSERT(false);

                return;
        }

        auto* const mon = spawned.monsters[0];

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
                        static_cast<PropShapeshifts*>(
                                property_factory::make(PropId::shapeshifts));

                shapeshifts->set_indefinite();

                shapeshifts->m_countdown = rnd::range(3, 5);

                mon->m_properties.apply(shapeshifts);
        }
}

PropEnded PropPoisoned::on_actor_turn()
{
        if (!m_owner->is_alive())
        {
                return PropEnded::no;
        }

        // NOTE: Monsters have a shorter, more intense poisoning

        if (!m_owner->is_player() && (m_nr_turns_left > 1))
        {
                --m_nr_turns_left;
        }

        const auto owner_hp = m_owner->m_hp;

        int dmg = 0;
        int pct_chance = 0;

        if (m_owner->is_player())
        {
                dmg = 1;
                pct_chance = (int)std::pow((double)owner_hp, 1.5);
        }
        else
        {
                dmg = 2;
                pct_chance = (int)std::pow((double)owner_hp, 2.0);
        }

        if (owner_hp <= dmg)
        {
                return PropEnded::no;
        }

        if (!rnd::percent(pct_chance))
        {
                return PropEnded::no;
        }

        if (m_owner->is_player())
        {
                msg_log::add(
                        "I am suffering from the poison!",
                        colors::msg_bad(),
                        MsgInterruptPlayer::yes);
        }
        else if (actor::can_player_see_actor(*m_owner))
        {
                // Is seen monster
                const std::string actor_name_the =
                        text_format::first_to_upper(
                                m_owner->name_the());

                msg_log::add(
                        actor_name_the +
                        " suffers from poisoning!");
        }

        actor::hit(*m_owner, dmg, DmgType::pure);

        return PropEnded::no;
}

void PropAiming::on_hit()
{
        m_owner->m_properties.end_prop(id());
}

bool PropTerrified::allow_attack_melee(const Verbose verbose) const
{
        if (m_owner->is_player() && verbose == Verbose::yes)
        {
                msg_log::add("I am too terrified to engage in close combat!");
        }

        return false;
}

bool PropTerrified::allow_attack_ranged(const Verbose verbose) const
{
        (void)verbose;
        return true;
}

void PropTerrified::on_applied()
{
        // If this is a monster, we reset its last direction moved. Otherwise it
        // would probably tend to move toward the player even while terrified
        // (the AI would typically use the idle movement algorithm, which
        // favors stepping in the same direction as the last move).

        if (!m_owner->is_player())
        {
                m_owner->m_ai_state.last_dir_moved = Dir::center;
        }
}

PropEnded PropNailed::affect_move_dir(Dir& dir)
{
        if (dir == Dir::center)
        {
                return PropEnded::no;
        }

        // Allow the player to melee attack adjacent known monsters
        if (m_owner->is_player())
        {
                const auto intended_target =
                        m_owner->m_pos +
                        dir_utils::offset(dir);

                if (is_player_aware_of_hostile_mon_at(intended_target))
                {
                        return PropEnded::no;
                }
        }

        dir = Dir::center;

        if (m_owner->is_player())
        {
                msg_log::add(
                        "I struggle to tear out the spike!",
                        colors::msg_bad());
        }
        else
        {
                // Is monster
                if (actor::can_player_see_actor(*m_owner))
                {
                        const std::string actor_name_the =
                                text_format::first_to_upper(
                                        m_owner->name_the());

                        msg_log::add(
                                actor_name_the + " struggles in pain!",
                                colors::msg_good());
                }
        }

        actor::hit(*m_owner, rnd::range(1, 3), DmgType::pure);

        if (!m_owner->is_alive() || !rnd::one_in(4))
        {
                return PropEnded::no;
        }

        --m_nr_spikes;

        if (m_nr_spikes > 0)
        {
                if (m_owner->is_player())
                {
                        msg_log::add("I rip out a spike from my flesh!");
                }
                else if (actor::can_player_see_actor(*m_owner))
                {
                        const std::string actor_name_the =
                                text_format::first_to_upper(
                                        m_owner->name_the());

                        msg_log::add(actor_name_the + " tears out a spike!");
                }
        }

        return PropEnded::no;
}

int PropNailed::ability_mod(const AbilityId ability) const
{
        switch (ability)
        {
        case AbilityId::ranged:
                return -10;

        case AbilityId::melee:
                return -20;

        default:
                return 0;
        }
}

void PropWound::save() const
{
        saving::put_int(m_nr_wounds);
}

void PropWound::load()
{
        m_nr_wounds = saving::get_int();
}

void Prop::set_duration(const int nr_turns)
{
        ASSERT(nr_turns > 0);

        m_duration_mode = PropDurationMode::specific;

        m_nr_turns_left = nr_turns;
}

int PropWound::ability_mod(const AbilityId ability) const
{
        // A player with Survivalist receives no ability penalties
        if (m_owner->is_player() &&
            player_bon::has_trait(Trait::survivalist))
        {
                return 0;
        }

        if (ability == AbilityId::melee)
        {
                return (m_nr_wounds * -5);
        }
        else if (ability == AbilityId::dodging)
        {
                return (m_nr_wounds * -5);
        }

        return 0;
}

int PropWound::affect_max_hp(const int hp_max) const
{
        const int pen_pct_per_wound = 10;

        int hp_pen_pct = m_nr_wounds * pen_pct_per_wound;

        // The HP penalty is halved for a player with Survivalist
        if (m_owner->is_player() &&
            player_bon::has_trait(Trait::survivalist))
        {
                hp_pen_pct /= 2;
        }

        // Cap the penalty percentage
        hp_pen_pct = std::min(70, hp_pen_pct);

        return (hp_max * (100 - hp_pen_pct)) / 100;
}

void PropWound::heal_one_wound()
{
        ASSERT(m_nr_wounds > 0);

        --m_nr_wounds;

        if (m_nr_wounds > 0)
        {
                msg_log::add("A wound is healed.");
        }
        else
        {
                // This was the last wound

                // End self
                m_owner->m_properties.end_prop(id());
        }
}

void PropWound::on_more(const Prop& new_prop)
{
        (void)new_prop;

        ++m_nr_wounds;

        if (m_nr_wounds >= 5)
        {
                if (m_owner == map::g_player)
                {
                        msg_log::add("I die from my wounds!");
                }

                actor::kill(
                        *m_owner,
                        IsDestroyed::no,
                        AllowGore::no,
                        AllowDropItems::yes);
        }
}

PropHpSap::PropHpSap() :
        Prop(PropId::hp_sap),
        m_nr_drained(rnd::range(1, 3)) {}

void PropHpSap::save() const
{
        saving::put_int(m_nr_drained);
}

void PropHpSap::load()
{
        m_nr_drained = saving::get_int();
}

int PropHpSap::affect_max_hp(const int hp_max) const
{
        return (hp_max - m_nr_drained);
}

void PropHpSap::on_more(const Prop& new_prop)
{
        m_nr_drained +=
                static_cast<const PropHpSap*>(&new_prop)
                        ->m_nr_drained;
}

PropSpiSap::PropSpiSap() :
        Prop(PropId::spi_sap),
        m_nr_drained(1) {}

void PropSpiSap::save() const
{
        saving::put_int(m_nr_drained);
}

void PropSpiSap::load()
{
        m_nr_drained = saving::get_int();
}

int PropSpiSap::affect_max_spi(const int spi_max) const
{
        return (spi_max - m_nr_drained);
}

void PropSpiSap::on_more(const Prop& new_prop)
{
        m_nr_drained +=
                static_cast<const PropSpiSap*>(&new_prop)
                        ->m_nr_drained;
}

PropMindSap::PropMindSap() :
        Prop(PropId::mind_sap),
        m_nr_drained(rnd::range(1, 3)) {}

void PropMindSap::save() const
{
        saving::put_int(m_nr_drained);
}

void PropMindSap::load()
{
        m_nr_drained = saving::get_int();
}

int PropMindSap::affect_shock(const int shock) const
{
        return (shock + m_nr_drained);
}

void PropMindSap::on_more(const Prop& new_prop)
{
        m_nr_drained +=
                static_cast<const PropMindSap*>(&new_prop)
                        ->m_nr_drained;
}

bool PropConfused::allow_read_absolute(const Verbose verbose) const
{
        if (m_owner->is_player() && verbose == Verbose::yes)
        {
                msg_log::add("I am too confused to read.");
        }

        return false;
}

bool PropConfused::allow_cast_intr_spell_absolute(
        const Verbose verbose) const
{
        if (m_owner->is_player() && (verbose == Verbose::yes))
        {
                msg_log::add("I am too confused to concentrate!");
        }

        return false;
}

bool PropConfused::allow_pray(Verbose verbose) const
{
        if (m_owner->is_player() && (verbose == Verbose::yes))
        {
                msg_log::add("I am too confused to concentrate!");
        }

        return false;
}

bool PropConfused::allow_attack_melee(const Verbose verbose) const
{
        (void)verbose;

        if (m_owner != map::g_player)
        {
                return rnd::coin_toss();
        }

        return true;
}

bool PropConfused::allow_attack_ranged(const Verbose verbose) const
{
        (void)verbose;

        if (m_owner != map::g_player)
        {
                return rnd::coin_toss();
        }

        return true;
}

PropEnded PropConfused::affect_move_dir(Dir& dir)
{
        if (dir == Dir::center)
        {
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

        if (!rnd::one_in(8))
        {
                return PropEnded::no;
        }

        std::vector<P> d_bucket;

        for (const auto& d : dir_utils::g_dir_list)
        {
                const auto tgt_p = m_owner->m_pos + d;

                if (!blocked.at(tgt_p))
                {
                        d_bucket.push_back(d);
                }
        }

        if (!d_bucket.empty())
        {
                const auto& d = rnd::element(d_bucket);

                dir = dir_utils::dir(d);
        }

        return PropEnded::no;
}

void PropHallucinating::on_applied()
{
        apply_fake_actor_data();

        create_fake_stairs();
}

void PropHallucinating::on_std_turn()
{
        if (rnd::one_in(10))
        {
                apply_fake_actor_data();
        }

        if (rnd::one_in(250))
        {
                create_fake_stairs();
        }
}

void PropHallucinating::on_end()
{
        clear_fake_actor_data();

        clear_all_fake_stairs();
}

void PropHallucinating::apply_fake_actor_data() const
{
        const auto allowed_data = get_allowed_fake_mon_data();

        if (allowed_data.empty())
        {
                ASSERT(false);

                return;
        }

        for (auto* const actor : game_time::g_actors)
        {
                if (!actor->is_player())
                {
                        actor->m_mimic_data = rnd::element(allowed_data);
                }
        }
}

void PropHallucinating::clear_fake_actor_data() const
{
        for (auto* const actor : game_time::g_actors)
        {
                if (!actor->is_player())
                {
                        actor->m_mimic_data = nullptr;
                }
        }
}

std::vector<const actor::ActorData*>
PropHallucinating::get_allowed_fake_mon_data() const
{
        std::vector<const actor::ActorData*> result;

        result.reserve((size_t)actor::Id::END);

        for (const auto& d : actor::g_data)
        {
                if ((d.id == actor::Id::player) ||
                    (d.id == actor::Id::spectral_wpn) ||
                    (d.id == actor::Id::strange_color) ||
                    (d.id == actor::Id::cultist))
                {
                        continue;
                }

                const bool gives_xp = (d.mon_shock_lvl > ShockLvl::none);

                if (gives_xp && !d.has_player_seen)
                {
                        continue;
                }

                result.push_back(&d);
        }

        return result;
}

void PropHallucinating::create_fake_stairs() const
{
        const bool was_valid_before = mapgen::g_is_map_valid;

        const auto p = mapgen::make_stairs_at_random_pos();

        // It's OK to fail placing the stairs, don't let this invalidate the map
        // (although map validity probably doesn't matter at all at this point).
        mapgen::g_is_map_valid = was_valid_before;

        if ((p.x > 0) && (p.y > 0))
        {
                auto* const terrain = map::g_terrain.at(p);

                if (terrain->id() != terrain::Id::stairs)
                {
                        ASSERT(false);

                        return;
                }

                auto* const stairs = static_cast<terrain::Stairs*>(terrain);

                stairs->set_fake();
        }
}

void PropHallucinating::clear_all_fake_stairs() const
{
        for (auto* const terrain : map::g_terrain)
        {
                if (terrain->id() != terrain::Id::stairs)
                {
                        continue;
                }

                // Is stairs

                const auto* const stairs =
                        static_cast<const terrain::Stairs*>(terrain);

                if (!stairs->is_fake())
                {
                        continue;
                }

                // Is fake stairs

                map::put(new terrain::RubbleLow(terrain->pos()));
        }
}

void PropAstralOpiumAddict::save() const
{
        saving::put_int(m_shock_lvl);
        saving::put_int(m_nr_dlvls_to_penalty);
        saving::put_int(m_nr_turns_to_penalty);
}

void PropAstralOpiumAddict::load()
{
        m_shock_lvl = saving::get_int();
        m_nr_dlvls_to_penalty = saving::get_int();
        m_nr_turns_to_penalty = saving::get_int();
}

std::string PropAstralOpiumAddict::name_short() const
{
        std::string str = m_data.name_short;

        if (is_active())
        {
                str += "(" + std::to_string(m_shock_lvl) + "%)";
        }

        return str;
}

void PropAstralOpiumAddict::on_applied()
{
        reset_penalty_countdown();

        m_shock_lvl = 10;
}

void PropAstralOpiumAddict::on_std_turn()
{
        if ((m_nr_dlvls_to_penalty == 0) && (m_nr_turns_to_penalty > 0))
        {
                --m_nr_turns_to_penalty;

                if (m_nr_turns_to_penalty == 0)
                {
                        msg_log::add(
                                "I crave Astral Opium!!",
                                colors::msg_note(),
                                MsgInterruptPlayer::no,
                                MorePromptOnMsg::yes);
                }
        }
}

void PropAstralOpiumAddict::on_new_dlvl()
{
        if (m_nr_dlvls_to_penalty > 0)
        {
                --m_nr_dlvls_to_penalty;
        }
}

void PropAstralOpiumAddict::on_more(const Prop& new_prop)
{
        (void)new_prop;

        msg_log::add("I need more!!");

        reset_penalty_countdown();

        m_shock_lvl += 5;
}

void PropAstralOpiumAddict::reset_penalty_countdown()
{
        m_nr_dlvls_to_penalty = rnd::range(0, 1);
        m_nr_turns_to_penalty = rnd::range(50, 75);
}

bool PropAstralOpiumAddict::is_active() const
{
        return ((m_nr_dlvls_to_penalty == 0) && (m_nr_turns_to_penalty == 0));
}

int PropAstralOpiumAddict::affect_shock(const int shock) const
{
        int result = shock;

        if (is_active())
        {
                result += m_shock_lvl;
        }

        return result;
}

bool PropFrenzied::allow_move_dir(const Dir dir)
{
        if (!m_owner->is_player() || (dir == Dir::center))
        {
                return true;
        }

        const auto seen_foes = actor::seen_foes(*m_owner);

        if (seen_foes.empty())
        {
                return true;
        }

        Array2<bool> blocked(map::dims());

        map_parsers::BlocksActor(*m_owner, ParseActors::no)
                .run(blocked, blocked.rect());

        // Mark the positions of all seen actors as free (the monsters may be
        // inside wall cells, e.g. worms crawling through rubble).
        for (const auto* const actor : seen_foes)
        {
                blocked.at(actor->m_pos) = false;
        }

        std::vector<const actor::Actor*> seen_reachable_foes;
        seen_reachable_foes.reserve(seen_foes.size());

        for (const auto* const actor : seen_foes)
        {
                const auto line =
                        line_calc::calc_new_line(
                                m_owner->m_pos,
                                actor->m_pos,
                                true,
                                999,
                                false);

                if (line.empty())
                {
                        continue;
                }

                const auto is_blocked =
                        std::any_of(
                                std::begin(line),
                                std::end(line),
                                [&blocked](const P& p) {
                                        return blocked.at(p);
                                });

                if (is_blocked)
                {
                        continue;
                }

                seen_reachable_foes.push_back(actor);
        }

        if (seen_reachable_foes.empty())
        {
                return true;
        }

        // There are seen reachable seen foes

        const auto new_pos = m_owner->m_pos + dir_utils::offset(dir);

        for (const auto* actor : seen_reachable_foes)
        {
                const int old_dist = king_dist(m_owner->m_pos, actor->m_pos);
                const int new_dist = king_dist(new_pos, actor->m_pos);

                if (new_dist < old_dist)
                {
                        // This step would take the player closer to at least
                        // one reachable seen foe - allow the move.
                        return true;
                }
        }

        msg_log::add("I will not step away!");

        return false;
}

bool PropFrenzied::is_resisting_other_prop(const PropId prop_id) const
{
        return prop_id == PropId::confused ||
                prop_id == PropId::fainted ||
                prop_id == PropId::terrified ||
                prop_id == PropId::weakened;
}

void PropFrenzied::on_applied()
{
        const PropEndConfig prop_end_config(
                PropEndAllowCallEndHook::no,
                PropEndAllowMsg::no,
                PropEndAllowHistoricMsg::yes);

        m_owner->m_properties.end_prop(PropId::confused, prop_end_config);
        m_owner->m_properties.end_prop(PropId::fainted, prop_end_config);
        m_owner->m_properties.end_prop(PropId::terrified, prop_end_config);
        m_owner->m_properties.end_prop(PropId::weakened, prop_end_config);
}

void PropFrenzied::on_end()
{
        // Only the player (except for Ghoul background) gets tired after a
        // frenzy (it looks weird for monsters)
        if (m_owner->is_player() && (player_bon::bg() != Bg::ghoul))
        {
                // TODO: Base the weakened duration on the number of turns
                // frenzied was active (m_nr_turns_active) (within some upper
                // and lower bounds)?
                m_owner->m_properties.apply(
                        property_factory::make(PropId::weakened));
        }
}

bool PropFrenzied::allow_read_absolute(const Verbose verbose) const
{
        if (m_owner->is_player() && verbose == Verbose::yes)
        {
                msg_log::add("I am too enraged to read!");
        }

        return false;
}

bool PropFrenzied::allow_cast_intr_spell_absolute(
        const Verbose verbose) const
{
        if (m_owner->is_player() && (verbose == Verbose::yes))
        {
                msg_log::add("I am too enraged to concentrate!");
        }

        return false;
}

bool PropFrenzied::allow_pray(Verbose verbose) const
{
        if (m_owner->is_player() && (verbose == Verbose::yes))
        {
                msg_log::add("I am too enraged to concentrate!");
        }

        return false;
}

PropEnded PropBurning::on_actor_turn()
{
        if (m_owner->is_player())
        {
                msg_log::add("AAAARGH IT BURNS!!!", colors::light_red());
        }

        actor::hit(*m_owner, rnd::range(1, 3), DmgType::fire);

        return PropEnded::no;
}

bool PropBurning::allow_read_chance(const Verbose verbose) const
{
        if (!rnd::coin_toss())
        {
                if (m_owner->is_player() && (verbose == Verbose::yes))
                {
                        msg_log::add("I fail to concentrate!");
                }

                return false;
        }

        return true;
}

bool PropBurning::allow_cast_intr_spell_chance(const Verbose verbose) const
{
        if (!rnd::coin_toss())
        {
                if (m_owner->is_player() &&
                    (verbose == Verbose::yes))
                {
                        msg_log::add("I fail to concentrate!");
                }

                return false;
        }

        return true;
}

bool PropBurning::allow_pray(Verbose verbose) const
{
        if (m_owner->is_player() && (verbose == Verbose::yes))
        {
                msg_log::add("I am burning!");
        }

        return false;
}

bool PropBurning::allow_attack_ranged(const Verbose verbose) const
{
        if (m_owner->is_player() && (verbose == Verbose::yes))
        {
                msg_log::add("Not while burning.");
        }

        return false;
}

PropActResult PropRecloaks::on_act()
{
        if (m_owner->is_alive() &&
            !m_owner->m_properties.has(PropId::cloaked) &&
            rnd::one_in(8))
        {
                auto* prop_cloaked = property_factory::make(PropId::cloaked);

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

bool PropBlind::allow_read_absolute(const Verbose verbose) const
{
        if (m_owner->is_player() && (verbose == Verbose::yes))
        {
                msg_log::add("I cannot read while blind.");
        }

        return false;
}

bool PropBlind::should_update_vision_on_toggled() const
{
        return m_owner->is_player();
}

PropEnded PropParalyzed::on_actor_turn()
{
        // Handle drowning

        if (!m_owner->is_alive())
        {
                return PropEnded::no;
        }

        if (!m_owner->m_properties.has(PropId::swimming))
        {
                return PropEnded::no;
        }

        if (m_owner->is_player())
        {
                msg_log::add("I am drowning!", colors::msg_bad());
        }
        else if (actor::can_player_see_actor(*m_owner))
        {
                const auto name_the =
                        text_format::first_to_upper(
                                m_owner->name_the());

                msg_log::add(name_the + " is drowning.", colors::msg_good());
        }

        actor::hit(*m_owner, 1, DmgType::pure);

        return PropEnded::no;
}

void PropParalyzed::on_applied()
{
        auto* const player = map::g_player;

        if (m_owner->is_player())
        {
                auto* const active_explosive = player->m_active_explosive;

                if (active_explosive)
                {
                        active_explosive->on_player_paralyzed();
                }
        }
}

bool PropFainted::should_update_vision_on_toggled() const
{
        return m_owner->is_player();
}

PropEnded PropFlared::on_actor_turn()
{
        actor::hit(*m_owner, 1, DmgType::fire);

        if (m_nr_turns_left <= 1)
        {
                m_owner->m_properties.apply(new PropBurning());

                m_owner->m_properties.end_prop(id());

                return PropEnded::yes;
        }

        return PropEnded::no;
}

DmgResistData PropRAcid::is_resisting_dmg(const DmgType dmg_type) const
{
        DmgResistData d;

        d.is_resisted = (dmg_type == DmgType::acid);

        d.msg_resist_player = "I feel a faint burning sensation.";

        d.msg_resist_mon = "seems unaffected.";

        return d;
}

DmgResistData PropRElec::is_resisting_dmg(const DmgType dmg_type) const
{
        DmgResistData d;

        d.is_resisted = (dmg_type == DmgType::electric);

        d.msg_resist_player = "I feel a faint tingle.";

        d.msg_resist_mon = "seems unaffected.";

        return d;
}

bool PropRConf::is_resisting_other_prop(const PropId prop_id) const
{
        return prop_id == PropId::confused;
}

void PropRConf::on_applied()
{
        m_owner->m_properties.end_prop(
                PropId::confused,
                PropEndConfig(
                        PropEndAllowCallEndHook::no,
                        PropEndAllowMsg::no,
                        PropEndAllowHistoricMsg::yes));
}

bool PropRFear::is_resisting_other_prop(const PropId prop_id) const
{
        return prop_id == PropId::terrified;
}

void PropRFear::on_applied()
{
        m_owner->m_properties.end_prop(
                PropId::terrified,
                PropEndConfig(
                        PropEndAllowCallEndHook::no,
                        PropEndAllowMsg::no,
                        PropEndAllowHistoricMsg::yes));

        if (m_owner->is_player() &&
            m_duration_mode == PropDurationMode::indefinite)
        {
                insanity::on_permanent_rfear();
        }
}

bool PropRSlow::is_resisting_other_prop(const PropId prop_id) const
{
        return prop_id == PropId::slowed;
}

void PropRSlow::on_applied()
{
        m_owner->m_properties.end_prop(
                PropId::slowed,
                PropEndConfig(
                        PropEndAllowCallEndHook::no,
                        PropEndAllowMsg::no,
                        PropEndAllowHistoricMsg::yes));
}

bool PropRPhys::is_resisting_other_prop(const PropId prop_id) const
{
        (void)prop_id;
        return false;
}

void PropRPhys::on_applied()
{
}

DmgResistData PropRPhys::is_resisting_dmg(const DmgType dmg_type) const
{
        DmgResistData d;

        d.is_resisted = is_physical_dmg_type(dmg_type);

        d.msg_resist_player = "I resist harm.";

        d.msg_resist_mon = "seems unharmed.";

        return d;
}

bool PropRFire::is_resisting_other_prop(const PropId prop_id) const
{
        return prop_id == PropId::burning;
}

void PropRFire::on_applied()
{
        m_owner->m_properties.end_prop(
                PropId::burning,
                PropEndConfig(
                        PropEndAllowCallEndHook::no,
                        PropEndAllowMsg::no,
                        PropEndAllowHistoricMsg::yes));
}

DmgResistData PropRFire::is_resisting_dmg(const DmgType dmg_type) const
{
        DmgResistData d;

        d.is_resisted = (dmg_type == DmgType::fire);

        d.msg_resist_player = "I feel warm.";

        d.msg_resist_mon = "seems unaffected.";

        return d;
}

bool PropRPoison::is_resisting_other_prop(const PropId prop_id) const
{
        return prop_id == PropId::poisoned;
}

void PropRPoison::on_applied()
{
        m_owner->m_properties.end_prop(
                PropId::poisoned,
                PropEndConfig(
                        PropEndAllowCallEndHook::no,
                        PropEndAllowMsg::no,
                        PropEndAllowHistoricMsg::yes));
}

bool PropRSleep::is_resisting_other_prop(const PropId prop_id) const
{
        return prop_id == PropId::fainted;
}

void PropRSleep::on_applied()
{
        m_owner->m_properties.end_prop(
                PropId::fainted,
                PropEndConfig(
                        PropEndAllowCallEndHook::no,
                        PropEndAllowMsg::no,
                        PropEndAllowHistoricMsg::yes));
}

bool PropRDisease::is_resisting_other_prop(const PropId prop_id) const
{
        return prop_id == PropId::diseased || prop_id == PropId::infected;
}

void PropRDisease::on_applied()
{
        m_owner->m_properties.end_prop(
                PropId::diseased,
                PropEndConfig(
                        PropEndAllowCallEndHook::no,
                        PropEndAllowMsg::no,
                        PropEndAllowHistoricMsg::yes));

        m_owner->m_properties.end_prop(
                PropId::infected,
                PropEndConfig(
                        PropEndAllowCallEndHook::no,
                        PropEndAllowMsg::no,
                        PropEndAllowHistoricMsg::yes));
}

bool PropRBlind::is_resisting_other_prop(const PropId prop_id) const
{
        return prop_id == PropId::blind;
}

void PropRBlind::on_applied()
{
        m_owner->m_properties.end_prop(
                PropId::blind,
                PropEndConfig(
                        PropEndAllowCallEndHook::no,
                        PropEndAllowMsg::no,
                        PropEndAllowHistoricMsg::yes));
}

bool PropRPara::is_resisting_other_prop(const PropId prop_id) const
{
        return prop_id == PropId::paralyzed;
}

void PropRPara::on_applied()
{
        m_owner->m_properties.end_prop(
                PropId::paralyzed,
                PropEndConfig(
                        PropEndAllowCallEndHook::no,
                        PropEndAllowMsg::no,
                        PropEndAllowHistoricMsg::yes));
}

bool PropSeeInvis::is_resisting_other_prop(const PropId prop_id) const
{
        return prop_id == PropId::blind;
}

void PropSeeInvis::on_applied()
{
        m_owner->m_properties.end_prop(
                PropId::blind,
                PropEndConfig(
                        PropEndAllowCallEndHook::no,
                        PropEndAllowMsg::no,
                        PropEndAllowHistoricMsg::yes));
}

PropEnded PropBurrowing::on_actor_turn()
{
        const P& p = m_owner->m_pos;

        map::g_terrain.at(p)->hit(DmgType::pure, nullptr);

        return PropEnded::no;
}

PropActResult PropVortex::on_act()
{
        // Not supported yet
        if (m_owner->is_player())
        {
                return {};
        }

        if (!m_owner->is_alive())
        {
                return {};
        }

        if (pull_cooldown > 0)
        {
                --pull_cooldown;
        }

        if (!m_owner->is_aware_of_player() || (pull_cooldown > 0))
        {
                return {};
        }

        const P& player_pos = map::g_player->m_pos;

        if (is_pos_adj(m_owner->m_pos, player_pos, true) ||
            !rnd::coin_toss())
        {
                return {};
        }

        TRACE << "Monster with vortex property attempting to pull player"
              << std::endl;

        const auto delta = player_pos - m_owner->m_pos;

        auto knockback_from_pos = player_pos;

        if (delta.x > 1)
        {
                ++knockback_from_pos.x;
        }

        if (delta.x < -1)
        {
                --knockback_from_pos.x;
        }

        if (delta.y > 1)
        {
                ++knockback_from_pos.y;
        }

        if (delta.y < -1)
        {
                --knockback_from_pos.y;
        }

        if (knockback_from_pos == player_pos)
        {
                return {};
        }

        TRACE << "Pos found to knockback player from: "
              << knockback_from_pos.x << ", "
              << knockback_from_pos.y << std::endl;

        TRACE << "Player pos: "
              << player_pos.x << ", " << player_pos.y << std::endl;

        Array2<bool> blocked_los(map::dims());

        const R fov_rect = fov::fov_rect(m_owner->m_pos, blocked_los.dims());

        map_parsers::BlocksLos()
                .run(blocked_los,
                     fov_rect,
                     MapParseMode::overwrite);

        if (actor::can_mon_see_actor(*m_owner, *map::g_player, blocked_los))
        {
                TRACE << "Is seeing player" << std::endl;

                static_cast<actor::Mon*>(m_owner)->set_player_aware_of_me();

                if (actor::can_player_see_actor(*m_owner))
                {
                        const auto name_the = text_format::first_to_upper(
                                m_owner->name_the());

                        msg_log::add(name_the + " pulls me!");
                }
                else
                {
                        msg_log::add("A powerful wind is pulling me!");
                }

                TRACE << "Attempt pull (knockback)" << std::endl;

                // TODO: Add sfx
                knockback::run(
                        *map::g_player,
                        knockback_from_pos,
                        false,
                        Verbose::no);

                pull_cooldown = 2;

                game_time::tick();

                PropActResult result;

                result.did_action = DidAction::yes;
                result.prop_ended = PropEnded::no;

                return result;
        }

        return {};
}

void PropExplodesOnDeath::on_death()
{
        TRACE_FUNC_BEGIN;

        // Setting the actor to destroyed here, just to make sure there will not
        // be a recursive call chain due to explosion damage to the corpse
        m_owner->m_state = ActorState::destroyed;

        explosion::run(m_owner->m_pos, ExplType::expl);

        TRACE_FUNC_END;
}

void PropSplitsOnDeath::on_death()
{
        if (m_owner->is_player())
        {
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
            m_owner->m_properties.has(PropId::burning) ||
            (f_id == terrain::Id::chasm) ||
            (f_id == terrain::Id::liquid_deep) ||
            (game_time::g_actors.size() >= g_max_nr_actors_on_map))
        {
                if (is_player_seeing_owner)
                {
                        // Print a standard death message
                        m_prevent_std_death_msg = false;

                        actor::print_mon_death_msg(*m_owner);
                }

                return;
        }

        // The monster should split

        if (is_player_seeing_owner)
        {
                // NOTE: This is printed instead of the standard death message
                const std::string name =
                        text_format::first_to_upper(
                                m_owner->name_the());

                msg_log::add(name + " splits.");
        }

        auto* const leader = m_owner->m_leader;

        const auto spawned =
                actor::spawn(pos, {2, m_owner->id()}, map::rect())
                        .make_aware_of_player()
                        .set_leader(leader);

        std::for_each(
                std::begin(spawned.monsters),
                std::end(spawned.monsters),
                [](auto* const mon) {
                        auto* prop_waiting = new PropWaiting();

                        prop_waiting->set_duration(1);

                        mon->m_properties.apply(prop_waiting);

                        // The new actors should usually not also split
                        if (rnd::fraction(4, 5))
                        {
                                mon->m_properties.end_prop(
                                        PropId::splits_on_death);
                        }

                        // Do not print a new "monster in view" message
                        mon->m_mon_aware_state
                                .is_msg_mon_in_view_printed = true;
                });

        // If no leader yet, set the first actor as leader of the second
        if (!leader && (spawned.monsters.size() == 2))
        {
                spawned.monsters[1]->m_leader = spawned.monsters[0];
        }
}

PropActResult PropCorpseEater::on_act()
{
        auto did_action = DidAction::no;

        if (m_owner->is_alive() && rnd::coin_toss())
        {
                did_action = m_owner->try_eat_corpse();

                if (did_action == DidAction::yes)
                {
                        game_time::tick();
                }
        }

        PropActResult result;

        result.did_action = did_action;
        result.prop_ended = PropEnded::no;

        return result;
}

PropActResult PropTeleports::on_act()
{
        const int teleport_one_in_n = 12;

        if (!m_owner->is_alive() || !rnd::one_in(teleport_one_in_n))
        {
                return {};
        }

        teleport(*m_owner);

        game_time::tick();

        PropActResult result;
        result.did_action = DidAction::yes;
        result.prop_ended = PropEnded::no;
        return result;
}

PropActResult PropTeleportsAway::on_act()
{
        const int dist = king_dist(m_owner->m_pos, map::g_player->m_pos);
        const bool is_player_near = (dist <= 1);

        if (!is_player_near ||
            !m_owner->is_alive() ||
            m_owner->is_actor_my_leader(map::g_player))
        {
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

void PropAlwaysAware::on_std_turn()
{
        m_owner->m_mon_aware_state.aware_counter = 10;
}

PropActResult PropCorruptsEnvColor::on_act()
{
        auto* const terrain = map::g_terrain.at(m_owner->m_pos);

        terrain->try_corrupt_color();

        return {};
}

void PropAltersEnv::on_std_turn()
{
        Array2<bool> blocked(map::dims());

        map_parsers::BlocksWalking(ParseActors::no)
                .run(blocked, blocked.rect());

        const std::vector<terrain::Id> free_terrains = {
                terrain::Id::door,
                terrain::Id::liquid_deep,
        };

        const int blocked_w = blocked.w();
        const int blocked_h = blocked.h();

        for (int x = 0; x < blocked_w; ++x)
        {
                for (int y = 0; y < blocked_h; ++y)
                {
                        const P p(x, y);

                        const bool is_free_terrain =
                                map_parsers::IsAnyOfTerrains(free_terrains)
                                        .run(p);

                        if (is_free_terrain)
                        {
                                blocked.at(p) = false;
                        }
                }
        }

        Array2<bool> has_actor(map::dims());

        for (auto* actor : game_time::g_actors)
        {
                if (actor->m_state != ActorState::destroyed)
                {
                        has_actor.at(actor->m_pos) = true;
                }
        }

        const int r = 3;

        const R area(
                std::max(1, m_owner->m_pos.x - r),
                std::max(1, m_owner->m_pos.y - r),
                std::min(map::w() - 2, m_owner->m_pos.x + r),
                std::min(map::h() - 2, m_owner->m_pos.y + r));

        for (const auto& p : area.positions())
        {
                if (has_actor.at(p) ||
                    map::g_items.at(p) ||
                    !rnd::one_in(6))
                {
                        continue;
                }

                const auto terrain_id = map::g_terrain.at(p)->id();

                if (terrain_id == terrain::Id::wall)
                {
                        blocked.at(p) = false;

                        if (map_parsers::is_map_connected(blocked))
                        {
                                map::put(new terrain::Floor(p));
                        }
                        else
                        {
                                blocked.at(p) = true;
                        }
                }
                else if (terrain_id == terrain::Id::floor)
                {
                        blocked.at(p) = true;

                        if (map_parsers::is_map_connected(blocked))
                        {
                                map::put(new terrain::RubbleHigh(p));
                        }
                        else
                        {
                                blocked.at(p) = false;
                        }
                }
        }
}

void PropRegenerates::on_std_turn()
{
        if (m_owner->is_alive() &&
            !m_owner->m_properties.has(PropId::burning))
        {
                m_owner->restore_hp(2, false, Verbose::no);
        }
}

PropActResult PropCorpseRises::on_act()
{
        const auto pos = m_owner->m_pos;

        if (!m_owner->is_corpse() ||
            map::first_actor_at_pos(m_owner->m_pos) ||
            (map::g_terrain.at(pos)->id() == terrain::Id::liquid_deep))
        {
                return {};
        }

        const bool is_seen_by_player = actor::can_player_see_actor(*m_owner);

        if (is_seen_by_player)
        {
                hints::display(hints::Id::destroying_corpses);
        }

        if (m_nr_turns_until_allow_rise > 0)
        {
                --m_nr_turns_until_allow_rise;

                return {};
        }

        const int rise_one_in_n = 9;

        if (!rnd::one_in(rise_one_in_n))
        {
                return {};
        }

        m_owner->m_state = ActorState::alive;

        m_owner->m_hp = actor::max_hp(*m_owner) / 2;

        --m_owner->m_data->nr_kills;

        if (is_seen_by_player)
        {
                ASSERT(!m_owner->m_data->corpse_name_the.empty());

                const std::string name =
                        text_format::first_to_upper(
                                m_owner->m_data->corpse_name_the);

                msg_log::add(
                        name + " rises again!!",
                        colors::text(),
                        MsgInterruptPlayer::yes);

                map::g_player->incr_shock(
                        ShockLvl::frightening,
                        ShockSrc::see_mon);
        }

        static_cast<actor::Mon*>(m_owner)
                ->become_aware_player(actor::AwareSource::other);

        game_time::tick();

        m_has_risen = true;

        PropActResult result;

        result.did_action = DidAction::yes;
        result.prop_ended = PropEnded::no;

        return result;
}

void PropCorpseRises::on_death()
{
        // If we have already risen before, and were killed again leaving a
        // corpse, destroy the corpse to prevent rising multiple times
        if (m_owner->is_corpse() && m_has_risen)
        {
                m_owner->m_state = ActorState::destroyed;
                m_owner->m_properties.on_destroyed_alive();
        }
}

void PropSpawnsZombiePartsOnDestroyed::on_destroyed_alive()
{
        try_spawn_zombie_dust();

        try_spawn_zombie_parts();
}

void PropSpawnsZombiePartsOnDestroyed::on_destroyed_corpse()
{
        // NOTE: We do not spawn zombie parts when the corpse is destroyed (it's
        // pretty annoying if parts are spawned when you bash a corpse)
        try_spawn_zombie_dust();
}

void PropSpawnsZombiePartsOnDestroyed::try_spawn_zombie_parts() const
{
        if (m_owner->m_properties.has(PropId::burning))
        {
                return;
        }

        if (!is_allowed_to_spawn_parts_here())
        {
                return;
        }

        const auto pos = m_owner->m_pos;

        // Spawning zombie part monsters is only allowed if the monster is not
        // destroyed "too hard". This is also rewarding heavy weapons, since
        // they will more often prevent spawning
        const bool is_very_destroyed = (m_owner->m_hp <= -8);

        const int summon_one_in_n = 5;

        if (is_very_destroyed || !rnd::one_in(summon_one_in_n))
        {
                return;
        }

        auto id_to_spawn = actor::Id::END;

        const std::vector<int> weights = {
                25,  // Hand
                25,  // Intestines
                1  // Floating skull
        };

        const int mon_choice = rnd::weighted_choice(weights);

        const std::string my_name = m_owner->name_the();

        std::string spawn_msg;

        switch (mon_choice)
        {
        case 0:
                id_to_spawn = actor::Id::crawling_hand;

                spawn_msg =
                        "The hand of " +
                        my_name +
                        " comes off and starts crawling around!";
                break;

        case 1:
                id_to_spawn = actor::Id::crawling_intestines;

                spawn_msg =
                        "The intestines of " +
                        my_name +
                        " starts crawling around!";
                break;

        case 2:
                id_to_spawn = actor::Id::floating_skull;

                spawn_msg =
                        "The head of " +
                        my_name +
                        " starts floating around!";
                break;

        default:
                ASSERT(false);
                break;
        }

        if (map::g_seen.at(pos))
        {
                ASSERT(!spawn_msg.empty());

                msg_log::add(spawn_msg);

                map::g_player->incr_shock(
                        ShockLvl::frightening,
                        ShockSrc::see_mon);
        }

        ASSERT(id_to_spawn != actor::Id::END);

        const auto spawned = actor::spawn(
                                     pos,
                                     {id_to_spawn},
                                     map::rect())
                                     .make_aware_of_player();

        std::for_each(
                std::begin(spawned.monsters),
                std::end(spawned.monsters),
                [](auto* const mon) {
                        auto* waiting = new PropWaiting();

                        waiting->set_duration(1);

                        mon->m_properties.apply(waiting);
                });
}

void PropSpawnsZombiePartsOnDestroyed::try_spawn_zombie_dust() const
{
        if (!is_allowed_to_spawn_parts_here())
        {
                return;
        }

        const int make_dust_one_in_n = 7;

        if (rnd::one_in(make_dust_one_in_n))
        {
                item::make_item_on_floor(item::Id::zombie_dust, m_owner->m_pos);
        }
}

bool PropSpawnsZombiePartsOnDestroyed::is_allowed_to_spawn_parts_here() const
{
        const auto& pos = m_owner->m_pos;

        const auto t_id = map::g_terrain.at(pos)->id();

        return (
                (t_id != terrain::Id::chasm) &&
                (t_id != terrain::Id::liquid_deep));
}

void PropBreeds::on_std_turn()
{
        const int spawn_new_one_in_n = 50;

        if (m_owner->is_player() ||
            !m_owner->is_alive() ||
            m_owner->m_properties.has(PropId::burning) ||
            m_owner->m_properties.has(PropId::paralyzed) ||
            (game_time::g_actors.size() >= g_max_nr_actors_on_map) ||
            !rnd::one_in(spawn_new_one_in_n))
        {
                return;
        }

        auto* const leader_of_spawned_mon =
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
                [](auto* const spawned_mon) {
                        auto* prop_waiting = new PropWaiting();

                        prop_waiting->set_duration(2);

                        spawned_mon->m_properties.apply(prop_waiting);

                        if (actor::can_player_see_actor(*spawned_mon))
                        {
                                const auto name =
                                        text_format::first_to_upper(
                                                spawned_mon->name_a());

                                msg_log::add(name + " is spawned.");
                        }
                });

        if (m_owner->is_aware_of_player())
        {
                spawned.make_aware_of_player();
        }
}

void PropVomitsOoze::on_std_turn()
{
        const int spawn_new_one_in_n =
                m_has_triggered_before
                ? 15
                : 5;

        if (m_owner->is_player() ||
            !m_owner->is_alive() ||
            !m_owner->is_aware_of_player() ||
            m_owner->m_properties.has(PropId::burning) ||
            m_owner->m_properties.has(PropId::paralyzed) ||
            (game_time::g_actors.size() >= g_max_nr_actors_on_map) ||
            !rnd::one_in(spawn_new_one_in_n))
        {
                return;
        }

        const auto area_allowed = R(m_owner->m_pos - 1, m_owner->m_pos + 1);

        actor::Id ooze_ids[] = {
                actor::Id::ooze_putrid,
                actor::Id::ooze_lurking,
                actor::Id::ooze_poison};

        std::vector<actor::Id> id_bucket;

        for (const auto id : ooze_ids)
        {
                const auto& d = actor::g_data[(size_t)id];

                if (map::g_dlvl >= d.spawn_min_dlvl)
                {
                        id_bucket.push_back(id);
                }
        }

        // Robustness - always allow at least Putrid Ooze
        if (id_bucket.empty())
        {
                id_bucket.push_back(actor::Id::ooze_putrid);
        }

        actor::Id id_to_spawn = rnd::element(id_bucket);

        auto* const leader =
                m_owner->m_leader
                ? m_owner->m_leader
                : m_owner;

        auto spawned =
                actor::spawn_random_position(
                        {id_to_spawn},
                        area_allowed)
                        .set_leader(leader);

        std::for_each(
                std::begin(spawned.monsters),
                std::end(spawned.monsters),
                [this](auto* const spawned_mon) {
                        auto* prop_waiting = new PropWaiting();

                        prop_waiting->set_duration(1);

                        spawned_mon->m_properties.apply(prop_waiting);

                        if (actor::can_player_see_actor(*m_owner) &&
                            actor::can_player_see_actor(*spawned_mon))
                        {
                                const auto parent_name =
                                        text_format::first_to_upper(
                                                m_owner->name_the());

                                const auto spawned_name =
                                        spawned_mon->name_a();

                                msg_log::add(
                                        parent_name +
                                        " spews " +
                                        spawned_name +
                                        ".");
                        }
                });

        if (m_owner->is_aware_of_player())
        {
                spawned.make_aware_of_player();
        }

        m_has_triggered_before = true;
}

void PropConfusesAdjacent::on_std_turn()
{
        if (!m_owner->is_alive() ||
            !actor::can_player_see_actor(*m_owner) ||
            !map::g_player->m_pos.is_adjacent(m_owner->m_pos))
        {
                return;
        }

        if (!map::g_player->m_properties.has(PropId::confused))
        {
                const std::string msg =
                        text_format::first_to_upper(m_owner->name_the()) +
                        " bewilders me.";

                msg_log::add(msg);
        }

        auto* prop_confusd = new PropConfused();

        prop_confusd->set_duration(rnd::range(8, 12));

        map::g_player->m_properties.apply(prop_confusd);
}

void PropFrenzyPlayerOnSeen::on_player_see()
{
        const int taunt_on_in_n = 3;

        auto& properties = map::g_player->m_properties;

        if (!properties.has(PropId::frenzied) &&
            rnd::one_in(taunt_on_in_n))
        {
                const auto name =
                        text_format::first_to_upper(
                                m_owner->name_the());

                msg_log::add(name + " is taunting me!");

                auto* const frenzy = property_factory::make(PropId::frenzied);

                frenzy->set_duration(rnd::range(20, 35));

                properties.apply(frenzy);
        }
}

PropActResult PropSpeaksCurses::on_act()
{
        if (m_owner->is_player())
        {
                return {};
        }

        const int curse_one_in_n = 3;

        if (!m_owner->is_alive() ||
            !m_owner->is_aware_of_player() ||
            !rnd::one_in(curse_one_in_n))
        {
                return {};
        }

        Array2<bool> blocked_los(map::dims());

        const auto fov_rect =
                fov::fov_rect(
                        m_owner->m_pos,
                        blocked_los.dims());

        map_parsers::BlocksLos().run(
                blocked_los,
                fov_rect,
                MapParseMode::overwrite);

        if (actor::can_mon_see_actor(*m_owner, *map::g_player, blocked_los))
        {
                const bool player_see_owner =
                        actor::can_player_see_actor(*m_owner);

                std::string snd_msg =
                        player_see_owner
                        ? text_format::first_to_upper(m_owner->name_the())
                        : "Someone";

                snd_msg += " spews forth a litany of curses.";

                Snd snd(
                        snd_msg,
                        audio::SfxId::END,
                        IgnoreMsgIfOriginSeen::no,
                        m_owner->m_pos,
                        m_owner,
                        SndVol::high,
                        AlertsMon::no);

                snd_emit::run(snd);

                map::g_player->m_properties.apply(new PropCursed());

                game_time::tick();

                PropActResult result;

                result.did_action = DidAction::yes;
                result.prop_ended = PropEnded::no;

                return result;
        }

        return {};
}

void PropAuraOfDecay::save() const
{
        saving::put_int(m_max_dmg);
}

void PropAuraOfDecay::load()
{
        m_max_dmg = saving::get_int();
}

int PropAuraOfDecay::range() const
{
        return g_expl_std_radi;
}

void PropAuraOfDecay::on_std_turn()
{
        if (!m_owner->is_alive())
        {
                return;
        }

        run_effect_on_actors();

        run_effect_on_env();
}

void PropAuraOfDecay::run_effect_on_actors() const
{
        for (auto* const actor : game_time::g_actors)
        {
                if ((actor == m_owner) ||
                    (actor->m_state == ActorState::destroyed) ||
                    (king_dist(m_owner->m_pos, actor->m_pos) > range()))
                {
                        continue;
                }

                if (actor::can_player_see_actor(*actor))
                {
                        print_msg_actor_hit(*actor);
                }

                const int dmg = rnd::range(1, m_max_dmg);

                actor::hit(*actor, dmg, DmgType::pure);

                if (!actor->is_player())
                {
                        static_cast<actor::Mon*>(actor)
                                ->become_aware_player(
                                        actor::AwareSource::other);
                }
        }
}

void PropAuraOfDecay::run_effect_on_env() const
{
        for (const P& d : dir_utils::g_dir_list)
        {
                const P p = m_owner->m_pos + d;

                if (!map::is_pos_inside_outer_walls(p))
                {
                        continue;
                }

                run_effect_on_env_at(p);
        }
}

void PropAuraOfDecay::run_effect_on_env_at(const P& p) const
{
        auto* const terrain = map::g_terrain.at(p);

        switch (terrain->id())
        {
        case terrain::Id::floor:
        {
                if (rnd::one_in(100))
                {
                        map::put(new terrain::RubbleLow(p));
                }
        }
        break;

        case terrain::Id::grass:
        {
                if (rnd::one_in(10))
                {
                        static_cast<terrain::Grass*>(terrain)->m_type =
                                terrain::GrassType::withered;
                }
        }
        break;

        case terrain::Id::fountain:
        {
                static_cast<terrain::Fountain*>(terrain)->curse();
        }
        break;

        case terrain::Id::wall:
        case terrain::Id::rubble_high:
        {
                if (rnd::one_in(250))
                {
                        if (map::g_seen.at(p))
                        {
                                const auto name =
                                        text_format::first_to_upper(
                                                terrain->name(Article::the));

                                msg_log::add(name + " collapses!");

                                msg_log::more_prompt();
                        }

                        terrain->hit(DmgType::pure, nullptr);
                }
        }
        break;

        default:
        {
        }
        break;
        }
}

void PropAuraOfDecay::print_msg_actor_hit(const actor::Actor& actor) const
{
        if (actor.is_player())
        {
                msg_log::add("I am decaying!", colors::msg_bad());
        }
}

PropActResult PropMajorClaphamSummon::on_act()
{
        if (m_owner->is_player())
        {
                return {};
        }

        if (!m_owner->is_alive() || !m_owner->is_aware_of_player())
        {
                return {};
        }

        Array2<bool> blocked_los(map::dims());

        const R fov_rect = fov::fov_rect(m_owner->m_pos, blocked_los.dims());

        map_parsers::BlocksLos()
                .run(blocked_los,
                     fov_rect,
                     MapParseMode::overwrite);

        if (!actor::can_mon_see_actor(*m_owner, *map::g_player, blocked_los))
        {
                return {};
        }

        static_cast<actor::Mon*>(m_owner)->set_player_aware_of_me();

        msg_log::add("Major Clapham Lee calls forth his Tomb-Legions!");

        std::vector<actor::Id> ids_to_summon = {actor::Id::dean_halsey};

        const int nr_of_extra_spawns = 4;

        const std::vector<actor::Id> possible_random_id_choices = {
                actor::Id::zombie,
                actor::Id::bloated_zombie};

        const std::vector<int> weights = {
                3,
                1};

        for (int i = 0; i < nr_of_extra_spawns; ++i)
        {
                const int idx = rnd::weighted_choice(weights);

                ids_to_summon.push_back(possible_random_id_choices[idx]);
        }

        auto spawned =
                actor::spawn(m_owner->m_pos, ids_to_summon, map::rect())
                        .make_aware_of_player()
                        .set_leader(m_owner);

        std::for_each(
                std::begin(spawned.monsters),
                std::end(spawned.monsters),
                [](auto* const spawned_mon) {
                        auto* prop_summoned = new PropSummoned();

                        prop_summoned->set_indefinite();

                        spawned_mon->m_properties.apply(prop_summoned);

                        spawned_mon->m_mon_aware_state
                                .is_player_feeling_msg_allowed = false;
                });

        map::g_player->incr_shock(ShockLvl::terrifying, ShockSrc::misc);

        m_owner->m_properties.end_prop(id());

        game_time::tick();

        PropActResult result;

        result.did_action = DidAction::yes;
        result.prop_ended = PropEnded::yes;

        return result;
}

void PropMagicSearching::save() const
{
        saving::put_int(m_range);

        saving::put_bool(m_allow_reveal_items);
        saving::put_bool(m_allow_reveal_creatures);
}

void PropMagicSearching::load()
{
        m_range = saving::get_int();

        m_allow_reveal_items = saving::get_bool();
        m_allow_reveal_creatures = saving::get_bool();
}

PropEnded PropMagicSearching::on_actor_turn()
{
        ASSERT(m_owner->is_player());

        const int orig_x = map::g_player->m_pos.x;
        const int orig_y = map::g_player->m_pos.y;

        const int x0 = std::max(
                0,
                orig_x - m_range);

        const int y0 = std::max(
                0,
                orig_y - m_range);

        const int x1 = std::min(
                map::w() - 1,
                orig_x + m_range);

        const int y1 = std::min(
                map::h() - 1,
                orig_y + m_range);

        for (int y = y0; y <= y1; ++y)
        {
                for (int x = x0; x <= x1; ++x)
                {
                        auto* const t = map::g_terrain.at(x, y);

                        const auto id = t->id();

                        if ((id == terrain::Id::trap) ||
                            (id == terrain::Id::door) ||
                            (id == terrain::Id::monolith) ||
                            (id == terrain::Id::stairs))
                        {
                                map::g_seen.at(x, y) = true;
                                map::g_explored.at(x, y) = true;

                                if (t->is_hidden())
                                {
                                        t->reveal(Verbose::yes);

                                        t->on_revealed_from_searching();

                                        msg_log::more_prompt();
                                }
                        }

                        if (m_allow_reveal_items && map::g_items.at(x, y))
                        {
                                map::g_seen.at(x, y) = true;
                                map::g_explored.at(x, y) = true;
                        }
                }
        }

        if (m_allow_reveal_creatures)
        {
                const int det_mon_multiplier = 20;

                for (auto* actor : game_time::g_actors)
                {
                        const P& p = actor->m_pos;

                        if (actor->is_player() ||
                            !actor->is_alive() ||
                            (king_dist(map::g_player->m_pos, p) > m_range))
                        {
                                continue;
                        }

                        static_cast<actor::Mon*>(actor)->set_player_aware_of_me(
                                det_mon_multiplier);
                }
        }

        states::draw();

        map::g_player->update_fov();

        states::draw();

        return PropEnded::no;
}

bool PropSwimming::allow_read_absolute(const Verbose verbose) const
{
        if (m_owner->is_player() && verbose == Verbose::yes)
        {
                msg_log::add("I cannot read this while swimming.");
        }

        return false;
}

bool PropSwimming::allow_attack_ranged(const Verbose verbose) const
{
        if (m_owner->is_player() && (verbose == Verbose::yes))
        {
                msg_log::add("Not while swimming.");
        }

        return false;
}

bool PropCannotReadCurse::allow_read_absolute(const Verbose verbose) const
{
        if (m_owner->is_player() && verbose == Verbose::yes)
        {
                msg_log::add("I cannot read it.");
        }

        return false;
}

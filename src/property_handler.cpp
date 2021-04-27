// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "property_handler.hpp"

#include <algorithm>
#include <iterator>
#include <optional>
#include <ostream>
#include <utility>

#include "actor.hpp"
#include "actor_player.hpp"
#include "actor_see.hpp"
#include "game.hpp"
#include "map.hpp"
#include "msg_log.hpp"
#include "property.hpp"
#include "property_factory.hpp"
#include "saving.hpp"
#include "text_format.hpp"
#include "actor_data.hpp"
#include "debug.hpp"
#include "player_bon.hpp"

// -----------------------------------------------------------------------------
// Property handler
// -----------------------------------------------------------------------------
PropHandler::PropHandler(actor::Actor* owner) :
        m_owner(owner)
{
        // Reset the active props info
        std::fill(
                std::begin(m_prop_count_cache),
                std::end(m_prop_count_cache),
                0);
}

void PropHandler::apply_natural_props_from_actor_data()
{
        const auto& d = *m_owner->m_data;

        // Add natural properties
        for (size_t i = 0; i < (size_t)PropId::END; ++i)
        {
                m_prop_count_cache[i] = 0;

                if (d.natural_props[i])
                {
                        Prop* const prop = property_factory::make(PropId(i));

                        prop->set_indefinite();

                        apply(prop, PropSrc::intr, true, Verbose::no);
                }
        }
}

void PropHandler::save() const
{
        // Save intrinsic properties to file

        ASSERT(m_owner);

        int nr_intr_props_ = 0;

        for (const auto& prop : m_props)
        {
                if (prop->m_src == PropSrc::intr)
                {
                        ++nr_intr_props_;
                }
        }

        saving::put_int(nr_intr_props_);

        for (const auto& prop : m_props)
        {
                if (prop->m_src == PropSrc::intr)
                {
                        saving::put_int((int)prop->m_id);
                        saving::put_int(prop->m_nr_turns_left);
                        saving::put_int(prop->m_nr_dlvls_left);

                        prop->save();
                }
        }
}

void PropHandler::load()
{
        // Load intrinsic properties from file

        ASSERT(m_owner);

        const int nr_props = saving::get_int();

        for (int i = 0; i < nr_props; ++i)
        {
                const auto prop_id = (PropId)saving::get_int();

                const int nr_turns = saving::get_int();

                const int nr_dlvls = saving::get_int();

                Prop* const prop = property_factory::make(prop_id);

                if (nr_turns == -1)
                {
                        prop->set_indefinite();
                }
                else
                {
                        prop->set_duration(nr_turns);

                        prop->m_nr_dlvls_left = nr_dlvls;
                }

                prop->m_owner = m_owner;

                prop->m_src = PropSrc::intr;

                m_props.push_back(std::unique_ptr<Prop>(prop));

                incr_prop_count(prop_id);

                prop->load();
        }
}

void PropHandler::apply(
        Prop* const prop,
        PropSrc src,
        const bool force_effect,
        const Verbose verbose)
{
        prop->m_owner = m_owner;

        prop->m_src = src;

        std::unique_ptr<Prop> prop_owned(prop);

        // Check if property is resisted
        if (!force_effect)
        {
                if (is_resisting_prop(prop->m_id))
                {
                        if ((verbose == Verbose::yes) &&
                            m_owner->is_alive())
                        {
                                print_resist_msg(*prop);
                        }

                        return;
                }
        }

        // The property can be applied

        if (prop->m_src == PropSrc::intr)
        {
                const bool did_apply_more =
                        try_apply_more_on_existing_intr_prop(*prop, verbose);

                if (did_apply_more)
                {
                        return;
                }
        }

        // The property should be applied individually

        m_props.push_back(std::move(prop_owned));

        incr_prop_count(prop->m_id);

        if ((verbose == Verbose::yes) && m_owner->is_alive())
        {
                if (prop->should_update_vision_on_toggled())
                {
                        map::update_vision();
                }

                print_start_msg(*prop);
        }

        if ((prop->duration_mode() == PropDurationMode::indefinite) &&
            (m_owner == map::g_player))
        {
                const auto& msg = prop->m_data.historic_msg_start_permanent;

                if (!msg.empty())
                {
                        game::add_history_event(msg);
                }
        }

        prop->on_applied();
}

void PropHandler::print_resist_msg(const Prop& prop)
{
        if (m_owner->is_player())
        {
                const auto msg = prop.m_data.msg_res_player;

                if (!msg.empty())
                {
                        msg_log::add(
                                msg,
                                colors::text(),
                                MsgInterruptPlayer::yes);
                }
        }
        else
        {
                // Is a monster
                if (actor::can_player_see_actor(*m_owner))
                {
                        const auto msg = prop.m_data.msg_res_mon;

                        if (!msg.empty())
                        {
                                const std::string monster_name =
                                        text_format::first_to_upper(
                                                m_owner->name_the());

                                msg_log::add(monster_name + " " + msg);
                        }
                }
        }
}

void PropHandler::print_start_msg(const Prop& prop)
{
        if (m_owner->is_player())
        {
                const auto msg = prop.m_data.msg_start_player;

                if (!msg.empty())
                {
                        const auto is_interrupting =
                                (prop.alignment() != PropAlignment::good)
                                ? MsgInterruptPlayer::yes
                                : MsgInterruptPlayer::no;

                        msg_log::add(msg, colors::text(), is_interrupting);
                }
        }
        else
        {
                // Is monster
                if (actor::can_player_see_actor(*m_owner))
                {
                        const auto msg = prop.m_data.msg_start_mon;

                        if (!msg.empty())
                        {
                                const std::string actor_name_the =
                                        text_format::first_to_upper(
                                                m_owner->name_the());

                                msg_log::add(actor_name_the + " " + msg);
                        }
                }
        }
}

bool PropHandler::try_apply_more_on_existing_intr_prop(
        const Prop& new_prop,
        const Verbose verbose)
{
        // NOTE: If an existing property exists which the new property shall be
        // merged with, we keep the old property object and discard the new one

        for (auto& old_prop : m_props)
        {
                if ((new_prop.m_id != old_prop->m_id) ||
                    (old_prop->m_src != PropSrc::intr))
                {
                        continue;
                }

                // Found another intrinsic property of same type

                // Use longest dungeon level duration
                old_prop->m_nr_dlvls_left =
                        std::max(
                                new_prop.m_nr_dlvls_left,
                                old_prop->m_nr_dlvls_left);

                const bool old_is_permanent = old_prop->m_nr_turns_left < 0;
                const bool new_is_permanent = new_prop.m_nr_turns_left < 0;

                if (new_is_permanent)
                {
                        old_prop->m_nr_turns_left = -1;

                        old_prop->m_duration_mode =
                                PropDurationMode::indefinite;
                }
                else if (!old_is_permanent)
                {
                        // Both the old and new property are temporary

                        // TODO: This is a hack to avoid resetting infection
                        // countdown when another infection is applied
                        if (new_prop.id() == PropId::infected)
                        {
                                // Use shortest duration
                                old_prop->m_nr_turns_left =
                                        std::min(
                                                old_prop->m_nr_turns_left,
                                                new_prop.m_nr_turns_left);
                        }
                        else
                        {
                                // Use longest duration
                                old_prop->m_nr_turns_left =
                                        std::max(
                                                old_prop->m_nr_turns_left,
                                                new_prop.m_nr_turns_left);
                        }
                }

                if (verbose == Verbose::yes)
                {
                        print_start_msg(*old_prop);
                }

                old_prop->on_more(new_prop);

                if ((m_owner == map::g_player) &&
                    !old_is_permanent &&
                    new_is_permanent)
                {
                        // The property was temporary and became permanent, log
                        // a historic event for applying a permanent property
                        const auto& msg =
                                old_prop->m_data.historic_msg_start_permanent;

                        if (!msg.empty())
                        {
                                game::add_history_event(msg);
                        }
                }

                return true;
        }

        return false;
}

void PropHandler::add_prop_from_equipped_item(
        const item::Item* const item,
        Prop* const prop,
        const Verbose verbose)
{
        prop->m_item_applying = item;

        apply(
                prop,
                PropSrc::inv,
                true,
                verbose);
}

Prop* PropHandler::prop(const PropId id) const
{
        if (has(id))
        {
                for (const auto& prop : m_props)
                {
                        if (prop->m_id == id)
                        {
                                return prop.get();
                        }
                }
        }

        return nullptr;
}

void PropHandler::remove_props_for_item(const item::Item* const item)
{
        for (auto it = std::begin(m_props); it != std::end(m_props);)
        {
                auto* const prop = it->get();

                if (prop->m_item_applying == item)
                {
                        ASSERT(prop->m_src == PropSrc::inv);

                        ASSERT(prop->m_duration_mode ==
                               PropDurationMode::indefinite);

                        auto moved_prop = std::move(*it);

                        it = m_props.erase(it);

                        decr_prop_count(moved_prop->m_id);

                        on_prop_end(moved_prop.get(), PropEndConfig());
                }
                else
                {
                        // Property was not added by this item
                        ++it;
                }
        }
}

void PropHandler::incr_prop_count(const PropId id)
{
        int& v = m_prop_count_cache[(size_t)id];

#ifndef NDEBUG
        if (v < 0)
        {
                TRACE << "Tried to increment property with current value "
                      << v << std::endl;

                ASSERT(false);
        }
#endif  // NDEBUG

        ++v;
}

void PropHandler::decr_prop_count(const PropId id)
{
        int& v = m_prop_count_cache[(size_t)id];

#ifndef NDEBUG
        if (v <= 0)
        {
                TRACE << "Tried to decrement property with current value "
                      << v << std::endl;

                ASSERT(false);
        }
#endif  // NDEBUG

        --v;
}

void PropHandler::on_prop_end(
        Prop* const prop,
        const PropEndConfig& end_config)
{
        if (prop->should_update_vision_on_toggled())
        {
                map::update_vision();
        }

        // Print end message if this is the last active property of this type
        if ((end_config.allow_msg == PropEndAllowMsg::yes) &&
            (m_owner->m_state == ActorState::alive) &&
            m_prop_count_cache[(size_t)prop->m_id] == 0)
        {
                if (m_owner->is_player())
                {
                        const auto msg = prop->msg_end_player();

                        if (!msg.empty())
                        {
                                msg_log::add(msg);
                        }
                }
                // Not player
                else if (actor::can_player_see_actor(*m_owner))
                {
                        const auto msg = prop->m_data.msg_end_mon;

                        if (!msg.empty())
                        {
                                const std::string actor_name_the =
                                        text_format::first_to_upper(
                                                m_owner->name_the());

                                msg_log::add(
                                        actor_name_the + " " + msg);
                        }
                }
        }

        if (end_config.allow_end_hook == PropEndAllowCallEndHook::yes)
        {
                prop->on_end();
        }

        if ((end_config.allow_historic_msg == PropEndAllowHistoricMsg::yes) &&
            (m_owner == map::g_player) &&
            (prop->duration_mode() == PropDurationMode::indefinite))
        {
                // A permanent property has ended, log a historic event
                const auto& msg = prop->m_data.historic_msg_end_permanent;

                if (!msg.empty())
                {
                        game::add_history_event(msg);
                }
        }
}

bool PropHandler::end_prop(
        const PropId id,
        const PropEndConfig& prop_end_config)
{
        for (auto it = std::begin(m_props); it != std::end(m_props); ++it)
        {
                Prop* const prop = it->get();

                if ((prop->m_id == id) &&
                    (prop->m_src == PropSrc::intr))
                {
                        auto moved_prop = std::move(*it);

                        m_props.erase(it);

                        decr_prop_count(moved_prop->m_id);

                        on_prop_end(moved_prop.get(), prop_end_config);

                        return true;
                }
        }

        return false;
}

void PropHandler::on_placed()
{
        for (auto& prop : m_props)
        {
                prop->on_placed();

                if (!m_owner->is_alive())
                {
                        break;
                }
        }
}

void PropHandler::on_new_dlvl()
{
        for (auto& prop : m_props)
        {
                if (prop->m_nr_dlvls_left > 0)
                {
                        --prop->m_nr_dlvls_left;

                        prop->on_new_dlvl();
                }
        }
}

void PropHandler::on_turn_begin()
{
        for (size_t i = 0; i < m_props.size();)
        {
                auto& prop = m_props[i];

                if ((prop->m_nr_dlvls_left <= 0) &&
                    (prop->m_nr_turns_left > 0))
                {
                        ASSERT(prop->m_src == PropSrc::intr);

                        --prop->m_nr_turns_left;
                }

                const auto prop_ended = prop->on_tick();

                if (prop_ended == PropEnded::no)
                {
                        ++i;
                }
        }
}

void PropHandler::on_turn_end()
{
        for (auto it = std::begin(m_props); it != std::end(m_props);)
        {
                Prop* prop = it->get();

                if (prop->is_finished())
                {
                        auto prop_moved = std::move(*it);

                        it = m_props.erase(it);

                        decr_prop_count(prop_moved->m_id);

                        on_prop_end(prop_moved.get(), PropEndConfig());
                }
                else
                {
                        // Property has not been removed
                        ++it;
                }
        }
}

void PropHandler::on_std_turn()
{
        for (auto& prop : m_props)
        {
                prop->on_std_turn();
        }
}

DidAction PropHandler::on_act()
{
        for (size_t i = 0; i < m_props.size();)
        {
                auto& prop = m_props[i];

                const auto result = prop->on_act();

                if (result.prop_ended == PropEnded::no)
                {
                        ++i;
                }

                if (result.did_action == DidAction::yes)
                {
                        return DidAction::yes;
                }
        }

        return DidAction::no;
}

bool PropHandler::is_temporary_negative_prop(const Prop& prop) const
{
        const auto id = prop.m_id;

        const bool is_natural_prop = m_owner->m_data->natural_props[(size_t)id];

        return !is_natural_prop &&
                (prop.m_duration_mode != PropDurationMode::indefinite) &&
                (prop.alignment() == PropAlignment::bad);
}

std::vector<PropTextListEntry> PropHandler::property_names_temporary_negative()
{
        ASSERT(m_owner != map::g_player);

        auto prop_list = m_owner->m_properties.property_names_and_descr();

        // Remove all non-negative properties (we should not show temporary
        // spell resistance for example), and all natural properties (properties
        // which all monsters of this type starts with)
        for (auto it = begin(prop_list); it != end(prop_list);)
        {
                const auto* const prop = it->prop;

                if (is_temporary_negative_prop(*prop))
                {
                        ++it;
                }
                else
                {
                        // Not a temporary negative property
                        it = prop_list.erase(it);
                }
        }

        return prop_list;
}

bool PropHandler::has_temporary_negative_prop_mon() const
{
        ASSERT(m_owner != map::g_player);

        for (const auto& prop : m_props)
        {
                if (is_temporary_negative_prop(*prop))
                {
                        return true;
                }
        }

        return false;
}

std::vector<ColoredString> PropHandler::property_names_short() const
{
        std::vector<ColoredString> line;

        for (const auto& prop : m_props)
        {
                std::string str = prop->name_short();

                if (str.empty())
                {
                        continue;
                }

                const int turns_left = prop->m_nr_turns_left;

                if (prop->m_duration_mode == PropDurationMode::indefinite)
                {
                        if (prop->src() == PropSrc::intr)
                        {
                                str = text_format::to_upper(str);
                        }
                }
                else
                {
                        // Not indefinite

                        // Player can see number of turns left on own properties
                        // with Self-aware?
                        if (m_owner->is_player() &&
                            player_bon::has_trait(Trait::self_aware) &&
                            prop->allow_display_turns())
                        {
                                // NOTE: Since turns left are decremented before
                                // the actors turn, and checked after the turn -
                                // "turns_left" practically represents how many
                                // more times the actor will act with the
                                // property enabled, EXCLUDING the current
                                // (ongoing) turn.
                                //
                                // I.e. one "turns_left" means that the property
                                // will be enabled the whole next turn, while
                                // Zero "turns_left", means that it will only be
                                // active the current turn. However, from a
                                // players perspective, this is unintuitive;
                                // "one turn left" means the current turn, plus
                                // the next - but is likely interpreted as just
                                // the current turn. Therefore we add +1 to the
                                // displayed value, so that a displayed value of
                                // one means that the property will end after
                                // performing the next action.
                                const int turns_displayed = turns_left + 1;

                                str += ":" + std::to_string(turns_displayed);
                        }
                }

                const auto alignment = prop->alignment();

                Color color;

                auto color_override = prop->color_override();

                if (color_override)
                {
                        color = color_override.value();
                }
                else if (alignment == PropAlignment::good)
                {
                        color = colors::msg_good();
                }
                else if (alignment == PropAlignment::bad)
                {
                        color = colors::msg_bad();
                }
                else
                {
                        color = colors::white();
                }

                line.emplace_back(str, color);
        }

        return line;
}

// TODO: Lots of copy paste here, refactor
std::vector<PropTextListEntry> PropHandler::property_names_and_descr() const
{
        std::vector<PropTextListEntry> list;

        for (const auto& prop : m_props)
        {
                std::string name = prop->name();

                if (name.empty())
                {
                        continue;
                }

                const int turns_left = prop->m_nr_turns_left;

                const bool is_indefinite =
                        prop->m_duration_mode ==
                        PropDurationMode::indefinite;

                const bool is_intr = prop->src() == PropSrc::intr;

                // Player can see number of turns left on own properties with
                // Self-aware?
                if ((is_indefinite || is_intr) &&
                    m_owner->is_player() &&
                    player_bon::has_trait(Trait::self_aware) &&
                    prop->allow_display_turns())
                {
                        // See NOTE in 'props_line' above.
                        const int turns_displayed =
                                turns_left + 1;

                        name += ":" + std::to_string(turns_displayed);
                }

                const PropAlignment alignment = prop->alignment();

                Color color;

                auto color_override = prop->color_override();

                if (color_override)
                {
                        color = color_override.value();
                }
                else if (alignment == PropAlignment::good)
                {
                        color = colors::msg_good();
                }
                else if (alignment == PropAlignment::bad)
                {
                        color = colors::msg_bad();
                }
                else
                {
                        color = colors::white();
                }

                const std::string descr = prop->descr();

                const size_t new_size = list.size() + 1;

                list.resize(new_size);

                auto& entry = list[new_size - 1];

                entry.title.str = name;

                entry.title.color = color;

                entry.descr = descr;

                entry.prop = prop.get();
        }

        return list;
}

bool PropHandler::is_resisting_prop(const PropId id) const
{
        for (const auto& prop : m_props)
        {
                if (prop->is_resisting_other_prop(id))
                {
                        return true;
                }
        }

        return false;
}

bool PropHandler::is_resisting_dmg(
        const DmgType dmg_type,
        const Verbose verbose) const
{
        DmgResistData res_data;

        for (const auto& prop : m_props)
        {
                res_data = prop->is_resisting_dmg(dmg_type);

                if (res_data.is_resisted)
                {
                        break;
                }
        }

        if (res_data.is_resisted &&
            (verbose == Verbose::yes))
        {
                if (m_owner->is_player())
                {
                        msg_log::add(res_data.msg_resist_player);
                }
                else
                {
                        // Is monster
                        if (m_owner->is_player_aware_of_me())
                        {
                                const bool can_player_see_mon =
                                        actor::can_player_see_actor(*m_owner);

                                const std::string mon_name =
                                        can_player_see_mon
                                        ? text_format::first_to_upper(
                                                  m_owner->name_the())
                                        : "It";

                                msg_log::add(
                                        mon_name +
                                        " " +
                                        res_data.msg_resist_mon);
                        }
                }
        }

        return res_data.is_resisted;
}

bool PropHandler::allow_see() const
{
        for (const auto& prop : m_props)
        {
                if (!prop->allow_see())
                {
                        return false;
                }
        }

        return true;
}

int PropHandler::affect_max_hp(const int hp_max) const
{
        int new_hp_max = hp_max;

        for (const auto& prop : m_props)
        {
                new_hp_max = prop->affect_max_hp(new_hp_max);
        }

        return new_hp_max;
}

int PropHandler::affect_max_spi(const int spi_max) const
{
        int new_spi_max = spi_max;

        for (const auto& prop : m_props)
        {
                new_spi_max = prop->affect_max_spi(new_spi_max);
        }

        return new_spi_max;
}

int PropHandler::affect_shock(const int shock) const
{
        int new_shock = shock;

        for (const auto& prop : m_props)
        {
                new_shock = prop->affect_shock(new_shock);
        }

        return new_shock;
}

void PropHandler::affect_move_dir(const P& actor_pos, Dir& dir) const
{
        for (size_t i = 0; i < m_props.size();)
        {
                const auto& prop = m_props[i];

                const auto prop_ended = prop->affect_move_dir(actor_pos, dir);

                if (prop_ended == PropEnded::no)
                {
                        ++i;
                }
        }
}

bool PropHandler::allow_attack(const Verbose verbose) const
{
        for (const auto& prop : m_props)
        {
                if (!prop->allow_attack_melee(verbose) &&
                    !prop->allow_attack_ranged(verbose))
                {
                        return false;
                }
        }

        return true;
}

bool PropHandler::allow_attack_melee(const Verbose verbose) const
{
        for (const auto& prop : m_props)
        {
                if (!prop->allow_attack_melee(verbose))
                {
                        return false;
                }
        }

        return true;
}

bool PropHandler::allow_attack_ranged(const Verbose verbose) const
{
        for (const auto& prop : m_props)
        {
                if (!prop->allow_attack_ranged(verbose))
                {
                        return false;
                }
        }

        return true;
}

bool PropHandler::allow_move() const
{
        for (const auto& prop : m_props)
        {
                if (!prop->allow_move())
                {
                        return false;
                }
        }

        return true;
}

bool PropHandler::allow_act() const
{
        for (const auto& prop : m_props)
        {
                if (!prop->allow_act())
                {
                        return false;
                }
        }

        return true;
}

bool PropHandler::allow_read_absolute(const Verbose verbose) const
{
        for (const auto& prop : m_props)
        {
                if (!prop->allow_read_absolute(verbose))
                {
                        return false;
                }
        }

        return true;
}

bool PropHandler::allow_read_chance(const Verbose verbose) const
{
        for (const auto& prop : m_props)
        {
                if (!prop->allow_read_chance(verbose))
                {
                        return false;
                }
        }

        return true;
}

bool PropHandler::allow_cast_intr_spell_absolute(
        const Verbose verbose) const
{
        for (const auto& prop : m_props)
        {
                if (!prop->allow_cast_intr_spell_absolute(verbose))
                {
                        return false;
                }
        }

        return true;
}

bool PropHandler::allow_cast_intr_spell_chance(
        const Verbose verbose) const
{
        for (const auto& prop : m_props)
        {
                if (!prop->allow_cast_intr_spell_chance(verbose))
                {
                        return false;
                }
        }

        return true;
}

bool PropHandler::allow_speak(const Verbose verbose) const
{
        for (const auto& prop : m_props)
        {
                if (!prop->allow_speak(verbose))
                {
                        return false;
                }
        }

        return true;
}

bool PropHandler::allow_eat(const Verbose verbose) const
{
        for (const auto& prop : m_props)
        {
                if (!prop->allow_eat(verbose))
                {
                        return false;
                }
        }

        return true;
}

bool PropHandler::allow_pray(const Verbose verbose) const
{
        for (const auto& prop : m_props)
        {
                if (!prop->allow_pray(verbose))
                {
                        return false;
                }
        }

        return true;
}

void PropHandler::on_hit()
{
        for (auto& prop : m_props)
        {
                prop->on_hit();
        }
}

void PropHandler::on_death()
{
        TRACE_FUNC_BEGIN_VERBOSE;

        for (size_t i = 0; i < m_props.size();)
        {
                auto& prop = m_props[i];

                const auto prop_ended = prop->on_death();

                if (prop_ended == PropEnded::no)
                {
                        ++i;
                }
        }

        TRACE_FUNC_END_VERBOSE;
}

void PropHandler::on_destroyed_alive()
{
        TRACE_FUNC_BEGIN_VERBOSE;

        for (auto& prop : m_props)
        {
                prop->on_destroyed_alive();
        }

        TRACE_FUNC_END_VERBOSE;
}

void PropHandler::on_destroyed_corpse()
{
        TRACE_FUNC_BEGIN_VERBOSE;

        for (auto& prop : m_props)
        {
                prop->on_destroyed_corpse();
        }

        TRACE_FUNC_END_VERBOSE;
}

int PropHandler::ability_mod(const AbilityId ability) const
{
        int modifier = 0;

        for (const auto& prop : m_props)
        {
                modifier += prop->ability_mod(ability);
        }

        return modifier;
}

bool PropHandler::affect_actor_color(Color& color) const
{
        bool did_affect_color = false;

        for (const auto& prop : m_props)
        {
                if (prop->affect_actor_color(color))
                {
                        did_affect_color = true;

                        // It's probably more likely that a color change due to
                        // a bad property is critical information (e.g.
                        // burning), so then we stop searching and use this
                        // color. If it's a good or neutral property that
                        // affected the color, then we keep searching.
                        if (prop->alignment() == PropAlignment::bad)
                        {
                                break;
                        }
                }
        }

        return did_affect_color;
}

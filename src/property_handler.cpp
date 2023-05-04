// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "property_handler.hpp"

#include <algorithm>
#include <iterator>
#include <memory>
#include <optional>
#include <ostream>
#include <utility>

#include "actor.hpp"
#include "actor_data.hpp"
#include "actor_see.hpp"
#include "colors.hpp"
#include "debug.hpp"
#include "game.hpp"
#include "gfx.hpp"
#include "io.hpp"
#include "map.hpp"
#include "msg_log.hpp"
#include "player_bon.hpp"
#include "property.hpp"
#include "property_factory.hpp"
#include "random.hpp"
#include "saving.hpp"
#include "text_format.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static int calc_resist_chance_from_player_traits()
{
        int resist_chance = 0;

        if (player_bon::has_trait(Trait::tough)) {
                resist_chance += 10;
        }

        if (player_bon::has_trait(Trait::rugged)) {
                resist_chance += 10;
        }

        if (player_bon::has_trait(Trait::resistant)) {
                resist_chance += 25;
        }

        return resist_chance;
}

static Color get_property_gui_color(const prop::Prop& property)
{
        const prop::PropAlignment alignment = property.alignment();

        const std::optional<Color> color_override =
                property.override_property_color();

        const bool is_ending =
                (property.duration_mode() != prop::PropDurationMode::indefinite) &&
                (property.nr_turns_left() == 0);

        if (is_ending) {
                return colors::gray();
        }
        else if (color_override) {
                return color_override.value();
        }
        else if (alignment == prop::PropAlignment::good) {
                return colors::msg_good();
        }
        else if (alignment == prop::PropAlignment::bad) {
                return colors::msg_bad();
        }
        else {
                return colors::white();
        }
}

static std::string get_property_nr_turns_suffix(const prop::Prop& property)
{
        // NOTE: Since turns left are decremented at the start of an actor's
        // turn, and checked when they end the turn - "turns left" practically
        // represents how many more times that the player can act without ending
        // the property (i.e. if the property has "N turns left" this means the
        // player can do N actions without ending the property, and it will only
        // end after action N + 1).
        //
        // However this is probably not what is most intuitive for the
        // player. What may feel more natural is a number that represents how
        // many actions they have left until the turn ends (i.e. "N turn left"
        // means after N actions, the property ends). Therefore the value is
        // incremented by one here.
        //
        return ":" + std::to_string(property.nr_turns_left() + 1);
}

// -----------------------------------------------------------------------------
// prop
// -----------------------------------------------------------------------------
namespace prop
{
const std::string g_property_ending_suffix = " (ending)";

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
        for (size_t i = 0; i < (size_t)Id::END; ++i) {
                m_prop_count_cache[i] = 0;

                if (d.natural_props[i]) {
                        Prop* const prop = make(Id(i));

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

        for (const auto& prop : m_props) {
                if (prop->m_src == PropSrc::intr) {
                        ++nr_intr_props_;
                }
        }

        saving::put_int(nr_intr_props_);

        for (const auto& prop : m_props) {
                if (prop->m_src == PropSrc::intr) {
                        saving::put_int((int)prop->m_id);
                        saving::put_int(prop->m_nr_turns_left);
                        saving::put_int(prop->m_nr_dlvls_left);
                        saving::put_int(prop->m_nr_turns_active);

                        prop->save();
                }
        }
}

void PropHandler::load()
{
        // Load intrinsic properties from file

        ASSERT(m_owner);

        const int nr_props = saving::get_int();

        for (int i = 0; i < nr_props; ++i) {
                const auto prop_id = (Id)saving::get_int();
                const int nr_turns_left = saving::get_int();
                const int nr_dlvls_left = saving::get_int();
                const int nr_turns_active = saving::get_int();

                auto* const prop = make(prop_id);

                if (nr_turns_left == -1) {
                        prop->set_indefinite();
                }
                else {
                        prop->set_duration(nr_turns_left);
                        prop->m_nr_dlvls_left = nr_dlvls_left;
                }

                prop->m_nr_turns_active = nr_turns_active;
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

        if (actor::is_player(m_owner) &&
            player_bon::has_trait(Trait::resistant) &&
            prop->m_data.is_preventable_by_player_trait &&
            (prop->m_duration_mode != PropDurationMode::indefinite) &&
            (prop->m_nr_turns_left >= 4)) {
                prop->m_nr_turns_left /= 2;
        }

        std::shared_ptr<Prop> prop_shared(prop);

        // Check if property is resisted
        if (!force_effect) {
                bool is_resisting = is_resisting_prop(prop->m_id);

                if (!is_resisting &&
                    actor::is_player(m_owner) &&
                    prop->m_data.is_preventable_by_player_trait) {
                        const int resist_chance =
                                calc_resist_chance_from_player_traits();

                        is_resisting = rnd::percent(resist_chance);
                }

                if (is_resisting) {
                        if ((verbose == Verbose::yes) &&
                            m_owner->is_alive()) {
                                print_resist_msg(*prop);
                        }

                        return;
                }
        }

        // The property can be applied

        if (prop->m_src == PropSrc::intr) {
                const bool did_apply_more =
                        try_apply_more_on_existing_intr_prop(
                                *prop,
                                verbose);

                if (did_apply_more) {
                        if (prop->m_data.force_interrupt_player_on_start) {
                                map::g_player->interrupt_actions(
                                        ForceInterruptActions::yes);
                        }

                        return;
                }
        }

        // The property should be applied individually

        if ((verbose == Verbose::yes) && m_owner->is_alive()) {
                print_start_msg(*prop);
        }

        std::weak_ptr<Prop> prop_weak = prop_shared;

        m_props.push_back(std::move(prop_shared));

        incr_prop_count(prop->m_id);

        if ((verbose == Verbose::yes) && m_owner->is_alive()) {
                if (prop->should_update_vision_on_toggled()) {
                        map::update_vision();
                        actor::make_player_aware_seen_monsters();
                }
        }

        if ((prop->duration_mode() == PropDurationMode::indefinite) &&
            (actor::is_player(m_owner))) {
                const auto& msg = prop->m_data.historic_msg_start_permanent;

                if (!msg.empty()) {
                        game::add_history_event(msg);
                }
        }

        prop->on_applied();

        if (prop_weak.expired()) {
                return;
        }

        if (prop->m_data.force_interrupt_player_on_start) {
                map::g_player->interrupt_actions(ForceInterruptActions::yes);
        }
}

void PropHandler::print_resist_msg(const Prop& prop)
{
        if (actor::is_player(m_owner)) {
                const auto msg = prop.m_data.msg_res_player;

                if (!msg.empty()) {
                        msg_log::add(
                                msg,
                                colors::text(),
                                MsgInterruptPlayer::yes);
                }
        }
        else {
                // Is a monster
                if (actor::can_player_see_actor(*m_owner)) {
                        const auto msg = prop.m_data.msg_res_mon;

                        if (!msg.empty()) {
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
        if (actor::is_player(m_owner)) {
                const auto msg = prop.m_data.msg_start_player;

                if (!msg.empty()) {
                        // TODO: We should also force interrupt if the player
                        // cannot act (but maybe not here).
                        const auto is_interrupting =
                                (prop.alignment() != PropAlignment::good)
                                ? MsgInterruptPlayer::yes
                                : MsgInterruptPlayer::no;

                        msg_log::add(msg, colors::text(), is_interrupting);
                }
        }
        else {
                // Is monster
                if (actor::can_player_see_actor(*m_owner)) {
                        const auto msg = prop.m_data.msg_start_mon;

                        if (!msg.empty()) {
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
        // merged with, we keep the old property object and discard the new one.

        for (auto& old_prop : m_props) {
                if ((new_prop.m_id != old_prop->m_id) ||
                    (old_prop->m_src != PropSrc::intr)) {
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

                if (new_is_permanent) {
                        old_prop->m_nr_turns_left = -1;

                        old_prop->m_duration_mode =
                                PropDurationMode::indefinite;
                }
                else if (!old_is_permanent) {
                        // Both the old and new property are temporary

                        // TODO: This is a hack to avoid resetting infection
                        // countdown when another infection is applied
                        if (new_prop.id() == Id::infected) {
                                // Use shortest duration
                                old_prop->m_nr_turns_left =
                                        std::min(
                                                old_prop->m_nr_turns_left,
                                                new_prop.m_nr_turns_left);
                        }
                        else {
                                // Use longest duration
                                old_prop->m_nr_turns_left =
                                        std::max(
                                                old_prop->m_nr_turns_left,
                                                new_prop.m_nr_turns_left);
                        }
                }

                // Use longest number of turns active
                old_prop->m_nr_turns_active =
                        std::max(
                                new_prop.m_nr_turns_active,
                                old_prop->m_nr_turns_active);

                if (verbose == Verbose::yes) {
                        print_start_msg(*old_prop);
                }

                old_prop->on_more(new_prop);

                if (actor::is_player(m_owner) &&
                    !old_is_permanent &&
                    new_is_permanent) {
                        // The property was temporary and became permanent, log
                        // a historic event for applying a permanent property
                        const auto& msg =
                                old_prop->m_data.historic_msg_start_permanent;

                        if (!msg.empty()) {
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

Prop* PropHandler::prop(const Id id) const
{
        if (has(id)) {
                for (const auto& prop : m_props) {
                        if (prop->m_id == id) {
                                return prop.get();
                        }
                }
        }

        return nullptr;
}

void PropHandler::remove_props_for_item(const item::Item* const item)
{
        for (auto it = std::begin(m_props); it != std::end(m_props);) {
                auto* const prop = it->get();

                if (prop->m_item_applying == item) {
                        ASSERT(prop->m_src == PropSrc::inv);

                        ASSERT(prop->m_duration_mode ==
                               PropDurationMode::indefinite);

                        auto moved_prop = std::move(*it);

                        it = m_props.erase(it);

                        decr_prop_count(moved_prop->m_id);

                        on_prop_end(moved_prop.get(), PropEndConfig());
                }
                else {
                        // Property was not added by this item
                        ++it;
                }
        }
}

void PropHandler::incr_prop_count(const Id id)
{
        int& v = m_prop_count_cache[(size_t)id];

#ifndef NDEBUG
        if (v < 0) {
                TRACE << "Tried to increment property with current value "
                      << v << std::endl;

                ASSERT(false);
        }
#endif  // NDEBUG

        ++v;
}

void PropHandler::decr_prop_count(const Id id)
{
        int& v = m_prop_count_cache[(size_t)id];

#ifndef NDEBUG
        if (v <= 0) {
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
        if (prop->should_update_vision_on_toggled()) {
                map::update_vision();
                actor::make_player_aware_seen_monsters();
        }

        // Print end message if this is the last active property of this type
        if ((end_config.allow_msg == PropEndAllowMsg::yes) &&
            (m_owner->m_state == ActorState::alive) &&
            m_prop_count_cache[(size_t)prop->m_id] == 0) {
                if (actor::is_player(m_owner)) {
                        const auto msg = prop->msg_end_player();

                        if (!msg.empty()) {
                                msg_log::add(msg);
                        }
                }
                // Not player
                else if (actor::can_player_see_actor(*m_owner)) {
                        const auto msg = prop->m_data.msg_end_mon;

                        if (!msg.empty()) {
                                const std::string actor_name_the =
                                        text_format::first_to_upper(
                                                m_owner->name_the());

                                msg_log::add(
                                        actor_name_the + " " + msg);
                        }
                }
        }

        if (end_config.allow_end_hook == PropEndAllowCallEndHook::yes) {
                prop->on_end();
        }

        if ((end_config.allow_historic_msg == PropEndAllowHistoricMsg::yes) &&
            actor::is_player(m_owner) &&
            (prop->duration_mode() == PropDurationMode::indefinite)) {
                // A permanent property has ended, log a historic event
                const auto& msg = prop->m_data.historic_msg_end_permanent;

                if (!msg.empty()) {
                        game::add_history_event(msg);
                }
        }
}

bool PropHandler::end_prop(
        const Id id,
        const PropEndConfig& prop_end_config)
{
        for (auto it = std::begin(m_props); it != std::end(m_props); ++it) {
                auto* const prop = it->get();

                if ((prop->m_id == id) &&
                    (prop->m_src == PropSrc::intr)) {
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
        for (auto& prop : m_props) {
                prop->on_placed();

                if (!m_owner->is_alive()) {
                        break;
                }
        }
}

void PropHandler::on_new_dlvl()
{
        for (auto& prop : m_props) {
                if (prop->m_nr_dlvls_left > 0) {
                        --prop->m_nr_dlvls_left;

                        prop->on_new_dlvl();
                }
        }
}

void PropHandler::on_turn_begin()
{
        // TODO: This pattern could be applied on all places that iterates over
        // properties, where the property list may change during iteration.
        std::vector<std::weak_ptr<Prop>> props_weak;

        props_weak.reserve(m_props.size());

        for (const auto& prop : m_props) {
                props_weak.push_back(prop);
        }

        for (auto& prop_weak : props_weak) {
                {
                        auto prop = prop_weak.lock();

                        if (!prop) {
                                continue;
                        }

                        if ((prop->m_nr_dlvls_left <= 0) &&
                            (prop->m_nr_turns_left > 0)) {
                                ASSERT(prop->m_src == PropSrc::intr);

                                --prop->m_nr_turns_left;
                        }

                        const PropEnded prop_ended = prop->on_actor_turn();

                        (void)prop_ended;
                }

                {
                        auto prop = prop_weak.lock();

                        if (!prop) {
                                continue;
                        }

                        ++prop->m_nr_turns_active;
                }
        }
}

void PropHandler::on_turn_end()
{
        for (auto it = std::begin(m_props); it != std::end(m_props);) {
                Prop* prop = it->get();

                if (prop->is_finished()) {
                        auto prop_moved = std::move(*it);

                        it = m_props.erase(it);

                        decr_prop_count(prop_moved->m_id);

                        on_prop_end(prop_moved.get(), PropEndConfig());
                }
                else {
                        // Property has not been removed
                        ++it;
                }
        }
}

void PropHandler::on_std_turn()
{
        for (auto& prop : m_props) {
                prop->on_std_turn();
        }
}

DidAction PropHandler::on_act()
{
        for (size_t i = 0; i < m_props.size();) {
                auto& prop = m_props[i];

                const auto result = prop->on_act();

                if (result.prop_ended == PropEnded::no) {
                        ++i;
                }

                if (result.did_action == DidAction::yes) {
                        return DidAction::yes;
                }
        }

        return DidAction::no;
}

void PropHandler::on_melee_attack()
{
        for (auto& prop : m_props) {
                prop->on_melee_attack();
        }
}

void PropHandler::on_player_see()
{
        for (auto& prop : m_props) {
                prop->on_player_see();
        }
}

bool PropHandler::is_temporary_negative_prop(const Prop& prop) const
{
        const auto id = prop.m_id;

        const bool is_natural_prop = m_owner->m_data->natural_props[(size_t)id];

        const bool is_temporary =
                !is_natural_prop &&
                (prop.m_duration_mode != PropDurationMode::indefinite);

        return is_temporary && (prop.alignment() == PropAlignment::bad);
}

bool PropHandler::has_temporary_negative_prop_mon() const
{
        ASSERT(!actor::is_player(m_owner));

        return (
                std::any_of(
                        std::cbegin(m_props),
                        std::cend(m_props),
                        [this](const auto& prop) {
                                return is_temporary_negative_prop(*prop);
                        }));
}

std::vector<ColoredString> PropHandler::property_names_short() const
{
        std::vector<ColoredString> line;

        line.reserve(m_props.size());

        const bool is_self_aware = player_bon::has_trait(Trait::self_aware);

        for (const auto& prop : m_props) {
                std::string name = prop->name_short();

                if (name.empty()) {
                        continue;
                }

                const bool is_indefinite =
                        (prop->m_duration_mode ==
                         PropDurationMode::indefinite);

                if (is_indefinite) {
                        if (prop->src() == PropSrc::intr) {
                                name = text_format::to_upper(name);
                        }
                }
                else if (prop->m_nr_turns_left == 0) {
                        name += g_property_ending_suffix;
                }
                else if (is_self_aware && prop->allow_display_turns()) {
                        name += get_property_nr_turns_suffix(*prop);
                }

                const Color color = get_property_gui_color(*prop);

                line.emplace_back(name, color);
        }

        return line;
}

std::vector<PropListEntry> PropHandler::property_names_and_descr() const
{
        std::vector<PropListEntry> list;

        list.reserve(m_props.size());

        const bool is_player = actor::is_player(m_owner);
        const bool is_self_aware = player_bon::has_trait(Trait::self_aware);

        for (const auto& prop : m_props) {
                std::string name = prop->name();

                if (name.empty()) {
                        continue;
                }

                const bool is_intr = (prop->src() == PropSrc::intr);

                if (is_player && is_intr) {
                        const bool is_indefinite =
                                (prop->m_duration_mode ==
                                 PropDurationMode::indefinite);

                        if (is_indefinite) {
                                name += " (indefinite)";
                        }
                        else if (prop->m_nr_turns_left == 0) {
                                name += g_property_ending_suffix;
                        }
                        else if (is_self_aware && prop->allow_display_turns()) {
                                name += get_property_nr_turns_suffix(*prop);
                        }
                }

                const size_t new_size = list.size() + 1;

                list.resize(new_size);

                PropListEntry& entry = list[new_size - 1];

                entry.title.str = name;
                entry.title.color = get_property_gui_color(*prop);
                entry.descr = prop->descr();
                entry.prop = prop.get();
        }

        return list;
}

bool PropHandler::is_resisting_prop(const Id id) const
{
        for (const auto& prop : m_props) {
                if (prop->is_resisting_other_prop(id)) {
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

        for (const auto& prop : m_props) {
                res_data = prop->is_resisting_dmg(dmg_type);

                if (res_data.is_resisted) {
                        break;
                }
        }

        if (res_data.is_resisted && (verbose == Verbose::yes)) {
                if (actor::is_player(m_owner)) {
                        msg_log::add(res_data.msg_resist_player);
                }
                else {
                        // Is monster
                        if (m_owner->is_player_aware_of_me()) {
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
        for (const auto& prop : m_props) {
                if (!prop->allow_see()) {
                        return false;
                }
        }

        return true;
}

int PropHandler::affect_max_hp(const int hp_max) const
{
        int new_hp_max = hp_max;

        for (const auto& prop : m_props) {
                new_hp_max = prop->affect_max_hp(new_hp_max);
        }

        return new_hp_max;
}

int PropHandler::affect_max_spi(const int spi_max) const
{
        int new_spi_max = spi_max;

        for (const auto& prop : m_props) {
                new_spi_max = prop->affect_max_spi(new_spi_max);
        }

        return new_spi_max;
}

int PropHandler::player_extra_min_shock() const
{
        int shock = 0;

        for (const auto& prop : m_props) {
                shock += prop->player_extra_min_shock();
        }

        return shock;
}

int PropHandler::armor_points() const
{
        int armor = 0;

        for (const auto& prop : m_props) {
                armor += prop->armor_points();
        }

        return armor;
}

void PropHandler::affect_move_dir(Dir& dir) const
{
        for (size_t i = 0; i < m_props.size();) {
                const auto& prop = m_props[i];

                const PropEnded prop_ended = prop->affect_move_dir(dir);

                if (prop_ended == PropEnded::no) {
                        ++i;
                }
        }
}

bool PropHandler::allow_move_dir(const Dir dir) const
{
        for (const auto& prop : m_props) {
                if (!prop->allow_move_dir(dir)) {
                        return false;
                }
        }

        return true;
}

void PropHandler::on_moved_non_center_dir() const
{
        for (size_t i = 0; i < m_props.size();) {
                const auto& prop = m_props[i];

                const PropEnded prop_ended = prop->on_moved_non_center_dir();

                if (prop_ended == PropEnded::no) {
                        ++i;
                }
        }
}

bool PropHandler::allow_attack(const Verbose verbose) const
{
        for (const auto& prop : m_props) {
                if (!prop->allow_attack_melee(verbose) &&
                    !prop->allow_attack_ranged(verbose)) {
                        return false;
                }
        }

        return true;
}

bool PropHandler::allow_attack_melee(const Verbose verbose) const
{
        for (const auto& prop : m_props) {
                if (!prop->allow_attack_melee(verbose)) {
                        return false;
                }
        }

        return true;
}

bool PropHandler::allow_attack_ranged(const Verbose verbose) const
{
        for (const auto& prop : m_props) {
                if (!prop->allow_attack_ranged(verbose)) {
                        return false;
                }
        }

        return true;
}

bool PropHandler::allow_move() const
{
        for (const auto& prop : m_props) {
                if (!prop->allow_move()) {
                        return false;
                }
        }

        return true;
}

bool PropHandler::allow_act() const
{
        for (const auto& prop : m_props) {
                if (!prop->allow_act()) {
                        return false;
                }
        }

        return true;
}

bool PropHandler::allow_read_absolute(const Verbose verbose) const
{
        for (const auto& prop : m_props) {
                if (!prop->allow_read_absolute(verbose)) {
                        return false;
                }
        }

        return true;
}

bool PropHandler::allow_read_chance(const Verbose verbose) const
{
        for (const auto& prop : m_props) {
                if (!prop->allow_read_chance(verbose)) {
                        return false;
                }
        }

        return true;
}

bool PropHandler::allow_cast_intr_spell_absolute(
        const Verbose verbose) const
{
        for (const auto& prop : m_props) {
                if (!prop->allow_cast_intr_spell_absolute(verbose)) {
                        return false;
                }
        }

        return true;
}

bool PropHandler::allow_cast_intr_spell_chance(
        const Verbose verbose) const
{
        for (const auto& prop : m_props) {
                if (!prop->allow_cast_intr_spell_chance(verbose)) {
                        return false;
                }
        }

        return true;
}

bool PropHandler::allow_speak(const Verbose verbose) const
{
        for (const auto& prop : m_props) {
                if (!prop->allow_speak(verbose)) {
                        return false;
                }
        }

        return true;
}

bool PropHandler::allow_eat(const Verbose verbose) const
{
        for (const auto& prop : m_props) {
                if (!prop->allow_eat(verbose)) {
                        return false;
                }
        }

        return true;
}

bool PropHandler::allow_pray(const Verbose verbose) const
{
        for (const auto& prop : m_props) {
                if (!prop->allow_pray(verbose)) {
                        return false;
                }
        }

        return true;
}

void PropHandler::on_hit(
        const int dmg,
        const DmgType dmg_type,
        actor::Actor* const attacker)
{
        for (size_t i = 0; i < m_props.size();) {
                const auto& prop = m_props[i];

                const PropEnded prop_ended =
                        prop->on_hit(dmg, dmg_type, attacker);

                if (prop_ended == PropEnded::no) {
                        ++i;
                }
        }
}

void PropHandler::on_death()
{
        TRACE_FUNC_BEGIN_VERBOSE;

        for (auto& prop : m_props) {
                prop->on_death();
        }

        TRACE_FUNC_END_VERBOSE;
}

void PropHandler::on_destroyed_alive()
{
        TRACE_FUNC_BEGIN_VERBOSE;

        for (auto& prop : m_props) {
                prop->on_destroyed_alive();
        }

        TRACE_FUNC_END_VERBOSE;
}

void PropHandler::on_destroyed_corpse()
{
        TRACE_FUNC_BEGIN_VERBOSE;

        for (auto& prop : m_props) {
                prop->on_destroyed_corpse();
        }

        TRACE_FUNC_END_VERBOSE;
}

int PropHandler::ability_mod(const AbilityId ability) const
{
        int modifier = 0;

        for (const auto& prop : m_props) {
                modifier += prop->ability_mod(ability);
        }

        return modifier;
}

void PropHandler::cycle_graphics(io::GraphicsCycle cycle) const
{
        if (cycle == io::GraphicsCycle::fast) {
                for (const auto& prop : m_props) {
                        prop->cycle_graphics();
                }
        }
}

std::optional<std::string> PropHandler::override_actor_name_the() const
{
        std::optional<std::string> name = {};

        for (const auto& prop : m_props) {
                auto new_name = prop->override_actor_name_the();

                if (new_name) {
                        name = new_name;
                        break;
                }
        }

        return name;
}

std::optional<std::string> PropHandler::override_actor_name_a() const
{
        std::optional<std::string> name = {};

        for (const auto& prop : m_props) {
                auto new_name = prop->override_actor_name_a();

                if (new_name) {
                        name = new_name;
                        break;
                }
        }

        return name;
}

std::optional<gfx::TileId> PropHandler::override_actor_tile() const
{
        std::optional<gfx::TileId> tile = {};

        for (const auto& prop : m_props) {
                auto new_tile = prop->override_actor_tile();

                if (new_tile) {
                        tile = new_tile;
                        break;
                }
        }

        return tile;
}

std::optional<char> PropHandler::override_actor_character() const
{
        std::optional<char> c = {};

        for (const auto& prop : m_props) {
                auto new_c = prop->override_actor_character();

                if (new_c) {
                        c = new_c;
                        break;
                }
        }

        return c;
}

std::optional<std::string> PropHandler::override_actor_descr() const
{
        std::optional<std::string> descr = {};

        for (const auto& prop : m_props) {
                auto new_name = prop->override_actor_descr();

                if (new_name) {
                        descr = new_name;
                        break;
                }
        }

        return descr;
}

std::optional<Color> PropHandler::override_actor_color() const
{
        std::optional<Color> color = {};

        for (const auto& prop : m_props) {
                auto new_color = prop->override_actor_color();

                if (new_color) {
                        color = new_color;

                        // It's probably more likely that a color change due to
                        // a bad property is critical information (e.g.
                        // burning), so we stop searching and use this color. If
                        // it's a good or neutral property that affected the
                        // color, then we keep searching.
                        if (prop->alignment() == PropAlignment::bad) {
                                break;
                        }
                }
        }

        return color;
}

}  // namespace prop

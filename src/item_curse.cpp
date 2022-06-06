// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "item_curse.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <utility>

#include "actor.hpp"
#include "actor_data.hpp"
#include "actor_factory.hpp"
#include "array2.hpp"
#include "audio_data.hpp"
#include "colors.hpp"
#include "debug.hpp"
#include "global.hpp"
#include "item.hpp"
#include "item_data.hpp"
#include "map.hpp"
#include "msg_log.hpp"
#include "pos.hpp"
#include "property.hpp"
#include "property_data.hpp"
#include "property_factory.hpp"
#include "property_handler.hpp"
#include "random.hpp"
#include "saving.hpp"
#include "sound.hpp"
#include "teleport.hpp"
#include "terrain.hpp"
#include "text_format.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static bool s_available_curses[(size_t)item_curse::Id::END];

static std::unique_ptr<item_curse::CurseImpl> make_impl(const item_curse::Id id)
{
        std::unique_ptr<item_curse::CurseImpl> impl;

        switch (id) {
        case item_curse::Id::hit_chance_penalty:
                return std::make_unique<item_curse::HitChancePenalty>();

        case item_curse::Id::increased_shock:
                return std::make_unique<item_curse::IncreasedShock>();

        case item_curse::Id::heavy:
                return std::make_unique<item_curse::Heavy>();

        case item_curse::Id::shriek:
                return std::make_unique<item_curse::Shriek>();

        case item_curse::Id::teleport:
                return std::make_unique<item_curse::Teleport>();

        case item_curse::Id::summon:
                return std::make_unique<item_curse::Summon>();

        case item_curse::Id::fire:
                return std::make_unique<item_curse::Fire>();

        case item_curse::Id::cannot_read:
                return std::make_unique<item_curse::CannotRead>();

        case item_curse::Id::light_sensitive:
                return std::make_unique<item_curse::LightSensitive>();

        case item_curse::Id::END:
                break;
        }

        ASSERT(false);

        return nullptr;
}

// -----------------------------------------------------------------------------
// item_curse
// -----------------------------------------------------------------------------
namespace item_curse
{
void init()
{
        std::fill(
                std::begin(s_available_curses),
                std::end(s_available_curses),
                true);
}

void save()
{
        for (size_t i = 0; i < (size_t)Id::END; ++i) {
                saving::put_bool(s_available_curses[i]);
        }
}

void load()
{
        for (size_t i = 0; i < (size_t)Id::END; ++i) {
                s_available_curses[i] = saving::get_bool();
        }
}

Curse try_make_random_free_curse(const item::Item& item)
{
        std::vector<Id> bucket;

        bucket.reserve((size_t)Id::END);

        for (size_t i = 0; i < (size_t)Id::END; ++i) {
                const auto id = (Id)i;

                if (s_available_curses[i] && item.is_curse_allowed(id)) {
                        bucket.push_back(id);
                }
        }

        if (bucket.empty()) {
                return Curse(nullptr);
        }

        const auto id = rnd::element(bucket);

        s_available_curses[(size_t)id] = false;

        return Curse(make_impl(id));
}

// -----------------------------------------------------------------------------
// Curse
// -----------------------------------------------------------------------------
Curse::Curse(Curse&& other) :
        m_dlvl_countdown(other.m_dlvl_countdown),
        m_turn_countdown(other.m_turn_countdown),
        m_warning_dlvl_countdown(other.m_warning_dlvl_countdown),
        m_warning_turn_countdown(other.m_warning_turn_countdown),
        m_curse_impl(std::move(other.m_curse_impl))
{
}

Curse::Curse(std::unique_ptr<CurseImpl> curse_impl) :
        m_curse_impl(std::move(curse_impl))
{
}

Curse& Curse::operator=(Curse&& other)
{
        m_dlvl_countdown = other.m_dlvl_countdown;
        m_turn_countdown = other.m_turn_countdown;

        m_warning_dlvl_countdown = other.m_warning_dlvl_countdown;
        m_warning_turn_countdown = other.m_warning_turn_countdown;

        m_curse_impl = std::move(other.m_curse_impl);

        return *this;
}

Id Curse::id() const
{
        if (m_curse_impl) {
                return m_curse_impl->id();
        }
        else {
                return Id::END;
        }
}

void Curse::save() const
{
        saving::put_int(m_dlvl_countdown);
        saving::put_int(m_turn_countdown);

        saving::put_int(m_warning_dlvl_countdown);
        saving::put_int(m_warning_turn_countdown);

        const auto curse_id = id();

        saving::put_int((int)curse_id);

        if (m_curse_impl) {
                m_curse_impl->save();
        }
}

void Curse::load()
{
        m_dlvl_countdown = saving::get_int();
        m_turn_countdown = saving::get_int();

        m_warning_dlvl_countdown = saving::get_int();
        m_warning_turn_countdown = saving::get_int();

        const auto curse_id = (Id)saving::get_int();

        if (curse_id == Id::END) {
                m_curse_impl = nullptr;
        }
        else {
                m_curse_impl = make_impl(curse_id);

                m_curse_impl->load();
        }
}

void Curse::on_new_turn(const item::Item& item)
{
        if (!m_curse_impl) {
                return;
        }

        if (m_dlvl_countdown == 0) {
                if (m_turn_countdown == 0) {
                        // Curse already triggered - run new turn events
                        m_curse_impl->on_new_turn_active(item);
                }
                else if (m_turn_countdown == 1) {
                        // Curse is triggered for the first time now
                        m_curse_impl->on_start(item);

                        print_trigger_msg(item);

                        map::g_player->incr_shock(
                                4.0,
                                ShockSrc::use_strange_item);

                        m_warning_dlvl_countdown = -1;
                        m_warning_turn_countdown = -1;
                }

                if (m_turn_countdown > 0) {
                        --m_turn_countdown;
                }
        }

        if (m_warning_dlvl_countdown == 0) {
                if (m_warning_turn_countdown == 1) {
                        // Warning is triggered now
                        print_warning_msg(item);

                        map::g_player->incr_shock(
                                2.0,
                                ShockSrc::use_strange_item);
                }

                if (m_warning_turn_countdown > 0) {
                        --m_warning_turn_countdown;
                }
        }
}

void Curse::print_trigger_msg(const item::Item& item) const
{
        // NOTE: Unique artifacts have a different "a" form (typically "the",
        // instead of "a").
        const auto item_name =
                item.name(
                        ItemNameType::a,
                        ItemNameInfo::none);

        msg_log::add(
                "A curse lies upon " + item_name + "!",
                colors::msg_note(),
                MsgInterruptPlayer::no,
                MorePromptOnMsg::yes);

        const auto specific_msg = m_curse_impl->curse_msg(item);

        if (!specific_msg.empty()) {
                msg_log::add(m_curse_impl->curse_msg(item));
        }
}

void Curse::print_warning_msg(const item::Item& item) const
{
        // NOTE: Unique artifacts have a different "a" form (typically "the",
        // instead of "a").
        const auto item_name =
                item.name(
                        ItemNameType::a,
                        ItemNameInfo::none);

        const std::vector<std::string> msg_bucket = {
                {"I am growing very attached to " +
                 item_name +
                 "."},
                {"I am starting to think that I should hold on to " +
                 item_name +
                 ", forever..."},
        };

        const auto msg = rnd::element(msg_bucket);

        msg_log::add(
                msg,
                colors::msg_note(),
                MsgInterruptPlayer::no,
                MorePromptOnMsg::yes);
}

void Curse::on_player_reached_new_dlvl()
{
        if (!m_curse_impl) {
                return;
        }

        if ((m_dlvl_countdown > 0) && (m_turn_countdown > 0)) {
                --m_dlvl_countdown;
        }

        if ((m_warning_dlvl_countdown > 0) && (m_warning_turn_countdown > 0)) {
                --m_warning_dlvl_countdown;
        }
}

void Curse::on_item_picked_up(const item::Item& item)
{
        if (!m_curse_impl) {
                return;
        }

        if (saving::is_loading()) {
                return;
        }

        if (m_turn_countdown == 0) {
                m_curse_impl->on_start(item);
        }
        else if (m_turn_countdown == -1) {
                // Initialize countdowns
                m_dlvl_countdown = rnd::range(1, 4);
                m_turn_countdown = rnd::range(100, 300);

                if (rnd::fraction(2, 3)) {
                        // Also initialize warning countdown
                        m_warning_dlvl_countdown =
                                rnd::range(1, m_dlvl_countdown);

                        if (m_warning_dlvl_countdown == m_dlvl_countdown) {
                                m_warning_turn_countdown =
                                        rnd::range(50, m_turn_countdown - 10);
                        }
                        else {
                                m_warning_turn_countdown =
                                        rnd::range(100, 300);
                        }
                }
        }
}

void Curse::on_item_dropped()
{
        if (!m_curse_impl) {
                return;
        }

        if (m_turn_countdown == 0) {
                m_curse_impl->on_stop();
        }
}

void Curse::on_curse_end()
{
        if (!m_curse_impl) {
                return;
        }

        if (m_turn_countdown == 0) {
                m_curse_impl->on_stop();
        }
}

int Curse::affect_weight(const int weight) const
{
        if (!m_curse_impl) {
                return weight;
        }

        if (m_turn_countdown == 0) {
                return m_curse_impl->affect_weight(weight);
        }
        else {
                return weight;
        }
}

std::string Curse::descr() const
{
        if (!m_curse_impl) {
                return "";
        }

        return "This item is cursed, " + m_curse_impl->descr();
}

// -----------------------------------------------------------------------------
// Hit chance penalty
// -----------------------------------------------------------------------------
void HitChancePenalty::on_start(const item::Item& item)
{
        (void)item;

        auto* const prop = property_factory::make(
                PropId::hit_chance_penalty_curse);

        prop->set_indefinite();

        map::g_player->m_properties.apply(prop);
}

void HitChancePenalty::on_stop()
{
        map::g_player->m_properties.end_prop(
                PropId::hit_chance_penalty_curse);
}

std::string HitChancePenalty::descr() const
{
        return "it makes the owner less accurate (-10% hit chance with melee "
               "and ranged attacks).";
}

// -----------------------------------------------------------------------------
// Increased shock
// -----------------------------------------------------------------------------
void IncreasedShock::on_start(const item::Item& item)
{
        (void)item;

        auto* const prop = property_factory::make(
                PropId::increased_shock_curse);

        prop->set_indefinite();

        map::g_player->m_properties.apply(prop);
}

void IncreasedShock::on_stop()
{
        map::g_player->m_properties.end_prop(
                PropId::increased_shock_curse);
}

std::string IncreasedShock::descr() const
{
        return "it is a burden on the mind of the owner (+10% minimum shock).";
}

// -----------------------------------------------------------------------------
// Heavy
// -----------------------------------------------------------------------------
int Heavy::affect_weight(const int weight)
{
        return weight + (int)item::Weight::medium;
}

std::string Heavy::descr() const
{
        return "it is inexplicably heavy for its size.";
}

std::string Heavy::curse_msg(const item::Item& item) const
{
        // NOTE: Unique artifacts have a different "a" form (typically "the",
        // instead of "a").
        const auto name =
                text_format::first_to_upper(
                        item.name(
                                ItemNameType::a,
                                ItemNameInfo::none));

        return name + " suddenly feels much heavier to carry.";
}

// -----------------------------------------------------------------------------
// Shriek
// -----------------------------------------------------------------------------
Shriek::Shriek()

{
        auto player_name = text_format::to_upper(map::g_player->name_the());

        m_words = {
                player_name,
                player_name,
                player_name,
                "BHUUDESCO",
                "STRAGARANA",
                "INFIRMUX",
                "BHAAVA",
                "CRUENTO",
                "PRETIACRUENTO",
                "VILOMAXUS",
                "DEATH",
                "DYING",
                "TAKE",
                "BLOOD",
                "END",
                "SACRIFICE",
                "POSSESSED",
                "PROPHECY",
                "SIGNS",
                "OATH",
                "FATE",
                "SUFFER",
                "BEHOLD",
                "MANIFEST",
                "BEWARE",
                "WATCHES",
                "LIGHT",
                "DARK",
                "DISAPPEAR",
                "APPEAR",
                "DECAY",
                "IMMORTAL",
                "ALL",
                "BOUNDLESS",
                "ETERNAL",
                "ANCIENT",
                "TIME",
                "NEVER-ENDING",
                "NEVER",
                "DIMENSIONS",
                "EYES",
                "STARS",
                "GAZE",
                "FORBIDDEN",
                "FOLLOW",
                "DOMINIONS",
                "RULER",
                "KING",
                "UNKNOWN",
                "ABYSS",
                "BENEATH",
                "BEYOND",
                "BELOW",
                "PASSAGE",
                "PATH",
                "GATE",
                "SERPENT",
                "HEAR",
                "SEE",
                "UNSEEN",
                "NYARLATHOTEP",
                "GOL-GOROTH",
                "ABHOLOS",
                "HASTUR",
                "ISTASHA",
                "ITHAQUA",
                "THOG",
                "TSATHOGGUA",
                "YMNAR",
                "XCTHOL",
                "ZATHOG",
                "ZINDARAK",
                "BASATAN",
                "CTHUGHA"};
}

void Shriek::on_start(const item::Item& item)
{
        shriek(item);
}

void Shriek::on_new_turn_active(const item::Item& item)
{
        if (rnd::one_in(300)) {
                shriek(item);
        }
}

void Shriek::shriek(const item::Item& item) const
{
        // NOTE: Unique artifacts have a different "a" form (typically "the",
        // instead of "a").
        const std::string name =
                text_format::first_to_upper(
                        item.name(
                                ItemNameType::a,
                                ItemNameInfo::none));

        msg_log::add(
                name + " shrieks...",
                colors::text(),
                MsgInterruptPlayer::no,
                MorePromptOnMsg::yes);

        const int nr_words = rnd::range(2, 4);

        std::string phrase;

        for (int i = 0; i < nr_words; ++i) {
                const auto& word = rnd::element(m_words);

                phrase += word + "...";

                if (i < (nr_words - 1)) {
                        phrase += " ";
                }
        }

        Snd snd(
                phrase,
                audio::SfxId::END,
                IgnoreMsgIfOriginSeen::no,
                map::g_player->m_pos,
                map::g_player,
                SndVol::high,
                AlertsMon::yes);

        snd_emit::run(snd);

        map::g_player->incr_shock(2.0, ShockSrc::misc);

        msg_log::more_prompt();
}

std::string Shriek::descr() const
{
        return "it occasionally emits a disembodied voice in a horrible "
               "shrieking tone.";
}

// -----------------------------------------------------------------------------
// Teleport
// -----------------------------------------------------------------------------
void Teleport::on_start(const item::Item& item)
{
        teleport(item);
}

void Teleport::on_new_turn_active(const item::Item& item)
{
        if (rnd::one_in(200) && map::g_player->m_properties.allow_act()) {
                teleport(item);
        }
}

void Teleport::teleport(const item::Item& item) const
{
        // NOTE: Unique artifacts have a different "a" form (typically "the",
        // instead of "a").
        const auto name = item.name(ItemNameType::a, ItemNameInfo::none);

        msg_log::add(
                "I somehow sense that a burst of energy is discharged "
                "from " +
                name +
                ".");

        msg_log::add(
                "I am being teleported...",
                colors::text(),
                MsgInterruptPlayer::yes,
                MorePromptOnMsg::yes);

        ::teleport(*map::g_player);
}

std::string Teleport::descr() const
{
        return "it occasionally teleports the wearer.";
}

// -----------------------------------------------------------------------------
// Summon
// -----------------------------------------------------------------------------
void Summon::on_new_turn_active(const item::Item& item)
{
        (void)item;

        if (rnd::one_in(1200)) {
                summon(item);
        }
}

void Summon::summon(const item::Item& item) const
{
        (void)item;

        msg_log::add(
                "There is a loud whistling sound.",
                colors::text(),
                MsgInterruptPlayer::no,
                MorePromptOnMsg::yes);

        actor::spawn(
                map::g_player->m_pos,
                {1, actor::Id::greater_polyp},
                map::rect())
                .make_aware_of_player();
}

std::string Summon::curse_msg(const item::Item& item) const
{
        (void)item;

        return "I hear a faint whistling sound coming nearer...";
}

std::string Summon::descr() const
{
        return "it calls deadly interdimensional beings into the existence of "
               "the owner.";
}

// -----------------------------------------------------------------------------
// Fire
// -----------------------------------------------------------------------------
void Fire::on_start(const item::Item& item)
{
        run_fire(item);
}

void Fire::on_new_turn_active(const item::Item& item)
{
        (void)item;

        if (rnd::one_in(300)) {
                run_fire(item);
        }
}

void Fire::run_fire(const item::Item& item) const
{
        (void)item;

        msg_log::add(
                "The surrounding area suddenly burst into flames!",
                colors::text(),
                MsgInterruptPlayer::no,
                MorePromptOnMsg::yes);

        const int d = g_fov_radi_int - 2;

        const auto& origin = map::g_player->m_pos;

        const int x0 = std::max(1, origin.x - d);
        const int y0 = std::max(1, origin.y - d);
        const int x1 = std::min(map::w() - 2, origin.x + d);
        const int y1 = std::min(map::h() - 2, origin.y + d);

        const int fire_cell_one_in_n = 2;

        for (int x = x0; x <= x1; ++x) {
                for (int y = y0; y <= y1; ++y) {
                        const P p(x, y);

                        if (rnd::one_in(fire_cell_one_in_n) &&
                            (p != origin)) {
                                map::g_terrain.at(p)->hit(
                                        DmgType::fire,
                                        nullptr);
                        }
                }
        }
}

std::string Fire::descr() const
{
        return "it spontaneously sets objects around the caster on fire.";
}

// -----------------------------------------------------------------------------
// Cannot read
// -----------------------------------------------------------------------------
void CannotRead::on_start(const item::Item& item)
{
        (void)item;

        auto* const prop = property_factory::make(
                PropId::cannot_read_curse);

        prop->set_indefinite();

        map::g_player->m_properties.apply(prop);
}

void CannotRead::on_stop()
{
        map::g_player->m_properties.end_prop(
                PropId::cannot_read_curse);
}

std::string CannotRead::descr() const
{
        return "it prevents the owner from comprehending written language.";
}

// -----------------------------------------------------------------------------
// Light sensitive
// -----------------------------------------------------------------------------
void LightSensitive::on_start(const item::Item& item)
{
        (void)item;

        auto* const prop = property_factory::make(
                PropId::light_sensitive_curse);

        prop->set_indefinite();

        map::g_player->m_properties.apply(prop);
}

void LightSensitive::on_stop()
{
        map::g_player->m_properties.end_prop(
                PropId::light_sensitive_curse);
}

std::string LightSensitive::descr() const
{
        return "the owner is harmed by light.";
}

}  // namespace item_curse

// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "insanity.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>

#include "actor.hpp"
#include "actor_data.hpp"
#include "actor_factory.hpp"
#include "actor_mon.hpp"
#include "actor_player.hpp"
#include "actor_see.hpp"
#include "array2.hpp"
#include "audio_data.hpp"
#include "debug.hpp"
#include "game.hpp"
#include "map.hpp"
#include "map_parsing.hpp"
#include "msg_log.hpp"
#include "player_bon.hpp"
#include "popup.hpp"
#include "pos.hpp"
#include "property.hpp"
#include "property_data.hpp"
#include "property_factory.hpp"
#include "property_handler.hpp"
#include "random.hpp"
#include "saving.hpp"
#include "sound.hpp"
#include "terrain_data.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// Insanity symptoms
// -----------------------------------------------------------------------------
void InsSympt::on_start()
{
        msg_log::more_prompt();

        const std::string heading = start_heading();

        const std::string msg = "Insanity draws nearer... " + start_msg();

        ASSERT(!heading.empty() && !msg.empty());

        if (!heading.empty() && !msg.empty())
        {
                popup::Popup(popup::AddToMsgHistory::yes)
                        .set_title(heading)
                        .set_msg(msg)
                        .set_sfx(audio::SfxId::insanity_rising)
                        .run();
        }

        const std::string history_event_msg = history_msg();

        ASSERT(!history_event_msg.empty());

        if (!history_event_msg.empty())
        {
                game::add_history_event(history_event_msg);
        }

        on_start_hook();
}

void InsSympt::on_end()
{
        const std::string msg = end_msg();

        ASSERT(!msg.empty());

        if (!msg.empty())
        {
                msg_log::add(msg);
        }

        const std::string history_event_msg = history_msg_end();

        ASSERT(!history_event_msg.empty());

        if (!history_event_msg.empty())
        {
                game::add_history_event(history_event_msg);
        }
}

bool InsReduceXp::is_allowed() const
{
        return game::xp_pct() >= 25;
}

void InsReduceXp::on_start_hook()
{
        game::decr_player_xp(25);
}

std::string InsReduceXp::start_msg() const
{
        return "Thanks to the mercy of the mind, some past experiences are "
               "forgotten (-25% XP).";
}

bool InsScream::is_allowed() const
{
        return !map::g_player->m_properties.has(PropId::r_fear);
}

void InsScream::on_start_hook()
{
        Snd snd("",
                audio::SfxId::END,
                IgnoreMsgIfOriginSeen::yes,
                map::g_player->m_pos,
                map::g_player,
                SndVol::high,
                AlertsMon::yes);

        snd_emit::run(snd);
}

std::string InsScream::start_msg() const
{
        if (rnd::coin_toss())
        {
                return "I let out a terrified shriek.";
        }
        else
        {
                return "I scream in terror.";
        }
}

void InsBabbling::babble() const
{
        const std::string player_name = map::g_player->name_the();

        for (int i = rnd::range(1, 3); i > 0; --i)
        {
                msg_log::add(player_name + ": " + actor::get_cultist_phrase());
        }

        Snd snd("",
                audio::SfxId::END,
                IgnoreMsgIfOriginSeen::yes,
                map::g_player->m_pos,
                map::g_player,
                SndVol::low,
                AlertsMon::yes);

        snd_emit::run(snd);
}

void InsBabbling::on_start_hook()
{
        babble();
}

void InsBabbling::on_new_player_turn(
        const std::vector<actor::Actor*>& seen_foes)
{
        (void)seen_foes;

        const int babble_on_in_n = 200;

        if (rnd::one_in(babble_on_in_n))
        {
                babble();
        }
}

bool InsFaint::is_allowed() const
{
        return true;
}

void InsFaint::on_start_hook()
{
        map::g_player->m_properties.apply(
                property_factory::make(PropId::fainted));
}

void InsLaugh::on_start_hook()
{
        Snd snd("",
                audio::SfxId::END,
                IgnoreMsgIfOriginSeen::yes,
                map::g_player->m_pos,
                map::g_player,
                SndVol::low,
                AlertsMon::yes);

        snd_emit::run(snd);
}

bool InsPhobiaRat::is_allowed() const
{
        const bool has_phobia = insanity::has_sympt_type(InsSymptType::phobia);
        const bool is_rfear = map::g_player->m_properties.has(PropId::r_fear);

        return !is_rfear && (!has_phobia || rnd::one_in(20));
}

void InsPhobiaRat::on_new_player_turn(
        const std::vector<actor::Actor*>& seen_foes)
{
        if (!rnd::one_in(10))
        {
                return;
        }

        for (auto* const actor : seen_foes)
        {
                if (actor->m_data->is_rat)
                {
                        msg_log::add("I am plagued by my phobia of rats!");

                        map::g_player->m_properties.apply(
                                property_factory::make(PropId::terrified));

                        break;
                }
        }
}

void InsPhobiaRat::on_permanent_rfear()
{
        insanity::end_sympt(id());
}

bool InsPhobiaSpider::is_allowed() const
{
        const bool has_phobia = insanity::has_sympt_type(InsSymptType::phobia);
        const bool is_rfear = map::g_player->m_properties.has(PropId::r_fear);

        return !is_rfear && (!has_phobia || rnd::one_in(20));
}

void InsPhobiaSpider::on_new_player_turn(
        const std::vector<actor::Actor*>& seen_foes)
{
        if (!rnd::one_in(10))
        {
                return;
        }

        for (auto* const actor : seen_foes)
        {
                if (actor->m_data->is_spider)
                {
                        msg_log::add("I am plagued by my phobia of spiders!");

                        map::g_player->m_properties.apply(
                                property_factory::make(PropId::terrified));

                        break;
                }
        }
}

void InsPhobiaSpider::on_permanent_rfear()
{
        insanity::end_sympt(id());
}

bool InsPhobiaReptileAndAmph::is_allowed() const
{
        const bool has_phobia = insanity::has_sympt_type(InsSymptType::phobia);
        const bool is_rfear = map::g_player->m_properties.has(PropId::r_fear);

        return !is_rfear && (!has_phobia || rnd::one_in(20));
}

void InsPhobiaReptileAndAmph::on_new_player_turn(
        const std::vector<actor::Actor*>& seen_foes)
{
        if (!rnd::one_in(10))
        {
                return;
        }

        bool is_triggered = false;

        std::string animal_str;

        for (auto* const actor : seen_foes)
        {
                if (actor->m_data->is_reptile)
                {
                        animal_str = "reptiles";

                        is_triggered = true;

                        break;
                }

                if (actor->m_data->is_amphibian)
                {
                        animal_str = "amphibians";

                        is_triggered = true;

                        break;
                }
        }

        if (is_triggered)
        {
                msg_log::add("I am plagued by my phobia of " + animal_str + "!");

                map::g_player->m_properties.apply(
                        property_factory::make(PropId::terrified));
        }
}

void InsPhobiaReptileAndAmph::on_permanent_rfear()
{
        insanity::end_sympt(id());
}

bool InsPhobiaCanine::is_allowed() const
{
        const bool has_phobia = insanity::has_sympt_type(InsSymptType::phobia);
        const bool is_rfear = map::g_player->m_properties.has(PropId::r_fear);

        return !is_rfear && (!has_phobia || rnd::one_in(20));
}

void InsPhobiaCanine::on_new_player_turn(
        const std::vector<actor::Actor*>& seen_foes)
{
        if (!rnd::one_in(10))
        {
                return;
        }

        for (auto* const actor : seen_foes)
        {
                if (actor->m_data->is_canine)
                {
                        msg_log::add("I am plagued by my phobia of canines!");

                        map::g_player->m_properties.apply(
                                property_factory::make(PropId::terrified));

                        break;
                }
        }
}

void InsPhobiaCanine::on_permanent_rfear()
{
        insanity::end_sympt(id());
}

bool InsPhobiaDead::is_allowed() const
{
        const bool has_phobia = insanity::has_sympt_type(InsSymptType::phobia);
        const bool is_rfear = map::g_player->m_properties.has(PropId::r_fear);

        return !is_rfear && (!has_phobia || rnd::one_in(20));
}

void InsPhobiaDead::on_new_player_turn(
        const std::vector<actor::Actor*>& seen_foes)
{
        if (!rnd::one_in(10))
        {
                return;
        }

        for (auto* const actor : seen_foes)
        {
                if (actor->m_data->is_undead)
                {
                        msg_log::add("I am plagued by my phobia of the dead!");

                        map::g_player->m_properties.apply(
                                property_factory::make(PropId::terrified));

                        break;
                }
        }
}

void InsPhobiaDead::on_permanent_rfear()
{
        insanity::end_sympt(id());
}

bool InsPhobiaDeep::is_allowed() const
{
        const bool has_phobia = insanity::has_sympt_type(InsSymptType::phobia);

        const bool is_rfear = map::g_player->m_properties.has(PropId::r_fear);

        return !is_rfear && (!has_phobia || rnd::one_in(20));
}

void InsPhobiaDeep::on_new_player_turn(
        const std::vector<actor::Actor*>& seen_foes)
{
        (void)seen_foes;

        if (!rnd::one_in(10))
        {
                return;
        }

        const std::vector<terrain::Id> deep_terrains = {
                terrain::Id::chasm};

        const auto parser =
                map_parsers::AnyAdjIsAnyOfTerrains(
                        deep_terrains);

        if (parser.run(map::g_player->m_pos))
        {
                msg_log::add("I am plagued by my phobia of deep places!");

                map::g_player->m_properties.apply(
                        property_factory::make(PropId::terrified));
        }
}

void InsPhobiaDeep::on_permanent_rfear()
{
        insanity::end_sympt(id());
}

bool InsPhobiaDark::is_allowed() const
{
        const bool has_phobia = insanity::has_sympt_type(InsSymptType::phobia);
        const bool is_rfear = map::g_player->m_properties.has(PropId::r_fear);

        return (
                !player_bon::is_bg(Bg::ghoul) &&
                !is_rfear &&
                (!has_phobia || rnd::one_in(20)));
}

void InsPhobiaDark::on_new_player_turn(
        const std::vector<actor::Actor*>& seen_foes)
{
        (void)seen_foes;

        if (rnd::one_in(10))
        {
                const P p(map::g_player->m_pos);
                const PropHandler& props = map::g_player->m_properties;

                if ((props.allow_act() && !props.allow_see()) ||
                    (map::g_dark.at(p) && !map::g_light.at(p)))
                {
                        msg_log::add("I am plagued by my phobia of the dark!");

                        map::g_player->m_properties.apply(
                                property_factory::make(PropId::terrified));
                }
        }
}

void InsPhobiaDark::on_permanent_rfear()
{
        insanity::end_sympt(id());
}

bool InsSadism::is_allowed() const
{
        return rnd::one_in(4);
}

void InsShadows::on_start_hook()
{
        TRACE_FUNC_BEGIN;

        const int nr_shadows_min = 2;

        const int nr_shadows_max =
                std::clamp(
                        map::g_dlvl - 2,
                        nr_shadows_min,
                        8);

        const size_t nr = rnd::range(nr_shadows_min, nr_shadows_max);

        auto summoned =
                actor::spawn(
                        map::g_player->m_pos,
                        {nr, actor::Id::shadow},
                        map::rect())
                        .make_aware_of_player();

        std::for_each(
                std::begin(summoned.monsters),
                std::end(summoned.monsters),
                [](auto* const mon) {
                        auto* prop = property_factory::make(PropId::waiting);

                        prop->set_duration(1);

                        mon->m_properties.apply(prop);

                        mon->m_mon_aware_state.player_aware_of_me_counter = 0;
                });

        map::update_vision();

        const auto player_seen_foes = actor::seen_foes(*map::g_player);

        for (auto* const actor : player_seen_foes)
        {
                static_cast<actor::Mon*>(actor)->set_player_aware_of_me();
        }

        TRACE_FUNC_END;
}

void InsParanoia::on_start_hook()
{
        // Flip a coint to decide if we should spawn a stalker or not
        // (Maybe it's just paranoia, or maybe it's real)
        if (rnd::coin_toss())
        {
                return;
        }

        std::vector<actor::Id> stalker_id(1, actor::Id::invis_stalker);

        const P& pos = map::g_player->m_pos;

        auto summoned =
                actor::spawn(
                        pos,
                        {actor::Id::invis_stalker},
                        map::rect())
                        .make_aware_of_player();

        std::for_each(
                std::begin(summoned.monsters),
                std::end(summoned.monsters),
                [](auto* const mon) {
                        auto* prop = property_factory::make(PropId::waiting);

                        prop->set_duration(1);

                        mon->m_properties.apply(prop);
                });
}

bool InsConfusion::is_allowed() const
{
        return !map::g_player->m_properties.has(PropId::r_conf);
}

void InsConfusion::on_start_hook()
{
        map::g_player->m_properties.apply(
                property_factory::make(PropId::confused));
}

bool InsFrenzy::is_allowed() const
{
        return true;
}

void InsFrenzy::on_start_hook()
{
        map::g_player->m_properties.apply(
                property_factory::make(PropId::frenzied));
}

// -----------------------------------------------------------------------------
// Insanity handling
// -----------------------------------------------------------------------------
namespace insanity
{
namespace
{
InsSympt* s_sympts[(size_t)InsSymptId::END];

InsSympt* make_sympt(const InsSymptId id)
{
        switch (id)
        {
        case InsSymptId::reduce_xp:
                return new InsReduceXp();

        case InsSymptId::scream:
                return new InsScream();

        case InsSymptId::babbling:
                return new InsBabbling();

        case InsSymptId::faint:
                return new InsFaint();

        case InsSymptId::laugh:
                return new InsLaugh();

        case InsSymptId::phobia_rat:
                return new InsPhobiaRat();

        case InsSymptId::phobia_spider:
                return new InsPhobiaSpider();

        case InsSymptId::phobia_reptile_and_amph:
                return new InsPhobiaReptileAndAmph();

        case InsSymptId::phobia_canine:
                return new InsPhobiaCanine();

        case InsSymptId::phobia_dead:
                return new InsPhobiaDead();

        case InsSymptId::phobia_deep:
                return new InsPhobiaDeep();

        case InsSymptId::phobia_dark:
                return new InsPhobiaDark();

        case InsSymptId::sadism:
                return new InsSadism();

        case InsSymptId::shadows:
                return new InsShadows();

        case InsSymptId::paranoia:
                return new InsParanoia();

        case InsSymptId::confusion:
                return new InsConfusion();

        case InsSymptId::frenzy:
                return new InsFrenzy();

        case InsSymptId::strange_sensation:
                return new InsStrangeSensation();

        case InsSymptId::END:
                break;
        }

        ASSERT(false);

        return nullptr;
}

}  // namespace

void init()
{
        for (size_t i = 0; i < (size_t)InsSymptId::END; ++i)
        {
                s_sympts[i] = nullptr;
        }
}

void cleanup()
{
        for (size_t i = 0; i < (size_t)InsSymptId::END; ++i)
        {
                delete s_sympts[i];

                s_sympts[i] = nullptr;
        }
}

void save()
{
        for (size_t i = 0; i < (size_t)InsSymptId::END; ++i)
        {
                const auto* const sympt = s_sympts[i];

                saving::put_bool(sympt != nullptr);

                if (sympt)
                {
                        sympt->save();
                }
        }
}

void load()
{
        for (size_t i = 0; i < (size_t)InsSymptId::END; ++i)
        {
                const bool has_symptom = saving::get_bool();

                if (has_symptom)
                {
                        auto* const sympt = make_sympt((InsSymptId)i);

                        s_sympts[i] = sympt;

                        sympt->load();
                }
        }
}

void run_sympt()
{
        std::vector<InsSympt*> sympt_bucket;

        for (size_t i = 0; i < (size_t)InsSymptId::END; ++i)
        {
                const InsSympt* const active_sympt = s_sympts[i];

                // Symptoms are only allowed if not already active
                if (!active_sympt)
                {
                        InsSympt* const new_sympt = make_sympt(InsSymptId(i));

                        const bool is_allowed = new_sympt->is_allowed();

                        if (is_allowed)
                        {
                                sympt_bucket.push_back(new_sympt);
                        }
                        else
                        {
                                delete new_sympt;
                        }
                }
        }

        if (sympt_bucket.empty())
        {
                // This should never happen, since there are symptoms which can
                // occur repeatedly unconditionally - but we do a check anyway
                // for robustness.
                return;
        }

        const auto bucket_idx = rnd::range(0, (int)sympt_bucket.size() - 1);

        auto* const sympt = sympt_bucket[bucket_idx];

        sympt_bucket.erase(std::begin(sympt_bucket) + bucket_idx);

        // Delete the remaining symptoms in the bucket
        for (auto* const other_sympt : sympt_bucket)
        {
                delete other_sympt;
        }

        // If the symptom is permanent (i.e. not a one-shot thing like
        // screaming), set it as active in the symptoms list
        if (sympt->is_permanent())
        {
                const auto sympt_idx = size_t(sympt->id());

                ASSERT(!s_sympts[sympt_idx]);

                s_sympts[sympt_idx] = sympt;
        }

        sympt->on_start();
}

bool has_sympt(const InsSymptId id)
{
        ASSERT(id != InsSymptId::END);

        return s_sympts[size_t(id)];
}

bool has_sympt_type(const InsSymptType type)
{
        for (size_t i = 0; i < (size_t)InsSymptId::END; ++i)
        {
                const InsSympt* const s = s_sympts[i];

                if (s && s->type() == type)
                {
                        return true;
                }
        }

        return false;
}

std::vector<const InsSympt*> active_sympts()
{
        std::vector<const InsSympt*> out;

        for (size_t i = 0; i < (size_t)InsSymptId::END; ++i)
        {
                const InsSympt* const sympt = s_sympts[i];

                if (sympt)
                {
                        out.push_back(sympt);
                }
        }

        return out;
}

void on_new_player_turn(const std::vector<actor::Actor*>& seen_foes)
{
        for (size_t i = 0; i < (size_t)InsSymptId::END; ++i)
        {
                InsSympt* const sympt = s_sympts[i];

                if (sympt)
                {
                        sympt->on_new_player_turn(seen_foes);
                }
        }
}

void on_permanent_rfear()
{
        for (size_t i = 0; i < (size_t)InsSymptId::END; ++i)
        {
                InsSympt* const sympt = s_sympts[i];

                if (sympt)
                {
                        sympt->on_permanent_rfear();
                }
        }
}

void end_sympt(const InsSymptId id)
{
        ASSERT(id != InsSymptId::END);

        const auto idx = size_t(id);
        auto* const sympt = s_sympts[idx];

        ASSERT(sympt);

        s_sympts[idx] = nullptr;

        sympt->on_end();

        delete sympt;
}

}  // namespace insanity

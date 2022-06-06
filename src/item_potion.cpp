// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "item_potion.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <memory>
#include <optional>
#include <ostream>

#include "actor.hpp"
#include "actor_see.hpp"
#include "array2.hpp"
#include "audio.hpp"
#include "audio_data.hpp"
#include "common_text.hpp"
#include "config.hpp"
#include "debug.hpp"
#include "game.hpp"
#include "game_time.hpp"
#include "insanity.hpp"
#include "inventory_handling.hpp"
#include "item_data.hpp"
#include "item_factory.hpp"
#include "map.hpp"
#include "map_parsing.hpp"
#include "msg_log.hpp"
#include "property.hpp"
#include "property_data.hpp"
#include "property_factory.hpp"
#include "property_handler.hpp"
#include "query.hpp"
#include "random.hpp"
#include "saving.hpp"
#include "state.hpp"
#include "terrain.hpp"
#include "terrain_data.hpp"
#include "text_format.hpp"

struct P;

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
struct PotionAppearance
{
        std::string name_plain;
        std::string name_a;
        Color color;
};

static std::vector<PotionAppearance> s_potion_appearances;

// -----------------------------------------------------------------------------
// potion
// -----------------------------------------------------------------------------
namespace potion
{
void init()
{
        TRACE_FUNC_BEGIN;

        // Init possible potion colors and fake names
        s_potion_appearances.assign(
                {{"Golden", "a Golden", colors::yellow()},
                 {"Yellow", "a Yellow", colors::yellow()},
                 {"Dark", "a Dark", colors::gray()},
                 {"Black", "a Black", colors::gray()},
                 {"Oily", "an Oily", colors::gray()},
                 {"Smoky", "a Smoky", colors::white()},
                 {"Slimy", "a Slimy", colors::green()},
                 {"Green", "a Green", colors::light_green()},
                 {"Fiery", "a Fiery", colors::light_red()},
                 {"Murky", "a Murky", colors::dark_brown()},
                 {"Muddy", "a Muddy", colors::brown()},
                 {"Violet", "a Violet", colors::violet()},
                 {"Orange", "an Orange", colors::orange()},
                 {"Watery", "a Watery", colors::light_blue()},
                 {"Metallic", "a Metallic", colors::gray()},
                 {"Clear", "a Clear", colors::light_white()},
                 {"Misty", "a Misty", colors::light_white()},
                 {"Bloody", "a Bloody", colors::red()},
                 {"Magenta", "a Magenta", colors::magenta()},
                 {"Clotted", "a Clotted", colors::green()},
                 {"Moldy", "a Moldy", colors::brown()},
                 {"Frothy", "a Frothy", colors::white()}});

        for (auto& d : item::g_data)
        {
                if (d.type != ItemType::potion)
                {
                        continue;
                }

                // Color and false name
                const size_t idx =
                        rnd::range(0, (int)s_potion_appearances.size() - 1);

                auto& look = s_potion_appearances[idx];

                d.base_name_un_id.names[(size_t)ItemNameType::plain] =
                        look.name_plain + " Potion";

                d.base_name_un_id.names[(size_t)ItemNameType::plural] =
                        look.name_plain + " Potions";

                d.base_name_un_id.names[(size_t)ItemNameType::a] =
                        look.name_a + " Potion";

                d.color = look.color;

                s_potion_appearances.erase(
                        std::begin(s_potion_appearances) + (int)idx);

                // True name
                const auto* const potion =
                        static_cast<const Potion*>(
                                item::make(d.id, 1));

                const std::string real_type_name = potion->real_name();

                delete potion;

                const std::string real_name =
                        "Potion of " + real_type_name;

                const std::string real_name_plural =
                        "Potions of " + real_type_name;

                const std::string real_name_a =
                        "a Potion of " + real_type_name;

                d.base_name.names[(size_t)ItemNameType::plain] =
                        real_name;

                d.base_name.names[(size_t)ItemNameType::plural] =
                        real_name_plural;

                d.base_name.names[(size_t)ItemNameType::a] =
                        real_name_a;
        }

        TRACE_FUNC_END;
}

void save()
{
        for (int i = 0; i < (int)item::Id::END; ++i)
        {
                auto& d = item::g_data[i];

                if (d.type != ItemType::potion)
                {
                        continue;
                }

                saving::put_str(
                        d.base_name_un_id.names[(size_t)ItemNameType::plain]);

                saving::put_str(
                        d.base_name_un_id.names[(size_t)ItemNameType::plural]);

                saving::put_str(
                        d.base_name_un_id.names[(size_t)ItemNameType::a]);

                saving::put_str(colors::color_to_name(d.color));
        }
}

void load()
{
        for (int i = 0; i < (int)item::Id::END; ++i)
        {
                auto& d = item::g_data[i];

                if (d.type != ItemType::potion)
                {
                        continue;
                }

                d.base_name_un_id.names[(size_t)ItemNameType::plain] =
                        saving::get_str();

                d.base_name_un_id.names[(size_t)ItemNameType::plural] =
                        saving::get_str();

                d.base_name_un_id.names[(size_t)ItemNameType::a] =
                        saving::get_str();

                d.color =
                        colors::name_to_color(saving::get_str()).value();
        }
}

Potion::Potion(item::ItemData* const item_data) :
        Item(item_data),
        m_alignment_feeling_dlvl_countdown(rnd::range(1, 3)),
        m_alignment_feeling_turn_countdown(rnd::range(100, 200))
{
}

void Potion::save_hook() const
{
        saving::put_int(m_alignment_feeling_dlvl_countdown);
        saving::put_int(m_alignment_feeling_turn_countdown);
}

void Potion::load_hook()
{
        m_alignment_feeling_dlvl_countdown = saving::get_int();
        m_alignment_feeling_turn_countdown = saving::get_int();
}

ConsumeItem Potion::activate(actor::Actor* const actor)
{
        ASSERT(actor);

        if (!actor->m_properties.allow_eat(Verbose::yes))
        {
                return ConsumeItem::no;
        }

        if (actor::is_player(actor))
        {
                // Really quaff a known malign potion?
                if ((alignment() == PotionAlignment::bad) &&
                    m_data->is_alignment_known &&
                    config::warn_on_drink_malign_potion())
                {
                        const std::string name = this->name(ItemNameType::a);

                        const std::string msg =
                                "Drink " +
                                name +
                                "? " +
                                common_text::g_yes_or_no_hint;

                        msg_log::add(
                                msg,
                                colors::light_white(),
                                MsgInterruptPlayer::no,
                                MorePromptOnMsg::no,
                                CopyToMsgHistory::no);

                        auto result = query::yes_or_no();

                        msg_log::clear();

                        if (result == BinaryAnswer::no)
                        {
                                return ConsumeItem::no;
                        }
                }

                m_data->is_tried = true;

                audio::play(audio::SfxId::potion_quaff);

                if (m_data->is_identified)
                {
                        const std::string potion_name =
                                name(ItemNameType::a, ItemNameInfo::none);

                        msg_log::add("I drink " + potion_name + "...");
                }
                else
                {
                        // Not identified
                        const std::string potion_name =
                                name(ItemNameType::plain, ItemNameInfo::none);

                        msg_log::add(
                                "I drink an unknown " + potion_name + "...");
                }

                map::g_player->incr_shock(12.0, ShockSrc::use_strange_item);

                if (!map::g_player->is_alive())
                {
                        return ConsumeItem::yes;
                }
        }

        quaff_impl(*actor);

        if (map::g_player->is_alive())
        {
                game_time::tick();
        }

        return ConsumeItem::yes;
}

void Potion::identify(const Verbose verbose)
{
        if (m_data->is_identified)
        {
                return;
        }

        m_data->is_identified = true;

        m_data->is_alignment_known = true;

        if (verbose == Verbose::yes)
        {
                const std::string name_after =
                        name(ItemNameType::a, ItemNameInfo::none);

                msg_log::add("I have identified " + name_after + ".");

                game::add_history_event("Identified " + name_after);
        }
}

std::vector<std::string> Potion::descr_hook() const
{
        if (m_data->is_identified)
        {
                return {descr_identified()};
        }
        else
        {
                auto lines = m_data->base_descr;

                if (m_data->is_alignment_known)
                {
                        lines.push_back(
                                "This potion is " +
                                text_format::first_to_lower(alignment_str()) +
                                ".");
                }
                else
                {
                        lines.emplace_back(
                                "Perhaps keeping it for a while will reveal "
                                "something about it.");
                }

                return lines;
        }
}

std::string Potion::alignment_str() const
{
        return (alignment() == PotionAlignment::good)
                ? "Benign"
                : "Malign";
}

void Potion::on_player_reached_new_dlvl_hook()
{
        auto& d = data();

        if (d.is_alignment_known ||
            d.is_identified ||
            (m_alignment_feeling_dlvl_countdown <= 0))
        {
                return;
        }

        --m_alignment_feeling_dlvl_countdown;
}

void Potion::on_actor_turn_in_inv_hook(const InvType inv_type)
{
        (void)inv_type;

        if (!actor::is_player(m_actor_carrying))
        {
                return;
        }

        auto& d = data();

        if (d.is_alignment_known ||
            d.is_identified ||
            (m_alignment_feeling_dlvl_countdown > 0) ||
            !map::g_player->m_properties.allow_act())
        {
                return;
        }

        ASSERT(m_alignment_feeling_turn_countdown > 0);

        --m_alignment_feeling_turn_countdown;

        if (m_alignment_feeling_turn_countdown <= 0)
        {
                TRACE << "Potion alignment discovered" << std::endl;

                const std::string name_plural =
                        d.base_name_un_id.names[(size_t)ItemNameType::plural];

                const std::string align_str =
                        text_format::first_to_lower(alignment_str());

                msg_log::add(
                        std::string(
                                "I feel like " +
                                name_plural +
                                " are " +
                                align_str +
                                "."),
                        colors::text(),
                        MsgInterruptPlayer::no,
                        MorePromptOnMsg::yes);

                d.is_alignment_known = true;
        }
}

void Potion::on_collide(const P& pos, actor::Actor* const actor)
{
        const bool is_seen = map::g_seen.at(pos);
        const auto* const terrain = map::g_terrain.at(pos);

        if (actor)
        {
                ASSERT(actor->is_alive());

                if (actor->is_player_aware_of_me())
                {
                        const std::string actor_name =
                                actor::can_player_see_actor(*actor)
                                ? actor->name_the()
                                : "it";

                        msg_log::add(
                                "The potion shatters on " +
                                actor_name +
                                ".");

                        actor->make_player_aware_of_me();
                }

                collide_hook(pos, actor);
        }
        else if (is_seen)
        {
                const std::vector<terrain::Id> deep_terrains = {
                        terrain::Id::chasm};

                if (!map_parsers::IsAnyOfTerrains(deep_terrains).run(pos))
                {
                        msg_log::add(
                                "The potion shatters on " +
                                terrain->name(Article::the) +
                                ".");
                }
        }
}

std::string Potion::name_info_str() const
{
        std::string str;

        if (data().is_alignment_known && !data().is_identified)
        {
                str = "(" + alignment_str() + ")";
        }

        return str;
}

void Vitality::quaff_impl(actor::Actor& actor)
{
        std::vector<PropId> props_can_heal = {
                PropId::blind,
                PropId::deaf,
                PropId::poisoned,
                PropId::infected,
                PropId::diseased,
                PropId::weakened,
                PropId::hp_sap,
                PropId::wound};

        for (PropId prop_id : props_can_heal)
        {
                actor.m_properties.end_prop(prop_id);
        }

        actor.restore_hp(999, false);

        actor.m_properties.apply(property_factory::make(PropId::regenerating));

        if (actor::can_player_see_actor(actor))
        {
                identify(Verbose::yes);
        }
}

void Vitality::collide_hook(const P& pos, actor::Actor* const actor)
{
        (void)pos;

        if (actor)
        {
                quaff_impl(*actor);
        }
}

void Spirit::quaff_impl(actor::Actor& actor)
{
        actor.m_properties.end_prop(PropId::spi_sap);

        // SP is always restored at least up to maximum, but can go beyond
        const int sp_max = actor::max_sp(actor);

        const int sp_restored = std::max(10, sp_max - actor.m_sp);

        actor.restore_sp(sp_restored, true);

        if (actor::can_player_see_actor(actor))
        {
                identify(Verbose::yes);
        }
}

void Spirit::collide_hook(const P& pos, actor::Actor* const actor)
{
        (void)pos;

        if (actor)
        {
                quaff_impl(*actor);
        }
}

void Blindness::quaff_impl(actor::Actor& actor)
{
        actor.m_properties.apply(
                property_factory::make(PropId::blind));

        if (actor::can_player_see_actor(actor))
        {
                identify(Verbose::yes);
        }
}

void Blindness::collide_hook(const P& pos, actor::Actor* const actor)
{
        (void)pos;

        if (actor)
        {
                quaff_impl(*actor);
        }
}

void Paral::quaff_impl(actor::Actor& actor)
{
        actor.m_properties.apply(new PropParalyzed());

        if (actor::can_player_see_actor(actor))
        {
                identify(Verbose::yes);
        }
}

void Paral::collide_hook(const P& pos, actor::Actor* const actor)
{
        (void)pos;

        if (actor)
        {
                quaff_impl(*actor);
        }
}

void Disease::quaff_impl(actor::Actor& actor)
{
        actor.m_properties.apply(new PropDiseased());

        if (actor::can_player_see_actor(actor))
        {
                identify(Verbose::yes);
        }
}

void Conf::quaff_impl(actor::Actor& actor)
{
        actor.m_properties.apply(new PropConfused());

        if (actor::can_player_see_actor(actor))
        {
                identify(Verbose::yes);
        }
}

void Conf::collide_hook(const P& pos, actor::Actor* const actor)
{
        (void)pos;

        if (actor)
        {
                quaff_impl(*actor);
        }
}

void Fortitude::quaff_impl(actor::Actor& actor)
{
        auto* prop_r_fear = property_factory::make(PropId::r_fear);
        auto* prop_r_conf = property_factory::make(PropId::r_conf);
        auto* prop_r_sleep = property_factory::make(PropId::r_sleep);

        // The duration of the last two properties is decided by the randomized
        // duration of the first property
        const int duration = prop_r_fear->nr_turns_left();

        prop_r_conf->set_duration(duration);
        prop_r_sleep->set_duration(duration);

        actor.m_properties.apply(prop_r_fear);
        actor.m_properties.apply(prop_r_conf);
        actor.m_properties.apply(prop_r_sleep);

        actor.m_properties.end_prop(PropId::frenzied);
        actor.m_properties.end_prop(PropId::hallucinating);
        actor.m_properties.end_prop(PropId::mind_sap);

        if (actor::is_player(&actor))
        {
                const std::vector<const InsSympt*> sympts =
                        insanity::active_sympts();

                if (!sympts.empty())
                {
                        const auto id = rnd::element(sympts)->id();

                        insanity::end_sympt(id);
                }

                map::g_player->restore_shock(999, false);

                msg_log::add("I feel more at ease.");
        }

        if (actor::can_player_see_actor(actor))
        {
                identify(Verbose::yes);
        }
}

void Fortitude::collide_hook(const P& pos, actor::Actor* const actor)
{
        (void)pos;

        if (actor)
        {
                quaff_impl(*actor);
        }
}

void Poison::quaff_impl(actor::Actor& actor)
{
        actor.m_properties.apply(new PropPoisoned());

        if (actor::can_player_see_actor(actor))
        {
                identify(Verbose::yes);
        }
}

void Poison::collide_hook(const P& pos, actor::Actor* const actor)
{
        (void)pos;

        if (actor)
        {
                quaff_impl(*actor);
        }
}

void RFire::quaff_impl(actor::Actor& actor)
{
        actor.m_properties.apply(property_factory::make(PropId::r_fire));

        if (actor::can_player_see_actor(actor))
        {
                identify(Verbose::yes);
        }
}

void RFire::collide_hook(const P& pos, actor::Actor* const actor)
{
        (void)pos;

        if (actor)
        {
                quaff_impl(*actor);
        }
}

void Curing::quaff_impl(actor::Actor& actor)
{
        std::vector<PropId> props_can_heal = {
                PropId::blind,
                PropId::deaf,
                PropId::poisoned,
                PropId::infected,
                PropId::diseased,
                PropId::weakened,
                PropId::hp_sap};

        bool is_noticable = false;

        for (PropId prop_id : props_can_heal)
        {
                if (actor.m_properties.end_prop(prop_id))
                {
                        is_noticable = true;
                }
        }

        if (actor.restore_hp(3, false))
        {
                is_noticable = true;
        }

        if (!is_noticable && actor::is_player(&actor))
        {
                msg_log::add("I feel fine.");

                is_noticable = true;
        }

        if (is_noticable &&
            actor::can_player_see_actor(actor))
        {
                identify(Verbose::yes);
        }
}

void Curing::collide_hook(const P& pos, actor::Actor* const actor)
{
        (void)pos;

        if (actor)
        {
                quaff_impl(*actor);
        }
}

void RElec::quaff_impl(actor::Actor& actor)
{
        actor.m_properties.apply(new PropRElec());

        if (actor::can_player_see_actor(actor))
        {
                identify(Verbose::yes);
        }
}

void RElec::collide_hook(const P& pos, actor::Actor* const actor)
{
        (void)pos;

        if (actor)
        {
                quaff_impl(*actor);
        }
}

void Insight::quaff_impl(actor::Actor& actor)
{
        (void)actor;

        identify(Verbose::yes);

        // Run identify selection menu

        // NOTE: We push this state BEFORE giving any XP (directly or via
        // identifying stuff), because if the player gains a new level in the
        // process, the trait selection should occur first
        states::push(std::make_unique<SelectIdentify>());

        // Insight gives some extra XP, to avoid making the potion worthless if
        // the player identifies all items)
        msg_log::add("I feel insightful.");

        game::incr_player_xp(5);

        msg_log::more_prompt();
}

void Descent::quaff_impl(actor::Actor& actor)
{
        (void)actor;

        if (map::g_dlvl < (g_dlvl_last - 1))
        {
                if (!map::g_player->m_properties.has(PropId::descend))
                {
                        map::g_player->m_properties.apply(new PropDescend());
                }
        }
        else
        {
                // Dungeon level is near the end
                msg_log::add(
                        "I feel a faint sinking sensation, "
                        "but it soon disappears...");
        }

        identify(Verbose::yes);
}

void Invis::quaff_impl(actor::Actor& actor)
{
        actor.m_properties.apply(property_factory::make(PropId::cloaked));

        if (actor::can_player_see_actor(actor))
        {
                identify(Verbose::yes);
        }
}

void Invis::collide_hook(const P& pos, actor::Actor* const actor)
{
        (void)pos;

        if (actor)
        {
                quaff_impl(*actor);
        }
}

}  // namespace potion

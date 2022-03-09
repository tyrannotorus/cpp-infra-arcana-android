// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "terrain_pylon.hpp"

#include <ostream>

#include "actor.hpp"
#include "actor_see.hpp"
#include "array2.hpp"
#include "colors.hpp"
#include "common_text.hpp"
#include "debug.hpp"
#include "direction.hpp"
#include "game.hpp"
#include "game_time.hpp"
#include "gfx.hpp"
#include "global.hpp"
#include "knockback.hpp"
#include "map.hpp"
#include "misc.hpp"
#include "msg_log.hpp"
#include "property.hpp"
#include "property_data.hpp"
#include "property_factory.hpp"
#include "property_handler.hpp"
#include "query.hpp"
#include "random.hpp"
#include "saving.hpp"
#include "teleport.hpp"
#include "text_format.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
struct PylonAppearance
{
        void save() const
        {
                saving::put_str(name_plain);
                saving::put_str(name_a);
                saving::put_int(color.r());
                saving::put_int(color.g());
                saving::put_int(color.b());
                saving::put_int((int)tile);
        }

        void load()
        {
                name_plain = saving::get_str();
                name_a = saving::get_str();

                const int r = saving::get_int();
                const int g = saving::get_int();
                const int b = saving::get_int();

                color.set_rgb(r, g, b);

                tile = (gfx::TileId)saving::get_int();
        }

        std::string name_plain {};
        std::string name_a {};
        Color color {};
        gfx::TileId tile {};
};

static const auto s_nr_pylon_types = (size_t)terrain::pylon::PylonId::END;
static PylonAppearance s_id_to_appearance[s_nr_pylon_types] {};
static bool s_is_identified[s_nr_pylon_types] {false};

static PylonAppearance get_appearance_for_id(const terrain::pylon::PylonId id)
{
        if (id == terrain::pylon::PylonId::END)
        {
                ASSERT(false);

                return {};
        }

        return s_id_to_appearance[(size_t)id];
}

static bool is_identified(const terrain::pylon::PylonId id)
{
        return s_is_identified[(size_t)id];
}

static std::string get_fake_name(
        const terrain::pylon::PylonId id,
        const Article article)
{
        const auto& appearance = get_appearance_for_id(id);

        if (article == Article::a)
        {
                return appearance.name_a;
        }
        else
        {
                return "the " + appearance.name_plain;
        }
}

// -----------------------------------------------------------------------------
// terrain
// -----------------------------------------------------------------------------
namespace terrain
{
// -----------------------------------------------------------------------------
// pylon
// -----------------------------------------------------------------------------
namespace pylon
{
void init()
{
        TRACE_FUNC_BEGIN;

        std::vector<PylonAppearance> appearances {
                {"Angled Pylon",
                 "an Angled Pylon",
                 colors::light_sepia(),
                 gfx::TileId::pylon_angled},
                {"Arched Pylon",
                 "an Arched Pylon",
                 colors::light_sepia(),
                 gfx::TileId::pylon_arched},
                {"Coiled Pylon",
                 "a Coiled Pylon",
                 colors::light_sepia(),
                 gfx::TileId::pylon_coiled},
                {"A Serrated Pylon",
                 "a Serrated Pylon",
                 colors::light_sepia(),
                 gfx::TileId::pylon_serrated},
                {"Star-crowned Pylon",
                 "a Star-crowned Pylon",
                 colors::light_sepia(),
                 gfx::TileId::pylon_star_crowned},
                {"Two-pronged Pylon",
                 "a Two-pronged Pylon",
                 colors::light_sepia(),
                 gfx::TileId::pylon_two_pronged}};

        ASSERT(appearances.size() >= s_nr_pylon_types);

        rnd::shuffle(appearances);

        for (size_t i = 0; i < s_nr_pylon_types; ++i)
        {
                s_id_to_appearance[i] = appearances[i];

                s_is_identified[i] = false;
        }

        TRACE_FUNC_END;
}

void save()
{
        for (size_t i = 0; i < s_nr_pylon_types; ++i)
        {
                s_id_to_appearance[i].save();

                saving::put_bool(s_is_identified[i]);
        }
}

void load()
{
        for (size_t i = 0; i < s_nr_pylon_types; ++i)
        {
                s_id_to_appearance[i].load();

                s_is_identified[i] = saving::get_bool();
        }
}

// -----------------------------------------------------------------------------
// Pylon implementation
// -----------------------------------------------------------------------------
std::vector<actor::Actor*> PylonImpl::living_actors_reached() const
{
        std::vector<actor::Actor*> actors;

        for (auto* const actor : game_time::g_actors)
        {
                // Actor is dead?
                if (actor->m_state != ActorState::alive)
                {
                        continue;
                }

                const auto& p = actor->m_pos;

                const int d = 1;

                // Actor is out of range?
                if (king_dist(m_pos, p) > d)
                {
                        continue;
                }

                actors.push_back(actor);
        }

        return actors;
}

actor::Actor* PylonImpl::rnd_reached_living_actor() const
{
        const auto actors = living_actors_reached();

        if (actors.empty())
        {
                return nullptr;
        }

        auto* actor = rnd::element(living_actors_reached());

        return actor;
}

void PylonImpl::reveal() const
{
        if (is_identified(id()))
        {
                return;
        }

        s_is_identified[(size_t)id()] = true;

        const std::string fake_name = get_fake_name(id(), Article::a);

        const std::string descr = effect_descr();

        msg_log::add("I now know that " + fake_name + " " + descr + ".");

        game::incr_player_xp(8);
}

// -----------------------------------------------------------------------------
// Invisibility Pylon
// -----------------------------------------------------------------------------
std::string PylonInvis::name(Article article) const
{
        std::string str;

        if (article == Article::a)
        {
                str = "a";
        }
        else
        {
                str = "the";
        }

        return str + " Cloaking Pylon";
}

std::string PylonInvis::effect_descr() const
{
        return "turns creatures invisible";
}

void PylonInvis::on_new_turn_activated()
{
        const auto actors = living_actors_reached();

        for (auto* actor : actors)
        {
                if (actor->m_properties.has(PropId::cloaked))
                {
                        continue;
                }

                const bool can_player_see_actor_before =
                        actor::can_player_see_actor(*actor);

                actor->m_properties.apply(
                        property_factory::make(PropId::cloaked));

                if (can_player_see_actor_before)
                {
                        reveal();
                }
        }
}

// -----------------------------------------------------------------------------
// Slowing pylon
// -----------------------------------------------------------------------------
std::string PylonSlow::name(Article article) const
{
        std::string str;

        if (article == Article::a)
        {
                str = "a";
        }
        else
        {
                str = "the";
        }

        return str + " Slowing Pylon";
}

std::string PylonSlow::effect_descr() const
{
        return "slows creatures";
}

void PylonSlow::on_new_turn_activated()
{
        const auto actors = living_actors_reached();

        for (auto* actor : actors)
        {
                if (actor->m_properties.has(PropId::slowed))
                {
                        continue;
                }

                actor->m_properties.apply(
                        property_factory::make(PropId::slowed));

                if (actor::can_player_see_actor(*actor))
                {
                        reveal();
                }
        }
}

// -----------------------------------------------------------------------------
// Hasting pylon
// -----------------------------------------------------------------------------
std::string PylonHaste::name(Article article) const
{
        std::string str;

        if (article == Article::a)
        {
                str = "an";
        }
        else
        {
                str = "the";
        }

        return str + " Accelerating Pylon";
}

std::string PylonHaste::effect_descr() const
{
        return "accelerates creatures";
}

void PylonHaste::on_new_turn_activated()
{
        const auto actors = living_actors_reached();

        for (auto* actor : actors)
        {
                if (actor->m_properties.has(PropId::hasted))
                {
                        continue;
                }

                actor->m_properties.apply(
                        property_factory::make(PropId::hasted));

                if (actor::can_player_see_actor(*actor))
                {
                        reveal();
                }
        }
}

// -----------------------------------------------------------------------------
// Knockback pylon
// -----------------------------------------------------------------------------
std::string PylonKnockback::name(Article article) const
{
        std::string str;

        if (article == Article::a)
        {
                str = "a";
        }
        else
        {
                str = "the";
        }

        return str + " Repelling Pylon";
}

std::string PylonKnockback::effect_descr() const
{
        return "repels creatures";
}

void PylonKnockback::on_new_turn_activated()
{
        // Occasionally do not run the effect
        if (rnd::one_in(4))
        {
                return;
        }

        const auto actors = living_actors_reached();

        for (auto* actor : actors)
        {
                const bool can_player_see_actor_before =
                        actor::can_player_see_actor(*actor);

                knockback::run(
                        *actor,
                        m_pos,
                        knockback::KnockbackSource::other,
                        Verbose::yes,
                        2);  // Extra paralyze turns

                if (can_player_see_actor_before)
                {
                        reveal();
                }
        }
}

// -----------------------------------------------------------------------------
// Teleport pylon
// -----------------------------------------------------------------------------
std::string PylonTeleport::name(Article article) const
{
        std::string str;

        if (article == Article::a)
        {
                str = "a";
        }
        else
        {
                str = "the";
        }

        return str + " Teleporting Pylon";
}

std::string PylonTeleport::effect_descr() const
{
        return "teleports creatures";
}

void PylonTeleport::on_new_turn_activated()
{
        // Occasionally do not run the effect
        if (rnd::coin_toss())
        {
                return;
        }

        const auto actors = living_actors_reached();

        const int max_dist = 15;

        for (auto* const actor : actors)
        {
                const bool can_player_see_actor_before =
                        actor::can_player_see_actor(*actor);

                teleport(*actor, ShouldCtrlTele::if_tele_ctrl_prop, max_dist);

                if (can_player_see_actor_before)
                {
                        reveal();
                }
        }
}

// -----------------------------------------------------------------------------
// Terrify Pylon
// -----------------------------------------------------------------------------
std::string PylonTerrify::name(Article article) const
{
        std::string str;

        if (article == Article::a)
        {
                str = "a";
        }
        else
        {
                str = "the";
        }

        return str + " Terror Pylon";
}

std::string PylonTerrify::effect_descr() const
{
        return "causes fear";
}

void PylonTerrify::on_new_turn_activated()
{
        auto actors = living_actors_reached();

        for (auto* actor : actors)
        {
                if (actor->m_properties.has(PropId::terrified))
                {
                        continue;
                }

                actor->m_properties.apply(
                        property_factory::make(PropId::terrified));

                if (actor::can_player_see_actor(*actor))
                {
                        reveal();
                }
        }
}

}  // namespace pylon

// -----------------------------------------------------------------------------
// Pylon
// -----------------------------------------------------------------------------
Pylon::Pylon(const P& p) :
        Terrain(p),
        m_pylon_impl(nullptr)
{
        const auto id =
                (pylon::PylonId)rnd::range(
                        0,
                        (int)pylon::PylonId::END - 1);

        m_pylon_impl.reset(make_pylon_impl_from_id(id));
}

pylon::PylonImpl* Pylon::make_pylon_impl_from_id(const pylon::PylonId id)
{
        switch (id)
        {
        case pylon::PylonId::invis:
                return new pylon::PylonInvis(id, m_pos);

        case pylon::PylonId::slow:
                return new pylon::PylonSlow(id, m_pos);

        case pylon::PylonId::haste:
                return new pylon::PylonHaste(id, m_pos);

        case pylon::PylonId::knockback:
                return new pylon::PylonKnockback(id, m_pos);

        case pylon::PylonId::terrify:
                return new pylon::PylonTerrify(id, m_pos);

        case pylon::PylonId::teleport:
                return new pylon::PylonTeleport(id, m_pos);

        case pylon::PylonId::END:
                break;
        }

        ASSERT(false);

        return nullptr;
}

gfx::TileId Pylon::tile() const
{
        if (!m_pylon_impl)
        {
                ASSERT(false);

                return (gfx::TileId)0;
        }

        const auto& appearance = get_appearance_for_id(m_pylon_impl->id());

        return appearance.tile;
}

Color Pylon::color_default() const
{
        if (!m_pylon_impl)
        {
                ASSERT(false);

                return {};
        }

        const auto& appearance = get_appearance_for_id(m_pylon_impl->id());

        if (m_is_activated)
        {
                return appearance.color;
        }
        else
        {
                return appearance.color.shaded(50);
        }
}

std::string Pylon::name(const Article article) const
{
        if (!m_pylon_impl)
        {
                ASSERT(false);

                return {};
        }

        std::string str;

        if (is_identified(m_pylon_impl->id()))
        {
                str = m_pylon_impl->name(article);
        }
        else
        {
                str = get_fake_name(m_pylon_impl->id(), article);
        }

        if (m_is_activated)
        {
                str += " (activated)";
        }
        else
        {
                str += " (deactivated)";
        }

        return str;
}

void Pylon::bump(actor::Actor& actor_bumping)
{
        (void)actor_bumping;

        TRACE_FUNC_BEGIN;

        map::memorize_terrain_at(m_pos);
        map::update_vision();

        // If player is blind, ask it they really want to activate the Pylon.
        if (!map::g_seen.at(m_pos))
        {
                msg_log::clear();

                const std::string msg =
                        "There is a Pylon here. Touch it? " +
                        common_text::g_yes_or_no_hint;

                msg_log::add(
                        msg,
                        colors::light_white(),
                        MsgInterruptPlayer::no,
                        MorePromptOnMsg::no,
                        CopyToMsgHistory::no);

                const auto answer = query::yes_or_no();

                if (answer == BinaryAnswer::no)
                {
                        msg_log::clear();

                        TRACE_FUNC_END;

                        return;
                }
        }

        msg_log::add("I touch the Pylon.");

        if (m_is_activated)
        {
                msg_log::add("Nothing happens.");

                return;
        }

        activate();

        map::memorize_terrain_at(m_pos);
        map::update_vision();

        game_time::tick();

        TRACE_FUNC_END;
}

void Pylon::activate()
{
        m_is_activated = true;
        m_startup_countdown = 3;

        // TODO: Add sound effect

        if (map::g_seen.at(m_pos))
        {
                msg_log::add(
                        "The Pylon starts to light up and emit a humming "
                        "noise.");
        }
}

void Pylon::on_hit(
        DmgType dmg_type,
        actor::Actor* actor,
        const P& from_pos,
        int dmg)
{
        (void)actor;
        (void)from_pos;
        (void)dmg;

        switch (dmg_type)
        {
        case DmgType::explosion:
        case DmgType::pure:
        {
                if (map::is_pos_seen_by_player(m_pos))
                {
                        const std::string terrain_name =
                                text_format::first_to_upper(
                                        this->name(Article::the));

                        msg_log::add(terrain_name + " is destroyed.");
                }

                map::put(new RubbleLow(m_pos));
                map::update_vision();
        }
        break;

        default:
        {
        }
        break;
        }
}

void Pylon::on_new_turn_hook()
{
        if (!m_pylon_impl)
        {
                ASSERT(false);

                return;
        }

        if (m_is_activated)
        {
                if (m_startup_countdown > 0)
                {
                        --m_startup_countdown;
                }
                else
                {
                        ASSERT(m_startup_countdown == 0);

                        m_pylon_impl->on_new_turn_activated();
                }
        }
}

void Pylon::add_light_hook(Array2<bool>& light) const
{
        if (m_is_activated)
        {
                for (const auto& d : dir_utils::g_dir_list_w_center)
                {
                        const auto p = m_pos + d;

                        light.at(p) = true;
                }
        }
}

}  // namespace terrain

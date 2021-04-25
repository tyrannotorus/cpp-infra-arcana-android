// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "terrain_pylon.hpp"

#include "actor.hpp"
#include "actor_see.hpp"
#include "flood.hpp"
#include "game_time.hpp"
#include "knockback.hpp"
#include "map.hpp"
#include "map_parsing.hpp"
#include "misc.hpp"
#include "msg_log.hpp"
#include "property.hpp"
#include "property_factory.hpp"
#include "random.hpp"
#include "sound.hpp"
#include "teleport.hpp"

namespace terrain
{
// -----------------------------------------------------------------------------
// Pylon
// -----------------------------------------------------------------------------
Pylon::Pylon(const P& p) :
        Terrain(p),
        m_pylon_impl(nullptr)
{
        const auto id = (PylonId)rnd::range(0, (int)PylonId::END - 1);

        m_pylon_impl.reset(make_pylon_impl_from_id(id));
}

PylonImpl* Pylon::make_pylon_impl_from_id(const PylonId id)
{
        switch (id)
        {
        case PylonId::invis:
                return new PylonInvis(m_pos, this);

        case PylonId::slow:
                return new PylonSlow(m_pos, this);

        case PylonId::haste:
                return new PylonHaste(m_pos, this);

        case PylonId::knockback:
                return new PylonKnockback(m_pos, this);

        case PylonId::terrify:
                return new PylonTerrify(m_pos, this);

        case PylonId::END:
                break;
        }

        TRACE << "Bad PylonId: " << (int)id << std::endl;

        ASSERT(false);

        return nullptr;
}

std::string Pylon::name(const Article article) const
{
        std::string str = (article == Article::a) ? "a" : "the";

        str += " Pylon";

        return str;
}

Color Pylon::color_default() const
{
        return colors::light_red();
}

void Pylon::on_hit(
        DmgType dmg_type,
        actor::Actor* actor,
        const P& from_pos,
        int dmg)
{
        (void)dmg_type;
        (void)actor;
        (void)from_pos;
        (void)dmg;

        // TODO
}

void Pylon::on_new_turn_hook()
{
        m_pylon_impl->on_new_turn_activated();
}

void Pylon::add_light_hook(Array2<bool>& light) const
{
        for (const P& d : dir_utils::g_dir_list_w_center)
        {
                const P p(m_pos + d);

                light.at(p) = true;
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
        auto actors = living_actors_reached();

        if (actors.empty())
        {
                return nullptr;
        }

        auto* actor = rnd::element(living_actors_reached());

        return actor;
}

// -----------------------------------------------------------------------------
// Invisibility Pylon
// -----------------------------------------------------------------------------
void PylonInvis::on_new_turn_activated()
{
        auto actors = living_actors_reached();

        for (auto* actor : actors)
        {
                if (actor->m_properties.has(PropId::cloaked))
                {
                        continue;
                }

                actor->m_properties.apply(
                        property_factory::make(PropId::cloaked));
        }
}

// -----------------------------------------------------------------------------
// Slowing pylon
// -----------------------------------------------------------------------------
void PylonSlow::on_new_turn_activated()
{
        auto actors = living_actors_reached();

        for (auto* actor : actors)
        {
                if (actor->m_properties.has(PropId::slowed))
                {
                        continue;
                }

                auto* const prop = property_factory::make(PropId::slowed);

                prop->set_duration(rnd::range(5, 10));

                actor->m_properties.apply(prop);
        }
}

// -----------------------------------------------------------------------------
// Hasting pylon
// -----------------------------------------------------------------------------
void PylonHaste::on_new_turn_activated()
{
        auto actors = living_actors_reached();

        for (auto* actor : actors)
        {
                if (actor->m_properties.has(PropId::hasted))
                {
                        continue;
                }

                auto* const prop = property_factory::make(PropId::hasted);

                prop->set_duration(rnd::range(5, 10));

                actor->m_properties.apply(prop);
        }
}

// -----------------------------------------------------------------------------
// Knockback pylon
// -----------------------------------------------------------------------------
void PylonKnockback::on_new_turn_activated()
{
        // Occasionally do not run the effect
        if (rnd::one_in(4))
        {
                return;
        }

        auto actors = living_actors_reached();

        for (auto* actor : actors)
        {
                knockback::run(
                        *actor,
                        m_pos,
                        false,  // Not spike gun
                        Verbose::yes,
                        2);  // Extra paralyze turns
        }
}

// -----------------------------------------------------------------------------
// Terrify Pylon
// -----------------------------------------------------------------------------
void PylonTerrify::on_new_turn_activated()
{
        auto actors = living_actors_reached();

        for (auto* actor : actors)
        {
                if (actor->m_properties.has(PropId::terrified))
                {
                        continue;
                }

                auto* const prop = property_factory::make(PropId::terrified);

                prop->set_duration(rnd::range(5, 10));

                actor->m_properties.apply(prop);
        }
}

}  // namespace terrain

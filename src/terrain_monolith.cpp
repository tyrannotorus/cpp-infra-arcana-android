// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "terrain_monolith.hpp"

#include "actor.hpp"
#include "actor_player.hpp"
#include "audio.hpp"
#include "audio_data.hpp"
#include "common_text.hpp"
#include "game.hpp"
#include "map.hpp"
#include "msg_log.hpp"
#include "player_bon.hpp"
#include "property_handler.hpp"
#include "random.hpp"

struct P;

namespace terrain
{
Monolith::Monolith(const P& p) :
        Terrain(p),
        m_is_activated(false) {}

void Monolith::on_hit(
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
                if (map::is_pos_seen_by_player(m_pos))
                {
                        msg_log::add("The monolith is destroyed.");
                }

                map::put(new RubbleLow(m_pos));
                map::update_vision();

                if (player_bon::is_bg(Bg::exorcist))
                {
                        const auto msg =
                                rnd::element(
                                        common_text::g_exorcist_purge_phrases);

                        msg_log::add(msg);

                        game::incr_player_xp(15);

                        map::g_player->restore_sp(999, false);
                        map::g_player->restore_sp(10, true);
                }
                break;

        default:
                break;
        }
}

std::string Monolith::name(const Article article) const
{
        std::string ret =
                article == Article::a
                ? "a "
                : "the ";

        return ret + "carved monolith";
}

Color Monolith::color_default() const
{
        return m_is_activated
                ? colors::gray()
                : colors::light_cyan();
}

void Monolith::bump(actor::Actor& actor_bumping)
{
        if (!actor_bumping.is_player())
        {
                return;
        }

        if (!map::g_player->m_properties.allow_see())
        {
                msg_log::add("There is a carved rock here.");

                return;
        }

        if (player_bon::is_bg(Bg::exorcist))
        {
                msg_log::add(
                        "This rock is defiled with blasphemous carvings, "
                        "it must be destroyed!");

                return;
        }

        msg_log::add("I recite the inscriptions on the Monolith...");

        if (m_is_activated)
        {
                msg_log::add("Nothing happens.");
        }
        else
        {
                activate();
        }
}

void Monolith::activate()
{
        msg_log::add("I feel powerful!");

        audio::play(audio::SfxId::monolith);

        game::incr_player_xp(20);

        m_is_activated = true;

        map::g_player->incr_shock(
                ShockLvl::terrifying,
                ShockSrc::misc);
}

}  // namespace terrain

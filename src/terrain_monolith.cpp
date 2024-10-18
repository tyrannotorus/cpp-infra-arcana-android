// =============================================================================
// Copyright 2011-2024 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "terrain_monolith.hpp"

#include "actor.hpp"
#include "audio.hpp"
#include "audio_data.hpp"
#include "common_text.hpp"
#include "game.hpp"
#include "game_time.hpp"
#include "global.hpp"
#include "map.hpp"
#include "msg_log.hpp"
#include "player_bon.hpp"
#include "player_spells.hpp"
#include "property_handler.hpp"
#include "random.hpp"
#include "terrain_factory.hpp"

struct P;

namespace terrain
{
Monolith::Monolith(const P& p, const TerrainData* const data) :
        Terrain(p, data) {}

void Monolith::hit(
        DmgType dmg_type,
        actor::Actor* actor,
        const P& from_pos,
        int dmg)
{
        (void)actor;
        (void)from_pos;
        (void)dmg;

        switch (dmg_type) {
        case DmgType::explosion:
        case DmgType::pure:
                if (map::g_seen.at(m_pos)) {
                        msg_log::add("The monolith is destroyed.");
                }

                map::update_terrain(make(Id::rubble_low, m_pos));
                map::update_vision();

                if (player_bon::is_bg(Bg::exorcist)) {
                        const std::string msg =
                                rnd::element(common_text::g_exorcist_purge_phrases);

                        msg_log::add(msg);

                        game::incr_player_xp(g_xp_on_exorcist_destroy_monolith);

                        actor::restore_sp(
                                *map::g_player,
                                999,
                                actor::AllowRestoreAboveMax::no,
                                Verbose::no);

                        actor::restore_exorcist_fervor(g_exorcist_fervor_destroy_monolith);
                }
                break;

        default:
                break;
        }
}

std::string Monolith::name(const Article article) const
{
        std::string str = article == Article::a ? "a " : "the ";

        return str + "carved monolith";
}

Color Monolith::color_default() const
{
        return m_is_activated ? colors::gray() : colors::light_cyan();
}

std::optional<map::MinimapAppearance> Monolith::minimap_appearance() const
{
        if (m_is_activated) {
                return {};
        }

        map::MinimapAppearance appearance;

        appearance.color = color_default();
        appearance.legend_text = "Monolith";
        appearance.symbol = map::MinimapSymbol::rectangle_edge;

        return appearance;
}

void Monolith::bump(actor::Actor& actor_bumping)
{
        if (!actor::is_player(&actor_bumping)) {
                return;
        }

        map::memorize_terrain_at(m_pos);
        map::update_vision();

        if (!map::g_player->m_properties.allow_see()) {
                if (player_bon::is_bg(Bg::exorcist)) {
                        msg_log::add(
                                "There is a carved rock defiled with blasphemous carvings here. "
                                "It must be destroyed!");
                }
                else {
                        msg_log::add("There is a carved rock here.");
                }

                return;
        }

        if (player_bon::is_bg(Bg::exorcist)) {
                msg_log::add(
                        "This rock is defiled with blasphemous carvings, "
                        "it must be destroyed!");

                return;
        }

        msg_log::add("I recite the inscriptions on the Monolith...");

        if (m_is_activated) {
                msg_log::add("Nothing happens.");
        }
        else {
                activate();

                map::memorize_terrain_at(m_pos);
                map::update_vision();

                game_time::tick();
        }
}

void Monolith::activate()
{
        msg_log::add("I feel powerful!");

        audio::play(audio::SfxId::monolith);

        game::incr_player_xp(g_xp_on_activate_monolith);

        m_is_activated = true;

        map::g_player->incr_shock(12.0, ShockSrc::misc);
}

}  // namespace terrain

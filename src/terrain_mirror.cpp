// =============================================================================
// Copyright 2011-2024 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "terrain_mirror.hpp"

#include <algorithm>
#include <iterator>
#include <vector>

#include "actor.hpp"
#include "array2.hpp"
#include "audio.hpp"
#include "audio_data.hpp"
#include "common_text.hpp"
#include "game.hpp"
#include "game_time.hpp"
#include "global.hpp"
#include "inventory.hpp"
#include "item.hpp"
#include "item_data.hpp"
#include "map.hpp"
#include "msg_log.hpp"
#include "player_bon.hpp"
#include "property_handler.hpp"
#include "random.hpp"
#include "terrain_data.hpp"
#include "terrain_factory.hpp"
#include "text_format.hpp"

struct P;

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static std::vector<item::Item*> find_identifiable_items()
{
        std::vector<item::Item*> result;

        const Inventory& inv = map::g_player->m_inv;

        std::for_each(
                std::begin(inv.m_slots),
                std::end(inv.m_slots),
                [&result](const InvSlot& slot) {
                        if (slot.item && !slot.item->data().is_identified) {
                                result.push_back(slot.item);
                        }
                });

        std::copy_if(
                std::begin(inv.m_backpack),
                std::end(inv.m_backpack),
                std::back_inserter(result),
                [](const item::Item* const item) {
                        return !item->data().is_identified;
                });

        return result;
}

static bool player_has_unidentified_item()
{
        return !find_identifiable_items().empty();
}

static void identify_items()
{
        for (item::Item* const item : find_identifiable_items()) {
                item->identify(Verbose::yes);
        }
}

// -----------------------------------------------------------------------------
// terrain
// -----------------------------------------------------------------------------
namespace terrain
{
Mirror::Mirror(const P& p, const TerrainData* const data) :
        Terrain(p, data) {}

void Mirror::hit(
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
                        msg_log::add(
                                text_format::first_to_upper(name(Article::the)) +
                                " is destroyed.");
                }

                map::update_terrain(make(Id::rubble_low, m_pos));
                map::update_vision();

                if (player_bon::is_bg(Bg::exorcist)) {
                        const std::string msg =
                                rnd::element(common_text::g_exorcist_purge_phrases);

                        msg_log::add(msg);

                        game::incr_player_xp(g_xp_on_exorcist_destroy_mirror);

                        actor::restore_sp(
                                *map::g_player,
                                999,
                                actor::AllowRestoreAboveMax::no,
                                Verbose::no);

                        actor::restore_exorcist_fervor(g_exorcist_fervor_destroy_mirror);
                }
                break;

        default:
                break;
        }
}

std::string Mirror::name(const Article article) const
{
        std::string str = article == Article::a ? "a " : "the ";

        return str + "hazy mirror";
}

Color Mirror::color_default() const
{
        return m_is_activated ? colors::gray() : colors::sepia();
}

std::optional<map::MinimapAppearance> Mirror::minimap_appearance() const
{
        if (m_is_activated) {
                return {};
        }

        map::MinimapAppearance appearance;

        appearance.color = colors::orange();
        appearance.legend_text = "Hazy Mirror";
        appearance.symbol = map::MinimapSymbol::rectangle_edge;

        return appearance;
}

void Mirror::bump(actor::Actor& actor_bumping)
{
        if (!actor::is_player(&actor_bumping)) {
                return;
        }

        map::memorize_terrain_at(m_pos);
        map::update_vision();

        if (!map::g_player->m_properties.allow_see()) {
                msg_log::add("There is a glass surface here.");

                if (player_bon::is_bg(Bg::exorcist)) {
                        // NOTE: No return in this case - we also want to print
                        // the second message below about destroying the mirror.
                        msg_log::add("As I touch it, I am chilled to the bone.");
                }
                else {
                        return;
                }
        }

        if (player_bon::is_bg(Bg::exorcist)) {
                msg_log::add("This evil artifact must be destroyed!");

                return;
        }

        msg_log::add("I stare deep into the " + name(Article::the) + ".");

        if (m_is_activated || !player_has_unidentified_item()) {
                msg_log::add("Nothing happens.");
        }
        else {
                audio::play(audio::SfxId::mirror_activate);

                msg_log::more_prompt();

                identify_items();

                m_is_activated = true;

                map::g_player->incr_shock(8.0, ShockSrc::misc);

                map::memorize_terrain_at(m_pos);
                map::update_vision();

                game_time::tick();
        }
}

}  // namespace terrain

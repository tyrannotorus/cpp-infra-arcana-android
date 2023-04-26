// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "terrain_mirror.hpp"

#include "text_format.hpp"
#include "actor.hpp"
#include "audio.hpp"
#include "audio_data.hpp"
#include "common_text.hpp"
#include "game.hpp"
#include "game_time.hpp"
#include "global.hpp"
#include "inventory_handling.hpp"
#include "item.hpp"
#include "item_factory.hpp"
#include "map.hpp"
#include "msg_log.hpp"
#include "player_bon.hpp"
#include "player_spells.hpp"
#include "property_handler.hpp"
#include "random.hpp"
#include "terrain_factory.hpp"

struct P;

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static bool player_has_unidentified_item()
{
        const Inventory& inv = map::g_player->m_inv;

        if (
                std::any_of(
                        std::begin(inv.m_slots),
                        std::end(inv.m_slots),
                        [](const InvSlot& slot) {
                                return slot.item && !slot.item->data().is_identified;
                        })) {
                return true;
        }

        if (
                std::any_of(
                        std::begin(inv.m_backpack),
                        std::end(inv.m_backpack),
                        [](const item::Item* item) {
                                return !item->data().is_identified;
                        })) {
                return true;
        }

        return false;
}

static void identify_item()
{
        msg_log::add(
                "I feel compelled to focus on one of my possessions. "
                "Suddenly, the mirror reveals its true nature to me.");

        msg_log::more_prompt();

        states::push(std::make_unique<SelectIdentify>());
}

static bool recall_all_spells()
{
        return player_spells::recall_all_spells();
}

static void incr_xp()
{
        msg_log::add("I feel like I have learned something.");

        game::incr_player_xp(5);
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

                        game::incr_player_xp(5);

                        map::g_player->restore_sp(999, false, Verbose::no);
                        map::g_player->restore_sp(5, true);
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

void Mirror::activate()
{
        audio::play(audio::SfxId::mirror_activate);

        msg_log::more_prompt();

        bool did_apply_effect = recall_all_spells();

        if (!did_apply_effect && player_has_unidentified_item() && rnd::one_in(4)) {
                identify_item();

                did_apply_effect = true;
        }

        if (!did_apply_effect) {
                incr_xp();
        }

        m_is_activated = true;

        map::g_player->incr_shock(12.0, ShockSrc::misc);
}

}  // namespace terrain

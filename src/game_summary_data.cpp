// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "game_summary_data.hpp"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <unordered_map>
#include <utility>

#include "actor.hpp"
#include "actor_data.hpp"
#include "game.hpp"
#include "game_time.hpp"
#include "global.hpp"
#include "insanity.hpp"
#include "item.hpp"
#include "item_data.hpp"
#include "item_factory.hpp"
#include "map.hpp"
#include "player_bon.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static void collect_nr_kills_tot(
        game_summary_data::GameSummaryData& d)
{
        d.nr_kills_tot = 0;

        for (const auto& e : actor::g_data) {
                const actor::ActorData& actor_data = e.second;

                if ((actor_data.id != "MON_PLAYER") && (actor_data.nr_kills > 0)) {
                        d.nr_kills_tot += actor_data.nr_kills;
                }
        }
}

static void collect_unique_monsters_killed(
        game_summary_data::GameSummaryData& d)
{
        for (const auto& e : actor::g_data) {
                const actor::ActorData& actor_data = e.second;

                if ((actor_data.id != "MON_PLAYER") && (actor_data.nr_kills > 0)) {
                        if (actor_data.is_unique) {
                                d.unique_monsters_killed.push_back(actor_data.name_a);
                        }
                }
        }
}

static void collect_background_title(
        game_summary_data::GameSummaryData& d)
{
        if (player_bon::is_bg(Bg::occultist)) {
                const OccultistDomain domain = player_bon::occultist_domain();

                d.background_title = player_bon::occultist_profession_title(domain);
        }
        else {
                d.background_title = player_bon::bg_title(player_bon::bg());
        }
}

static void collect_potion_knowledge(
        game_summary_data::GameSummaryData& d)
{
        d.potion_knowledge.clear();

        for (int i = 0; i < (int)item::Id::END; ++i) {
                const item::ItemData& item_data = item::g_data[i];

                if ((item_data.type == ItemType::potion) &&
                    (item_data.is_tried || item_data.is_identified)) {
                        std::unique_ptr<item::Item> item(item::make(item_data.id));

                        const std::string name = item->name(ItemNameType::plain);

                        d.potion_knowledge.emplace_back(name, item_data.color);
                }
        }
}

static void collect_scroll_knowledge(
        game_summary_data::GameSummaryData& d)
{
        d.scroll_knowledge.clear();

        for (int i = 0; i < (int)item::Id::END; ++i) {
                const item::ItemData& item_data = item::g_data[i];

                if ((item_data.type == ItemType::scroll) &&
                    (item_data.is_tried || item_data.is_identified)) {
                        std::unique_ptr<item::Item> item(item::make(item_data.id));

                        const std::string name = item->name(ItemNameType::plain);

                        d.scroll_knowledge.emplace_back(name, item_data.color);
                }
        }
}

static void collect_current_traits(
        game_summary_data::GameSummaryData& d)
{
        d.current_traits.clear();

        for (size_t i = 0; i < (size_t)Trait::END; ++i) {
                if (player_bon::has_trait((Trait)i)) {
                        const auto trait = (Trait)i;

                        game_summary_data::TraitData trait_data;
                        trait_data.name = player_bon::trait_title(trait);
                        trait_data.descr = player_bon::trait_descr(trait);

                        d.current_traits.push_back(trait_data);
                }
        }
}

// -----------------------------------------------------------------------------
// game_summary_data
// -----------------------------------------------------------------------------
namespace game_summary_data
{
GameSummaryData collect()
{
        GameSummaryData d;

        d.highscore = highscore::make_entry_from_current_session();

        d.player_name = map::g_player->name_a();
        d.xp = game::xp_accumulated();
        d.clvl = game::clvl();
        d.dlvl = map::g_dlvl;
        d.turns = game_time::turn_nr();
        d.insanity = map::g_player->insanity();
        collect_background_title(d);
        collect_nr_kills_tot(d);
        collect_unique_monsters_killed(d);
        d.insanity_symptons = insanity::active_sympts();
        d.trait_log = player_bon::trait_log();
        d.player_history = game::history();
        d.msg_history = msg_log::history();
        d.properties = map::g_player->m_properties.property_names_and_descr();
        collect_potion_knowledge(d);
        collect_scroll_knowledge(d);
        collect_current_traits(d);

        return d;
}

}  // namespace game_summary_data

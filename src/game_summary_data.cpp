// =============================================================================
// Copyright 2011-2023 Martin Törnqvist <m.tornq@gmail.com>
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
#include "actor_player_state.hpp"
#include "game.hpp"
#include "game_time.hpp"
#include "global.hpp"
#include "insanity.hpp"
#include "item.hpp"
#include "item_data.hpp"
#include "item_factory.hpp"
#include "map.hpp"
#include "player_bon.hpp"
#include "text_format.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static void collect_nr_kills_tot(
        game_summary_data::GameSummaryData& d)
{
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

static std::vector<ColoredString> get_item_knowledge_names_for_type(
        const ItemType item_type)
{
        std::vector<ColoredString> names;

        for (int i = 0; i < (int)item::Id::END; ++i) {
                const item::ItemData& item_data = item::g_data[i];

                if ((item_data.type == item_type) &&
                    (item_data.is_tried || item_data.is_identified)) {
                        std::unique_ptr<item::Item> item(item::make(item_data.id));

                        const std::string name = item->name(ItemNameType::plain);

                        names.emplace_back(name, item_data.color);
                }
        }

        // Sort the result so that the player cannot identify tried items from
        // the order they appear in (e.g. if the first item is only tried and
        // the next item is known, the player can know what the first item is if
        // it always comes before the second item).

        std::sort(
                std::begin(names),
                std::end(names),
                [](
                        const ColoredString& v1,
                        const ColoredString& v2) {
                        return v1.str < v2.str;
                });

        return names;
}

template <typename T>
static void append_to_vector(std::vector<T>& a, const std::vector<T>& b)
{
        a.insert(std::end(a), std::begin(b), std::end(b));
}

static void collect_item_knowledge(game_summary_data::GameSummaryData& d)
{
        d.item_knowledge.clear();

        append_to_vector(d.item_knowledge, get_item_knowledge_names_for_type(ItemType::potion));
        append_to_vector(d.item_knowledge, get_item_knowledge_names_for_type(ItemType::scroll));
        append_to_vector(d.item_knowledge, get_item_knowledge_names_for_type(ItemType::rod));
        append_to_vector(d.item_knowledge, get_item_knowledge_names_for_type(ItemType::device));
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

static void collect_inventory(game_summary_data::GameSummaryData& d)
{
        auto get_item_name = [](const item::Item& item) {
                return (
                        text_format::first_to_upper(
                                item.name(
                                        ItemNameType::plural,
                                        ItemNameInfo::yes,
                                        ItemNameAttackInfo::main_attack_mode)));
        };

        for (const InvSlot& slot : map::g_player->m_inv.m_slots) {
                game_summary_data::InventoryItemData item_data;

                item_data.slot_name = slot.name;

                if (slot.item) {
                        item_data.item_name = get_item_name(*slot.item);
                }

                d.inventory.push_back(item_data);
        }

        for (const item::Item* const item : map::g_player->m_inv.m_backpack) {
                game_summary_data::InventoryItemData item_data;

                item_data.item_name = get_item_name(*item);

                d.inventory.push_back(item_data);
        }
}

static void collect_total_shock_taken(game_summary_data::GameSummaryData& d)
{
        d.total_shock = (int)actor::player_state::g_player_total_shock_taken;

        // Retrieve total shock for each shock source type.
        for (size_t src_idx = 0; src_idx < (size_t)ShockSrc::END; ++src_idx) {
                d.total_shock_from_src[src_idx] =
                        (int)actor::player_state::g_player_total_shock_from_src[src_idx];
        }

        // Calculate total shock from casting spells.
        for (int domain_idx = 0; domain_idx < (int)SpellDomain::END; ++domain_idx) {
                const ShockSrc src = spells::spell_domain_to_shock_type((SpellDomain)domain_idx);

                d.total_shock_from_casting_spells += d.total_shock_from_src[(size_t)src];
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
        d.current_shock = map::g_player->shock_tot();
        collect_total_shock_taken(d);
        collect_background_title(d);
        collect_nr_kills_tot(d);
        collect_unique_monsters_killed(d);
        d.insanity_symptons = insanity::active_sympts();
        d.trait_log = player_bon::trait_log();
        d.player_history = game::history();
        d.msg_history = msg_log::history();
        d.properties = map::g_player->m_properties.property_names_and_descr();
        collect_item_knowledge(d);
        collect_current_traits(d);
        collect_inventory(d);

        return d;
}

}  // namespace game_summary_data

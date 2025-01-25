// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "game_summary_data.hpp"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <tuple>
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
#include "item_device.hpp"
#include "item_factory.hpp"
#include "item_potion.hpp"
#include "item_rod.hpp"
#include "item_scroll.hpp"
#include "map.hpp"
#include "player_bon.hpp"
#include "text_format.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
// Used as elements in lists to be sorted for the item knowledge section. The
// sorting depends on what type of item it is, and may involve sorting on two
// values (e.g. first domain and then name for scrolls), so we need both an item
// object and the name that will be presented on the screen for comparison.
template <typename T>
struct ItemCompareElement
{
        std::unique_ptr<const T> item {};
        std::string name {};
};

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

// Convert a sorted list of item comparison elements to a list of item knowledge
// entries with information to be presented to the player.
template <typename T>
static std::vector<game_summary_data::ItemKnowledgeData>
sorted_item_list_to_item_knowledge_list(
        const std::vector<ItemCompareElement<T>>& item_compare_list)
{
        std::vector<game_summary_data::ItemKnowledgeData> item_knowledge_list;

        for (const ItemCompareElement<T>& compare_element : item_compare_list) {
                game_summary_data::ItemKnowledgeData e;

                const item::ItemData& item_data = compare_element.item->data();

                e.name = compare_element.name;
                e.item_color = item_data.color;
                e.is_identified = item_data.is_identified;

                item_knowledge_list.push_back(e);
        }

        return item_knowledge_list;
}

static std::vector<game_summary_data::ItemKnowledgeData> get_item_knowledge_names_for_potions()
{
        std::vector<ItemCompareElement<potion::Potion>> item_sort_list;

        for (int i = 0; i < (int)item::Id::END; ++i) {
                const item::ItemData& item_data = item::g_data[i];

                if (item_data.type == ItemType::potion) {
                        ItemCompareElement<potion::Potion> item_compare_element;

                        item_compare_element.item.reset(
                                static_cast<const potion::Potion*>(
                                        item::make(item_data.id)));

                        item_compare_element.name =
                                item_compare_element.item->name(
                                        ItemNameType::plain,
                                        ItemNameInfo::yes,
                                        ItemNameAttackInfo::none,
                                        ItemNameIdentified::force_identified);

                        item_sort_list.push_back(std::move(item_compare_element));
                }
        }

        std::sort(
                std::begin(item_sort_list),
                std::end(item_sort_list),
                [](
                        const ItemCompareElement<potion::Potion>& v1,
                        const ItemCompareElement<potion::Potion>& v2) {
                        return (
                                std::make_tuple(v1.item->alignment(), v1.name) <
                                std::make_tuple(v2.item->alignment(), v2.name));
                });

        return sorted_item_list_to_item_knowledge_list(item_sort_list);
}

static std::vector<game_summary_data::ItemKnowledgeData> get_item_knowledge_names_for_scrolls()
{
        std::vector<ItemCompareElement<scroll::Scroll>> item_sort_list;

        for (int i = 0; i < (int)item::Id::END; ++i) {
                const item::ItemData& item_data = item::g_data[i];

                if (item_data.type == ItemType::scroll) {
                        ItemCompareElement<scroll::Scroll> item_compare_element;

                        item_compare_element.item.reset(
                                static_cast<const scroll::Scroll*>(
                                        item::make(item_data.id)));

                        item_compare_element.name =
                                item_compare_element.item->name(
                                        ItemNameType::plain,
                                        ItemNameInfo::yes,
                                        ItemNameAttackInfo::none,
                                        ItemNameIdentified::force_identified);

                        item_sort_list.push_back(std::move(item_compare_element));
                }
        }

        std::sort(
                std::begin(item_sort_list),
                std::end(item_sort_list),
                [](
                        const ItemCompareElement<scroll::Scroll>& v1,
                        const ItemCompareElement<scroll::Scroll>& v2) {
                        return (
                                std::make_tuple(v1.item->domain_str(), v1.name) <
                                std::make_tuple(v2.item->domain_str(), v2.name));
                });

        return sorted_item_list_to_item_knowledge_list(item_sort_list);
}

static std::vector<game_summary_data::ItemKnowledgeData> get_item_knowledge_names_for_rods()
{
        std::vector<ItemCompareElement<rod::Rod>> item_sort_list;

        for (int i = 0; i < (int)item::Id::END; ++i) {
                const item::ItemData& item_data = item::g_data[i];

                if (item_data.type == ItemType::rod) {
                        ItemCompareElement<rod::Rod> item_compare_element;

                        item_compare_element.item.reset(
                                static_cast<const rod::Rod*>(
                                        item::make(item_data.id)));

                        // NOTE: We are not including extra info in the name,
                        // since we don't want to display something like number
                        // of turns left to recharge.
                        item_compare_element.name =
                                item_compare_element.item->name(
                                        ItemNameType::plain,
                                        ItemNameInfo::none,
                                        ItemNameAttackInfo::none,
                                        ItemNameIdentified::force_identified);

                        item_sort_list.push_back(std::move(item_compare_element));
                }
        }

        std::sort(
                std::begin(item_sort_list),
                std::end(item_sort_list),
                [](
                        const ItemCompareElement<rod::Rod>& v1,
                        const ItemCompareElement<rod::Rod>& v2) {
                        return v1.name < v2.name;
                });

        return sorted_item_list_to_item_knowledge_list(item_sort_list);
}

static std::vector<game_summary_data::ItemKnowledgeData> get_item_knowledge_names_for_devices()
{
        std::vector<ItemCompareElement<device::Device>> item_sort_list;

        for (int i = 0; i < (int)item::Id::END; ++i) {
                const item::ItemData& item_data = item::g_data[i];

                if (item_data.type == ItemType::device) {
                        ItemCompareElement<device::Device> item_compare_element;

                        item_compare_element.item.reset(
                                static_cast<const device::Device*>(
                                        item::make(item_data.id)));

                        // NOTE: We are not including extra info in the name,
                        // since we don't want to display something like
                        // "breaking".
                        item_compare_element.name =
                                item_compare_element.item->name(
                                        ItemNameType::plain,
                                        ItemNameInfo::none,
                                        ItemNameAttackInfo::none,
                                        ItemNameIdentified::force_identified);

                        item_sort_list.push_back(std::move(item_compare_element));
                }
        }

        std::sort(
                std::begin(item_sort_list),
                std::end(item_sort_list),
                [](
                        const ItemCompareElement<device::Device>& v1,
                        const ItemCompareElement<device::Device>& v2) {
                        return v1.name < v2.name;
                });

        return sorted_item_list_to_item_knowledge_list(item_sort_list);
}

template <typename T>
static void append_to_vector(std::vector<T>& a, const std::vector<T>& b)
{
        a.insert(std::end(a), std::begin(b), std::end(b));
}

static void collect_item_knowledge(game_summary_data::GameSummaryData& d)
{
        d.item_knowledge.clear();

        d.item_knowledge.push_back(get_item_knowledge_names_for_potions());
        d.item_knowledge.push_back(get_item_knowledge_names_for_scrolls());
        d.item_knowledge.push_back(get_item_knowledge_names_for_rods());
        d.item_knowledge.push_back(get_item_knowledge_names_for_devices());
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
                                        ItemNameAttackInfo::main_attack_mode,
                                        ItemNameIdentified::force_identified)));
        };

        for (const InvSlot& slot : map::g_player->m_inv.m_slots) {
                game_summary_data::InventoryItemData item_data;

                item_data.slot_name = slot.name;

                if (slot.item) {
                        item_data.item_name = get_item_name(*slot.item);

                        item_data.is_identified = slot.item->data().is_identified;
                }

                d.inventory.push_back(item_data);
        }

        for (const item::Item* const item : map::g_player->m_inv.m_backpack) {
                game_summary_data::InventoryItemData item_data;

                item_data.item_name = get_item_name(*item);

                item_data.is_identified = item->data().is_identified;

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
        //
        // NOTE: The iteration over spell domains INCLUDES "SpellDomain::END",
        // which is used for domainless spells.
        //
        for (int domain_idx = 0; domain_idx <= (int)SpellDomain::END; ++domain_idx) {
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

        d.player_name = actor::name_a(*map::g_player);
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

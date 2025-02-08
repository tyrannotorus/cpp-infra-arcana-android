// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "inventory_handling.hpp"

#include <algorithm>
#include <iterator>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "SDL_keycode.h"
#include "actor.hpp"
#include "actor_player_state.hpp"
#include "audio.hpp"
#include "audio_data.hpp"
#include "browser.hpp"
#include "colors.hpp"
#include "common_text.hpp"
#include "config.hpp"
#include "debug.hpp"
#include "draw_box.hpp"
#include "drop.hpp"
#include "game_commands.hpp"
#include "game_time.hpp"
#include "io.hpp"
#include "item.hpp"
#include "item_curse.hpp"
#include "item_data.hpp"
#include "map.hpp"
#include "marker.hpp"
#include "msg_log.hpp"
#include "panel.hpp"
#include "player_bon.hpp"
#include "pos.hpp"
#include "property_data.hpp"
#include "property_handler.hpp"
#include "query.hpp"
#include "random.hpp"
#include "rect.hpp"
#include "text_format.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static const int s_nr_turns_to_handle_armor = 7;

// Number of description lines to scroll past the item description (up or down),
// when the item description must be scrolled due to lack of window space.
static const int nr_descr_lines_scroll_past = 6;

// Index can mean Slot index or Backpack Index (both start from zero)
static bool run_drop_query(
        item::Item& item,
        const InvType inv_type,
        const size_t idx)
{
        TRACE_FUNC_BEGIN;

        const item::ItemData& data = item.data();

        msg_log::clear();

        if (!data.is_stackable || (item.m_nr_items <= 1)) {
                // Not a stack
                TRACE << "Item not stackable, or only one item" << std::endl;

                item_drop::drop_item_from_inv(*map::g_player, inv_type, idx);

                TRACE_FUNC_END;

                return true;
        }

        TRACE << "Item is stackable and more than one" << std::endl;

        states::draw();

        const std::string title =
                item.name(ItemNameType::plural) +
                " - drop how many?";

        query::QueryNumberConfig query_config;

        query_config.allowed_range = {0, item.m_nr_items};
        query_config.default_value = item.m_nr_items;
        query_config.cancel_returns_default = false;

        const int nr_to_drop = query::number(query_config, title);

        if (nr_to_drop <= 0) {
                TRACE << "Nr to drop <= 0, nothing to be done" << std::endl;

                TRACE_FUNC_END;

                return false;
        }

        // Number to drop is at least one

        item_drop::drop_item_from_inv(
                *map::g_player,
                inv_type,
                idx,
                nr_to_drop);

        TRACE_FUNC_END;

        return true;
}

static void cap_str_to_menu_x1(
        std::string& str,
        const int str_x0)
{
        const int name_max_len = panels::x1(Panel::inventory_menu) - str_x0;

        if ((int)str.length() > name_max_len) {
                str.erase(name_max_len, std::string::npos);
        }
}

static void activate(const size_t backpack_idx)
{
        item::Item* item = map::g_player->m_inv.m_backpack[backpack_idx];

        ConsumeItem result = item->activate(map::g_player);

        if (result == ConsumeItem::yes) {
                map::g_player->m_inv.decr_item_in_backpack(backpack_idx);
        }
}

static void print_cannot_remove_torture_collar_msg(const item::Item& item)
{
        const std::string name =
                item.name(
                        ItemNameType::plain,
                        ItemNameInfo::none);

        msg_log::add("The " + name + " cannot be removed!");
}

static void on_equipable_backpack_item_selected(const size_t backpack_idx)
{
        Inventory& inv = map::g_player->m_inv;
        item::Item* const item_to_equip = inv.m_backpack[backpack_idx];
        const ItemType item_type = item_to_equip->data().type;

        switch (item_type) {
        case ItemType::melee_wpn:
        case ItemType::ranged_wpn: {
                if (inv.has_item_in_slot(SlotId::wpn)) {
                        inv.unequip_slot(SlotId::wpn);

                        actor::player_state::g_item_equipping = item_to_equip;
                }
                else {
                        inv.equip_backpack_item(backpack_idx, SlotId::wpn);
                }
        } break;

        case ItemType::head_wear: {
                const item::Item* const item = inv.item_in_slot(SlotId::head);

                if (item) {
                        // HACK: The Flagellant Torture Collar is not allowed to
                        // be removed.
                        if (item->id() == item::Id::torture_collar) {
                                print_cannot_remove_torture_collar_msg(*item);

                                return;
                        }

                        inv.unequip_slot(SlotId::head);

                        actor::player_state::g_item_equipping = item_to_equip;
                }
                else {
                        inv.equip_backpack_item(backpack_idx, SlotId::head);
                }
        } break;

        case ItemType::armor: {
                if (map::g_player->m_properties.has(prop::Id::burning)) {
                        msg_log::add("Not while burning.");

                        return;
                }

                if (inv.has_item_in_slot(SlotId::body)) {
                        actor::player_state::g_remove_armor_countdown =
                                s_nr_turns_to_handle_armor;
                }

                actor::player_state::g_item_equipping = item_to_equip;

                actor::player_state::g_equip_armor_countdown =
                        s_nr_turns_to_handle_armor;
        } break;

        default:
        {
                ASSERT(false);
        } break;
        }

        game_time::tick();
}

static void reserve_key_for_filtered_inventory_index(
        const char key,
        const int filtered_inventory_index,
        std::vector<FilteredInvEntry>& filtered_inventory,
        MenuBrowser& browser)
{
        // This function ensures that the given key is used for the item at the given index of the
        // filtered inventory index. This menu option is moved to the top of the list.
        //
        // An index value of -1 means that the item to reserve the key for does not exist in the
        // player inventory. In that case, the key is only removed from the browser keys, so that is
        // is not used for anything else (it should only ever be used for the thing it's reserved
        // for).
        //
        // Some use cases are for example reserving a key for the Medical Bag or the last thrown
        // item.
        //
        // This function can be called multiple times to reserve several keys, if so the order is
        // important (the menu options will appear in reversed order, so call the option that shall
        // appear at the top last).
        //

        // NOTE: Modifying the browser keys only ever affects the keys on the first page. See also
        // note in browser.hpp.

        // The reserved key shall never used for any other item. Remove this key regardless of
        // whether we assign it later or not.
        browser.remove_key(key);

        if (filtered_inventory_index == -1) {
                // Item does not exist.
                return;
        }

        // The item exists.

        // Move this filtered inventory entry to the top of the list.
        std::rotate(
                std::begin(filtered_inventory),
                std::begin(filtered_inventory) + filtered_inventory_index,
                std::begin(filtered_inventory) + filtered_inventory_index + 1);

        // Insert the reserved key at the beginning of the key list.
        std::vector<char> custom_keys = browser.menu_keys();

        custom_keys.insert(std::begin(custom_keys), key);

        browser.set_custom_menu_keys(custom_keys);
}

// -----------------------------------------------------------------------------
// Abstract inventory screen state
// -----------------------------------------------------------------------------
StateId InvState::id() const
{
        return StateId::inventory;
}

void InvState::cycle_graphics(const io::GraphicsCycle cycle)
{
        if (cycle != io::GraphicsCycle::slow) {
                return;
        }

        const size_t nr_lines_can_be_displayed =
                panels::h(Panel::inventory_descr);

        const size_t nr_lines = make_detailed_descr_lines().size();

        if ((nr_lines > nr_lines_can_be_displayed) && (nr_lines != 0)) {
                const size_t last_idx_shown =
                        std::max(0, m_descr_idx) +
                        nr_lines_can_be_displayed - 1;

                const size_t last_line_idx = nr_lines - 1;

                const size_t reset_idx =
                        (last_line_idx + nr_descr_lines_scroll_past);

                if (last_idx_shown >= reset_idx) {
                        m_descr_idx = -nr_descr_lines_scroll_past;
                }
                else {
                        m_descr_idx += 1;
                }
        }
}

void InvState::on_window_resized()
{
        m_descr_idx = -nr_descr_lines_scroll_past;
}

void InvState::set_viewed_item(
        const item::Item* item,
        const ItemNameAttackInfo attack_info)
{
        if (item != m_viewed_item) {
                m_descr_idx = -nr_descr_lines_scroll_past;
        }

        m_viewed_item = item;
        m_viewed_item_attack_info = attack_info;
}

void InvState::draw_slot(
        const SlotId id,
        const int y,
        const char key,
        const bool is_marked,
        const ItemNameAttackInfo attack_info)
{
        // Draw key
        Color color =
                is_marked
                ? colors::menu_key_highlight()
                : colors::menu_key_dark();

        P p(0, y);

        std::string key_str = "(?)";

        key_str[1] = key;

        io::draw_text(
                key_str,
                Panel::inventory_menu,
                p,
                color);

        p.x += (int)key_str.length() + 1;

        // Draw slot label
        const InvSlot& slot = map::g_player->m_inv.m_slots[(size_t)id];

        const std::string slot_name = slot.name;

        color =
                is_marked
                ? colors::light_white()
                : colors::menu_dark();

        io::draw_text(
                slot_name,
                Panel::inventory_menu,
                p,
                color);

        p.x += 7;  // Offset to leave room for slot label

        // Draw item
        const item::Item* const item = slot.item;

        if (item) {
                // An item is equipped here
                // draw_item_symbol(*item, p);

                // p.x += 2;

                std::string item_name =
                        item->name(
                                ItemNameType::plural,
                                ItemNameInfo::yes,
                                attack_info);

                ASSERT(!item_name.empty());

                item_name = text_format::first_to_upper(item_name);

                const Color color_item =
                        is_marked
                        ? colors::light_white()
                        : item->interface_color();

                cap_str_to_menu_x1(item_name, p.x);

                io::draw_text(
                        item_name,
                        Panel::inventory_menu,
                        p,
                        color_item);

                draw_weight_pct_and_dots(
                        p,
                        item_name.size(),
                        *item,
                        color_item,
                        is_marked);
        }
        else {
                // No item in this slot
                p.x += 2;

                io::draw_text(
                        "<empty>",
                        Panel::inventory_menu,
                        p,
                        color);
        }

        if (is_marked) {
                set_viewed_item(item, attack_info);
                draw_item_descr();
        }
}

void InvState::draw_backpack_item(
        const size_t backpack_idx,
        const int y,
        const char key,
        const bool is_marked,
        const ItemNameAttackInfo attack_info)
{
        // Draw key
        const Color color =
                is_marked
                ? colors::menu_key_highlight()
                : colors::menu_key_dark();

        std::string key_str = "(?)";

        key_str[1] = key;

        P p(0, y);

        io::draw_text(
                key_str,
                Panel::inventory_menu,
                p,
                color);

        p.x += (int)key_str.length() + 1;

        // Draw item
        const item::Item* const item = map::g_player->m_inv.m_backpack[backpack_idx];

        // draw_item_symbol(*item, p);

        // p.x += 2;

        std::string item_name =
                item->name(
                        ItemNameType::plural,
                        ItemNameInfo::yes,
                        attack_info);

        item_name = text_format::first_to_upper(item_name);

        cap_str_to_menu_x1(item_name, p.x);

        const Color color_item =
                is_marked
                ? colors::light_white()
                : item->interface_color();

        io::draw_text(
                item_name,
                Panel::inventory_menu,
                p,
                color_item);

        draw_weight_pct_and_dots(
                p,
                item_name.size(),
                *item,
                color_item,
                is_marked);

        if (is_marked) {
                set_viewed_item(item, attack_info);
                draw_item_descr();
        }
}

void InvState::draw_weight_pct_and_dots(
        const P item_pos,
        const size_t item_name_len,
        const item::Item& item,
        const Color& item_name_color,
        const bool is_marked) const
{
        const int weight_carried_tot = map::g_player->m_inv.total_item_weight();

        int item_weight_pct = 0;

        if (weight_carried_tot > 0) {
                item_weight_pct = (item.weight() * 100) / weight_carried_tot;
        }

        ASSERT(item_weight_pct >= 0 && item_weight_pct <= 100);

        std::string weight_str;
        int weight_x = 0;

        if (item_weight_pct > 0 && item_weight_pct < 100) {
                weight_str = std::to_string(item_weight_pct) + "%";

                weight_x =
                        panels::w(Panel::inventory_menu) -
                        (int)weight_str.size();

                const P weight_pos(weight_x, item_pos.y);

                const Color weight_color =
                        is_marked
                        ? colors::light_white()
                        : colors::menu_dark();

                io::draw_text(
                        weight_str,
                        Panel::inventory_menu,
                        weight_pos,
                        weight_color);
        }
        else {
                // Zero weight, or 100% of weight

                // No weight percent is displayed
                weight_str = "";
                weight_x = panels::w(Panel::inventory_menu);
        }

        int dots_x = item_pos.x + (int)item_name_len;
        int dots_w = weight_x - dots_x;

        std::string dots_str;
        Color dots_color;

        // At least one dot must be drawn, otherwise we truncate the name
        if (dots_w > 0) {
                // Item name fits
                dots_str = std::string(dots_w, '.');

                dots_color =
                        is_marked
                        ? colors::white()
                        : item_name_color.shaded(85);
        }
        else {
                // Item name does not fit
                dots_str = " (...) ";
                dots_w = (int)dots_str.size();
                dots_x = weight_x - dots_w;

                dots_color = colors::gray();
        }

        io::draw_text(
                dots_str,
                Panel::inventory_menu,
                P(dots_x, item_pos.y),
                dots_color);
}

std::vector<std::string> InvState::make_detailed_descr_lines() const
{
        if (!m_viewed_item) {
                return {};
        }

        std::vector<std::string> lines;

        // -------------------------------------------------------------
        // Base description
        // -------------------------------------------------------------
        const std::vector<std::string> base_descr = m_viewed_item->descr();

        if (!base_descr.empty()) {
                for (const std::string& paragraph : base_descr) {
                        lines.emplace_back(paragraph);
                }
        }

        const bool is_plural =
                (m_viewed_item->m_nr_items > 1) &&
                m_viewed_item->data().is_stackable;

        const std::string ref_str = is_plural ? "They are " : "It is ";

        const item::ItemData& d = m_viewed_item->data();

        // -------------------------------------------------------------
        // Long reach melee weapon?
        // -------------------------------------------------------------
        if (d.melee.reach > 1) {
                lines.emplace_back(
                        "This weapon has a long reach, "
                        "press [f] to attack further away.");
        }

        // -------------------------------------------------------------
        // Damage and hit chance
        // -------------------------------------------------------------
        if (d.allow_display_dmg) {
                std::string combat_descr;

                const std::string dmg_str =
                        m_viewed_item->dmg_str(
                                m_viewed_item_attack_info,
                                ItemNameDmg::range);

                const std::string dmg_str_avg =
                        m_viewed_item->dmg_str(
                                m_viewed_item_attack_info,
                                ItemNameDmg::average);

                if (!dmg_str.empty() && !dmg_str_avg.empty()) {
                        text_format::append_with_space(
                                combat_descr,
                                ("The damage dealt with this weapon is " +
                                 dmg_str +
                                 " (average " +
                                 dmg_str_avg +
                                 ")."));
                }

                const std::string plus_str =
                        m_viewed_item->plus_str(
                                m_viewed_item_attack_info);

                if (!plus_str.empty()) {
                        text_format::append_with_space(
                                combat_descr,
                                ("Due to its quality, damage is " +
                                 plus_str +
                                 " higher than normal."));
                }

                const std::string hit_mod_str =
                        m_viewed_item->hit_mod_str(
                                m_viewed_item_attack_info,
                                AbbrevItemAttackInfo::yes);

                if (!hit_mod_str.empty()) {
                        text_format::append_with_space(
                                combat_descr,
                                ("It has a hit chance modifier of " +
                                 hit_mod_str +
                                 "."));
                }

                if (!combat_descr.empty()) {
                        lines.push_back(combat_descr);
                }
        }

        // -------------------------------------------------------------
        // Can be used for breaking doors or destroying corpses?
        // -------------------------------------------------------------
        const bool can_att_door = d.melee.can_attack_door_wood;
        const bool can_att_corpse = d.melee.can_attack_corpse;

        std::string att_obj_str;

        if (can_att_door || can_att_corpse) {
                att_obj_str = "This weapon can be used for ";
        }

        if (can_att_door) {
                att_obj_str += "breaching doors";
        }

        if (can_att_corpse) {
                if (can_att_door) {
                        att_obj_str += " and ";
                }

                att_obj_str += "destroying corpses";
        }

        if (can_att_door || can_att_corpse) {
                att_obj_str +=
                        " more effectively (while the weapon is "
                        "wielded, its attack damage is automatically "
                        "used instead of the kick damage).";

                lines.emplace_back(att_obj_str);
        }

        // -------------------------------------------------------------
        // Weight
        // -------------------------------------------------------------
        // TODO: This can be removed if weight is converted to weight units
        // instead of percentage of carried weight.
        std::string weight_str =
                ref_str +
                m_viewed_item->weight_str() +
                " to carry";

        const int weight_carried_tot =
                map::g_player->m_inv.total_item_weight();

        int weight_pct = 0;

        if (weight_carried_tot > 0) {
                weight_pct =
                        (m_viewed_item->weight() * 100) /
                        weight_carried_tot;
        }

        ASSERT(weight_pct >= 0 && weight_pct <= 100);

        if ((weight_pct > 0) && (weight_pct < 100)) {
                weight_str +=
                        " (" +
                        std::to_string(weight_pct) +
                        "% of carried weight)";
        }

        weight_str += ".";

        lines.emplace_back(weight_str);

        // -------------------------------------------------------------
        // Format the lines
        // -------------------------------------------------------------
        std::vector<std::string> formatted_lines;

        const int w = panels::w(Panel::inventory_descr);

        for (const std::string& line : lines) {
                const std::vector<std::string> new_formatted_lines =
                        text_format::split(line, w);

                if (!formatted_lines.empty()) {
                        formatted_lines.emplace_back("");
                }

                formatted_lines.insert(
                        std::end(formatted_lines),
                        std::begin(new_formatted_lines),
                        std::end(new_formatted_lines));
        }

        return formatted_lines;
}

void InvState::draw_item_descr() const
{
        // NOTE: We clear this area of the screen regardless of whether there is
        // a description to draw or not.
        io::cover_panel(Panel::inventory_descr);

        if (!m_viewed_item) {
                return;
        }

        const std::vector<std::string> lines = make_detailed_descr_lines();

        if (m_descr_idx >= (int)lines.size()) {
                ASSERT(false);
                return;
        }

        P pos(0, 0);

        const int nr_lines = (int)lines.size();
        const int max_nr_lines_shown = panels::h(Panel::inventory_descr);
        const int y1 = max_nr_lines_shown - 1;
        const int idx_lo = 0;
        const int idx_hi = std::max(0, nr_lines - max_nr_lines_shown);
        const int descr_idx = std::clamp(m_descr_idx, idx_lo, idx_hi);
        const int last_idx_can_show = descr_idx + max_nr_lines_shown - 1;
        const int y_to_fade_from = (y1 * 3) / 4;
        const int last_idx = std::min(last_idx_can_show, nr_lines - 1);
        const bool should_fade = last_idx_can_show < (nr_lines - 1);

        for (int i = descr_idx; i <= last_idx; ++i) {
                const std::string& line = lines[i];

                Color color = colors::text();

                if (should_fade && (pos.y >= y_to_fade_from)) {
                        const int y_rel = pos.y - y_to_fade_from;
                        const int y1_rel = y1 - y_to_fade_from;

                        const int pct_shaded = (y_rel * 99) / y1_rel;

                        color = color.shaded(pct_shaded);
                }

                io::draw_text(
                        line,
                        Panel::inventory_descr,
                        pos,
                        color);

                ++pos.y;
        }
}

// -----------------------------------------------------------------------------
// Inventory browsing state
// -----------------------------------------------------------------------------
void BrowseInv::on_start()
{
        map::g_player->m_inv.sort_backpack();

        const int list_size =
                (int)SlotId::END +
                (int)map::g_player->m_inv.m_backpack.size();

        m_browser.reset(list_size, panels::h(Panel::inventory_menu));

        m_browser.set_selection_audio_enabled(false);

        // Remove the "browse inventory" key, to avoid player key press misstakes, and to allow
        // using this key for closing the menu.
        //
        // NOTE: This will only ever affect the first screen (it will not affect screens further
        // down if the player scrolls down past the first screen), but this should be good enough -
        // the goal is to prevent double pressing "i".
        //
        m_browser.remove_key('i');

        map::g_player->m_inv.sort_backpack();

        audio::play(audio::SfxId::backpack);
}

void BrowseInv::draw()
{
        draw_box(panels::area(Panel::screen));

        const int browser_y = m_browser.y();

        const auto nr_slots = (size_t)SlotId::END;

        io::draw_text_center(
                " Browsing inventory ",
                Panel::screen,
                {panels::center_x(Panel::screen), 0},
                colors::title());

        io::draw_text_center(
                " " + common_text::g_screen_exit_hint + " ",
                Panel::screen,
                {panels::center_x(Panel::screen), panels::y1(Panel::screen)},
                colors::title());

        const Range idx_range_shown = m_browser.range_shown();

        int y = 0;

        for (int i = idx_range_shown.min; i <= idx_range_shown.max; ++i) {
                const char key = m_browser.menu_keys()[y];

                const bool is_marked = browser_y == i;

                if (i < (int)nr_slots) {
                        const auto slot_id = (SlotId)i;

                        draw_slot(
                                slot_id,
                                y,
                                key,
                                is_marked,
                                ItemNameAttackInfo::main_attack_mode);
                }
                else {
                        // This index is in backpack
                        const size_t backpack_idx = i - nr_slots;

                        draw_backpack_item(
                                backpack_idx,
                                y,
                                key,
                                is_marked,
                                ItemNameAttackInfo::main_attack_mode);
                }

                ++y;
        }

        // Draw "more" labels
        if (!m_browser.is_on_top_page()) {
                io::draw_text(
                        common_text::g_next_page_up_hint,
                        Panel::inventory_menu,
                        {0, -1},
                        colors::light_white());
        }

        if (!m_browser.is_on_btm_page()) {
                io::draw_text(
                        common_text::g_next_page_down_hint,
                        Panel::inventory_menu,
                        {0, panels::h(Panel::inventory_menu)},
                        colors::light_white());
        }
}

void BrowseInv::update()
{
        const io::InputData input = io::read_input();

        if (input.key == 'i') {
                // Exit screen

                states::pop();

                return;
        }

        const MenuAction action = m_browser.read(input, MenuInputMode::scrolling_and_letters);

        switch (action) {
        case MenuAction::selected: {
                if (m_allow_inv_action) {
                        on_selected();
                }
        } break;

        case MenuAction::esc:
        case MenuAction::space: {
                // Exit screen

                states::pop();
        } break;

        default:
                break;
        }
}

void BrowseInv::on_selected() const
{
        const InvType inv_type_marked =
                (m_browser.y() < (int)SlotId::END)
                ? InvType::slots
                : InvType::backpack;

        if (inv_type_marked == InvType::slots) {
                InvSlot& slot = map::g_player->m_inv.m_slots[m_browser.y()];

                on_inventory_slot_selected(slot);
        }
        else {
                const size_t backpack_idx = m_browser.y() - (int)SlotId::END;

                on_backpack_item_selected(backpack_idx);
        }
}

void BrowseInv::on_inventory_slot_selected(InvSlot& slot) const
{
        if (!slot.item) {
                // No item in slot, let player select something
                // to equip.
                states::push(std::make_unique<Equip>(slot));
        }
        else {
                // Has item in selected slot
                on_inventory_slot_with_item_selected(slot);
        }
}

void BrowseInv::on_inventory_slot_with_item_selected(InvSlot& slot) const
{
        states::pop();

        msg_log::clear();

        // NOTE: This object is now deleted!

        // HACK: The Flagellant Torture Collar is not allowed to be removed.
        if (slot.item->id() == item::Id::torture_collar) {
                print_cannot_remove_torture_collar_msg(*slot.item);
        }
        else if (slot.id == SlotId::body) {
                if (map::g_player->m_properties.has(prop::Id::burning)) {
                        msg_log::add("Not while burning.");

                        return;
                }

                actor::player_state::g_remove_armor_countdown =
                        s_nr_turns_to_handle_armor;

                game_time::tick();
        }
        else {
                map::g_player->m_inv.unequip_slot(slot.id);

                game_time::tick();
        }
}

void BrowseInv::on_backpack_item_selected(const size_t backpack_idx) const
{
        // Exit screen
        states::pop();

        item::Item* item = map::g_player->m_inv.m_backpack[backpack_idx];

        const item::ItemData& data = item->data();

        if ((data.type == ItemType::melee_wpn) ||
            (data.type == ItemType::ranged_wpn) ||
            (data.type == ItemType::armor) ||
            (data.type == ItemType::head_wear)) {
                on_equipable_backpack_item_selected(backpack_idx);
        }
        else {
                activate(backpack_idx);
        }
}

// -----------------------------------------------------------------------------
// Apply item state
// -----------------------------------------------------------------------------
void Apply::on_start()
{
        map::g_player->m_inv.sort_backpack();

        std::vector<item::Item*>& backpack = map::g_player->m_inv.m_backpack;

        m_filtered_backpack_indexes.clear();

        m_filtered_backpack_indexes.reserve(backpack.size());

        for (size_t i = 0; i < backpack.size(); ++i) {
                const item::Item* const item = backpack[i];

                const item::ItemData& d = item->data();

                if (d.has_std_activate) {
                        FilteredInvEntry entry;
                        entry.relative_idx = i;
                        entry.is_slot = false;

                        m_filtered_backpack_indexes.push_back(entry);
                }
        }

        if (m_filtered_backpack_indexes.empty()) {
                // Exit screen
                states::pop();

                msg_log::add("I carry nothing to apply.");

                return;
        }

        m_browser.reset(
                (int)m_filtered_backpack_indexes.size(),
                panels::h(Panel::inventory_menu));

        m_browser.set_selection_audio_enabled(false);

        // NOTE: These must be called in reverse order, because the key will be moved to the top.
        if (player_bon::is_bg(Bg::exorcist)) {
                reserve_key_for_item_id(item::Id::holy_symbol, 's');
        }
        reserve_key_for_item_id(item::Id::medical_bag, 'b');
        reserve_key_for_item_id(item::Id::lantern, 'a');

        audio::play(audio::SfxId::backpack);
}

void Apply::draw()
{
        // Only draw this state if it's the current state, so that messages can be drawn on the map.
        if (!states::is_current_state(this)) {
                return;
        }

        draw_box(panels::area(Panel::screen));

        const int browser_y = m_browser.y();

        io::draw_text_center(
                " Apply which item? ",
                Panel::screen,
                {panels::center_x(Panel::screen), 0},
                colors::title());

        io::draw_text_center(
                " " + common_text::g_screen_exit_hint + " ",
                Panel::screen,
                {panels::center_x(Panel::screen), panels::y1(Panel::screen)},
                colors::title());

        const Range idx_range_shown = m_browser.range_shown();

        int y = 0;

        for (int i = idx_range_shown.min; i <= idx_range_shown.max; ++i) {
                const char key = m_browser.menu_keys()[y];

                const bool is_marked = browser_y == i;

                // TODO: Update this if "applying" from slots should be supported.
                const size_t backpack_idx = m_filtered_backpack_indexes[i].relative_idx;

                draw_backpack_item(
                        backpack_idx,
                        y,
                        key,
                        is_marked,
                        ItemNameAttackInfo::main_attack_mode);

                ++y;
        }

        // Draw "more" labels
        if (!m_browser.is_on_top_page()) {
                io::draw_text(
                        common_text::g_next_page_up_hint,
                        Panel::inventory_menu,
                        {0, -1},
                        colors::light_white());
        }

        if (!m_browser.is_on_btm_page()) {
                io::draw_text(
                        common_text::g_next_page_down_hint,
                        Panel::inventory_menu,
                        {0, panels::h(Panel::inventory_menu)},
                        colors::light_white());
        }
}

void Apply::update()
{
        io::InputData input = io::read_input();

        const MenuAction action =
                m_browser.read(input, MenuInputMode::scrolling_and_letters);

        switch (action) {
        case MenuAction::selected: {
                if (!m_filtered_backpack_indexes.empty()) {
                        // TODO: Update this if "applying" from slots should be supported.
                        const size_t backpack_idx =
                                m_filtered_backpack_indexes[m_browser.y()].relative_idx;

                        // Exit screen
                        states::pop();

                        activate(backpack_idx);

                        return;
                }
        } break;

        case MenuAction::esc:
        case MenuAction::space: {
                // Exit screen
                states::pop();
                return;
        } break;

        default:
                break;
        }
}

void Apply::reserve_key_for_item_id(const item::Id id, const char key)
{
        Inventory& inventory = map::g_player->m_inv;

        int filtered_inv_idx_with_item = -1;

        for (size_t i = 0; i < m_filtered_backpack_indexes.size(); ++i) {
                FilteredInvEntry& entry = m_filtered_backpack_indexes[i];

                const item::Item* const item = inventory.m_backpack[entry.relative_idx];

                if (item->id() == id) {
                        filtered_inv_idx_with_item = (int)i;

                        break;
                }
        }

        reserve_key_for_filtered_inventory_index(
                key,
                filtered_inv_idx_with_item,
                m_filtered_backpack_indexes,
                m_browser);
}

// -----------------------------------------------------------------------------
// Drop item state
// -----------------------------------------------------------------------------
void Drop::on_start()
{
        map::g_player->m_inv.sort_backpack();

        const int list_size =
                (int)SlotId::END +
                (int)map::g_player->m_inv.m_backpack.size();

        m_browser.reset(
                list_size,
                panels::h(Panel::inventory_menu));

        m_browser.set_selection_audio_enabled(false);

        // The 'i' key is removed in the inventory menu, so we remove it in this menu as well for
        // consistency.
        m_browser.remove_key('i');

        audio::play(audio::SfxId::backpack);
}

void Drop::draw()
{
        draw_box(panels::area(Panel::screen));

        io::draw_text_center(
                " Drop which item? ",
                Panel::screen,
                {panels::center_x(Panel::screen), 0},
                colors::title());

        io::draw_text_center(
                " " + common_text::g_screen_exit_hint + " ",
                Panel::screen,
                {panels::center_x(Panel::screen), panels::y1(Panel::screen)},
                colors::title());

        const int browser_y = m_browser.y();

        const Range idx_range_shown = m_browser.range_shown();

        int y = 0;

        for (int i = idx_range_shown.min; i <= idx_range_shown.max; ++i) {
                const char key = m_browser.menu_keys()[y];

                const bool is_marked = browser_y == i;

                if (i < (int)SlotId::END) {
                        const auto slot_id = (SlotId)i;

                        draw_slot(
                                slot_id,
                                y,
                                key,
                                is_marked,
                                ItemNameAttackInfo::main_attack_mode);
                }
                else {
                        // This index is in backpack
                        const auto backpack_idx =
                                (size_t)(i - (int)SlotId::END);

                        draw_backpack_item(
                                backpack_idx,
                                y,
                                key,
                                is_marked,
                                ItemNameAttackInfo::main_attack_mode);
                }

                ++y;
        }

        // Draw "more" labels
        if (!m_browser.is_on_top_page()) {
                io::draw_text(
                        common_text::g_next_page_up_hint,
                        Panel::inventory_menu,
                        {0, -1},
                        colors::light_white());
        }

        if (!m_browser.is_on_btm_page()) {
                io::draw_text(
                        common_text::g_next_page_down_hint,
                        Panel::inventory_menu,
                        {0, panels::h(Panel::inventory_menu)},
                        colors::light_white());
        }
}

void Drop::update()
{
        const io::InputData input = io::read_input();

        const MenuAction action =
                m_browser.read(input, MenuInputMode::scrolling_and_letters);

        switch (action) {
        case MenuAction::selected: {
                on_selected();
        } break;

        case MenuAction::esc:
        case MenuAction::space: {
                // Exit screen
                states::pop();
        } break;

        default:
                break;
        }
}

void Drop::on_selected() const
{
        const int browser_y = m_browser.y();

        const InvType inv_type_marked =
                (m_browser.y() < (int)SlotId::END)
                ? InvType::slots
                : InvType::backpack;

        auto idx = (size_t)browser_y;

        item::Item* item = nullptr;

        Inventory& inv = map::g_player->m_inv;

        if (inv_type_marked == InvType::slots) {
                if (!inv.has_item_in_slot((SlotId)idx)) {
                        // No item in this slot, just do nothing (keep browsing
                        // for item to drop).
                        return;
                }

                item = inv.m_slots[idx].item;
        }
        else {
                // Backpack item marked.
                idx -= (size_t)SlotId::END;

                item = inv.m_backpack[idx];
        }

        ASSERT(item);

        // Exit screen
        states::pop();

        // HACK: The Flagellant Torture Collar is not allowed to be removed.
        if (item->id() == item::Id::torture_collar) {
                print_cannot_remove_torture_collar_msg(*item);

                return;
        }

        if (item->current_curse().is_active()) {
                const std::string name =
                        item->name(
                                ItemNameType::plain,
                                ItemNameInfo::none,
                                ItemNameAttackInfo::none);

                msg_log::add("I refuse to drop the " + name + "!");

                return;
        }

        if ((inv_type_marked == InvType::slots) &&
            (idx == (size_t)SlotId::body)) {
                // Body slot marked, start dropping the armor
                actor::player_state::g_remove_armor_countdown =
                        s_nr_turns_to_handle_armor;

                actor::player_state::g_is_dropping_armor_from_body = true;

                game_time::tick();
        }
        else {
                // Not dropping from body slot, drop immediately
                const bool did_drop =
                        run_drop_query(*item, inv_type_marked, idx);

                if (did_drop) {
                        game_time::tick();
                }
        }
}

// -----------------------------------------------------------------------------
// Equip state
// -----------------------------------------------------------------------------
void Equip::on_start()
{
        map::g_player->m_inv.sort_backpack();

        // Filter backpack
        const std::vector<item::Item*>& backpack =
                map::g_player->m_inv.m_backpack;

        m_filtered_backpack_indexes.clear();

        for (size_t i = 0; i < backpack.size(); ++i) {
                const item::Item* const item = backpack[i];
                const item::ItemData& data = item->data();

                FilteredInvEntry entry;
                entry.relative_idx = i;
                entry.is_slot = false;

                switch (m_slot_to_equip.id) {
                case SlotId::wpn:
                        if ((data.melee.is_melee_wpn) ||
                            (data.ranged.is_ranged_wpn)) {
                                m_filtered_backpack_indexes.push_back(entry);
                        }
                        break;

                case SlotId::wpn_alt:
                        if ((data.melee.is_melee_wpn) ||
                            (data.ranged.is_ranged_wpn)) {
                                m_filtered_backpack_indexes.push_back(entry);
                        }
                        break;

                case SlotId::body:
                        if (data.type == ItemType::armor) {
                                m_filtered_backpack_indexes.push_back(entry);
                        }
                        break;

                case SlotId::head:
                        if (data.type == ItemType::head_wear) {
                                m_filtered_backpack_indexes.push_back(entry);
                        }
                        break;

                case SlotId::END:
                        break;
                }
        }

        m_browser.reset(
                (int)m_filtered_backpack_indexes.size(),
                panels::h(Panel::inventory_menu));

        m_browser.set_selection_audio_enabled(false);

        m_browser.set_y(0);
}

void Equip::draw()
{
        draw_box(panels::area(Panel::screen));

        const bool has_item = !m_filtered_backpack_indexes.empty();

        std::string heading;

        switch (m_slot_to_equip.id) {
        case SlotId::wpn:
                heading =
                        has_item
                        ? "Wield which item?"
                        : "I carry no weapon to wield.";
                break;

        case SlotId::wpn_alt:
                heading =
                        has_item
                        ? "Prepare which weapon?"
                        : "I carry no weapon to wield.";
                break;

        case SlotId::body:
                heading =
                        has_item
                        ? "Wear which armor?"
                        : "I carry no armor.";
                break;

        case SlotId::head:
                heading =
                        has_item
                        ? "Wear what on head?"
                        : "I carry no headwear.";
                break;

        case SlotId::END:
                break;
        }

        if (!has_item) {
                io::draw_text(
                        " " + heading + " " + common_text::g_any_key_hint + " ",
                        Panel::screen,
                        {0, 0},
                        colors::light_white());

                return;
        }

        // An item is available

        io::draw_text_center(
                " " + heading + " ",
                Panel::screen,
                {panels::center_x(Panel::screen), 0},
                colors::title());

        io::draw_text_center(
                " " + common_text::g_screen_exit_hint + " ",
                Panel::screen,
                {panels::center_x(Panel::screen), panels::y1(Panel::screen)},
                colors::title());

        const int browser_y = m_browser.y();

        const Range idx_range_shown = m_browser.range_shown();

        int y = 0;

        for (int i = idx_range_shown.min; i <= idx_range_shown.max; ++i) {
                const char key = m_browser.menu_keys()[y];
                const bool is_marked = browser_y == i;
                const size_t backpack_idx = m_filtered_backpack_indexes[i].relative_idx;
                item::Item* const item = map::g_player->m_inv.m_backpack[backpack_idx];
                const item::ItemData& d = item->data();
                ItemNameAttackInfo att_inf = ItemNameAttackInfo::none;

                if ((m_slot_to_equip.id == SlotId::wpn) ||
                    (m_slot_to_equip.id == SlotId::wpn_alt)) {
                        // Thrown weapons are forced to show melee info instead
                        att_inf =
                                (d.main_attack_mode == AttackMode::thrown)
                                ? ItemNameAttackInfo::melee
                                : ItemNameAttackInfo::main_attack_mode;
                }

                draw_backpack_item(
                        backpack_idx,
                        y,
                        key,
                        is_marked,
                        att_inf);

                ++y;
        }

        // Draw "more" labels
        if (!m_browser.is_on_top_page()) {
                io::draw_text(
                        common_text::g_next_page_up_hint,
                        Panel::inventory_menu,
                        {0, -1},
                        colors::light_white());
        }

        if (!m_browser.is_on_btm_page()) {
                io::draw_text(
                        common_text::g_next_page_down_hint,
                        Panel::inventory_menu,
                        {0, panels::h(Panel::inventory_menu)},
                        colors::light_white());
        }
}

void Equip::update()
{
        const io::InputData input = io::read_input();

        if (m_filtered_backpack_indexes.empty() ||
            (input.key == SDLK_SPACE) ||
            (input.key == SDLK_ESCAPE)) {
                // Leave screen, and go back to inventory.
                states::pop();

                return;
        }

        const MenuAction action =
                m_browser.read(input, MenuInputMode::scrolling_and_letters);

        switch (action) {
        case MenuAction::selected: {
                const size_t idx = m_filtered_backpack_indexes[m_browser.y()].relative_idx;

                const SlotId slot_id = m_slot_to_equip.id;

                states::pop_until(StateId::game);

                if (slot_id == SlotId::body) {
                        if (map::g_player->m_properties.has(prop::Id::burning)) {
                                msg_log::add("Not while burning.");

                                return;
                        }

                        // Start putting on armor
                        actor::player_state::g_equip_armor_countdown =
                                s_nr_turns_to_handle_armor;

                        actor::player_state::g_item_equipping =
                                map::g_player->m_inv.m_backpack[idx];
                }
                else {
                        // Not the body slot, equip the item immediately
                        map::g_player->m_inv.equip_backpack_item(idx, slot_id);
                }

                game_time::tick();

                return;
        } break;

        default:
                break;
        }
}

// -----------------------------------------------------------------------------
// Select throwing state
// -----------------------------------------------------------------------------
void SelectThrow::on_start()
{
        Inventory& inventory = map::g_player->m_inv;

        inventory.sort_backpack();

        // Filter slots
        for (InvSlot& slot : inventory.m_slots) {
                const item::Item* const item = slot.item;

                if (item) {
                        const item::ItemData& d = item->data();

                        if (d.ranged.is_throwable_wpn) {
                                FilteredInvEntry entry;
                                entry.relative_idx = (size_t)slot.id;
                                entry.is_slot = true;

                                m_filtered_inv.push_back(entry);
                        }
                }
        }

        // Filter backpack
        for (size_t i = 0; i < inventory.m_backpack.size(); ++i) {
                const item::Item* const item = inventory.m_backpack[i];

                const item::ItemData& d = item->data();

                if (d.ranged.is_throwable_wpn) {
                        FilteredInvEntry entry;
                        entry.relative_idx = i;
                        entry.is_slot = false;

                        m_filtered_inv.push_back(entry);
                }
        }

        const size_t list_size = m_filtered_inv.size();

        if (list_size == 0) {
                // Nothing to throw, exit screen.
                states::pop();

                msg_log::add("I carry no throwing weapons.");

                return;
        }

        m_browser.reset((int)list_size, panels::h(Panel::inventory_menu));

        m_browser.set_selection_audio_enabled(false);

        reserve_keys();

        audio::play(audio::SfxId::backpack);
}

void SelectThrow::draw()
{
        draw_box(panels::area(Panel::screen));

        io::draw_text_center(
                " Throw which item? ",
                Panel::screen,
                {panels::center_x(Panel::screen), 0},
                colors::title());

        io::draw_text_center(
                " " + common_text::g_screen_exit_hint + " ",
                Panel::screen,
                {panels::center_x(Panel::screen), panels::y1(Panel::screen)},
                colors::title());

        const int browser_y = m_browser.y();

        const Range idx_range_shown = m_browser.range_shown();

        int y = 0;

        for (int i = idx_range_shown.min; i <= idx_range_shown.max; ++i) {
                const char key = m_browser.menu_keys()[y];

                const bool is_marked = browser_y == i;

                const FilteredInvEntry& inv_entry = m_filtered_inv[i];

                if (inv_entry.is_slot) {
                        const auto slot_id = (SlotId)inv_entry.relative_idx;

                        draw_slot(
                                slot_id,
                                y,
                                key,
                                is_marked,
                                ItemNameAttackInfo::thrown);
                }
                else {
                        // This index is in backpack
                        const size_t backpack_idx = inv_entry.relative_idx;

                        draw_backpack_item(
                                backpack_idx,
                                y,
                                key,
                                is_marked,
                                ItemNameAttackInfo::thrown);
                }

                ++y;
        }

        // Draw "more" labels
        if (!m_browser.is_on_top_page()) {
                io::draw_text(
                        common_text::g_next_page_up_hint,
                        Panel::inventory_menu,
                        {0, -1},
                        colors::light_white());
        }

        if (!m_browser.is_on_btm_page()) {
                io::draw_text(
                        common_text::g_next_page_down_hint,
                        Panel::inventory_menu,
                        {0, panels::h(Panel::inventory_menu)},
                        colors::light_white());
        }
}

void SelectThrow::update()
{
        const io::InputData input = io::read_input();

        const MenuAction action = m_browser.read(input, MenuInputMode::scrolling_and_letters);

        const FilteredInvEntry& inv_entry_marked = m_filtered_inv[m_browser.y()];

        const Inventory& inv = map::g_player->m_inv;

        item::Item* item = nullptr;

        if (inv_entry_marked.is_slot) {
                item = inv.m_slots[inv_entry_marked.relative_idx].item;
        }
        else {
                // Backpack item selected
                item = inv.m_backpack[inv_entry_marked.relative_idx];
        }

        ASSERT(item);

        switch (action) {
        case MenuAction::selected: {
                states::pop();

                const std::string name =
                        item->name(
                                ItemNameType::plain,
                                ItemNameInfo::none,
                                ItemNameAttackInfo::none);

                if (item->current_curse().is_active()) {
                        msg_log::add("I refuse to throw the " + name + "!");

                        return;
                }

                const bool is_potion = (item->data().type == ItemType::potion);

                const bool is_equipped =
                        ((item == inv.item_in_slot(SlotId::wpn)) ||
                         (item == inv.item_in_slot(SlotId::wpn_alt)));

                if (config::warn_on_throw_valuable() &&
                    (is_potion || is_equipped)) {
                        const std::string msg =
                                "Throw the " +
                                name +
                                "? " +
                                common_text::g_yes_or_no_hint;

                        msg_log::add(
                                msg,
                                colors::light_white(),
                                MsgInterruptPlayer::no,
                                MorePromptOnMsg::no,
                                CopyToMsgHistory::no);

                        const BinaryAnswer answer = query::yes_or_no();

                        msg_log::clear();

                        if (answer == BinaryAnswer::no) {
                                return;
                        }
                }

                states::push(std::make_unique<Throwing>(map::g_player->m_pos, *item));

                return;
        } break;

        case MenuAction::esc:
        case MenuAction::space: {
                // Exit screen
                states::pop();

                return;
        } break;

        default:
                break;
        }
}

void SelectThrow::reserve_keys()
{
        Inventory& inventory = map::g_player->m_inv;

        int filtered_inv_idx_with_last_thrown_item = -1;

        for (size_t i = 0; i < m_filtered_inv.size(); ++i) {
                FilteredInvEntry& entry = m_filtered_inv[i];

                const item::Item* const item =
                        entry.is_slot
                        ? inventory.m_slots[entry.relative_idx].item
                        : inventory.m_backpack[entry.relative_idx];

                if (item == actor::player_state::g_last_thrown_item) {
                        filtered_inv_idx_with_last_thrown_item = (int)i;

                        break;
                }
        }

        reserve_key_for_filtered_inventory_index(
                game_commands::throw_key(),
                filtered_inv_idx_with_last_thrown_item,
                m_filtered_inv,
                m_browser);
}

// -----------------------------------------------------------------------------
// Select identify state
// -----------------------------------------------------------------------------
void SelectIdentify::on_start()
{
        map::g_player->m_inv.sort_backpack();

        auto is_allowed_item_type =
                [](const ItemType item_type,
                   const std::vector<ItemType>& allowed_item_types) {
                        if (allowed_item_types.empty()) {
                                return true;
                        }
                        else {
                                const auto result =
                                        std::find(
                                                std::begin(allowed_item_types),
                                                std::end(allowed_item_types),
                                                item_type);

                                return result != end(allowed_item_types);
                        }
                };

        // Filter slots
        for (InvSlot& slot : map::g_player->m_inv.m_slots) {
                const item::Item* const item = slot.item;

                if (!item) {
                        continue;
                }

                const item::ItemData& d = item->data();

                if (!d.is_identified &&
                    is_allowed_item_type(d.type, m_item_types_allowed)) {
                        FilteredInvEntry entry;
                        entry.relative_idx = (size_t)slot.id;
                        entry.is_slot = true;

                        m_filtered_inv.push_back(entry);
                }
        }

        // Filter backpack
        for (size_t i = 0; i < map::g_player->m_inv.m_backpack.size(); ++i) {
                const item::Item* const item =
                        map::g_player->m_inv.m_backpack[i];

                const item::ItemData& d = item->data();

                if (!d.is_identified &&
                    is_allowed_item_type(d.type, m_item_types_allowed)) {
                        FilteredInvEntry entry;
                        entry.relative_idx = i;
                        entry.is_slot = false;

                        m_filtered_inv.push_back(entry);
                }
        }

        const size_t list_size = m_filtered_inv.size();

        if (list_size == 0) {
                // Nothing to identify, exit screen
                states::pop();

                msg_log::add("There is nothing to identify.");

                return;
        }

        m_browser.reset(
                (int)list_size,
                panels::h(Panel::inventory_menu));

        m_browser.set_selection_audio_enabled(false);

        audio::play(audio::SfxId::backpack);
}

void SelectIdentify::draw()
{
        draw_box(panels::area(Panel::screen));

        const int browser_y = m_browser.y();

        io::draw_text_center(
                " Identify which item? ",
                Panel::screen,
                {panels::center_x(Panel::screen), 0},
                colors::title());

        const Range idx_range_shown = m_browser.range_shown();

        int y = 0;

        for (int i = idx_range_shown.min; i <= idx_range_shown.max; ++i) {
                const char key = m_browser.menu_keys()[y];

                const bool is_marked = browser_y == i;

                const FilteredInvEntry& inv_entry = m_filtered_inv[i];

                if (inv_entry.is_slot) {
                        const auto slot_id = (SlotId)inv_entry.relative_idx;

                        draw_slot(
                                slot_id,
                                y,
                                key,
                                is_marked,
                                ItemNameAttackInfo::main_attack_mode);
                }
                else {
                        // This index is in backpack
                        const size_t backpack_idx = inv_entry.relative_idx;

                        draw_backpack_item(
                                backpack_idx,
                                y,
                                key,
                                is_marked,
                                ItemNameAttackInfo::main_attack_mode);
                }

                ++y;
        }

        // Draw "more" labels
        if (!m_browser.is_on_top_page()) {
                io::draw_text(
                        common_text::g_next_page_up_hint,
                        Panel::inventory_menu,
                        {0, -1},
                        colors::light_white());
        }

        if (!m_browser.is_on_btm_page()) {
                io::draw_text(
                        common_text::g_next_page_down_hint,
                        Panel::inventory_menu,
                        {0, panels::h(Panel::inventory_menu)},
                        colors::light_white());
        }
}

void SelectIdentify::update()
{
        MenuAction action = MenuAction::none;

        if (config::is_bot_playing()) {
                action = MenuAction::selected;
        }
        else {
                const io::InputData input = io::read_input();

                action = m_browser.read(input, MenuInputMode::scrolling_and_letters);
        }

        const Inventory& inv = map::g_player->m_inv;

        switch (action) {
        case MenuAction::selected: {
                const FilteredInvEntry& inv_entry_marked = m_filtered_inv[m_browser.y()];

                item::Item* item_to_identify = nullptr;

                if (inv_entry_marked.is_slot) {
                        item_to_identify = inv.m_slots[inv_entry_marked.relative_idx].item;
                }
                else {
                        // Backpack item selected
                        item_to_identify = inv.m_backpack[inv_entry_marked.relative_idx];
                }

                // Exit screen
                states::pop();

                io::clear_screen();

                map::update_vision();

                // Identify item
                item_to_identify->identify(Verbose::yes);

                return;
        } break;

        default:
                break;

        }  // action switch
}

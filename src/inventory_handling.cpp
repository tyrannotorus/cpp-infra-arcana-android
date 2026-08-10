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
#include "io_display.hpp"
#include "io_internal.hpp"
#include "item.hpp"
#include "item_curse.hpp"
#include "item_data.hpp"
#include "item_explosive.hpp"
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
#include "reload.hpp"
#include "scrollbar.hpp"
#include "text_format.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static const int s_nr_turns_to_handle_armor = 7;

// The rows are indented as if they still began with a "(x)" selection key.
// The key itself is gone (there is no keyboard to press it on - entries are
// engaged by tapping), but the indentation is what separates the rows from
// the screen border, and keeps the item names lined up.
static const int s_key_indent_w = 4;

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

// Takes off what is worn or wielded in the slot (armor comes off over
// several turns instead)
static void unequip_slot_item(InvSlot& slot)
{
        if (!slot.item) {
                return;
        }

        // HACK: The Flagellant Torture Collar is not allowed to be removed.
        if (slot.item->id() == item::Id::torture_collar) {
                print_cannot_remove_torture_collar_msg(*slot.item);

                return;
        }

        if (slot.id == SlotId::body) {
                if (map::g_player->m_properties.has(prop::Id::burning)) {
                        msg_log::add("Not while burning.");

                        return;
                }

                actor::player_state::g_remove_armor_countdown =
                        s_nr_turns_to_handle_armor;

                game_time::tick();

                return;
        }

        map::g_player->m_inv.unequip_slot(slot.id);

        game_time::tick();
}

// Drops the item (asking how many, for a stack), or refuses to
static void try_drop_item(
        item::Item& item,
        const InvType inv_type,
        const size_t idx)
{
        // HACK: The Flagellant Torture Collar is not allowed to be removed.
        if (item.id() == item::Id::torture_collar) {
                print_cannot_remove_torture_collar_msg(item);

                return;
        }

        if (item.current_curse().is_active()) {
                const std::string name =
                        item.name(
                                ItemNameType::plain,
                                ItemNameInfo::none,
                                ItemNameAttackInfo::none);

                msg_log::add("I refuse to drop the " + name + "!");

                return;
        }

        if ((inv_type == InvType::slots) &&
            (idx == (size_t)SlotId::body)) {
                // Body armor is dropped when it has been taken off
                actor::player_state::g_remove_armor_countdown =
                        s_nr_turns_to_handle_armor;

                actor::player_state::g_is_dropping_armor_from_body = true;

                game_time::tick();

                return;
        }

        if (run_drop_query(item, inv_type, idx)) {
                game_time::tick();
        }
}

// Opens the throw marker with the item, unless throwing it is refused (or
// the player thinks better of throwing something valuable). Returns
// whether the marker was opened.
static bool try_throw_item(item::Item& item)
{
        const std::string name =
                item.name(
                        ItemNameType::plain,
                        ItemNameInfo::none,
                        ItemNameAttackInfo::none);

        if (item.current_curse().is_active()) {
                msg_log::add("I refuse to throw the " + name + "!");

                return false;
        }

        const Inventory& inv = map::g_player->m_inv;

        const bool is_potion = (item.data().type == ItemType::potion);

        const bool is_equipped =
                ((&item == inv.item_in_slot(SlotId::wpn)) ||
                 (&item == inv.item_in_slot(SlotId::wpn_alt)));

        if (config::warn_on_throw_valuable() && (is_potion || is_equipped)) {
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
                        return false;
                }
        }

        states::push(
                std::make_unique<Throwing>(map::g_player->m_pos, item));

        return true;
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

std::vector<ActionPin> InvState::marked_entry_actions() const
{
        return marked_item_actions();
}

void InvState::run_action(const int action_id)
{
        run_item_action((ItemActionId)action_id);

        // NOTE: This object may now be deleted!
}

void InvState::set_viewed_item(
        const item::Item* item,
        const ItemNameAttackInfo attack_info)
{
        if (item != m_viewed_item) {
                // Another item is being looked at - its description starts
                // at the top
                m_descr.reset_scroll();
        }

        m_viewed_item = item;
        m_viewed_item_attack_info = attack_info;
}

void InvState::draw_slot(
        const SlotId id,
        const int y,
        const bool is_marked,
        const ItemNameAttackInfo attack_info)
{
        // NOTE: The rows keep the indentation of the "(x)" selection key
        // that used to be drawn here - there is no keyboard to press it
        // on, entries are engaged by tapping (see s_key_indent_w)
        P p(s_key_indent_w, y);

        // Draw slot label
        const InvSlot& slot = map::g_player->m_inv.m_slots[(size_t)id];

        const std::string slot_name = slot.name;

        const Color color =
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
        const bool is_marked,
        const ItemNameAttackInfo attack_info)
{
        // NOTE: Indented as if the "(x)" selection key was still drawn
        // here, see draw_slot
        P p(s_key_indent_w, y);

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

        if (is_marked) {
                set_viewed_item(item, attack_info);
                draw_item_descr();
        }
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
        // The state of the item right now - LAST, after everything that
        // describes what it is
        // -------------------------------------------------------------
        if (m_viewed_item->data().type == ItemType::explosive) {
                const auto* const explosive =
                        static_cast<const item::Explosive*>(m_viewed_item);

                if (explosive->is_lit()) {
                        lines.emplace_back(explosive->str_when_lit());
                }
        }

        // -------------------------------------------------------------
        // Format the lines
        // -------------------------------------------------------------
        std::vector<std::string> formatted_lines;

        // NOTE: Not the full panel width - the description column keeps
        // room for its scrollbar
        const int w = m_descr.text_w();

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

void InvState::draw_item_descr()
{
        // NOTE: We clear this area of the screen regardless of whether there is
        // a description to draw or not.
        io::cover_panel(Panel::inventory_descr);

        if (!m_viewed_item) {
                return;
        }

        m_descr.draw(make_detailed_descr_lines(), colors::text());
}

// -----------------------------------------------------------------------------
// Inventory browsing state
// -----------------------------------------------------------------------------
int BrowseInv::list_size() const
{
        return (int)SlotId::END +
                (int)map::g_player->m_inv.m_backpack.size();
}

void BrowseInv::on_inventory_changed()
{
        sync_browser_to_inventory();
}

void BrowseInv::sync_browser_to_inventory()
{
        // Items may have moved between the slots and the backpack
        // (equipping, dropping, using something up), so the browser is
        // re-made for the current inventory - but left on the entry the
        // player had marked.
        const int y = m_browser.y();

        map::g_player->m_inv.sort_backpack();

        m_browser.reset(list_size(), panels::h(Panel::inventory_menu));

        // NOTE: set_y clamps to the current number of entries
        m_browser.set_y(y);
}

void BrowseInv::on_resume()
{
        // A screen opened from here has closed
        //
        // NOTE: Drawing is deliberately NOT resumed here - see
        // InvState::pop_if_action_spent_turn
        sync_browser_to_inventory();
}

void BrowseInv::on_start()
{
        map::g_player->m_inv.sort_backpack();

        m_browser.reset(list_size(), panels::h(Panel::inventory_menu));

        m_browser.set_selection_audio_enabled(false);

        // Remove the "browse inventory" key, to avoid player key press misstakes, and to allow
        // using this key for closing the menu.
        //
        // NOTE: This will only ever affect the first page (it will not affect pages further down if
        // the player scrolls down past the first page), but this should be good enough - the goal
        // is to prevent double pressing "i".
        //
        m_browser.remove_key('i');

        map::g_player->m_inv.sort_backpack();

        audio::play(audio::SfxId::backpack);
}

void BrowseInv::draw()
{
        draw_box(panels::screen_box_area());

        const int browser_y = m_browser.y();

        const auto nr_slots = (size_t)SlotId::END;

        io::draw_text_center(
                " Inventory ",
                Panel::screen,
                {panels::center_x(Panel::screen), panels::screen_box_area().p0.y},
                colors::title());

        // The same footer as the other selectable lists (the screen is
        // closed with the [ x ] control)
        io::draw_text_center(
                " " + common_text::g_menu_select_hint + " ",
                Panel::screen,
                {panels::center_x(Panel::screen), panels::screen_box_area().p1.y},
                colors::title());

        prepare_action_pins();

        const Range idx_range_shown = m_browser.range_shown();

        int y = 0;

        for (int i = idx_range_shown.min; i <= idx_range_shown.max; ++i) {
                const bool is_marked = browser_y == i;

                if (i < (int)nr_slots) {
                        const auto slot_id = (SlotId)i;

                        draw_slot(
                                slot_id,
                                y,
                                is_marked,
                                ItemNameAttackInfo::main_attack_mode);
                }
                else {
                        // This index is in backpack
                        const size_t backpack_idx = i - nr_slots;

                        draw_backpack_item(
                                backpack_idx,
                                y,
                                is_marked,
                                ItemNameAttackInfo::main_attack_mode);
                }

                ++y;
        }

        // Where the list continues, its edge fades out
        draw_list_fades();

        // Last - the pins sit on top of the faded out end of the text
        draw_action_pins();

        // An action taken from here can ask a question that is answered in
        // the message log ("Light a Stick of Dynamite?"). The log is not
        // part of this screen, so it is drawn on top while it waits -
        // otherwise the game would sit waiting for an answer to a question
        // the player cannot see.
        if (msg_log::is_waiting_prompt()) {
                io::cover_panel(Panel::log);

                msg_log::draw();
        }
}

void BrowseInv::update()
{
        if (pop_if_action_spent_turn()) {
                // NOTE: This object is now deleted!
                return;
        }

        const io::InputData input = io::read_input();

        if (handle_pending_action()) {
                // NOTE: This object may now be deleted!
                return;
        }

        if ((input.key == 'i') && m_browser.is_on_top_page()) {
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

InvState::MarkedItem BrowseInv::marked_item() const
{
        MarkedItem marked;

        Inventory& inv = map::g_player->m_inv;

        if (m_browser.y() < (int)SlotId::END) {
                marked.inv_type = InvType::slots;
                marked.idx = (size_t)m_browser.y();
                marked.item = inv.m_slots[marked.idx].item;
        }
        else {
                marked.inv_type = InvType::backpack;
                marked.idx = (size_t)m_browser.y() - (size_t)SlotId::END;

                if (marked.idx < inv.m_backpack.size()) {
                        marked.item = inv.m_backpack[marked.idx];
                }
        }

        return marked;
}

// A lit explosive is carried like anything else, but there is nothing
// left to do with it except throw it or put it down (see item::Explosive)
static bool is_lit_explosive(const item::Item& item)
{
        return (item.data().type == ItemType::explosive) &&
                static_cast<const item::Explosive&>(item).is_lit();
}

// Lighting one from a screen: the screen has closed already (the question
// and the outcome belong on the map), and if the player was not hurt while
// lighting it, the throw marker opens with the lit item - the common case
// is to light and throw in one motion.
static void light_explosive_and_aim(const size_t backpack_idx)
{
        const std::vector<item::Explosive*> lit_before =
                item::player_lit_explosives();

        const int hp_before = map::g_player->m_hp;

        activate(backpack_idx);

        item::Explosive* newly_lit = nullptr;

        for (auto* const explosive : item::player_lit_explosives()) {
                const bool was_lit_before =
                        std::find(
                                std::begin(lit_before),
                                std::end(lit_before),
                                explosive) != std::end(lit_before);

                if (!was_lit_before) {
                        newly_lit = explosive;

                        break;
                }
        }

        if (!newly_lit) {
                // Nothing was lit (the player said no, or it was refused)
                return;
        }

        if (map::g_player->m_hp < hp_before) {
                // Interrupted - something hurt the player while the fuse
                // was being lit, and aiming a throw is not what they need
                // to decide right now
                return;
        }

        states::push(
                std::make_unique<ThrowingExplosive>(
                        map::g_player->m_pos,
                        *newly_lit));
}

// Whether using the item commits it there and then: it is consumed (or
// ends up in the player's hand), the answer to any confirmation decides
// the turn, and the player is left standing in the world. Such a use
// closes the inventory FIRST, so that its question ("Light a Stick of
// Dynamite?") and its outcome are seen on the map - the message log is
// not part of this screen.
static bool is_committing_activation(const item::ItemData& d)
{
        switch (d.type) {
        case ItemType::potion:
        case ItemType::scroll:
        case ItemType::explosive:
                return true;

        default:
                return false;
        }
}

// The verb for using an item, by what the item is ("use" covers whatever
// has no better word)
static std::string activate_label(const item::ItemData& d)
{
        switch (d.type) {
        case ItemType::potion:
                return "drink";

        case ItemType::scroll:
                return "read";

        case ItemType::explosive:
                return "light";

        default:
                return "use";
        }
}

std::vector<ActionPin> BrowseInv::marked_item_actions() const
{
        std::vector<ActionPin> pins;

        if (!m_allow_inv_action) {
                // The screen is only being read (the game info screens
                // borrow it)
                return pins;
        }

        const MarkedItem marked = marked_item();

        const Inventory& inv = map::g_player->m_inv;

        if (!marked.item) {
                if (marked.inv_type == InvType::slots) {
                        // An empty slot is filled from the backpack
                        const bool is_worn =
                                (marked.idx == (size_t)SlotId::body) ||
                                (marked.idx == (size_t)SlotId::head);

                        pins.push_back(
                                {(int)ItemActionId::equip_in,
                                 is_worn ? "wear" : "wield",
                                 {}});
                }

                return pins;
        }

        const item::ItemData& d = marked.item->data();

        if (is_lit_explosive(*marked.item)) {
                // Burning in hand - it must be thrown or put down
                pins.push_back({(int)ItemActionId::throw_item, "throw", {}});
                pins.push_back({(int)ItemActionId::drop, "drop", {}});

                return pins;
        }

        // Primary action first
        if (marked.inv_type == InvType::slots) {
                pins.push_back({(int)ItemActionId::unequip, "remove", {}});
        }
        else if ((d.type == ItemType::melee_wpn) ||
                 (d.type == ItemType::ranged_wpn)) {
                pins.push_back({(int)ItemActionId::equip, "wield", {}});
        }
        else if ((d.type == ItemType::armor) ||
                 (d.type == ItemType::head_wear)) {
                pins.push_back({(int)ItemActionId::equip, "wear", {}});
        }
        else if (d.has_std_activate) {
                pins.push_back({(int)ItemActionId::activate, activate_label(d), {}});
        }

        // Reloading is done with the weapon in hand
        const bool is_wielded_firearm =
                (marked.item == inv.item_in_slot(SlotId::wpn)) &&
                d.ranged.is_ranged_wpn &&
                !d.ranged.has_infinite_ammo;

        if (is_wielded_firearm) {
                pins.push_back({(int)ItemActionId::reload, "reload", {}});
        }

        if (d.ranged.is_throwable_wpn) {
                pins.push_back({(int)ItemActionId::throw_item, "throw", {}});
        }

        // Last - the one action that is not wanted by accident
        pins.push_back({(int)ItemActionId::drop, "drop", {}});

        return pins;
}

void InvState::run_item_action(const ItemActionId action_id)
{
        const MarkedItem marked = marked_item();

        Inventory& inv = map::g_player->m_inv;

        msg_log::clear();

        note_action_started();

        switch (action_id) {
        case ItemActionId::equip_in:
                // Opens the item selection for the marked slot - the
                // inventory stays under it
                states::push(
                        std::make_unique<Equip>(inv.m_slots[marked.idx]));
                break;

        case ItemActionId::equip:
                on_equipable_backpack_item_selected(marked.idx);
                break;

        case ItemActionId::unequip:
                unequip_slot_item(inv.m_slots[marked.idx]);
                break;

        case ItemActionId::activate:
                if (marked.item &&
                    is_committing_activation(marked.item->data())) {
                        const size_t backpack_idx = marked.idx;

                        const bool is_explosive =
                                (marked.item->data().type ==
                                 ItemType::explosive);

                        // NOTE: Read before popping - this object is
                        // deleted by then
                        const bool aim_after_lighting =
                                is_explosive && should_aim_after_lighting();

                        states::pop();

                        // NOTE: This object is now deleted!
                        if (aim_after_lighting) {
                                light_explosive_and_aim(backpack_idx);
                        }
                        else {
                                activate(backpack_idx);
                        }

                        return;
                }

                activate(marked.idx);
                break;

        case ItemActionId::reload:
                reload::try_reload(*map::g_player, marked.item);
                break;

        case ItemActionId::throw_item:
                if (marked.item && is_lit_explosive(*marked.item)) {
                        // A burning fuse is aimed with the blast overlay
                        auto* const explosive =
                                static_cast<item::Explosive*>(marked.item);

                        disable_drawing();

                        states::push(
                                std::make_unique<ThrowingExplosive>(
                                        map::g_player->m_pos,
                                        *explosive));
                }
                else if (marked.item) {
                        // The throw is aimed on the MAP - this screen is
                        // not drawn under the marker (see states::draw),
                        // and any confirmation is asked over the map too.
                        // Drawing is resumed by the next update (see
                        // pop_if_action_spent_turn), or right here if the
                        // throw is refused.
                        disable_drawing();

                        if (!try_throw_item(*marked.item)) {
                                enable_drawing();
                        }
                }
                break;

        case ItemActionId::drop:
                if (marked.item && is_lit_explosive(*marked.item)) {
                        // Put down where the player stands, still burning
                        static_cast<item::Explosive*>(marked.item)
                                ->drop_lit_at_player(true);

                        game_time::tick();
                }
                else if (marked.item) {
                        try_drop_item(
                                *marked.item,
                                marked.inv_type,
                                marked.idx);
                }
                break;
        }

        close_if_action_spent_time();

        // NOTE: This object may now be deleted!
}

void BrowseInv::on_selected()
{
        note_action_started();

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

void BrowseInv::on_inventory_slot_selected(InvSlot& slot)
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

void BrowseInv::on_inventory_slot_with_item_selected(InvSlot& slot)
{
        msg_log::clear();

        unequip_slot_item(slot);

        close_if_action_spent_time();

        // NOTE: This object may now be deleted!
}

void BrowseInv::on_backpack_item_selected(const size_t backpack_idx)
{
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

        close_if_action_spent_time();

        // NOTE: This object may now be deleted!
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
        draw_box(panels::screen_box_area());

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
                        panels::screen_box_area().p0,
                        colors::light_white());

                return;
        }

        // An item is available

        io::draw_text_center(
                " " + heading + " ",
                Panel::screen,
                {panels::center_x(Panel::screen), panels::screen_box_area().p0.y},
                colors::title());

        io::draw_text_center(
                " " + common_text::g_screen_exit_hint + " ",
                Panel::screen,
                {panels::center_x(Panel::screen), panels::screen_box_area().p1.y},
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
                        is_marked,
                        att_inf);

                ++y;
        }

        // Where the list continues, its edge fades out
        draw_list_fades();
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

        // Filter backpack. NOTE: Explosives are listed too, lit or not -
        // an unlit one is lit from here (its pin), and a lit one is thrown
        // from here like anything else.
        for (size_t i = 0; i < inventory.m_backpack.size(); ++i) {
                const item::Item* const item = inventory.m_backpack[i];

                const item::ItemData& d = item->data();

                if (d.ranged.is_throwable_wpn ||
                    (d.type == ItemType::explosive)) {
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

                msg_log::add("I carry nothing to throw.");

                return;
        }

        m_browser.reset((int)list_size, panels::h(Panel::inventory_menu));

        m_browser.set_selection_audio_enabled(false);

        reserve_keys();

        audio::play(audio::SfxId::backpack);
}

InvState::MarkedItem SelectThrow::marked_item() const
{
        MarkedItem marked;

        if ((m_browser.y() < 0) ||
            (m_browser.y() >= (int)m_filtered_inv.size())) {
                return marked;
        }

        const FilteredInvEntry& entry = m_filtered_inv[m_browser.y()];

        Inventory& inv = map::g_player->m_inv;

        marked.idx = entry.relative_idx;

        if (entry.is_slot) {
                marked.inv_type = InvType::slots;
                marked.item = inv.m_slots[entry.relative_idx].item;
        }
        else {
                marked.inv_type = InvType::backpack;

                if (entry.relative_idx < inv.m_backpack.size()) {
                        marked.item = inv.m_backpack[entry.relative_idx];
                }
        }

        return marked;
}

std::vector<ActionPin> SelectThrow::marked_item_actions() const
{
        std::vector<ActionPin> pins;

        const MarkedItem marked = marked_item();

        if (!marked.item) {
                return pins;
        }

        const item::ItemData& d = marked.item->data();

        if ((d.type == ItemType::explosive) &&
            !static_cast<const item::Explosive*>(marked.item)->is_lit()) {
                // Lighting it is the primary action - the throw is then
                // aimed right after (see light_explosive_and_aim). The
                // unlit thing can still be thrown as the plain object it
                // is though (lobbing a stick of dynamite over to where it
                // is wanted, getting rid of dead weight), so it keeps the
                // throw pin as well.
                pins.push_back({(int)ItemActionId::activate, "light", {}});
                pins.push_back({(int)ItemActionId::throw_item, "throw", {}});

                return pins;
        }

        pins.push_back({(int)ItemActionId::throw_item, "throw", {}});

        return pins;
}

void SelectThrow::draw()
{
        draw_box(panels::screen_box_area());

        io::draw_text_center(
                " Throw which item? ",
                Panel::screen,
                {panels::center_x(Panel::screen), panels::screen_box_area().p0.y},
                colors::title());

        io::draw_text_center(
                " " + common_text::g_menu_select_hint + " ",
                Panel::screen,
                {panels::center_x(Panel::screen), panels::screen_box_area().p1.y},
                colors::title());

        prepare_action_pins();

        const int browser_y = m_browser.y();

        const Range idx_range_shown = m_browser.range_shown();

        int y = 0;

        for (int i = idx_range_shown.min; i <= idx_range_shown.max; ++i) {
                const bool is_marked = browser_y == i;

                const FilteredInvEntry& inv_entry = m_filtered_inv[i];

                if (inv_entry.is_slot) {
                        const auto slot_id = (SlotId)inv_entry.relative_idx;

                        draw_slot(
                                slot_id,
                                y,
                                is_marked,
                                ItemNameAttackInfo::thrown);
                }
                else {
                        // This index is in backpack
                        const size_t backpack_idx = inv_entry.relative_idx;

                        draw_backpack_item(
                                backpack_idx,
                                y,
                                is_marked,
                                ItemNameAttackInfo::thrown);
                }

                ++y;
        }

        // Where the list continues, its edge fades out
        draw_list_fades();

        // Last - the pins sit on top of the faded out end of the text
        draw_action_pins();
}

void SelectThrow::update()
{
        if (pop_if_action_spent_turn()) {
                // NOTE: This object is now deleted!
                return;
        }

        const io::InputData input = io::read_input();

        if (handle_pending_action()) {
                // NOTE: This object may now be deleted!
                return;
        }

        const MenuAction action = m_browser.read(input, MenuInputMode::scrolling_and_letters);

        switch (action) {
        case MenuAction::selected: {
                // The same thing the primary pin does (throw, or light
                // what has to be lit first)
                const std::vector<ActionPin> pins = marked_item_actions();

                if (!pins.empty()) {
                        run_item_action((ItemActionId)pins.front().id);
                }

                // NOTE: This object may now be deleted!
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
        draw_box(panels::screen_box_area());

        const int browser_y = m_browser.y();

        io::draw_text_center(
                " Identify which item? ",
                Panel::screen,
                {panels::center_x(Panel::screen), panels::screen_box_area().p0.y},
                colors::title());

        const Range idx_range_shown = m_browser.range_shown();

        int y = 0;

        for (int i = idx_range_shown.min; i <= idx_range_shown.max; ++i) {
                const bool is_marked = browser_y == i;

                const FilteredInvEntry& inv_entry = m_filtered_inv[i];

                if (inv_entry.is_slot) {
                        const auto slot_id = (SlotId)inv_entry.relative_idx;

                        draw_slot(
                                slot_id,
                                y,
                                is_marked,
                                ItemNameAttackInfo::main_attack_mode);
                }
                else {
                        // This index is in backpack
                        const size_t backpack_idx = inv_entry.relative_idx;

                        draw_backpack_item(
                                backpack_idx,
                                y,
                                is_marked,
                                ItemNameAttackInfo::main_attack_mode);
                }

                ++y;
        }

        // Where the list continues, its edge fades out
        draw_list_fades();
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

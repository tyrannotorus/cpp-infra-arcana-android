// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "action_bar.hpp"

#include <algorithm>
#include <cstddef>
#include <set>
#include <sstream>

#include "SDL_keycode.h"
#include "colors.hpp"
#include "config.hpp"
#include "io.hpp"
#include "io_display.hpp"
#include "io_icons.hpp"
#include "io_internal.hpp"
#include "panel.hpp"
#include "pos.hpp"
#include "rect.hpp"
#include "state.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------

// All configurable actions in default order. The action keys are exactly the
// keyboard keys the game already understands (see game_commands.cpp).
// NOTE: There are deliberately NO "escape/cancel", "confirm/select" or
// "on-screen keyboard" actions (removed per user decision 2026-08-02) -
// those must be handled through mobile-friendly interactions instead
// (context pins, the [ x ] close control, tapping, two finger tap
// for the keyboard). Do not re-add them. There is also deliberately NO
// "pick up" action (removed 2026-08-02) - picking up goes through the
// standing [ pick up ] context pin, shown for as long as the player
// stands on an item (see context_pins) - and NO
// "close door" action (removed 2026-08-04) - closing/jamming goes
// through the contextual [ close ]/[ jam ] look-pin buttons (see
// GameState::on_map_panned) - and NO "melee / interact" (tab) action
// (removed 2026-08-05): attacking an adjacent monster is what swiping
// into it does (the bump attack runs the very same code, see
// player_bump_known_hostile_mon), attacking at weapon reach is done by
// aiming the melee weapon ("fire"), and its trap disarming is the
// contextual [ disarm ] look-pin button. The auto_interact command
// itself remains - the bot plays with it (see bot.cpp) - and NO
// "unload weapon" action (removed 2026-08-05) - emptying a firearm on
// the ground goes through the standing [ unload ] context pin beside
// [ pick up ] (see context_pins), and emptying one found
// in a container through the [ unload ] answer of its pick up query
// (see query::SpecialChoice) - and NO "apply item" action (removed
// 2026-08-05): every item is used through the action pins of the
// inventory screen, which is where the item to use is chosen anyway -
// and NO "drop item" action (removed 2026-08-05), for the same reason:
// putting something down is a [ drop ] action pin of the item in the
// inventory, so the bar does not need a second way in (the separate
// "Drop which item?" screen it opened is gone with it) - and NO
// "reload" action: it is a context pin of the targeting mode (safe
// because targeting engages even when the shot cannot be taken, see
// game_commands::ranged_wpn_unfireable_reason) and a [ reload ] pin of
// the wielded firearm in the inventory. The "swap" button coexists with
// the [ swap weapon ] targeting pin - swapping is also wanted outside
// of aiming.
static const std::vector<action_bar::ActionDef> s_all_actions = {
        // NOTE: The target and throw buttons TOGGLE their targeting mode
        // rather than sending the fire/throw key - tapping one while
        // already aiming drops out of it (see
        // game_commands::toggle_aim_key and MarkerState). Loosing it is
        // the [ fire ] / [ throw ] context pin.
        {"fire", "point_scan", "Target wielded weapon", {SDLK_F14, false}},
        {"throw", "switch_access_shortcut", "Throw item", {SDLK_F15, false}},
        {"swap", "swap_horiz", "Swap to readied weapon", {'z', false}, false},
        {"inventory", "backpack", "Inventory", {'i', false}},
        {"medical", "medical_services", "Use medical bag", {'b', false}, false},
        {"cast", "auto_fix_high", "Cast spell", {'x', false}},
        {"wait", "hourglass_empty", "Wait one turn", {'.', false}},
        {"rest", "bedtime", "Rest", {'s', false}, false},
        {"kick", "sports_martial_arts", "Kick", {'k', false}},
        {"noise", "campaign", "Make noise", {'n', false}},
        {"lantern", "flashlight_on", "Toggle lantern", {'l', false}},
        {"minimap", "map", "View minimap", {'m', false}, false},
};

// The hamburger menu button - always the first button, not configurable. It
// opens the in-game menu (escape), which contains the Actions
// configuration entry.
static const action_bar::ActionDef s_hamburger = {
        "_menu",
        "menu",
        "Menu",
        {SDLK_ESCAPE, false}};

// User configuration: ordering of all action ids, and the disabled subset
// (default: registry order, enabled per each action's default_enabled)
static std::vector<std::string> s_order;
static std::set<std::string> s_disabled;
static bool s_config_loaded = false;

static std::vector<std::string> split_csv(const std::string& csv)
{
        std::vector<std::string> result;

        std::stringstream ss(csv);

        std::string item;

        while (std::getline(ss, item, ',')) {
                if (!item.empty()) {
                        result.push_back(item);
                }
        }

        return result;
}

static std::string join_csv(const std::vector<std::string>& items)
{
        std::string result;

        for (const auto& item : items) {
                if (!result.empty()) {
                        result += ",";
                }

                result += item;
        }

        return result;
}

static void load_config_if_needed()
{
        if (s_config_loaded) {
                return;
        }

        s_config_loaded = true;

        s_order.clear();
        s_disabled.clear();

        // Only keep ids that still exist in the registry, and drop
        // duplicates (a reorder bug once saved corrupted orders with a
        // duplicated id - self-heal such configs on load)
        for (const auto& id : split_csv(config::action_bar_order())) {
                if (action_bar::action_def(id) &&
                    (std::find(
                             std::begin(s_order),
                             std::end(s_order),
                             id) == std::end(s_order))) {
                        s_order.push_back(id);
                }
        }

        // Append registry actions missing from the stored order (new
        // actions, or no stored config at all), toggled per their registry
        // default. Actions in the stored order keep what the player chose.
        for (const auto& def : s_all_actions) {
                if (std::find(
                            std::begin(s_order),
                            std::end(s_order),
                            def.id) == std::end(s_order)) {
                        s_order.push_back(def.id);

                        if (!def.default_enabled) {
                                s_disabled.insert(def.id);
                        }
                }
        }

        for (const auto& id : split_csv(config::action_bar_disabled())) {
                if (action_bar::action_def(id)) {
                        s_disabled.insert(id);
                }
        }
}

// The buttons currently shown on the bar: the hamburger, then the enabled
// actions in user order
static std::vector<const action_bar::ActionDef*> visible_buttons()
{
        load_config_if_needed();

        std::vector<const action_bar::ActionDef*> buttons;

        buttons.push_back(&s_hamburger);

        for (const auto& id : s_order) {
                if (s_disabled.count(id) > 0) {
                        continue;
                }

                buttons.push_back(action_bar::action_def(id));
        }

        return buttons;
}

// Buttons have a FIXED size (square, half the bar height) regardless of
// how many are enabled, flowing in up to two rows from the hamburger's
// corner inward and FILLING FROM THE BOTTOM UP (a single row of buttons
// sits at the very bottom of the screen). When the side panel is on the
// left (the bar column mirrored to the right), the flow starts from the
// right edge instead, so that the hamburger always sits at the screen
// corner opposite the side panel. Buttons beyond the bar's fixed capacity
// are not shown.
static int buttons_per_row()
{
        const auto bar_px_rect =
                io::panel_logical_px_rect(Panel::action_bar);

        return std::max(1, bar_px_rect.w() / action_bar::button_px_dims().x);
}

static int max_buttons()
{
        return buttons_per_row() * 2;
}

// Pixel rectangle (logical screen pixels) for a button index within the
// visible button list, end-inclusive
static R button_px_rect(const int button_idx)
{
        const auto bar_px_rect =
                io::panel_logical_px_rect(Panel::action_bar);

        const auto dims = action_bar::button_px_dims();

        const int per_row = buttons_per_row();

        const int row = button_idx / per_row;

        const int idx_in_row = button_idx % per_row;

        int x0;

        if (config::is_side_panel_left()) {
                // Flow from the right edge (mirrored)
                x0 = bar_px_rect.w() - ((idx_in_row + 1) * dims.x);
        }
        else {
                x0 = idx_in_row * dims.x;
        }

        // Rows fill from the bottom of the bar upward
        const int y0 = bar_px_rect.p1.y - ((row + 1) * dims.y) + 1;

        return {
                {bar_px_rect.p0.x + x0, y0},
                {bar_px_rect.p0.x + x0 + dims.x - 1, y0 + dims.y - 1}};
}

// The slot a position falls on, as an index into the visible button list
// (the inverse of button_px_rect). Positions outside the bar clamp to the
// nearest slot, so that a finger straying off the bar mid-drag still has a
// sensible target.
static int slot_idx_at(const P& logical_px)
{
        const auto bar_px_rect =
                io::panel_logical_px_rect(Panel::action_bar);

        const auto dims = action_bar::button_px_dims();

        const int per_row = buttons_per_row();

        const int col =
                config::is_side_panel_left()
                // Mirrored: the flow starts at the right edge
                ? ((bar_px_rect.p1.x - logical_px.x) / dims.x)
                : ((logical_px.x - bar_px_rect.p0.x) / dims.x);

        // Rows fill from the bottom up
        const int row = (bar_px_rect.p1.y - logical_px.y) / dims.y;

        return (std::clamp(row, 0, 1) * per_row) +
                std::clamp(col, 0, per_row - 1);
}

// A button being carried by the finger (see try_begin_reorder_drag), as an
// index into the visible button list, and the finger position
static int s_drag_button_idx = -1;
static P s_drag_px;

// The face of a button, inset from its slot (see draw_button_face)
static R button_face_px_rect(const R& slot_px_rect)
{
        return {
                slot_px_rect.p0.with_offsets(2, 2),
                slot_px_rect.p1.with_offsets(-2, -2)};
}

static void draw_button(
        const action_bar::ActionDef& def,
        const R& px_rect,
        const bool is_lifted = false)
{
        action_bar::draw_button_face(px_rect, is_lifted);

        const R button_rect = button_face_px_rect(px_rect);

        const P center(
                button_rect.p0.x + (button_rect.w() / 2),
                button_rect.p0.y + (button_rect.h() / 2));

        // Pale yellow, matching the player name/class in the side panel
        io::draw_icon(
                def.icon,
                center,
                action_bar::icon_px_size(),
                is_lifted ? colors::menu_highlight() : colors::light_sepia());
}

// -----------------------------------------------------------------------------
// action_bar
// -----------------------------------------------------------------------------
namespace action_bar
{
const std::vector<ActionDef>& all_actions()
{
        return s_all_actions;
}

const std::vector<std::string>& current_order()
{
        load_config_if_needed();

        return s_order;
}

bool is_action_enabled(const std::string& id)
{
        load_config_if_needed();

        return s_disabled.count(id) == 0;
}

void set_action_enabled(const std::string& id, const bool enabled)
{
        load_config_if_needed();

        if (enabled) {
                s_disabled.erase(id);
        }
        else {
                s_disabled.insert(id);
        }
}

void move_action(const std::string& id, const int new_idx)
{
        load_config_if_needed();

        const auto it = std::find(std::begin(s_order), std::end(s_order), id);

        if (it == std::end(s_order)) {
                return;
        }

        // NOTE: "id" may be a reference INTO s_order (the caller passes
        // current_order()[i]) - the element must be moved into place with
        // rotate, NOT erase+insert: erase shifts the elements under the
        // reference, so the insert would then duplicate the NEIGHBORING
        // action's id and lose the dragged one.
        const int old_idx = (int)(it - std::begin(s_order));

        const int idx = std::clamp(new_idx, 0, (int)s_order.size() - 1);

        if (idx < old_idx) {
                std::rotate(std::begin(s_order) + idx, it, it + 1);
        }
        else if (idx > old_idx) {
                std::rotate(it, it + 1, std::begin(s_order) + idx + 1);
        }
}

void save_to_config()
{
        load_config_if_needed();

        std::vector<std::string> disabled(
                std::begin(s_disabled),
                std::end(s_disabled));

        config::set_action_bar_layout(
                join_csv(s_order),
                join_csv(disabled));
}

const ActionDef* action_def(const std::string& id)
{
        for (const auto& def : s_all_actions) {
                if (id == def.id) {
                        return &def;
                }
        }

        return nullptr;
}

P button_px_dims()
{
        const int h = io::panel_logical_px_rect(Panel::action_bar).h() / 2;

        return {h, h};
}

int icon_px_size()
{
        const auto dims = button_px_dims();

        const R face =
                button_face_px_rect({P(0, 0), P(dims.x - 1, dims.y - 1)});

        return (std::min(face.w(), face.h()) * 3) / 4;
}

void draw_button_face(const R& px_rect, const bool is_highlighted)
{
        const R face = button_face_px_rect(px_rect);

        io::draw_rectangle_filled(face, colors::extra_dark_gray());

        // A highlighted face is outlined and tinted like a marked menu
        // entry, so that it clearly reads as "held"
        io::draw_rectangle(
                face,
                is_highlighted
                        ? colors::menu_highlight()
                        : colors::dark_gray());
}

bool is_visible()
{
        const State* const state = states::current_state();

        if (!state) {
                return false;
        }

        const auto id = state->id();

        return (id == StateId::game) || (id == StateId::marker);
}

int occupied_top_px()
{
        const auto bar_px_rect =
                io::panel_logical_px_rect(Panel::action_bar);

        const int nr_shown =
                (int)std::min(
                        visible_buttons().size(),
                        (size_t)max_buttons());

        const int per_row = buttons_per_row();

        const int nr_rows = (nr_shown + per_row - 1) / per_row;

        return bar_px_rect.p1.y - (nr_rows * button_px_dims().y) + 1;
}

void draw()
{
        // The bar (including the hamburger) is only shown while playing -
        // never over menu screens
        if (!is_visible()) {
                return;
        }

        io::set_display_for_panel(Panel::action_bar);

        const auto bar_px_rect =
                io::panel_logical_px_rect(Panel::action_bar);

        // Bar background - only behind the occupied button rows, so that
        // the message log (anchored directly above the buttons) shows the
        // map through the unoccupied part of the bar's reserved rows
        io::draw_rectangle_filled(
                {P(bar_px_rect.p0.x, occupied_top_px()), bar_px_rect.p1},
                colors::black());

        const auto buttons = visible_buttons();

        const size_t nr_shown =
                std::min(buttons.size(), (size_t)max_buttons());

        for (size_t i = 0; i < nr_shown; ++i) {
                if ((int)i == s_drag_button_idx) {
                        // Drawn last, at the finger - its slot is left
                        // empty, showing where it would drop
                        continue;
                }

                draw_button(*buttons[i], button_px_rect((int)i));
        }

        if ((s_drag_button_idx >= 0) &&
            (s_drag_button_idx < (int)nr_shown)) {
                const auto dims = button_px_dims();

                // Centered on the finger, kept within the bar (the bar has
                // a display of its own - drawing outside it would composite
                // with the bar's offset)
                const int x0 =
                        std::clamp(
                                s_drag_px.x - (dims.x / 2),
                                bar_px_rect.p0.x,
                                bar_px_rect.p1.x - dims.x + 1);

                const int y0 =
                        std::clamp(
                                s_drag_px.y - (dims.y / 2),
                                bar_px_rect.p0.y,
                                bar_px_rect.p1.y - dims.y + 1);

                draw_button(
                        *buttons[s_drag_button_idx],
                        {P(x0, y0), P(x0 + dims.x - 1, y0 + dims.y - 1)},
                        true);
        }
}

std::optional<Action> action_at(const P& logical_px)
{
        if (!is_visible()) {
                return std::nullopt;
        }

        const auto bar_px_rect =
                io::panel_logical_px_rect(Panel::action_bar);

        if (!bar_px_rect.is_pos_inside(logical_px)) {
                return std::nullopt;
        }

        const auto buttons = visible_buttons();

        const size_t nr_shown =
                std::min(buttons.size(), (size_t)max_buttons());

        for (size_t i = 0; i < nr_shown; ++i) {
                if (button_px_rect((int)i).is_pos_inside(logical_px)) {
                        return buttons[i]->action;
                }
        }

        return std::nullopt;
}

bool is_pos_on_bar(const P& logical_px)
{
        if (!is_visible()) {
                return false;
        }

        const auto bar_px_rect =
                io::panel_logical_px_rect(Panel::action_bar);

        const R occupied(
                P(bar_px_rect.p0.x, occupied_top_px()),
                bar_px_rect.p1);

        return occupied.is_pos_inside(logical_px);
}

bool try_begin_reorder_drag(const P& logical_px)
{
        if (!is_visible()) {
                return false;
        }

        const auto buttons = visible_buttons();

        const size_t nr_shown =
                std::min(buttons.size(), (size_t)max_buttons());

        // NOTE: From 1 - the hamburger is not configurable
        for (size_t i = 1; i < nr_shown; ++i) {
                if (button_px_rect((int)i).is_pos_inside(logical_px)) {
                        s_drag_button_idx = (int)i;
                        s_drag_px = logical_px;

                        return true;
                }
        }

        return false;
}

void reorder_drag_move(const P& logical_px)
{
        if (s_drag_button_idx < 0) {
                return;
        }

        s_drag_px = logical_px;

        const auto buttons = visible_buttons();

        const int nr_shown =
                (int)std::min(buttons.size(), (size_t)max_buttons());

        // Never onto the hamburger's slot, never past the last button
        const int target =
                std::clamp(slot_idx_at(logical_px), 1, nr_shown - 1);

        if (target == s_drag_button_idx) {
                return;
        }

        // The buttons are the ENABLED actions in user order, while the
        // order itself also holds the hidden ones - so the dragged action
        // moves to the position of the action currently in the target slot
        // (the hidden ones in between simply stay where they are).
        // NOTE: The id must be a copy - move_action rotates the order.
        const std::string dragged_id = buttons[s_drag_button_idx]->id;
        const std::string target_id = buttons[target]->id;

        const auto target_it =
                std::find(std::begin(s_order), std::end(s_order), target_id);

        if (target_it == std::end(s_order)) {
                return;
        }

        move_action(dragged_id, (int)(target_it - std::begin(s_order)));

        s_drag_button_idx = target;
}

void end_reorder_drag()
{
        if (s_drag_button_idx < 0) {
                return;
        }

        s_drag_button_idx = -1;

        save_to_config();
}

bool is_reorder_drag_active()
{
        return s_drag_button_idx >= 0;
}

}  // namespace action_bar

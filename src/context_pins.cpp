// =============================================================================
// Copyright Werewolf Camp
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "context_pins.hpp"

#include <string>
#include <vector>

#include "action_bar.hpp"
#include "actor.hpp"
#include "colors.hpp"
#include "config.hpp"
#include "game_commands.hpp"
#include "io.hpp"
#include "io_display.hpp"
#include "io_internal.hpp"
#include "map.hpp"
#include "msg_log.hpp"
#include "panel.hpp"
#include "pickup.hpp"
#include "pos.hpp"
#include "rect.hpp"
#include "state.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------

// The hit areas are in logical screen pixels, recorded when the pin is
// drawn (negative coordinates = not drawn)
struct Pin
{
        std::string label {};
        int key {0};
        Color color {colors::white()};
        R px_rect {-1, -1, -1, -1};
};

static std::vector<Pin> s_pushed_pins;

// Refreshed at the start of draw(), so the recorded hit areas always match
// the latest frame
static std::vector<Pin> s_standing_pins;

// Horizontal gap between pins, in gui cells
static const int s_pin_gap = 2;

// Empty rows kept between the pins and the action bar's buttons, in gui
// cells. The bar is the one thing a pin must never be confused with: a
// tap is matched against the bar FIRST (see io_input), so a finger
// reaching for a pin and landing low does not merely miss - it presses a
// button and takes a turn. The gap is what that finger lands in instead.
static const int s_bar_gap_cells = 1;

static std::string pin_str(const Pin& pin)
{
        return "[ " + pin.label + " ]";
}

// The pins belong to playing: the same states that show the action bar.
// A waiting query is the exception - a screen that asks a question puts
// its answers here too (see query::yes_or_no over the inventory screen).
// A "more" prompt owns the input instead, and hides the pins entirely.
static bool is_shown()
{
        if (msg_log::is_waiting_more_prompt()) {
                return false;
        }

        return action_bar::is_visible() || msg_log::is_waiting_prompt();
}

// The pins flow from the same screen edge as the action bar's buttons, so
// that they land under the same thumb (see action_bar).
static bool is_right_aligned()
{
        return config::is_side_panel_left();
}

// The pin row sits above the bar's buttons, one row of clear space away
// from them (see s_bar_gap_cells)
static int row_px_y0()
{
        const int cell_h = config::gui_cell_px_h();

        return action_bar::occupied_top_px() -
                ((s_bar_gap_cells + 1) * cell_h);
}

static int row_px_w(const int w_cells)
{
        return w_cells * config::gui_cell_px_w();
}

static void refresh_standing_pins()
{
        s_standing_pins.clear();

        if (msg_log::is_waiting_prompt()) {
                // A waiting prompt or query owns the input row
                return;
        }

        const State* const state = states::current_state();

        if (!state || (state->id() != StateId::game)) {
                // Only during plain play - e.g. the aiming states have
                // their own pins, and do not read the get key
                return;
        }

        if (!map::g_player || !actor::is_alive(*map::g_player)) {
                return;
        }

        if (!map::g_items.at(map::g_player->m_pos)) {
                return;
        }

        Pin pin;

        pin.label = "pick up";
        pin.key = game_commands::get_key();
        pin.color = colors::menu_highlight();

        s_standing_pins.push_back(pin);

        // A firearm with ammo in it can be emptied where it lies - the ammo
        // is usually what is wanted, not the weight of the gun, which is
        // what the pin says. There is no "unload" action bar button; this
        // is THE touch path for it (the on-screen keyboard 'u' remains as a
        // fallback).
        if (item_pickup::can_unload_item_at_player()) {
                pin.label = "take ammo";
                pin.key = game_commands::unload_key();

                s_standing_pins.push_back(pin);
        }
}

// Total width in gui cells of everything on the row
static int row_w_cells()
{
        int w = 0;

        auto add_w = [&w](const std::vector<Pin>& pins) {
                for (const Pin& pin : pins) {
                        if (w > 0) {
                                w += s_pin_gap;
                        }

                        w += (int)pin_str(pin).size();
                }
        };

        add_w(s_pushed_pins);
        add_w(s_standing_pins);

        return w;
}

// -----------------------------------------------------------------------------
// context_pins
// -----------------------------------------------------------------------------
namespace context_pins
{
void add(const std::string& label, const int key, const Color& color)
{
        Pin pin;

        pin.label = label;
        pin.key = key;
        pin.color = color;

        s_pushed_pins.push_back(pin);
}

void clear()
{
        s_pushed_pins.clear();
}

void draw()
{
        if (!is_shown()) {
                return;
        }

        refresh_standing_pins();

        if (s_pushed_pins.empty() && s_standing_pins.empty()) {
                return;
        }

        // The bar's own display, so that the pins composite on top of
        // everything - including a screen that is waiting for an answer.
        // NOTE: The display, not the panel: the row is anchored to the top
        // of the bar's BUTTONS, which is above the bar panel when the
        // buttons take up two rows.
        io::set_display(io::Display::bar);

        const int cell_w = config::gui_cell_px_w();
        const int cell_h = config::gui_cell_px_h();

        const R bar_px_rect = io::panel_logical_px_rect(Panel::action_bar);

        const int y_px = row_px_y0();

        const int w_px = row_px_w(row_w_cells());

        int x_px =
                is_right_aligned()
                ? (bar_px_rect.p1.x + 1 - w_px)
                : bar_px_rect.p0.x;

        auto draw_pins = [&](std::vector<Pin>& pins) {
                for (Pin& pin : pins) {
                        const std::string str = pin_str(pin);

                        io::draw_text_at_px(
                                str,
                                {x_px, y_px},
                                pin.color,
                                io::DrawBg::yes,
                                colors::black());

                        const int pin_w_px = (int)str.size() * cell_w;

                        pin.px_rect = {
                                P(x_px, y_px),
                                P(x_px + pin_w_px - 1, y_px + cell_h - 1)};

                        x_px += pin_w_px + (s_pin_gap * cell_w);
                }
        };

        draw_pins(s_pushed_pins);
        draw_pins(s_standing_pins);
}

// The hit areas are expanded a little, for easier tapping - but NOT
// evenly: upward is the map, where a generous target costs nothing, while
// downward is the action bar, where it would eat into the very gap that
// keeps a low finger off a button (see s_bar_gap_cells).
static R expanded(R area)
{
        const int cell_w = config::gui_cell_px_w();
        const int cell_h = config::gui_cell_px_h();

        area.p0 = area.p0.with_offsets(-cell_w / 2, -cell_h);
        area.p1 = area.p1.with_offsets(cell_w / 2, cell_h / 4);

        return area;
}

int key_at(const P& logical_px)
{
        if (!is_shown()) {
                // Never steal taps through stale hit areas
                return 0;
        }

        for (const auto* const pins : {&s_pushed_pins, &s_standing_pins}) {
                for (const Pin& pin : *pins) {
                        if ((pin.px_rect.p0.x >= 0) &&
                            expanded(pin.px_rect).is_pos_inside(logical_px)) {
                                return pin.key;
                        }
                }
        }

        return 0;
}

bool is_pos_on_pins(const P& logical_px)
{
        return key_at(logical_px) != 0;
}

int rows_above_bar()
{
        // The pin row itself, plus the gap kept below it (see row_px_y0 -
        // it offsets from the top of the BUTTONS, which at most reaches the
        // top of the bar panel)
        return s_bar_gap_cells + 1;
}

}  // namespace context_pins

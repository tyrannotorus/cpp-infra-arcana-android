// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "marker.hpp"

#include <algorithm>
#include <climits>
#include <cstring>
#include <iterator>
#include <utility>
#include <vector>

#include "SDL_keycode.h"
#include "ability_values.hpp"
#include "actor.hpp"
#include "actor_player_state.hpp"
#include "actor_see.hpp"
#include "attack.hpp"
#include "attack_data.hpp"
#include "bash.hpp"
#include "colors.hpp"
#include "common_text.hpp"
#include "config.hpp"
#include "context_pins.hpp"
#include "debug.hpp"
#include "draw_map.hpp"
#include "explosion.hpp"
#include "game_commands.hpp"
#include "gfx.hpp"
#include "inventory.hpp"
#include "io.hpp"
#include "io_display.hpp"
#include "item.hpp"
#include "item_data.hpp"
#include "item_explosive.hpp"
#include "item_factory.hpp"
#include "item_weapon.hpp"
#include "line_calc.hpp"
#include "map.hpp"
#include "map_parsing.hpp"
#include "misc.hpp"
#include "msg_log.hpp"
#include "popup.hpp"
#include "query.hpp"
#include "rect.hpp"
#include "spells.hpp"
#include "teleport.hpp"
#include "terrain.hpp"
#include "terrain_data.hpp"
#include "terrain_door.hpp"
#include "text_format.hpp"
#include "throwing.hpp"
#include "view.hpp"
#include "view_actor_descr.hpp"
#include "viewport.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------

// A cell the next marker opened shall start on, see
// marker::request_start_pos
static bool s_has_requested_start_pos = false;
static P s_requested_start_pos;

// Whether there is a weapon readied to swap to - the [ swap weapon ] pin
// of the aim marker. Swapping to nothing but an empty hand mid-aim is not
// something anyone means to do, so the pin stays away then.
static bool has_readied_wpn_to_swap_to()
{
        return map::g_player->m_inv.item_in_slot(SlotId::wpn_alt) != nullptr;
}

// Whether the weapon being aimed takes ammunition and is not already full
// - the [ reload ] pin.
//
// NOTE: Whether the player actually CARRIES ammunition for it is left
// unchecked here (that would mean walking the backpack every time the
// reticle moves) - reload::try_reload says so itself, and costs no turn
// when it cannot be done.
static bool can_reload_wpn(const item::Wpn& wpn)
{
        const auto& ranged = wpn.data().ranged;

        return ranged.is_ranged_wpn &&
                !ranged.has_infinite_ammo &&
                (ranged.max_ammo > 0) &&
                (wpn.m_ammo_loaded < ranged.max_ammo);
}

// Runs a command that costs a turn from within the aim marker, WITHOUT
// giving up the aim.
//
// The marker has to close for the turn to actually pass: it sits on top of
// the game state, which is what runs the monsters, so nothing would move
// while it is up. So it closes, the command runs, the monsters take their
// turn, and a request is left behind to put the reticle back on the same
// cell when the player is next to act (see player_act). Being interrupted
// meanwhile - attacked, something coming into view - drops that request
// (see Actor::interrupt_actions), leaving the player a normal turn to
// react with instead of a mouthful of sights.
//
// NOTE: The marker state is DESTROYED by this call - do not touch it (or
// any of its members) afterwards. The aim position is therefore taken BY
// VALUE: callers pass their own m_pos, which dies with the state.
static void run_cmd_and_resume_aim(const GameCmd cmd, const P aim_pos)
{
        msg_log::clear();

        states::pop();

        // NOTE: The marker state is now destroyed

        actor::player_state::g_is_aim_marker_pending = true;
        actor::player_state::g_aim_marker_pending_pos = aim_pos;

        // NOTE: The request is set BEFORE the command runs, so that
        // anything interrupting the player during it drops the request too
        game_commands::handle(cmd);
}

// -----------------------------------------------------------------------------
// marker
// -----------------------------------------------------------------------------
namespace marker
{
void request_start_pos(const P& pos)
{
        s_has_requested_start_pos = true;
        s_requested_start_pos = pos;
}

}  // namespace marker

// -----------------------------------------------------------------------------
// Marker state
// -----------------------------------------------------------------------------
StateId MarkerState::id() const
{
        return StateId::marker;
}

void MarkerState::on_start()
{
        m_pos = map::g_player->m_pos;

        // Resuming an aim that an action taken from the marker interrupted
        // (the [ swap weapon ] and [ reload ] pins) - the reticle goes back
        // on the very cell it was left on
        const bool did_start_at_requested_pos = try_go_to_requested_pos();

        // Opened from drag-to-look (e.g. the contextual [ throw ] button):
        // the marker takes over the cell that was being looked at - its
        // reticle replaces the look pin, rather than auto-targeting
        // somewhere else
        const bool did_start_at_look_pin =
                !did_start_at_requested_pos && try_go_to_look_pin();

        if (!did_start_at_requested_pos &&
            !did_start_at_look_pin &&
            use_player_tgt()) {
                // First, attempt to place marker at player target.
                const bool did_go_to_tgt = try_go_to_tgt();

                if (!did_go_to_tgt) {
                        // If no target available, attempt to place marker at
                        // closest visible monster. This sets a new player
                        // target if successful.
                        actor::player_state::g_target = nullptr;

                        try_go_to_closest_enemy();
                }
        }

        on_start_hook();

        // Dragging pans the view with the marker fixed at the centering
        // point - center the view on the marker so that the marker and
        // the center "pin" coincide from the start
        viewport::show(m_pos, viewport::ForceCentering::yes);

        on_moved();
}

void MarkerState::on_window_resized()
{
        // This is safe and convenient:
        m_pos = map::g_player->m_pos;

        viewport::show(m_pos, viewport::ForceCentering::yes);

        msg_log::clear();
}

void MarkerState::on_popped()
{
        on_popped_hook();

        // The view may have scrolled beyond the map edges - drop any
        // manual pan and snap the camera back to the player
        viewport::end_pan_and_center_on_player();
}

void MarkerState::on_map_panned()
{
        const P center = viewport::center_map_pos();

        if (center != m_pos) {
                m_pos = center;

                msg_log::clear();

                on_moved();
        }
}

void MarkerState::draw()
{
        if (!m_allow_draw) {
                return;
        }

        if (!viewport::is_in_view(m_pos)) {
                viewport::show(m_pos, viewport::ForceCentering::yes);
        }

        auto line =
                line_calc::calc_new_line(
                        m_origin,
                        m_pos,
                        true,     // Stop at target
                        INT_MAX,  // Travel limit
                        true);    // Allow outside map

        // Remove origin position
        if (!line.empty()) {
                line.erase(std::begin(line));
        }

        const Range effective_dist_range = effective_king_dist_range();

        const int warn_until_including_dist =
                (effective_dist_range.min == -1)
                ? -1
                : (effective_dist_range.min - 1);

        const int warn_from_dist =
                (effective_dist_range.max == -1)
                ? -1
                : (effective_dist_range.max + 1);

        // NOTE: Maximum distance "-1" means "no maximum distance". If the thing
        // being aimed HAS a maximum distance (greater than -1), then the marker
        // should be red from distances greater than the maximum distance.
        const int max_dist = max_king_dist();

        const int out_of_range_from_dist =
                (max_dist == -1) ? -1 : (max_dist + 1);

        int blocked_from_idx = -1;

        if (show_blocked()) {
                for (size_t i = 0; i < line.size(); ++i) {
                        const P& p = line[i];

                        if (!map::is_pos_inside_map(p)) {
                                break;
                        }

                        if (map::g_seen.at(p) && is_pos_blocked(p)) {
                                blocked_from_idx = (int)i;
                                break;
                        }
                }
        }

        draw_marker(
                line,
                warn_until_including_dist,
                warn_from_dist,
                out_of_range_from_dist,
                blocked_from_idx);

        on_draw();
}

bool MarkerState::is_pos_blocked(const P& pos) const
{
        map_parsers::BlocksProjectiles blocked_parser;

        return blocked_parser.run(pos);
}

void MarkerState::update()
{
        io::InputData input;

        if (!config::is_bot_playing()) {
                input = io::read_input();
        }

        const GameCmd game_cmd = game_commands::to_cmd(input);

        switch (game_cmd) {
        case GameCmd::right:
        case GameCmd::down:
        case GameCmd::left:
        case GameCmd::up:
        case GameCmd::up_right:
        case GameCmd::down_right:
        case GameCmd::up_left:
        case GameCmd::down_left:
        case GameCmd::auto_move_right:
        case GameCmd::auto_move_down:
        case GameCmd::auto_move_left:
        case GameCmd::auto_move_up:
        case GameCmd::auto_move_up_right:
        case GameCmd::auto_move_down_right:
        case GameCmd::auto_move_up_left:
        case GameCmd::auto_move_down_left: {
                if (!allow_swipe_cancel()) {
                        // Swipes do nothing here (the current cell info
                        // shall not be cleared by ignored input)
                        return;
                }

                // A movement swipe cancels the targeting and performs the
                // move - the same way a movement swipe during drag-to-look
                // cancels looking and returns to normal, centered play
                // (swipes are for movement, dragging is for targeting)
                msg_log::clear();

                states::pop();

                // NOTE: This object is now destroyed

                game_commands::handle(game_cmd);

                return;
        }

        case GameCmd::toggle_aim:
        case GameCmd::toggle_throw: {
                // The action bar's target and throw buttons are MODE
                // TOGGLES: one of them engaged the targeting, and tapping
                // it again drops out of it. Either one closes whichever
                // mode is up - a button that opens a way of aiming can
                // never be the thing that looses the shot.
                //
                // NOTE: The bar buttons only - the [ fire ] / [ throw ]
                // context pins and the plain keys still loose it (see
                // game_commands::toggle_aim_key).
                if (!allow_swipe_cancel()) {
                        // Targeting that cannot be walked away from cannot
                        // be tapped away from either (a forced teleport,
                        // surveying the map mid character creation)
                        return;
                }

                msg_log::clear();

                states::pop();

                // NOTE: This object is now destroyed

                // A throw is aimed on top of the screen that chose the
                // item, so popping the marker alone would only step back
                // into the item list. The button cancels the whole FLOW -
                // back to play - since stepping back to the list is what
                // the [ cancel ] pin and the [ x ] control already do.
                //
                // NOTE: This is a no-op for an aim marker, which is opened
                // straight from the game state with nothing in between.
                const State* const below = states::current_state();

                if (below && (below->id() == StateId::inventory)) {
                        states::pop();
                }

                return;
        }

        default:
                break;
        }

        if (game_cmd != GameCmd::none) {
                msg_log::clear();
        }

        // Delegate to child classes (e.g. fire, throw, cancel)
        handle_input(input);
}

// The marker colors carry ONE system, across every kind of aiming:
//
//   hue        = what you are doing - green looking, orange throwing, red
//                attacking (see the marker_color_normal overrides)
//   gold       = it reaches, but with reduced effect (outside the item's
//                effective range - a weaker hit, half damage)
//   gray       = it does not reach at all (past the maximum range, or the
//                path is blocked) - inert, the same "unavailable" gray the
//                rest of the interface uses, and deliberately NOT a fourth
//                hue competing with the three above
const Color& MarkerState::marker_color_normal() const
{
        return colors::light_green();
}

const Color& MarkerState::marker_color_warning() const
{
        return colors::gold();
}

void MarkerState::draw_marker(
        const std::vector<P>& line,
        int warn_until_including_king_dist,
        int warn_from_king_dist,
        int out_of_range_from_king_dist,
        int blocked_from_idx)
{
        Color color = marker_color_normal();

        // Draw the line

        // NOTE: We include the head index in this loop, so that we can set up
        // which color it should be drawn with, but we do the actual drawing of
        // the head after the loop
        for (size_t line_idx = 0; line_idx < line.size(); ++line_idx) {
                const P& line_pos = line[line_idx];

                if (!viewport::is_in_view(line_pos)) {
                        continue;
                }

                const int dist = king_dist(m_origin, line_pos);

                const bool is_warn_by_near_dist =
                        (warn_until_including_king_dist != -1) &&
                        (dist <= warn_until_including_king_dist);

                const bool is_warn_by_far_dist =
                        (warn_from_king_dist != -1) &&
                        (dist >= warn_from_king_dist);

                const bool is_out_of_range =
                        (out_of_range_from_king_dist != -1) &&
                        (dist >= out_of_range_from_king_dist);

                const bool is_blocked =
                        (blocked_from_idx != -1) &&
                        ((int)line_idx >= blocked_from_idx);

                // NOTE: Final color is stored for drawing the head.
                if (is_blocked || is_out_of_range) {
                        // Nothing happens out here - the same gray whatever
                        // is being aimed (see the color system above)
                        color = colors::gray();
                }
                else if (is_warn_by_near_dist || is_warn_by_far_dist) {
                        color = marker_color_warning();
                }
                else {
                        color = marker_color_normal();
                }

                // Do not draw the head yet
                const int tail_size_int = (int)line.size() - 1;

                if ((int)line_idx < tail_size_int) {
                        const P view_pos = viewport::to_view_pos(line_pos);

                        io::MapDrawObj draw_obj;

                        draw_obj.pos = view_pos;
                        draw_obj.tile = gfx::TileId::aim_marker_line;
                        draw_obj.character = '*';
                        draw_obj.color = color;
                        draw_obj.color_bg = colors::black();

                        draw_obj.draw();
                }
        }  // line loop

        // Draw the head
        const P& head_pos = line.empty() ? m_origin : line.back();

        if (viewport::is_in_view(head_pos)) {
                // If we are currently only drawing the head and the line is
                // empty, draw the head in the warning color if the aiming
                // has a defined minimum effective range (if the line is
                // non-empty, the head color would be set by the line
                // drawing above)
                if (line.empty() && (warn_until_including_king_dist >= 0)) {
                        color = marker_color_warning();
                }

                const P view_pos = viewport::to_view_pos(head_pos);

                // Corner brackets overlaid on the cell, keeping the cell's
                // content (e.g. the targeted monster) visible - the color
                // carries the same semantics as the line
                draw_map::draw_reticle(view_pos, color);
        }
}

bool MarkerState::try_go_to_requested_pos()
{
        if (!s_has_requested_start_pos) {
                return false;
        }

        // NOTE: The request is consumed whether it can be honored or not -
        // it is meant for the very next marker, and must never leak into a
        // later one
        s_has_requested_start_pos = false;

        if (!map::is_pos_inside_map(s_requested_start_pos)) {
                return false;
        }

        m_pos = s_requested_start_pos;

        return true;
}

bool MarkerState::try_go_to_look_pin()
{
        if (!viewport::is_pan_active()) {
                return false;
        }

        const P pin_pos = viewport::center_map_pos();

        if (!map::is_pos_inside_map(pin_pos)) {
                return false;
        }

        m_pos = pin_pos;

        return true;
}

bool MarkerState::try_go_to_tgt()
{
        if (!actor::player_state::g_target) {
                return false;
        }

        const std::vector<actor::Actor*> seen_foes = actor::seen_foes(*map::g_player);

        if (!seen_foes.empty()) {
                for (actor::Actor* const actor : seen_foes) {
                        if (actor::player_state::g_target == actor) {
                                m_pos = actor->m_pos;

                                return true;
                        }
                }
        }

        return false;
}

void MarkerState::try_go_to_closest_enemy()
{
        const std::vector<actor::Actor*> seen_foes = actor::seen_foes(*map::g_player);

        std::vector<P> seen_foes_positions;

        seen_foes_positions.reserve(seen_foes.size());

        for (const auto* const actor : seen_foes) {
                seen_foes_positions.push_back(actor->m_pos);
        }

        // If player sees enemies, suggest one for targeting
        if (!seen_foes_positions.empty()) {
                m_pos = closest_pos(map::g_player->m_pos, seen_foes_positions);

                actor::player_state::g_target = map::living_actor_at(m_pos);
        }
}

// -----------------------------------------------------------------------------
// View state
// -----------------------------------------------------------------------------
void Viewing::on_moved()
{
        msg_log::clear();

        view::print_location_info_msgs(m_pos);

        // A visible monster at the marker can be described in detail -
        // offer it via a context pin (sends the view key)
        const actor::Actor* const actor = map::living_actor_at(m_pos);

        if (actor &&
            !actor::is_player(actor) &&
            actor::can_player_see_actor(*actor)) {
                context_pins::add("describe", game_commands::view_key());
        }

        context_pins::add(
                "cancel",
                (char)SDLK_ESCAPE,
                colors::menu_dark());
}

void Viewing::handle_input(const io::InputData& input)
{
        const GameCmd game_cmd = game_commands::to_cmd(input);

        if (game_cmd == GameCmd::look) {
                actor::Actor* const actor = map::living_actor_at(m_pos);

                if (actor &&
                    !actor::is_player(actor) &&
                    actor::can_player_see_actor(*actor)) {
                        msg_log::clear();

                        auto view_actor_descr = std::make_unique<ViewActorDescr>(*actor);

                        states::push(std::move(view_actor_descr));
                }
        }
        else if ((input.key == SDLK_ESCAPE) || (input.key == SDLK_SPACE)) {
                msg_log::clear();

                states::pop();
        }
}

// -----------------------------------------------------------------------------
// Aim marker state
// -----------------------------------------------------------------------------
void Aiming::on_moved()
{
        auto* const actor = map::living_actor_at(m_pos);

        const bool is_visible_monster =
                actor &&
                !actor::is_player(actor) &&
                actor::can_player_see_actor(*actor);

        // "Fire <weapon> at <target>."
        std::string tgt_name;

        if (is_visible_monster) {
                tgt_name = actor::name_the(*actor);
        }
        else if (map::g_seen.at(m_pos)) {
                tgt_name = map::g_terrain.at(m_pos)->name(Article::the);
        }
        else {
                tgt_name = "the darkness";
        }

        const std::string wpn_name =
                m_wpn.name(ItemNameType::plain, ItemNameInfo::none);

        msg_log::add(
                "Fire " + wpn_name + " at " + tgt_name + ".",
                colors::light_white(),
                MsgInterruptPlayer::no,
                MorePromptOnMsg::no,
                CopyToMsgHistory::no);

        // The targeting is engaged even when the shot cannot be taken -
        // that is what puts [ reload ] and [ swap weapon ] within reach of
        // a player holding an empty gun (see
        // game_commands::ranged_wpn_unfireable_reason). Say why instead of
        // talking about hit chances and range, and offer no [ fire ].
        const std::string unfireable_reason =
                game_commands::ranged_wpn_unfireable_reason(m_wpn);

        const bool can_fire = unfireable_reason.empty();

        if (!can_fire) {
                msg_log::add(
                        unfireable_reason,
                        colors::msg_note(),
                        MsgInterruptPlayer::no,
                        MorePromptOnMsg::no,
                        CopyToMsgHistory::no);
        }

        const int dist = king_dist(m_origin, m_pos);

        const bool is_in_max_range = (dist <= max_king_dist());

        if (can_fire && is_in_max_range) {
                if (is_visible_monster) {
                        RangedAttData att_data(
                                map::g_player,
                                m_origin,
                                actor->m_pos,  // Aim position
                                actor->m_pos,  // Current position
                                m_wpn);

                        const int hit_chance =
                                ability_roll::success_chance_pct_actual(
                                        att_data.hit_chance_tot);

                        msg_log::add(
                                std::to_string(hit_chance) + "% hit chance.",
                                colors::light_white(),
                                MsgInterruptPlayer::no,
                                MorePromptOnMsg::no,
                                CopyToMsgHistory::no);
                }

                const bool is_in_effective_range =
                        effective_king_dist_range()
                                .is_in_range(dist);

                if (!is_in_effective_range &&
                    (m_wpn.data().ranged.effective_range.max > 0)) {
                        msg_log::add(
                                ("Aiming outside effective weapon range "
                                 "(50% damage)."),
                                colors::msg_note(),
                                MsgInterruptPlayer::no,
                                MorePromptOnMsg::no,
                                CopyToMsgHistory::no);
                }
        }

        // NOTE: The pins are ordered by how often they are wanted, LEAST
        // first: the row flows from the thumb's corner (see context_pins),
        // so the last ones added land nearest the thumb. Getting the gun
        // ready comes first because it is the rarer errand - and it is
        // done from behind the sights, without giving up the aim, see
        // run_cmd_and_resume_aim.
        if (has_readied_wpn_to_swap_to()) {
                context_pins::add(
                        "swap weapon",
                        game_commands::swap_weapon_key());
        }

        if (can_reload_wpn(m_wpn)) {
                context_pins::add("reload", game_commands::reload_key());
        }

        if (can_fire && (m_pos != map::g_player->m_pos)) {
                context_pins::add("fire", game_commands::fire_key());
        }

        context_pins::add(
                "cancel",
                (char)SDLK_ESCAPE,
                colors::menu_dark());
}

void Aiming::handle_input(const io::InputData& input)
{
        GameCmd game_cmd = GameCmd::undefined;

        if (config::is_bot_playing()) {
                // Bot is playing, fire at a random position
                game_cmd = GameCmd::fire;

                m_pos.set(
                        rnd::range(0, map::w() - 1),
                        rnd::range(0, map::h() - 1));
        }
        else {
                // Human player
                game_cmd = game_commands::to_cmd(input);
        }

        if ((game_cmd == GameCmd::swap_weapon) &&
            has_readied_wpn_to_swap_to()) {
                run_cmd_and_resume_aim(GameCmd::swap_weapon, m_pos);

                // NOTE: This object is now destroyed
                return;
        }

        if ((game_cmd == GameCmd::reload) && can_reload_wpn(m_wpn)) {
                run_cmd_and_resume_aim(GameCmd::reload, m_pos);

                // NOTE: This object is now destroyed
                return;
        }

        if ((game_cmd == GameCmd::fire) || (input.key == SDLK_RETURN)) {
                if (m_pos == map::g_player->m_pos) {
                        return;
                }

                // There is no [ fire ] pin when the shot cannot be taken,
                // but the fire key can still ask for it - and so can a
                // stray tap, which synthesizes the confirm key. Stay in
                // the targeting, where the pins that DO something about it
                // are.
                //
                // NOTE: The log was cleared for this input (see
                // MarkerState::update), so the cell's messages and pins
                // have to be put back - reason included.
                if (!game_commands::ranged_wpn_unfireable_reason(m_wpn)
                             .empty()) {
                        on_moved();

                        return;
                }

                msg_log::clear();

                actor::Actor* const actor = map::living_actor_at(m_pos);

                if (actor && actor::can_player_see_actor(*actor)) {
                        actor::player_state::g_target = actor;
                }

                const P pos = m_pos;

                item::Wpn* const wpn = &m_wpn;

                states::pop();

                // NOTE: This object is now destroyed

                attack::ranged(map::g_player, map::g_player->m_pos, pos, *wpn);
        }
        else if ((input.key == SDLK_ESCAPE) || (input.key == SDLK_SPACE)) {
                states::pop();
        }
}

const Color& Aiming::marker_color_normal() const
{
        // An attack being aimed is red - the line of fire and its reticle
        // are told apart from the green look pin and the orange throw at a
        // glance (see the color system in MarkerState)
        return colors::light_red();
}

Range Aiming::effective_king_dist_range() const
{
        return m_wpn.data().ranged.effective_range;
}

int Aiming::max_king_dist() const
{
        return m_wpn.data().ranged.max_range;
}

// -----------------------------------------------------------------------------
// Aim melee weapon marker state
// -----------------------------------------------------------------------------
void AimingMeleeWpn::on_moved()
{
        actor::Actor* const actor = map::living_actor_at(m_pos);

        const bool is_visible_monster =
                actor &&
                !actor::is_player(actor) &&
                actor::can_player_see_actor(*actor);

        // "Attack <target> with <weapon>."
        std::string tgt_name;

        if (is_visible_monster) {
                tgt_name = actor::name_the(*actor);
        }
        else if (map::g_seen.at(m_pos)) {
                tgt_name = map::g_terrain.at(m_pos)->name(Article::the);
        }
        else {
                tgt_name = "the darkness";
        }

        const std::string wpn_name =
                m_wpn.name(ItemNameType::plain, ItemNameInfo::none);

        msg_log::add(
                "Attack " + tgt_name + " with " + wpn_name + ".",
                colors::light_white(),
                MsgInterruptPlayer::no,
                MorePromptOnMsg::no,
                CopyToMsgHistory::no);

        const int dist = king_dist(m_origin, m_pos);

        const bool is_in_max_range = (dist <= max_king_dist());

        if (is_in_max_range && is_visible_monster) {
                MeleeAttData att_data(map::g_player, *actor, m_wpn);

                const int hit_chance =
                        ability_roll::success_chance_pct_actual(
                                att_data.hit_chance_tot);

                msg_log::add(
                        std::to_string(hit_chance) + "% hit chance.",
                        colors::light_white(),
                        MsgInterruptPlayer::no,
                        MorePromptOnMsg::no,
                        CopyToMsgHistory::no);
        }
        else if (!is_in_max_range) {
                msg_log::add(
                        "Out of reach.",
                        colors::msg_note(),
                        MsgInterruptPlayer::no,
                        MorePromptOnMsg::no,
                        CopyToMsgHistory::no);
        }

        // Aiming a melee weapon is the same targeting mode, so it swaps
        // the same way - and it is what makes the swap reversible, since
        // going from a firearm to a melee weapon lands here.
        //
        // NOTE: Before the strike, so that the strike lands nearest the
        // thumb - see the pin order in Aiming::on_moved.
        if (has_readied_wpn_to_swap_to()) {
                context_pins::add(
                        "swap weapon",
                        game_commands::swap_weapon_key());
        }

        // Striking is only possible within the weapon's reach
        if ((m_pos != map::g_player->m_pos) && is_in_max_range) {
                context_pins::add("attack", game_commands::fire_key());
        }

        context_pins::add(
                "cancel",
                (char)SDLK_ESCAPE,
                colors::menu_dark());
}

void AimingMeleeWpn::handle_input(const io::InputData& input)
{
        GameCmd game_cmd = GameCmd::undefined;

        if (config::is_bot_playing()) {
                // Bot is playing, strike at a random position
                game_cmd = GameCmd::fire;

                m_pos.set(
                        rnd::range(0, map::w() - 1),
                        rnd::range(0, map::h() - 1));
        }
        else {
                // Human player
                game_cmd = game_commands::to_cmd(input);
        }

        if ((game_cmd == GameCmd::swap_weapon) &&
            has_readied_wpn_to_swap_to()) {
                run_cmd_and_resume_aim(GameCmd::swap_weapon, m_pos);

                // NOTE: This object is now destroyed
                return;
        }

        if ((game_cmd == GameCmd::fire) || (input.key == SDLK_RETURN)) {
                if (m_pos == map::g_player->m_pos) {
                        return;
                }

                const int dist = king_dist(m_origin, m_pos);

                const bool is_in_max_range = (dist <= max_king_dist());

                if (!is_in_max_range) {
                        msg_log::add(
                                "Out of reach.",
                                colors::msg_note(),
                                MsgInterruptPlayer::no,
                                MorePromptOnMsg::no,
                                CopyToMsgHistory::no);

                        context_pins::add(
                                "cancel",
                                (char)SDLK_ESCAPE,
                                colors::menu_dark());

                        return;
                }

                msg_log::clear();

                actor::Actor* const actor = map::living_actor_at(m_pos);

                if (actor && actor::can_player_see_actor(*actor)) {
                        actor::player_state::g_target = actor;
                }

                const P aim_pos = m_pos;
                item::Wpn* const wpn = &m_wpn;

                states::pop();

                // NOTE: This object is now destroyed

                attack::melee(map::g_player, map::g_player->m_pos, aim_pos, *wpn);
        }
        else if ((input.key == SDLK_ESCAPE) || (input.key == SDLK_SPACE)) {
                states::pop();
        }
}

bool AimingMeleeWpn::is_pos_blocked(const P& pos) const
{
        // NOTE: It is possible to attack through terrain that projectiles can
        // pass through (such as barred gates).
        return !map::g_terrain.at(pos)->is_projectile_passable();
}

const Color& AimingMeleeWpn::marker_color_normal() const
{
        // Striking at weapon reach is an attack like any other (see Aiming)
        return colors::light_red();
}

int AimingMeleeWpn::max_king_dist() const
{
        return m_wpn.data().melee.reach;
}

// -----------------------------------------------------------------------------
// Throw attack marker state
// -----------------------------------------------------------------------------
void Throwing::on_moved()
{
        auto* const actor = map::living_actor_at(m_pos);

        const bool is_visible_monster =
                actor &&
                !actor::is_player(actor) &&
                actor::can_player_see_actor(*actor);

        // "Throw <item> at <monster>." / "Throw <item> onto <terrain>."
        const std::string item_name =
                m_inv_item->name(
                        ItemNameType::a,
                        ItemNameInfo::none,
                        ItemNameAttackInfo::none);

        std::string msg = "Throw " + item_name;

        if (m_pos == map::g_player->m_pos) {
                // The marker was not opened from drag-to-look, so it starts
                // on the player - there is nothing to throw at yet
                msg += " - drag to aim";
        }
        else if (is_visible_monster) {
                msg += " at " + actor::name_the(*actor);
        }
        else if (map::g_seen.at(m_pos)) {
                msg += " onto " + map::g_terrain.at(m_pos)->name(Article::the);
        }
        else {
                msg += " into the darkness";
        }

        msg_log::add(
                msg + ".",
                colors::light_white(),
                MsgInterruptPlayer::no,
                MorePromptOnMsg::no,
                CopyToMsgHistory::no);

        const int dist = king_dist(m_origin, m_pos);

        const bool is_in_max_range = (dist <= max_king_dist());

        if (is_in_max_range) {
                if (is_visible_monster) {
                        ThrowAttData att_data(
                                map::g_player,
                                m_origin,
                                actor->m_pos,  // Aim position
                                actor->m_pos,  // Current position
                                *m_inv_item);

                        const int hit_chance =
                                ability_roll::success_chance_pct_actual(
                                        att_data.hit_chance_tot);

                        msg_log::add(
                                std::to_string(hit_chance) + "% hit chance.",
                                colors::light_white(),
                                MsgInterruptPlayer::no,
                                MorePromptOnMsg::no,
                                CopyToMsgHistory::no);
                }

                const bool is_in_effective_range =
                        effective_king_dist_range()
                                .is_in_range(dist);

                if (!is_in_effective_range &&
                    (m_inv_item->data().ranged.effective_range.max > 0)) {
                        msg_log::add(
                                ("Aiming outside effective weapon range "
                                 "(50% damage)."),
                                colors::msg_note(),
                                MsgInterruptPlayer::no,
                                MorePromptOnMsg::no,
                                CopyToMsgHistory::no);
                }
        }

        if (m_pos != map::g_player->m_pos) {
                context_pins::add("throw", game_commands::throw_key());
        }

        context_pins::add(
                "cancel",
                (char)SDLK_ESCAPE,
                colors::menu_dark());
}

void Throwing::handle_input(const io::InputData& input)
{
        const GameCmd game_cmd = game_commands::to_cmd(input);

        if ((game_cmd == GameCmd::throw_item) ||
            (input.key == SDLK_RETURN)) {
                if (m_pos == map::g_player->m_pos) {
                        return;
                }

                msg_log::clear();

                actor::Actor* const actor = map::living_actor_at(m_pos);

                if (actor && actor::can_player_see_actor(*actor)) {
                        actor::player_state::g_target = actor;
                }

                item::Item* item_to_throw = item::copy_item(*m_inv_item);

                item_to_throw->m_nr_items = 1;

                item_to_throw->clear_actor_carrying();

                m_inv_item = map::g_player->m_inv.decr_item(m_inv_item);

                actor::player_state::g_last_thrown_item = m_inv_item;

                const P pos = m_pos;

                states::pop();

                // NOTE: This object is now destroyed

                // Perform the actual throwing
                throwing::throw_item(*map::g_player, pos, *item_to_throw);
        }
        else if ((input.key == SDLK_ESCAPE) || (input.key == SDLK_SPACE)) {
                states::pop();
        }
}

const Color& Throwing::marker_color_normal() const
{
        // Orange all the way out to the maximum throwing range - the throw
        // path and reticle are told apart from the green look pin and the
        // red attack at a glance. NOTE: The warning color (gold, the throw
        // landing for half damage) and the out of range color are the
        // standard ones, see MarkerState.
        return colors::orange();
}

Range Throwing::effective_king_dist_range() const
{
        return m_inv_item->data().ranged.effective_range;
}

int Throwing::max_king_dist() const
{
        return m_inv_item->data().ranged.max_range;
}

// -----------------------------------------------------------------------------
// Throw explosive marker state
// -----------------------------------------------------------------------------
void ThrowingExplosive::on_draw()
{
        const auto id = m_explosive.id();

        if ((id != item::Id::dynamite) &&
            (id != item::Id::molotov) &&
            (id != item::Id::smoke_grenade)) {
                return;
        }

        const R expl_area =
                explosion::explosion_area_outside_map_allowed(
                        m_pos,
                        g_expl_std_radi);

        const Color color = colors::red();

        io::set_display_for_panel(Panel::map);

        // Draw explosion radius area overlay
        for (int y = expl_area.p0.y; y <= expl_area.p1.y; ++y) {
                for (int x = expl_area.p0.x; x <= expl_area.p1.x; ++x) {
                        const P p(x, y);

                        if (!viewport::is_in_view(p)) {
                                continue;
                        }

                        const P view_pos = viewport::to_view_pos(p);

                        const P px_pos =
                                io::map_to_px_coords(Panel::map, view_pos);

                        const P px_dims(
                                config::map_cell_px_w(),
                                config::map_cell_px_h());

                        io::draw_rectangle_filled(
                                {px_pos, px_pos + px_dims - 1},
                                color,
                                80);
                }
        }
}

void ThrowingExplosive::on_moved()
{
        auto* const actor = map::living_actor_at(m_pos);

        const bool is_visible_monster =
                actor &&
                !actor::is_player(actor) &&
                actor::can_player_see_actor(*actor);

        // "Throw <explosive> at <monster>." / "... onto <terrain>."
        const std::string name =
                m_explosive.name(ItemNameType::a, ItemNameInfo::none);

        std::string msg = "Throw " + name;

        if (is_visible_monster) {
                msg += " at " + actor::name_the(*actor);
        }
        else if (map::g_seen.at(m_pos)) {
                msg += " onto " + map::g_terrain.at(m_pos)->name(Article::the);
        }
        else {
                msg += " into the darkness";
        }

        msg_log::add(
                msg + ".",
                colors::light_white(),
                MsgInterruptPlayer::no,
                MorePromptOnMsg::no,
                CopyToMsgHistory::no);

        context_pins::add("throw", game_commands::throw_key());

        context_pins::add(
                "cancel",
                (char)SDLK_ESCAPE,
                colors::menu_dark());
}

void ThrowingExplosive::handle_input(const io::InputData& input)
{
        const GameCmd game_cmd = game_commands::to_cmd(input);

        if ((game_cmd == GameCmd::throw_item) || (input.key == SDLK_RETURN)) {
                msg_log::clear();

                const P pos = m_pos;

                auto* const explosive = &m_explosive;

                states::pop();

                // NOTE: This object is now destroyed

                throwing::player_throw_lit_explosive(pos, *explosive);
        }
        else if ((input.key == SDLK_ESCAPE) || (input.key == SDLK_SPACE)) {
                states::pop();
        }
}

const Color& ThrowingExplosive::marker_color_normal() const
{
        // A lit explosive is thrown like anything else (see Throwing)
        return colors::orange();
}

int ThrowingExplosive::max_king_dist() const
{
        return m_explosive.data().ranged.max_range;
}

// -----------------------------------------------------------------------------
// Teleport control marker state
// -----------------------------------------------------------------------------
CtrlTele::CtrlTele(const P& origin, Array2<bool> blocked, const int max_dist) :
        MarkerState(origin),
        m_origin(origin),
        m_max_dist(max_dist),
        m_blocked(std::move(blocked))
{
}

int CtrlTele::chance_of_success_pct() const
{
        const int dist = king_dist(map::g_player->m_pos, m_pos);

        if ((m_max_dist > 0) && (dist > m_max_dist)) {
                // Target is too far away
                return 0;
        }
        else {
                return std::clamp(100 - dist, 25, 95);
        }
}

void CtrlTele::on_start_hook()
{
        msg_log::add(
                "I can control where I teleport.",
                colors::white(),
                MsgInterruptPlayer::no,
                MorePromptOnMsg::yes,
                CopyToMsgHistory::yes);
}

void CtrlTele::on_moved()
{
        view::print_location_info_msgs(m_pos);

        if (m_pos != map::g_player->m_pos) {
                const int chance_pct = chance_of_success_pct();

                msg_log::add(
                        std::to_string(chance_pct) + "% chance of success.",
                        colors::light_white(),
                        MsgInterruptPlayer::no,
                        MorePromptOnMsg::no,
                        CopyToMsgHistory::no);

                context_pins::add("teleport here", (char)SDLK_RETURN);
        }
}

void CtrlTele::handle_input(const io::InputData& input)
{
        if ((input.key != SDLK_RETURN) || (m_pos == map::g_player->m_pos)) {
                return;
        }

        const int chance = chance_of_success_pct();

        const bool roll_ok = rnd::percent(chance);

        const bool is_success =
                roll_ok &&
                m_blocked.rect().is_pos_inside(m_pos) &&
                !m_blocked.at(m_pos);

        // Copy data before object is deleted.
        const int max_dist = m_max_dist;
        const P tgt_p = m_pos;
        const Array2<bool> blocked = m_blocked;

        states::pop();

        // NOTE: This object is now destroyed

        if (is_success) {
                // Teleport to this exact destination.
                teleport(*map::g_player, tgt_p, blocked, true);
        }
        else {
                // Failed to teleport (blocked or roll failed)
                msg_log::add(
                        "I failed to go there...",
                        colors::white(),
                        MsgInterruptPlayer::no,
                        MorePromptOnMsg::yes,
                        CopyToMsgHistory::yes);

                // Run a randomized teleport with no teleport control.
                teleport(*map::g_player, ShouldCtrlTele::never, max_dist);
        }
}

// -----------------------------------------------------------------------------
// Control Object marker state
// -----------------------------------------------------------------------------
bool CtrlObjOpen::can_control(
        const terrain::Terrain& terrain,
        const SpellSkill skill) const
{
        (void)skill;

        switch (terrain.id()) {
        case terrain::Id::chest: {
                return !static_cast<const terrain::Chest&>(terrain).is_open();
        } break;

        case terrain::Id::cabinet: {
                return !static_cast<const terrain::Cabinet&>(terrain).is_open();
        } break;

        case terrain::Id::tomb: {
                return !static_cast<const terrain::Tomb&>(terrain).is_open();
        } break;

        case terrain::Id::door: {
                const auto& door = static_cast<const terrain::Door&>(terrain);

                if (door.is_open() || door.is_hidden()) {
                        return false;
                }
                else {
                        return !door.is_known_stuck();
                }
        } break;

        default:
        {
        } break;
        }

        return false;
}

DidAction CtrlObjOpen::run(
        terrain::Terrain& terrain,
        const SpellSkill skill) const
{
        if (terrain.id() == terrain::Id::door) {
                auto& door = static_cast<terrain::Door&>(terrain);

                if (door.is_stuck()) {
                        ASSERT(!door.is_known_stuck());

                        door.reveal_stuck_status(terrain::PrintRevealMsg::if_seen);

                        return DidAction::yes;
                }
        }

        spells::run_opening_spell_effect_at(terrain.pos(), skill);

        return DidAction::yes;
}

std::string CtrlObjOpen::menu_label(const terrain::Terrain& terrain) const
{
        const std::string name = terrain.name(Article::the);

        return "(o) Open " + name;
}

char CtrlObjOpen::menu_key() const
{
        return 'o';
}

bool CtrlObjCloseDoor::can_control(
        const terrain::Terrain& terrain,
        const SpellSkill skill) const
{
        if (terrain.id() != terrain::Id::door) {
                return false;
        }

        const auto& door = static_cast<const terrain::Door&>(terrain);

        if (!door.is_open()) {
                return false;
        }

        if (door.is_hidden()) {
                return false;
        }

        const bool is_metal = (door.type() == terrain::DoorType::metal);
        const bool is_basic_skill = (skill == SpellSkill::basic);

        if (is_metal && is_basic_skill) {
                return false;
        }

        return true;
}

DidAction CtrlObjCloseDoor::run(
        terrain::Terrain& terrain,
        const SpellSkill skill) const
{
        const actor::Actor* const actor_here = map::living_actor_at(terrain.pos());

        if (actor_here) {
                std::string actor_name;

                if (actor::can_player_see_actor(*actor_here)) {
                        actor_name =
                                text_format::first_to_upper(
                                        actor::name_the(
                                                *actor_here));
                }
                else {
                        actor_name = "Something";
                }

                const std::string terrain_name = terrain.name(Article::the);

                msg_log::add(
                        actor_name +
                        " prevents closing " +
                        terrain_name +
                        ".");

                return DidAction::no;
        }

        spells::run_close_spell_effect_at(terrain.pos(), skill);

        return DidAction::yes;
}

std::string CtrlObjCloseDoor::menu_label(const terrain::Terrain& terrain) const
{
        const std::string name = terrain.name(Article::the);

        return "(c) Close " + name;
}

char CtrlObjCloseDoor::menu_key() const
{
        return 'c';
}

bool CtrlObjJamDoor::can_control(
        const terrain::Terrain& terrain,
        const SpellSkill skill) const
{
        (void)skill;

        if (terrain.id() != terrain::Id::door) {
                return false;
        }

        const auto& door = static_cast<const terrain::Door&>(terrain);
        const bool is_metal = door.type() == terrain::DoorType::metal;

        return (
                !door.is_open() &&
                !door.is_hidden() &&
                !door.is_known_stuck() &&
                !is_metal);
}

DidAction CtrlObjJamDoor::run(
        terrain::Terrain& terrain,
        const SpellSkill skill) const
{
        (void)skill;

        auto& door = static_cast<terrain::Door&>(terrain);

        const auto name_the =
                text_format::first_to_upper(
                        door.name(Article::the));

        msg_log::add(name_the + " is jammed.");

        door.jam(map::g_player);

        return DidAction::yes;
}

std::string CtrlObjJamDoor::menu_label(const terrain::Terrain& terrain) const
{
        const std::string name = terrain.name(Article::the);

        return "(c) Jam " + name;
}

char CtrlObjJamDoor::menu_key() const
{
        return 'c';
}

bool CtrlObjDeactivateCrystal::can_control(
        const terrain::Terrain& terrain,
        const SpellSkill skill) const
{
        (void)skill;

        if (terrain.id() != terrain::Id::crystal_key) {
                return false;
        }

        const auto& crystal = static_cast<const terrain::CrystalKey&>(terrain);

        return crystal.is_active();
}

DidAction CtrlObjDeactivateCrystal::run(
        terrain::Terrain& terrain,
        const SpellSkill skill) const
{
        (void)skill;

        auto& lever = static_cast<terrain::CrystalKey&>(terrain);

        const std::string name_the =
                text_format::first_to_upper(
                        lever.name(Article::the));

        msg_log::add(name_the + " is deactivated.");

        lever.player_deactivate();

        return DidAction::yes;
}

std::string CtrlObjDeactivateCrystal::menu_label(
        const terrain::Terrain& terrain) const
{
        (void)terrain;

        return "(d) Deactivate crystal";
}

char CtrlObjDeactivateCrystal::menu_key() const
{
        return 'd';
}

bool CtrlObjStrike::can_control(
        const terrain::Terrain& terrain,
        const SpellSkill skill) const
{
        (void)skill;

        switch (terrain.id()) {
        case terrain::Id::door: {
                const auto& door = static_cast<const terrain::Door&>(terrain);
                const bool is_metal = (door.type() == terrain::DoorType::metal);

                return (
                        !door.is_open() &&
                        !door.is_hidden() &&
                        !is_metal);
        } break;

        case terrain::Id::brazier:
        case terrain::Id::statue:
        case terrain::Id::urn: {
                return true;
        } break;

        default: {
        } break;
        }

        return false;
}

DidAction CtrlObjStrike::run(
        terrain::Terrain& terrain,
        const SpellSkill skill) const
{
        (void)skill;

        switch (terrain.id()) {
        case terrain::Id::door: {
                const int dmg = 15;

                terrain.hit(
                        DmgType::control_object_spell,
                        map::g_player,
                        terrain.pos(),
                        dmg);

                return DidAction::yes;
        } break;

        case terrain::Id::brazier:
        case terrain::Id::statue:
        case terrain::Id::urn: {
                const std::string query_msg =
                        common_text::g_direction_query +
                        " " +
                        common_text::g_cancel_hint;

                msg_log::add(
                        query_msg,
                        colors::light_white(),
                        MsgInterruptPlayer::no,
                        MorePromptOnMsg::no,
                        CopyToMsgHistory::no);

                const auto input_dir = query::dir(AllowCenter::no);

                msg_log::clear();

                if (input_dir == Dir::END) {
                        return DidAction::no;
                }

                const auto from_pos = terrain.pos() - input_dir;

                const int dmg = 15;

                terrain.hit(
                        DmgType::control_object_spell,
                        map::g_player,
                        from_pos,
                        dmg);

                return DidAction::yes;
        } break;

        default:
        {
        } break;
        }

        ASSERT(false);

        return DidAction::no;
}

std::string CtrlObjStrike::menu_label(const terrain::Terrain& terrain) const
{
        const std::string name = terrain.name(Article::the);

        return "(w) Strike " + name;
}

char CtrlObjStrike::menu_key() const
{
        return 'w';
}

bool CtrlObjDestrWall::can_control(
        const terrain::Terrain& terrain,
        const SpellSkill skill) const
{
        if (skill != SpellSkill::transcendent) {
                return false;
        }

        switch (terrain.id()) {
        case terrain::Id::wall:
        case terrain::Id::rubble_high: {
                return true;
        } break;

        default:
        {
        } break;
        }

        return false;
}

DidAction CtrlObjDestrWall::run(
        terrain::Terrain& terrain,
        const SpellSkill skill) const
{
        (void)skill;

        if (!map::is_pos_inside_outer_walls(terrain.pos())) {
                msg_log::add("Nothing happens.");

                return DidAction::yes;
        }

        switch (terrain.id()) {
        case terrain::Id::door: {
                // NOTE: The door is hidden.
                msg_log::add("Nothing happens.");

                return DidAction::yes;
        } break;

        case terrain::Id::wall:
        case terrain::Id::rubble_high: {
                terrain.hit(DmgType::pure, map::g_player);

                return DidAction::yes;
        };

        default:
        {
        } break;
        }

        ASSERT(false);

        return DidAction::no;
}

std::string CtrlObjDestrWall::menu_label(const terrain::Terrain& terrain) const
{
        const std::string name = terrain.name(Article::the);

        return "(w) Destroy " + name;
}

char CtrlObjDestrWall::menu_key() const
{
        return 'w';
}

CtrlObj::CtrlObj(const P& origin, const int max_dist, SpellSkill skill) :
        MarkerState(origin),
        m_origin(origin),
        m_max_dist(max_dist),
        m_skill(skill)
{
}

int CtrlObj::current_dist() const
{
        return king_dist(map::g_player->m_pos, m_pos);
}

bool CtrlObj::is_allowed_at_dist() const
{
        const int d = current_dist();

        return ((d != 0) && (d <= m_max_dist));
}

void CtrlObj::on_start_hook()
{
        msg_log::add(
                "Select an object to control.",
                colors::white(),
                MsgInterruptPlayer::no,
                MorePromptOnMsg::yes,
                CopyToMsgHistory::no);

        set_terrain();
        set_possible_actions();
}

void CtrlObj::on_moved()
{
        set_terrain();
        set_possible_actions();

        view::print_location_info_msgs(m_pos);

        const std::string dist_str = std::to_string(current_dist());
        const std::string max_dist_str = std::to_string(m_max_dist);

        const auto dist_msg_color =
                is_allowed_at_dist()
                ? colors::msg_good()
                : colors::msg_bad();

        const std::string dist_msg =
                "Distance: " +
                dist_str +
                "/" +
                max_dist_str;

        msg_log::add(
                dist_msg,
                dist_msg_color,
                MsgInterruptPlayer::no,
                MorePromptOnMsg::no,
                CopyToMsgHistory::no);

        if (is_allowed_at_dist() && !m_possible_actions.empty()) {
                context_pins::add("control", (char)SDLK_RETURN);
        }

        context_pins::add(
                "cancel",
                (char)SDLK_ESCAPE,
                colors::menu_dark());
}

void CtrlObj::handle_input(const io::InputData& input)
{
        if ((input.key == SDLK_ESCAPE) || (input.key == SDLK_SPACE)) {
                msg_log::clear();

                states::pop();
        }

        if (input.key != SDLK_RETURN) {
                return;
        }

        if (!map::g_seen.at(m_pos)) {
                msg_log::add(
                        "I have no vision here.",
                        colors::white(),
                        MsgInterruptPlayer::no,
                        MorePromptOnMsg::no,
                        CopyToMsgHistory::no);

                return;
        }

        if (!is_allowed_at_dist()) {
                const std::string msg =
                        (current_dist() == 0)
                        ? "The distance is too small."
                        : "The distance is too great.";

                msg_log::add(
                        msg,
                        colors::white(),
                        MsgInterruptPlayer::no,
                        MorePromptOnMsg::no,
                        CopyToMsgHistory::no);

                return;
        }

        if (m_possible_actions.empty()) {
                msg_log::add(
                        "I cannot control any object here.",
                        colors::white(),
                        MsgInterruptPlayer::no,
                        MorePromptOnMsg::no,
                        CopyToMsgHistory::no);

                return;
        }

        const CtrlObjActionPtr action = query_control();

        if (!action) {
                return;
        }

        m_allow_draw = false;
        const DidAction did_action = action->run(*m_terrain, m_skill);
        m_allow_draw = true;

        if (did_action == DidAction::yes) {
                states::pop();
        }
}

void CtrlObj::set_terrain()
{
        m_terrain = map::g_terrain.at(m_pos);
}

void CtrlObj::set_possible_actions()
{
        std::vector<CtrlObjActionPtr> all_actions;

        // ---------------------------------------------------------------------
        // Add all possible control actions here
        // ---------------------------------------------------------------------
        all_actions.emplace_back(std::make_shared<CtrlObjOpen>());
        all_actions.emplace_back(std::make_shared<CtrlObjCloseDoor>());
        all_actions.emplace_back(std::make_shared<CtrlObjJamDoor>());
        all_actions.emplace_back(std::make_shared<CtrlObjDeactivateCrystal>());
        all_actions.emplace_back(std::make_shared<CtrlObjStrike>());
        all_actions.emplace_back(std::make_shared<CtrlObjDestrWall>());
        // ---------------------------------------------------------------------

        m_possible_actions.clear();

        m_possible_actions.reserve(all_actions.size());

        std::copy_if(
                std::begin(all_actions),
                std::end(all_actions),
                std::back_inserter(m_possible_actions),
                [this](auto& action) {
                        return action->can_control(*m_terrain, m_skill);
                });

        std::sort(
                std::begin(m_possible_actions),
                std::end(m_possible_actions),
                [](const auto& a1, const auto& a2) {
                        return a1->menu_key() < a2->menu_key();
                });
}

CtrlObjActionPtr CtrlObj::query_control() const
{
        popup::Popup popup(popup::AddToMsgHistory::no);

        std::vector<std::string> menu_labels;

        menu_labels.reserve(m_possible_actions.size());

        for (const CtrlObjActionPtr& action : m_possible_actions) {
                menu_labels.push_back(action->menu_label(*m_terrain));
        }

        menu_labels.emplace_back("(space, esc) Choose another position");

        int choice = 0;

        popup.setup_menu_mode(
                menu_labels,
                &choice);

        popup.set_title("Control object");

        popup.run();

        if ((choice == -1) || (choice == (int)m_possible_actions.size())) {
                return {};
        }
        else {
                return m_possible_actions[choice];
        }
}

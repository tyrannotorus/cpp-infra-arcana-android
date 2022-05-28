// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
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
#include "actor_player.hpp"
#include "actor_see.hpp"
#include "attack.hpp"
#include "attack_data.hpp"
#include "colors.hpp"
#include "common_text.hpp"
#include "config.hpp"
#include "debug.hpp"
#include "explosion.hpp"
#include "game_commands.hpp"
#include "gfx.hpp"
#include "inventory.hpp"
#include "io.hpp"
#include "item.hpp"
#include "item_data.hpp"
#include "item_factory.hpp"
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
#include "terrain_pylon.hpp"
#include "text_format.hpp"
#include "throwing.hpp"
#include "view.hpp"
#include "view_actor_descr.hpp"
#include "viewport.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------

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

        if (use_player_tgt())
        {
                // First, attempt to place marker at player target.
                const bool did_go_to_tgt = try_go_to_tgt();

                if (!did_go_to_tgt)
                {
                        // If no target available, attempt to place marker at
                        // closest visible monster. This sets a new player
                        // target if successful.
                        map::g_player->m_tgt = nullptr;

                        try_go_to_closest_enemy();
                }
        }

        on_start_hook();

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
}

void MarkerState::draw()
{
        if (!m_allow_draw)
        {
                return;
        }

        if (!viewport::is_in_view(m_pos))
        {
                viewport::show(m_pos, viewport::ForceCentering::yes);
        }

        auto line =
                line_calc::calc_new_line(
                        m_origin,
                        m_pos,
                        true,  // Stop at target
                        INT_MAX,  // Travel limit
                        true);  // Allow outside map

        // Remove origin position
        if (!line.empty())
        {
                line.erase(std::begin(line));
        }

        const auto effective_dist_range = effective_king_dist_range();

        const int orange_until_including_dist =
                (effective_dist_range.min == -1)
                ? -1
                : (effective_dist_range.min - 1);

        const int orange_from_dist =
                (effective_dist_range.max == -1)
                ? -1
                : (effective_dist_range.max + 1);

        auto red_from_dist = max_king_dist();

        int red_from_idx = -1;

        auto blocked_parser = map_parsers::BlocksProjectiles();

        if (show_blocked())
        {
                for (size_t i = 0; i < line.size(); ++i)
                {
                        const P& p = line[i];

                        if (!map::is_pos_inside_map(p))
                        {
                                break;
                        }

                        if (map::g_seen.at(p) && blocked_parser.run(p))
                        {
                                red_from_idx = (int)i;
                                break;
                        }
                }
        }

        draw_marker(
                line,
                orange_until_including_dist,
                orange_from_dist,
                red_from_dist,
                red_from_idx);

        on_draw();
}

void MarkerState::update()
{
        const int nr_jump_steps = 5;

        io::InputData input;

        if (!config::is_bot_playing())
        {
                input = io::read_input();
        }

        const auto game_cmd = game_commands::to_cmd(input);

        if (game_cmd != GameCmd::none)
        {
                msg_log::clear();
        }

        switch (game_cmd)
        {
        case GameCmd::right:
                move(Dir::right);
                break;

        case GameCmd::down:
                move(Dir::down);
                break;

        case GameCmd::left:
                move(Dir::left);
                break;

        case GameCmd::up:
                move(Dir::up);
                break;

        case GameCmd::up_right:
                move(Dir::up_right);
                break;

        case GameCmd::down_right:
                move(Dir::down_right);
                break;

        case GameCmd::up_left:
                move(Dir::up_left);
                break;

        case GameCmd::down_left:
                move(Dir::down_left);
                break;

        case GameCmd::auto_move_right:
                move(Dir::right, nr_jump_steps);
                break;

        case GameCmd::auto_move_down:
                move(Dir::down, nr_jump_steps);
                break;

        case GameCmd::auto_move_left:
                move(Dir::left, nr_jump_steps);
                break;

        case GameCmd::auto_move_up:
                move(Dir::up, nr_jump_steps);
                break;

        case GameCmd::auto_move_up_right:
                move(Dir::up_right, nr_jump_steps);
                break;

        case GameCmd::auto_move_down_right:
                move(Dir::down_right, nr_jump_steps);
                break;

        case GameCmd::auto_move_up_left:
                move(Dir::up_left, nr_jump_steps);
                break;

        case GameCmd::auto_move_down_left:
                move(Dir::down_left, nr_jump_steps);
                break;

        default:
                // Input not handled here - delegate to child classes
                handle_input(input);
        }
}

void MarkerState::draw_marker(
        const std::vector<P>& line,
        int orange_until_including_king_dist,
        int orange_from_king_dist,
        int red_from_king_dist,
        int red_from_idx)
{
        auto color = colors::light_green();

        // Draw the line

        // NOTE: We include the head index in this loop, so that we can set up
        // which color it should be drawn with, but we do the actual drawing of
        // the head after the loop
        for (size_t line_idx = 0; line_idx < line.size(); ++line_idx)
        {
                const P& line_pos = line[line_idx];

                if (!viewport::is_in_view(line_pos))
                {
                        continue;
                }

                const int dist = king_dist(m_origin, line_pos);

                const bool is_near_orange =
                        (orange_until_including_king_dist != -1) &&
                        (dist <= orange_until_including_king_dist);

                const bool is_far_orange =
                        (orange_from_king_dist != -1) &&
                        (dist >= orange_from_king_dist);

                const bool is_red_by_dist =
                        (red_from_king_dist != -1) &&
                        (dist >= red_from_king_dist);

                const bool is_red_by_idx =
                        (red_from_idx != -1) &&
                        ((int)line_idx >= red_from_idx);

                // NOTE: Final color is stored for drawing the head
                if (is_red_by_idx || is_red_by_dist)
                {
                        color = colors::light_red();
                }
                else if (is_near_orange || is_far_orange)
                {
                        color = colors::orange();
                }
                else
                {
                        color = colors::light_green();
                }

                // Do not draw the head yet
                const int tail_size_int = (int)line.size() - 1;

                if ((int)line_idx < tail_size_int)
                {
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
        const P& head_pos =
                line.empty()
                ? m_origin
                : line.back();

        if (viewport::is_in_view(head_pos))
        {
                // If we are currently only drawing the head and the line is
                // empty, draw the head as orange if the aiming has a defined
                // minimum effective range (if the line is non-empty, the head
                // color would be set by the line drawing above)
                if (line.empty() && (orange_until_including_king_dist >= 0))
                {
                        color = colors::orange();
                }

                const P view_pos = viewport::to_view_pos(head_pos);

                io::MapDrawObj draw_obj;

                draw_obj.pos = view_pos;
                draw_obj.tile = gfx::TileId::aim_marker_head;
                draw_obj.character = 'X';
                draw_obj.color = color;
                draw_obj.color_bg = colors::black();

                draw_obj.draw();
        }
}

void MarkerState::move(const Dir dir, const int nr_steps)
{
        const P new_pos(m_pos + dir_utils::offset(dir).scaled_up(nr_steps));

        // We limit the distance from the player that the marker can be moved to
        // (mostly just to avoid segfaults or weird integer wraparound behavior)
        // The limit is an arbitrary big number, larger than any map should be
        const int max_dist_from_player = 300;

        if (king_dist(map::g_player->m_pos, new_pos) <= max_dist_from_player)
        {
                m_pos = new_pos;

                on_moved();
        }
}

bool MarkerState::try_go_to_tgt()
{
        const auto* const tgt = map::g_player->m_tgt;

        if (!tgt)
        {
                return false;
        }

        const auto seen_foes = actor::seen_foes(*map::g_player);

        if (!seen_foes.empty())
        {
                for (auto* const actor : seen_foes)
                {
                        if (tgt == actor)
                        {
                                m_pos = actor->m_pos;

                                return true;
                        }
                }
        }

        return false;
}

void MarkerState::try_go_to_closest_enemy()
{
        const auto seen_foes = actor::seen_foes(*map::g_player);

        std::vector<P> seen_foes_positions;

        seen_foes_positions.reserve(seen_foes.size());

        for (const auto* const actor : seen_foes)
        {
                seen_foes_positions.push_back(actor->m_pos);
        }

        // If player sees enemies, suggest one for targeting
        if (!seen_foes_positions.empty())
        {
                m_pos = closest_pos(map::g_player->m_pos, seen_foes_positions);

                map::g_player->m_tgt = map::living_actor_at(m_pos);
        }
}

// -----------------------------------------------------------------------------
// View state
// -----------------------------------------------------------------------------
void Viewing::on_moved()
{
        msg_log::clear();

        view::print_location_info_msgs(m_pos);

        const auto* const actor = map::living_actor_at(m_pos);

        if (actor &&
            !actor::is_player(actor) &&
            actor::can_player_see_actor(*actor))
        {
                // TODO: This should not be specified here
                const auto view_key = 'v';

                // In debug mode, confirm that this is actually the correct key,
                // however see TODO above
#ifndef NDEBUG
                {
                        io::InputData dummy_input;
                        dummy_input.key = view_key;

                        const auto game_cmd =
                                game_commands::to_cmd(dummy_input);

                        ASSERT(game_cmd == GameCmd::look);
                }
#endif  // NDEBUG

                const std::string msg =
                        std::string("[") +
                        view_key +
                        std::string("] for description");

                msg_log::add(
                        msg,
                        colors::light_white(),
                        MsgInterruptPlayer::no,
                        MorePromptOnMsg::no,
                        CopyToMsgHistory::no);
        }

        msg_log::add(
                common_text::g_cancel_hint,
                colors::light_white(),
                MsgInterruptPlayer::no,
                MorePromptOnMsg::no,
                CopyToMsgHistory::no);
}

void Viewing::handle_input(const io::InputData& input)
{
        const auto game_cmd = game_commands::to_cmd(input);

        if (game_cmd == GameCmd::look)
        {
                auto* const actor = map::living_actor_at(m_pos);

                if (actor &&
                    !actor::is_player(actor) &&
                    actor::can_player_see_actor(*actor))
                {
                        msg_log::clear();

                        auto view_actor_descr =
                                std::make_unique<ViewActorDescr>(*actor);

                        states::push(std::move(view_actor_descr));
                }
        }
        else if ((input.key == SDLK_ESCAPE) || (input.key == SDLK_SPACE))
        {
                msg_log::clear();

                states::pop();
        }
}

// -----------------------------------------------------------------------------
// Aim marker state
// -----------------------------------------------------------------------------
void Aiming::on_moved()
{
        view::print_living_actor_info_msg(m_pos);

        const int dist = king_dist(m_origin, m_pos);

        const bool is_in_max_range =
                (dist <= max_king_dist());

        if (is_in_max_range)
        {
                auto* const actor = map::living_actor_at(m_pos);

                if (actor &&
                    !actor::is_player(actor) &&
                    actor::can_player_see_actor(*actor))
                {
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
        }

        // TODO: This should not be specified here
        const auto fire_key = 'f';

        // In debug mode, confirm that this is actually the correct key,
        // however see TODO above
#ifndef NDEBUG
        {
                io::InputData dummy_input;
                dummy_input.key = fire_key;

                const auto game_cmd = game_commands::to_cmd(dummy_input);

                ASSERT(game_cmd == GameCmd::fire);
        }
#endif  // NDEBUG

        const std::string msg =
                std::string("[") +
                fire_key +
                std::string("] to fire ") +
                common_text::g_cancel_hint;

        msg_log::add(
                msg,
                colors::light_white(),
                MsgInterruptPlayer::no,
                MorePromptOnMsg::no,
                CopyToMsgHistory::no);
}

void Aiming::handle_input(const io::InputData& input)
{
        auto game_cmd = GameCmd::undefined;

        if (config::is_bot_playing())
        {
                // Bot is playing, fire at a random position
                game_cmd = GameCmd::fire;

                m_pos.set(
                        rnd::range(0, map::w() - 1),
                        rnd::range(0, map::h() - 1));
        }
        else
        {
                // Human player
                game_cmd = game_commands::to_cmd(input);
        }

        if ((game_cmd == GameCmd::fire) || (input.key == SDLK_RETURN))
        {
                if (m_pos == map::g_player->m_pos)
                {
                        return;
                }

                msg_log::clear();

                const int dist = king_dist(m_origin, m_pos);

                const bool is_in_effective_range =
                        effective_king_dist_range()
                                .is_in_range(dist);

                const bool is_in_max_range =
                        (dist <= max_king_dist());

                if (!is_in_effective_range &&
                    is_in_max_range &&
                    (m_wpn.data().ranged.effective_range.max > 0))
                {
                        const std::string msg =
                                "Aiming outside effective weapon range "
                                "(50% damage) fire anyway? " +
                                common_text::g_yes_or_no_hint;

                        msg_log::add(msg);

                        const auto answer = query::yes_or_no();

                        msg_log::clear();

                        if (answer == BinaryAnswer::no)
                        {
                                return;
                        }
                }

                auto* const actor = map::living_actor_at(m_pos);

                if (actor && actor::can_player_see_actor(*actor))
                {
                        map::g_player->m_tgt = actor;
                }

                const P pos = m_pos;

                auto* const wpn = &m_wpn;

                states::pop();

                // NOTE: This object is now destroyed

                attack::ranged(
                        map::g_player,
                        map::g_player->m_pos,
                        pos,
                        *wpn);
        }
        else if ((input.key == SDLK_ESCAPE) || (input.key == SDLK_SPACE))
        {
                states::pop();
        }
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
// Throw attack marker state
// -----------------------------------------------------------------------------
void Throwing::on_moved()
{
        view::print_living_actor_info_msg(m_pos);

        const bool is_in_range =
                king_dist(m_origin, m_pos) <=
                max_king_dist();

        if (is_in_range)
        {
                auto* const actor = map::living_actor_at(m_pos);

                if (actor &&
                    !actor::is_player(actor) &&
                    actor::can_player_see_actor(*actor))
                {
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
        }

        // TODO: This should not be specified here
        const auto throw_key = 't';

        // In debug mode, confirm that this is actually the correct key,
        // however see TODO above
#ifndef NDEBUG
        {
                io::InputData dummy_input;
                dummy_input.key = throw_key;

                const auto game_cmd = game_commands::to_cmd(dummy_input);

                ASSERT(game_cmd == GameCmd::throw_item);
        }
#endif  // NDEBUG

        const std::string msg =
                std::string("[") +
                throw_key +
                std::string("] to throw ") +
                common_text::g_cancel_hint;

        msg_log::add(
                msg,
                colors::light_white(),
                MsgInterruptPlayer::no,
                MorePromptOnMsg::no,
                CopyToMsgHistory::no);
}

void Throwing::handle_input(const io::InputData& input)
{
        const auto game_cmd = game_commands::to_cmd(input);

        if ((game_cmd == GameCmd::throw_item) ||
            (input.key == SDLK_RETURN))
        {
                if (m_pos == map::g_player->m_pos)
                {
                        return;
                }

                msg_log::clear();

                const int dist = king_dist(m_origin, m_pos);

                const bool is_in_effective_range =
                        effective_king_dist_range()
                                .is_in_range(dist);

                const bool is_in_max_range =
                        (dist <= max_king_dist());

                if (!is_in_effective_range &&
                    is_in_max_range &&
                    (m_inv_item->data().ranged.effective_range.max > 0))
                {
                        const std::string msg =
                                "Aiming outside effective weapon range "
                                "(50% damage) throw anyway? " +
                                common_text::g_yes_or_no_hint;

                        msg_log::add(msg);

                        const auto answer = query::yes_or_no();

                        msg_log::clear();

                        if (answer == BinaryAnswer::no)
                        {
                                return;
                        }
                }

                auto* const actor = map::living_actor_at(m_pos);

                if (actor && actor::can_player_see_actor(*actor))
                {
                        map::g_player->m_tgt = actor;
                }

                auto* item_to_throw = item::copy_item(*m_inv_item);

                item_to_throw->m_nr_items = 1;

                item_to_throw->clear_actor_carrying();

                m_inv_item = map::g_player->m_inv.decr_item(m_inv_item);

                map::g_player->m_last_thrown_item = m_inv_item;

                const auto pos = m_pos;

                states::pop();

                // NOTE: This object is now destroyed

                // Perform the actual throwing
                throwing::throw_item(
                        *map::g_player,
                        pos,
                        *item_to_throw);
        }
        else if ((input.key == SDLK_ESCAPE) || (input.key == SDLK_SPACE))
        {
                states::pop();
        }
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
            (id != item::Id::smoke_grenade))
        {
                return;
        }

        const R expl_area =
                explosion::explosion_area_outside_map_allowed(
                        m_pos,
                        g_expl_std_radi);

        const Color color = colors::red();

        // Draw explosion radius area overlay
        for (int y = expl_area.p0.y; y <= expl_area.p1.y; ++y)
        {
                for (int x = expl_area.p0.x; x <= expl_area.p1.x; ++x)
                {
                        const P p(x, y);

                        if (!viewport::is_in_view(p))
                        {
                                continue;
                        }

                        const P view_pos = viewport::to_view_pos(p);

                        const P px_pos = io::map_to_px_coords(view_pos);

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
        view::print_location_info_msgs(m_pos);

        // TODO: This should not be specified here
        const auto throw_key = 't';

        // In debug mode, confirm that this is actually the correct key,
        // however see TODO above
#ifndef NDEBUG
        {
                io::InputData dummy_input;
                dummy_input.key = throw_key;

                const auto game_cmd = game_commands::to_cmd(dummy_input);

                ASSERT(game_cmd == GameCmd::throw_item);
        }
#endif  // NDEBUG

        const std::string msg =
                std::string("[") +
                throw_key +
                std::string("] to throw ") +
                common_text::g_cancel_hint;

        msg_log::add(
                msg,
                colors::light_white(),
                MsgInterruptPlayer::no,
                MorePromptOnMsg::no,
                CopyToMsgHistory::no);
}

void ThrowingExplosive::handle_input(const io::InputData& input)
{
        const auto game_cmd = game_commands::to_cmd(input);

        if ((game_cmd == GameCmd::throw_item) || (input.key == SDLK_RETURN))
        {
                msg_log::clear();

                const P pos = m_pos;

                states::pop();

                // NOTE: This object is now destroyed

                throwing::player_throw_lit_explosive(pos);
        }
        else if ((input.key == SDLK_ESCAPE) || (input.key == SDLK_SPACE))
        {
                states::pop();
        }
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

        if ((m_max_dist > 0) && (dist > m_max_dist))
        {
                // Target is too far away
                return 0;
        }
        else
        {
                return std::clamp(100 - dist, 25, 95);
        }
}

void CtrlTele::on_start_hook()
{
        msg_log::add(
                "I have the power to control teleportation.",
                colors::white(),
                MsgInterruptPlayer::no,
                MorePromptOnMsg::yes,
                CopyToMsgHistory::yes);
}

void CtrlTele::on_moved()
{
        view::print_location_info_msgs(m_pos);

        if (m_pos != map::g_player->m_pos)
        {
                const int chance_pct = chance_of_success_pct();

                msg_log::add(
                        std::to_string(chance_pct) + "% chance of success.",
                        colors::light_white(),
                        MsgInterruptPlayer::no,
                        MorePromptOnMsg::no,
                        CopyToMsgHistory::no);

                msg_log::add(
                        "[enter] to try teleporting here",
                        colors::light_white(),
                        MsgInterruptPlayer::no,
                        MorePromptOnMsg::no,
                        CopyToMsgHistory::no);
        }
}

void CtrlTele::handle_input(const io::InputData& input)
{
        if ((input.key != SDLK_RETURN) || (m_pos == map::g_player->m_pos))
        {
                return;
        }

        const int chance = chance_of_success_pct();

        const bool roll_ok = rnd::percent(chance);

        const bool is_success =
                roll_ok &&
                m_blocked.rect().is_pos_inside(m_pos) &&
                !m_blocked.at(m_pos);

        // Copy position before object is deleted
        const P tgt_p = m_pos;

        states::pop();

        // NOTE: This object is now destroyed

        if (is_success)
        {
                // Teleport to this exact destination
                teleport(*map::g_player, tgt_p, m_blocked);
        }
        else
        {
                // Failed to teleport (blocked or roll failed)
                msg_log::add(
                        "I failed to go there...",
                        colors::white(),
                        MsgInterruptPlayer::no,
                        MorePromptOnMsg::yes,
                        CopyToMsgHistory::yes);

                // Run a randomized teleport with no teleport control
                teleport(*map::g_player, ShouldCtrlTele::never);
        }
}

// -----------------------------------------------------------------------------
// Control Object marker state
// -----------------------------------------------------------------------------
bool CtrlObjOpen::can_control(
        const terrain::Terrain& terrain,
        const SpellSkill skill) const
{
        switch (terrain.id())
        {
        case terrain::Id::chest:
        {
                return !static_cast<const terrain::Chest&>(terrain).is_open();
        }
        break;

        case terrain::Id::cabinet:
        {
                return !static_cast<const terrain::Cabinet&>(terrain).is_open();
        }
        break;

        case terrain::Id::tomb:
        {
                return !static_cast<const terrain::Tomb&>(terrain).is_open();
        }
        break;

        case terrain::Id::door:
        {
                const auto& door = static_cast<const terrain::Door&>(terrain);
                const bool is_metal = door.type() == terrain::DoorType::metal;
                const bool is_basic_skill = skill == SpellSkill::basic;

                if (door.is_open() || door.is_hidden())
                {
                        return false;
                }
                else if (is_metal)
                {
                        return !is_basic_skill;
                }
                else
                {
                        return !door.is_known_stuck();
                }
        }
        break;

        default:
        {
        }
        break;
        }

        return false;
}

DidAction CtrlObjOpen::run(
        terrain::Terrain& terrain,
        const SpellSkill skill) const
{
        if (terrain.id() == terrain::Id::door)
        {
                auto& door = static_cast<terrain::Door&>(terrain);
                const bool is_metal = door.type() == terrain::DoorType::metal;

                if (door.is_stuck() && !is_metal)
                {
                        ASSERT(!door.is_known_stuck());

                        door.reveal_stuck_status();

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
        if (terrain.id() != terrain::Id::door)
        {
                return false;
        }

        const auto& door = static_cast<const terrain::Door&>(terrain);

        if (!door.is_open())
        {
                return false;
        }

        if (door.is_hidden())
        {
                return false;
        }

        const bool is_metal = door.type() == terrain::DoorType::metal;
        const bool is_basic_skill = skill == SpellSkill::basic;

        if (is_metal && is_basic_skill)
        {
                return false;
        }

        return true;
}

DidAction CtrlObjCloseDoor::run(
        terrain::Terrain& terrain,
        const SpellSkill skill) const
{
        const auto* const actor_here = map::living_actor_at(terrain.pos());

        if (actor_here)
        {
                std::string actor_name;

                if (actor::can_player_see_actor(*actor_here))
                {
                        actor_name =
                                text_format::first_to_upper(
                                        actor_here->name_the());
                }
                else
                {
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

        if (terrain.id() != terrain::Id::door)
        {
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

bool CtrlObjToggleLever::can_control(
        const terrain::Terrain& terrain,
        const SpellSkill skill) const
{
        (void)skill;

        return terrain.id() == terrain::Id::lever;
}

DidAction CtrlObjToggleLever::run(
        terrain::Terrain& terrain,
        const SpellSkill skill) const
{
        (void)skill;

        auto& lever = static_cast<terrain::Lever&>(terrain);

        const auto name_the =
                text_format::first_to_upper(
                        lever.name(Article::the));

        msg_log::add(name_the + " is toggled.");

        lever.toggle();

        return DidAction::yes;
}

std::string CtrlObjToggleLever::menu_label(
        const terrain::Terrain& terrain) const
{
        (void)terrain;

        return "(t) Toggle lever";
}

char CtrlObjToggleLever::menu_key() const
{
        return 't';
}

bool CtrlObjActivatePylon::can_control(
        const terrain::Terrain& terrain,
        const SpellSkill skill) const
{
        (void)skill;

        if (terrain.id() != terrain::Id::pylon)
        {
                return false;
        }

        const auto& pylon = static_cast<const terrain::Pylon&>(terrain);

        return !pylon.is_activated();
}

DidAction CtrlObjActivatePylon::run(
        terrain::Terrain& terrain,
        const SpellSkill skill) const
{
        (void)skill;

        auto& pylon = static_cast<terrain::Pylon&>(terrain);

        pylon.activate();

        return DidAction::yes;
}

std::string CtrlObjActivatePylon::menu_label(
        const terrain::Terrain& terrain) const
{
        (void)terrain;

        return "(p) Activate pylon";
}

char CtrlObjActivatePylon::menu_key() const
{
        return 'p';
}

bool CtrlObjStrike::can_control(
        const terrain::Terrain& terrain,
        const SpellSkill skill) const
{
        (void)skill;

        switch (terrain.id())
        {
        case terrain::Id::door:
        {
                const auto& door = static_cast<const terrain::Door&>(terrain);
                const bool is_metal = door.type() == terrain::DoorType::metal;

                return !door.is_open() && !door.is_hidden() && !is_metal;
        }
        break;

        case terrain::Id::brazier:
        case terrain::Id::statue:
        {
                return true;
        }
        break;

        default:
        {
        }
        break;
        }

        return false;
}

DidAction CtrlObjStrike::run(
        terrain::Terrain& terrain,
        const SpellSkill skill) const
{
        (void)skill;

        switch (terrain.id())
        {
        case terrain::Id::door:
        {
                const int dmg = 15;

                terrain.hit(
                        DmgType::control_object_spell,
                        map::g_player,
                        terrain.pos(),
                        dmg);

                return DidAction::yes;
        }
        break;

        case terrain::Id::brazier:
        case terrain::Id::statue:
        {
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

                if (input_dir == Dir::END)
                {
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
        }
        break;

        default:
        {
        }
        break;
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
        if (skill != SpellSkill::transcendent)
        {
                return false;
        }

        switch (terrain.id())
        {
        case terrain::Id::wall:
        case terrain::Id::rubble_high:
        {
                return true;
        }
        break;

        default:
        {
        }
        break;
        }

        return false;
}

DidAction CtrlObjDestrWall::run(
        terrain::Terrain& terrain,
        const SpellSkill skill) const
{
        (void)skill;

        if (!map::is_pos_inside_outer_walls(terrain.pos()))
        {
                msg_log::add("Nothing happens.");

                return DidAction::yes;
        }

        switch (terrain.id())
        {
        case terrain::Id::door:
        {
                // NOTE: The door is hidden.
                msg_log::add("Nothing happens.");

                return DidAction::yes;
        }
        break;

        case terrain::Id::wall:
        case terrain::Id::rubble_high:
        {
                terrain.hit(DmgType::pure, map::g_player);

                return DidAction::yes;
        };

        default:
        {
        }
        break;
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

        if (is_allowed_at_dist() && !m_possible_actions.empty())
        {
                const auto* const terrain = map::g_terrain.at(m_pos);
                const auto name_the = terrain->name(Article::the);

                const std::string control_str =
                        "[enter] to control " + name_the;

                msg_log::add(
                        control_str,
                        colors::light_white(),
                        MsgInterruptPlayer::no,
                        MorePromptOnMsg::no,
                        CopyToMsgHistory::no);
        }

        msg_log::add(
                common_text::g_cancel_hint,
                colors::light_white(),
                MsgInterruptPlayer::no,
                MorePromptOnMsg::no,
                CopyToMsgHistory::no);
}

void CtrlObj::handle_input(const io::InputData& input)
{
        if ((input.key == SDLK_ESCAPE) || (input.key == SDLK_SPACE))
        {
                msg_log::clear();

                states::pop();
        }

        if (input.key != SDLK_RETURN)
        {
                return;
        }

        if (!map::g_seen.at(m_pos))
        {
                msg_log::add(
                        "I have no vision here.",
                        colors::white(),
                        MsgInterruptPlayer::no,
                        MorePromptOnMsg::no,
                        CopyToMsgHistory::no);

                return;
        }

        if (!is_allowed_at_dist())
        {
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

        if (m_possible_actions.empty())
        {
                msg_log::add(
                        "I cannot control any object here.",
                        colors::white(),
                        MsgInterruptPlayer::no,
                        MorePromptOnMsg::no,
                        CopyToMsgHistory::no);

                return;
        }

        const auto action = query_control();

        if (!action)
        {
                return;
        }

        m_allow_draw = false;
        const auto did_action = action->run(*m_terrain, m_skill);
        m_allow_draw = true;

        if (did_action == DidAction::yes)
        {
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
        all_actions.emplace_back(
                std::make_shared<CtrlObjOpen>());

        all_actions.emplace_back(
                std::make_shared<CtrlObjCloseDoor>());

        all_actions.emplace_back(
                std::make_shared<CtrlObjJamDoor>());

        all_actions.emplace_back(
                std::make_shared<CtrlObjToggleLever>());

        all_actions.emplace_back(
                std::make_shared<CtrlObjActivatePylon>());

        all_actions.emplace_back(
                std::make_shared<CtrlObjStrike>());

        all_actions.emplace_back(
                std::make_shared<CtrlObjDestrWall>());
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

        std::vector<char> menu_keys;
        std::vector<std::string> menu_labels;

        menu_keys.reserve(m_possible_actions.size());
        menu_labels.reserve(m_possible_actions.size());

        for (const auto& action : m_possible_actions)
        {
                menu_keys.push_back(action->menu_key());
                menu_labels.push_back(action->menu_label(*m_terrain));
        }

        menu_keys.push_back(0);
        menu_labels.emplace_back("(space, esc) Choose another position");

        int choice = 0;

        popup.setup_menu_mode(menu_labels, menu_keys, &choice);

        popup.set_title("Control object");

        popup.run();

        if ((choice == -1) || (choice == (int)m_possible_actions.size()))
        {
                return {};
        }
        else
        {
                return m_possible_actions[choice];
        }
}

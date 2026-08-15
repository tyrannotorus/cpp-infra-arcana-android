// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "game.hpp"

#include <algorithm>
#include <memory>
#include <ostream>

#include "SDL_keycode.h"
#include "ability_values.hpp"
#include "actor.hpp"
#include "actor_act.hpp"
#include "actor_cycle_graphics.hpp"
#include "actor_data.hpp"
#include "actor_items.hpp"
#include "actor_player_state.hpp"
#include "actor_see.hpp"
#include "array2.hpp"
#include "audio.hpp"
#include "audio_data.hpp"
#include "bash.hpp"
#include "colors.hpp"
#include "common_text.hpp"
#include "config.hpp"
#include "context_pins.hpp"
#include "create_character.hpp"
#include "debug.hpp"
#include "draw_health_bars.hpp"
#include "draw_map.hpp"
#include "fade.hpp"
#include "game_commands.hpp"
#include "game_over.hpp"
#include "game_time.hpp"
#include "init.hpp"
#include "insanity.hpp"
#include "inventory.hpp"
#include "io.hpp"
#include "io_display.hpp"
#include "item_data.hpp"
#include "map.hpp"
#include "map_builder.hpp"
#include "map_controller.hpp"
#include "map_mode_gui.hpp"
#include "map_travel.hpp"
#include "minimap.hpp"
#include "msg_log.hpp"
#include "panel.hpp"
#include "player_bon.hpp"
#include "popup.hpp"
#include "property.hpp"
#include "property_data.hpp"
#include "property_factory.hpp"
#include "property_handler.hpp"
#include "rect.hpp"
#include "saving.hpp"
#include "terrain.hpp"
#include "terrain_door.hpp"
#include "text_format.hpp"
#include "view.hpp"
#include "viewport.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static int s_clvl = 0;
static int s_xp_pct = 0;
static int s_xp_accum = 0;
static TimeData s_start_time;
static IsWin s_is_win = IsWin::no;

static int s_death_overlay_tint = 0;
static int s_death_overlay_last_update_cycle = 0;

static std::vector<HistoryEvent> s_history_events;

static const std::vector<std::string> s_win_msg_default = {
        {"As I approach the crystal, an eerie glow illuminates the area. "
         "I notice a figure observing me from the edge of the light. There is "
         "no doubt in my mind concerning the nature of this entity."},

        {"I panic. Why is it I find myself here, stumbling around in "
         "darkness? Is this all part of a plan? The being beckons me to "
         "gaze into the stone."},

        {"In the radiance I see visions beyond eternity, visions of "
         "unreal reality, visions of the brightest light of day and the "
         "darkest night of madness. There is only onward now, I have to see, "
         "I have to KNOW."},

        {"So I make a pact with the Fiend."},

        {"I now harness the shadows that stride from world to world to "
         "sow death and madness. The destinies of all things on earth, "
         "living and dead, are mine."}};

static double mon_shock_lvl_to_shock_value(const MonShockLvl shock_lvl)
{
        switch (shock_lvl) {
        case MonShockLvl::unsettling:
                return 2.0;

        case MonShockLvl::frightening:
                return 4.0;

        case MonShockLvl::terrifying:
                return 10.0;

        case MonShockLvl::mind_shattering:
                return 33.0;

        case MonShockLvl::none:
        case MonShockLvl::END:
                return 0.0;
        }

        return 0.0;
}

// -----------------------------------------------------------------------------
// game
// -----------------------------------------------------------------------------
namespace game
{
void init()
{
        s_clvl = 0;
        s_xp_pct = 0;
        s_xp_accum = 0;
        s_is_win = IsWin::no;

        s_death_overlay_tint = 100;
        s_death_overlay_last_update_cycle = 0;

        s_history_events.clear();
}

void save()
{
        saving::put_int(s_clvl);
        saving::put_int(s_xp_pct);
        saving::put_int(s_xp_accum);
        saving::put_int(s_start_time.year);
        saving::put_int(s_start_time.month);
        saving::put_int(s_start_time.day);
        saving::put_int(s_start_time.hour);
        saving::put_int(s_start_time.minute);
        saving::put_int(s_start_time.second);

        saving::put_int((int)s_history_events.size());

        for (const HistoryEvent& event : s_history_events) {
                saving::put_str(event.msg);
                saving::put_int(event.turn);
        }
}

void load()
{
        s_clvl = saving::get_int();
        s_xp_pct = saving::get_int();
        s_xp_accum = saving::get_int();
        s_start_time.year = saving::get_int();
        s_start_time.month = saving::get_int();
        s_start_time.day = saving::get_int();
        s_start_time.hour = saving::get_int();
        s_start_time.minute = saving::get_int();
        s_start_time.second = saving::get_int();

        const int nr_events = saving::get_int();

        for (int i = 0; i < nr_events; ++i) {
                const std::string msg = saving::get_str();
                const int turn = saving::get_int();

                s_history_events.emplace_back(msg, turn);
        }
}

int clvl()
{
        return s_clvl;
}

int xp_pct()
{
        return s_xp_pct;
}

int xp_accumulated()
{
        return s_xp_accum;
}

IsWin is_win()
{
        return s_is_win;
}

void set_is_win()
{
        s_is_win = IsWin::yes;
}

TimeData start_time()
{
        return s_start_time;
}

void incr_player_xp(const int xp_gained, const Verbose verbose)
{
        if (!actor::is_alive(*map::g_player)) {
                return;
        }

        if (verbose == Verbose::yes) {
                msg_log::add("(+" + std::to_string(xp_gained) + "% XP)");
        }

        s_xp_pct += xp_gained;

        s_xp_accum += xp_gained;

        while (s_xp_pct >= 100) {
                if (s_clvl < g_player_max_clvl) {
                        ++s_clvl;

                        msg_log::add(
                                std::string(
                                        "Welcome to level " +
                                        std::to_string(s_clvl) +
                                        "!"),
                                colors::green(),
                                MsgInterruptPlayer::no,
                                MorePromptOnMsg::yes);

                        msg_log::more_prompt();

                        {
                                const int hp_gained = 1;

                                actor::change_max_hp(
                                        *map::g_player,
                                        hp_gained,
                                        Verbose::no);

                                actor::restore_hp(
                                        *map::g_player,
                                        hp_gained,
                                        actor::AllowRestoreAboveMax::no,
                                        Verbose::no);
                        }

                        {
                                const int sp_gained = 1;

                                actor::change_max_sp(
                                        *map::g_player,
                                        sp_gained,
                                        Verbose::no);

                                actor::restore_sp(
                                        *map::g_player,
                                        sp_gained,
                                        actor::AllowRestoreAboveMax::no,
                                        Verbose::no);
                        }

                        player_bon::on_player_gained_lvl(s_clvl);

                        states::push(
                                std::make_unique<PickTraitState>(
                                        "Which trait do you gain?",
                                        IsCharacterCreationTraitPick::no));
                }

                s_xp_pct -= 100;
        }
}

void decr_player_xp(int xp_lost)
{
        // XP should never be reduced below 0% (if this should happen, it is
        // considered to be a bug)
        ASSERT(xp_lost <= s_xp_pct);

        // If XP lost is greater than the current XP, be nice in release mode
        xp_lost = std::min(xp_lost, s_xp_pct);

        s_xp_pct -= xp_lost;
}

void incr_clvl_number()
{
        ++s_clvl;
}

void player_discover_monster(actor::Actor& actor)
{
        if (init::g_is_cheat_vision_enabled) {
                return;
        }

        if (actor.m_mimic_data) {
                return;
        }

        actor::ActorData& d = *actor.m_data;

        if (d.has_player_seen) {
                return;
        }

        d.has_player_seen = true;

        const int xp_gained = mon_shock_lvl_to_xp(d.mon_shock_lvl);

        const double shock_value = mon_shock_lvl_to_shock_value(d.mon_shock_lvl);

        if (xp_gained <= 0) {
                return;
        }

        const std::string name = actor::name_a(actor);

        msg_log::add("I have discovered " + name + "!");

        incr_player_xp(xp_gained);

        msg_log::more_prompt();

        add_history_event("Discovered " + name);

        map::g_player->incr_shock(shock_value, ShockSrc::see_mon);

        actor::player_state::g_allow_print_mon_warning = false;
}

int mon_shock_lvl_to_xp(const MonShockLvl shock_lvl)
{
        switch (shock_lvl) {
        case MonShockLvl::unsettling:
                return 3;

        case MonShockLvl::frightening:
                return 5;

        case MonShockLvl::terrifying:
                return 8;

        case MonShockLvl::mind_shattering:
                return 15;

        case MonShockLvl::none:
        case MonShockLvl::END:
                return 0;
        }

        return 0;
}

void on_mon_killed(actor::Actor& actor)
{
        auto& d = *actor.m_data;

        d.nr_kills += 1;

        const int min_hp_for_sadism_bon = 4;

        if (d.hp >= min_hp_for_sadism_bon &&
            insanity::has_sympt(InsSymptId::sadism)) {
                actor::player_state::g_shock =
                        std::max(0.0, actor::player_state::g_shock - 3.0);
        }

        if (d.is_unique) {
                const std::string name = actor::name_the(actor);

                add_history_event("Defeated " + name);
        }
}

void add_history_event(const std::string& msg)
{
        if (saving::is_loading()) {
                // If we are loading the game, never add historic messages (this
                // allows silently running stuff like equip hooks for items)
                return;
        }

        const int turn_nr = game_time::turn_nr();

        s_history_events.emplace_back(msg, turn_nr);
}

const std::vector<HistoryEvent>& history()
{
        return s_history_events;
}

}  // namespace game

// -----------------------------------------------------------------------------
// Game state
// -----------------------------------------------------------------------------
StateId GameState::id() const
{
        return StateId::game;
}

void GameState::on_start()
{
        if (m_entry_mode == GameEntryMode::new_game) {
                // Character creation may have affected maximum hp and spi
                // (either positively or negatively), so here we need to (re)set
                // the current hp and spi to the maximum values
                map::g_player->m_hp = actor::max_hp(*map::g_player);
                map::g_player->m_sp = actor::max_sp(*map::g_player);

                map::g_player->m_data->ability_values.reset();

                actor_items::make_for_actor(*map::g_player);

                game::add_history_event("Started journey");

                // NOTE: The "The story so far..." intro popup is shown by
                // IntroStoryState (create_character.cpp) - a character
                // creation step of its own, so that its [ x ] control can
                // step BACK to the name entry.
        }

        if (config::is_intro_lvl_skipped() ||
            (m_entry_mode == GameEntryMode::load_game)) {
                map_travel::go_to_nxt();
        }
        else {
                const auto map_builder =
                        map_builder::make(MapType::intro_forest);

                map_builder->build();

                minimap::clear();

                viewport::cut_to(map::g_player->m_pos);

                map::update_vision();

                if (map_control::g_controller) {
                        map_control::g_controller->on_enter();
                }
        }

        if (config::is_gj_mode() &&
            (m_entry_mode == GameEntryMode::new_game)) {
                // Start with some disadvantages
                auto* const cursed =
                        prop::make(prop::Id::cursed);

                cursed->set_indefinite();

                auto* const diseased =
                        prop::make(prop::Id::diseased);

                diseased->set_indefinite();

                map::g_player->m_properties.apply(cursed);
                map::g_player->m_properties.apply(diseased);

                actor::change_max_hp(*map::g_player, -4, Verbose::yes);
        }

        s_start_time = current_time();
}

void GameState::cycle_graphics(const io::GraphicsCycle cycle)
{
        for (auto* const t : map::g_terrain) {
                t->cycle_graphics(cycle);
        }

        for (auto* const a : game_time::g_actors) {
                actor::cycle_graphics(*a, cycle);
        }
}

// The movement key that steps from the player onto an ADJACENT position -
// used by the contextual [ open ] button, since a closed door is opened by
// walking into it (there is no open command)
static int move_key_towards(const P& adjacent_pos)
{
        const P d = adjacent_pos - map::g_player->m_pos;

        if (d.x > 0) {
                return (d.y < 0)
                        ? SDLK_KP_9
                        : ((d.y > 0) ? SDLK_KP_3 : SDLK_KP_6);
        }

        if (d.x < 0) {
                return (d.y < 0)
                        ? SDLK_KP_7
                        : ((d.y > 0) ? SDLK_KP_1 : SDLK_KP_4);
        }

        return (d.y < 0) ? SDLK_KP_8 : SDLK_KP_2;
}

// While the map is manually panned, the cell at the view's centering point
// (the look "pin") is highlighted - dragging the map IS the look
// interaction (see GameState::on_map_panned)
static void draw_look_pin()
{
        if (!viewport::is_pan_active()) {
                return;
        }

        const P pin_pos = viewport::center_map_pos();

        if (!viewport::is_in_view(pin_pos) ||
            !map::is_pos_inside_map(pin_pos)) {
                return;
        }

        // Corner brackets overlaid on the cell - the cell's content stays
        // visible under the reticle
        draw_map::draw_reticle(viewport::to_view_pos(pin_pos));
}

bool GameState::has_map_display_draw() const
{
        // Tile graphics cycling and flash animations only change what is on
        // the map (see GameState::cycle_graphics, which walks terrain and
        // actors). The stats panel, the log and the action bar change on
        // game actions, and those always go through a full draw.
        return (map::w() > 0);
}

void GameState::draw_map_display()
{
        if (map::w() == 0) {
                return;
        }

        if (states::is_current_state(this)) {
                const bool was_panned = viewport::is_pan_active();

                if (viewport::should_auto_center()) {
                        // If a manual pan was just dropped (the player
                        // moved, e.g. by a movement swipe while looking
                        // around), snap the camera back to fully centered
                        // on the player - even if the player is still
                        // within the panned view
                        viewport::show(
                                map::g_player->m_pos,
                                was_panned
                                        ? viewport::ForceCentering::yes
                                        : viewport::ForceCentering::no);
                }
        }

        draw_map::run();

        // NOTE: Only when the game is the current state - the marker states
        // draw their own (identical looking) marker at the pin position
        if (states::is_current_state(this)) {
                draw_look_pin();
        }

        // NOTE: Everything that goes on the MAP display is drawn together,
        // before the overlays on their own displays. Switching the render
        // target is a pipeline flush and, on the tile based GPUs mobile
        // devices use, a resolve of the whole target - so the fewer times
        // per frame the drawing hops between displays, the better. The
        // overlays composite on top of the map regardless of the order they
        // were drawn in; only the composite order decides what covers what.

        // NOTE: This must be drawn BEFORE life bars and other such overlay
        // graphics - otherwise the life bars will flash as well.
        io::draw_flash_animations();

        if (config::display_health_bars()) {
                draw_health_bars();
        }

        // If the player is dead, fade to red.
        if (!actor::is_alive(*map::g_player)) {
                const int current_cycle = io::graphics_cycle_nr(io::GraphicsCycle::fast);

                if (current_cycle > s_death_overlay_last_update_cycle) {
                        s_death_overlay_last_update_cycle = current_cycle;

                        const int min_tint = 30;

                        s_death_overlay_tint = std::max(min_tint, s_death_overlay_tint - 10);
                }

                io::set_display_for_panel(Panel::map);

                io::draw_rectangle_filled_mod_blending(
                        io::gui_to_px_rect(panels::area(Panel::map)),
                        colors::red().tinted(s_death_overlay_tint));
        }
}

void GameState::draw()
{
        if (map::w() == 0) {
                return;
        }

        draw_map_display();

        map_mode_gui::draw();

        msg_log::draw();
}

void GameState::on_map_panned()
{
        // Dragging the map IS the look interaction: describe the cell at
        // the view's centering point (the look "pin"). Looking is free -
        // no game time passes.
        if (msg_log::is_waiting_prompt()) {
                // The log is waiting for input (a "more" prompt or a yes/no
                // question) - do not wipe it
                return;
        }

        const P pin_pos = viewport::center_map_pos();

        if (!map::is_pos_inside_map(pin_pos)) {
                return;
        }

        msg_log::clear();

        view::print_location_info_msgs(pin_pos);

        // A visible monster at the pin can be described in detail - offer
        // it via a context pin (sends the view key)
        const actor::Actor* const actor = map::living_actor_at(pin_pos);

        const bool is_visible_mon_at_pin =
                actor &&
                !actor::is_player(actor) &&
                actor::can_player_see_actor(*actor);

        if (is_visible_mon_at_pin) {
                context_pins::add("describe", game_commands::view_key());
        }

        // Context-sensitive actions for the pinned cell. Each button sends
        // the key of a command whose handler acts on the pin when it rests
        // on an adjacent cell (see close::player_try_close_or_jam and
        // bash::run) - except [ open ], which sends the movement key
        // towards the door, since walking into a door IS how it is opened.
        // There is NO "close door" action bar button - this is THE touch
        // path for closing and jamming doors (the on-screen keyboard 'c'
        // with its direction query remains as a fallback), nor a "disarm"
        // one; kicking does have a bar button, and keeps it. NOTE: Hidden
        // (secret) doors and undiscovered traps must not be revealed by
        // the buttons.
        const bool is_pin_adjacent =
                pin_pos.is_adjacent(map::g_player->m_pos) &&
                (pin_pos != map::g_player->m_pos);

        if (is_pin_adjacent && map::g_seen.at(pin_pos)) {
                const auto* const terrain = map::g_terrain.at(pin_pos);

                if ((terrain->id() == terrain::Id::door) &&
                    !terrain->is_hidden()) {
                        const auto* const door =
                                static_cast<const terrain::Door*>(terrain);

                        if (door->is_open()) {
                                context_pins::add(
                                        "close",
                                        game_commands::close_key());
                        }
                        else {
                                context_pins::add(
                                        "open",
                                        move_key_towards(pin_pos));

                                if ((door->type() !=
                                     terrain::DoorType::metal) &&
                                    map::g_player->m_inv
                                            .has_item_in_backpack(
                                                    item::Id::iron_spike)) {
                                        // A closed door can be jammed shut
                                        // with an iron spike (the close
                                        // command handles it)
                                        context_pins::add(
                                                "jam",
                                                game_commands::close_key());
                                }
                        }
                }

                // A fountain is drunk from by walking into it (there is no
                // drink command), so this sends the movement key towards
                // it, the same way [ open ] does for a door. NOTE: A
                // dried-up one gets no pin - the pin would promise a drink
                // that bumping only answers with "the fountain is
                // dried-up". That reveals nothing: whether it has run dry
                // is part of the fountain's name and color, and the name
                // is printed for the pinned cell right above.
                if (terrain->id() == terrain::Id::fountain) {
                        const auto* const fountain =
                                static_cast<const terrain::Fountain*>(
                                        terrain);

                        if (fountain->has_drinks_left()) {
                                context_pins::add(
                                        "drink",
                                        move_key_towards(pin_pos));
                        }
                }

                // A visible monster standing there, or anything that a
                // kick could do something to - doors, crates, tombs, etc.
                // Kicking a MONSTER at the pin is the whole point of the
                // button: it is how a specific target is kicked without
                // answering the direction query (bash_something_at_pos
                // kicks a monster at the position before considering the
                // terrain). NOTE: The hidden check is what keeps a secret
                // door (kickable, unlike the wall it looks like) from
                // giving itself away, and the visibility check does the
                // same for a monster the player cannot see.
                const bool is_kickable_terrain_at_pin =
                        !terrain->is_hidden() &&
                        bash::is_kickable_terrain(*terrain);

                // Bashing a corpse apart is the same command, and this is
                // the touch path for it (see the "destroying corpses"
                // hint) - a corpse is neither a living monster nor
                // terrain, so it needs its own check
                const bool is_corpse_at_pin =
                        bash::get_corpse_to_bash_at(pin_pos) != nullptr;

                if (is_visible_mon_at_pin ||
                    is_kickable_terrain_at_pin ||
                    is_corpse_at_pin) {
                        context_pins::add(
                                "kick",
                                game_commands::kick_at_look_pin_key());
                }

                // A revealed trap can be disarmed. NOTE: The hidden check
                // is what keeps an undiscovered trap from giving itself
                // away.
                if ((terrain->id() == terrain::Id::trap) &&
                    !terrain->is_hidden()) {
                        context_pins::add(
                                "disarm",
                                game_commands::disarm_key());
                }
        }

        // NOTE: Throwing is NOT offered here - it is an action bar button
        // (removed from the pin 2026-08-05). Engaging it while looking
        // still throws from the pinned cell though: the item selection
        // screen leaves the pin alone, so cancelling out of it returns to
        // looking at the same cell, and picking an item puts the throw
        // marker on that cell (see MarkerState::on_start).
}

void GameState::on_resume()
{
        // Returning from a screen that was opened off the look pin (the
        // throw item selection) - the pin is still where it was, so put its
        // actions back on the log row
        if (viewport::is_pan_active()) {
                on_map_panned();
        }
}

void GameState::update()
{
        // To avoid redrawing the map for each actor, we run acting inside a loop here. We exit the
        // loop if the next actor is the player - then another state cycle will be executed, and
        // rendering performed.
        while (true) {
                // Let the current actor act
                actor::Actor* const actor = game_time::current_actor();

                const bool allow_act = actor->m_properties.allow_act();

                const bool is_gibbed = actor->m_state == ActorState::destroyed;

#ifndef NDEBUG
                // Allow the "tick" function in game_time to be called, to advance time. Otherwise
                // calling the tick function is an error. This helps catching cases where time is
                // ticked multiple times during the same actor's action.
                game_time::g_allow_tick = true;
#endif  // NDEBUG

                if (allow_act && !is_gibbed) {
                        // Tell actor to "do something". If this is the player, input is read from
                        // either the player or the bot. If it's a monster, the AI handles it.
                        actor::act(*actor);
                }
                else {
                        // Actor cannot act

                        if (actor::is_player(actor)) {
                                io::sleep(g_ms_delay_player_unable_act);
                        }

                        game_time::tick();
                }

                // NOTE: This state may have been popped at this point.

                // We have quit the current game, or the player is dead?
                if (!map::g_player ||
                    !states::contains_state(StateId::game) ||
                    !actor::is_alive(*map::g_player)) {
                        break;
                }

                // Stop if the next actor is the player (to trigger rendering).
                const actor::Actor* next_actor = game_time::current_actor();

                if (actor::is_player(next_actor)) {
                        break;
                }
        }

        // Player is dead?
        if (map::g_player && !actor::is_alive(*map::g_player)) {
                TRACE << "Player died" << std::endl;

                audio::play(audio::SfxId::death);

                msg_log::add(
                        "-I AM DEAD!-",
                        colors::msg_bad(),
                        MsgInterruptPlayer::no,
                        MorePromptOnMsg::yes);

                saving::erase_save();

                states::pop();

                on_game_over();

                return;
        }
}

// -----------------------------------------------------------------------------
// Win game state
// -----------------------------------------------------------------------------
StateId WinGameState::id() const
{
        return StateId::win_game;
}

bool WinGameState::has_close_button() const
{
        // The first section has none - see the class comment
        return m_section_idx > 0;
}

std::string WinGameState::page_title() const
{
        // None - the ending is read as prose, and a heading over it would
        // be the game talking over the end of the story
        return "";
}

std::string WinGameState::page_text() const
{
        // NOTE: One section per page. The switch has no cases of its own
        // yet - the ending is the same whatever the player was - but it is
        // where a background's own ending would go.
        const std::vector<std::string>* win_msg = nullptr;

        switch (player_bon::bg()) {
        default:
                win_msg = &s_win_msg_default;
                break;
        }

        if (m_section_idx >= win_msg->size()) {
                ASSERT(false);

                return "";
        }

        return (*win_msg)[m_section_idx];
}

void WinGameState::on_confirmed()
{
        if ((m_section_idx + 1) < s_win_msg_default.size()) {
                ++m_section_idx;

                rebuild_text();

                // The next section is read from ITS top, not from however
                // far the previous one was scrolled
                set_scroll_px(0);

                return;
        }

        // The last section has been read - out into the dark, and on to
        // the game summary waiting underneath (see on_game_over)
        fade::to_black();

        states::pop();

        // NOTE: This object is now deleted!
}

void WinGameState::on_cancelled()
{
        if (m_section_idx == 0) {
                // Nothing to step back to, and the ending is not something
                // to back out of - the [ x ] is not even drawn here (see
                // has_close_button)
                return;
        }

        --m_section_idx;

        rebuild_text();

        set_scroll_px(0);
}

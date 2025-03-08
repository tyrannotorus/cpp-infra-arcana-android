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
#include "array2.hpp"
#include "audio.hpp"
#include "audio_data.hpp"
#include "colors.hpp"
#include "common_text.hpp"
#include "config.hpp"
#include "create_character.hpp"
#include "debug.hpp"
#include "draw_health_bars.hpp"
#include "draw_map.hpp"
#include "game_over.hpp"
#include "game_time.hpp"
#include "init.hpp"
#include "insanity.hpp"
#include "io.hpp"
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
#include "text_format.hpp"
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

static const std::string s_intro_msg_default =
        "I stand at the end of a cobbled forest path, before me lies a shunned "
        "and decrepit old church building. This is the access point to the "
        "domains of the abhorred \"Cult of Starry Wisdom\". "
        "I am determined to enter these sprawling catacombs and rob them of "
        "treasures and knowledge. Somewhere below lies my true destiny, "
        "an artifact of non-human origin referred to as "
        "{COLOR_YELLOW}\"The shining Trapezohedron\"{reset_color} "
        "- a window to all the secrets of the universe!";

static const std::string s_intro_msg_exorcist =
        "I stand at the end of a cobbled forest path, before me lies a shunned "
        "and decrepit old church building. This is the access point to the "
        "domains of the abhorred \"Cult of Starry Wisdom\". "
        "I am determined to enter these sprawling catacombs and purge them of "
        "the corruption that dwells within. Somewhere below lies "
        "an artifact of non-human origin referred to as "
        "{COLOR_YELLOW}\"The shining Trapezohedron\"{reset_color} "
        "- rumored to be a window to all the secrets of the universe. "
        "This must be destroyed, so that none more may be tempted by "
        "its deceitful promises!";

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

                if (!config::is_intro_lvl_skipped() &&
                    !config::is_intro_popup_skipped()) {
                        io::clear_screen();

                        std::string intro_msg;

                        switch (player_bon::bg()) {
                        case Bg::exorcist:
                                intro_msg = s_intro_msg_exorcist;
                                break;

                        default:
                                intro_msg = s_intro_msg_default;
                                break;
                        }

                        popup::Popup(popup::AddToMsgHistory::yes)
                                .set_title("The story so far...")
                                .set_msg(intro_msg)
                                .run();
                }
        }  // namespace game

        if (config::is_intro_lvl_skipped() ||
            (m_entry_mode == GameEntryMode::load_game)) {
                map_travel::go_to_nxt();
        }
        else {
                const auto map_builder =
                        map_builder::make(MapType::intro_forest);

                map_builder->build();

                minimap::clear();

                viewport::show(
                        map::g_player->m_pos,
                        viewport::ForceCentering::yes);

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

void GameState::draw()
{
        if (map::w() == 0) {
                return;
        }

        if (states::is_current_state(this)) {
                viewport::show(map::g_player->m_pos);
        }

        draw_map::run();

        map_mode_gui::draw();

        msg_log::draw();

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

                io::draw_rectangle_filled_mod_blending(
                        io::gui_to_px_rect(panels::area(Panel::map)),
                        colors::red().tinted(s_death_overlay_tint));
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
void WinGameState::draw()
{
        const int padding = 9;
        const int x0 = padding;
        const int max_w = panels::w(Panel::screen) - (padding * 2);

        int y = 2;

        std::vector<std::string> win_msg;

        switch (player_bon::bg()) {
        default:
                win_msg = s_win_msg_default;
                break;
        }

        for (const std::string& section_msg : win_msg) {
                const std::vector<std::string> section_lines =
                        text_format::split(section_msg, max_w);

                for (const std::string& line : section_lines) {
                        io::draw_text(
                                line,
                                Panel::screen,
                                {x0, y},
                                colors::white(),
                                io::DrawBg::no,
                                colors::black());

                        ++y;
                }
                ++y;
        }

        ++y;

        const int screen_w = panels::w(Panel::screen);
        const int screen_h = panels::h(Panel::screen);

        io::draw_text_center(
                common_text::g_confirm_hint,
                Panel::screen,
                {(screen_w - 1) / 2, screen_h - 2},
                colors::menu_dark(),
                io::DrawBg::no,
                colors::black(),
                false);  // Do not allow pixel-level adjustment
}

void WinGameState::update()
{
        const io::InputData input = io::read_input();

        switch (input.key) {
        case SDLK_SPACE:
        case SDLK_ESCAPE:
        case SDLK_RETURN:
                states::pop();
                break;

        default:
                break;
        }
}

StateId WinGameState::id() const
{
        return StateId::win_game;
}

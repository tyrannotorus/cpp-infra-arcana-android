// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "create_character.hpp"

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <memory>

#include "SDL_keycode.h"
#include "actor.hpp"
#include "actor_data.hpp"
#include "browser.hpp"
#include "character_descr.hpp"
#include "colors.hpp"
#include "common_text.hpp"
#include "config.hpp"
#include "debug.hpp"
#include "draw_box.hpp"
#include "game.hpp"
#include "game_summary_data.hpp"
#include "global.hpp"
#include "init.hpp"
#include "inventory_handling.hpp"
#include "io.hpp"
#include "io_internal.hpp"
#include "map.hpp"
#include "marker.hpp"
#include "minimap.hpp"
#include "panel.hpp"
#include "player_spells.hpp"
#include "popup.hpp"
#include "pos.hpp"
#include "random.hpp"
#include "rect.hpp"
#include "spells.hpp"
#include "text_format.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------

// Entry indices picked so far during THIS character creation run, in pick
// order (background, occultist domain, trait). Used for stepping BACK:
// picks are applied to the character immediately when made, so going back
// one step means resetting the session and auto-replaying all but the
// last pick (see CreateCharPageState::update).
static std::vector<int> s_creation_picks;
static std::vector<int> s_creation_replay_queue;

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

// Restarts the character creation session, auto-replaying the given picks
// (see CreateCharPageState::update)
static void creation_restart_and_replay(std::vector<int> picks_to_replay)
{
        s_creation_replay_queue = std::move(picks_to_replay);

        s_creation_picks.clear();

        init::cleanup_session();

        states::pop_until(StateId::main_menu);

        init::init_session();

        states::push(std::make_unique<NewGameState>());
}

// Steps back to the previous character creation page ([ x ] / escape on a
// creation page). On the first page (nothing picked yet), aborts to the
// main menu instead.
static void creation_go_back()
{
        if (s_creation_picks.empty()) {
                s_creation_replay_queue.clear();

                init::cleanup_session();

                states::pop_until(StateId::main_menu);

                return;
        }

        auto replay = s_creation_picks;

        replay.pop_back();

        creation_restart_and_replay(std::move(replay));
}

static void handle_show_player_info_command()
{
        int choice = 0;

        popup::Popup(popup::AddToMsgHistory::no)
                .setup_menu_mode(
                        {"(C)haracter information",
                         "(I)nventory",
                         "(K)nown spells",
                         "(L)ook around",
                         "(V)iew map"},
                        &choice)
                .run();

        // NOTE: Any states created here shall run immediately until done, since
        // drawing of the trait pick state is paused and resumed around the call
        // to this function (so that for example the view marker state can be
        // drawn on top of the game map).

        switch (choice) {
        case 0: {
                game_summary_data::GameSummaryData game_data = game_summary_data::collect();

                auto char_descr = std::make_unique<CharacterDescr>();

                char_descr->setup(game_data);

                states::run_until_state_done(std::move(char_descr));
        } break;

        case 1: {
                auto browse_inv = std::make_unique<BrowseInv>();

                browse_inv->disable_allow_inventory_actions();

                states::run_until_state_done(std::move(browse_inv));
        } break;

        case 2: {
                auto browse_spells = std::make_unique<BrowseSpell>();

                browse_spells->disable_allow_cast();

                states::run_until_state_done(std::move(browse_spells));

                // TODO: Consider this:
                msg_log::more_prompt();
        } break;

        case 3: {
                states::run_until_state_done(std::make_unique<Viewing>(map::g_player->m_pos));
        } break;

        case 4: {
                states::run_until_state_done(std::make_unique<ViewMinimap>());
        } break;
        }
}

// -----------------------------------------------------------------------------
// New game state
// -----------------------------------------------------------------------------
void NewGameState::on_pushed()
{
        // NOTE: The replay queue is NOT cleared - stepping back re-enters
        // this state with the queued picks to replay
        s_creation_picks.clear();

        states::push(std::make_unique<GameState>(GameEntryMode::new_game));

        states::push(std::make_unique<IntroStoryState>());

        states::push(std::make_unique<EnterNameState>());

        states::push(
                std::make_unique<PickTraitState>(
                        "Which extra trait do you start with?",
                        IsCharacterCreationTraitPick::yes));

        states::push(std::make_unique<PickBgState>());
}

void NewGameState::on_resume()
{
        states::pop();
}

// -----------------------------------------------------------------------------
// Create character page state
// -----------------------------------------------------------------------------
void CreateCharPageState::update()
{
        // Auto-replay a recorded pick (stepping BACK during character
        // creation replays all but the last pick, see creation_go_back)
        if (!s_creation_replay_queue.empty()) {
                const int idx = s_creation_replay_queue.front();

                s_creation_replay_queue.erase(
                        std::begin(s_creation_replay_queue));

                on_entry_selected(idx);

                return;
        }

        MenuPageState::update();
}

// -----------------------------------------------------------------------------
// Pick background state
// -----------------------------------------------------------------------------
void PickBgState::on_start()
{
        m_bgs = player_bon::pickable_bgs();

        // NOTE: After the backgrounds are set up - the marked entry is
        // restored (or defaulted, see default_marked_idx) from here
        MenuPageState::on_start();
}

int PickBgState::default_marked_idx() const
{
        // War veteran is recommended as a default choice for new players.
        // NOTE: This is the FIRST time the page is opened only - stepping
        // back into it returns to the entry it was left on.
        const auto war_vet_pos =
                std::find(std::begin(m_bgs), std::end(m_bgs), Bg::war_vet);

        return (int)std::distance(std::begin(m_bgs), war_vet_pos);
}

void PickBgState::update()
{
        if (config::is_stress_test()) {
                // Stress-test mode, we just want to run everything
                // automatically without requiring manual input.
                on_entry_selected(m_browser.y());

                return;
        }

        CreateCharPageState::update();
}

std::string PickBgState::page_title() const
{
        return "What is your background?";
}

std::vector<MenuPageEntry> PickBgState::page_entries() const
{
        std::vector<MenuPageEntry> entries;

        entries.reserve(m_bgs.size());

        for (const auto bg : m_bgs) {
                entries.push_back({player_bon::bg_title(bg), ""});
        }

        return entries;
}

void PickBgState::on_entry_selected(const int idx)
{
        const Bg bg = m_bgs[idx];

        s_creation_picks.push_back(idx);

        player_bon::pick_bg(bg);

        // Increase clvl to 1
        game::incr_clvl_number();

        states::pop();

        // Occultists also pick a domain
        if (bg == Bg::occultist) {
                states::push(std::make_unique<PickOccultistState>());
        }
}

void PickBgState::on_cancelled()
{
        creation_go_back();
}

void PickBgState::draw_page_content()
{
        // Description of the marked background
        const auto bg_marked = m_bgs[m_browser.y()];

        const auto descr = player_bon::bg_descr(bg_marked);

        ASSERT(!descr.empty());

        std::vector<std::vector<ColoredString>> lines;

        for (const auto& descr_entry : descr) {
                if (descr_entry.str.empty()) {
                        lines.emplace_back();

                        continue;
                }

                append_descr_text(
                        lines,
                        descr_entry.str,
                        descr_entry.color,
                        descr_text_w());
        }

        draw_descr_lines(lines);
}

// -----------------------------------------------------------------------------
// Pick occultist state
// -----------------------------------------------------------------------------
void PickOccultistState::on_start()
{
        m_domains = player_bon::pickable_occultist_domains();

        MenuPageState::on_start();
}

std::string PickOccultistState::page_title() const
{
        return "What is your spell domain?";
}

std::vector<MenuPageEntry> PickOccultistState::page_entries() const
{
        std::vector<MenuPageEntry> entries;

        entries.reserve(m_domains.size());

        for (const OccultistDomain domain : m_domains) {
                const SpellDomain spell_domain =
                        player_bon::occultist_to_spell_domain(domain);

                entries.push_back(
                        {spells::spell_domain_title(spell_domain), ""});
        }

        return entries;
}

void PickOccultistState::on_entry_selected(const int idx)
{
        const OccultistDomain domain = m_domains[idx];

        s_creation_picks.push_back(idx);

        player_bon::pick_occultist_domain(domain);

        states::pop();
}

void PickOccultistState::on_cancelled()
{
        creation_go_back();
}

void PickOccultistState::draw_page_content()
{
        // Description of the marked domain
        const OccultistDomain domain_marked = m_domains[m_browser.y()];

        const std::string descr =
                player_bon::occultist_domain_descr(domain_marked);

        ASSERT(!descr.empty());

        if (descr.empty()) {
                return;
        }

        std::vector<std::vector<ColoredString>> lines;

        append_descr_text(
                lines,
                descr,
                colors::text(),
                descr_text_w());

        draw_descr_lines(lines);
}

// -----------------------------------------------------------------------------
// Pick trait state
// -----------------------------------------------------------------------------
void PickTraitState::on_start()
{
        const player_bon::UnpickedTraitsData unpicked_traits_data =
                player_bon::unpicked_traits(
                        player_bon::bg(),
                        player_bon::occultist_domain());

        m_traits_avail = unpicked_traits_data.traits_can_be_picked;
        m_traits_unavail = unpicked_traits_data.traits_prereqs_not_met;

        MenuPageState::on_start();
}

void PickTraitState::update()
{
        if (config::is_bot_playing()) {
                states::pop();

                return;
        }

        CreateCharPageState::update();
}

Trait PickTraitState::trait_at(const int idx) const
{
        return is_idx_unavail(idx)
                ? m_traits_unavail[idx - (int)m_traits_avail.size()]
                : m_traits_avail[idx];
}

bool PickTraitState::is_idx_unavail(const int idx) const
{
        return idx >= (int)m_traits_avail.size();
}

bool PickTraitState::handle_custom_input(const io::InputData& input)
{
        if ((input.key == 'i') && (m_is_char_creation == IsCharacterCreationTraitPick::no)) {
                disable_drawing();

                handle_show_player_info_command();

                enable_drawing();

                return true;
        }

        return false;
}

std::string PickTraitState::page_title() const
{
        return m_title;
}

std::string PickTraitState::page_hint() const
{
        if (m_is_char_creation == IsCharacterCreationTraitPick::no) {
                return "[i] to view game info";
        }

        return MenuPageState::page_hint();
}

std::vector<MenuPageEntry> PickTraitState::page_entries() const
{
        // The pickable traits, then the currently unavailable ones (shown
        // in red - browsable to read their descriptions/prerequisites, but
        // not selectable)
        std::vector<MenuPageEntry> entries;

        entries.reserve(m_traits_avail.size() + m_traits_unavail.size());

        for (const Trait trait : m_traits_avail) {
                entries.push_back({player_bon::trait_title(trait), ""});
        }

        for (const Trait trait : m_traits_unavail) {
                entries.push_back({player_bon::trait_title(trait), ""});
        }

        return entries;
}

Color PickTraitState::entry_color(const int idx, const bool is_marked) const
{
        if (is_idx_unavail(idx)) {
                return is_marked ? colors::light_red() : colors::red();
        }

        return MenuPageState::entry_color(idx, is_marked);
}

void PickTraitState::on_entry_selected(const int idx)
{
        if (is_idx_unavail(idx)) {
                // Unavailable traits can only be browsed, not picked
                return;
        }

        const Trait trait = m_traits_avail[idx];

        const std::string name = player_bon::trait_title(trait);

        bool should_pick_trait = true;

        const bool is_character_creation =
                states::contains_state(StateId::pick_name);

        if (!is_character_creation) {
                states::draw();

                const std::string title = "Gain trait \"" + name + "\"?";

                int choice = 0;

                popup::Popup(popup::AddToMsgHistory::no)
                        .set_title(title)
                        .setup_menu_mode(
                                {"(Y)es", "(N)o"},
                                &choice)
                        .run();

                should_pick_trait = (choice == 0);
        }

        if (should_pick_trait) {
                if (is_character_creation) {
                        // Record the pick, so that stepping back from a
                        // LATER creation step can replay it
                        s_creation_picks.push_back(idx);
                }

                player_bon::pick_trait(trait);

                if (!is_character_creation) {
                        game::add_history_event(
                                "Gained trait \"" +
                                name +
                                "\"");
                }

                states::pop();
        }
}

void PickTraitState::on_cancelled()
{
        if (states::contains_state(StateId::pick_name)) {
                // Character creation - step back to the previous page
                creation_go_back();
        }
}

void PickTraitState::draw_page_content()
{
        const int idx_marked = m_browser.y();

        const Trait trait_marked = trait_at(idx_marked);

        // Description
        std::string descr = player_bon::trait_descr(trait_marked);

        std::string descr_extra = player_bon::trait_descr_extra_when_picking(trait_marked);

        text_format::append_with_space(descr, descr_extra);

        std::vector<std::vector<ColoredString>> lines;

        append_descr_text(
                lines,
                descr,
                colors::text(),
                descr_text_w());

        if (is_idx_unavail(idx_marked)) {
                lines.emplace_back();

                lines.push_back(
                        {{"Currently unavailable - prerequisite(s) not met:",
                          colors::light_red()}});
        }

        // Prerequisites, one per line (green = met, red = not met)
        const player_bon::TraitPrereqData prereq_data =
                player_bon::trait_prereqs(
                        trait_marked,
                        player_bon::bg(),
                        player_bon::occultist_domain());

        if (!prereq_data.traits.empty() || (prereq_data.bg != Bg::END)) {
                if (!is_idx_unavail(idx_marked)) {
                        lines.emplace_back();

                        lines.push_back(
                                {{"Prerequisite(s):", colors::text()}});
                }

                const auto& clr_prereq_ok = colors::light_green();
                const auto& clr_prereq_not_ok = colors::light_red();

                if (prereq_data.bg != Bg::END) {
                        const auto& color =
                                (player_bon::bg() == prereq_data.bg)
                                ? clr_prereq_ok
                                : clr_prereq_not_ok;

                        lines.push_back(
                                {{player_bon::bg_title(prereq_data.bg),
                                  color}});
                }

                for (const auto prereq_trait : prereq_data.traits) {
                        const auto& color =
                                player_bon::has_trait(prereq_trait)
                                ? clr_prereq_ok
                                : clr_prereq_not_ok;

                        lines.push_back(
                                {{player_bon::trait_title(prereq_trait),
                                  color}});
                }
        }

        draw_descr_lines(lines);
}

// -----------------------------------------------------------------------------
// Remove trait state
// -----------------------------------------------------------------------------
void RemoveTraitState::on_start()
{
        m_traits_can_be_removed = player_bon::traits_can_be_removed();

        MenuPageState::on_start();
}

void RemoveTraitState::update()
{
        if (config::is_bot_playing()) {
                states::pop();

                return;
        }

        CreateCharPageState::update();
}

bool RemoveTraitState::handle_custom_input(const io::InputData& input)
{
        if (input.key == 'i') {
                handle_show_player_info_command();

                return true;
        }

        return false;
}

std::string RemoveTraitState::page_title() const
{
        return "Lose which trait?";
}

std::string RemoveTraitState::page_hint() const
{
        return "[i] to view game info";
}

std::vector<MenuPageEntry> RemoveTraitState::page_entries() const
{
        std::vector<MenuPageEntry> entries;

        entries.reserve(m_traits_can_be_removed.size());

        for (const Trait trait : m_traits_can_be_removed) {
                entries.push_back({player_bon::trait_title(trait), ""});
        }

        return entries;
}

void RemoveTraitState::on_entry_selected(const int idx)
{
        const auto trait = m_traits_can_be_removed[idx];

        const auto name = player_bon::trait_title(trait);

        states::draw();

        const std::string title = "Remove trait \"" + name + "\"?";

        int choice = 0;

        popup::Popup(popup::AddToMsgHistory::no)
                .set_title(title)
                .setup_menu_mode(
                        {"(Y)es", "(N)o"},
                        &choice)
                .run();

        if (choice == 0) {
                player_bon::remove_trait(trait);

                game::add_history_event(
                        "Lost trait \"" +
                        name +
                        "\"");

                states::pop();
        }
}

void RemoveTraitState::on_cancelled()
{
        // Losing a trait is not optional - the choice cannot be cancelled
}

void RemoveTraitState::draw_page_content()
{
        const Trait trait_marked = m_traits_can_be_removed.at(m_browser.y());

        const std::string descr = player_bon::trait_descr(trait_marked);

        std::vector<std::vector<ColoredString>> lines;

        append_descr_text(
                lines,
                descr,
                colors::text(),
                descr_text_w());

        draw_descr_lines(lines);
}

// -----------------------------------------------------------------------------
// Enter name state
// -----------------------------------------------------------------------------
void EnterNameState::on_start()
{
        // The field always starts empty and focused - the player should be
        // able to just start typing, instead of having to backspace away a
        // remembered name on the on-screen keyboard first
        m_current_str.clear();

        // Text entry screen - bring up the on-screen keyboard immediately
        io::show_screen_keyboard();
}

void EnterNameState::on_popped()
{
        io::hide_screen_keyboard();
}

bool EnterNameState::try_tap(const P& logical_px)
{
        (void)logical_px;

        // A stray tap must NOT confirm the name (unconsumed taps
        // synthesize the confirm key, which would drop the player
        // straight into the game) - the name is confirmed via the
        // keyboard's enter key, and [ x ] steps back. A tap just
        // re-opens the on-screen keyboard in case it was dismissed.
        io::show_screen_keyboard();

        return true;
}

void EnterNameState::update()
{
        if (config::is_bot_playing()) {
                auto& d = *map::g_player->m_data;

                d.name_a = d.name_the = "Bot";

                states::pop();

                return;
        }

        const io::InputData input = io::read_input();

        if (input.key == SDLK_ESCAPE) {
                // Step back to the previous character creation page
                creation_go_back();
                return;
        }

        if (input.key == SDLK_RETURN) {
                if (m_current_str.empty()) {
                        m_current_str = "Player";
                }

                auto& d = *map::g_player->m_data;

                m_current_str = text_format::trim_leading_and_trailing_spaces(m_current_str);

                d.name_a = d.name_the = m_current_str;

                TRACE << "Player name: '" << d.name_a << "'" << std::endl;

                states::pop();

                return;
        }

        if (m_current_str.size() < g_player_name_max_len) {
                const bool is_space = input.key == SDLK_SPACE;

                if (is_space) {
                        m_current_str.push_back(' ');

                        return;
                }

                const bool is_valid_non_space_char =
                        (input.key >= 'a' && input.key <= 'z') ||
                        (input.key >= 'A' && input.key <= 'Z') ||
                        (input.key >= '0' && input.key <= '9');

                if (is_valid_non_space_char) {
                        m_current_str.push_back((char)input.key);

                        return;
                }
        }

        if (!m_current_str.empty()) {
                if (input.key == SDLK_BACKSPACE) {
                        m_current_str.erase(m_current_str.end() - 1);
                }
        }
}

void EnterNameState::draw()
{
        draw_box(panels::screen_box_area());

        const int screen_center_x = panels::center_x(Panel::screen);

        io::draw_text_center(
                " What is your name? ",
                Panel::screen,
                {screen_center_x, panels::screen_box_area().p0.y},
                colors::title(),
                io::DrawBg::yes);

        // Centered in the area left free above the on-screen keyboard,
        // which covers the lower part of the screen without the window ever
        // being resized for it (centering on the whole screen would leave
        // the field half behind the keyboard)
        const int free_px_h =
                io::panel_px_h(Panel::screen) -
                io::screen_keyboard_covered_px_h();

        const R box_area = panels::screen_box_area();

        const int y_name =
                std::clamp(
                        (free_px_h / 2) / config::gui_cell_px_h(),
                        box_area.p0.y + 2,
                        box_area.p1.y - 2);

        const bool is_cursor_shown =
                (m_current_str.size() < g_player_name_max_len) &&
                ((io::graphics_cycle_nr(io::GraphicsCycle::fast) % 2) == 0);

        // The cursor cell is always part of the string - blanking it out
        // instead of dropping it keeps the name from shifting sideways as
        // the cursor blinks
        const std::string name_str =
                m_current_str + (is_cursor_shown ? "_" : " ");

        io::draw_text_center(
                name_str,
                Panel::screen,
                {screen_center_x, y_name},
                colors::menu_highlight(),
                io::DrawBg::no);
}

// -----------------------------------------------------------------------------
// Intro story state
// -----------------------------------------------------------------------------
void IntroStoryState::on_start()
{
        if (config::is_intro_lvl_skipped() ||
            config::is_intro_popup_skipped()) {
                states::pop();

                return;
        }

        TextPageState::on_start();

        // The story is also kept in the message history, for reading again
        // later on
        add_text_to_msg_history();
}

std::string IntroStoryState::page_title() const
{
        return "The story so far...";
}

std::string IntroStoryState::page_text() const
{
        return (player_bon::bg() == Bg::exorcist)
                ? s_intro_msg_exorcist
                : s_intro_msg_default;
}

void IntroStoryState::on_cancelled()
{
        // [ x ] / escape - step BACK to the name entry: restart the
        // creation session replaying ALL recorded picks (the name entry
        // comes after the picks, so nothing is dropped).
        // NOTE: This state is destroyed by the call - do not touch any
        // members afterwards!
        creation_restart_and_replay(s_creation_picks);
}

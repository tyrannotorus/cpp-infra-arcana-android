// =============================================================================
// Copyright 2011-2024 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "create_character.hpp"

#include <algorithm>
#include <cstddef>
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
#include "text.hpp"
#include "text_format.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static void handle_show_player_info_command()
{
        int choice = 0;

        popup::Popup(popup::AddToMsgHistory::no)
                .setup_menu_mode(
                        {"(c) Character information",
                         "(i) Inventory",
                         "(x) Known spells",
                         "(v) Look around",
                         "(m) View map"},
                        {'c',
                         'i',
                         'x',
                         'v',
                         'm'},
                        popup::MenuModeShowCancelHint::yes,
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
        states::push(std::make_unique<GameState>(GameEntryMode::new_game));

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
// Pick background state
// -----------------------------------------------------------------------------
void PickBgState::on_start()
{
        m_bgs = player_bon::pickable_bgs();

        m_browser.reset(
                (int)m_bgs.size(),
                panels::h(Panel::create_char_menu));

        // Set the marker on war veteran, to recommend it as a default choice
        // for new players.
        const auto war_vet_pos = std::find(std::begin(m_bgs), std::end(m_bgs), Bg::war_vet);
        const auto idx = (int)std::distance(std::begin(m_bgs), war_vet_pos);

        m_browser.set_y(idx);
}

void PickBgState::update()
{
        MenuAction action = MenuAction::selected;

        if (config::is_stress_test()) {
                // Stress-test mode, we just want to run everything
                // automatically without requiring manual input.
                action = MenuAction::selected;
        }
        else {
                const io::InputData input = io::read_input();

                action = m_browser.read(input, MenuInputMode::scrolling_and_letters);
        }

        switch (action) {
        case MenuAction::selected: {
                const Bg bg = m_bgs[m_browser.y()];

                player_bon::pick_bg(bg);

                // Increase clvl to 1
                game::incr_clvl_number();

                states::pop();

                // Occultists also pick a domain
                if (bg == Bg::occultist) {
                        states::push(std::make_unique<PickOccultistState>());
                }
        } break;

        case MenuAction::esc: {
                init::cleanup_session();
                states::pop_until(StateId::main_menu);
        } break;

        default:
                break;
        }
}

void PickBgState::draw()
{
        draw_box(panels::area(Panel::screen));

        const int screen_center_x = panels::center_x(Panel::screen);

        io::draw_text_center(
                " What is your background? ",
                Panel::screen,
                {screen_center_x, 0},
                colors::title(),
                io::DrawBg::yes,
                colors::black(),
                true);  // Allow pixel-level adjustment

        int y = 0;

        const auto bg_marked = m_bgs[m_browser.y()];

        // Backgrounds
        for (const auto bg : m_bgs) {
                const auto key_str =
                        std::string("(") +
                        m_browser.menu_keys()[y] +
                        std::string(")");

                const auto bg_name = player_bon::bg_title(bg);

                const bool is_marked = (bg == bg_marked);

                auto color =
                        is_marked
                        ? colors::menu_key_highlight()
                        : colors::menu_key_dark();

                io::draw_text(
                        key_str,
                        Panel::create_char_menu,
                        {0, y},
                        color);

                color =
                        is_marked
                        ? colors::menu_highlight()
                        : colors::menu_dark();

                io::draw_text(
                        bg_name,
                        Panel::create_char_menu,
                        {(int)key_str.length() + 1, y},
                        color);

                ++y;
        }

        // Description
        y = 0;

        const auto descr = player_bon::bg_descr(bg_marked);

        ASSERT(!descr.empty());

        for (const auto& descr_entry : descr) {
                if (descr_entry.str.empty()) {
                        ++y;

                        continue;
                }

                Text text;
                text.set_str(descr_entry.str);
                text.set_w(panels::w(Panel::create_char_descr));
                text.set_color(descr_entry.color);

                text.draw(Panel::create_char_descr, {0, y});

                y += text.nr_lines();
        }
}

// -----------------------------------------------------------------------------
// Pick occultist state
// -----------------------------------------------------------------------------
void PickOccultistState::on_start()
{
        m_domains = player_bon::pickable_occultist_domains();

        m_browser.reset(
                (int)m_domains.size(),
                panels::h(Panel::create_char_menu));
}

void PickOccultistState::update()
{
        const io::InputData input = io::read_input();

        const auto action =
                m_browser.read(
                        input,
                        MenuInputMode::scrolling_and_letters);

        switch (action) {
        case MenuAction::selected: {
                const OccultistDomain domain = m_domains[m_browser.y()];

                player_bon::pick_occultist_domain(domain);

                states::pop();
        } break;

        case MenuAction::esc: {
                init::cleanup_session();
                states::pop_until(StateId::main_menu);
        } break;

        default:
                break;
        }
}

void PickOccultistState::draw()
{
        draw_box(panels::area(Panel::screen));

        const int screen_center_x = panels::center_x(Panel::screen);

        io::draw_text_center(
                " What is your spell domain? ",
                Panel::screen,
                {screen_center_x, 0},
                colors::title(),
                io::DrawBg::yes,
                colors::black(),
                true);  // Allow pixel-level adjustment

        int y = 0;

        const OccultistDomain domain_marked = m_domains[m_browser.y()];

        // Domains
        for (OccultistDomain domain : m_domains) {
                auto str =
                        std::string("(") +
                        m_browser.menu_keys()[y] +
                        std::string(")");

                const bool is_marked = (domain == domain_marked);

                Color color =
                        is_marked
                        ? colors::menu_key_highlight()
                        : colors::menu_key_dark();

                io::draw_text(
                        str,
                        Panel::create_char_menu,
                        {0, y},
                        color);

                const SpellDomain spell_domain =
                        player_bon::occultist_to_spell_domain(domain);

                str = spells::spell_domain_title(spell_domain);

                color =
                        is_marked
                        ? colors::menu_highlight()
                        : colors::menu_dark();

                io::draw_text(
                        str,
                        Panel::create_char_menu,
                        {4, y},
                        color);

                ++y;
        }

        // Description
        y = 0;

        const std::string descr =
                player_bon::occultist_domain_descr(domain_marked);

        ASSERT(!descr.empty());

        if (descr.empty()) {
                return;
        }

        const std::vector<std::string> formatted_lines = text_format::split(
                descr,
                panels::w(Panel::create_char_descr));

        for (const std::string& line : formatted_lines) {
                io::draw_text(
                        line,
                        Panel::create_char_descr,
                        {0, y},
                        colors::text());

                ++y;
        }
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

        init_browsers();
}

void PickTraitState::on_window_resized()
{
        init_browsers();
}

void PickTraitState::init_browsers()
{
        const int choices_h = panels::h(Panel::create_char_menu);

        m_browser_traits_avail.reset((int)m_traits_avail.size(), choices_h);
        m_browser_traits_unavail.reset((int)m_traits_unavail.size(), choices_h);

        // The "i" key is used for showing player information, do not use it as
        // a menu key.
        //
        // NOTE: The "i" key is not available at character creation, but this
        // menu key is removed in that case as well for consistency.
        //
        m_browser_traits_avail.remove_key('i');
        m_browser_traits_unavail.remove_key('i');

        m_browser_traits_avail.set_y(0);
        m_browser_traits_unavail.set_y(0);
}

void PickTraitState::update()
{
        if (config::is_bot_playing()) {
                states::pop();

                return;
        }

        const io::InputData input = io::read_input();

        // Switch trait screen mode?
        if (input.key == SDLK_TAB) {
                m_screen_mode =
                        (m_screen_mode == TraitScreenMode::pick_new)
                        ? TraitScreenMode::view_unavail
                        : TraitScreenMode::pick_new;

                return;
        }

        if ((input.key == 'i') && (m_is_char_creation == IsCharacterCreationTraitPick::no)) {
                disable_drawing();

                handle_show_player_info_command();

                enable_drawing();

                return;
        }

        MenuBrowser& browser =
                (m_screen_mode == TraitScreenMode::pick_new)
                ? m_browser_traits_avail
                : m_browser_traits_unavail;

        const MenuAction action = browser.read(input, MenuInputMode::scrolling_and_letters);

        switch (action) {
        case MenuAction::selected: {
                if (m_screen_mode == TraitScreenMode::pick_new) {
                        const Trait trait = m_traits_avail[browser.y()];

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
                                                {'y', 'n'},
                                                popup::MenuModeShowCancelHint::no,
                                                &choice)
                                        .run();

                                should_pick_trait = (choice == 0);
                        }

                        if (should_pick_trait) {
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
        } break;

        case MenuAction::esc: {
                if (states::contains_state(StateId::pick_name)) {
                        init::cleanup_session();
                        states::pop_until(StateId::main_menu);
                }
        }

        default:
                break;
        }
}

void PickTraitState::draw()
{
        draw_box(panels::area(Panel::screen));

        std::string title = m_title;
        std::string cmd_info;

        if (m_screen_mode == TraitScreenMode::pick_new) {
                cmd_info = "[TAB] to view unavailable traits";
        }
        else {
                // Viewing unavailable traits
                title = "Currently unavailable traits";
                cmd_info = "[TAB] to view available traits";
        }

        if (m_is_char_creation == IsCharacterCreationTraitPick::no) {
                cmd_info += " [i] to view game info";
        }

        const int screen_center_x = panels::center_x(Panel::screen);

        io::draw_text_center(
                " " + title + " ",
                Panel::screen,
                {screen_center_x, 0},
                colors::title(),
                io::DrawBg::yes,
                colors::black(),
                true);

        io::draw_text_center(
                " " + cmd_info + " ",
                Panel::screen,
                {screen_center_x, panels::y1(Panel::screen)},
                colors::title());

        MenuBrowser* browser = nullptr;

        std::vector<Trait>* traits = nullptr;

        if (m_screen_mode == TraitScreenMode::pick_new) {
                browser = &m_browser_traits_avail;
                traits = &m_traits_avail;
        }
        else {
                // Viewing unavailable traits
                browser = &m_browser_traits_unavail;
                traits = &m_traits_unavail;
        }

        const int browser_y = browser->y();

        const Trait trait_marked = traits->at(browser_y);

        const Range idx_range_shown = browser->range_shown();

        int y = 0;

        // Traits
        for (int i = idx_range_shown.min; i <= idx_range_shown.max; ++i) {
                const Trait trait = traits->at(i);

                const bool is_idx_marked = (browser_y == i);

                draw_trait_menu_item(trait, y, is_idx_marked, *browser);

                ++y;
        }

        // Draw "more" labels
        if (!browser->is_on_top_page()) {
                io::draw_text(
                        common_text::g_next_page_up_hint,
                        Panel::create_char_menu,
                        {0, -1},
                        colors::light_white());
        }

        if (!browser->is_on_btm_page()) {
                io::draw_text(
                        common_text::g_next_page_down_hint,
                        Panel::create_char_menu,
                        {0, panels::h(Panel::create_char_menu)},
                        colors::light_white());
        }

        // Description
        y = 0;

        std::string descr = player_bon::trait_descr(trait_marked);

        std::string descr_extra = player_bon::trait_descr_extra_when_picking(trait_marked);

        text_format::append_with_space(descr, descr_extra);

        const std::vector<std::string> formatted_descr =
                text_format::split(
                        descr,
                        panels::w(Panel::create_char_descr));

        for (const std::string& str : formatted_descr) {
                io::draw_text(
                        str,
                        Panel::create_char_descr,
                        {0, y},
                        colors::text());
                ++y;
        }

        // Prerequisites
        const player_bon::TraitPrereqData prereq_data =
                player_bon::trait_prereqs(
                        trait_marked,
                        player_bon::bg(),
                        player_bon::occultist_domain());

        const int y0_prereqs = 10;

        y = y0_prereqs;

        if (!prereq_data.traits.empty() || prereq_data.bg != Bg::END) {
                int x = 0;

                const std::string label = "Prerequisite(s):";

                io::draw_text(
                        label,
                        Panel::create_char_descr,
                        {x, y},
                        colors::text());

                x += (int)label.length() + 1;

                draw_trait_prereq_info(prereq_data, x, y);
        }
}

void PickTraitState::draw_trait_menu_item(
        const Trait trait,
        const int y,
        const bool is_marked,
        const MenuBrowser& browser) const
{
        const auto key_str =
                std::string("(") +
                browser.menu_keys()[y] +
                std::string(")");

        auto trait_name = player_bon::trait_title(trait);

        Color color_key;
        Color color;

        if (m_screen_mode == TraitScreenMode::pick_new) {
                if (is_marked) {
                        color_key = colors::menu_key_highlight();
                        color = colors::menu_highlight();
                }
                else {
                        // Not marked
                        color_key = colors::menu_key_dark();
                        color = colors::menu_dark();
                }
        }
        else {
                // Viewing unavailable traits
                if (is_marked) {
                        color_key = colors::menu_key_highlight();
                        color = colors::menu_highlight();
                }
                else {
                        // Not marked
                        color_key = colors::menu_key_dark();
                        color = colors::light_red();
                }
        }

        io::draw_text(
                key_str,
                Panel::create_char_menu,
                {0, y},
                color_key);

        io::draw_text(
                trait_name,
                Panel::create_char_menu,
                {(int)key_str.length() + 1, y},
                color);
}

void PickTraitState::draw_trait_prereq_info(
        const player_bon::TraitPrereqData& prereq_data,
        int x,
        const int y) const
{
        std::vector<ColoredString> prereq_titles;

        const auto& clr_prereq_ok = colors::light_green();
        const auto& clr_prereq_not_ok = colors::light_red();

        if (prereq_data.bg != Bg::END) {
                const auto& color =
                        (player_bon::bg() == prereq_data.bg)
                        ? clr_prereq_ok
                        : clr_prereq_not_ok;

                const std::string bg_title =
                        player_bon::bg_title(prereq_data.bg);

                prereq_titles.emplace_back(bg_title, color);
        }

        for (const auto prereq_trait : prereq_data.traits) {
                const bool is_picked =
                        player_bon::has_trait(prereq_trait);

                const auto& color =
                        is_picked
                        ? clr_prereq_ok
                        : clr_prereq_not_ok;

                const std::string trait_title =
                        player_bon::trait_title(prereq_trait);

                prereq_titles.emplace_back(trait_title, color);
        }

        const size_t nr_prereq_titles = prereq_titles.size();

        for (size_t prereq_idx = 0;
             prereq_idx < nr_prereq_titles;
             ++prereq_idx) {
                const auto& prereq_title = prereq_titles[prereq_idx];

                io::draw_text(
                        prereq_title.str,
                        Panel::create_char_descr,
                        {x, y},
                        prereq_title.color);

                x += prereq_title.str.length();

                if (prereq_idx < (nr_prereq_titles - 1)) {
                        io::draw_text(
                                ",",
                                Panel::create_char_descr,
                                {x, y},
                                colors::text());

                        ++x;
                }

                ++x;
        }
}

// -----------------------------------------------------------------------------
// Remove trait state
// -----------------------------------------------------------------------------
void RemoveTraitState::on_start()
{
        m_traits_can_be_removed = player_bon::traits_can_be_removed();

        init_browser();
}

void RemoveTraitState::on_window_resized()
{
        init_browser();
}

void RemoveTraitState::init_browser()
{
        const int choices_h = panels::h(Panel::create_char_menu);

        m_browser.reset(
                (int)m_traits_can_be_removed.size(),
                choices_h);

        // The "i" key is used for showing player information, do not use it as
        // a menu key.
        m_browser.remove_key('i');

        m_browser.set_y(0);
}

void RemoveTraitState::update()
{
        if (config::is_bot_playing()) {
                states::pop();

                return;
        }

        const io::InputData input = io::read_input();

        if (input.key == 'i') {
                handle_show_player_info_command();

                return;
        }

        const MenuAction action = m_browser.read(input, MenuInputMode::scrolling_and_letters);

        switch (action) {
        case MenuAction::selected: {
                const auto trait = m_traits_can_be_removed[m_browser.y()];

                const auto name = player_bon::trait_title(trait);

                bool should_remove_trait = true;

                states::draw();

                const std::string title = "Remove trait \"" + name + "\"?";

                int choice = 0;

                popup::Popup(popup::AddToMsgHistory::no)
                        .set_title(title)
                        .setup_menu_mode(
                                {"(Y)es", "(N)o"},
                                {'y', 'n'},
                                popup::MenuModeShowCancelHint::no,
                                &choice)
                        .run();

                should_remove_trait = (choice == 0);

                if (should_remove_trait) {
                        player_bon::remove_trait(trait);

                        game::add_history_event(
                                "Lost trait \"" +
                                name +
                                "\"");

                        states::pop();
                }
        } break;

        default:
                break;
        }
}

void RemoveTraitState::draw()
{
        draw_box(panels::area(Panel::screen));

        const int screen_center_x = panels::center_x(Panel::screen);

        io::draw_text_center(
                " Lose which trait? ",
                Panel::screen,
                {screen_center_x, 0},
                colors::title(),
                io::DrawBg::yes,
                colors::black(),
                true);

        io::draw_text_center(
                " [i] to view game info ",
                Panel::screen,
                {screen_center_x, panels::y1(Panel::screen)},
                colors::title());

        const int browser_y = m_browser.y();

        const Trait trait_marked = m_traits_can_be_removed.at(browser_y);

        const Range idx_range_shown = m_browser.range_shown();

        int y = 0;

        // Traits
        for (int i = idx_range_shown.min; i <= idx_range_shown.max; ++i) {
                const Trait trait = m_traits_can_be_removed[i];

                const bool is_idx_marked = (browser_y == i);

                draw_trait_menu_item(trait, y, is_idx_marked, m_browser);

                ++y;
        }

        // Draw "more" labels
        if (!m_browser.is_on_top_page()) {
                io::draw_text(
                        common_text::g_next_page_up_hint,
                        Panel::create_char_menu,
                        {0, -1},
                        colors::light_white());
        }

        if (!m_browser.is_on_btm_page()) {
                io::draw_text(
                        common_text::g_next_page_down_hint,
                        Panel::create_char_menu,
                        {0, panels::h(Panel::create_char_menu)},
                        colors::light_white());
        }

        // Description
        y = 0;

        std::string descr = player_bon::trait_descr(trait_marked);

        const std::vector<std::string> formatted_descr =
                text_format::split(
                        descr,
                        panels::w(Panel::create_char_descr));

        for (const std::string& str : formatted_descr) {
                io::draw_text(
                        str,
                        Panel::create_char_descr,
                        {0, y},
                        colors::text());
                ++y;
        }
}

void RemoveTraitState::draw_trait_menu_item(
        const Trait trait,
        const int y,
        const bool is_marked,
        const MenuBrowser& browser) const
{
        const auto key_str =
                std::string("(") +
                browser.menu_keys()[y] +
                std::string(")");

        auto trait_name = player_bon::trait_title(trait);

        Color color_key;
        Color color;

        if (is_marked) {
                color_key = colors::menu_key_highlight();
                color = colors::menu_highlight();
        }
        else {
                // Not marked
                color_key = colors::menu_key_dark();
                color = colors::menu_dark();
        }

        io::draw_text(
                key_str,
                Panel::create_char_menu,
                {0, y},
                color_key);

        io::draw_text(
                trait_name,
                Panel::create_char_menu,
                {(int)key_str.length() + 1, y},
                color);
}

// -----------------------------------------------------------------------------
// Enter name state
// -----------------------------------------------------------------------------
void EnterNameState::on_start()
{
        const std::string default_name = config::default_player_name();

        m_current_str = default_name;
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
                states::pop_until(StateId::main_menu);
                return;
        }

        if (input.key == SDLK_RETURN) {
                if (m_current_str.empty()) {
                        m_current_str = "Player";
                }
                else {
                        // Player has entered a string
                        config::set_default_player_name(m_current_str);
                }

                auto& d = *map::g_player->m_data;

                d.name_a = d.name_the = m_current_str;

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
        draw_box(panels::area(Panel::screen));

        const int screen_center_x = panels::center_x(Panel::screen);

        io::draw_text_center(
                " What is your name? ",
                Panel::screen,
                {screen_center_x, 0},
                colors::title(),
                io::DrawBg::yes);

        const int y_name = 3;

        std::string name_str = m_current_str;

        if ((m_current_str.size() < g_player_name_max_len) &&
            ((io::graphics_cycle_nr(io::GraphicsCycle::fast) % 2) == 0)) {
                name_str += "_";
        }

        const auto name_x0 = screen_center_x - ((int)g_player_name_max_len / 2);
        const auto name_x1 = name_x0 + (int)g_player_name_max_len - 1;

        io::draw_text(
                name_str,
                Panel::screen,
                {name_x0, y_name},
                colors::menu_highlight());

        R box_rect(
                {(int)name_x0 - 1, y_name - 1},
                {(int)name_x1 + 1, y_name + 1});

        draw_box(box_rect);
}

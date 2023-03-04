// =============================================================================
// Copyright 2011-2022 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "browser.hpp"

#include <algorithm>
#include <iterator>

#include "SDL_keycode.h"
#include "audio.hpp"
#include "audio_data.hpp"
#include "config.hpp"
#include "debug.hpp"
#include "global.hpp"
#include "io.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static int nr_menu_keys_avail(const std::vector<char>& menu_keys)
{
        return (
                (int)std::distance(
                        std::cbegin(menu_keys),
                        std::cend(menu_keys)));
}

static bool is_printable_ascii_char(const int key)
{
        // '!' = 33
        // '~' = 126

        return (key >= 33) && (key < 126);
}

// -----------------------------------------------------------------------------
// MenuBrowser
// -----------------------------------------------------------------------------
MenuAction MenuBrowser::read(
        const io::InputData& input,
        const MenuInputMode mode,
        const ForceAutoSelect force_auto_select)
{
        // NOTE: j k l are reserved for browsing with vi keys (not included in
        // the standard menu key letters)

        // Using both shortcut keys and left/right keys is not allowed; It is
        // enough that j k l are reserved from being used as a shortcut keys,
        // 'h' should not also be reserved.
        ASSERT(!(
                (mode == MenuInputMode::scrolling_and_letters) &&
                m_use_left_right_keys));

        if ((input.key == SDLK_UP) ||
            (input.key == SDLK_KP_8) ||
            (input.key == 'k')) {
                move(VerDir::up);
                return MenuAction::moved;
        }

        if ((input.key == SDLK_DOWN) ||
            (input.key == SDLK_KP_2) ||
            (input.key == 'j')) {
                move(VerDir::down);
                return MenuAction::moved;
        }

        if ((input.key == SDLK_PAGEUP) ||
            (input.key == '<')) {
                move_page(VerDir::up);
                return MenuAction::moved;
        }

        if ((input.key == SDLK_PAGEDOWN) ||
            (input.key == '>')) {
                move_page(VerDir::down);
                return MenuAction::moved;
        }

        if (m_use_left_right_keys) {
                // Left/right keys are used
                if ((input.key == SDLK_LEFT) ||
                    (input.key == SDLK_KP_4) ||
                    (input.key == 'h')) {
                        if (m_play_selection_audio) {
                                audio::play(audio::SfxId::menu_select);
                        }

                        return MenuAction::left;
                }

                if ((input.key == SDLK_RIGHT) ||
                    (input.key == SDLK_KP_6) ||
                    (input.key == 'l')) {
                        if (m_play_selection_audio) {
                                audio::play(audio::SfxId::menu_select);
                        }

                        return MenuAction::right;
                }
        }
        else {
                // Left/right keys are not used - consider 'l' as "selected".
                if (input.key == 'l') {
                        if (m_play_selection_audio) {
                                audio::play(audio::SfxId::menu_select);
                        }

                        return MenuAction::selected;
                }
        }

        if (input.key == SDLK_RETURN) {
                if (m_play_selection_audio) {
                        audio::play(audio::SfxId::menu_select);
                }

                return MenuAction::selected;
        }

        if (input.key == SDLK_SPACE) {
                return MenuAction::space;
        }

        if (input.key == SDLK_ESCAPE) {
                return MenuAction::esc;
        }

        // Handle shortcut keys.
        if ((mode == MenuInputMode::scrolling_and_letters) &&
            is_printable_ascii_char(input.key)) {
                const auto c = (char)input.key;

                const auto find_result =
                        std::find(
                                std::cbegin(m_menu_keys),
                                std::cend(m_menu_keys),
                                c);

                if (find_result == std::cend(m_menu_keys)) {
                        // Not a valid menu key, ever.
                        return MenuAction::none;
                }

#ifndef NDEBUG
                // Should never be used as letters (reserved for browsing).
                if ((c == 'j') || (c == 'k') || (c == 'l')) {
                        PANIC;
                }
#endif  // NDEBUG

                const auto relative_idx =
                        (int)std::distance(
                                std::cbegin(m_menu_keys),
                                find_result);

                if (relative_idx >= nr_items_shown()) {
                        // The key is not in the range of shown items.
                        return MenuAction::none;
                }

                // OK, the user did select an item.
                const int global_idx = top_idx_shown() + relative_idx;

                const bool is_new_idx_selected = (m_y != global_idx);

                set_y(global_idx);

                if (m_play_selection_audio) {
                        audio::play(audio::SfxId::menu_select);
                }

                if ((mode == MenuInputMode::scrolling_and_letters) &&
                    is_new_idx_selected &&
                    !config::is_auto_select_menu() &&
                    (force_auto_select == ForceAutoSelect::no)) {
                        // A different position than the current position was
                        // selected with a letter key, and auto select is
                        // disabled - only jump to this location.
                        return MenuAction::none;
                }

                return MenuAction::selected;
        }

        return MenuAction::none;
}

void MenuBrowser::move(const VerDir dir)
{
        const int last_idx = m_nr_items - 1;

        if (dir == VerDir::up) {
                // Up
                m_y = (m_y == 0) ? last_idx : (m_y - 1);
        }
        else {
                // Down
                m_y = (m_y == last_idx) ? 0 : (m_y + 1);
        }

        update_range_shown();

        audio::play(audio::SfxId::menu_browse);
}

void MenuBrowser::move_page(const VerDir dir)
{
        if (dir == VerDir::up) {
                // Up
                if (m_list_h >= 0) {
                        m_y -= m_list_h;
                }
                else {
                        // List height undefined (i.e. showing all)
                        m_y = 0;
                }
        }
        else {
                // Down
                if (m_list_h >= 0) {
                        m_y += m_list_h;
                }
                else {
                        // List height undefined (i.e. showing all)
                        m_y = m_nr_items - 1;
                }
        }

        set_y_nearest_valid();

        update_range_shown();

        audio::play(audio::SfxId::menu_browse);
}

void MenuBrowser::set_y(const int y)
{
        m_y = y;

        set_y_nearest_valid();

        update_range_shown();
}

Range MenuBrowser::range_shown() const
{
        // Shown ranged defined?
        if (m_list_h >= 0) {
                return m_range_shown;
        }
        else {
                // List height undefined (i.e. showing all)

                // Just return a range of the total number of items
                return {0, m_nr_items - 1};
        }
}

void MenuBrowser::update_range_shown()
{
        // Shown ranged defined?
        if (m_list_h >= 0) {
                const int top = (m_y / m_list_h) * m_list_h;
                const int btm = std::min(top + m_list_h, m_nr_items) - 1;

                m_range_shown.set(top, btm);
        }
}

void MenuBrowser::set_y_nearest_valid()
{
        if (m_nr_items >= 1) {
                m_y = std::clamp(m_y, 0, m_nr_items - 1);
        }
        else {
                m_y = 0;
        }
}

int MenuBrowser::nr_items_shown() const
{
        if (m_list_h >= 0) {
                // The list height has been defined
                return m_range_shown.len();
        }
        else {
                // List height undefined (i.e. showing all) - just return total
                // number of items
                return m_nr_items;
        }
}

int MenuBrowser::top_idx_shown() const
{
        // Shown ranged defined?
        if (m_list_h >= 0) {
                // List height undefined (i.e. showing all)
                return m_range_shown.min;
        }
        else {
                // Not showing all items
                return 0;
        }
}

int MenuBrowser::btm_idx_shown() const
{
        // Shown ranged defined?
        if (m_list_h >= 0) {
                return m_range_shown.max;
        }
        else {
                // List height undefined (i.e. showing all)
                return m_nr_items - 1;
        }
}

bool MenuBrowser::is_on_top_page() const
{
        // Shown ranged defined?
        if (m_list_h >= 0) {
                return m_range_shown.min == 0;
        }
        else {
                // List height undefined (i.e. showing all)
                return true;
        }
}

bool MenuBrowser::is_on_btm_page() const
{
        // Shown ranged defined?
        if (m_list_h >= 0) {
                return m_range_shown.max == m_nr_items - 1;
        }
        else {
                // List height undefined (i.e. showing all)
                return true;
        }
}

void MenuBrowser::reset(const int nr_items, const int list_h)
{
        m_nr_items = nr_items;

        // The size of the list viewable on screen is capped to the global
        // number of menu selection keys available (note that the client asks
        // the browser how many items should actually be drawn, so this capping
        // should be reflected for all clients).
        m_list_h = std::min(list_h, nr_menu_keys_avail(m_menu_keys));

        set_y_nearest_valid();

        update_range_shown();
}

void MenuBrowser::remove_key(const char key)
{
        const auto it =
                std::find(
                        std::cbegin(m_menu_keys),
                        std::cend(m_menu_keys),
                        key);

        if (it != std::cend(m_menu_keys)) {
                m_menu_keys.erase(it);
        }
}

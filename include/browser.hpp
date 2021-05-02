// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef BROWSER_HPP
#define BROWSER_HPP

#include <vector>

#include "random.hpp"

struct InputData;

enum class VerDir;

enum class MenuAction
{
        none,
        moved,
        selected,
        left,
        right,
        space,
        esc
};

enum class MenuInputMode
{
        scrolling_and_letters,
        scrolling
};

// TODO: Define in cpp file instead
const std::vector<char> std_menu_keys = {
        // NOTE: j k l is used for browsing and selecting in menus in vi mode
        'a',
        'b',
        'c',
        'd',
        'e',
        'f',
        'g',
        'h',
        'i',
        // 'j', 'k', 'l'
        'm',
        'n',
        'o',
        'p',
        'q',
        'r',
        's',
        't',
        'u',
        'v',
        'w',
        'x',
        'y',
        'z',

        'A',
        'B',
        'C',
        'D',
        'E',
        'F',
        'G',
        'H',
        'I',
        'J',
        'K',
        'L',
        'M',
        'N',
        'O',
        'P',
        'Q',
        'R',
        'S',
        'T',
        'U',
        'V',
        'W',
        'X',
        'Y',
        'Z',
};

// TODO: There's probably some public methods here that could be private/removed
class MenuBrowser
{
public:
        MenuBrowser(const int nr_items, const int list_h = -1)
        {
                reset(nr_items, list_h);
        }

        MenuBrowser() = default;

        MenuBrowser& operator=(const MenuBrowser&) = default;

        MenuAction read(const InputData& input, MenuInputMode mode);

        void move(VerDir dir);

        void move_page(VerDir dir);

        int y() const
        {
                return m_y;
        }

        void set_y(int y);

        Range range_shown() const;

        int nr_items_shown() const;

        int top_idx_shown() const;

        int btm_idx_shown() const;

        bool is_on_top_page() const;

        bool is_on_btm_page() const;

        int nr_items_tot() const
        {
                return m_nr_items;
        }

        bool is_at_idx(const int idx) const
        {
                return m_y == idx;
        }

        void reset(int nr_items, int list_h = -1);

        const std::vector<char>& menu_keys() const
        {
                return m_menu_keys;
        }

        void set_custom_menu_keys(const std::vector<char>& keys)
        {
                m_menu_keys = keys;
        }

        void remove_key(char key);

        void disable_selection_audio()
        {
                m_play_selection_audio = false;
        }

        void enable_left_right_keys()
        {
                m_use_left_right_keys = true;
        }

private:
        void set_y_nearest_valid();

        void update_range_shown();

        std::vector<char> m_menu_keys {std_menu_keys};
        int m_nr_items {0};
        int m_y {0};
        int m_list_h {-1};
        Range m_range_shown {-1, -1};
        bool m_play_selection_audio {true};
        bool m_use_left_right_keys {false};
};

#endif  // BROWSER_HPP

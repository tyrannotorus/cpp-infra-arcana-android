// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef BROWSER_HPP
#define BROWSER_HPP

#include <vector>

#include "random.hpp"

namespace io
{
struct InputData;
}  // namespace io

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

enum class ForceAutoSelect
{
        no,
        yes
};

extern const std::vector<char> std_menu_keys;

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

        MenuAction read(
                const io::InputData& input,
                MenuInputMode mode,
                ForceAutoSelect force_auto_select = ForceAutoSelect::no);

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

        // Entries the marker may not land on (section headers): browsing
        // steps over them. Indexed by entry, cleared by reset (call after
        // it). Out of range entries are selectable. An all unselectable
        // mask is ignored - the marker must be able to exist somewhere.
        void set_unselectable(const std::vector<bool>& unselectable);

        bool is_selectable(int idx) const;

        const std::vector<char>& menu_keys() const;

        // NOTE: This ONLY ever affects the keys on the first screen, if scrolling down to screens
        // further down, the default keys will always be used.
        //
        // Rationale: There should be two reasons for setting custom keys:
        //
        // 1) Menus like inventory screens where certain keys shall be reserved for a certain item
        //    type, like the medical bag, and appear at the top of the list.
        // 2) Small menus with completely custom keys that will only ever have one page.
        //
        // For case 1, the key should only be reserved and moved to the top on the first screen. If
        // scrolling to screens further down, the item will not exist on that screen, and there
        // should not be any specially handled reserved keys. For case 2, it doesn't matter.
        //
        void set_custom_menu_keys(const std::vector<char>& keys)
        {
                m_menu_keys = keys;
        }

        // Only affects the first screen, see note above.
        void remove_key(char key);

        void set_selection_audio_enabled(const bool value)
        {
                m_play_selection_audio = value;
        }

        void enable_left_right_keys()
        {
                m_use_left_right_keys = true;
        }

private:
        void set_y_nearest_valid();

        void update_range_shown();

        // Nearest selectable entry from idx in that direction, or -1
        int next_selectable(int idx, VerDir dir) const;

        std::vector<char> m_menu_keys {std_menu_keys};
        std::vector<bool> m_unselectable {};
        int m_nr_items {0};
        int m_y {0};
        int m_list_h {-1};
        Range m_range_shown {-1, -1};
        bool m_play_selection_audio {true};
        bool m_use_left_right_keys {false};
};

#endif  // BROWSER_HPP

// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef MENU_PAGE_HPP
#define MENU_PAGE_HPP

#include <string>
#include <vector>

#include "browser.hpp"
#include "scrollbar.hpp"
#include "colors.hpp"
#include "state.hpp"

namespace io
{
struct InputData;
}  // namespace io

// Length of a "(x)" style key prefix at the start of a menu entry label (0 if
// there is none). On this port the prefix is purely stylistic - it does not
// represent an actual key (there is no keyboard), so duplicate letters within
// one menu are fine.
int menu_key_prefix_len(const std::string& label);

// A thin horizontal divider rule at the vertical middle of screen gui row
// 'y', spanning gui columns x0..x1 (the same look as the popup rules)
void draw_menu_divider(int x0, int x1, int y);

// Stylistic width limits (gui cells) for the standard above/below divider
// rules framing centered content (menu lists, popup text blocks). A rule
// narrower than wide content is fine - a short centered rule reads as a
// section divider - but very wide rules are not.
// Menu pages remember which entry was marked, per page title, so that
// stepping back into a page returns to where you were. That memory belongs
// to ONE game - a new game must not open its character creation pages on
// the previous game's picks (see MainMenuState).
void forget_remembered_marked_entries();

constexpr int g_divider_min_w = 28;
constexpr int g_divider_max_w = 56;

// One row of a menu page. The label may start with a stylistic "(x)" key
// prefix, which is drawn in the menu key colors. The value is an optional
// second column (e.g. the current value of an option).
//
// A header row names the section below it: browsing steps over it, it
// cannot be selected or tapped, and it has no value column.
struct MenuPageEntry
{
        std::string label {};
        std::string value {};
        bool is_header {false};
};

// The standard fullscreen menu page: a fullscreen border with the page title
// embedded top center and a hint line embedded bottom center, and a centered
// selectable list framed by two horizontal divider rules (one above and one
// below the list). All fullscreen menus with a centered list extend this
// class, so that every such page renders and behaves through the same code.
// Pages with description text beside the list (character creation, the
// manual) use the two column variant, which is top aligned and drops the
// divider rules (see MenuDescrPageState).
//
// Interaction: swiping moves the marked entry, tapping a row marks and
// selects it, tapping elsewhere selects the marked entry, and the [ x ]
// border control (or the device back button) sends escape (cancel).
class MenuPageState : public State
{
public:

        void draw() final;

        void update() override;

        bool try_tap(const P& logical_px) final;

        void on_start() override;

        void on_window_resized() override;

protected:
        // --- Page content hooks ---
        virtual std::string page_title() const = 0;

        // Embedded in the bottom border row (empty = no footer). Defaults
        // to the standard "swipe/tap to select" guidance - override for a
        // more specific hint (or an empty string for none).
        virtual std::string page_hint() const;

        virtual std::vector<MenuPageEntry> page_entries() const = 0;

        virtual void on_entry_selected(int idx) = 0;

        // Which entry is marked when the page is opened for the FIRST time
        // (afterwards the page opens on the entry it was left on, see
        // forget_remembered_marked_entries). Called from on_start, i.e.
        // after the subclass has set up its entries.
        virtual int default_marked_idx() const
        {
                return 0;
        }

        // Left/right on an entry (e.g. stepping an option value)
        virtual void on_entry_left(int idx)
        {
                (void)idx;
        }

        virtual void on_entry_right(int idx)
        {
                (void)idx;
        }

        // Escape or space (including the [ x ] border control)
        virtual void on_cancelled();

        // Called with the input before the standard menu input handling -
        // return true if the input was consumed
        virtual bool handle_custom_input(const io::InputData& input)
        {
                (void)input;

                return false;
        }

        // Extra page content drawn after the frame and the list (e.g. a
        // description of the marked entry)
        virtual void draw_page_content() {}

        // Label/value color of an entry (the "(x)" key prefix always uses
        // the menu key colors)
        virtual Color entry_color(int idx, bool is_marked) const;

        virtual bool entry_plays_selection_audio(int idx) const
        {
                (void)idx;

                return true;
        }

        // --- List geometry (screen gui cells) ---

        // Leftmost column of the list block. Default: horizontally centered.
        virtual int list_x0(int block_w) const;

        // Row of the first visible entry. Default: vertically centered.
        virtual int list_y0(int nr_entries_shown) const;

        // Rightmost column the divider rules may extend to (e.g. to stay
        // clear of a description area beside the list)
        virtual int list_max_x1() const;

        // Rows available for the list. Bounded by the screen by default:
        // an unbounded list is drawn centered and simply runs off both
        // edges of the screen, with its outermost entries unreachable.
        virtual int list_h() const;

        // Whether the divider rules above and below the list are drawn.
        // The list block extents are computed either way - they are also
        // the scroll fade area.
        virtual bool show_list_dividers() const
        {
                return true;
        }

        virtual bool use_left_right_keys() const
        {
                return false;
        }

        // Resets the browser to the current entries, keeping the marked
        // entry when possible. Call whenever the entry set changes.
        void reset_browser();

        MenuBrowser m_browser {};

private:
        // Tap mapping, recorded while drawing (negative = no list drawn).
        // The row extents span the whole list block (the divider rule
        // width), not just the labels - the rows are finger targets.
        int m_drawn_list_y0 {-1};

        // Left edge of the drawn entries, and each visible entry's right
        // edge (label through value column) - the tap zone, see try_tap
        int m_drawn_entry_x0 {-1};
        std::vector<int> m_drawn_entry_x1 {};
};

#endif  // MENU_PAGE_HPP

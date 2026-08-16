// =============================================================================
// Copyright Werewolf Camp
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "text_page.hpp"

#include <algorithm>

#include "colors.hpp"
#include "common_text.hpp"
#include "config.hpp"
#include "draw_box.hpp"
#include "io.hpp"
#include "io_internal.hpp"
#include "menu_page.hpp"
#include "msg_log.hpp"
#include "panel.hpp"
#include "pos.hpp"
#include "scrollbar.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------

// Stylistic maximum width of the text (gui cells) - a text page is read as
// prose, and very long lines are hard to read (they would also run into the
// scrollbar). Bounded by what fits inside the border box.
static int text_max_w()
{
        const int max_w =
                panels::screen_box_area().w() -
                (2 * g_screen_border_content_inset);

        return std::min(74, max_w);
}

// Width of the longest line the text is drawn as - i.e. of the text as it
// actually renders, with the formatting markup resolved
static int drawn_block_w(Text& text)
{
        int block_w = 0;
        int line_w = 0;

        for (const TextAction& action : text.actions()) {
                switch (action.id) {
                case TextActionId::write_str:
                        line_w += (int)action.str.size();
                        break;

                case TextActionId::newline:
                case TextActionId::done:
                        block_w = std::max(block_w, line_w);
                        line_w = 0;
                        break;

                case TextActionId::change_color:
                        break;
                }
        }

        return block_w;
}

// -----------------------------------------------------------------------------
// TextPageState
// -----------------------------------------------------------------------------
void TextPageState::on_start()
{
        rebuild_text();
}

void TextPageState::on_window_resized()
{
        rebuild_text();
}

void TextPageState::rebuild_text()
{
        m_text = Text(page_text());

        m_text.set_w(text_max_w());

        // NOTE: The color must be set BEFORE the text is compiled (which
        // asking for the line count does) - it is what "{reset_color}"
        // resolves to, and the default is black
        m_text.set_color(colors::text());

        m_nr_lines = m_text.nr_lines();

        m_block_w = drawn_block_w(m_text);

        // The text may now be shorter than what is scrolled past
        set_scroll_px(m_scroll_px);
}

std::string TextPageState::page_hint() const
{
        return common_text::g_any_key_hint;
}

std::string TextPageState::title() const
{
        return page_title();
}

R TextPageState::content_gui_area() const
{
        const R box = panels::screen_box_area();

        // The divider rules sit one row inside the border box, and the text
        // one blank row clear of the rules
        return {
                box.p0.x + 1,
                box.p0.y + 3,
                box.p1.x - 1,
                box.p1.y - 3};
}

R TextPageState::content_px_rect() const
{
        return io::gui_to_px_rect(content_gui_area());
}

void TextPageState::draw()
{
        const R box = panels::screen_box_area();

        const int screen_center_x = panels::center_x(Panel::screen);

        draw_box(box);

        // An empty title leaves the top border unbroken - a page with
        // nothing to call itself says nothing there, rather than punching
        // a blank gap in the frame (the same as the footer, below)
        const std::string title_str = page_title();

        if (!title_str.empty()) {
                io::draw_text_center(
                        " " + title_str + " ",
                        Panel::screen,
                        {screen_center_x, box.p0.y},
                        colors::title(),
                        io::DrawBg::yes,
                        colors::black(),
                        true);  // Allow pixel-level adjustment
        }

        const std::string hint = page_hint();

        if (!hint.empty()) {
                io::draw_text_center(
                        " " + hint + " ",
                        Panel::screen,
                        {screen_center_x, box.p1.y},
                        colors::title(),
                        io::DrawBg::yes,
                        colors::black(),
                        true);  // Allow pixel-level adjustment
        }

        const R content = content_gui_area();

        const bool is_scrolled = (max_scroll_px() > 0);

        int rule_y_above = 0;
        int rule_y_below = 0;
        int text_y0 = 0;

        if (is_scrolled) {
                // Longer than the page: the text starts at the top of the
                // content area and scrolls, with the rules staying at the
                // edges of the area. NOTE: The scroll position is pixel
                // based (the bar moves smoothly), but the text is one
                // formatted block - it is drawn by whole rows.
                rule_y_above = content.p0.y - 2;
                rule_y_below = content.p1.y + 2;

                text_y0 =
                        content.p0.y -
                        (m_scroll_px / config::gui_cell_px_h());
        }
        else {
                // The whole block - the rule above, one blank row, the
                // text, one blank row, the rule below - is centered
                // vertically on the screen
                const int block_h = m_nr_lines + 4;

                const int block_y0 =
                        std::clamp(
                                panels::center_y(Panel::screen) -
                                        (block_h / 2),
                                box.p0.y + 1,
                                box.p1.y - block_h);

                rule_y_above = block_y0;
                text_y0 = block_y0 + 2;
                rule_y_below = block_y0 + block_h - 1;
        }

        // Divider rules kept within the stylistic width limits (a rule
        // narrower than the text is fine - reads as a section divider)
        const int rule_w =
                std::clamp(m_block_w, g_divider_min_w, g_divider_max_w);

        const int rule_x0 = screen_center_x - (rule_w / 2);

        draw_menu_divider(rule_x0, rule_x0 + rule_w - 1, rule_y_above);
        draw_menu_divider(rule_x0, rule_x0 + rule_w - 1, rule_y_below);

        if (is_scrolled) {
                // Scrolled text is clipped at the content area edges
                io::set_clip_rect_px(Panel::screen, content_px_rect());
        }

        io::draw_text(
                m_text,
                Panel::screen,
                {screen_center_x - (m_block_w / 2), text_y0},
                colors::text(),
                io::DrawBg::no);

        if (is_scrolled) {
                io::disable_clip_rect();

                draw_scroll_affordances();
        }
}

void TextPageState::update()
{
        if (config::is_bot_playing()) {
                // A page never blocks the bot
                on_confirmed();

                return;
        }

        InfoScreenState::update();
}

void TextPageState::add_text_to_msg_history()
{
        const std::string page_title_str = page_title();

        if (!page_title_str.empty()) {
                msg_log::add_line_to_history(page_title_str);
        }

        std::string line;

        for (const TextAction& action : m_text.actions()) {
                switch (action.id) {
                case TextActionId::write_str:
                        line += action.str;
                        break;

                case TextActionId::newline:
                case TextActionId::done:
                        msg_log::add_line_to_history(line);
                        line.clear();
                        break;

                case TextActionId::change_color:
                        break;
                }
        }
}

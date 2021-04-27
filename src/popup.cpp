// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "popup.hpp"

#include <stddef.h>
#include <algorithm>
#include <iterator>
#include <ostream>
#include <utility>

#include "audio.hpp"
#include "browser.hpp"
#include "colors.hpp"
#include "common_text.hpp"
#include "config.hpp"
#include "draw_box.hpp"
#include "io.hpp"
#include "msg_log.hpp"
#include "panel.hpp"
#include "rect.hpp"
#include "text_format.hpp"
#include "SDL_keycode.h"
#include "debug.hpp"
#include "pos.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static int get_x0(const int width)
{
        return panels::center_x(Panel::screen) - (width / 2);
}

static int get_box_y0(const int box_h)
{
        return panels::center_y(Panel::screen) - (box_h / 2) - 1;
}

static int get_title_y(const int text_h)
{
        const int box_h = text_h + 2;

        const int title_y = get_box_y0(box_h) + 1;

        return title_y;
}

static int max_msg_w()
{
        return io::g_min_nr_gui_cells_x - 6;
}

static void draw_horizontal_line(const int line_w, const int line_y)
{
        const P cell_px_dims(
                config::gui_cell_px_w(),
                config::gui_cell_px_h());

        const auto screen_center_x = panels::center_x(Panel::screen);

        const auto p0 =
                P(screen_center_x - (line_w / 2), line_y)
                        .scaled_up(cell_px_dims)
                        .with_y_offset(cell_px_dims.y / 2);

        const auto p1 =
                P(screen_center_x + (line_w / 2), line_y)
                        .scaled_up(cell_px_dims)
                        .with_y_offset(cell_px_dims.y / 2);

        io::draw_rectangle({p0, p1}, colors::dark_gray());
}

static int find_key_str_len(const std::string& str)
{
        if (str.empty())
        {
                ASSERT(false);

                return 0;
        }

        if (str[0] != '(')
        {
                return 0;
        }

        for (size_t i = 0; i < str.size(); ++i)
        {
                if (str[i] == ')')
                {
                        return i + 1;
                }
        }

        return 0;
}

// -----------------------------------------------------------------------------
// popup
// -----------------------------------------------------------------------------
namespace popup
{
// -----------------------------------------------------------------------------
// Popup
// -----------------------------------------------------------------------------
Popup::Popup(const AddToMsgHistory add_to_msg_history) :
        m_popup_state(std::make_unique<PopupState>(add_to_msg_history))
{
}

Popup::~Popup()
{
        // Sanity check: The popup has actually been run
        ASSERT(!m_popup_state);
}

void Popup::run()
{
        states::run_until_state_done(std::move(m_popup_state));
}

Popup& Popup::set_title(const std::string& title)
{
        m_popup_state->m_title = title;

        return *this;
}

Popup& Popup::set_msg(const std::string& msg)
{
        m_popup_state->m_msg = msg;

        return *this;
}

Popup& Popup::set_menu(
        const std::vector<std::string>& choices,
        const std::vector<char>& menu_keys,
        int* menu_choice_result)
{
        m_popup_state->m_menu_choices = choices;
        m_popup_state->m_menu_keys = menu_keys;
        m_popup_state->m_menu_choice_result = menu_choice_result;

        return *this;
}

Popup& Popup::set_sfx(audio::SfxId sfx)
{
        m_popup_state->m_sfx = sfx;

        return *this;
}

// -----------------------------------------------------------------------------
// PopupState
// -----------------------------------------------------------------------------
PopupState::PopupState(const AddToMsgHistory add_to_msg_history) :
        m_add_to_msg_history(add_to_msg_history)
{
}

void PopupState::on_start()
{
        ASSERT(m_menu_choices.size() == m_menu_keys.size());

        if (m_sfx != audio::SfxId::END)
        {
                audio::play(m_sfx);
        }

        if (m_add_to_msg_history == AddToMsgHistory::yes)
        {
                if (!m_title.empty())
                {
                        msg_log::add_line_to_history(m_title);
                }

                if (!m_msg.empty())
                {
                        const auto msg_lines =
                                text_format::split(
                                        m_msg,
                                        io::g_min_nr_gui_cells_x - 2);

                        for (const auto& line : msg_lines)
                        {
                                msg_log::add_line_to_history(line);
                        }
                }
        }

        if (!m_menu_choices.empty())
        {
                m_browser.reset(m_menu_choices.size());

                m_browser.set_custom_menu_keys(m_menu_keys);
        }
}

void PopupState::draw()
{
        if (m_menu_choices.empty())
        {
                draw_msg_popup();
        }
        else
        {
                draw_menu_popup();
        }
}

void PopupState::draw_msg_popup() const
{
        const auto text_max_w = max_msg_w();
        const auto msg_lines = text_format::split(m_msg, text_max_w);
        const auto text_h = (int)msg_lines.size() + 3;

        int horizontal_line_w = 0;

        {
                const auto msg_line_w =
                        (msg_lines.size() == 1)
                        ? (int)msg_lines[0].size()
                        : text_max_w;

                const auto confirm_line_w =
                        (int)common_text::g_confirm_hint.size();

                const int title_padding = 12;
                const int confirm_padding = title_padding;

                horizontal_line_w =
                        std::max(
                                {(int)m_title.size() + title_padding,
                                 msg_line_w,
                                 confirm_line_w + confirm_padding});

                horizontal_line_w = std::min(horizontal_line_w, text_max_w);
        }

        io::clear_screen();

        draw_box(panels::area(Panel::screen));

        auto y = get_title_y(text_h);

        draw_horizontal_line(horizontal_line_w, y);

        if (!m_title.empty())
        {
                io::draw_text_center(
                        " " + m_title + " ",
                        Panel::screen,
                        {panels::center_x(Panel::screen), y},
                        colors::title(),
                        io::DrawBg::yes,
                        colors::black(),
                        true);  // Allow pixel-level adjustmet
        }

        const bool show_msg_centered = msg_lines.size() == 1;

        for (const std::string& line : msg_lines)
        {
                ++y;

                if (show_msg_centered)
                {
                        io::draw_text_center(
                                line,
                                Panel::screen,
                                {panels::center_x(Panel::screen), y},
                                colors::text(),
                                io::DrawBg::no,
                                colors::black(),
                                true);  // Allow pixel-level adjustmet
                }
                else
                {
                        const auto text_x0 = get_x0(text_max_w);

                        io::draw_text(
                                line,
                                Panel::screen,
                                {text_x0, y},
                                colors::text(),
                                io::DrawBg::no);
                }
        }

        y += 2;

        draw_horizontal_line(horizontal_line_w, y);

        io::draw_text_center(
                " " + common_text::g_confirm_hint + " ",
                Panel::screen,
                {panels::center_x(Panel::screen), y},
                colors::menu_dark(),
                io::DrawBg::yes,
                colors::black());
}

void PopupState::draw_menu_popup() const
{
        const auto text_max_w = max_msg_w();
        const auto msg_lines = text_format::split(m_msg, text_max_w);
        const auto nr_msg_lines = (int)msg_lines.size();
        const auto title_h = m_title.empty() ? 0 : 1;

        const auto nr_blank_lines =
                ((nr_msg_lines == 0) && (title_h == 0))
                ? 0
                : 1;

        const auto nr_choices = (int)m_menu_choices.size();

        const auto text_h_tot =
                title_h +
                nr_msg_lines +
                nr_blank_lines +
                nr_choices;

        int choice_lines_max_w = 0;

        std::for_each(
                std::begin(m_menu_choices),
                std::end(m_menu_choices),
                [&choice_lines_max_w](const auto& choice_str) {
                        choice_lines_max_w =
                                std::max(
                                        choice_lines_max_w,
                                        (int)choice_str.length());
                });

        int horizontal_line_w = 0;

        if (nr_msg_lines <= 1)
        {
                const auto msg_line_w =
                        (nr_msg_lines == 0)
                        ? 0
                        : (int)msg_lines[0].size();

                const auto title_w = (int)m_title.size();

                horizontal_line_w =
                        std::max(
                                {title_w,
                                 choice_lines_max_w,
                                 msg_line_w});

                // Make the horizontal line somewhat wider than the title or
                // widest choice string
                horizontal_line_w += 12;

                // ...but not wider than the maximum text width
                horizontal_line_w = std::min(horizontal_line_w, text_max_w);
        }
        else
        {
                // More than one message line
                horizontal_line_w = text_max_w;
        }

        io::clear_screen();

        draw_box(panels::area(Panel::screen));

        int y = get_title_y(text_h_tot);

        draw_horizontal_line(horizontal_line_w, y);

        if (!m_title.empty())
        {
                io::draw_text_center(
                        " " + m_title + " ",
                        Panel::screen,
                        {panels::center_x(Panel::screen), y},
                        colors::title(),
                        io::DrawBg::yes,
                        colors::black(),
                        true);  // Allow pixel-level adjustmet
        }

        ++y;

        const bool show_msg_centered = (msg_lines.size() == 1);

        for (const std::string& line : msg_lines)
        {
                if (show_msg_centered)
                {
                        io::draw_text_center(
                                line,
                                Panel::screen,
                                {panels::center_x(Panel::screen), y},
                                colors::text(),
                                io::DrawBg::no,
                                colors::black(),
                                true);  // Allow pixel-level adjustmet
                }
                else
                {
                        // Draw the message with left alignment
                        const auto text_x0 = get_x0(text_max_w);

                        io::draw_text(
                                line,
                                Panel::screen,
                                {text_x0, y},
                                colors::text(),
                                io::DrawBg::no);
                }

                ++y;
        }

        if (!msg_lines.empty() || !m_title.empty())
        {
                ++y;
        }

        const int choice_x_pos =
                panels::center_x(Panel::screen) -
                (choice_lines_max_w / 2);

        for (size_t i = 0; i < m_menu_choices.size(); ++i)
        {
                const auto choice_str = m_menu_choices[i];

                const int key_str_len = find_key_str_len(choice_str);

                int draw_x_pos = choice_x_pos;

                if (key_str_len > 0)
                {
                        const auto key_str = choice_str.substr(0, key_str_len);

                        const auto key_color =
                                m_browser.is_at_idx(i)
                                ? colors::menu_key_highlight()
                                : colors::menu_key_dark();

                        io::draw_text(
                                key_str,
                                Panel::screen,
                                {draw_x_pos, y},
                                key_color,
                                io::DrawBg::no);
                }

                const auto choice_label_start = key_str_len;

                draw_x_pos = choice_x_pos + key_str_len;

                const auto choice_suffix =
                        choice_str.substr(
                                choice_label_start,
                                std::string::npos);

                const auto color =
                        m_browser.is_at_idx(i)
                        ? colors::menu_highlight()
                        : colors::menu_dark();

                io::draw_text(
                        choice_suffix,
                        Panel::screen,
                        {draw_x_pos, y},
                        color,
                        io::DrawBg::no);

                ++y;
        }

        draw_horizontal_line(horizontal_line_w, y);

        io::update_screen();
}

void PopupState::on_window_resized()
{
}

void PopupState::update()
{
        if (config::is_bot_playing())
        {
                if (m_menu_choice_result)
                {
                        *m_menu_choice_result = 0;
                }

                states::pop();

                return;
        }

        const auto input = io::get();

        if (m_menu_choices.empty())
        {
                switch (input.key)
                {
                case SDLK_SPACE:
                case SDLK_ESCAPE:
                case SDLK_RETURN:
                        states::pop();
                        break;

                default:
                        break;
                }
        }
        else
        {
                const auto action =
                        m_browser.read(
                                input,
                                MenuInputMode::scrolling_and_letters);

                switch (action)
                {
                case MenuAction::moved:
                        break;

                case MenuAction::esc:
                case MenuAction::space:
                        *m_menu_choice_result = -1;
                        states::pop();
                        break;

                case MenuAction::selected:
                        *m_menu_choice_result = m_browser.y();

                        TRACE
                                << "*m_menu_choice_result: "
                                << *m_menu_choice_result
                                << std::endl;

                        states::pop();
                        break;

                case MenuAction::left:
                case MenuAction::right:
                case MenuAction::none:
                        break;
                }
        }
}

StateId PopupState::id() const
{
        return StateId::popup;
}

}  // namespace popup

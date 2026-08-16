// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "popup.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <ostream>
#include <utility>

#include "SDL_keycode.h"
#include "audio.hpp"
#include "browser.hpp"
#include "colors.hpp"
#include "common_text.hpp"
#include "config.hpp"
#include "debug.hpp"
#include "draw_box.hpp"
#include "io.hpp"
#include "io_display.hpp"
#include "menu_page.hpp"
#include "msg_log.hpp"
#include "panel.hpp"
#include "pos.hpp"
#include "rect.hpp"
#include "text_format.hpp"

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
        return 74;
}

// Tappable confirm/cancel button areas drawn at the bottom of popups on
// touch devices (gui cell coordinates)
static R s_ok_button_area = {-1, -1, -1, -1};
static R s_cancel_button_area = {-1, -1, -1, -1};

// Menu choice rows of the most recently drawn menu popup: gui row, choice
// index, and the drawn extent of the choice text (for tapping entries
// directly)
struct ChoiceRow
{
        int y;
        int idx;
        int x0;
        int x1;
};

static std::vector<ChoiceRow> s_choice_rows;

static void reset_recorded_tap_areas()
{
        s_ok_button_area = {-1, -1, -1, -1};
        s_cancel_button_area = {-1, -1, -1, -1};

        s_choice_rows.clear();
}

// Draws tappable [ ok ] / [ cancel ] buttons centered on a popup line
// (replaces the keyboard oriented hint texts on touch devices)
static void draw_popup_buttons(const int y, const bool with_cancel)
{
        const std::string ok_str = "[ ok ]";
        const std::string cancel_str = "[ cancel ]";

        const std::string gap = "   ";

        const int total_w =
                with_cancel
                ? (int)(ok_str.size() + gap.size() + cancel_str.size())
                : (int)ok_str.size();

        const int screen_center_x = panels::center_x(Panel::screen);

        const int x0 = screen_center_x - (total_w / 2);

        io::draw_text(
                " " + ok_str + " ",
                Panel::screen,
                {x0 - 1, y},
                colors::menu_highlight(),
                io::DrawBg::yes,
                colors::black());

        s_ok_button_area = {
                x0,
                y,
                x0 + (int)ok_str.size() - 1,
                y};

        if (with_cancel) {
                const int cancel_x0 =
                        x0 + (int)ok_str.size() + (int)gap.size();

                io::draw_text(
                        " " + cancel_str + " ",
                        Panel::screen,
                        {cancel_x0 - 1, y},
                        colors::menu_dark(),
                        io::DrawBg::yes,
                        colors::black());

                s_cancel_button_area = {
                        cancel_x0,
                        y,
                        cancel_x0 + (int)cancel_str.size() - 1,
                        y};
        }
}

static void draw_horizontal_line(const int line_w, const int line_y)
{
        const auto screen_center_x = panels::center_x(Panel::screen);

        const int x0 = screen_center_x - (line_w / 2);

        draw_menu_divider(x0, x0 + line_w - 1, line_y);
}

// Draws the popup title embedded in the top border of the fullscreen box
// (the same way fullscreen menu pages draw their titles)
static void draw_title_in_border(const std::string& title)
{
        if (title.empty()) {
                return;
        }

        io::draw_text_center(
                " " + title + " ",
                Panel::screen,
                {panels::center_x(Panel::screen), panels::screen_box_area().p0.y},
                colors::title(),
                io::DrawBg::yes,
                colors::black(),
                true);  // Allow pixel-level adjustment
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
        m_popup_state(std::make_unique<MsgPopupState>()),
        m_add_to_msg_history(add_to_msg_history)
{
        m_popup_state->m_add_to_msg_history = m_add_to_msg_history;
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

void Popup::copy_common_config(
        const PopupState* const from,
        PopupState* const to) const
{
        to->m_title = from->m_title;
        to->m_msg = from->m_msg;
        to->m_sfx = from->m_sfx;
        to->m_add_to_msg_history = from->m_add_to_msg_history;
        to->m_cancelled_result = from->m_cancelled_result;
}

Popup& Popup::set_cancelled_result(bool* const cancelled_result)
{
        m_popup_state->m_cancelled_result = cancelled_result;

        return *this;
}

Popup& Popup::setup_menu_mode(
        const std::vector<std::string>& choices,
        int* menu_choice_result)
{
        auto new_popup_state = std::make_unique<MenuPopupState>();

        copy_common_config(m_popup_state.get(), new_popup_state.get());

        new_popup_state->m_menu_choices = choices;
        new_popup_state->m_menu_choice_result = menu_choice_result;

        m_popup_state = std::move(new_popup_state);

        return *this;
}

Popup& Popup::setup_number_query_mode(
        const query::QueryNumberConfig& config,
        int* const number_result)
{
        auto new_popup_state = std::make_unique<NumberQueryPopupState>();

        copy_common_config(m_popup_state.get(), new_popup_state.get());

        new_popup_state->m_config = config;
        new_popup_state->m_number_result = number_result;

        m_popup_state = std::move(new_popup_state);

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
PopupState::PopupState() = default;

void PopupState::on_start()
{
        if (m_sfx != audio::SfxId::END) {
                audio::play(m_sfx);
        }

        if (m_add_to_msg_history == AddToMsgHistory::yes) {
                if (!m_title.empty()) {
                        msg_log::add_line_to_history(m_title);
                }

                if (!m_msg.empty()) {
                        Text text(m_msg);

                        text.set_w(max_msg_w());

                        std::string str;

                        for (const TextAction& action : text.actions()) {
                                switch (action.id) {
                                case TextActionId::write_str:
                                        str += action.str;
                                        break;

                                case TextActionId::newline:
                                case TextActionId::done:
                                        msg_log::add_line_to_history(str);
                                        str = "";
                                        break;

                                case TextActionId::change_color:
                                        // str += action.str;
                                        break;
                                }
                        }
                }
        }

        on_start_specific();
}

void PopupState::on_window_resized()
{
}

StateId PopupState::id() const
{
        return StateId::popup;
}

// -----------------------------------------------------------------------------
// MsgPopupState
// -----------------------------------------------------------------------------
MsgPopupState::MsgPopupState() = default;

void MsgPopupState::on_start_specific()
{
}

void MsgPopupState::draw()
{
        const auto text_max_w = max_msg_w();
        const auto msg_lines = text_format::split(m_msg, text_max_w);

        const bool show_msg_centered = msg_lines.size() == 1;

        // Divider rules kept within the stylistic width limits (a rule
        // narrower than the text is fine - reads as a section divider)
        const int horizontal_line_w =
                std::clamp(
                        show_msg_centered
                                ? (int)msg_lines[0].size()
                                : text_max_w,
                        g_divider_min_w,
                        g_divider_max_w);

        reset_recorded_tap_areas();

        draw_box(panels::screen_box_area());

        draw_title_in_border(m_title);

        Text text(m_msg);

        text.set_w(text_max_w);

        const int nr_lines =
                show_msg_centered ? 1 : text.nr_lines();

        // The whole block - top rule, one padding row, the message lines,
        // one padding row, bottom rule (with the confirm hint embedded) -
        // is centered vertically on the screen
        const int block_h = nr_lines + 4;

        int y = panels::center_y(Panel::screen) - (block_h / 2);

        draw_horizontal_line(horizontal_line_w, y);

        y += 2;

        if (show_msg_centered) {
                // Centered one-liner message.

                // NOTE: draw_text_center just takes a plain std::string -
                // *FORMATTING NOT SUPPORTED!*

                io::draw_text_center(
                        msg_lines[0],
                        Panel::screen,
                        {panels::center_x(Panel::screen), y},
                        colors::text(),
                        io::DrawBg::no,
                        colors::black(),
                        true);  // Allow pixel-level adjustmet

                ++y;
        }
        else {
                // Multiline message, formatting supported here (draw_text takes
                // a Text object).

                const auto text_x0 = get_x0(text_max_w);

                io::draw_text(
                        text,
                        Panel::screen,
                        {text_x0, y},
                        colors::text(),
                        io::DrawBg::no);

                y += nr_lines;
        }

        ++y;

        draw_horizontal_line(horizontal_line_w, y);

        io::draw_text_center(
                " " + common_text::g_confirm_hint + " ",
                Panel::screen,
                {panels::center_x(Panel::screen), y},
                colors::menu_dark(),
                io::DrawBg::yes,
                colors::black());
}

void MsgPopupState::update()
{
        if (config::is_bot_playing()) {
                states::pop();

                return;
        }

        const auto input = io::read_input();

        switch (input.key) {
        case SDLK_ESCAPE:
                // Cancelled (the [ x ] control / back button)
                if (m_cancelled_result) {
                        *m_cancelled_result = true;
                }

                states::pop();
                break;

        case SDLK_SPACE:
        case SDLK_RETURN:
#ifndef NDEBUG
        // Cheat key for descending
        case SDLK_F2:
#endif  // NDEBUG
                states::pop();
                break;

        default:
                break;
        }
}

// -----------------------------------------------------------------------------
// MenuPopupState
// -----------------------------------------------------------------------------
MenuPopupState::MenuPopupState() = default;

void MenuPopupState::on_start_specific()
{
        m_browser.reset((int)m_menu_choices.size());
}

void MenuPopupState::draw()
{
        const auto text_max_w = max_msg_w();
        const auto msg_lines = text_format::split(m_msg, text_max_w);
        const auto nr_msg_lines = (int)msg_lines.size();

        const auto nr_blank_lines = (nr_msg_lines == 0) ? 0 : 1;

        const auto nr_choices = (int)m_menu_choices.size();

        const auto text_h_tot =
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

        {
                const auto msg_line_w =
                        (nr_msg_lines == 0)
                        ? 0
                        : (int)msg_lines[0].size();

                const int padding = 12;

                horizontal_line_w =
                        std::max(
                                msg_line_w,
                                choice_lines_max_w + padding);

                horizontal_line_w = std::min(horizontal_line_w, text_max_w);
        }

        reset_recorded_tap_areas();

        draw_box(panels::screen_box_area());

        draw_title_in_border(m_title);

        int y = get_title_y(text_h_tot);

        draw_horizontal_line(horizontal_line_w, y);

        ++y;

        const bool show_msg_centered = (msg_lines.size() == 1);

        for (const std::string& line : msg_lines) {
                if (show_msg_centered) {
                        io::draw_text_center(
                                line,
                                Panel::screen,
                                {panels::center_x(Panel::screen), y},
                                colors::text(),
                                io::DrawBg::no,
                                colors::black(),
                                true);  // Allow pixel-level adjustmet
                }
                else {
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

        if (!msg_lines.empty()) {
                ++y;
        }

        const int choice_x_pos =
                panels::center_x(Panel::screen) -
                (choice_lines_max_w / 2);

        for (size_t i = 0; i < m_menu_choices.size(); ++i) {
                const auto choice_str = m_menu_choices[i];

                s_choice_rows.push_back(
                        {y,
                         (int)i,
                         choice_x_pos,
                         choice_x_pos + (int)choice_str.size() - 1});

                const int key_str_len = menu_key_prefix_len(choice_str);

                int draw_x_pos = choice_x_pos;

                if (key_str_len > 0) {
                        const auto key_str = choice_str.substr(0, key_str_len);

                        const auto key_color =
                                m_browser.is_at_idx((int)i)
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
                        m_browser.is_at_idx((int)i)
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

        // Menu popups are operated by swiping (navigate) and tapping
        // (select); cancelling is done via the [ x ] control, tapping
        // outside the popup, or a "No" style choice - no button row
        draw_horizontal_line(horizontal_line_w, y);
}

void MenuPopupState::update()
{
        if (config::is_bot_playing()) {
                *m_menu_choice_result = 0;

                states::pop();

                return;
        }

        const auto input = io::read_input();

        // NOTE: Scrolling input only - any "(x)" style letters in the choice
        // strings are purely stylistic (entries are engaged by tapping, or
        // by swiping and confirming)
        const auto action =
                m_browser.read(
                        input,
                        MenuInputMode::scrolling,
                        ForceAutoSelect::yes);

        switch (action) {
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

bool MenuPopupState::try_tap(const P& logical_px)
{
        const P gui_pos(
                logical_px.x / config::gui_cell_px_w(),
                logical_px.y / config::gui_cell_px_h());

        // The choice's own text marks it, NOT the whole row - the rows span
        // the popup, so a row test would steal taps meant to confirm the
        // marked entry
        for (const auto& row : s_choice_rows) {
                if ((row.y == gui_pos.y) &&
                    (gui_pos.x >= (row.x0 - g_entry_tap_margin)) &&
                    (gui_pos.x <= (row.x1 + g_entry_tap_margin))) {
                        m_browser.set_y(row.idx);

                        break;
                }
        }

        // NOTE: Deliberately reported as NOT handled - the input layer then
        // synthesizes a confirm key for the tap (see io_input.cpp), which
        // selects the marked entry through the normal update path (a tap
        // handler must not pop states itself)
        return false;
}

// -----------------------------------------------------------------------------
// NumberQueryPopupState
// -----------------------------------------------------------------------------
NumberQueryPopupState::NumberQueryPopupState() = default;

void NumberQueryPopupState::on_start_specific()
{
        *m_number_result =
                std::clamp(
                        m_config.default_value,
                        m_config.allowed_range.min,
                        m_config.allowed_range.max);
}

std::string NumberQueryPopupState::calc_input_str_number_only() const
{
        return (*m_number_result >= 0) ? std::to_string(*m_number_result) : "";
}

void NumberQueryPopupState::update_input_str()
{
        m_input_str.clear();

        if (!m_is_empty_nr) {
                m_input_str = calc_input_str_number_only();
        }

        if (m_has_player_entered_value &&
            ((io::graphics_cycle_nr(io::GraphicsCycle::fast) % 2) == 0)) {
                m_input_str += "_";
        }
}

int NumberQueryPopupState::calc_max_nr_digits() const
{
        const auto max_str = std::to_string(m_config.allowed_range.max);

        return (int)max_str.length();
}

void NumberQueryPopupState::draw()
{
        const auto text_max_w = max_msg_w();
        const auto msg_lines = text_format::split(m_msg, text_max_w);
        const auto text_h = (int)msg_lines.size() + 4;

        // Including underscore
        const int nr_str_max_w = calc_max_nr_digits() + 1;

        int horizontal_line_w = 0;

        {
                const auto msg_line_w =
                        (msg_lines.size() == 1)
                        ? (int)msg_lines[0].size()
                        : text_max_w;

                const auto confirm_line_w =
                        (int)common_text::g_confirm_hint.size() +
                        (int)common_text::g_cancel_hint.size() +
                        1;

                const int padding = 12;

                horizontal_line_w =
                        std::max(
                                {msg_line_w,
                                 nr_str_max_w + padding,
                                 confirm_line_w + padding});

                horizontal_line_w = std::min(horizontal_line_w, text_max_w);
        }

        reset_recorded_tap_areas();

        draw_box(panels::screen_box_area());

        draw_title_in_border(m_title);

        auto y = get_title_y(text_h);

        draw_horizontal_line(horizontal_line_w, y);

        const bool show_msg_centered = msg_lines.size() == 1;

        if (show_msg_centered) {
                // Centered one-liner message.

                // NOTE: draw_text_center just takes a plain std::string -
                // *FORMATTING NOT SUPPORTED!*

                for (const std::string& line : msg_lines) {
                        ++y;

                        io::draw_text_center(
                                line,
                                Panel::screen,
                                {panels::center_x(Panel::screen), y},
                                colors::text(),
                                io::DrawBg::no,
                                colors::black(),
                                true);  // Allow pixel-level adjustmet
                }

                y += 2;
        }
        else {
                // Multiline message, formatting supported here (draw_text takes
                // a Text object).

                ++y;

                const auto text_x0 = get_x0(text_max_w);

                Text text(m_msg);

                text.set_w(text_max_w);

                io::draw_text(
                        text,
                        Panel::screen,
                        {text_x0, y},
                        colors::text(),
                        io::DrawBg::no);

                y += text.nr_lines() + 1;
        }

        update_input_str();

        Color fg_color;
        Color bg_color;

        if (m_has_player_entered_value) {
                fg_color = colors::light_white();
        }
        else {
                fg_color = colors::gray();
        }

        io::draw_text(
                m_input_str,
                Panel::screen,
                {panels::center_x(Panel::screen) - (nr_str_max_w / 2) - 1, y},
                fg_color);

        y += 2;

        draw_horizontal_line(horizontal_line_w, y);

        draw_popup_buttons(y, true);
}

void NumberQueryPopupState::update()
{
        auto input = io::read_input();

        // Convert keypad keys to numbers.
        switch (input.key) {
        case SDLK_KP_1:
                input.key = '1';
                break;
        case SDLK_KP_2:
                input.key = '2';
                break;
        case SDLK_KP_3:
                input.key = '3';
                break;
        case SDLK_KP_4:
                input.key = '4';
                break;
        case SDLK_KP_5:
                input.key = '5';
                break;
        case SDLK_KP_6:
                input.key = '6';
                break;
        case SDLK_KP_7:
                input.key = '7';
                break;
        case SDLK_KP_8:
                input.key = '8';
                break;
        case SDLK_KP_9:
                input.key = '9';
                break;
        case SDLK_KP_0:
                input.key = '0';
                break;
        default:
                break;
        }

        if (input.key == SDLK_RETURN) {
                // Confirm entered.

                *m_number_result =
                        std::clamp(
                                *m_number_result,
                                m_config.allowed_range.min,
                                m_config.allowed_range.max);

                states::pop();

                return;
        }

        if ((input.key == SDLK_SPACE) || (input.key == SDLK_ESCAPE)) {
                // Cancel entered.

                *m_number_result =
                        m_config.cancel_returns_default
                        ? m_config.default_value
                        : -1;

                states::pop();

                return;
        }

        if (input.key == SDLK_BACKSPACE) {
                // Backspace entered.

                m_has_player_entered_value = true;

                if (calc_input_str_number_only().length() == 1) {
                        // Single digit erased.
                        m_is_empty_nr = true;

                        *m_number_result = 0;
                }
                else {
                        // Erased a digit in a series of digits.
                        *m_number_result = *m_number_result / 10;
                }

                return;
        }

        if ((input.key >= '0') && (input.key <= '9')) {
                // Number entered.

                if (!m_has_player_entered_value) {
                        // Player has not yet modified the default value, clear the current value.
                        *m_number_result = 0;
                }

                const int current_num_digits =
                        m_is_empty_nr
                        ? 0
                        : (int)calc_input_str_number_only().length();

                const int max_nr_digits = calc_max_nr_digits();

                if (m_has_player_entered_value && (current_num_digits >= max_nr_digits)) {
                        // Not possible to add more digits.
                        return;
                }

                // Add a digit and clamp the result.

                int current_digit = input.key - '0';

                if (!m_is_empty_nr) {
                        *m_number_result *= 10;
                }

                *m_number_result += current_digit;

                *m_number_result =
                        std::clamp(
                                *m_number_result,
                                m_config.allowed_range.min,
                                m_config.allowed_range.max);

                m_has_player_entered_value = true;
                m_is_empty_nr = false;
        }
}

R ok_button_area()
{
        return s_ok_button_area;
}

R cancel_button_area()
{
        return s_cancel_button_area;
}

}  // namespace popup

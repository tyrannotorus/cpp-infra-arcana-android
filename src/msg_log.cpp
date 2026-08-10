// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "msg_log.hpp"

#include <algorithm>
#include <ostream>
#include <string>
#include <vector>

#include "actor.hpp"
#include "colors.hpp"
#include "debug.hpp"
#include "game_time.hpp"
#include "config.hpp"
#include "context_pins.hpp"
#include "global.hpp"
#include "io.hpp"
#include "io_display.hpp"
#include "io_internal.hpp"
#include "map.hpp"
#include "panel.hpp"
#include "pos.hpp"
#include "property_data.hpp"
#include "property_handler.hpp"
#include "query.hpp"
#include "rect.hpp"
#include "saving.hpp"
#include "state.hpp"
#include "text_format.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static MsgLine s_lines[msg_log::g_nr_log_lines];
static const size_t s_history_cap = 200;

static size_t s_history_size = 0;
static size_t s_history_count = 0;

static Msg s_history[s_history_cap];

// We only allow grouping up to 9 identical messages into a repeated message (e.g. "Bla. (x3)"), so
// that the repeat number is always a single digit (this simplifies calculation of required space
// for messages).
static const int s_max_nr_repeats = 9;

// "(xN)" where N is guaranteed to be a single digit, see above.
static const int s_repeat_str_len = 4;

static bool s_is_waiting_more_pompt = false;

// A log query (see query::yes_or_no) is waiting for an answer - the log
// content must not be overwritten while this is set.
static bool s_is_waiting_query = false;

// NOTE: The tappable [ action ] pins that used to trail the messages here
// are their own thing now, drawn on top of the action bar instead (see
// context_pins) - within reach of the thumb, rather than at the far top of
// the screen. What remains here is their LIFETIME: a pushed pin belongs to
// the context of the messages it was added with, so adding a message or
// clearing the log drops the pins along with it.

// When the message log is cleared, the current messages fade out. New messages will interrupt the
// fading and remove the messages immediately. This tracks the state of the fade mechanism.
enum class MsgFadeState
{
        allow_start_fade,
        is_fading,
        done,
        prevent_fade,
};

static MsgFadeState s_msg_fade_state = MsgFadeState::allow_start_fade;

// Tracks fade out percent for the current messages.
static int s_msg_fade_pct = 0;
static int s_msg_fade_turn_count = 0;

static size_t find_current_line_nr()
{
        if (s_lines[0].messages.empty()) {
                return 0;
        }

        size_t line_nr = 1;

        while (true) {
                if (s_lines[line_nr].messages.empty()) {
                        // Empty line found, return previous line number.
                        return line_nr - 1;
                }

                if (line_nr == (msg_log::g_nr_log_lines - 1)) {
                        // This is the last line, return this line number.
                        return line_nr;
                }

                ++line_nr;
        }

        ASSERT(false);
        return 0;
}

static size_t find_next_empty_line_nr()
{
        size_t next_empty_line_nr = 0;

        while (true) {
                if (s_lines[next_empty_line_nr].messages.empty()) {
                        // Empty line found, return this line number.
                        return next_empty_line_nr;
                }

                if (next_empty_line_nr == (msg_log::g_nr_log_lines - 1)) {
                        // All lines have content, wrap around.
                        return 0;
                }

                ++next_empty_line_nr;
        }

        ASSERT(false);
        return 0;
}

static int x_after_msg(const Msg* const msg)
{
        if (!msg) {
                return 0;
        }

        const std::string str = msg->text_with_repeats();

        return msg->x_pos() + (int)str.size() + 1;
}

static int worst_case_msg_w_for_line_nr(
        const int line_nr,
        const std::string& text)
{
        // NOTE: The "more" prompt is always drawn on an own row below the
        // messages - no line space is reserved
        (void)line_nr;

        const int max_w =
                (int)text.size() +
                s_repeat_str_len;

        return max_w;
}

static int msg_area_w_avail_for_text_part()
{
        const int w_avail =
                panels::w(Panel::log) -
                s_repeat_str_len;

        return w_avail;
}

static bool allow_convert_to_frenzied_str(const std::string& str)
{
        bool has_lower_case = false;

        for (auto c : str) {
                if ((c >= 'a') && (c <= 'z')) {
                        has_lower_case = true;
                        break;
                }
        }

        const char last_msg_char = str.back();

        bool is_ended_by_punctuation =
                (last_msg_char == '.') ||
                (last_msg_char == '!');

        return has_lower_case && is_ended_by_punctuation;
}

static std::string convert_to_frenzied_str(const std::string& str)
{
        // Convert to upper case.
        std::string frenzied_str = text_format::to_upper(str);

        // Do not put "!" if string contains "..."
        if (frenzied_str.find("...") == std::string::npos) {
                // Change "." to "!" at the end.
                if (frenzied_str.back() == '.') {
                        frenzied_str.back() = '!';
                }

                // Add some exclamation marks
                frenzied_str += "!!";
        }

        return frenzied_str;
}

// The log content overlays the top of the screen, with each row aligned
// horizontally towards the action bar's button side (upper left or upper
// right corner).
static bool is_right_aligned()
{
        // The bar's buttons flow from the corner opposite the side stats
        // panel (see action_bar)
        return config::is_side_panel_left();
}

// Width in gui cells of a line's messages
static int line_w(const MsgLine& line)
{
        if (line.messages.empty()) {
                return 0;
        }

        return x_after_msg(&line.messages.back()) - 1;
}

// Width in gui cells of the trailing content following the newest line
// (the "more" prompt)
static int trailing_w()
{
        return (int)msg_log::g_more_str.size();
}

static void draw_trailing(const P& px_pos)
{
        io::draw_text_at_px(
                msg_log::g_more_str,
                px_pos,
                colors::msg_more(),
                io::DrawBg::no,
                colors::black());
}

static void on_msg_not_fit_on_line(
        const std::string& str,
        Color color,
        const MsgInterruptPlayer interrupt_player,
        const MorePromptOnMsg add_more_prompt_on_msg,
        const CopyToMsgHistory copy_to_history,
        const size_t next_empty_line_nr)
{
        const bool is_next_empty_last_line =
                (next_empty_line_nr == (msg_log::g_nr_log_lines - 1));

        if (is_next_empty_last_line) {
                // The next empty line is the last message log line, Run a more prompt to clear the
                // log before running this message (it's annoying to have to confirm a more prompt
                // in the middle of a message).
                msg_log::more_prompt();

                msg_log::add(
                        str,
                        color,
                        interrupt_player,
                        add_more_prompt_on_msg,
                        copy_to_history);

                return;
        }

        int w_avail = msg_area_w_avail_for_text_part();

        // Since we split the message, we do not have to reserve space for the repeat string
        // (e.g. "x4").
        //
        // NOTE: In theory, it's actually possible that the last message will be repeated - this
        // would happen if another message equal to the last sub-message is added subsequently,
        // e.g.:
        //
        // Message 1  : "a long message foo bar", which is split into ->
        // Message 1a : "a long message"
        // Message 1b : "foo bar"
        // Message 2: : "foo bar"
        //
        // But this seems extremely unlikely in practice...
        //
        w_avail -= s_repeat_str_len;

        const auto lines = text_format::split(str, w_avail);

        for (size_t i = 0; i < lines.size(); ++i) {
                const bool is_last_msg = (i == (lines.size() - 1));

                // If the message is interrupting, only allow this for the last line of the split
                // message.
                const auto interrupt_actions_current_line =
                        is_last_msg
                        ? interrupt_player
                        : MsgInterruptPlayer::no;

                // If a more prompt was requested through the parameter, only allow this on the last
                // message.
                const auto add_more_prompt_current_line =
                        is_last_msg
                        ? add_more_prompt_on_msg
                        : MorePromptOnMsg::no;

                msg_log::add(
                        lines[i],
                        color,
                        interrupt_actions_current_line,
                        add_more_prompt_current_line,
                        copy_to_history);
        }
}

// -----------------------------------------------------------------------------
// msg_log
// -----------------------------------------------------------------------------
namespace msg_log
{
void init()
{
        for (auto& line : s_lines) {
                line.messages.clear();
                line.has_forced_line_break = false;
        }

        s_history_size = 0;
        s_history_count = 0;
        s_is_waiting_more_pompt = false;
        s_is_waiting_query = false;
        context_pins::clear();
        s_msg_fade_state = MsgFadeState::allow_start_fade;
        s_msg_fade_pct = 0;
        s_msg_fade_turn_count = 0;
}

void draw()
{
        // NOTE: The log is an overlay on top of the fullscreen map - only
        // the text itself covers the map (each text cell has a black
        // background), so the map stays visible where the log is empty.
        //
        // The content overlays the top of the screen, with each row
        // aligned horizontally towards the action bar's button side. The
        // top of the screen is for READING - the actions that go with what
        // is read are pins down by the bar (see context_pins).

        int nr_lines = 0;

        while ((nr_lines < (int)msg_log::g_nr_log_lines) &&
               !s_lines[nr_lines].messages.empty()) {
                ++nr_lines;
        }

        const bool has_trailing = s_is_waiting_more_pompt;

        if ((nr_lines == 0) && !has_trailing) {
                return;
        }

        io::set_display_for_panel(Panel::log);

        const int cell_w = config::gui_cell_px_w();
        const int cell_h = config::gui_cell_px_h();

        const R panel_px = io::panel_logical_px_rect(Panel::log);

        // The "more" prompt goes on an own row below the newest message
        // line, clearly set apart from the text
        const int trailing_w_cells = has_trailing ? trailing_w() : 0;

        const int nr_rows = nr_lines + (has_trailing ? 1 : 0);

        const int shade_pct =
                (s_msg_fade_state == MsgFadeState::is_fading)
                ? (100 - s_msg_fade_pct)
                : 0;

        auto row_px_y = [&](const int row) {
                return panel_px.p0.y + (row * cell_h);
        };

        auto row_px_x0 = [&](const int w_cells) {
                if (is_right_aligned()) {
                        return panel_px.p1.x + 1 - (w_cells * cell_w);
                }

                return panel_px.p0.x;
        };

        for (int i = 0; i < nr_lines; ++i) {
                const int x0_px = row_px_x0(line_w(s_lines[i]));
                const int y_px = row_px_y(i);

                for (const Msg& msg : s_lines[i].messages) {
                        io::draw_text_at_px(
                                msg.text_with_repeats(),
                                {x0_px + (msg.x_pos() * cell_w), y_px},
                                msg.color().shaded(shade_pct),
                                io::DrawBg::yes,
                                colors::black());
                }
        }

        if (has_trailing) {
                draw_trailing(
                        {row_px_x0(trailing_w_cells),
                         row_px_y(nr_rows - 1)});
        }
}

bool is_waiting_prompt()
{
        return s_is_waiting_more_pompt || s_is_waiting_query;
}

bool is_waiting_more_prompt()
{
        return s_is_waiting_more_pompt;
}

void set_waiting_query(const bool waiting)
{
        s_is_waiting_query = waiting;
}

void on_player_turn_start()
{
        if (s_msg_fade_state != MsgFadeState::is_fading) {
                return;
        }

        ASSERT(!s_is_waiting_more_pompt);

        if (s_msg_fade_turn_count >= 4) {
                // Force immediate clearing of the log.
                s_msg_fade_state = MsgFadeState::done;

                clear();

                return;
        }

        s_msg_fade_pct -= 10;

        ++s_msg_fade_turn_count;
}

void clear()
{
        // Context pins never outlive the log content they belong to
        context_pins::clear();

        if (s_msg_fade_state == MsgFadeState::is_fading) {
                // Fade out it ongoing, clearing the log has no effect.
                return;
        }

        bool is_all_copied_to_history = true;

        for (auto& line : s_lines) {
                for (auto& msg : line.messages) {
                        if (msg.should_copy_to_history() == CopyToMsgHistory::no) {
                                is_all_copied_to_history = false;
                        }
                }
        }

        if (
                !is_empty() &&
                is_all_copied_to_history &&
                (s_msg_fade_state == MsgFadeState::allow_start_fade)) {
                //
                // All of the following is fulfilled:
                // * The message log contains messages.
                // * All messages are allowed to be copied to message history (we do not want to
                //   fade out things like "which direction?").
                // * We are allowed to start a new fade.
                //
                // Start fading out the messages.
                //
                s_msg_fade_state = MsgFadeState::is_fading;

                s_msg_fade_pct = 100;
                s_msg_fade_turn_count = 0;

                return;
        }

        for (auto& line : s_lines) {
                for (auto& msg : line.messages) {
                        if (msg.should_copy_to_history() == CopyToMsgHistory::yes) {
                                // Add cleared line to history.
                                s_history[s_history_count % s_history_cap] = msg;

                                ++s_history_count;

                                if (s_history_size < s_history_cap) {
                                        ++s_history_size;
                                }
                        }
                }

                line.messages.clear();
                line.has_forced_line_break = false;
        }

        // Now that we have cleared the log, allow starting a fade again.
        s_msg_fade_state = MsgFadeState::allow_start_fade;
}

bool is_empty()
{
        // The log is considered empty if the first line is empty.
        return s_lines[0].messages.empty();
}

void add(
        const std::string& str,
        Color color,
        const MsgInterruptPlayer interrupt_player,
        const MorePromptOnMsg add_more_prompt_on_msg,
        const CopyToMsgHistory copy_to_history)
{
        ASSERT(!str.empty());

        if (saving::is_loading()) {
                // If we are loading the game, never print messages (this allows silently running
                // stuff like equip hooks for items).
                return;
        }

        // New messages invalidate any context pins (the pins belong to
        // the context of the previous messages)
        context_pins::clear();

        if (str.empty()) {
                return;
        }

        if (str[0] == ' ') {
                TRACE
                        << "Message starts with space: \""
                        << str
                        << "\""
                        << std::endl;

                ASSERT(false);

                return;
        }

        if (s_msg_fade_state == MsgFadeState::is_fading) {
                // A fade out of old messages is ongoing while a new message was added. Force
                // immediate clearing of the log before adding new messages.
                s_msg_fade_state = MsgFadeState::done;

                clear();
        }

        if ((color == colors::text()) && !game_time::g_is_player_acting) {
                // This is something happening outside of the player acting, color the message
                // differently.
                color = colors::passive_text();
        }

        // If frenzied, change the message
        if (map::g_player->m_properties.has(prop::Id::frenzied) &&
            (copy_to_history == CopyToMsgHistory::yes) &&
            allow_convert_to_frenzied_str(str)) {
                const auto frenzied_str = convert_to_frenzied_str(str);

                add(
                        frenzied_str,
                        color,
                        interrupt_player,
                        add_more_prompt_on_msg,
                        copy_to_history);

                return;
        }

        // If a single message will not fit on the next empty line in the worst case (i.e. with
        // space reserved for a repetition string, and also for a "more" prompt if the next empty
        // line is the last line), split the message into multiple messages through recursive calls.
        const size_t next_empty_line_nr = find_next_empty_line_nr();

        const bool is_msg_fit_on_line =
                worst_case_msg_w_for_line_nr((int)next_empty_line_nr, str) <=
                panels::w(Panel::log);

        if (!is_msg_fit_on_line) {
                on_msg_not_fit_on_line(
                        str,
                        color,
                        interrupt_player,
                        add_more_prompt_on_msg,
                        copy_to_history,
                        next_empty_line_nr);

                return;
        }

        // Find the line number to add the message to
        auto current_line_nr = find_current_line_nr();

        // Handle forced line break
        if (s_lines[current_line_nr].has_forced_line_break) {
                ++current_line_nr;

                if (current_line_nr > (g_nr_log_lines - 1)) {
                        more_prompt();

                        current_line_nr = 0;
                }
        }

        // Are we on a non-empty line which is not the last line?
        if ((current_line_nr < (g_nr_log_lines - 1)) &&
            !s_lines[current_line_nr].messages.empty()) {
                // Does the new message fit?
                const int worst_case_w = worst_case_msg_w_for_line_nr((int)current_line_nr, str);

                const int new_x = x_after_msg(&s_lines[current_line_nr].messages.back());

                const int worst_case_x1 = new_x + worst_case_w - 1;

                if (worst_case_x1 >= panels::w(Panel::log)) {
                        ++current_line_nr;
                }
        }

        Msg* prev_msg = nullptr;

        if (!s_lines[current_line_nr].messages.empty()) {
                prev_msg = &s_lines[current_line_nr].messages.back();
        }

        bool is_repeated = false;

        // Check if message is identical to previous
        if (prev_msg && (add_more_prompt_on_msg == MorePromptOnMsg::no)) {
                const std::string prev_text = prev_msg->text();

                if ((prev_text == str) && (prev_msg->nr_repeats() < s_max_nr_repeats)) {
                        prev_msg->incr_repeats();

                        is_repeated = true;
                }
        }

        if (!is_repeated) {
                int msg_x0 = x_after_msg(prev_msg);

                const int worst_case_msg_w =
                        worst_case_msg_w_for_line_nr(
                                (int)current_line_nr,
                                str);

                const int worst_case_msg_x1 = msg_x0 + worst_case_msg_w - 1;

                if (worst_case_msg_x1 >= panels::w(Panel::log)) {
                        if (current_line_nr < (g_nr_log_lines - 1)) {
                                ++current_line_nr;
                        }
                        else {
                                more_prompt();

                                current_line_nr = 0;
                        }

                        msg_x0 = 0;
                }

                s_lines[current_line_nr]
                        .messages
                        .emplace_back(
                                str,
                                color,
                                msg_x0,
                                copy_to_history);
        }

        if (add_more_prompt_on_msg == MorePromptOnMsg::yes) {
                more_prompt();
        }

        // Messages may stop long actions like first aid.
        if (interrupt_player == MsgInterruptPlayer::yes) {
                map::g_player->interrupt_actions(ForceInterruptActions::no);
        }

        // Some actions are always interrupted by messages, regardless of the
        // "interrupt_all_player_actions" parameter.
        map::g_player->on_log_msg_printed();
}

void more_prompt()
{
        // If the current log is empty, do nothing.
        if (s_lines[0].messages.empty()) {
                return;
        }

        // This will prevent messages from fading out while waiting for the "more" prompt.
        //
        // This could otherwise happen in cases where the client code is calling "more_prompt"
        // directly while we are fading out another message.
        //
        // TODO: Is this still relevant? Or is that from when messages just faded out automatically
        // in real time (as opposed to fading when the player turns start)?
        //
        s_msg_fade_state = MsgFadeState::prevent_fade;

        s_is_waiting_more_pompt = true;

        states::draw();

        query::wait_for_msg_more();

        s_is_waiting_more_pompt = false;

        // Force immediate clearing of the log.
        s_msg_fade_state = MsgFadeState::done;

        clear();
}

void newline()
{
        const auto line_nr = find_current_line_nr();
        auto& line = s_lines[line_nr];

        if (!line.messages.empty()) {
                line.has_forced_line_break = true;
        }
}

void add_line_to_history(const std::string& line_to_add)
{
        const Msg msg(
                line_to_add,
                colors::white(),
                0,
                CopyToMsgHistory::yes);  // Doesn't matter at this point

        s_history[s_history_count % s_history_cap] = {msg};

        ++s_history_count;

        if (s_history_size < s_history_cap) {
                ++s_history_size;
        }
}

std::vector<Msg> history()
{
        std::vector<Msg> result;

        result.reserve(s_history_size);

        size_t start = 0;

        if (s_history_count >= s_history_cap) {
                start = s_history_count - s_history_cap;
        }

        for (size_t i = start; i < s_history_count; ++i) {
                const auto& msg = s_history[i % s_history_cap];

                result.push_back(msg);
        }

        return result;
}

}  // namespace msg_log

// -----------------------------------------------------------------------------
// Message history state
// -----------------------------------------------------------------------------
StateId MsgHistoryState::id() const
{
        return StateId::message_history;
}

void MsgHistoryState::on_start()
{
        m_history = msg_log::history();

        // Start at the latest messages
        scroll_to_bottom();
}

void MsgHistoryState::on_window_resized()
{
        scroll_to_bottom();
}

std::string MsgHistoryState::title() const
{
        std::string title;

        if (m_history.empty()) {
                title = "No message history";
        }
        else {
                // History has content
                const std::string msg_nr_str_first =
                        std::to_string(first_visible_line() + 1);

                const std::string msg_nr_str_last =
                        std::to_string(last_visible_line() + 1);

                title =
                        "Messages " +
                        msg_nr_str_first + "-" +
                        msg_nr_str_last +
                        " of " + std::to_string(m_history.size());
        }

        return title;
}

void MsgHistoryState::draw()
{
        draw_interface();

        draw_scrollable_content();
}

ColoredString MsgHistoryState::content_line(const int line_idx) const
{
        const Msg& msg = m_history[line_idx];

        return {msg.text_with_repeats(), msg.color()};
}

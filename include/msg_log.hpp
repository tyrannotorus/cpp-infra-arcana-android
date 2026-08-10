// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef MSG_LOG_HPP
#define MSG_LOG_HPP

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "colors.hpp"
#include "info_screen_state.hpp"
#include "state.hpp"

struct P;

namespace io
{
enum class GraphicsCycle;
}  // namespace io

enum class MorePromptOnMsg
{
        no,
        yes
};

enum class MsgInterruptPlayer
{
        no,
        yes
};

enum class CopyToMsgHistory
{
        no,
        yes
};

class Msg
{
public:
        Msg(std::string text,
            const Color& color,
            const int x_pos,
            CopyToMsgHistory copy_to_history) :
                m_text(std::move(text)),
                m_color(color),
                m_x_pos(x_pos),
                m_copy_to_history(copy_to_history) {}

        Msg() = default;

        std::string text_with_repeats() const
        {
                std::string result_str = m_text;

                if (m_nr_repeats > 1) {
                        result_str += m_repeats_str;
                }

                return result_str;
        }

        std::string text() const
        {
                return m_text;
        }

        int nr_repeats() const
        {
                return m_nr_repeats;
        }

        void incr_repeats()
        {
                ++m_nr_repeats;

                m_repeats_str = "(x" + std::to_string(m_nr_repeats) + ")";
        }

        int x_pos() const
        {
                return m_x_pos;
        }

        Color color() const
        {
                return m_color;
        }

        CopyToMsgHistory should_copy_to_history() const
        {
                return m_copy_to_history;
        }

private:
        std::string m_text {};
        std::string m_repeats_str {};
        Color m_color {colors::white()};
        int m_nr_repeats {1};
        int m_x_pos {0};
        CopyToMsgHistory m_copy_to_history {CopyToMsgHistory::yes};
};

struct MsgLine
{
        bool has_forced_line_break {true};
        std::vector<Msg> messages {};
};

namespace msg_log
{
inline constexpr size_t g_nr_log_lines = 3;

// Tapping the screen confirms "more" prompts (a tap sends the confirm
// key). Styled like a context pin, and drawn on an own row below the
// messages - it stays with the text it is holding, unlike the pins, which
// live down by the action bar (see context_pins).
const std::string g_more_str = "[ tap to continue ]";

void init();

void draw();

// Whether the log is currently waiting for input (a "more" prompt or a
// query) - its content must then not be overwritten
bool is_waiting_prompt();

// Whether a "more" prompt specifically is waiting. NOTE: This is NOT the
// same as is_waiting_prompt: a "more" prompt is answered by tapping and
// nothing else, while a QUERY may well be answered by a gesture - the
// direction query (see query::dir) is answered by swiping.
bool is_waiting_more_prompt();

// Marks that a log query is waiting for an answer (see query::yes_or_no),
// so that the query text is not overwritten (see is_waiting_prompt)
void set_waiting_query(bool waiting);

// NOTE: The tappable [ action ] pins accompanying the messages are added
// through context_pins - they are drawn on top of the action bar, not
// here. The log only decides WHEN they go stale: clearing it, or adding a
// message, drops the pins that belonged to the previous context.

void on_player_turn_start();

void add(
        const std::string& str,
        Color color = colors::text(),
        MsgInterruptPlayer interrupt_player = MsgInterruptPlayer::no,
        MorePromptOnMsg add_more_prompt_on_msg = MorePromptOnMsg::no,
        CopyToMsgHistory copy_to_history = CopyToMsgHistory::yes);

// NOTE: This function can safely be called at any time. If there is content in the log, a "more"
// prompt will be run, and the log is cleared. If the log happens to be empty, nothing is done.
void more_prompt();

void newline();

void clear();

bool is_empty();

void add_line_to_history(const std::string& line_to_add);

std::vector<Msg> history();

}  // namespace msg_log

// -----------------------------------------------------------------------------
// Message history state
// -----------------------------------------------------------------------------
class MsgHistoryState : public InfoScreenState
{
public:
        MsgHistoryState() = default;

        ~MsgHistoryState() = default;

        void on_start() override;

        void on_window_resized() override;

        void draw() override;

        StateId id() const override;

private:
        std::string title() const override;

        InfoScreenType type() const override
        {
                return InfoScreenType::scrolling;
        }

        ColoredString content_line(int line_idx) const override;

        std::vector<Msg> m_history {};

        int get_lines_total() const override
        {
                return m_history.size();
        }
};

#endif  // MSG_LOG_HPP

// =============================================================================
// Copyright 2011-2024 Martin Törnqvist <m.tornq@gmail.com>
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

const std::string g_more_str = "[space]";

void init();

void draw();

void add(
        const std::string& str,
        Color color = colors::text(),
        MsgInterruptPlayer interrupt_player = MsgInterruptPlayer::no,
        MorePromptOnMsg add_more_prompt_on_msg = MorePromptOnMsg::no,
        CopyToMsgHistory copy_to_history = CopyToMsgHistory::yes);

// NOTE: This function can safely be called at any time. If there is content in
// the log, a "more" prompt will be run, and the log is cleared. If the log
// happens to be empty, nothing is done.
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

        void update() override;

        StateId id() const override;

private:
        void init_top_btm_line_numbers();

        std::string title() const override;

        InfoScreenType type() const override
        {
                return InfoScreenType::scrolling;
        }

        int m_top_line_nr {0};
        int m_btm_line_nr {0};

        std::vector<Msg> m_history {};
};

#endif  // MSG_LOG_HPP

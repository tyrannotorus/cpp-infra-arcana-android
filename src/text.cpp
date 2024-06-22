// =============================================================================
// Copyright 2011-2024 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "text.hpp"

#include <algorithm>
#include <cctype>
#include <iterator>
#include <memory>
#include <optional>

#include "io.hpp"
#include "misc.hpp"
#include "panel.hpp"
#include "pos.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// Text
// -----------------------------------------------------------------------------
void Text::draw(Panel panel, const P& pos)
{
        io::draw_text(*this, panel, pos, m_default_color);
}

int Text::nr_lines()
{
        compile();

        if (m_actions[0].id == TextActionId::done) {
                return 0;
        }

        auto n =
                std::count_if(
                        std::begin(m_actions),
                        std::end(m_actions),
                        [](const auto& a) {
                                return a.id == TextActionId::newline;
                        });

        ++n;

        return (int)n;
}

std::vector<TextAction> Text::actions()
{
        compile();

        return m_actions;
}

void Text::compile()
{
        if (!m_actions.empty()) {
                // Already built
                return;
        }

        TextCompiler compiler(m_raw_str, m_max_w, m_default_color);

        m_actions = compiler.compile();
}

// -----------------------------------------------------------------------------
// TextCompiler
// -----------------------------------------------------------------------------
std::vector<TextAction> TextCompiler::compile()
{
        std::vector<TextAction> actions;

        m_raw_str_pos = 0;
        m_line_w = 0;

        while (true) {
                const auto token_result = next_token(m_raw_str_pos);
                const std::string str = token_result.first;
                m_raw_str_pos = token_result.second;

                const auto action = token_to_action(str);

                if (action.id == TextActionId::write_str) {
                        const bool should_add_newline =
                                should_add_newline_before_write_action(
                                        action);

                        if (should_add_newline) {
                                // Remove trailing space
                                if (!actions.empty() &&
                                    (actions.back().str == " ")) {
                                        actions.pop_back();
                                }

                                TextAction newline_action;
                                newline_action.id = TextActionId::newline;
                                actions.push_back(newline_action);

                                m_line_w = 0;

                                // Skip adding single space after newline
                                if (action.str == " ") {
                                        continue;
                                }
                        }
                }
                else if (action.id == TextActionId::newline) {
                        m_line_w = 0;
                }

                actions.push_back(action);

                if (action.id == TextActionId::write_str) {
                        m_line_w += action.str.size();
                }
                else if (action.id == TextActionId::done) {
                        break;
                }
        }

        return actions;
}

bool TextCompiler::should_add_newline_before_write_action(
        const TextAction& current_action)
{
        size_t new_line_w =
                m_line_w +
                current_action.str.size();

        if (current_action.str != " ") {
                auto fwd_pos = m_raw_str_pos;

                while (true) {
                        const auto fwd_token_result = next_token(fwd_pos);
                        const auto fwd_str = fwd_token_result.first;
                        fwd_pos = fwd_token_result.second;
                        const auto fwd_action = token_to_action(fwd_str);

                        const bool is_newline =
                                fwd_action.id == TextActionId::newline;

                        const bool is_breaking_space =
                                (fwd_action.str == " ") &&
                                (fwd_str != "{_}");

                        const bool is_done_action =
                                fwd_action.id == TextActionId::done;

                        if (is_newline || is_done_action || is_breaking_space) {
                                break;
                        }

                        if (fwd_action.id == TextActionId::write_str) {
                                new_line_w += fwd_action.str.size();
                        }
                }
        }

        return new_line_w > m_max_w;
}

TextAction TextCompiler::token_to_action(const std::string& token) const
{
        TextAction action;

        if (token.empty()) {
                action.id = TextActionId::done;

                return action;
        }

        action.id = TextActionId::write_str;
        action.str = token;

        const size_t token_len = token.size();
        const auto first_c = token[0];
        const auto last_c = token[token_len - 1];

        if (first_c == '\n') {
                action.id = TextActionId::newline;

                return action;
        }

        if (token == "{_}") {
                action.str = " ";

                return action;
        }

        if ((first_c == '{') && (last_c == '}') && (token_len >= 3)) {
                const auto content = token.substr(1, token_len - 2);

                if ((content == "reset_color") ||
                    (content == "color_reset")) {
                        action.id = TextActionId::change_color;
                        action.color = m_default_color;

                        return action;
                }

                const auto color = colors::name_to_color(content);

                if (color.has_value()) {
                        // This is a valid color
                        action.id = TextActionId::change_color;
                        action.color = color.value();

                        return action;
                }
        }

        return action;
}

std::pair<std::string, size_t> TextCompiler::next_token(size_t pos) const
{
        std::string token;

        const auto raw_str_len = m_raw_str.size();

        if (pos == raw_str_len) {
                return {token, pos};
        }

        const auto starting_c = m_raw_str[pos];

        if (starting_c == '\n') {
                // Newline
                token = starting_c;
                ++pos;

                return {token, pos};
        }

        if (starting_c == ' ') {
                // A sequence of at least one space
                while (true) {
                        const auto c = m_raw_str[pos];

                        token += c;
                        ++pos;

                        if (pos == raw_str_len) {
                                // End of raw string
                                break;
                        }

                        const auto next_c = m_raw_str[pos];

                        if (next_c != ' ') {
                                break;
                        }
                }

                return {token, pos};
        }

        while (true) {
                const auto c = m_raw_str[pos];

                token += c;
                ++pos;

                if ((starting_c == '{') && (c == '}')) {
                        // Maybe a format change
                        break;
                }

                if (pos == raw_str_len) {
                        // End of the raw string
                        break;
                }

                const auto next_c = m_raw_str[pos];

                if ((next_c == ' ') || (next_c == '\n') || (next_c == '{')) {
                        // Next character is the start of a new token
                        break;
                }
        }

        return {token, pos};
}

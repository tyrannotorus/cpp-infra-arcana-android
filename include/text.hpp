// =============================================================================
// Copyright 2011-2021 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef TEXT_HPP
#define TEXT_HPP

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "colors.hpp"

enum class Panel;
struct P;
class Text;

enum class TextActionId
{
        write_str,
        newline,
        change_color,
        done,
};

struct TextAction
{
        TextActionId id {(TextActionId)0};
        std::string str {};
        Color color;
};

class Text
{
public:
        Text() = default;

        Text(const char* str) :
                m_raw_str(str) {}

        Text(std::string str) :
                m_raw_str(std::move(str)) {}

        void set_str(std::string str)
        {
                m_raw_str = std::move(str);
        }

        void append_str(const std::string& str)
        {
                m_raw_str += str;
        }

        void set_w(const int w)
        {
                m_max_w = w;
        }

        void set_color(const Color& color)
        {
                m_default_color = color;
        }

        void draw(Panel panel, const P& pos);

        int nr_lines();

        std::vector<TextAction> actions();

private:
        void compile();

        std::string m_raw_str {};
        size_t m_max_w {999};
        Color m_default_color {};

        std::vector<TextAction> m_actions {};
};

class TextCompiler
{
public:
        TextCompiler(
                const std::string& raw_str,
                size_t w,
                const Color& default_color) :
                m_raw_str(raw_str),
                m_max_w(w),
                m_default_color(default_color) {}

        std::vector<TextAction> compile();

private:
        bool should_add_newline_before_write_action(
                const TextAction& current_action);

        std::pair<std::string, size_t> next_token(size_t pos) const;

        TextAction token_to_action(const std::string& token) const;

        const std::string& m_raw_str;
        const size_t m_max_w;
        const Color& m_default_color;
        size_t m_line_w {0};
        size_t m_raw_str_pos {0};
};

#endif  // TEXT_HPP

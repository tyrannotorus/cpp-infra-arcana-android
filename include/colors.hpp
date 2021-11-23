// =============================================================================
// Copyright 2011-2021 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef COLORS_HPP
#define COLORS_HPP

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include "SDL_pixels.h"

//-----------------------------------------------------------------------------
// Color
//-----------------------------------------------------------------------------
class Color
{
public:
        Color();

        Color(const Color& other) = default;

        Color(uint8_t r, uint8_t g, uint8_t b);

        Color(const SDL_Color& sdl_color);

        ~Color() = default;

        Color& operator=(const Color& other)
        {
                if (&other == this)
                {
                        return *this;
                }

                m_sdl_color = other.m_sdl_color;

                return *this;
        }

        bool operator==(const Color& other) const
        {
                return (
                        m_sdl_color.r == other.m_sdl_color.r &&
                        m_sdl_color.g == other.m_sdl_color.g &&
                        m_sdl_color.b == other.m_sdl_color.b);
        }

        bool operator!=(const Color& other) const
        {
                return (
                        m_sdl_color.r != other.m_sdl_color.r ||
                        m_sdl_color.g != other.m_sdl_color.g ||
                        m_sdl_color.b != other.m_sdl_color.b);
        }

        Color fraction(double div) const;

        Color shaded(int pct) const;

        Color tinted(int pct) const;

        void clear();

        SDL_Color sdl_color() const
        {
                return m_sdl_color;
        }

        uint8_t r() const
        {
                return m_sdl_color.r;
        }

        uint8_t g() const
        {
                return m_sdl_color.g;
        }

        uint8_t b() const
        {
                return m_sdl_color.b;
        }

        void set_rgb(uint8_t r, uint8_t g, uint8_t b);

        void randomize_rgb(int range);

private:
        SDL_Color m_sdl_color;
};

//-----------------------------------------------------------------------------
// colors
//-----------------------------------------------------------------------------
namespace colors
{
void init();

std::optional<Color> name_to_color(const std::string& name);

std::string color_to_name(const Color& color);

// Available colors
const Color& black();
const Color& extra_dark_gray();
const Color& dark_gray();
const Color& gray();
const Color& white();
const Color& light_white();
const Color& red();
const Color& light_red();
const Color& dark_green();
const Color& green();
const Color& light_green();
const Color& dark_yellow();
const Color& yellow();
const Color& blue();
const Color& light_blue();
const Color& magenta();
const Color& light_magenta();
const Color& cyan();
const Color& light_cyan();
const Color& brown();
const Color& dark_brown();
const Color& gray_brown();
const Color& dark_gray_brown();
const Color& violet();
const Color& dark_violet();
const Color& orange();
const Color& gold();
const Color& sepia();
const Color& light_sepia();
const Color& dark_sepia();
const Color& teal();
const Color& light_teal();
const Color& dark_teal();

// GUI colors (using the colors above)
const Color& text();
const Color& passive_text();
const Color& menu_highlight();
const Color& menu_dark();
const Color& menu_key_highlight();
const Color& menu_key_dark();
const Color& title();
const Color& msg_good();
const Color& msg_bad();
const Color& msg_note();
const Color& msg_more();
const Color& mon_unaware();
const Color& mon_allied();
const Color& mon_temp_property();

}  // namespace colors

//-----------------------------------------------------------------------------
// Colored string
//-----------------------------------------------------------------------------
struct ColoredString
{
        ColoredString() = default;

        ColoredString(std::string the_str, const Color& the_color) :
                str(std::move(the_str)),
                color(the_color) {}

        ColoredString& operator=(const ColoredString& other) = default;

        std::string str {};
        Color color {colors::white()};
};

#endif  // COLORS_HPP

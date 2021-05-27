// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
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

        Color& operator=(const Color& other);

        bool operator==(const Color& other) const;

        bool operator!=(const Color& other) const;

        Color fraction(double div) const;

        Color shaded(int pct) const;

        Color tinted(int pct) const;

        void clear();

        SDL_Color sdl_color() const;

        uint8_t r() const;
        uint8_t g() const;
        uint8_t b() const;

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
Color black();
Color extra_dark_gray();
Color dark_gray();
Color gray();
Color white();
Color light_white();
Color red();
Color light_red();
Color dark_green();
Color green();
Color light_green();
Color dark_yellow();
Color yellow();
Color blue();
Color light_blue();
Color magenta();
Color light_magenta();
Color cyan();
Color light_cyan();
Color brown();
Color dark_brown();
Color gray_brown();
Color dark_gray_brown();
Color violet();
Color dark_violet();
Color orange();
Color gold();
Color sepia();
Color light_sepia();
Color dark_sepia();
Color teal();
Color light_teal();
Color dark_teal();

// GUI colors (using the colors above)
Color text();
Color passive_text();
Color menu_highlight();
Color menu_dark();
Color menu_key_highlight();
Color menu_key_dark();
Color title();
Color msg_good();
Color msg_bad();
Color msg_note();
Color msg_more();
Color mon_unaware_bg();
Color mon_allied_bg();
Color mon_temp_property_bg();

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

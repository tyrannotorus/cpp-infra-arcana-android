// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "colors.hpp"

#include "SDL_video.h"
#include <algorithm>
#include <vector>

#include "debug.hpp"
#include "misc.hpp"
#include "paths.hpp"
#include "random.hpp"
#include "xml.hpp"

//-----------------------------------------------------------------------------
// Private
//-----------------------------------------------------------------------------
// Defined in colors.xml
static SDL_Color s_black;
static SDL_Color s_extra_dark_gray;
static SDL_Color s_dark_gray;
static SDL_Color s_gray;
static SDL_Color s_white;
static SDL_Color s_light_white;
static SDL_Color s_red;
static SDL_Color s_light_red;
static SDL_Color s_dark_green;
static SDL_Color s_green;
static SDL_Color s_light_green;
static SDL_Color s_dark_yellow;
static SDL_Color s_yellow;
static SDL_Color s_blue;
static SDL_Color s_light_blue;
static SDL_Color s_magenta;
static SDL_Color s_light_magenta;
static SDL_Color s_cyan;
static SDL_Color s_light_cyan;
static SDL_Color s_brown;
static SDL_Color s_dark_brown;
static SDL_Color s_gray_brown;
static SDL_Color s_dark_gray_brown;
static SDL_Color s_violet;
static SDL_Color s_dark_violet;
static SDL_Color s_orange;
static SDL_Color s_gold;
static SDL_Color s_sepia;
static SDL_Color s_light_sepia;
static SDL_Color s_dark_sepia;
static SDL_Color s_teal;
static SDL_Color s_light_teal;
static SDL_Color s_dark_teal;

// Defined in colors_gui.xml
static SDL_Color s_text;
static SDL_Color s_menu_highlight;
static SDL_Color s_menu_dark;
static SDL_Color s_menu_key_highlight;
static SDL_Color s_menu_key_dark;
static SDL_Color s_title;
static SDL_Color s_msg_good;
static SDL_Color s_msg_bad;
static SDL_Color s_msg_note;
static SDL_Color s_msg_more;
static SDL_Color s_mon_unaware_bg;
static SDL_Color s_mon_allied_bg;
static SDL_Color s_mon_temp_property_bg;

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static std::vector<std::pair<std::string, Color>> s_str_color_pairs;

static SDL_Color rgb_hex_str_to_sdl_color(const std::string& str)
{
        if (str.size() != 6)
        {
                TRACE_ERROR_RELEASE
                        << "Invalid rgb hex string: '"
                        << str
                        << "'"
                        << std::endl;

                PANIC;
        }

        uint8_t rgb[3] = {};

        for (int i = 0; i < 3; ++i)
        {
                const std::string hex8_str = str.substr(2 * i, 2);

                rgb[i] = (uint8_t)std::stoi(hex8_str, nullptr, 16);
        }

        const SDL_Color sdl_color = {rgb[0], rgb[1], rgb[2], 0};

        return sdl_color;
}

static void load_color(
        xml::Element* colors_e,
        const std::string& name,
        SDL_Color& target_color)
{
        for (auto* e = xml::first_child(colors_e);
             e;
             e = xml::next_sibling(e))
        {
                const std::string current_name =
                        xml::get_attribute_str(e, "name");

                if (current_name != name)
                {
                        continue;
                }

                const std::string rgb_hex_str =
                        xml::get_attribute_str(e, "rgb_hex");

                const SDL_Color sdl_color =
                        rgb_hex_str_to_sdl_color(rgb_hex_str);

                TRACE << "Loaded color - "
                      << "name: \"" << name << "\""
                      << ", hexadecimal RGB string: \"" << rgb_hex_str << "\""
                      << ", decimal RGB values: "
                      << (int)sdl_color.r << ", "
                      << (int)sdl_color.g << ", "
                      << (int)sdl_color.b
                      << std::endl;

                target_color = sdl_color;

                s_str_color_pairs.emplace_back(name, Color(sdl_color));

                break;
        }
}

static void load_gui_color(
        xml::Element* gui_e,
        const std::string& type,
        SDL_Color& target_color)
{
        for (auto* e = xml::first_child(gui_e);
             e;
             e = xml::next_sibling(e))
        {
                const std::string current_type =
                        xml::get_attribute_str(e, "type");

                if (current_type != type)
                {
                        continue;
                }

                const std::string name =
                        xml::get_attribute_str(e, "color");

                TRACE << "Loaded gui color - "
                      << "type: \"" << type << "\", "
                      << "name: \"" << name << "\""
                      << std::endl;

                const auto color = colors::name_to_color(name).value();

                target_color = color.sdl_color();

                s_str_color_pairs.emplace_back(name, color);

                break;
        }
}

static void load_colors()
{
        tinyxml2::XMLDocument doc;

        xml::load_file(paths::data_dir() + "colors/colors.xml", doc);

        auto* colors_e = xml::first_child(doc);

        load_color(colors_e, "black", s_black);
        load_color(colors_e, "extra_dark_gray", s_extra_dark_gray);
        load_color(colors_e, "dark_gray", s_dark_gray);
        load_color(colors_e, "gray", s_gray);
        load_color(colors_e, "white", s_white);
        load_color(colors_e, "light_white", s_light_white);
        load_color(colors_e, "red", s_red);
        load_color(colors_e, "light_red", s_light_red);
        load_color(colors_e, "dark_green", s_dark_green);
        load_color(colors_e, "green", s_green);
        load_color(colors_e, "light_green", s_light_green);
        load_color(colors_e, "dark_yellow", s_dark_yellow);
        load_color(colors_e, "yellow", s_yellow);
        load_color(colors_e, "blue", s_blue);
        load_color(colors_e, "light_blue", s_light_blue);
        load_color(colors_e, "magenta", s_magenta);
        load_color(colors_e, "light_magenta", s_light_magenta);
        load_color(colors_e, "cyan", s_cyan);
        load_color(colors_e, "light_cyan", s_light_cyan);
        load_color(colors_e, "brown", s_brown);
        load_color(colors_e, "dark_brown", s_dark_brown);
        load_color(colors_e, "gray_brown", s_gray_brown);
        load_color(colors_e, "dark_gray_brown", s_dark_gray_brown);
        load_color(colors_e, "violet", s_violet);
        load_color(colors_e, "dark_violet", s_dark_violet);
        load_color(colors_e, "orange", s_orange);
        load_color(colors_e, "gold", s_gold);
        load_color(colors_e, "sepia", s_sepia);
        load_color(colors_e, "light_sepia", s_light_sepia);
        load_color(colors_e, "dark_sepia", s_dark_sepia);
        load_color(colors_e, "teal", s_teal);
        load_color(colors_e, "light_teal", s_light_teal);
        load_color(colors_e, "dark_teal", s_dark_teal);
}

static void load_gui_colors()
{
        tinyxml2::XMLDocument doc;

        xml::load_file(paths::data_dir() + "colors/colors_gui.xml", doc);

        auto* gui_e = xml::first_child(doc);

        load_gui_color(gui_e, "text", s_text);
        load_gui_color(gui_e, "menu_highlight", s_menu_highlight);
        load_gui_color(gui_e, "menu_dark", s_menu_dark);
        load_gui_color(gui_e, "menu_key_highlight", s_menu_key_highlight);
        load_gui_color(gui_e, "menu_key_dark", s_menu_key_dark);
        load_gui_color(gui_e, "title", s_title);
        load_gui_color(gui_e, "message_good", s_msg_good);
        load_gui_color(gui_e, "message_bad", s_msg_bad);
        load_gui_color(gui_e, "message_note", s_msg_note);
        load_gui_color(gui_e, "message_more", s_msg_more);
        load_gui_color(gui_e, "monster_unaware", s_mon_unaware_bg);
        load_gui_color(gui_e, "monster_allied", s_mon_allied_bg);
        load_gui_color(gui_e, "monster_temp_property", s_mon_temp_property_bg);
}

//-----------------------------------------------------------------------------
// Color
//-----------------------------------------------------------------------------
Color::Color() :
        m_sdl_color({0, 0, 0, 0})
{
}

Color::Color(uint8_t r, uint8_t g, uint8_t b) :
        m_sdl_color({r, g, b, 0})
{
}

Color::Color(const SDL_Color& sdl_color) :
        m_sdl_color(sdl_color)
{
}

Color& Color::operator=(const Color& other)
{
        if (&other == this)
        {
                return *this;
        }

        m_sdl_color = other.m_sdl_color;

        return *this;
}

bool Color::operator==(const Color& other) const
{
        return (
                m_sdl_color.r == other.m_sdl_color.r &&
                m_sdl_color.g == other.m_sdl_color.g &&
                m_sdl_color.b == other.m_sdl_color.b);
}

bool Color::operator!=(const Color& other) const
{
        return (
                m_sdl_color.r != other.m_sdl_color.r ||
                m_sdl_color.g != other.m_sdl_color.g ||
                m_sdl_color.b != other.m_sdl_color.b);
}

Color Color::fraction(const double div) const
{
        const auto result =
                Color(
                        (uint8_t)((double)m_sdl_color.r / div),
                        (uint8_t)((double)m_sdl_color.g / div),
                        (uint8_t)((double)m_sdl_color.b / div));

        return result;
}

Color Color::shaded(int pct) const
{
        pct = std::clamp(pct, 0, 100);

        const double f = (double)(100 - pct) / 100.0;

        const auto result =
                Color(
                        (uint8_t)((double)m_sdl_color.r * f),
                        (uint8_t)((double)m_sdl_color.g * f),
                        (uint8_t)((double)m_sdl_color.b * f));

        return result;
}

Color Color::tinted(int pct) const
{
        pct = std::clamp(pct, 0, 100);

        const double f = (double)pct / 100.0;

        const auto current_r = m_sdl_color.r;
        const auto current_g = m_sdl_color.g;
        const auto current_b = m_sdl_color.b;

        const auto new_r =
                (uint8_t)((double)current_r + ((double)(255 - current_r) * f));

        const auto new_g =
                (uint8_t)((double)current_g + ((double)(255 - current_g) * f));

        const auto new_b =
                (uint8_t)((double)current_b + ((double)(255 - current_b) * f));

        return Color(new_r, new_g, new_b);
}

void Color::clear()
{
        m_sdl_color.r = 0;
        m_sdl_color.g = 0;
        m_sdl_color.b = 0;
}

SDL_Color Color::sdl_color() const
{
        return m_sdl_color;
}

uint8_t Color::r() const
{
        return m_sdl_color.r;
}

uint8_t Color::g() const
{
        return m_sdl_color.g;
}

uint8_t Color::b() const
{
        return m_sdl_color.b;
}

void Color::set_rgb(const uint8_t r, const uint8_t g, const uint8_t b)
{
        m_sdl_color.r = r;
        m_sdl_color.g = g;
        m_sdl_color.b = b;
}

void Color::randomize_rgb(const int range)
{
        const Range random(-range / 2, range / 2);

        const int new_r = (int)m_sdl_color.r + random.roll();
        const int new_g = (int)m_sdl_color.g + random.roll();
        const int new_b = (int)m_sdl_color.b + random.roll();

        m_sdl_color.r = (uint8_t)std::clamp(new_r, 0, 255);
        m_sdl_color.g = (uint8_t)std::clamp(new_g, 0, 255);
        m_sdl_color.b = (uint8_t)std::clamp(new_b, 0, 255);
}

// -----------------------------------------------------------------------------
// Color handling
// -----------------------------------------------------------------------------
namespace colors
{
void init()
{
        TRACE_FUNC_BEGIN;

        s_str_color_pairs.clear();

        load_colors();

        load_gui_colors();

        TRACE_FUNC_END;
}

std::optional<Color> name_to_color(const std::string& name)
{
        auto search = std::find_if(
                std::begin(s_str_color_pairs),
                std::end(s_str_color_pairs),
                [name](const auto& str_color) {
                        return str_color.first == name;
                });

        if (search == std::end(s_str_color_pairs))
        {
                return {};
        }

        return search->second;
}

std::string color_to_name(const Color& color)
{
        auto search = std::find_if(
                std::begin(s_str_color_pairs),
                std::end(s_str_color_pairs),
                [color](const auto& str_color) {
                        return str_color.second == color;
                });

        if (search == std::end(s_str_color_pairs))
        {
#ifndef NDEBUG
                const auto sdl_color = color.sdl_color();

                TRACE << "No color name stored for color with RGB: "
                      << sdl_color.r << ", "
                      << sdl_color.g << ", "
                      << sdl_color.b << std::endl;

                ASSERT(false);
#endif  // NDEBUG

                return "";
        }

        return search->first;
}

//-----------------------------------------------------------------------------
// Available colors
//-----------------------------------------------------------------------------
Color black()
{
        return {s_black};
}

Color extra_dark_gray()
{
        return {s_extra_dark_gray};
}

Color dark_gray()
{
        return {s_dark_gray};
}

Color gray()
{
        return {s_gray};
}

Color white()
{
        return {s_white};
}

Color light_white()
{
        return {s_light_white};
}

Color red()
{
        return {s_red};
}

Color light_red()
{
        return {s_light_red};
}

Color dark_green()
{
        return {s_dark_green};
}

Color green()
{
        return {s_green};
}

Color light_green()
{
        return {s_light_green};
}

Color dark_yellow()
{
        return {s_dark_yellow};
}

Color yellow()
{
        return {s_yellow};
}

Color blue()
{
        return {s_blue};
}

Color light_blue()
{
        return {s_light_blue};
}

Color magenta()
{
        return {s_magenta};
}

Color light_magenta()
{
        return {s_light_magenta};
}

Color cyan()
{
        return {s_cyan};
}

Color light_cyan()
{
        return {s_light_cyan};
}

Color brown()
{
        return {s_brown};
}

Color dark_brown()
{
        return {s_dark_brown};
}

Color gray_brown()
{
        return {s_gray_brown};
}

Color dark_gray_brown()
{
        return {s_dark_gray_brown};
}

Color violet()
{
        return {s_violet};
}

Color dark_violet()
{
        return {s_dark_violet};
}

Color orange()
{
        return {s_orange};
}

Color gold()
{
        return {s_gold};
}

Color sepia()
{
        return {s_sepia};
}

Color light_sepia()
{
        return {s_light_sepia};
}

Color dark_sepia()
{
        return {s_dark_sepia};
}

Color teal()
{
        return {s_teal};
}

Color light_teal()
{
        return {s_light_teal};
}

Color dark_teal()
{
        return {s_dark_teal};
}

//-----------------------------------------------------------------------------
// GUI colors
//-----------------------------------------------------------------------------
Color text()
{
        return {s_text};
}

Color menu_highlight()
{
        return {s_menu_highlight};
}

Color menu_dark()
{
        return {s_menu_dark};
}

Color menu_key_highlight()
{
        return {s_menu_key_highlight};
}

Color menu_key_dark()
{
        return {s_menu_key_dark};
}

Color title()
{
        return {s_title};
}

Color msg_good()
{
        return {s_msg_good};
}

Color msg_bad()
{
        return {s_msg_bad};
}

Color msg_note()
{
        return {s_msg_note};
}

Color msg_more()
{
        return {s_msg_more};
}

Color mon_unaware_bg()
{
        return {s_mon_unaware_bg};
}

Color mon_allied_bg()
{
        return {s_mon_allied_bg};
}

Color mon_temp_property_bg()
{
        return {s_mon_temp_property_bg};
}

}  // namespace colors

// =============================================================================
// Copyright 2011-2024 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "colors.hpp"

#include <algorithm>
#include <iterator>
#include <ostream>
#include <vector>

#include "config.hpp"
#include "debug.hpp"
#include "paths.hpp"
#include "random.hpp"
#include "tinyxml2.h"
#include "xml.hpp"

//-----------------------------------------------------------------------------
// Private
//-----------------------------------------------------------------------------

// --- SDL Colors ---

// Defined in colors.xml
static SDL_Color s_sdl_black;
static SDL_Color s_sdl_extra_dark_gray;
static SDL_Color s_sdl_dark_gray;
static SDL_Color s_sdl_gray;
static SDL_Color s_sdl_white;
static SDL_Color s_sdl_light_white;
static SDL_Color s_sdl_red;
static SDL_Color s_sdl_light_red;
static SDL_Color s_sdl_dark_green;
static SDL_Color s_sdl_green;
static SDL_Color s_sdl_light_green;
static SDL_Color s_sdl_dark_yellow;
static SDL_Color s_sdl_yellow;
static SDL_Color s_sdl_blue;
static SDL_Color s_sdl_light_blue;
static SDL_Color s_sdl_dark_blue;
static SDL_Color s_sdl_magenta;
static SDL_Color s_sdl_light_magenta;
static SDL_Color s_sdl_cyan;
static SDL_Color s_sdl_light_cyan;
static SDL_Color s_sdl_brown;
static SDL_Color s_sdl_dark_brown;
static SDL_Color s_sdl_gray_brown;
static SDL_Color s_sdl_dark_gray_brown;
static SDL_Color s_sdl_violet;
static SDL_Color s_sdl_dark_violet;
static SDL_Color s_sdl_orange;
static SDL_Color s_sdl_gold;
static SDL_Color s_sdl_sepia;
static SDL_Color s_sdl_light_sepia;
static SDL_Color s_sdl_dark_sepia;
static SDL_Color s_sdl_teal;
static SDL_Color s_sdl_light_teal;
static SDL_Color s_sdl_dark_teal;

// Defined in colors_gui.xml
static SDL_Color s_sdl_text;
static SDL_Color s_sdl_passive_text;
static SDL_Color s_sdl_menu_highlight;
static SDL_Color s_sdl_menu_dark;
static SDL_Color s_sdl_menu_key_highlight;
static SDL_Color s_sdl_menu_key_dark;
static SDL_Color s_sdl_title;
static SDL_Color s_sdl_msg_good;
static SDL_Color s_sdl_msg_bad;
static SDL_Color s_sdl_msg_note;
static SDL_Color s_sdl_msg_more;
static SDL_Color s_sdl_mon_unaware;
static SDL_Color s_sdl_mon_allied;
static SDL_Color s_sdl_mon_temp_property;

// --- Colors ---

static Color s_black;
static Color s_extra_dark_gray;
static Color s_dark_gray;
static Color s_gray;
static Color s_white;
static Color s_light_white;
static Color s_red;
static Color s_light_red;
static Color s_dark_green;
static Color s_green;
static Color s_light_green;
static Color s_dark_yellow;
static Color s_yellow;
static Color s_blue;
static Color s_light_blue;
static Color s_dark_blue;
static Color s_magenta;
static Color s_light_magenta;
static Color s_cyan;
static Color s_light_cyan;
static Color s_brown;
static Color s_dark_brown;
static Color s_gray_brown;
static Color s_dark_gray_brown;
static Color s_violet;
static Color s_dark_violet;
static Color s_orange;
static Color s_gold;
static Color s_sepia;
static Color s_light_sepia;
static Color s_dark_sepia;
static Color s_teal;
static Color s_light_teal;
static Color s_dark_teal;

static Color s_text;
static Color s_passive_text;
static Color s_menu_highlight;
static Color s_menu_dark;
static Color s_menu_key_highlight;
static Color s_menu_key_dark;
static Color s_title;
static Color s_msg_good;
static Color s_msg_bad;
static Color s_msg_note;
static Color s_msg_more;
static Color s_mon_unaware;
static Color s_mon_allied;
static Color s_mon_temp_property;

static std::vector<std::pair<std::string, Color>> s_str_color_pairs;

static SDL_Color rgb_hex_str_to_sdl_color(const std::string& str)
{
        if (str.size() != 6) {
                TRACE_ERROR_RELEASE
                        << "Invalid rgb hex string: '"
                        << str
                        << "'"
                        << std::endl;

                PANIC;
        }

        uint8_t rgb[3] = {};

        for (int i = 0; i < 3; ++i) {
                const std::string hex8_str = str.substr(2 * i, 2);

                rgb[i] = (uint8_t)std::stoi(hex8_str, nullptr, 16);
        }

        const SDL_Color sdl_color = {rgb[0], rgb[1], rgb[2], 0};

        return sdl_color;
}

static SDL_Color load_color_from_xml(
        xml::Element* const colors_e,
        const std::string& id)
{
        for (auto* e = xml::first_child(colors_e); e; e = xml::next_sibling(e)) {
                const std::string found_id = xml::get_attribute_str(e, "name");

                if (found_id != id) {
                        continue;
                }

                const std::string rgb_hex_str = xml::get_attribute_str(e, "rgb_hex");

                const SDL_Color sdl_color = rgb_hex_str_to_sdl_color(rgb_hex_str);

                TRACE
                        << "Loaded color - "
                        << "ID: \"" << id << "\""
                        << ", hexadecimal RGB string: \"" << rgb_hex_str << "\""
                        << ", decimal RGB values: "
                        << (int)sdl_color.r << ", "
                        << (int)sdl_color.g << ", "
                        << (int)sdl_color.b
                        << std::endl;

                return sdl_color;
        }

        TRACE_ERROR_RELEASE
                << "Expected color with ID "
                << "'" << id << "' "
                << "to be defined, but it was not found" << std::endl;

        PANIC;

        return {};
}

static SDL_Color load_gui_color_from_xml(
        xml::Element* const gui_e,
        const std::string& id)
{
        for (auto* e = xml::first_child(gui_e); e; e = xml::next_sibling(e)) {
                const std::string found_id = xml::get_attribute_str(e, "type");

                if (found_id != id) {
                        continue;
                }

                const std::string color_id = xml::get_attribute_str(e, "color");

                TRACE
                        << "Loaded gui color - "
                        << "ID: \"" << id << "\", "
                        << "Color ID: \"" << color_id << "\""
                        << std::endl;

                const auto color = colors::name_to_color(color_id);

                return color.sdl_color();
        }

        TRACE_ERROR_RELEASE
                << "Expected GUI color with ID "
                << "'" << id << "' "
                << "to be defined, but it was not found" << std::endl;

        PANIC;

        return {};
}

static void init_color(
        xml::Element* const colors_e,
        const std::string& id,
        SDL_Color& sdl_color,
        Color& color)
{
        sdl_color = load_color_from_xml(colors_e, id);

        color = Color(sdl_color);

        sdl_color = color.sdl_color();

        s_str_color_pairs.emplace_back(id, color);
}

static void init_gui_color(
        xml::Element* const gui_e,
        const std::string& id,
        SDL_Color& sdl_color,
        Color& color)
{
        sdl_color = load_gui_color_from_xml(gui_e, id);

        color = Color(sdl_color);

        s_str_color_pairs.emplace_back(id, color);
}

static void init_colors()
{
        tinyxml2::XMLDocument doc;

        xml::load_file(paths::data_dir() + "colors/colors.xml", doc);

        auto* colors_e = xml::first_child(doc);

        init_color(colors_e, "COLOR_BLACK", s_sdl_black, s_black);
        init_color(colors_e, "COLOR_EXTRA_DARK_GRAY", s_sdl_extra_dark_gray, s_extra_dark_gray);
        init_color(colors_e, "COLOR_DARK_GRAY", s_sdl_dark_gray, s_dark_gray);
        init_color(colors_e, "COLOR_GRAY", s_sdl_gray, s_gray);
        init_color(colors_e, "COLOR_WHITE", s_sdl_white, s_white);
        init_color(colors_e, "COLOR_LIGHT_WHITE", s_sdl_light_white, s_light_white);
        init_color(colors_e, "COLOR_RED", s_sdl_red, s_red);
        init_color(colors_e, "COLOR_LIGHT_RED", s_sdl_light_red, s_light_red);
        init_color(colors_e, "COLOR_DARK_GREEN", s_sdl_dark_green, s_dark_green);
        init_color(colors_e, "COLOR_GREEN", s_sdl_green, s_green);
        init_color(colors_e, "COLOR_LIGHT_GREEN", s_sdl_light_green, s_light_green);
        init_color(colors_e, "COLOR_DARK_YELLOW", s_sdl_dark_yellow, s_dark_yellow);
        init_color(colors_e, "COLOR_YELLOW", s_sdl_yellow, s_yellow);
        init_color(colors_e, "COLOR_BLUE", s_sdl_blue, s_blue);
        init_color(colors_e, "COLOR_LIGHT_BLUE", s_sdl_light_blue, s_light_blue);
        init_color(colors_e, "COLOR_DARK_BLUE", s_sdl_dark_blue, s_dark_blue);
        init_color(colors_e, "COLOR_MAGENTA", s_sdl_magenta, s_magenta);
        init_color(colors_e, "COLOR_LIGHT_MAGENTA", s_sdl_light_magenta, s_light_magenta);
        init_color(colors_e, "COLOR_CYAN", s_sdl_cyan, s_cyan);
        init_color(colors_e, "COLOR_LIGHT_CYAN", s_sdl_light_cyan, s_light_cyan);
        init_color(colors_e, "COLOR_BROWN", s_sdl_brown, s_brown);
        init_color(colors_e, "COLOR_DARK_BROWN", s_sdl_dark_brown, s_dark_brown);
        init_color(colors_e, "COLOR_GRAY_BROWN", s_sdl_gray_brown, s_gray_brown);
        init_color(colors_e, "COLOR_DARK_GRAY_BROWN", s_sdl_dark_gray_brown, s_dark_gray_brown);
        init_color(colors_e, "COLOR_VIOLET", s_sdl_violet, s_violet);
        init_color(colors_e, "COLOR_DARK_VIOLET", s_sdl_dark_violet, s_dark_violet);
        init_color(colors_e, "COLOR_ORANGE", s_sdl_orange, s_orange);
        init_color(colors_e, "COLOR_GOLD", s_sdl_gold, s_gold);
        init_color(colors_e, "COLOR_SEPIA", s_sdl_sepia, s_sepia);
        init_color(colors_e, "COLOR_LIGHT_SEPIA", s_sdl_light_sepia, s_light_sepia);
        init_color(colors_e, "COLOR_DARK_SEPIA", s_sdl_dark_sepia, s_dark_sepia);
        init_color(colors_e, "COLOR_TEAL", s_sdl_teal, s_teal);
        init_color(colors_e, "COLOR_LIGHT_TEAL", s_sdl_light_teal, s_light_teal);
        init_color(colors_e, "COLOR_DARK_TEAL", s_sdl_dark_teal, s_dark_teal);
}

static void init_gui_colors()
{
        tinyxml2::XMLDocument doc;

        xml::load_file(paths::data_dir() + "colors/colors_gui.xml", doc);

        auto* gui_e = xml::first_child(doc);

        init_gui_color(gui_e, "GUICOLOR_TEXT", s_sdl_text, s_text);
        init_gui_color(gui_e, "GUICOLOR_PASSIVE_TEXT", s_sdl_passive_text, s_passive_text);
        init_gui_color(gui_e, "GUICOLOR_MENU_HIGHLIGHT", s_sdl_menu_highlight, s_menu_highlight);
        init_gui_color(gui_e, "GUICOLOR_MENU_DARK", s_sdl_menu_dark, s_menu_dark);
        init_gui_color(
                gui_e,
                "GUICOLOR_MENU_KEY_HIGHLIGHT",
                s_sdl_menu_key_highlight,
                s_menu_key_highlight);
        init_gui_color(gui_e, "GUICOLOR_MENU_KEY_DARK", s_sdl_menu_key_dark, s_menu_key_dark);
        init_gui_color(gui_e, "GUICOLOR_TITLE", s_sdl_title, s_title);
        init_gui_color(gui_e, "GUICOLOR_MESSAGE_GOOD", s_sdl_msg_good, s_msg_good);
        init_gui_color(gui_e, "GUICOLOR_MESSAGE_BAD", s_sdl_msg_bad, s_msg_bad);
        init_gui_color(gui_e, "GUICOLOR_MESSAGE_NOTE", s_sdl_msg_note, s_msg_note);
        init_gui_color(gui_e, "GUICOLOR_MESSAGE_MORE", s_sdl_msg_more, s_msg_more);
        init_gui_color(gui_e, "GUICOLOR_MONSTER_UNAWARE", s_sdl_mon_unaware, s_mon_unaware);
        init_gui_color(gui_e, "GUICOLOR_MONSTER_ALLIED", s_sdl_mon_allied, s_mon_allied);
        init_gui_color(
                gui_e,
                "GUICOLOR_MONSTER_TEMP_PROPERTY",
                s_sdl_mon_temp_property,
                s_mon_temp_property);
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

Color Color::with_brightness(int pct) const
{
        pct = std::max(0, pct);

        const uint8_t current_r = m_sdl_color.r;
        const uint8_t current_g = m_sdl_color.g;
        const uint8_t current_b = m_sdl_color.b;

        const uint8_t new_r = std::min(255, std::max(0, current_r * pct / 100));
        const uint8_t new_g = std::min(255, std::max(0, current_g * pct / 100));
        const uint8_t new_b = std::min(255, std::max(0, current_b * pct / 100));

        return {new_r, new_g, new_b};
}

Color Color::shaded(int pct) const
{
        pct = std::clamp(pct, 0, 100);

        const int f = 100 - pct;

        return {
                (uint8_t)(((int)m_sdl_color.r * f) / 100),
                (uint8_t)(((int)m_sdl_color.g * f) / 100),
                (uint8_t)(((int)m_sdl_color.b * f) / 100)};
}

Color Color::tinted(int pct) const
{
        pct = std::clamp(pct, 0, 100);

        const double f = (double)pct / 100.0;

        const uint8_t current_r = m_sdl_color.r;
        const uint8_t current_g = m_sdl_color.g;
        const uint8_t current_b = m_sdl_color.b;

        const auto new_r = (uint8_t)((double)current_r + ((double)(255 - current_r) * f));
        const auto new_g = (uint8_t)((double)current_g + ((double)(255 - current_g) * f));
        const auto new_b = (uint8_t)((double)current_b + ((double)(255 - current_b) * f));

        return {new_r, new_g, new_b};
}

void Color::clear()
{
        m_sdl_color.r = 0;
        m_sdl_color.g = 0;
        m_sdl_color.b = 0;
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

        init_colors();

        init_gui_colors();

        TRACE_FUNC_END;
}

Color name_to_color(const std::string& name)
{
        auto search = std::find_if(
                std::begin(s_str_color_pairs),
                std::end(s_str_color_pairs),
                [name](const auto& str_color) {
                        return str_color.first == name;
                });

        if (search == std::end(s_str_color_pairs)) {
                TRACE_ERROR_RELEASE
                        << "No color defined for name: "
                        << "'" << name << "'" << std::endl;

                PANIC;
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

        if (search == std::end(s_str_color_pairs)) {
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
const Color& black()
{
        return s_black;
}

const Color& extra_dark_gray()
{
        return s_extra_dark_gray;
}

const Color& dark_gray()
{
        return s_dark_gray;
}

const Color& gray()
{
        return s_gray;
}

const Color& white()
{
        return s_white;
}

const Color& light_white()
{
        return s_light_white;
}

const Color& red()
{
        return s_red;
}

const Color& light_red()
{
        return s_light_red;
}

const Color& dark_green()
{
        return s_dark_green;
}

const Color& green()
{
        return s_green;
}

const Color& light_green()
{
        return s_light_green;
}

const Color& dark_yellow()
{
        return s_dark_yellow;
}

const Color& yellow()
{
        return s_yellow;
}

const Color& blue()
{
        return s_blue;
}

const Color& light_blue()
{
        return s_light_blue;
}

const Color& dark_blue()
{
        return s_dark_blue;
}

const Color& magenta()
{
        return s_magenta;
}

const Color& light_magenta()
{
        return s_light_magenta;
}

const Color& cyan()
{
        return s_cyan;
}

const Color& light_cyan()
{
        return s_light_cyan;
}

const Color& brown()
{
        return s_brown;
}

const Color& dark_brown()
{
        return s_dark_brown;
}

const Color& gray_brown()
{
        return s_gray_brown;
}

const Color& dark_gray_brown()
{
        return s_dark_gray_brown;
}

const Color& violet()
{
        return s_violet;
}

const Color& dark_violet()
{
        return s_dark_violet;
}

const Color& orange()
{
        return s_orange;
}

const Color& gold()
{
        return s_gold;
}

const Color& sepia()
{
        return s_sepia;
}

const Color& light_sepia()
{
        return s_light_sepia;
}

const Color& dark_sepia()
{
        return s_dark_sepia;
}

const Color& teal()
{
        return s_teal;
}

const Color& light_teal()
{
        return s_light_teal;
}

const Color& dark_teal()
{
        return s_dark_teal;
}

//-----------------------------------------------------------------------------
// GUI colors
//-----------------------------------------------------------------------------
const Color& text()
{
        return s_text;
}

const Color& passive_text()
{
        return s_passive_text;
}

const Color& menu_highlight()
{
        return s_menu_highlight;
}

const Color& menu_dark()
{
        return s_menu_dark;
}

const Color& menu_key_highlight()
{
        return s_menu_key_highlight;
}

const Color& menu_key_dark()
{
        return s_menu_key_dark;
}

const Color& title()
{
        return s_title;
}

const Color& msg_good()
{
        return s_msg_good;
}

const Color& msg_bad()
{
        return s_msg_bad;
}

const Color& msg_note()
{
        return s_msg_note;
}

const Color& msg_more()
{
        return s_msg_more;
}

const Color& mon_unaware()
{
        return s_mon_unaware;
}

const Color& mon_allied()
{
        return s_mon_allied;
}

const Color& mon_temp_property()
{
        return s_mon_temp_property;
}

}  // namespace colors

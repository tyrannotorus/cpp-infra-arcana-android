// =============================================================================
// Copyright 2011-2024 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef IO_HPP
#define IO_HPP

#include <cstdint>
#include <string>
#include <vector>

#include "SDL_pixels.h"
#include "colors.hpp"
#include "gfx.hpp"
#include "panel.hpp"
#include "pos.hpp"
#include "rect.hpp"
#include "text.hpp"

namespace actor
{
class Actor;
}  // namespace actor

namespace io
{
// Character indicating a filled square
inline constexpr char g_filled_rect_char = 1;

enum class GraphicsCycle
{
        fast,
        slow,
        very_slow,

        END
};

enum class DrawBg
{
        no,
        yes
};

struct TileDrawObj
{
        void draw() const;

        gfx::TileId tile {gfx::TileId::END};

        Panel panel {Panel::screen};
        P pos {-1, -1};

        Color color {colors::black()};
        Color bg_color = {colors::black()};
        DrawBg draw_bg {DrawBg::yes};
};

struct CharacterDrawObj
{
        void draw() const;

        char character {0};

        Panel panel {Panel::screen};
        P pos {-1, -1};

        Color color {colors::black()};
        Color bg_color = {colors::black()};
        DrawBg draw_bg {DrawBg::yes};
};

struct MapDrawObj
{
        void draw() const;

        gfx::TileId tile {gfx::TileId::END};
        char character {0};
        P pos {-1, -1};
        Color color {colors::black()};
        Color color_bg {colors::black()};
};

struct InputData
{
        int key {-1};
        bool is_shift_held {false};
        bool is_ctrl_held {false};
        bool is_alt_held {false};
};

void init_sdl();
void cleanup_sdl();

void init_sdl_audio();
void cleanup_sdl_audio();

void init_other();
void cleanup_other();

// Updates the sceen with what is currently drawn
void update_screen();

void clear_screen();

// Actual user resolution (i.e. not logical size)
P get_native_resolution();

void on_user_toggle_fullscreen();
void on_user_toggle_scaling();

int graphics_cycle_nr(GraphicsCycle cycle_type);

R gui_to_px_rect(const R& rect);

// Scale from gui/map cell coordinate(s) to pixel coordinate(s)
int gui_to_px_coords_x(int value);
int gui_to_px_coords_y(int value);

int map_to_px_coords_x(int value);
int map_to_px_coords_y(int value);

P gui_to_px_coords(const P& pos);
P gui_to_px_coords(int x, int y);

P map_to_px_coords(const P& pos);
P map_to_px_coords(int x, int y);

P px_to_gui_coords(const P& px_pos);

P px_to_map_coords(const P& px_pos);

P gui_to_map_coords(const P& gui_pos);

// Returns a screen pixel position, relative to a cell position in a panel
P gui_to_px_coords(Panel panel, const P& offset);
P map_to_px_coords(Panel panel, const P& offset);

void draw_map_obj(const MapDrawObj& obj);

void draw_tile(const TileDrawObj& obj);

void draw_character(const CharacterDrawObj& obj);

void draw_text(
        Text text,
        Panel panel,
        P pos,
        Color color,
        DrawBg draw_bg = DrawBg::yes,
        const Color& bg_color = colors::black());

void draw_text_center(
        const std::string& str,
        Panel panel,
        P pos,
        const Color& color,
        DrawBg draw_bg = DrawBg::yes,
        const Color& bg_color = colors::black(),
        bool is_pixel_pos_adj_allowed = true);

void draw_text_right(
        const std::string& str,
        Panel panel,
        P pos,
        const Color& color,
        DrawBg draw_bg = DrawBg::yes,
        const Color& bg_color = colors::black());

void cover_cell(Panel panel, const P& offset);

void cover_panel(
        Panel panel,
        const Color& color = colors::black());

void cover_area(
        Panel panel,
        const R& area,
        const Color& color = colors::black());

void cover_area(
        Panel panel,
        const P& offset,
        const P& dims,
        const Color& color = colors::black());

void draw_rectangle(
        R px_rect,
        const Color& color);

void draw_rectangle_filled(
        R px_rect,
        const Color& color,
        uint8_t alpha = SDL_ALPHA_OPAQUE);

void draw_rectangle_filled_mod_blending(
        R px_rect,
        const Color& color,
        uint8_t alpha = SDL_ALPHA_OPAQUE);

void draw_logo();

// Draws a description "box" for items, spells, etc. The parameter lines may be
// empty, in which case an empty area is drawn.
//
// TODO: This does not belong in the io namespace (too high level).
//
void draw_descr_box(const std::vector<ColoredString>& lines);

// Run a flash animation at position.
void flash_at(const P& pos, const Color& color, int speed_pct = 100);

// Run a flash animation at actor. The flash will follow the actor if it moves.
void flash_at_actor(const actor::Actor& actor, const Color& color, int speed_pct = 100);

// Draw all currently active flash animations.
void draw_flash_animations();

// Clear all ongoing flash animations (e.g. when viewport changes).
void clear_all_flash_animations();

void sleep(uint32_t duration);

void clear_input();

InputData read_input();

}  // namespace io

#endif  // IO_HPP

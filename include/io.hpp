// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#ifndef IO_HPP
#define IO_HPP

#include <stdint.h>
#include <vector>
#include <string>

#include "colors.hpp"
#include "gfx.hpp"
#include "panel.hpp"
#include "rect.hpp"
#include "pos.hpp"

namespace actor
{
class Actor;
}  // namespace actor

struct CellRenderData
{
        CellRenderData& operator=(const CellRenderData&) = default;

        gfx::TileId tile = gfx::TileId::END;
        char character = 0;
        Color color = colors::black();
        Color color_bg = colors::black();
};

struct InputData
{
        int key {-1};
        bool is_shift_held {false};
        bool is_ctrl_held {false};
        bool is_alt_held {false};
};

namespace io
{
// The minimum window size is defined by whichever gives the largest window
// width or height: the minimum number of gui cells, or the minimum resolution.
inline constexpr int g_min_nr_gui_cells_x = 80;
inline constexpr int g_min_nr_gui_cells_y = 26;
inline constexpr int g_min_res_w = 800;
inline constexpr int g_min_res_h = 600;

enum class DrawBg
{
        no,
        yes
};

void init();
void cleanup();

void update_screen();

void clear_screen();

void on_fullscreen_toggled();

P min_screen_gui_dims();

R gui_to_px_rect(R rect);

// Scale from gui/map cell coordinate(s) to pixel coordinate(s)
int gui_to_px_coords_x(int value);
int gui_to_px_coords_y(int value);

int map_to_px_coords_x(int value);
int map_to_px_coords_y(int value);

P gui_to_px_coords(P pos);
P gui_to_px_coords(int x, int y);

P map_to_px_coords(P pos);
P map_to_px_coords(int x, int y);

P px_to_gui_coords(P px_pos);

P px_to_map_coords(P px_pos);

P gui_to_map_coords(P gui_pos);

// Returns a screen pixel position, relative to a cell position in a panel
P gui_to_px_coords(Panel panel, P offset);
P map_to_px_coords(Panel panel, P offset);

void draw_symbol(
        gfx::TileId tile,
        char character,
        Panel panel,
        P pos,
        const Color& color,
        DrawBg draw_bg = DrawBg::yes,
        const Color& color_bg = colors::black());

void draw_tile(
        gfx::TileId tile,
        Panel panel,
        const P& pos,
        const Color& color,
        DrawBg draw_bg = DrawBg::yes,
        const Color& bg_color = colors::black());

void draw_character(
        char character,
        Panel panel,
        P pos,
        const Color& color,
        DrawBg draw_bg = DrawBg::yes,
        const Color& bg_color = colors::black());

void draw_text(
        const std::string& str,
        Panel panel,
        P pos,
        const Color& color,
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

void draw_rectangle(R px_rect, const Color& color);

void draw_rectangle_filled(R px_rect, const Color& color);

void draw_blast_at_cells(
        const std::vector<P>& positions,
        const Color& color);

void draw_blast_at_seen_cells(
        const std::vector<P>& positions,
        const Color& color);

void draw_blast_at_seen_actors(
        const std::vector<actor::Actor*>& actors,
        const Color& color);

void draw_logo();

// Draws a description "box" for items, spells, etc. The parameter lines may be
// empty, in which case an empty area is drawn.
void draw_descr_box(const std::vector<ColoredString>& lines);

std::string sdl_pref_dir();

void sleep(uint32_t duration);

// ----------------------------------------
// TODO: WTF is the difference between these two functions?
void flush_input();
void clear_events();
// ----------------------------------------

InputData get();

}  // namespace io

#endif  // IO_HPP

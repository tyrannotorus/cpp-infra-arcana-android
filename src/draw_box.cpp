// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "draw_box.hpp"

#include "config.hpp"
#include "io.hpp"
#include "pos.hpp"
#include "rect.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static void draw_corner_top_l(
        const P& p0,
        const P& cell_dims,
        const Color& color)
{
        const P c = p0.with_offsets(cell_dims.scaled_down(2));

        // Small inner rectangle
        io::draw_rectangle(
                {c.with_offsets(-2, -2), c.with_offsets(-1, -1)},
                color);

        // Outer rectangle
        io::draw_rectangle(
                {c.with_offsets(-4, -4), c.with_offsets(1, 1)},
                color);

        // Dot
        io::draw_rectangle(
                {c.with_offsets(2, 2), c.with_offsets(2, 2)},
                color);

        // Dot
        io::draw_rectangle(
                {c.with_offsets(3, 3), c.with_offsets(3, 3)},
                color);

        // Line
        io::draw_rectangle(
                {c.with_offsets(3, -2), c.with_offsets(3, 0)},
                color);

        // Line
        io::draw_rectangle(
                {c.with_offsets(-2, 3), c.with_offsets(0, 3)},
                color);
}

static void draw_corner_top_r(
        const P& p0,
        const P& cell_dims,
        const Color& color)
{
        const P c = p0.with_offsets(cell_dims.scaled_down(2));

        // Small inner rectangle
        io::draw_rectangle(
                {c.with_offsets(1, -2), c.with_offsets(2, -1)},
                color);

        // Outer rectangle
        io::draw_rectangle(
                {c.with_offsets(-1, -4), c.with_offsets(4, 1)},
                color);

        // Dot
        io::draw_rectangle(
                {c.with_offsets(-2, 2), c.with_offsets(-2, 2)},
                color);

        // Dot
        io::draw_rectangle(
                {c.with_offsets(-3, 3), c.with_offsets(-3, 3)},
                color);

        // Line
        io::draw_rectangle(
                {c.with_offsets(-3, -2), c.with_offsets(-3, 0)},
                color);

        // Line
        io::draw_rectangle(
                {c.with_offsets(0, 3), c.with_offsets(2, 3)},
                color);
}

static void draw_corner_btm_l(
        const P& p0,
        const P& cell_dims,
        const Color& color)
{
        const P c = p0.with_offsets(cell_dims.scaled_down(2));

        // Small inner rectangle
        io::draw_rectangle(
                {c.with_offsets(-2, 1), c.with_offsets(-1, 2)},
                color);

        // Outer rectangle
        io::draw_rectangle(
                {c.with_offsets(-4, -1), c.with_offsets(1, 4)},
                color);

        // Dot
        io::draw_rectangle(
                {c.with_offsets(2, -2), c.with_offsets(2, -2)},
                color);

        // Dot
        io::draw_rectangle(
                {c.with_offsets(3, -3), c.with_offsets(3, -3)},
                color);

        // Line
        io::draw_rectangle(
                {c.with_offsets(3, 0), c.with_offsets(3, 2)},
                color);

        // Line
        io::draw_rectangle(
                {c.with_offsets(-2, -3), c.with_offsets(0, -3)},
                color);
}

static void draw_corner_btm_r(
        const P& p0,
        const P& cell_dims,
        const Color& color)
{
        const P c = p0.with_offsets(cell_dims.scaled_down(2));

        // Small inner rectangle
        io::draw_rectangle(
                {c.with_offsets(1, 1), c.with_offsets(2, 2)},
                color);

        // Outer rectangle
        io::draw_rectangle(
                {c.with_offsets(-1, -1), c.with_offsets(4, 4)},
                color);

        // Dot
        io::draw_rectangle(
                {c.with_offsets(-2, -2), c.with_offsets(-2, -2)},
                color);

        // Dot
        io::draw_rectangle(
                {c.with_offsets(-3, -3), c.with_offsets(-3, -3)},
                color);

        // Line
        io::draw_rectangle(
                {c.with_offsets(-3, 0), c.with_offsets(-3, 2)},
                color);

        // Line
        io::draw_rectangle(
                {c.with_offsets(0, -3), c.with_offsets(2, -3)},
                color);
}

static void draw_line_hor(
        const P& p0,
        const P& cell_dims,
        const int w,
        const Color& color)
{
        const int cell_w = cell_dims.x;
        const int cell_h = cell_dims.y;
        const int c_y = p0.y + (cell_h / 2);

        // NOTE: We draw a bit inside the left/right cells
        const int x0 = p0.x - cell_w + (cell_w / 2) + 4;
        const int x1 = p0.x + w + (cell_w / 2) - 4;

        const int y0 = c_y - 2;
        const int y1 = c_y;
        const int y2 = c_y + 1;
        const int y3 = c_y + 3;

        io::draw_rectangle({{x0, y0}, {x1, y0}}, color);
        io::draw_rectangle({{x0, y1}, {x1, y2}}, color);
        io::draw_rectangle({{x0, y3}, {x1, y3}}, color);
}

static void draw_line_ver(
        const P& p0,
        const P& cell_dims,
        const int h,
        const Color& color)
{
        const int cell_w = cell_dims.x;
        const int cell_h = cell_dims.y;
        const int c_x = p0.x + (cell_w / 2);

        // NOTE: We draw a bit inside the upper/lower cells
        const int y0 = p0.y - cell_h + (cell_h / 2) + 4;
        const int y1 = p0.y + h + (cell_h / 2) - 4;

        const int x0 = c_x - 2;
        const int x1 = c_x;
        const int x2 = c_x + 1;
        const int x3 = c_x + 3;

        io::draw_rectangle({{x0, y0}, {x0, y1}}, color);
        io::draw_rectangle({{x1, y0}, {x2, y1}}, color);
        io::draw_rectangle({{x3, y0}, {x3, y1}}, color);
}

// -----------------------------------------------------------------------------
// Global namesapce
// -----------------------------------------------------------------------------
void draw_box(R border, const Color& color)
{
        // Convert border from gui coordinates to pixel coordinates
        // NOTE: All coordinates are pixel coordinates from this point
        border = io::gui_to_px_rect(border);

        const int cell_w = config::gui_cell_px_w();
        const int cell_h = config::gui_cell_px_h();

        const P cell_dims(cell_w, cell_h);

        if ((cell_w <= 8) || (cell_h <= 8))
        {
                // GUI cells too small to draw a box properly with fancy
                // graphics, draw a simple rectangle box instead
                const auto cell_dims_half = cell_dims.scaled_down(2);
                border.p0 = border.p0 + cell_dims_half;
                border.p1 = border.p1 - cell_dims_half;

                io::draw_rectangle(border, color);

                return;
        }

        const int corner_r_x0 = border.p1.x - cell_w + 1;
        const int corner_btm_y0 = border.p1.y - cell_h + 1;

        const P corner_top_l_p = border.p0;
        const P corner_btm_l_p = {border.p0.x, corner_btm_y0};
        const P corner_top_r_p = {corner_r_x0, border.p0.y};
        const P corner_btm_r_p = {corner_r_x0, corner_btm_y0};

        const int line_w = border.w() - (cell_w * 2);
        const int line_h = border.h() - (cell_h * 2);

        const P line_top_p = corner_top_l_p.with_x_offset(cell_w);
        const P line_btm_p = corner_btm_l_p.with_x_offset(cell_w);

        const P line_l_p = corner_top_l_p.with_y_offset(cell_h);
        const P line_r_p = corner_top_r_p.with_y_offset(cell_h);

        // Corners
        draw_corner_top_l(corner_top_l_p, cell_dims, color);
        draw_corner_top_r(corner_top_r_p, cell_dims, color);
        draw_corner_btm_l(corner_btm_l_p, cell_dims, color);
        draw_corner_btm_r(corner_btm_r_p, cell_dims, color);

        // Horizontal lines
        draw_line_hor(line_top_p, cell_dims, line_w, color);
        draw_line_hor(line_btm_p, cell_dims, line_w, color);

        // Vertical lines
        draw_line_ver(line_l_p, cell_dims, line_h, color);
        draw_line_ver(line_r_p, cell_dims, line_h, color);
}

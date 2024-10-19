// =============================================================================
// Copyright 2011-2024 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "minimap.hpp"

#include <algorithm>
#include <climits>
#include <cstddef>
#include <iterator>
#include <set>
#include <string>

#include "SDL_keycode.h"
#include "actor.hpp"
#include "array2.hpp"
#include "colors.hpp"
#include "common_text.hpp"
#include "config.hpp"
#include "draw_box.hpp"
#include "gfx.hpp"
#include "global.hpp"
#include "io.hpp"
#include "item.hpp"
#include "item_data.hpp"
#include "map.hpp"
#include "map_parsing.hpp"
#include "panel.hpp"
#include "pos.hpp"
#include "rect.hpp"
#include "terrain.hpp"
#include "terrain_data.hpp"
#include "terrain_door.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
struct LegendEntryCmp
{
        bool operator()(
                const map::MinimapAppearance& v1,
                const map::MinimapAppearance& v2) const
        {
                const std::string& str1 = v1.legend_text;
                const std::string& str2 = v2.legend_text;

                return lexicographical_compare(
                        std::cbegin(str1),
                        std::cend(str1),
                        std::cbegin(str2),
                        std::cend(str2));
        }
};

using LegendSet = std::set<map::MinimapAppearance, LegendEntryCmp>;

static int get_px_w_per_cell()
{
        // TODO: Make this dynamic, i.e. depend on number of map cells and/or
        // window dimensions
        return 6;
}

static R get_map_area_explored()
{
        // Find the most top left and bottom right map cells explored
        R area_explored(INT_MAX, INT_MAX, 0, 0);

        const P& map_dims = map::dims();

        for (int x = 0; x < map_dims.x; ++x) {
                for (int y = 0; y < map_dims.y; ++y) {
                        const bool has_vision =
                                map::g_seen.at(x, y);

                        const map::PlayerMemoryTerrain& terrain_memory =
                                map::g_terrain_memory.at(x, y);

                        const map::PlayerMemoryItem& item_memory =
                                map::g_item_memory.at(x, y);

                        if (!has_vision &&
                            !terrain_memory.appearance.is_defined() &&
                            !item_memory.appearance.is_defined()) {
                                // Nothing seen or remembered here.
                                continue;
                        }

                        area_explored.p0.x = std::min(area_explored.p0.x, x);
                        area_explored.p0.y = std::min(area_explored.p0.y, y);
                        area_explored.p1.x = std::max(area_explored.p1.x, x);
                        area_explored.p1.y = std::max(area_explored.p1.y, y);
                }
        }

        return area_explored;
}

static R get_minimap_px_rect_on_screen(
        const R& map_area_explored,
        const int minimap_min_allowed_x0)
{
        const int px_w_per_cell = get_px_w_per_cell();

        const P minimap_px_dims = map_area_explored.dims().scaled_up(px_w_per_cell);

        const P screen_px_dims = io::gui_to_px_coords(panels::dims(Panel::screen));

        P minimap_p0(
                screen_px_dims.x / 2 - minimap_px_dims.x / 2,
                screen_px_dims.y / 2 - minimap_px_dims.y / 2);

        minimap_p0.x = std::max(io::gui_to_px_coords_x(minimap_min_allowed_x0), minimap_p0.x);

        const R px_rect(
                minimap_p0,
                minimap_p0 + minimap_px_dims - 1);

        return px_rect;
}

static void draw_title()
{
        io::draw_text_center(
                " Viewing map ",
                Panel::screen,
                {panels::center_x(Panel::screen), 0},
                colors::title());
}

static void draw_minimap_hint()
{
        io::draw_text_center(
                " " + common_text::g_minimap_exit_hint + " ",
                Panel::screen,
                {panels::center_x(Panel::screen), panels::y1(Panel::screen)},
                colors::title());
}

static map::MinimapAppearance get_top_memory_appearance_at_map_pos(const P& pos)
{
        if (pos == map::g_player->m_pos
            // && io::graphics_cycle_nr(io::GraphicsCycle::fast) % 2 == 0
        ) {
                map::MinimapAppearance player_appearance;

                player_appearance.color = colors::light_green();
                player_appearance.legend_text = actor::name_the(*map::g_player);

                return player_appearance;
        }

        const map::PlayerMemoryTerrain& terrain_memory = map::g_terrain_memory.at(pos);
        const map::PlayerMemoryItem& item_memory = map::g_item_memory.at(pos);

        if (item_memory.appearance.is_defined()) {
                return item_memory.appearance.minimap;
        }
        else {
                return terrain_memory.appearance.minimap;
        }
}

static R calc_minimap_rectangle_px_area(const P& draw_px_pos, const int px_w)
{
        const P px_dims(px_w, px_w);

        return {draw_px_pos, draw_px_pos + px_dims - 1};
}

static void draw_minimap_rectangle(
        const P& draw_px_pos,
        const int px_w,
        const map::MinimapSymbol symbol_type,
        const Color& color,
        const Color& color_bg = colors::black())
{
        const R px_rect = calc_minimap_rectangle_px_area(draw_px_pos, px_w);

        switch (symbol_type) {
        case map::MinimapSymbol::rectangle_filled:
                io::draw_rectangle_filled(px_rect, color);
                break;

        case map::MinimapSymbol::rectangle_filled_small:
                io::draw_rectangle_filled({px_rect.p0 + 1, px_rect.p1 - 1}, color);
                break;

        case map::MinimapSymbol::rectangle_edge:
                io::draw_rectangle_filled(px_rect, color_bg);
                io::draw_rectangle(px_rect, color);
                break;
        }
}

static void draw_minimap_rectangle_for_map_pos(
        const P& map_pos,
        const P& draw_px_pos,
        const int px_w)
{
        if (map::g_player->m_pos == map_pos) {
                const Color color =
                        (io::graphics_cycle_nr(io::GraphicsCycle::fast) % 2 == 0)
                        ? colors::light_green()
                        : minimap::floor_color();

                draw_minimap_rectangle(
                        draw_px_pos,
                        px_w,
                        map::MinimapSymbol::rectangle_filled,
                        color);
        }
        else {
                const map::MinimapAppearance appearance =
                        get_top_memory_appearance_at_map_pos(map_pos);

                draw_minimap_rectangle(
                        draw_px_pos,
                        px_w,
                        appearance.symbol,
                        appearance.color,
                        minimap::floor_color());
        }
}

static void draw_minimap(const int minimap_min_allowed_x0)
{
        const R area_explored = get_map_area_explored();

        const R minimap_px_rect =
                get_minimap_px_rect_on_screen(
                        area_explored,
                        minimap_min_allowed_x0);

        const P& minimap_px_offset = minimap_px_rect.p0;

        const int px_w_per_cell = get_px_w_per_cell();

        for (int x = area_explored.p0.x; x <= area_explored.p1.x; ++x) {
                for (int y = area_explored.p0.y; y <= area_explored.p1.y; ++y) {
                        const P map_pos(x, y);

                        const P pos_relative_to_explored_area = map_pos - area_explored.p0;

                        const P px_pos =
                                pos_relative_to_explored_area
                                        .scaled_up(px_w_per_cell)
                                        .with_offsets(minimap_px_offset);

                        draw_minimap_rectangle_for_map_pos(map_pos, px_pos, px_w_per_cell);
                }
        }
}

static LegendSet find_legend_data()
{
        LegendSet legend;

        for (int x = 0; x < map::w(); ++x) {
                for (int y = 0; y < map::h(); ++y) {
                        const P pos(x, y);

                        const map::MinimapAppearance& appearance =
                                get_top_memory_appearance_at_map_pos(pos);

                        if (!appearance.legend_text.empty()) {
                                legend.insert(appearance);
                        }
                }
        }

        return legend;
}

// Returns the rightmost position that were drawn at, so that it can be ensured
// that the minimap is drawn to the right of this point.
static int draw_legend()
{
        LegendSet legend = find_legend_data();

        const int rect_x0 = 2;
        const int text_x0 = 4;

        int y = 2;

        int longest_text_w = 0;

        for (const map::MinimapAppearance& entry : legend) {
                const P rect_gui_pos(rect_x0, y);

                const int rect_w = get_px_w_per_cell();

                const int gui_px_w = config::gui_cell_px_w();
                const int gui_px_h = config::gui_cell_px_h();

                const P rect_px_pos(
                        io::gui_to_px_coords(rect_gui_pos)
                                .with_offsets(
                                        (gui_px_w / 2) - (rect_w / 2) + 1,
                                        (gui_px_h / 2) - (rect_w / 2) + 1));

                draw_minimap_rectangle(
                        rect_px_pos,
                        rect_w,
                        entry.symbol,
                        entry.color,
                        colors::black());

                io::draw_text(
                        entry.legend_text,
                        Panel::screen,
                        {text_x0, y},
                        entry.color,
                        io::DrawBg::no);

                longest_text_w = std::max(longest_text_w, (int)entry.legend_text.length());

                ++y;
        }

        return text_x0 + longest_text_w - 1;
}

// -----------------------------------------------------------------------------
// ViewMinimap
// -----------------------------------------------------------------------------
void ViewMinimap::draw()
{
        draw_box(panels::area(Panel::screen));

        draw_title();

        draw_minimap_hint();

        const int legend_x1 = draw_legend();

        const int minimap_min_allowed_x0 = legend_x1 + 4;

        draw_minimap(minimap_min_allowed_x0);
}

void ViewMinimap::update()
{
        const io::InputData input = io::read_input();

        switch (input.key) {
        case SDLK_SPACE:
        case SDLK_ESCAPE:
        case SDLK_KP_0:
        case 'm':
                // Exit screen
                states::pop();
                break;

        default:
                break;
        }
}

// -----------------------------------------------------------------------------
// minimap
// -----------------------------------------------------------------------------
namespace minimap
{
void clear()
{
}

void update()
{
}

Color wall_color()
{
        return colors::sepia().shaded(10);
}

Color floor_color()
{
        return colors::dark_gray_brown();
}

}  // namespace minimap

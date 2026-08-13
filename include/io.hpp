// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
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

        END
};

enum class DrawBg
{
        no,
        yes
};

// A touch interaction that changes what a gesture is going to do, rather
// than doing something itself, has nothing to show for itself at the moment
// it takes - the device's own haptics are what confirm it (see
// haptic_feedback).
enum class HapticFeedback
{
        // Something was picked up or engaged (a long press landing)
        press,

        // Something clicked into place (the pad snapping to its slot)
        tick
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

// A frame's worth of map cells, resolved to at most one object per cell
// before anything is handed to SDL.
//
// Why this exists: SDL only merges consecutive draws that use the same
// texture and the same shader (a solid fill and a texture copy are different
// shaders - see GLES2_RunCommandQueue). Drawing the map cell by cell, each
// one a background rectangle followed by a tile copy, therefore cost two
// draw calls and a shader switch PER CELL, and the map layers (terrain,
// corpses, items, mobiles, monsters) paid that repeatedly for the same cell.
//
// Buffering resolves the layers in memory - later layers simply replace
// earlier ones, as overdrawing did - and then emits the whole map as one
// pass of backgrounds (grouped by color) and one pass of foregrounds (all
// out of the tile atlas). That is a handful of draw calls for the map
// instead of a couple of thousand.
class MapDrawBuffer
{
public:
        // Sets the view cell area that will be drawn, and clears it
        void reset(const R& view_area);

        // Stores an object at its view position. Objects that would not draw
        // anything are ignored, leaving whatever an earlier layer put there.
        void put(const MapDrawObj& obj);

        void draw() const;

private:
        R m_view_area {{0, 0}, {-1, -1}};
        std::vector<MapDrawObj> m_cells {};
};

struct InputData
{
        int key {-1};
        bool is_shift_held {false};
        bool is_ctrl_held {false};
};

void init_sdl();
void cleanup_sdl();

void init_sdl_audio();
void cleanup_sdl_audio();

void init_other();
void cleanup_other();

// Updates the sceen with what is currently drawn
#ifndef NDEBUG
extern bool g_allow_render;
#endif  // NDEBUG
void update_screen();

void clear_screen();

std::string sdl_pref_dir();

// Actual user resolution (i.e. not logical size)
P get_native_resolution();

// Recreates the window, the renderer and every texture, then redraws. Called
// when the GPU context was lost (see io_input's render device reset).
void on_render_device_reset();

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

// Fills many rectangles of the SAME color in a single draw call. Filling
// them one by one queues one draw call each, which is what made the map's
// cell backgrounds so expensive.
void draw_rectangles_filled(
        const std::vector<R>& px_rects,
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

// Clear all ongoing flash animations.
void clear_all_flash_animations();

void sleep(uint32_t duration);

void clear_input();

InputData read_input();

// A short buzz from the device. Does nothing when the user has touch
// feedback switched off in the system settings.
void haptic_feedback(HapticFeedback kind);

// Shows/hides the on-screen keyboard (the two finger tap gesture toggles
// it manually - these are for screens that require text entry, e.g. the
// name entry screen)
void show_screen_keyboard();

void hide_screen_keyboard();

// Height in logical pixels of the screen area covered by the on-screen
// keyboard, or 0 when it is down. The window is never resized for the
// keyboard, so screens with text entry must keep their content above this.
int screen_keyboard_covered_px_h();

}  // namespace io

#endif  // IO_HPP

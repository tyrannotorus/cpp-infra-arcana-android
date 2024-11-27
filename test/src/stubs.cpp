// =============================================================================
// Copyright 2011-2024 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include <string>
#include <vector>

#include "SDL_keycode.h"
#include "SDL_stdinc.h"
#include "audio.hpp"
#include "audio_data.hpp"
#include "colors.hpp"
#include "direction.hpp"
#include "gfx.hpp"
#include "io.hpp"
#include "panel.hpp"
#include "pos.hpp"
#include "rect.hpp"

namespace actor
{
class Actor;
}  // namespace actor

// -----------------------------------------------------------------------------
// io
// -----------------------------------------------------------------------------
namespace io
{
void init_sdl() {}

void init_other()
{
        panels::init({100, 100});
}

void cleanup_sdl() {}

void cleanup_other() {}

void init_sdl_audio() {}

void cleanup_sdl_audio() {}

void update_screen() {}

void clear_screen() {}

P get_native_resolution()
{
        return {};
}

void on_user_toggle_fullscreen() {}
void on_user_toggle_scaling() {}

R gui_to_px_rect(const R&)
{
        return {};
}

int gui_to_px_coords_x(const int)
{
        return 0;
}

int gui_to_px_coords_y(const int)
{
        return 0;
}

int map_to_px_coords_x(const int)
{
        return 0;
}

int map_to_px_coords_y(const int)
{
        return 0;
}

P gui_to_px_coords(const P&)
{
        return {};
}

P gui_to_px_coords(const int, const int)
{
        return {};
}

P map_to_px_coords(const P&)
{
        return {};
}

P map_to_px_coords(const int, const int)
{
        return {};
}

P px_to_gui_coords(const P&)
{
        return {};
}

P px_to_map_coords(const P&)
{
        return {};
}

P gui_to_map_coords(const P&)
{
        return {};
}

P gui_to_px_coords(const Panel, const P&)
{
        return {};
}

P map_to_px_coords(const Panel, const P&)
{
        return {};
}

void draw_map_obj(const MapDrawObj&) {}

void draw_tile(const TileDrawObj&) {}

void draw_character(const CharacterDrawObj&) {}

void draw_text(
        Text,
        Panel,
        P,
        Color,
        const DrawBg,
        const Color&) {}

void draw_text_center(
        const std::string&,
        const Panel,
        P,
        const Color&,
        const DrawBg,
        const Color&,
        const bool) {}

void draw_text_right(
        const std::string&,
        const Panel,
        P,
        const Color&,
        const DrawBg,
        const Color&) {}

void cover_cell(const Panel, const P&) {}

void cover_panel(
        const Panel,
        const Color&) {}

void cover_area(
        const Panel,
        const R&,
        const Color&) {}

void cover_area(
        const Panel,
        const P&,
        const P&,
        const Color&) {}

void draw_rectangle(R, const Color&) {}

void draw_rectangle_filled(R, const Color&, const uint8_t) {}

void draw_rectangle_filled_mod_blending(R, const Color&, uint8_t) {}

void TileDrawObj::draw() const {}

void CharacterDrawObj::draw() const {}

void MapDrawObj::draw() const {}

void draw_logo() {}

void clear_input() {}

InputData read_input()
{
        InputData d = {};

        d.key = SDLK_SPACE;

        return d;
}

int graphics_cycle_nr(const GraphicsCycle)
{
        return 0;
}

void flash_at(const P&, const Color&, const int)
{
}

void flash_at_actor(const actor::Actor&, const Color&, const int)
{
}

void draw_flash_animations() {}

void clear_all_flash_animations() {}

std::string sdl_pref_dir()
{
        return "./";
}

void sleep(const Uint32) {}

}  // namespace io

// -----------------------------------------------------------------------------
// audio
// -----------------------------------------------------------------------------
namespace audio
{
void init() {}

void cleanup() {}

void play(const SfxId, const int, const int) {}

void play_from_direction(const SfxId, const Dir, const int) {}

void try_play_ambient(const int) {}

void stop_ambient() {}

void play_music(const MusId) {}

void set_music_volume(const int) {}

void fade_out_music() {}

}  // namespace audio

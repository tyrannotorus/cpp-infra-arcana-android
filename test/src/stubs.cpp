// =============================================================================
// Copyright 2011-2020 Martin Törnqvist <m.tornq@gmail.com>
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
void init()
{
        panels::init({100, 100});
}

void cleanup() {}

void update_screen() {}

void clear_screen() {}

void on_fullscreen_toggled() {}

P min_screen_gui_dims()
{
        return {};
}

R gui_to_px_rect(const R rect)
{
        (void)rect;

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

P gui_to_px_coords(const P)
{
        return {};
}

P gui_to_px_coords(const int, const int)
{
        return {};
}

P map_to_px_coords(const P)
{
        return {};
}

P map_to_px_coords(const int, const int)
{
        return {};
}

P px_to_gui_coords(const P)
{
        return {};
}

P px_to_map_coords(const P)
{
        return {};
}

P gui_to_map_coords(const P)
{
        return {};
}

P gui_to_px_coords(const Panel, const P)
{
        return {};
}

P map_to_px_coords(const Panel, const P)
{
        return {};
}

void draw_symbol(
        const gfx::TileId,
        const char,
        const Panel,
        P,
        const Color&,
        const DrawBg,
        const Color&) {}

void draw_tile(
        const gfx::TileId,
        const Panel,
        const P&,
        const Color&,
        const DrawBg,
        const Color&) {}

void draw_character(
        const char,
        const Panel,
        P,
        const Color&,
        const DrawBg,
        const Color&) {}

void draw_text(
        const std::string&,
        const Panel,
        P,
        const Color&,
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

void draw_rectangle_filled(R, const Color&) {}

void draw_blast_at_cells(
        const std::vector<P>&,
        const Color&) {}

void draw_blast_at_seen_cells(
        const std::vector<P>&,
        const Color&) {}

void draw_blast_at_seen_actors(
        const std::vector<actor::Actor*>&,
        const Color&) {}

void draw_logo() {}

void draw_descr_box(const std::vector<ColoredString>&) {}

void flush_input() {}

void clear_events() {}

InputData get()
{
        InputData d = {};

        d.key = SDLK_SPACE;

        return d;
}

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

void play(const SfxId, const Dir, const int) {}

void try_play_amb(const int) {}

void play_music(const MusId) {}

void set_music_volume(const int) {}

void fade_out_music() {}

}  // namespace audio

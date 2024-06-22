// =============================================================================
// Copyright 2011-2024 Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <vector>

#include "SDL_timer.h"
#include "actor.hpp"
#include "actor_see.hpp"
#include "colors.hpp"
#include "config.hpp"
#include "debug.hpp"
#include "game_time.hpp"
#include "io.hpp"
#include "io_internal.hpp"
#include "pos.hpp"
#include "rect.hpp"
#include "state.hpp"
#include "viewport.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------
static const size_t nr_graphics_cycle_types = (size_t)io::GraphicsCycle::END;
static uint32_t s_graphics_cycle_delay_ms[nr_graphics_cycle_types];
static uint32_t s_last_graphics_cycle_ms[nr_graphics_cycle_types];
static int s_graphics_cycle_nr[nr_graphics_cycle_types];

static std::vector<io::FlashData> s_flashes;

static const uint32_t s_flash_animation_delay_ms = 40U;
static const int s_flash_alpha_pct_start_value = 80;
static const int s_flash_alpha_pct_default_decr = 12;
static uint32_t s_last_flash_animation_step_ms = 0U;

// Convert map position to a pixel area on the screen for drawing a flash
// rectangle.
static R map_pos_to_px_rect(const P& pos)
{
        const P view_pos = viewport::to_view_pos(pos);

        const P px_pos = io::map_to_px_coords(view_pos);

        const P px_dims(
                config::map_cell_px_w(),
                config::map_cell_px_h());

        return {px_pos, px_pos + px_dims - 1};
}

static bool does_actor_exist(const actor::Actor* const actor)
{
        const auto result =
                std::find(
                        std::cbegin(game_time::g_actors),
                        std::cend(game_time::g_actors),
                        actor);

        return (result != std::cend(game_time::g_actors));
}

static void update_flash_with_seen_actor(io::FlashData& flash)
{
        // Update rectangle to this actor's position.
        flash.px_rect = map_pos_to_px_rect(flash.actor_flashed_at->m_pos);

        if (!flash.actor_flashed_at->is_alive()) {
                // The actor is dead - fade out faster.
                flash.alpha_pct -= flash.alpha_pct_decr_step;
        }
}

static void update_flash_with_existing_actor(io::FlashData& flash)
{
        if (actor::can_player_see_actor(*flash.actor_flashed_at)) {
                update_flash_with_seen_actor(flash);
        }
        else {
                // The actor's position is not seeen by the player -
                // stop drawing a flash here.
                flash.alpha_pct = 0;
        }
}

static void update_flash_with_actor(io::FlashData& flash)
{
        if (does_actor_exist(flash.actor_flashed_at)) {
                update_flash_with_existing_actor(flash);
        }
        else {
                // Actor no longer exists - fade out faster.
                flash.alpha_pct -= flash.alpha_pct_decr_step;
        }
}

static void erase_finished_flahes()
{
        for (auto it = std::begin(s_flashes); it != std::end(s_flashes);) {
                if (it->alpha_pct <= 0) {
                        s_flashes.erase(it);
                }
                else {
                        ++it;
                }
        }
}

static void erase_flahes_at(const P& pos)
{
        for (auto it = std::begin(s_flashes); it != std::end(s_flashes);) {
                if (it->pos == pos) {
                        s_flashes.erase(it);
                }
                else {
                        ++it;
                }
        }
}

static void erase_flahes_with_actor(const actor::Actor* const actor)
{
        for (auto it = std::begin(s_flashes); it != std::end(s_flashes);) {
                if (it->actor_flashed_at == actor) {
                        s_flashes.erase(it);
                }
                else {
                        ++it;
                }
        }
}

// -----------------------------------------------------------------------------
// io
// -----------------------------------------------------------------------------
namespace io
{
void init_animation()
{
        for (size_t i = 0; i < (size_t)GraphicsCycle::END; ++i) {
                auto& delay = s_graphics_cycle_delay_ms[i];

                const auto cycle = (GraphicsCycle)i;

                switch (cycle) {
                case GraphicsCycle::fast:
                        delay = 300;
                        break;

                case GraphicsCycle::slow:
                        delay = 500;
                        break;

                case GraphicsCycle::very_slow:
                        delay = 1400;
                        break;

                case GraphicsCycle::END:
                        ASSERT(false);
                        break;
                }

                s_last_graphics_cycle_ms[i] = 0;
                s_graphics_cycle_nr[i] = 0;
        }
}

bool step_graphics_cycling()
{
        bool did_step = false;

        const auto current_time_ms = SDL_GetTicks();

        for (size_t i = 0; i < (size_t)io::GraphicsCycle::END; ++i) {
                const auto d = current_time_ms - s_last_graphics_cycle_ms[i];

                if (d < s_graphics_cycle_delay_ms[i]) {
                        continue;
                }

                s_last_graphics_cycle_ms[i] = current_time_ms;

                const auto cycle = (io::GraphicsCycle)i;

                states::cycle_graphics(cycle);

                ++s_graphics_cycle_nr[i];

                did_step = true;
        }

        return did_step;
}

int graphics_cycle_nr(const GraphicsCycle cycle_type)
{
        if (cycle_type == GraphicsCycle::END) {
                ASSERT(false);

                return 0;
        }

        return s_graphics_cycle_nr[(size_t)cycle_type];
}

void flash_at(const P& pos, const Color& color, const int speed_pct)
{
        erase_flahes_at(pos);

        FlashData flash;

        flash.pos = pos;
        flash.px_rect = map_pos_to_px_rect(pos);
        flash.color = color;
        flash.alpha_pct = s_flash_alpha_pct_start_value;
        flash.alpha_pct_decr_step = (s_flash_alpha_pct_default_decr * speed_pct) / 100;

        s_flashes.push_back(flash);
}

void flash_at_actor(const actor::Actor& actor, const Color& color, const int speed_pct)
{
        erase_flahes_with_actor(&actor);

        FlashData flash;

        flash.actor_flashed_at = &actor;
        flash.px_rect = map_pos_to_px_rect(actor.m_pos);
        flash.color = color;
        flash.alpha_pct = s_flash_alpha_pct_start_value;
        flash.alpha_pct_decr_step = (s_flash_alpha_pct_default_decr * speed_pct) / 100;

        s_flashes.push_back(flash);
}

bool step_flash_animations()
{
        const uint32_t current_time_ms = SDL_GetTicks();

        const uint32_t d = current_time_ms - s_last_flash_animation_step_ms;

        if (d < s_flash_animation_delay_ms) {
                return false;
        }

        s_last_flash_animation_step_ms = current_time_ms;

        bool should_redraw = false;

        for (FlashData& flash : s_flashes) {
                should_redraw = true;

                flash.alpha_pct -= flash.alpha_pct_decr_step;

                update_flash_with_actor(flash);
        }

        erase_finished_flahes();

        return should_redraw;
}

void draw_flash_animations()
{
        // TODO: This works for now, when only stuff on the map is flashed. But
        // this will have to be updated if flashing shall be implemented for gui
        // elements (like flashing text):
        set_clip_rect_to_panel(Panel::map);

        for (const FlashData& flash : s_flashes) {
                const auto a = (uint8_t)((255 * flash.alpha_pct) / 100);

                draw_rectangle_filled(flash.px_rect, flash.color, a);
        }

        disable_clip_rect();
}

void clear_all_flash_animations()
{
        s_flashes.clear();
}

}  // namespace io

// =============================================================================
// Copyright Martin Törnqvist <m.tornq@gmail.com>
//
// SPDX-License-Identifier: AGPL-3.0-or-later
// =============================================================================

#include "fade.hpp"

#include <algorithm>

#include "SDL_timer.h"
#include "colors.hpp"
#include "config.hpp"
#include "io.hpp"
#include "io_display.hpp"
#include "panel.hpp"
#include "rect.hpp"
#include "state.hpp"

// -----------------------------------------------------------------------------
// Private
// -----------------------------------------------------------------------------

// Roughly 60 frames per second - the fade is a full redraw per step, so
// there is no point asking for more than the display can show
static const uint32_t s_step_delay_ms = 16;

// -----------------------------------------------------------------------------
// fade
// -----------------------------------------------------------------------------
namespace fade
{
void to_black(const uint32_t duration_ms)
{
        if (config::is_bot_playing()) {
                // The bot plays with no one watching
                return;
        }

        const uint32_t start_ms = SDL_GetTicks();

        while (true) {
                const uint32_t elapsed_ms = SDL_GetTicks() - start_ms;

                const bool is_done =
                        (duration_ms == 0) || (elapsed_ms >= duration_ms);

                const int alpha =
                        is_done
                        ? 255
                        : (int)((elapsed_ms * 255) / duration_ms);

                states::draw();

                // Over everything, including the action bar and the context
                // pins - the overlay display is composited last, and is the
                // only one that spans the whole screen on top of them (see
                // io_display)
                io::set_display(io::Display::overlay);

                io::draw_rectangle_filled(
                        io::panel_logical_px_rect(Panel::screen),
                        colors::black(),
                        (uint8_t)std::clamp(alpha, 0, 255));

                io::update_screen();

                if (is_done) {
                        break;
                }

                // Everything that arrived during this step is thrown away,
                // rather than left to queue up and be acted on the moment
                // the fade ends
                io::clear_input();

                io::sleep(s_step_delay_ms);
        }

        io::clear_input();
}

}  // namespace fade
